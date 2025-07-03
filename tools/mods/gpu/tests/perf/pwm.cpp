/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018,2020 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/include/gpusbdev.h"
#include "gpu/perf/pwrsub.h"
#include "gpu/tests/gputest.h"

// This test is a thin wrapper that calls the JS functions in pwm.js
class PwmMode : public GpuTest
{
public:
    RC Setup() override;
    RC Run() override;
    RC Cleanup() override;
    bool IsSupported() override;

    SETGET_PROP(MinTargetTimeMs, UINT32);
    SETGET_PROP(MinLowTimeMs, UINT32);
    SETGET_PROP(VfePollingPeriodMs, UINT32);

protected:
    RC RunJsFunc(const char* jsFuncName);

    PStateOwner m_PStateOwner;
    Perf::PerfPoint m_OrigPerfPoint;
    UINT32 m_MinTargetTimeMs = 0;
    UINT32 m_MinLowTimeMs = 0;
    UINT32 m_VfePollingPeriodMs = 0;
    UINT32 m_OrigVfePollingPeriodMs = 0;
};

JS_CLASS_INHERIT(PwmMode, GpuTest, "Run tests with PWM");

CLASS_PROP_READWRITE(PwmMode, MinTargetTimeMs, UINT32,
    "Minimum cumulative time spent at the target point");
CLASS_PROP_READWRITE(PwmMode, MinLowTimeMs, UINT32,
    "Minimum cumulative time spent at the lower point");
CLASS_PROP_READWRITE(PwmMode, VfePollingPeriodMs, UINT32,
    "Override the VFE polling period");

bool PwmMode::IsSupported()
{
    Perf* pPerf = GetBoundGpuSubdevice()->GetPerf();
    if (!(pPerf->IsPState30Supported() && pPerf->HasPStates()))
        return false;

    UINT32 tgpmW = 0;
    GetBoundGpuSubdevice()->GetPower()->GetPowerCap(Power::TGP, &tgpmW);
    if (!tgpmW)
        return false;

    return true;
}

RC PwmMode::Setup()
{
    RC rc;

    CHECK_RC(GpuTest::Setup());

    CHECK_RC(m_PStateOwner.ClaimPStates(GetBoundGpuSubdevice()));
    Perf* pPerf = GetBoundGpuSubdevice()->GetPerf();
    CHECK_RC(pPerf->GetLwrrentPerfPoint(&m_OrigPerfPoint));

    // Reserve the display so that we don't cause underflows due to violating IMP
    if (GetTestConfiguration()->DisplayMgrRequirements() == DisplayMgr::RequireRealDisplay)
    {
        CHECK_RC(GpuTest::AllocDisplay());
    }

    if (m_VfePollingPeriodMs)
    {
        CHECK_RC(pPerf->GetVfePollingPeriodMs(&m_OrigVfePollingPeriodMs));
        CHECK_RC(pPerf->SetVfePollingPeriodMs(m_VfePollingPeriodMs));
    }

    CHECK_RC(RunJsFunc("JsFuncSetup"));

    return rc;
}

RC PwmMode::Run()
{
    return RunJsFunc("JsFuncRun");
}

RC PwmMode::Cleanup()
{
    StickyRC stickyRc;
    Perf* pPerf = GetBoundGpuSubdevice()->GetPerf();

    if (m_VfePollingPeriodMs)
    {
        stickyRc = pPerf->SetVfePollingPeriodMs(m_OrigVfePollingPeriodMs);
    }

    stickyRc = pPerf->SetPerfPoint(m_OrigPerfPoint);
    m_PStateOwner.ReleasePStates();
    stickyRc = GpuTest::Cleanup();

    return stickyRc;
}

RC PwmMode::RunJsFunc(const char* jsFuncName)
{
    StickyRC stickyRc;

    JavaScriptPtr pJs;
    JsArray Args;
    jsval RetValJs = JSVAL_NULL;
    stickyRc = pJs->CallMethod(GetJSObject(), jsFuncName, Args, &RetValJs);
    if (OK == stickyRc)
    {
        UINT32 JSRetVal = RC::SOFTWARE_ERROR;
        stickyRc = pJs->FromJsval(RetValJs, &JSRetVal);
        stickyRc = JSRetVal;
    }

    return stickyRc;
}

