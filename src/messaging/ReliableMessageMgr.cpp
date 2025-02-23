/*
 *    Copyright (c) 2020-2021 Project CHIP Authors
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

/**
 *    @file
 *      This file implements the CHIP reliable message protocol.
 *
 */

#include <inttypes.h>

#include <messaging/ReliableMessageMgr.h>

#include <lib/support/BitFlags.h>
#include <lib/support/CHIPFaultInjection.h>
#include <lib/support/CodeUtils.h>
#include <lib/support/logging/CHIPLogging.h>
#include <messaging/ErrorCategory.h>
#include <messaging/ExchangeMessageDispatch.h>
#include <messaging/ExchangeMgr.h>
#include <messaging/Flags.h>
#include <messaging/ReliableMessageContext.h>

namespace chip {
namespace Messaging {

ReliableMessageMgr::RetransTableEntry::RetransTableEntry(ReliableMessageContext * rc) :
    ec(*rc->GetExchangeContext()), retainedBuf(EncryptedPacketBufferHandle()), nextRetransTime(0), sendCount(0)
{
    ec->SetMessageNotAcked(true);
}

ReliableMessageMgr::RetransTableEntry::~RetransTableEntry()
{
    ec->SetMessageNotAcked(false);
}

ReliableMessageMgr::ReliableMessageMgr(BitMapObjectPool<ExchangeContext, CHIP_CONFIG_MAX_EXCHANGE_CONTEXTS> & contextPool) :
    mContextPool(contextPool), mSystemLayer(nullptr)
{}

ReliableMessageMgr::~ReliableMessageMgr() {}

void ReliableMessageMgr::Init(chip::System::Layer * systemLayer)
{
    mSystemLayer = systemLayer;
}

void ReliableMessageMgr::Shutdown()
{
    StopTimer();

    // Clear the retransmit table
    mRetransTable.ForEachActiveObject([&](auto * entry) {
        mRetransTable.ReleaseObject(entry);
        return Loop::Continue;
    });

    mSystemLayer = nullptr;
}

#if defined(RMP_TICKLESS_DEBUG)
void ReliableMessageMgr::TicklessDebugDumpRetransTable(const char * log)
{
    ChipLogDetail(ExchangeManager, log);

    mRetransTable.ForEachActiveObject([&](auto * entry) {
        ChipLogDetail(ExchangeManager,
                      "EC:" ChipLogFormatExchange " MessageCounter:" ChipLogFormatMessageCounter " NextRetransTimeCtr:%" PRIu64,
                      ChipLogValueExchange(&entry->ec.Get()), entry->retainedBuf.GetMessageCounter(),
                      entry->nextRetransTime.count());
        return Loop::Continue;
    });
}
#else
void ReliableMessageMgr::TicklessDebugDumpRetransTable(const char * log)
{
    return;
}
#endif // RMP_TICKLESS_DEBUG

void ReliableMessageMgr::ExecuteActions()
{
    System::Clock::Timestamp now = System::SystemClock().GetMonotonicTimestamp();

#if defined(RMP_TICKLESS_DEBUG)
    ChipLogDetail(ExchangeManager, "ReliableMessageMgr::ExecuteActions at % " PRIu64 "ms", now.count());
#endif

    ExecuteForAllContext([&](ReliableMessageContext * rc) {
        if (rc->IsAckPending())
        {
            if (rc->mNextAckTime <= now)
            {
#if defined(RMP_TICKLESS_DEBUG)
                ChipLogDetail(ExchangeManager, "ReliableMessageMgr::ExecuteActions sending ACK %p", rc);
#endif
                rc->SendStandaloneAckMessage();
            }
        }
    });

    // Retransmit / cancel anything in the retrans table whose retrans timeout has expired
    mRetransTable.ForEachActiveObject([&](auto * entry) {
        if (entry->nextRetransTime > now)
            return Loop::Continue;

        VerifyOrDie(!entry->retainedBuf.IsNull());

        uint8_t sendCount       = entry->sendCount;
        uint32_t messageCounter = entry->retainedBuf.GetMessageCounter();

        if (sendCount == CHIP_CONFIG_RMP_DEFAULT_MAX_RETRANS)
        {
            ChipLogError(ExchangeManager,
                         "Failed to Send CHIP MessageCounter:" ChipLogFormatMessageCounter " on exchange " ChipLogFormatExchange
                         " sendCount: %" PRIu8 " max retries: %d",
                         messageCounter, ChipLogValueExchange(&entry->ec.Get()), sendCount, CHIP_CONFIG_RMP_DEFAULT_MAX_RETRANS);

            // Do not StartTimer, we will schedule the timer at the end of the timer handler.
            mRetransTable.ReleaseObject(entry);
            return Loop::Continue;
        }

        ChipLogDetail(ExchangeManager,
                      "Retransmitting MessageCounter:" ChipLogFormatMessageCounter " on exchange " ChipLogFormatExchange
                      " Send Cnt %d",
                      messageCounter, ChipLogValueExchange(&entry->ec.Get()), entry->sendCount);
        // TODO: Choose active/idle timeout corresponding to the activity of exchanges of the session.
        entry->nextRetransTime = System::SystemClock().GetMonotonicTimestamp() + entry->ec->GetMRPConfig().mActiveRetransTimeout;
        SendFromRetransTable(entry);
        // For test not using async IO loop, the entry may have been removed after send, do not use entry below

        return Loop::Continue;
    });

    TicklessDebugDumpRetransTable("ReliableMessageMgr::ExecuteActions Dumping mRetransTable entries after processing");
}

void ReliableMessageMgr::Timeout(System::Layer * aSystemLayer, void * aAppState)
{
    ReliableMessageMgr * manager = reinterpret_cast<ReliableMessageMgr *>(aAppState);

    VerifyOrDie((aSystemLayer != nullptr) && (manager != nullptr));

#if defined(RMP_TICKLESS_DEBUG)
    ChipLogDetail(ExchangeManager, "ReliableMessageMgr::Timeout\n");
#endif

    // Execute any actions that are due this tick
    manager->ExecuteActions();

    // Calculate next physical wakeup
    manager->StartTimer();
}

CHIP_ERROR ReliableMessageMgr::AddToRetransTable(ReliableMessageContext * rc, RetransTableEntry ** rEntry)
{
    VerifyOrDie(!rc->IsMessageNotAcked());

    *rEntry = mRetransTable.CreateObject(rc);
    if (*rEntry == nullptr)
    {
        ChipLogError(ExchangeManager, "mRetransTable Already Full");
        return CHIP_ERROR_RETRANS_TABLE_FULL;
    }

    return CHIP_NO_ERROR;
}

void ReliableMessageMgr::StartRetransmision(RetransTableEntry * entry)
{
    // TODO: Choose active/idle timeout corresponding to the activity of exchanges of the session.
    entry->nextRetransTime = System::SystemClock().GetMonotonicTimestamp() + entry->ec->GetMRPConfig().mIdleRetransTimeout;
    StartTimer();
}

bool ReliableMessageMgr::CheckAndRemRetransTable(ReliableMessageContext * rc, uint32_t ackMessageCounter)
{
    bool removed = false;
    mRetransTable.ForEachActiveObject([&](auto * entry) {
        if (entry->ec->GetReliableMessageContext() == rc && entry->retainedBuf.GetMessageCounter() == ackMessageCounter)
        {
            // Clear the entry from the retransmision table.
            ClearRetransTable(*entry);

            ChipLogDetail(ExchangeManager,
                          "Rxd Ack; Removing MessageCounter:" ChipLogFormatMessageCounter
                          " from Retrans Table on exchange " ChipLogFormatExchange,
                          ackMessageCounter, ChipLogValueExchange(rc->GetExchangeContext()));
            removed = true;
            return Loop::Break;
        }
        return Loop::Continue;
    });

    return removed;
}

CHIP_ERROR ReliableMessageMgr::SendFromRetransTable(RetransTableEntry * entry)
{
    if (!entry->ec->HasSessionHandle())
    {
        // Using same error message for all errors to reduce code size.
        ChipLogError(ExchangeManager,
                     "Crit-err %" CHIP_ERROR_FORMAT " when sending CHIP MessageCounter:" ChipLogFormatMessageCounter
                     " on exchange " ChipLogFormatExchange ", send tries: %d",
                     CHIP_ERROR_INCORRECT_STATE.Format(), entry->retainedBuf.GetMessageCounter(),
                     ChipLogValueExchange(&entry->ec.Get()), entry->sendCount);
        ClearRetransTable(*entry);
        return CHIP_ERROR_INCORRECT_STATE;
    }

    auto * sessionManager = entry->ec->GetExchangeMgr()->GetSessionManager();
    CHIP_ERROR err        = sessionManager->SendPreparedMessage(entry->ec->GetSessionHandle(), entry->retainedBuf);

    if (err == CHIP_NO_ERROR)
    {
        const ExchangeManager * exchangeMgr = entry->ec->GetExchangeMgr();
        // TODO: investigate why in ReliableMessageMgr::CheckResendApplicationMessageWithPeerExchange unit test released exchange
        // context with mExchangeMgr==nullptr is used.
        if (exchangeMgr)
        {
            // After the first failure notify session manager to refresh device data
            if (entry->sendCount == 0)
            {
                exchangeMgr->GetSessionManager()->RefreshSessionOperationalData(entry->ec->GetSessionHandle());
            }
        }
        // Update the counters
        entry->sendCount++;
    }
    else
    {
        // Remove from table
        // Using same error message for all errors to reduce code size.
        ChipLogError(ExchangeManager,
                     "Crit-err %" CHIP_ERROR_FORMAT " when sending CHIP MessageCounter:" ChipLogFormatMessageCounter
                     " on exchange " ChipLogFormatExchange ", send tries: %d",
                     err.Format(), entry->retainedBuf.GetMessageCounter(), ChipLogValueExchange(&entry->ec.Get()),
                     entry->sendCount);

        ClearRetransTable(*entry);
    }
    return err;
}

void ReliableMessageMgr::ClearRetransTable(ReliableMessageContext * rc)
{
    mRetransTable.ForEachActiveObject([&](auto * entry) {
        if (entry->ec->GetReliableMessageContext() == rc)
        {
            ClearRetransTable(*entry);
            return Loop::Break;
        }
        return Loop::Continue;
    });
}

void ReliableMessageMgr::ClearRetransTable(RetransTableEntry & entry)
{
    mRetransTable.ReleaseObject(&entry);
    // Expire any virtual ticks that have expired so all wakeup sources reflect the current time
    StartTimer();
}

void ReliableMessageMgr::StartTimer()
{
    // When do we need to next wake up to send an ACK?
    System::Clock::Timestamp nextWakeTime = System::Clock::Timestamp::max();

    ExecuteForAllContext([&](ReliableMessageContext * rc) {
        if (rc->IsAckPending() && rc->mNextAckTime < nextWakeTime)
        {
            nextWakeTime = rc->mNextAckTime;
        }
    });

    // When do we need to next wake up for ReliableMessageProtocol retransmit?
    mRetransTable.ForEachActiveObject([&](auto * entry) {
        if (entry->nextRetransTime < nextWakeTime)
        {
            nextWakeTime = entry->nextRetransTime;
        }
        return Loop::Continue;
    });

    if (nextWakeTime != System::Clock::Timestamp::max())
    {
#if defined(RMP_TICKLESS_DEBUG)
        ChipLogDetail(ExchangeManager, "ReliableMessageMgr::StartTimer wake at %" PRIu64 "ms", nextWakeTime);
#endif

        StopTimer();
        VerifyOrDie(mSystemLayer->StartTimer(nextWakeTime, Timeout, this) == CHIP_NO_ERROR);
    }
    else
    {
#if defined(RMP_TICKLESS_DEBUG)
        ChipLogDetail(ExchangeManager, "ReliableMessageMgr skipped timer");
#endif
        StopTimer();
    }

    TicklessDebugDumpRetransTable("ReliableMessageMgr::StartTimer Dumping mRetransTable entries after setting wakeup times");
}

void ReliableMessageMgr::StopTimer()
{
    mSystemLayer->CancelTimer(Timeout, this);
}

#if CHIP_CONFIG_TEST
int ReliableMessageMgr::TestGetCountRetransTable()
{
    int count = 0;
    mRetransTable.ForEachActiveObject([&](auto * entry) {
        count++;
        return Loop::Continue;
    });
    return count;
}
#endif // CHIP_CONFIG_TEST

} // namespace Messaging
} // namespace chip
