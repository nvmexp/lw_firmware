/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwlerrorcollector.h"

#include "core/include/cpu.h"
#include "core/include/mgrmgr.h"
#include "core/include/script.h"
#include "core/include/tasker.h"
#include "device/interface/lwlink.h"
#include "gpu/include/testdevicemgr.h"

namespace LwLinkErrorCollector
{
    constexpr UINT32 POLLING_DISABLED = 0;

    vector<LwLink *> m_LwLinkDevices;
    Tasker::ThreadID m_PollingThread   = Tasker::NULL_THREAD;
    ModsEvent*       m_PollingEvent    = nullptr;
    volatile INT32   m_StopPolling     = 0;
    volatile INT32   m_PollingRC       = RC::OK;
    volatile INT32   m_PausePolling    = 0;
    UINT32           m_PollingPeriodMs = POLLING_DISABLED;

    void PollingThread();
    RC SetPollingPeriodMs(UINT32 pollingPeriodMs) { m_PollingPeriodMs = pollingPeriodMs; return RC::OK; }
    UINT32 GetPollingPeriodMs() { return m_PollingPeriodMs; }
}

RC LwLinkErrorCollector::Start()
{
    if (m_PollingPeriodMs == POLLING_DISABLED)
    {
        Printf(Tee::PriLow, "LwLink error collector disabled, skipping start");
        return RC::OK;
    }

    if (!m_PollingEvent)
    {
        m_PollingEvent =
            Tasker::AllocEvent("LwLink error collector", false);
    }
    if (!m_PollingEvent)
    {
        return RC::SOFTWARE_ERROR;
    }

    Cpu::AtomicWrite(&m_StopPolling, 0);
    Cpu::AtomicWrite(&m_PollingRC,  OK);

    for (auto const & pTestDev : *DevMgrMgr::d_TestDeviceMgr)
    {
        if (pTestDev->SupportsInterface<LwLink>())
        {
            auto pLwLink = pTestDev->GetInterface<LwLink>();
            if (pLwLink->SupportsErrorCaching())
            {
                m_LwLinkDevices.push_back(pLwLink);
            }
        }
    }

    if (m_LwLinkDevices.empty())
    {
        Printf(Tee::PriLow, "No LwLink devices that support error caching found\n");
        return RC::OK;
    }

    MASSERT(m_PollingThread == Tasker::NULL_THREAD);
    m_PollingThread = Tasker::CreateThread(
            [](void* that) { LwLinkErrorCollector::PollingThread(); },
            nullptr, 0, "LwLink error collector");
    if (m_PollingThread == Tasker::NULL_THREAD)
    {
        return RC::SOFTWARE_ERROR;
    }

    return RC::OK;
}

RC LwLinkErrorCollector::Stop()
{
    Cpu::AtomicWrite(&m_StopPolling, 1);

    if (m_PollingEvent)
    {
        Tasker::SetEvent(m_PollingEvent);
    }

    if (m_PollingThread != Tasker::NULL_THREAD)
    {
        Tasker::Join(m_PollingThread);
        m_PollingThread = Tasker::NULL_THREAD;
    }

    if (m_PollingEvent)
    {
        Tasker::FreeEvent(m_PollingEvent);
        m_PollingEvent = nullptr;
    }

    m_LwLinkDevices.clear();

    return Cpu::AtomicXchg32(reinterpret_cast<volatile UINT32*>(&m_PollingRC), OK);
}

void LwLinkErrorCollector::PollingThread()
{
    Tasker::DetachThread detach;

    const UINT32 timeoutMs = m_PollingPeriodMs;

    while (Cpu::AtomicRead(&m_StopPolling) == 0)
    {
        MASSERT(m_PollingEvent);
        const RC rc = Tasker::WaitOnEvent(m_PollingEvent, timeoutMs);
        if (rc != RC::OK && rc != RC::TIMEOUT_ERROR)
        {
            Cpu::AtomicWrite(&m_PollingRC, rc.Get());
            Printf(Tee::PriError, "Event wait failed, "
                                  "LwLink error collector thread is exiting\n");
            return;
        }

        if (Cpu::AtomicRead(&m_PausePolling) != 0)
        {
            continue;
        }

        for (auto & pLwLink : m_LwLinkDevices)
        {
            if (!pLwLink->IsPostDiscovery())
                continue;

            for (UINT32 lwrLink = 0; lwrLink < pLwLink->GetMaxLinks(); lwrLink++)
            {
                LwLink::ErrorCounts errorCounts;

                // Getting the error counts updates the cache
                const RC rc = pLwLink->GetErrorCounts(lwrLink, &errorCounts);
                if (rc != RC::OK)
                {
                    Cpu::AtomicWrite(&m_PollingRC, rc.Get());
                    Printf(Tee::PriError, "Failed to read LwLink errors, "
                                          "LwLink error collector thread is exiting\n");
                    return;
                }
            }
        }
    }
}

LwLinkErrorCollector::Pause::Pause()
{
    Cpu::AtomicAdd(&m_PausePolling, 1);
}

LwLinkErrorCollector::Pause::~Pause()
{
    Cpu::AtomicAdd(&m_PausePolling, -1);
}

//-----------------------------------------------------------------------------
static JSClass LwLinkErrorCollector_class =
{
    "LwLinkErrorCollector",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ColwertStub,
    JS_FinalizeStub
};

//-----------------------------------------------------------------------------
static SObject LwLinkErrorCollector_Object
(
    "LwLinkErrorCollector",
    LwLinkErrorCollector_class,
    0,
    0,
    "LwLink Error Collector JS Object"
);

PROP_READWRITE_NAMESPACE(LwLinkErrorCollector, PollingPeriodMs, UINT32, "Rate at which to poll and accumulate LwLink errors");
PROP_CONST(LwLinkErrorCollector, POLLING_DISABLED, LwLinkErrorCollector::POLLING_DISABLED);

JS_STEST_LWSTOM(LwLinkErrorCollector, Start, 0, "Start the LwLink error collector thread")
{
    STEST_HEADER(0, 0, "Usage: LwLinkErrorCollector.Start()");
    RETURN_RC(LwLinkErrorCollector::Start());
}
