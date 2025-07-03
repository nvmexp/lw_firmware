/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation. All rights
 * reserved.  All information contained herein is proprietary and confidential
 * to LWPU Corporation.  Any use, reproduction, or disclosure without the
 * written permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/cmdline.h"
#include "core/include/heartbeatmonitor.h"
#include "core/include/jscript.h"
#include "core/include/log.h"
#include "core/include/mods_gdm_client.h"
#include "core/include/utility.h"
#include "core/include/xp.h"
#include <vector>

namespace {
    static constexpr UINT32 HBMONITOR_SLEEP_TIME_MILSEC     = 5000;
    static constexpr UINT32 HBMONITOR_DEFAULT_PERIOD_SEC    = 10;

    struct HeartBeatContext
    {
        UINT64 HeartBeatPeriodMs                      = 0;
        UINT64 HeartBeatPeriodExpirationTime          = 0;
        INT32  TestId                                 = -1;
        INT32  DevInst                                = -1;
    };

    static void* s_HBMutex = nullptr;
    static Tasker::ThreadID s_HbmThreadId = Tasker::NULL_THREAD;
    static std::vector<HeartBeatContext> s_HbContextVec;
    static Tasker::EventID s_ModsExitEvent;

    UINT64 GetContextId(HeartBeatMonitor::MonitorHandle handle)
    {
        return static_cast<UINT64>(-handle - 1);
    }

    bool IsHeartBeatMonitorActive()
    {
        return s_HBMutex != nullptr;
    }
}

static void HeartBeatMonitorThread(void*);

void HeartBeatMonitor::StopHeartBeat()
{
    if (!CommandLine::HeartBeatMonitorEnabled())
        return;

    Tasker::SetEvent(s_ModsExitEvent);
    if (OK != Tasker::Join(s_HbmThreadId))
    {
        Printf(Tee::PriError, "Failed to join heart beat monitor thread\n");
        MASSERT(0);
    }
    Tasker::FreeMutex(s_HBMutex);
    s_HBMutex = nullptr;
    Tasker::FreeEvent(s_ModsExitEvent);
    s_ModsExitEvent = nullptr;
}

RC HeartBeatMonitor::InitMonitor()
{
    s_HBMutex = Tasker::AllocMutex("HeartBeatMutex", Tasker::mtxLast);
    if (s_HBMutex == nullptr)
    {
        Printf(Tee::PriError, "Unable to allocate hert beat mutex\n");
        return RC::LWRM_OPERATING_SYSTEM;
    }

    s_ModsExitEvent = Tasker::AllocEvent("ModsExitEvent");
    if (s_ModsExitEvent == NULL)
    {
        Printf(Tee::PriError, "Unable to allocate mods exit event\n");
        Tasker::FreeMutex(s_HBMutex);
        s_HBMutex = nullptr;
        return RC::LWRM_OPERATING_SYSTEM;
    }

    s_HbmThreadId = Tasker::CreateThread(HeartBeatMonitorThread, 0, 0, "HeartBeatMonitor");
    if (s_HbmThreadId == Tasker::NULL_THREAD)
    {
        Printf(Tee::PriError, "Unable to create heartbeat monitor thread\n");
        Tasker::FreeMutex(s_HBMutex);
        s_HBMutex = nullptr;
        Tasker::FreeEvent(s_ModsExitEvent);
        s_ModsExitEvent = nullptr;
        return RC::LWRM_OPERATING_SYSTEM;
    }
    return RC::OK;
}

RC HeartBeatMonitor::SendUpdate(HeartBeatMonitor::MonitorHandle regId)
{
    if (!IsHeartBeatMonitorActive())
        return RC::OK;

    Tasker::MutexHolder lock(s_HBMutex);
    const UINT64 contextIndex = GetContextId(regId);
    if (regId == 0 || contextIndex >= s_HbContextVec.size())
    {
        Printf(Tee::PriError, "Heart beat registration Id out of bounds\n");
        MASSERT(!"Heart beat registration Id out of bounds");
        return RC::ILWALID_ARGUMENT;
    }
    if (s_HbContextVec[contextIndex].HeartBeatPeriodExpirationTime == 0)
    {
        Printf(Tee::PriError, "Invalid heart bear registration Id\n");
        MASSERT(!"Invalid heart beat registration Id");
        return RC::ILWALID_ARGUMENT;

    }
    const UINT64 lwrrentTime = Xp::GetWallTimeMS();
    const UINT64 expirationTime = s_HbContextVec[contextIndex].HeartBeatPeriodMs + lwrrentTime;
    s_HbContextVec[contextIndex].HeartBeatPeriodExpirationTime = expirationTime;
    return RC::OK;
}

HeartBeatMonitor::MonitorHandle HeartBeatMonitor::RegisterTest(INT32 testId, INT32 devInst, UINT64 heartBeatPeriodSec)
{
    if (!IsHeartBeatMonitorActive())
        return RC::OK;

    Tasker::MutexHolder lock(s_HBMutex);
    HeartBeatContext testRegistration;
    testRegistration.TestId  = testId;
    testRegistration.DevInst = devInst;
    if (heartBeatPeriodSec == 0)
    {
        Printf(Tee::PriLow, "Hearbeat period of 0 not valid setting it to default value\n");
        heartBeatPeriodSec = HBMONITOR_DEFAULT_PERIOD_SEC;
    }
    testRegistration.HeartBeatPeriodMs = heartBeatPeriodSec * 1000;
    const UINT64 registrationTime = Xp::GetWallTimeMS();
    const UINT64 expirationTime = registrationTime + (heartBeatPeriodSec * 1000);
    testRegistration.HeartBeatPeriodExpirationTime = expirationTime;

    const auto unusedSlotIt = find_if(s_HbContextVec.begin(), s_HbContextVec.end(),
            [](const HeartBeatContext& ctx) -> bool { return ctx.HeartBeatPeriodExpirationTime == 0; });
    const INT64 contextIndex = unusedSlotIt - s_HbContextVec.begin();
    if (unusedSlotIt == s_HbContextVec.end())
    {
        s_HbContextVec.push_back(testRegistration);
    }
    else
    {
        s_HbContextVec[contextIndex] = testRegistration;
    }

    return -(1 + contextIndex);
}

RC HeartBeatMonitor::UnRegisterTest(HeartBeatMonitor::MonitorHandle regId)
{
    if (!IsHeartBeatMonitorActive())
        return RC::OK;

    Tasker::MutexHolder lock(s_HBMutex);
    const UINT64 contextIndex = GetContextId(regId);
    if (regId == 0 || contextIndex >= s_HbContextVec.size())
    {
        Printf(Tee::PriError, "Heart beat registration Id out of bounds\n");
        MASSERT(!"Heart beat registration Id out of bounds");
        return RC::ILWALID_ARGUMENT;
    }
    if (s_HbContextVec[contextIndex].HeartBeatPeriodExpirationTime == 0)
    {
        Printf(Tee::PriError, "Invalid registration Id\n");
        MASSERT(!"Invalid heart beat registration Id");
        return RC::ILWALID_ARGUMENT;
    }
    s_HbContextVec[contextIndex].HeartBeatPeriodExpirationTime = 0;
    return RC::OK;
}

static void HeartBeatMonitorThread(void*)
{
    Tasker::DetachThread detach;
    while (Tasker::WaitOnEvent(s_ModsExitEvent, HBMONITOR_SLEEP_TIME_MILSEC) == RC::TIMEOUT_ERROR)
    {
        ModsGdmClient::Instance().SendMonitorHeartBeat();
        Tasker::MutexHolder lock(s_HBMutex);
        for (const auto& testEntry: s_HbContextVec)
        {
            const UINT64 expirationTime = testEntry.HeartBeatPeriodExpirationTime;
            const UINT64 lwrrentTime = Xp::GetWallTimeMS();
            if (expirationTime == 0 || lwrrentTime < expirationTime)
                continue;

            Printf(Tee::PriError, 
                    "Missing heartbeat from test id %d on devinst %d, last update time %llu"
                    ",current time %llu, expected update interval %llu, exiting\n",
                    testEntry.TestId, testEntry.DevInst,
                    (expirationTime - testEntry.HeartBeatPeriodMs),
                    lwrrentTime, testEntry.HeartBeatPeriodMs
                  );
            ErrorMap::SetTest(testEntry.TestId);

            Tasker::MutexHolder unlock(s_HBMutex);
            Log::SetFirstError(ErrorMap(RC::TIMEOUT_ERROR));
            Log::SetNext(false);
            JavaScriptPtr()->CallMethod(JavaScriptPtr()->GetGlobalObject(),
                    "PrintErrorWrapperPostTeardown");
            Tee::FlushToDisk();
            Utility::ExitMods(RC::TIMEOUT_ERROR, Utility::ExitQuickAndDirty);
        }
    }
    Printf(Tee::PriLow, "Exiting HeartBeatMonitorThread\n");
}
