/*
 *
 *    Copyright (c) 2020 Project CHIP Authors
 *    Copyright (c) 2020 Texas Instruments Incorporated
 *    All rights reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include "AppTask.h"
#include "AppConfig.h"
#include "AppEvent.h"
#include <app/server/Server.h>

#include "FreeRTOS.h"
#include "app-common/zap-generated/attributes/Accessors.h"

#include <credentials/DeviceAttestationCredsProvider.h>
#include <credentials/examples/DeviceAttestationCredsExample.h>

#include <lib/support/CHIPMem.h>
#include <lib/support/CHIPPlatformMemory.h>
#include <platform/CHIPDeviceLayer.h>

#include <app/server/OnboardingCodesUtil.h>

#include <ti/drivers/apps/Button.h>
#include <ti/drivers/apps/LED.h>

/* syscfg */
#include <ti_drivers_config.h>

#define APP_TASK_STACK_SIZE (4096)
#define APP_TASK_PRIORITY 4
#define APP_EVENT_QUEUE_SIZE 10

using namespace ::chip::Credentials;
using namespace ::chip::DeviceLayer;

static TaskHandle_t sAppTaskHandle;
static QueueHandle_t sAppEventQueue;

static LED_Handle sAppRedHandle;
static LED_Handle sAppGreenHandle;
static Button_Handle sAppLeftHandle;
static Button_Handle sAppRightHandle;

AppTask AppTask::sAppTask;

int AppTask::StartAppTask()
{
   int ret = 0;

   sAppEventQueue = xQueueCreate(APP_EVENT_QUEUE_SIZE, sizeof(AppEvent));
   if (sAppEventQueue == NULL)
   {
       PLAT_LOG("Failed to allocate app event queue");
       while (1)
           ;
   }

   // Start App task.
   if (xTaskCreate(AppTaskMain, "APP", APP_TASK_STACK_SIZE / sizeof(StackType_t), NULL, APP_TASK_PRIORITY, &sAppTaskHandle) !=
       pdPASS)
   {
       PLAT_LOG("Failed to create app task");
       while (1)
           ;
   }
   return ret;
}

int AppTask::Init()
{
   LED_Params ledParams;
   Button_Params buttonParams;

   cc13x2_26x2LogInit();

   // Init Chip memory management before the stack
   chip::Platform::MemoryInit();

   CHIP_ERROR ret = PlatformMgr().InitChipStack();
   if (ret != CHIP_NO_ERROR)
   {
       PLAT_LOG("PlatformMgr().InitChipStack() failed");
       while (1)
           ;
   }

   ret = ThreadStackMgr().InitThreadStack();
   if (ret != CHIP_NO_ERROR)
   {
       PLAT_LOG("ThreadStackMgr().InitThreadStack() failed");
       while (1)
           ;
   }

   ret = ConnectivityMgr().SetThreadDeviceType(ConnectivityManager::kThreadDeviceType_Router);
   if (ret != CHIP_NO_ERROR)
   {
       PLAT_LOG("ConnectivityMgr().SetThreadDeviceType() failed");
       while (1)
           ;
   }

   ret = PlatformMgr().StartEventLoopTask();
   if (ret != CHIP_NO_ERROR)
   {
       PLAT_LOG("PlatformMgr().StartEventLoopTask() failed");
       while (1)
           ;
   }

   ret = ThreadStackMgrImpl().StartThreadTask();
   if (ret != CHIP_NO_ERROR)
   {
       PLAT_LOG("ThreadStackMgr().StartThreadTask() failed");
       while (1)
           ;
   }

   // Init ZCL Data Model and start server
   PLAT_LOG("Initialize Server");
   chip::Server::GetInstance().Init();

   // Initialize device attestation config
   SetDeviceAttestationCredentialsProvider(Examples::GetExampleDACProvider());

   // Initialize LEDs
   PLAT_LOG("Initialize LEDs");
   LED_init();

   LED_Params_init(&ledParams); // default PWM LED
   sAppRedHandle = LED_open(CONFIG_LED_RED, &ledParams);
   LED_setOff(sAppRedHandle);

   LED_Params_init(&ledParams); // default PWM LED
   sAppGreenHandle = LED_open(CONFIG_LED_GREEN, &ledParams);
   LED_setOff(sAppGreenHandle);

   // Initialize buttons
   PLAT_LOG("Initialize buttons");
   Button_init();

   Button_Params_init(&buttonParams);
   buttonParams.buttonEventMask   = Button_EV_CLICKED | Button_EV_LONGCLICKED;
   buttonParams.longPressDuration = 1000U; // ms
   sAppLeftHandle                 = Button_open(CONFIG_BTN_LEFT, &buttonParams);
   Button_setCallback(sAppLeftHandle, ButtonLeftEventHandler);

   Button_Params_init(&buttonParams);
   buttonParams.buttonEventMask   = Button_EV_CLICKED | Button_EV_LONGCLICKED;
   buttonParams.longPressDuration = 1000U; // ms
   sAppRightHandle                = Button_open(CONFIG_BTN_RIGHT, &buttonParams);
   Button_setCallback(sAppRightHandle, ButtonRightEventHandler);

   ConfigurationMgr().LogDeviceConfig();

   // QR code will be used with CHIP Tool
   PrintOnboardingCodes(chip::RendezvousInformationFlags(chip::RendezvousInformationFlag::kBLE));

   return 0;
}

void AppTask::AppTaskMain(void * pvParameter)
{
   AppEvent event;

   sAppTask.Init();

   while (1)
   {
       /* Task pend until we have stuff to do */
       if (xQueueReceive(sAppEventQueue, &event, pdMS_TO_TICKS(1000)) == pdTRUE)
       {
           sAppTask.DispatchEvent(&event);
       }
       LED_toggle(sAppRedHandle);
   }
}

void AppTask::PostEvent(const AppEvent * aEvent)
{
   if (xQueueSend(sAppEventQueue, aEvent, 0) != pdPASS)
   {
       /* Failed to post the message */
   }
}

void AppTask::ButtonLeftEventHandler(Button_Handle handle, Button_EventMask events)
{
   AppEvent event;
   event.Type = AppEvent::kEventType_ButtonLeft;

   if (events & Button_EV_CLICKED)
   {
       event.ButtonEvent.Type = AppEvent::kAppEventButtonType_Clicked;
   }
   else if (events & Button_EV_LONGCLICKED)
   {
       event.ButtonEvent.Type = AppEvent::kAppEventButtonType_LongClicked;
   }
   // button callbacks are in ISR context
   if (xQueueSendFromISR(sAppEventQueue, &event, NULL) != pdPASS)
   {
       /* Failed to post the message */
   }
}

void AppTask::ButtonRightEventHandler(Button_Handle handle, Button_EventMask events)
{
   AppEvent event;
   event.Type = AppEvent::kEventType_ButtonRight;

   if (events & Button_EV_CLICKED)
   {
       event.ButtonEvent.Type = AppEvent::kAppEventButtonType_Clicked;
   }
   else if (events & Button_EV_LONGCLICKED)
   {
       event.ButtonEvent.Type = AppEvent::kAppEventButtonType_LongClicked;
   }
   // button callbacks are in ISR context
   if (xQueueSendFromISR(sAppEventQueue, &event, NULL) != pdPASS)
   {
       /* Failed to post the message */
   }
}

void AppTask::DispatchEvent(AppEvent * aEvent)
{
   switch (aEvent->Type)
   {
   case AppEvent::kEventType_ButtonLeft:
       if (AppEvent::kAppEventButtonType_Clicked == aEvent->ButtonEvent.Type)
       {
           LED_setOn(sAppGreenHandle, 0xff);
           ConfigurationMgr().InitiateFactoryReset();
       }
       else if (AppEvent::kAppEventButtonType_LongClicked == aEvent->ButtonEvent.Type)
       {
           // Disable BLE advertisements
           if (ConnectivityMgr().IsBLEAdvertisingEnabled())
           {
               ConnectivityMgr().SetBLEAdvertisingEnabled(false);
               PLAT_LOG("Disabled BLE Advertisements");
           }
       }
       break;

   case AppEvent::kEventType_ButtonRight:
       if (AppEvent::kAppEventButtonType_Clicked == aEvent->ButtonEvent.Type)
       {
           // Do nothing yet
       }
       else if (AppEvent::kAppEventButtonType_LongClicked == aEvent->ButtonEvent.Type)
       {
           // Enable BLE advertisements
           if (!ConnectivityMgr().IsBLEAdvertisingEnabled())
           {
               if (chip::Server::GetInstance().GetCommissioningWindowManager().OpenBasicCommissioningWindow() == CHIP_NO_ERROR)
               {
                   PLAT_LOG("Enabled BLE Advertisement");
               }
               else
               {
                   PLAT_LOG("OpenBasicCommissioningWindow() failed");
               }
           }
       }
       break;

   case AppEvent::kEventType_AppEvent:
       if (NULL != aEvent->Handler)
       {
           aEvent->Handler(aEvent);
       }
       break;

   case AppEvent::kEventType_None:
   default:
       break;
   }
}
