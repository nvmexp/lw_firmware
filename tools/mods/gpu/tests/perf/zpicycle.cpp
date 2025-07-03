/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/cnslmgr.h"
#include "gpu/perf/zpi.h"
#include "gpu/tests/gputest.h"

/**
 * @file   zpi.cpp
 * @brief  Implementation of a Zero Power Idle Cycle test.
 */

class ZPICycle : public GpuTest
{
    public:
        ZPICycle();
        RC Setup() override;
        RC Run() override;
        RC Cleanup() override;
        bool IsSupported() override;
        SETGET_PROP(DelayMs, UINT32);
        SETGET_PROP(InteractiveMode, bool);
        SETGET_PROP(DelayBeforeOOBEntryMs, UINT32);
        SETGET_PROP(DelayAfterOOBExitMs, UINT32);
        SETGET_PROP(DelayBeforeRescanMs, UINT32);
        SETGET_PROP(RescanRetryDelayMs, UINT32);
    private:
        Perf::PerfPoint m_PerfPt;
        UINT32 m_DelayMs = 200;
        bool m_InteractiveMode = false;
        UINT32 m_DelayBeforeOOBEntryMs = 0;
        UINT32 m_DelayAfterOOBExitMs = 0;
        UINT32 m_DelayBeforeRescanMs = 0;
        UINT32 m_RescanRetryDelayMs = 100;
        std::unique_ptr<ZeroPowerIdle> m_ZpiImpl;
        static constexpr UINT32 PROMPT_LOOP_SLEEP_MS = 500;
        RC Prompt();
};

JS_CLASS_INHERIT(ZPICycle, GpuTest, "Zero power idle test");

CLASS_PROP_READWRITE(ZPICycle, DelayMs, UINT32,
                     "Delay after ZPI entry in ms (default is 200)");
CLASS_PROP_READWRITE(ZPICycle, InteractiveMode, bool,
                     "Interactive mode brings out prompt for user and wait for user"
                     " input after entering ZPI(Useful for SLT)");
CLASS_PROP_READWRITE(ZPICycle, DelayBeforeOOBEntryMs, UINT32,
                     "Delay before OOB entry action (or after GPU PCI removal) in ms (default is 0)");
CLASS_PROP_READWRITE(ZPICycle, DelayAfterOOBExitMs, UINT32,
                     "Delay after OOB exit action (or before GPU PCI rediscovery) in ms (default is 0)");
CLASS_PROP_READWRITE(ZPICycle, DelayBeforeRescanMs, UINT32,
                     "Delay before PCI rescan start (or after Link enable) in ms (default is 0)");
CLASS_PROP_READWRITE(ZPICycle, RescanRetryDelayMs, UINT32,
                     "Delay between PCI rescan retries in ms (default is 100)");

//----------------------------------------------------------------------------
ZPICycle::ZPICycle()
{
    SetName("ZPICycle");
}

//----------------------------------------------------------------------------
//! \brief Determine if the ZPICycle test is supported
//!
//! \return true if ZPICycle test is supported, false if not
//!
bool ZPICycle::IsSupported()
{
    GpuSubdevice *pGpuSubdev = GetBoundGpuSubdevice();
    if (pGpuSubdev->IsSOC())
    {
        return false;
    }
    m_ZpiImpl = make_unique<ZeroPowerIdle>();
    return m_ZpiImpl->IsSupported(pGpuSubdev);
}

//----------------------------------------------------------------------------
//! \brief Setup the ZPICycle test
//!
//! \return OK if setup was successful, not OK otherwise
//!
RC ZPICycle::Setup()
{
    RC rc;

    if (m_ZpiImpl.get() == nullptr)
    {
        m_ZpiImpl = make_unique<ZeroPowerIdle>();
    }

    CHECK_RC(m_ZpiImpl->Setup(GetBoundGpuSubdevice()));
    m_ZpiImpl->SetDelayBeforeOOBEntryMs(m_DelayBeforeOOBEntryMs);
    m_ZpiImpl->SetDelayAfterOOBExitMs(m_DelayAfterOOBExitMs);
    m_ZpiImpl->SetDelayBeforeRescanMs(m_DelayBeforeRescanMs);
    m_ZpiImpl->SetRescanRetryDelayMs(m_RescanRetryDelayMs);
    CHECK_RC(GetBoundGpuSubdevice()->GetPerf()->GetLwrrentPerfPoint(&m_PerfPt));

    return rc;
}

//----------------------------------------------------------------------------
//! \brief Run the ZPICycle test
//!
//! \return OK if the test was successful, not OK otherwise
//!
RC ZPICycle::Run()
{
    RC rc;

    CHECK_RC(m_ZpiImpl->EnterZPI(m_DelayMs * 1000));

    if (m_InteractiveMode)
    {
        CHECK_RC(Prompt());
    }

    CHECK_RC(m_ZpiImpl->ExitZPI());

    return rc;
}

RC ZPICycle::Cleanup()
{
    StickyRC rc;

    Perf  *pPerf = GetBoundGpuSubdevice()->GetPerf();
    Printf(Tee::PriLow, "Restoring PerfPt %s\n",
        m_PerfPt.name(pPerf->IsPState30Supported()).c_str());
    rc = pPerf->SetPerfPoint(m_PerfPt);
    rc = m_ZpiImpl->Cleanup();
    m_ZpiImpl.reset();

    return rc;
}

RC ZPICycle::Prompt()
{
    ConsoleManager::ConsoleContext consoleCtx;
    Console *pConsole = consoleCtx.AcquireRealConsole(true);
    Printf(Tee::PriNormal, "GPU has entered ZPI, press y to proceed to ZPI exit\n");
    while (true)
    {
        if (pConsole->KeyboardHit())
        {
            char ch = tolower(pConsole->GetKey());
            if (ch == 'y')
                break;
        }
        Tasker::Sleep(PROMPT_LOOP_SLEEP_MS);
    }
    return RC::OK;
}
