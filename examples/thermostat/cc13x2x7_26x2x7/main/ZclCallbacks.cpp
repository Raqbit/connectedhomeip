/*
 *
 *    Copyright (c) 2021 Project CHIP Authors
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

#include "AppConfig.h"
#include "app-common/zap-generated/attributes/Accessors.h"
#include "app-common/zap-generated/callback.h"
#include "lib/core/CHIPEncoding.h"
#include "protocols/interaction_model/Constants.h"
#include <app-common/zap-generated/ids/Attributes.h>
#include <app-common/zap-generated/ids/Clusters.h>
#include <app/ConcreteAttributePath.h>
#include <lib/support/logging/CHIPLogging.h>

using namespace ::chip;
using namespace ::chip::app;
using namespace ::chip::app::Clusters;

constexpr int16_t DefaultTemp = 20 * 100 + 11;

void emberAfThermostatClusterInitCallback(EndpointId endpoint)
{
    EmberAfStatus stat = Thermostat::Attributes::OccupiedHeatingSetpoint::Set(endpoint, DefaultTemp);
    PLAT_LOG("Thermostat init, endpoint %d, ret status %d", endpoint, stat);
}

Protocols::InteractionModel::Status MatterPreAttributeChangeCallback(const ConcreteAttributePath & attributePath, uint8_t mask,
                                                                     uint8_t type, uint16_t size, uint8_t * value)
{
    Protocols::InteractionModel::Status status = Protocols::InteractionModel::Status::Success;
    if (attributePath.mClusterId == chip::app::Clusters::Thermostat::Id)
    {
        status = MatterThermostatClusterServerPreAttributeChangedCallback(attributePath, type, size, value);
    }
    return status;
}

void MatterPostAttributeChangeCallback(const chip::app::ConcreteAttributePath & attributePath, uint8_t mask, uint8_t type,
                                       uint16_t size, uint8_t * value)
{
    if (attributePath.mClusterId == Thermostat::Id)
    {
        int16_t temp = static_cast<int16_t>(chip::Encoding::LittleEndian::Get16(value));

        switch (attributePath.mAttributeId)
        {
        case Thermostat::Attributes::OccupiedHeatingSetpoint::Id:
            PLAT_LOG("New heating set point: %d.%d", temp / 100, temp % 100);
            break;
        case Thermostat::Attributes::OccupiedCoolingSetpoint::Id:
            PLAT_LOG("New cooling set point: %d.%d", temp / 100, temp % 100);
        }
    }
}
