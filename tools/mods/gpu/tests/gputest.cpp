/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gputest.h"
#include "core/include/device.h"
#include "gpu/display/js_disp.h"
#include "gpu/js_gpusb.h"
#include "gpu/js_gpudv.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/gpumgr.h"
#include "gpu/lwlink/lwlinkimpl.h"
#include "core/include/display.h"
#include "core/include/jscript.h"
#include "core/include/mgrmgr.h"
#include "core/include/script.h"
#include "core/include/xp.h"
#include "core/include/jsonlog.h"
#include "core/include/errcount.h"
#include "core/include/platform.h"
#include "core/include/registry.h"
#include "gpu/perf/pwrsub.h"
#include "cheetah/include/tegrasocdevice.h"
#include "cheetah/include/tegrasocdeviceptr.h"
#include "core/include/help.h"
#include "gpu/include/testdevice.h"
#include "gpu/include/testdevicemgr.h"
#include "gpu/uphy/uphyreglogger.h"
#include "gpu/utility/ecccount.h"
#include "gpu/utility/edccount.h"
#include "gpu/utility/js_testdevice.h"
#include "gpu/utility/tpceccerror.h"
#include "gpu/utility/lwlink/lwlinkdevif/lwl_topology_mgr.h"
#include "core/include/deprecat.h"
#include <memory>

// When no display goldens are available, tests may attempt to force this EDID
// (similar to "-simulate_dfp") so that the test will still run and provide
// coverage even when the correct goldens for the current display are not
// present
//
// This EDID was taken from edid.js and is TMDS_1024x768_HN_VN_beXYOE_seYE
UINT08 GpuTest::s_DefaultTmdsGoldensEdid[] =
{
    0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,
    0x3A, 0xC4, 0x02, 0x00, 0x02, 0x00, 0x00, 0x00,
    0x1C, 0x09, 0x01, 0x02, 0x80, 0x1E, 0x17, 0xBC,
    0xEA, 0xA8, 0xE0, 0x99, 0x57, 0x4B, 0x92, 0x25,
    0x1C, 0x50, 0x54, 0x00, 0x00, 0x00, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x64, 0x19,
    0x00, 0x40, 0x41, 0x00, 0x26, 0x30, 0x18, 0x28,
    0x33, 0x12, 0x2C, 0xE4, 0x10, 0x00, 0x00, 0x18,
    0x00, 0x00, 0x00, 0xFD, 0x00, 0x28, 0x4B, 0x1E,
    0x50, 0x08, 0x00, 0x0A, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x20, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x30,
    0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
    0x30, 0x30, 0x31, 0x0A, 0x00, 0x00, 0x00, 0xFC,
    0x00, 0x4E, 0x56, 0x49, 0x44, 0x49, 0x41, 0x5F,
    0x58, 0x47, 0x41, 0x0A, 0x20, 0x20, 0x00, 0xEC,
};
// This EDID was taken from edid.js and is LVDS_1680x1050_HN_VN_beXYOO_seYO
UINT08 GpuTest::s_DefaultLvdsGoldensEdid[] =
{
    0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,
    0x4C, 0xA3, 0x50, 0x31, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x0C, 0x01, 0x03, 0x80, 0x21, 0x15, 0x78,
    0x0A, 0x87, 0xF5, 0x94, 0x57, 0x4F, 0x8C, 0x27,
    0x27, 0x50, 0x54, 0x00, 0x00, 0x00, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x95, 0x2E,
    0x90, 0xA0, 0x60, 0x1A, 0x1E, 0x40, 0x30, 0x20,
    0x26, 0x00, 0x4B, 0xCF, 0x10, 0x00, 0x00, 0x19,
    0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3C, 0xD2, 0x02,
    0x64, 0x00, 0x00, 0x00, 0x00, 0xFE, 0x00, 0x37,
    0x54, 0x37, 0x37, 0x34, 0x02, 0x31, 0x35, 0x34,
    0x50, 0x31, 0x0A, 0x20, 0x00, 0x00, 0x00, 0xFE,
    0x00, 0xE8, 0xC1, 0xAC, 0x95, 0x73, 0x50, 0x24,
    0x00, 0x02, 0x0A, 0x20, 0x20, 0x20, 0x00, 0x56,
};

// This EDID was taken from edid.js and is DP_1366x768_DpL1__HP_VN_beXYOO_seYE
UINT08 GpuTest::s_DefaultDpGoldensEdid[] =
{
    0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00,
    0x0d, 0xae, 0x92, 0x14, 0x00, 0x00, 0x00, 0x00,
    0x2d, 0x16, 0x01, 0x04, 0x95, 0x1f, 0x11, 0x78,
    0x02, 0xf8, 0x45, 0x96, 0x57, 0x54, 0x92, 0x28,
    0x23, 0x50, 0x54, 0x00, 0x00, 0x00, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0xda, 0x1d,
    0x56, 0xe2, 0x50, 0x00, 0x20, 0x30, 0x44, 0x2d,
    0x47, 0x00, 0x35, 0xae, 0x10, 0x00, 0x00, 0x1a,
    0xe7, 0x13, 0x56, 0xe2, 0x50, 0x00, 0x20, 0x30,
    0x44, 0x2d, 0x47, 0x00, 0x35, 0xae, 0x10, 0x00,
    0x00, 0x1a, 0x00, 0x00, 0x00, 0xfe, 0x00, 0x30,
    0x38, 0x48, 0x48, 0x32, 0x01, 0x31, 0x34, 0x30,
    0x42, 0x47, 0x45, 0x0a, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x41, 0x31, 0x94, 0x01, 0x10, 0x00,
    0x00, 0x09, 0x01, 0x0a, 0x20, 0x20, 0x00, 0xe3,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

//Disable ErrorChecking by default
UINT64 GpuTest::s_ErrorCheckPeriodMS = ~0ULL;

//------------------------------------------------------------------------------
GpuTest::GpuTest()
{
    SetName("GpuTest");
    m_hPreRunCallback = GetCallbacks(ModsTest::PRE_RUN)->Connect(&GpuTest::OnPreRun, this);
    m_hPostRunCallback = GetCallbacks(ModsTest::POST_RUN)->Connect(&GpuTest::OnPostRun, this);
    m_DidResolveMemErrorResult = false;
}

//------------------------------------------------------------------------------
GpuTest::~GpuTest()
{
    m_TpcEccErrorObjs.clear();

    if (m_hPreRunCallback)
        GetCallbacks(ModsTest::PRE_RUN)->Disconnect(m_hPreRunCallback);
    if (m_hPostRunCallback)
        GetCallbacks(ModsTest::POST_RUN)->Disconnect(m_hPostRunCallback);
    if (m_hPreSetupCallback)
        GetCallbacks(ModsTest::PRE_SETUP)->Disconnect(m_hPreSetupCallback);
    if (m_hPostCleanupCallback)
        GetCallbacks(ModsTest::POST_CLEANUP)->Disconnect(m_hPostCleanupCallback);
}

//------------------------------------------------------------------------------
bool GpuTest::IsSupported()
{
    if (m_TargetLwSwitch && GetBoundTestDevice()->GetType() != TestDevice::TYPE_LWIDIA_LWSWITCH)
        return false;

    return true;
}

//------------------------------------------------------------------------------
RC GpuTest::Setup()
{
    RC rc = OK;
    GpuSubdevice * pSubdev = GetBoundGpuSubdevice();

    if (RequiresSoftwareTreeValidation())
        CHECK_RC(ValidateSoftwareTree());

    CHECK_RC(InitFromJs());

    if (m_TestConfig.Verbose())
        m_VerbosePri = Tee::PriNormal;

    GpuDevice * pGpuDevice = GetBoundGpuDevice();
    if (pGpuDevice != nullptr)
    {
        m_TpcEccErrorObjs.reserve(pGpuDevice->GetNumSubdevices());
        m_ErrorObjs.reserve(pGpuDevice->GetNumSubdevices());
        for (UINT32 lwrSubIdx = 0; lwrSubIdx < pGpuDevice->GetNumSubdevices(); lwrSubIdx++)
        {
            GpuSubdevice *pSubdev = pGpuDevice->GetSubdevice(lwrSubIdx);
            m_TpcEccErrorObjs.push_back(unique_ptr<TpcEccError>(new TpcEccError(pSubdev,
                                                                                GetName())));
            m_ErrorObjs.push_back(make_unique<MemError>());
            GetMemError(lwrSubIdx).SetCriticalFbRange(pSubdev->GetCriticalFbRanges());
            GetMemError(lwrSubIdx).SetTestName(GetName());
            CHECK_RC(GetMemError(lwrSubIdx).SetupTest(pSubdev->GetFB()));
        }
    }

    PrintJsProperties(m_VerbosePri);

    // If necessary disable Robust Channels
    if (m_DisableRcWatchdog)
    {
        m_DisableRcWd.reset(new LwRm::DisableRcWatchdog(pSubdev));
    }

    if (m_ElpgForced)
    {
        PMU *pPmu;
        CHECK_RC(pSubdev->GetPmu(&pPmu));
        CHECK_RC(m_ElpgOwner.ClaimElpg(pPmu));
        CHECK_RC(pPmu->GetPowerGateEnableEngineMask(&m_OrgElpgMask));
        CHECK_RC(pPmu->SetPowerGateEnableEngineMask(m_ForcedElpgMask));
    }

    if (m_IssueRateOverride != "")
    {
        CHECK_RC(pSubdev->OverrideIssueRate(m_IssueRateOverride, &m_OrigSpeedSelect));
    }

    m_LastErrorCheckTimeMS = Platform::GetTimeMS();

    return rc;
}

//------------------------------------------------------------------------------
RC GpuTest::Cleanup()
{
    //Record performance data
    SetPerformanceMetric(ENCJSENT("NumCrcs"), m_Golden.GetNumSurfaceChecks());

    GpuSubdevice* pSubdev = GetBoundGpuSubdevice();
    StickyRC firstRc;

    for (unique_ptr<MemError> &pMemError : m_ErrorObjs)
    {
        if (pMemError->IsSetup())
        {
            if (!m_DidResolveMemErrorResult)
            {
                firstRc = pMemError->ResolveMemErrorResult();
            }
            firstRc = pMemError->Cleanup();
        }
    }
    m_DidResolveMemErrorResult = true;
    if (m_Golden.IsErrorRateTestCallMissing())
    {
        firstRc = RC::SOFTWARE_ERROR;
        Printf(Tee::PriError,
            "Possible software error: Missing ErrorRateTest call in \"%s\".",
            GetName().c_str());
    }

    if (m_IssueRateOverride != "")
    {
        firstRc = pSubdev->RestoreIssueRate(m_OrigSpeedSelect);
    }

    if (m_ElpgForced)
    {
        PMU *pPmu;
        firstRc = GetBoundGpuSubdevice()->GetPmu(&pPmu);
        if (pPmu && pPmu->IsElpgOwned())
        {
            firstRc = pPmu->SetPowerGateEnableEngineMask(m_OrgElpgMask);
            m_ElpgOwner.ReleaseElpg();
            m_ElpgForced = false;
            m_ForcedElpgMask = 0;
        }
    }

    if (m_FakedEdids)
    {
        Display *pDisplay = GetDisplay();
        pDisplay->ResetFakeEdids(m_FakedEdids);
        pDisplay->SetPendingSetModeChange();
        m_FakedEdids = 0;
    }

    if (m_pDisplayMgrTestContext)
    {
        if (m_TestConfig.DisableCrt())
            firstRc = GetDisplay()->TurnScreenOff(false);

        delete m_pDisplayMgrTestContext;
        m_pDisplayMgrTestContext = NULL;
    }

    if (m_DisableRcWatchdog)
    {
        m_DisableRcWd.reset();
    }

    if (!m_TpcEccErrorsResolved)
    {
        for (auto & pTpcEccError : m_TpcEccErrorObjs)
        {
            firstRc = pTpcEccError->ResolveResult();
        }
        m_TpcEccErrorsResolved = true;
    }
    m_TpcEccErrorObjs.clear();

    if (m_CheckPickers)
    {
        firstRc = GetFancyPickerArray()->CheckUsed();
    }

    return firstRc;
}

RC GpuTest::EndLoop(UINT32 loop, UINT64 mleProgress, RC rcFromRun)
{
    RC rc;
    CallbackArguments args;
    GpuSubdevice* pGpuSubdevice = GetBoundGpuSubdevice();
    args.Push(pGpuSubdevice->GetGpuInst());
    args.Push(GetName());
    args.Push(GetLoop());
    CHECK_RC(FireCallbacks(ModsTest::END_LOOP, Callbacks::STOP_ON_ERROR,
                           args, "EndLoop callback"));

    if ((Platform::GetTimeMS() - m_LastErrorCheckTimeMS) > s_ErrorCheckPeriodMS)
    {
        StickyRC errCountRc =
            pGpuSubdevice->GetEccErrCounter()->CheckCounterExitStackFrame(this, GetRunFrameId());
        errCountRc =
            pGpuSubdevice->GetEdcErrCounter()->CheckCounterExitStackFrame(this, GetRunFrameId());

        if (errCountRc != RC::OK)
        {
            if (mleProgress != NoLoopId)
            {
                PrintErrorUpdate(mleProgress, errCountRc);
            }
            if (m_TestConfig.EarlyExitOnErrCount())
            {
                GetJsonExit()->AddFailLoop(loop);
                return errCountRc;
            }
        }

        if (GetGoldelwalues()->GetStopOnError())
        {
            for (auto & pTpcEccError : m_TpcEccErrorObjs)
            {
                rc = pTpcEccError->ResolveResult();
                if (rc != RC::OK)
                {
                    if (loop != NoLoopId)
                    {
                        GetJsonExit()->AddFailLoop(loop);
                    }
                    m_TpcEccErrorsResolved = true;
                    return rc;
                }
            }
        }
        m_LastErrorCheckTimeMS = Platform::GetTimeMS();
    }

    if (rcFromRun != RC::OK)
    {
        PrintErrorUpdate(mleProgress, rcFromRun);
    }

    CHECK_RC(ModsTest::EndLoop());
    if (mleProgress != NoLoopId)
    {
        PrintProgressUpdate(mleProgress);
    }

    return rc;
}

RC GpuTest::EndLoop(UINT32 loop)
{
    RC rc = EndLoop(loop, NoLoopId, RC::OK);
    return rc;
}

RC GpuTest::EndLoopMLE(UINT64 mleProgress)
{
    RC rc = EndLoop(NoLoopId, mleProgress, RC::OK);
    return rc;
}
//-----------------------------------------------------------------------------
//! \brief Identify the type of ModsTest subclass
//!
/* virtual */ bool GpuTest::IsTestType(TestType tt)
{
    return (tt == GPU_TEST) || ModsTest::IsTestType(tt);
}

//! \brief Add in extra objects needed by a GpuTest.
//!
//! This function will create a TestConfiguration and a Golden object within
//! the given GpuTest object.
//------------------------------------------------------------------------------
RC GpuTest::AddExtraObjects(JSContext *cx, JSObject *obj)
{
    JS_CLASS(TestConfiguration);
    JS_CLASS(Golden);

    JSObject * testConfigObject;
    testConfigObject = JS_DefineObject(cx, obj, "TestConfiguration",
                                       &TestConfigurationClass, NULL, 0);
    if (!testConfigObject)
    {
        JS_ReportWarning(cx, "Unable to define TestConfiguration object");
        return RC::SOFTWARE_ERROR;
    }

    // Add the .Help() to this JSObject
    JS_DefineFunction(cx, testConfigObject, "Help", &C_Global_Help, 1, 0);

    JSObject * goldenObject;
    goldenObject = JS_DefineObject(cx, obj, "Golden",
                                   &GoldenClass, NULL, 0);
    if (!goldenObject)
    {
        JS_ReportWarning(cx, "Unable to define Golden object");
        return RC::SOFTWARE_ERROR;
    }

    // Add the .Help() to this JSObject
    JS_DefineFunction(cx, goldenObject, "Help", &C_Global_Help, 1, 0);

    return OK;
}

//------------------------------------------------------------------------------
static void GoldenJsonCallback
(
    void *   pvJsonItem
    ,UINT32  loop
    ,RC      rc
    ,UINT32  surf
)
{
    JsonItem * pJsonItem = (JsonItem *) pvJsonItem;
    pJsonItem->AddFailLoop(loop);
}

//! \brief Initialize all members to their default values with any JS overrides
//!
//! \note This function is called from Setup().
//!
//! \note If you redefine Setup() in a child class, make sure to call this
//! function.
//------------------------------------------------------------------------------
RC GpuTest::InitFromJs()
{
    RC rc = OK;

    CHECK_RC(ModsTest::InitFromJs());

    JSObject * pTestObj = GetJSObject();

    if (pTestObj)
    {
        CHECK_RC(m_TestConfig.InitFromJs(pTestObj));
        CHECK_RC(m_Golden.InitFromJs(pTestObj));

        if (m_TestConfig.UseOldRNG())
        {
            GetFpContext()->Rand.UseOld();
        }

        GetFpContext()->Rand.SeedRandom(m_TestConfig.Seed());
    }

    return rc;
}

//! \brief Print out information about all the JS accessable properties
void GpuTest::PrintJsProperties(Tee::Priority pri)
{
    const char * TF[2] = { "false", "true" };

    ModsTest::PrintJsProperties(pri);
    Printf(pri, "\tTesting device                : %s\n",
           GetBoundTestDevice()->GetName().c_str());
    Printf(pri, "\tDisableRcWatchdog             : %s\n", TF[m_DisableRcWatchdog]);
    Printf(pri, "\tPwrSampleIntervalMs           : %d\n", m_PwrSampleIntervalMs);
    Printf(pri, "\tTargetLwSwitch                : %s\n", TF[m_TargetLwSwitch]);
    Printf(pri, "\tCheckPickers                  : %s\n", TF[m_CheckPickers]);

    // Print the JS properties for Goldens and TestConfigs
    GetTestConfiguration()->PrintJsProperties(pri);
    GetGoldelwalues()->PrintJsProperties(pri);
    return;
}

//------------------------------------------------------------------------------
RC GpuTest::SetupJsonItems()
{
    RC rc;
    CHECK_RC(ModsTest::SetupJsonItems());

    // Tell our JsonItem what gpus we're going to use.
    JsArray jsaDevInsts;
    TestDevicePtr pTestDevice = GetBoundTestDevice();
    if (pTestDevice)
    {
        auto pSubdev = pTestDevice->GetInterface<GpuSubdevice>();
        if (pSubdev && GetTestsAllSubdevices())
        {
            auto pGpuDev = pSubdev->GetParentDevice();
            for (UINT32 i = 0; i < pGpuDev->GetNumSubdevices(); i++)
            {
                GpuSubdevice* pSub = pGpuDev->GetSubdevice(i);
                UINT32 devInst = pSub->DevInst();
                jsaDevInsts.push_back(INT_TO_JSVAL(devInst));
            }
        }
        else
        {
            jsaDevInsts.push_back(INT_TO_JSVAL(pTestDevice->DevInst()));
        }
    }

    GetJsonEnter()->SetField(ENCJSENT("dev_insts"), &jsaDevInsts);
    GetJsonExit()->SetField(ENCJSENT("dev_insts"), &jsaDevInsts);

    // Hook a Goldelwalues error callback to update the "exit" JsonItem.
    m_Golden.AddErrorCallback(&GoldenJsonCallback, GetJsonExit());

    return rc;
}

//! \brief Bind a RM client to the test
//!
//! This virtual function is overwritten to also bind the RM client to other
//! critical objects, such as the GpuTestConfiguration.
void GpuTest::BindRmClient(LwRm *pLwRm)
{
    m_TestConfig.BindRmClient(pLwRm);
    m_pLwRm = pLwRm;
}

//! \brief Bind a testdevice the test
RC GpuTest::BindTestDevice(TestDevicePtr pTestDevice)
{
    m_pTestDevice = pTestDevice;
    m_TestConfig.BindTestDevice(pTestDevice);
    m_Golden.BindTestDevice(pTestDevice);
    m_Golden.SetSubdeviceMask(GetSubdevMask());
    return RC::OK;
}

RC GpuTest::BindTestDevice(TestDevicePtr pTestDevice, JSContext *cx, JSObject *obj)
{
    RC rc;

    CHECK_RC(BindTestDevice(pTestDevice));

    m_JsTestDevice = new JsTestDevice();
    MASSERT(m_JsTestDevice);
    m_JsTestDevice->SetTestDevice(pTestDevice);
    CHECK_RC(m_JsTestDevice->CreateJSObject(cx, obj));

    auto pSubdev = pTestDevice->GetInterface<GpuSubdevice>();
    if (pSubdev)
    {
        m_JsGpuSubdevice = new JsGpuSubdevice();
        MASSERT(m_JsGpuSubdevice);
        m_JsGpuSubdevice->SetGpuSubdevice(pSubdev);
        CHECK_RC(m_JsGpuSubdevice->CreateJSObject(cx, obj));

        m_JsGpuDevice = new JsGpuDevice();
        MASSERT(m_JsGpuDevice);
        m_JsGpuDevice->SetGpuDevice(pSubdev->GetParentDevice());
        CHECK_RC(m_JsGpuDevice->CreateJSObject(cx, obj));
    }

    return OK;
}

//! \brief Return subdevice mask of tested subdevices
//!
UINT32 GpuTest::GetSubdevMask()
{
    if (GetTestsAllSubdevices())
    {
        return ~0U;
    }
    else
    {
        if (GetBoundGpuSubdevice())
        {
            return 1 << GetBoundGpuSubdevice()->GetSubdeviceInst();
        }
        else
        {
            return 0x1;
        }
    }
}

//! \brief Return the bound RM client
//!
//! This will return the base RM client if no client is bound.  This way, tests
//! which haven't had a RM client bound yet can still be updated and continue to
//! function.  Ideally, eventually this would MASSERT that a RM Client is
//! actually bound.
LwRm *GpuTest::GetBoundRmClient()
{
    if (!m_pLwRm)
    {
        return LwRmPtr(0).Get();
    }
    else
    {
        return m_pLwRm;
    }
}

//! \brief Return the bound device
//!
//! This will return the current device if no device is bound.  This way, tests
//! which haven't had a GPU bound yet can still be updated and continue to
//! function.  Ideally, eventually this would MASSERT that a GpuDevice is
//! actually bound.
GpuDevice *GpuTest::GetBoundGpuDevice()
{
    auto pSubdev = m_pTestDevice->GetInterface<GpuSubdevice>();
    if (pSubdev)
        return pSubdev->GetParentDevice();

    return nullptr;
}

//! \brief Return the bound subdevice
//!
//! This will return current subdevice if no subdevice is bound.  This way, tests
//! which haven't had a GPU bound yet can still be updated and continue to
//! function.  Ideally, eventually this would MASSERT that a GpuSubdevice is
//! actually bound.
GpuSubdevice *GpuTest::GetBoundGpuSubdevice()
{
    return m_pTestDevice->GetInterface<GpuSubdevice>();
}

DisplayMgr::TestContext * GpuTest::GetDisplayMgrTestContext() const
{
    return m_pDisplayMgrTestContext;
}

Display * GpuTest::GetDisplay()
{
    if (m_pDisplayMgrTestContext)
    {
        if (m_pDisplayMgrTestContext->IsCompositionEnabled() &&
            !m_pDisplayMgrTestContext->GetDisplay())
        {
            Printf(Tee::PriError, "Tests are no longer allowed to deal with Display directly\n");
            return nullptr;
        }
        // Get either the real or the NULL display, depending on
        // whether or not we are running off-screen.
        return m_pDisplayMgrTestContext->GetDisplay();
    }

    // Else this test isn't using DisplayMgr or hasn't yet
    // allocated its display -- use the real display.
    if (GetBoundGpuDevice())
        return GetBoundGpuDevice()->GetDisplay();

    return nullptr;
}

//! Private function: use JS requirements, but force to require HW display
//! if we're using DAC CRCs.
DisplayMgr::Requirements GpuTest::GetDispMgrReqs()
{
    DisplayMgr::Requirements reqs = m_TestConfig.DisplayMgrRequirements();

    if (m_Golden.GetCodes() &
        (Goldelwalues::DacCrc |
         Goldelwalues::TmdsCrc))
    {
        reqs = DisplayMgr::RequireRealDisplay;
    }
    return reqs;
}

RC GpuTest::GetDisplayPitch(UINT32 *pitch)
{
    if (m_pDisplayMgrTestContext)
    {
        return m_pDisplayMgrTestContext->GetPitch(pitch);
    }

    Display *display = GetBoundGpuDevice()->GetDisplay();
    return display->GetPitch(display->Selected(), pitch);
}

RC GpuTest::AdjustPitchForScanout(UINT32* pPitch)
{
    if (m_pDisplayMgrTestContext)
    {
        return m_pDisplayMgrTestContext->AdjustPitchForScanout(*pPitch, pPitch);
    }

    Display* const pDisplay = GetBoundGpuDevice()->GetDisplay();
    return pDisplay->AdjustPitchForScanout(pDisplay->Selected(), *pPitch, pPitch);
}

UINT32 GpuTest::GetPrimaryDisplay()
{
    if (m_pDisplayMgrTestContext)
    {
        return m_pDisplayMgrTestContext->GetPrimaryDisplay();
    }

    return GetBoundGpuDevice()->GetDisplay()->Selected();
}

//! Call DisplayMgr::SetupTest in the easy way.
RC GpuTest::AllocDisplayAndSurface(bool useBlockLinear)
{
    RC rc;
    CHECK_RC(GetBoundGpuDevice()->GetDisplayMgr()->SetupTest(
            GetDispMgrReqs(),
            m_TestConfig.DisplayWidth(),
            m_TestConfig.DisplayHeight(),
            m_TestConfig.SurfaceWidth(),
            m_TestConfig.SurfaceHeight(),
            ColorUtils::ColorDepthToFormat(m_TestConfig.DisplayDepth()),
            m_TestConfig.GetFSAAMode(),
            m_TestConfig.RefreshRate(),
            false, // isCompressed -- simple tests don't want this
            useBlockLinear,
            NULL,
            NULL,
            NULL,
            &m_pDisplayMgrTestContext));

    // Set display subdevices
    Display* const display = GetBoundGpuDevice()->GetDisplay();
    display->SetDisplaySubdevices(display->Selected(), GetSubdevMask());

    if (m_TestConfig.DisableCrt())
        GetDisplay()->TurnScreenOff(true);

    return rc;
}

//! Call DisplayMgr::SetupTest without allocating any primary surface.
RC GpuTest::AllocDisplay()
{
    RC rc;
    CHECK_RC(GetBoundGpuDevice()->GetDisplayMgr()->SetupTest(
            GetDispMgrReqs(),
            m_TestConfig.DisplayWidth(),
            m_TestConfig.DisplayHeight(),
            m_TestConfig.DisplayDepth(),
            m_TestConfig.RefreshRate(),
            &m_pDisplayMgrTestContext));

    // Set display subdevices
    Display* const display = GetBoundGpuDevice()->GetDisplay();
    display->SetDisplaySubdevices(display->Selected(), GetSubdevMask());

    if (m_TestConfig.DisableCrt())
        GetDisplay()->TurnScreenOff(true);

    return rc;
}

//------------------------------------------------------------------------------
// Protected Member functions
//------------------------------------------------------------------------------

//! \brief Allow subclasses to access the test configuration
//!
//! This accessor allows tests to grab info that TestConfiguration tracks for
//! us.
//!
//! \sa GpuTestConfiguration, TestConfiguration
//! \return The GpuTestConfiguration for this test, or NULL if there's a failure
//------------------------------------------------------------------------------
GpuTestConfiguration * GpuTest::GetTestConfiguration()
{
    // For now, can't fail
    return &m_TestConfig;
}
const GpuTestConfiguration * GpuTest::GetTestConfiguration() const
{
    // For now, can't fail
    return &m_TestConfig;
}

//! \brief Allow subclasses to access the golden values
//!
//! This accessor allows tests to grab info that Goldelwalues tracks for
//! us.
//!
//! \sa Goldelwalues
//! \return The Goldelwalues for this test, or NULL if there's a failure
//------------------------------------------------------------------------------
Goldelwalues * GpuTest::GetGoldelwalues()
{
    // For now, can't fail
    return &m_Golden;
}

//! \brief Setup the golden value surfaces
//!
//! This function sets the golden value surfaces for the test and if the
//! goldens do not exist and are display goldens, it attempts to fake an EDID
//! for goldens that should be present
//!
//! \param DisplayMask : Mask of displays to set the goldens on
//! \param pGoldSurfs  : Pointer to the golden surfaces to use
//! \param LoopOrDbIndex : Loop to use to validate goldens are present (will be
//!                        rounded up to next generated loop) or DbIndex for
//!                        goldens (which is used as is)
//! \param bDbIndex      : true if the previous parameter is a DbIndex
//!
//! \sa Goldelwalues
//! \return The Goldelwalues for this test, or NULL if there's a failure
//------------------------------------------------------------------------------
RC GpuTest::InnerSetupGoldens
(
    UINT32             DisplayMask,
    GoldenSurfaces    *pGoldSurfs,
    UINT32             LoopOrDbIndex,
    bool               bDbIndex
)
{
    RC rc;
    Display *pDisplay = GetDisplay();

    CHECK_RC(m_Golden.SetSurfaces(pGoldSurfs));

    CHECK_RC(SetGoldenName());

    if (!pDisplay->IsEdidFaked(DisplayMask) &&
        (m_Golden.GetAction() == Goldelwalues::Check) &&
        ((m_Golden.GetCodes() & Goldelwalues::TmdsCrc) ||
         ((m_Golden.GetCodes() & Goldelwalues::DacCrc) &&
          (pDisplay->GetDisplayType(DisplayMask) == Display::DFP))))
    {
        bool bGoldenPresent;

        CHECK_RC(m_Golden.IsGoldenPresent(LoopOrDbIndex, bDbIndex, &bGoldenPresent));
        if (!bGoldenPresent)
        {
            Display::Encoder encoder;
            UINT08 *pFakedEdid = s_DefaultTmdsGoldensEdid;
            UINT32  fakedEdidSize = sizeof(s_DefaultTmdsGoldensEdid);
            const char *displayTypeName = "TMDS";

            m_Golden.ClearSurfaces();

            CHECK_RC(pDisplay->GetEncoder(DisplayMask, &encoder));
            if (encoder.Signal == Display::Encoder::LVDS)
            {
                pFakedEdid = s_DefaultLvdsGoldensEdid;
                fakedEdidSize = sizeof(s_DefaultLvdsGoldensEdid);
                displayTypeName = "LVDS";
            }
            else if (encoder.Signal == Display::Encoder::DP)
            {
                pFakedEdid = s_DefaultDpGoldensEdid;
                fakedEdidSize = sizeof(s_DefaultDpGoldensEdid);
                displayTypeName = "DP";
            }

            rc = pDisplay->SetEdid(DisplayMask, pFakedEdid, fakedEdidSize);
            if (rc == OK)
            {
                m_FakedEdids |= DisplayMask;
                Printf(Tee::PriLow, "\tFaked EDID on display 0x%x.\n", DisplayMask);

                // Make sure to mark this display as forced
                pDisplay->SetForceDetectMask(pDisplay->GetForceDetectMask() | DisplayMask);
            }

            // Some properties affecting goldens (for example number of DP lanes)
            // can be determined only after a new modeset is exelwted. Force it here:
            UINT32 ModeWidth = 0;
            UINT32 ModeHeight = 0;
            UINT32 ModeDepth = 0;
            UINT32 ModeRefreshRate = 0;
            enum Display::FilterTaps ModeFilterTaps = Display::FilterTapsNoChange;
            enum Display::ColorCompression ModeColorComp = Display::ColorCompressionNoChange;
            UINT32 ModeDdScalerMode = 0;
            CHECK_RC(pDisplay->GetMode(
                DisplayMask,
                &ModeWidth,
                &ModeHeight,
                &ModeDepth,
                &ModeRefreshRate,
                &ModeFilterTaps,
                &ModeColorComp,
                &ModeDdScalerMode));

            CHECK_RC(pDisplay->DetachAllDisplays()); // It is needed to retrain DP
                                                     // to another link speed
            pDisplay->SetPendingSetModeChange();
            CHECK_RC(pDisplay->SetMode(
                DisplayMask,
                ModeWidth,
                ModeHeight,
                ModeDepth,
                ModeRefreshRate,
                ModeFilterTaps,
                ModeColorComp,
                ModeDdScalerMode));

            CHECK_RC(m_Golden.SetSurfaces(pGoldSurfs));
            CHECK_RC(SetGoldenName());
            CHECK_RC(m_Golden.IsGoldenPresent(LoopOrDbIndex, bDbIndex, &bGoldenPresent));
            if (!bGoldenPresent)
            {
                rc = RC::GOLDEN_VALUE_NOT_FOUND;
            }
            else
            {
                Printf(Tee::PriNormal,
                       "******************************************************"
                       "***********************************\n"
                       "* Goldens for display 0x%08x not present, simulating "
                       "%s display with a default EDID *\n"
                       "******************************************************"
                       "***********************************\n",
                       DisplayMask, displayTypeName);
            }
        }
    }

    return rc;
}

//! \brief Setup the golden value surfaces
//!
//! This function sets the golden value surfaces for the test and if the
//! goldens do not exist and are display goldens, it attempts to fake an EDID
//! for goldens that should be present
//!
//! \param DisplayMask   : Mask of displays to set the goldens on
//! \param pGoldSurfs    : Pointer to the golden surfaces to use
//! \param LoopOrDbIndex : Loop to use to validate goldens are present (will be
//!                        rounded up to next generated loop) or DbIndex for
//!                        goldens (which is used as is)
//! \param bDbIndex      : true if the previous parameter is a DbIndex
//! \param pbEdidFaked   : returned boolean indicating whether the EDID is
//!                        faked for this golden
//!
//! \sa Goldelwalues
//! \return OK if successful, not OK otherwise
//------------------------------------------------------------------------------
RC GpuTest::SetupGoldens
(
    UINT32             DisplayMask,
    GoldenSurfaces    *pGoldSurfs,
    UINT32             LoopOrDbIndex,
    bool               bDbIndex,
    bool              *pbEdidFaked
)
{
    RC rc;
    Display *pDisplay = GetDisplay();

    if (pbEdidFaked)
        *pbEdidFaked = false;

    rc = InnerSetupGoldens(DisplayMask, pGoldSurfs, LoopOrDbIndex, bDbIndex);

    if (rc == OK)
    {
        if (m_FakedEdids & DisplayMask)
        {
            if (pbEdidFaked)
                *pbEdidFaked = true;
        }
    }
    else
    {
        // Free golden buffer.
        m_Golden.ClearSurfaces();

        if (m_FakedEdids & DisplayMask)
        {
            pDisplay->ResetFakeEdids(DisplayMask);
            m_FakedEdids &= ~DisplayMask;
        }
    }

    return rc;
}

/* virtual */ RC GpuTest::SetGoldenName()
{
    return OK;
}

//! \brief Make sure that this GPU is supported in this tree.
RC GpuTest::ValidateSoftwareTree()
{
    return GetBoundTestDevice()->ValidateSoftwareTree();
}

//! This function is exported to JS below.  C++ side code can use it too.
UINT32 GpuTest::GetDeviceId()
{
    if (!GpuPtr()->IsMgrInit())
        return 0;

    return GetBoundTestDevice()->GetDeviceId();
}

RC GpuTest::OnPreTest(const CallbackArguments &args)
{
    RC rc;

    Perf *perf = GetBoundGpuSubdevice()->GetPerf();
    CHECK_RC(perf->GetLwrrentPerfPoint(&m_LastPerf));

    return OK;
}

MemError& GpuTest::GetMemError(UINT32 errObj)
{
    return *m_ErrorObjs[errObj];
}

RC GpuTest::OnPreRun(const CallbackArguments &args)
{
    // Multiple runs per setup are allowed, clear any lingering data
    for (auto &pMemError: GetMemErrors())
    {
        pMemError->DeleteErrorList();
    }
    m_DidResolveMemErrorResult = false;
    for (auto & pTpcEccError : m_TpcEccErrorObjs)
    {
        pTpcEccError->ClearErrors();
    }
    m_TpcEccErrorsResolved = false;

    RC rc;
    const UINT32 uphyLogMask = m_TestConfig.UphyLogMask() | UphyRegLogger::GetLogPointMask();
    if (uphyLogMask & UphyRegLogger::LogPoint::PreTest)
    {
        CHECK_RC(UphyRegLogger::LogRegs(GetBoundTestDevice(),
                                        UphyRegLogger::UphyInterface::All,
                                        UphyRegLogger::PreTest,
                                        RC::OK));
    }

    // Log GPU memory statistics before the test
    GpuSubdevice* const pSubdev = GetBoundGpuSubdevice();
    if (pSubdev && pSubdev->HasFeature(Device::GPUSUB_HAS_FB_HEAP))
    {
        UINT64 totalFree = 0;
        UINT64 totalHeap = 0;
        CHECK_RC(LwRmPtr()->VidHeapInfo(&totalFree, &totalHeap, nullptr, nullptr, GetBoundGpuDevice()));
        Printf(Tee::PriLow, "Allocated %llu MB out of total %llu MB in vidmem heap\n",
               (totalHeap - totalFree) / 1_MB, totalHeap / 1_MB);
    }
    return rc;
}

RC GpuTest::OnPostRun(const CallbackArguments &args)
{
    INT32 RcFromRun = OK;
    RC rc;
    CHECK_RC(args.At(0, &RcFromRun));
    if (RcFromRun != OK)
    {
        CHECK_RC(m_TestConfig.IdleAllChannels());
    }

    StickyRC rcFromTpcEccErrors;
    rcFromTpcEccErrors = ErrCounter::CheckExitStackFrame(this, GetRunFrameId());
    for (auto & pTpcEccError : m_TpcEccErrorObjs)
    {
        rcFromTpcEccErrors = pTpcEccError->ResolveResult();
    }
    m_TpcEccErrorsResolved = true;
    for (auto &pMemError: GetMemErrors())
    {
        if (pMemError->IsSetup())
        {
            if (0 != pMemError->GetErrorCount())
            {
                pMemError->AddErrCountsToJsonItem(GetJsonExit());
            }
            if (!m_DidResolveMemErrorResult)
            {
                rcFromTpcEccErrors = pMemError->ResolveMemErrorResult();
            }
        }
    }
    m_DidResolveMemErrorResult = true;

    const UINT32 uphyLogMask = m_TestConfig.UphyLogMask() | UphyRegLogger::GetLogPointMask();
    if ((uphyLogMask & UphyRegLogger::LogPoint::PostTest) ||
        ((RcFromRun != RC::OK) && (uphyLogMask & UphyRegLogger::LogPoint::PostTestError)))
    {
        UphyRegLogger::LogPoint logPoint = UphyRegLogger::LogPoint::PostTest;
        if (!(uphyLogMask & UphyRegLogger::LogPoint::PostTest) ||
            ((RcFromRun != RC::OK) && (uphyLogMask & UphyRegLogger::LogPoint::PostTestError)))
        {
            logPoint = UphyRegLogger::LogPoint::PostTestError;
        }

        CHECK_RC(UphyRegLogger::LogRegs(GetBoundTestDevice(),
                                        UphyRegLogger::UphyInterface::All,
                                        logPoint,
                                        RcFromRun));
    }
    return rcFromTpcEccErrors;
}

RC GpuTest::OnPostTest(const CallbackArguments &args)
{
    RC rc;
    Perf *perf = GetBoundGpuSubdevice()->GetPerf();

    Perf::PerfPoint lwrrPerf;
    CHECK_RC(perf->GetLwrrentPerfPoint(&lwrrPerf));

    if ((rc = perf->ComparePerfPoint(m_LastPerf,
                                     lwrrPerf,
                                     m_TestConfig.ClkChangeThreshold())) != OK)
    {
        Printf(Tee::PriNormal, "Perf settings changed during test run.\n");
        return rc;
    }

    return OK;
}

RC GpuTest::HookupPerfCallbacks()
{
    m_hPreSetupCallback =
        GetCallbacks(ModsTest::PRE_SETUP)->Connect(&GpuTest::OnPreTest, this);
    m_hPostCleanupCallback =
        GetCallbacks(ModsTest::POST_CLEANUP)->Connect(&GpuTest::OnPostTest, this);

    return OK;
}

void GpuTest::RecordBps
(
    double bytesReadOrWritten,
    double seconds,
    Tee::Priority pri
)
{
    GpuSubdevice * pGpuSub = GetBoundGpuSubdevice();
    if (pGpuSub->GetFB()->IsFbBroken())
    {
        Printf(pri, "Bps is not measured when mods is running with -fb_broken argument\n");
        return;
    }

    // Make visible to JS code after Run exits.
    if (seconds > 0.0)
        m_BytesReadOrWrittenPerSecond = bytesReadOrWritten / seconds;
    else
        m_BytesReadOrWrittenPerSecond = 0.0;

    // Add to .jso JSON logfile "exit" structure for this test.
    GetJsonExit()->SetField(ENCJSENT("Bps"), m_BytesReadOrWrittenPerSecond);
    GetJsonExit()->SetField(ENCJSENT("BpsBytes"), bytesReadOrWritten);
    GetJsonExit()->SetField(ENCJSENT("BpsSeconds"), seconds);

    // Add to mods.log.
    Printf(pri, "Bps: %.4f GB read or written per second (%.4f GB in %.3f sec)\n",
           m_BytesReadOrWrittenPerSecond/1e9,
           bytesReadOrWritten/1e9,
           seconds);

    //Record performance data
    SetPerformanceMetric(ENCJSENT("GbPerSec"), m_BytesReadOrWrittenPerSecond/1.0e9);

    UINT64 maxTheoreticalBw = 0U;
    if (pGpuSub->GetMemoryBandwidthBps(&maxTheoreticalBw) != RC::OK)
    {
        return;
    }

    MASSERT(maxTheoreticalBw != 0);
    Printf(pri, "Bps: %.1f%% percent of raw FB bw (%.4f GB per second)\n",
           m_BytesReadOrWrittenPerSecond * 100.0 / maxTheoreticalBw,
           maxTheoreticalBw / 1e9);
}

void GpuTest::RecordL2Bps
(
    double bytesRead,
    double bytesWritten,
    UINT64 avgL2FreqHz,
    double seconds,
    Tee::Priority pri
)
{
    GpuSubdevice * pGpuSub = GetBoundGpuSubdevice();

    UINT64 maxTheoreticalReadBw = 0U;
    UINT64 maxTheoreticalWriteBw = 0U;
    if (pGpuSub->GetMaxL2BandwidthBps(avgL2FreqHz, &maxTheoreticalReadBw, &maxTheoreticalWriteBw) == RC::OK)
    {
        if (maxTheoreticalReadBw != 0)
        {
            double bytesReadPerSec = 0.0;
            if (seconds > 0.0)
            {
                bytesReadPerSec = bytesRead / seconds;
                Printf(pri, "L2 read Bps: %.1f%% percent of raw L2 read bw (%.4f GB per second)\n",
                       bytesReadPerSec * 100.0 / maxTheoreticalReadBw,
                       maxTheoreticalReadBw / 1e9);
            }
        }

        if (maxTheoreticalWriteBw != 0)
        {
            double bytesWrittenPerSec = 0.0;
            if (seconds > 0.0)
            {
                bytesWrittenPerSec = bytesWritten / seconds;
                Printf(pri, "L2 write Bps: %.1f%% percent of raw L2 write bw (%.4f GB per second)\n",
                       bytesWrittenPerSec * 100.0 / maxTheoreticalWriteBw,
                       maxTheoreticalWriteBw / 1e9);
            }
        }
    }
}

void GpuTest::SetDisableWatchdog(bool disable)
{
    m_DisableRcWatchdog = disable;
}

bool GpuTest::GetDisableWatchdog()
{
    return m_DisableRcWatchdog;
}

RC GpuTest::SetForcedElpgMask(UINT32 Mask)
{
    if (GetBoundGpuSubdevice()->HasFeature(Device::GPUSUB_HAS_ELPG))
    {
        m_ForcedElpgMask = Mask;
        m_ElpgForced = true;
    }
    return OK;
}

UINT32 GpuTest::GetForcedElpgMask() const
{
    return m_ForcedElpgMask;
}

RC GpuTest::StartPowerThread()
{
    return StartPowerThread(false);
}

RC GpuTest::StartPowerThread(bool detachable)
{
    if (0 == m_PwrSampleIntervalMs)
        return OK;

    if (m_PwrThreadID != Tasker::NULL_THREAD)
    {
        MASSERT(!"Power thread already started");
        return RC::SOFTWARE_ERROR;
    }

    // Always detach on CheetAh
    if (GetBoundGpuSubdevice()->IsSOC())
    {
        detachable = true;
    }

    m_PwrThreadDetachable = detachable;

    const auto pPower = GetBoundGpuSubdevice()->GetPower();

    const UINT32 sensorMask = pPower ? pPower->GetPowerSensorMask() : 0U;

    if (!sensorMask)
    {
        Printf(Tee::PriLow, "No power sensors found.\n");
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }

    m_TotalPwr = BasicStatistics<UINT64>();
    m_PerChannelPwr.clear();
    m_PerChannelPwr.insert(m_PerChannelPwr.begin(),
                           LW2080_CTRL_PMGR_PWR_CHANNEL_MAX_CHANNELS, m_TotalPwr);
    m_StopPowerThreadEvent =
        Tasker::AllocEvent("StopPowerThreadEvent");
    if (m_StopPowerThreadEvent == NULL)
        return RC::SOFTWARE_ERROR;

    m_PwrThreadID = Tasker::CreateThread(PowerThread, this,
        Tasker::MIN_STACK_SIZE, "GpuTest::PowerThread");

    return OK;
}

void GpuTest::PowerThread(void *pThis)
{
    GpuTest *pGpuTest = static_cast<GpuTest*>(pThis);
    unique_ptr<Tasker::DetachThread> pDetach(
        pGpuTest->m_PwrThreadDetachable ? new Tasker::DetachThread : nullptr);
    Power*   pPower = pGpuTest->GetBoundGpuSubdevice()->GetPower();
    const UINT32 sensorMask = pPower->GetPowerSensorMask();
    FLOAT64  EventTimeoutMs = pGpuTest->m_PwrSampleIntervalMs;
    pGpuTest->m_TimeOfNextSample = Platform::GetTimeMS() +
        pGpuTest->m_PwrSampleIntervalMs;

    // Signal that we've gotten to the main part of the power thread
    pGpuTest->m_PwrThreadStarted = true;

    while (RC::TIMEOUT_ERROR ==
           Tasker::WaitOnEvent(pGpuTest->m_StopPowerThreadEvent,
           EventTimeoutMs))
    {
        if (!pGpuTest->m_PausePwrThread)
        {
            LW2080_CTRL_PMGR_PWR_MONITOR_GET_STATUS_PARAMS sample;
            if (pPower->GetPowerSensorSample(&sample) == OK)
            {
                pGpuTest->m_TotalPwr.AddSample(sample.totalGpuPowermW);

                for (UINT32 i = 0; i < pGpuTest->m_PerChannelPwr.size(); i++)
                {
                    if (sensorMask & (1<<i))
                    {
                        pGpuTest->m_PerChannelPwr[i].AddSample(
                            sample.channelStatus[i].tuplePolled.pwrmW);
                    }
                }
                pGpuTest->PowerControlCallback(sample.totalGpuPowermW);
            }
        }
        // WaitOnEvent can return late when other threads are busy,
        // adjust the next sample time.
        UINT64 LwrrentTime = Platform::GetTimeMS();
        do
        {
            pGpuTest->m_TimeOfNextSample += pGpuTest->m_PwrSampleIntervalMs;
        }
        while (LwrrentTime >= pGpuTest->m_TimeOfNextSample);

        EventTimeoutMs = FLOAT64(pGpuTest->m_TimeOfNextSample - LwrrentTime);
    }
}

namespace // anonymous namespace
{
RC AddMeanMaxProperties
(
    JSObject * powerObj,
    const char * channelName,
    double mean,
    UINT32 max
)
{
    JavaScriptPtr pJs;
    RC rc;
    JSObject * channelObj;
    if ((OK != pJs->GetProperty(powerObj, channelName, &channelObj)) ||
        (!channelObj))
    {
        CHECK_RC(pJs->DefineProperty(powerObj, channelName, &channelObj));
    }
    CHECK_RC(pJs->SetProperty(channelObj, "mean", mean));
    CHECK_RC(pJs->SetProperty(channelObj, "max", max));
    return rc;
}
};  // end anonymous namespace

RC GpuTest::StopPowerThread()
{
    if (0 == m_PwrSampleIntervalMs)
        return OK;

    StickyRC rc;
    const UINT32 sensorMask = GetBoundGpuSubdevice()->GetPower()->GetPowerSensorMask();

    if (m_PwrThreadID == Tasker::NULL_THREAD)
    {
        if (!sensorMask)
            return OK;
        else
            return RC::SOFTWARE_ERROR;
    }

    Tasker::SetEvent(m_StopPowerThreadEvent);
    rc = Tasker::Join(m_PwrThreadID);
    m_PwrThreadID = Tasker::NULL_THREAD;
    Tasker::FreeEvent(m_StopPowerThreadEvent);
    m_StopPowerThreadEvent = NULL;

    if (m_TotalPwr.NumSamples() > 0)
    {
        Power * pPower = GetBoundGpuSubdevice()->GetPower();
        const double totalMean = m_TotalPwr.Mean();

        rc = SetPerformanceMetric(ENCJSENT("AveragePowermW"), totalMean);

        // Add a tree of JS properties:
        // testObj.PowerMw.Total.mean
        // testObj.PowerMw.Total.max
        //  for each separately-measured chanel, i.e. INPUT_EXT12V_8PIN0:
        //   testObj.PowerMw.<chan>.mean
        //   testObj.PowerMw.<chan>.max

        JSObject * testObj = GetJSObject();
        JavaScriptPtr pJs;
        JSObject * powerObj;
        if ((OK != pJs->GetProperty(testObj, "PowerMw", &powerObj)) ||
            (!powerObj))
        {
            rc = pJs->DefineProperty(testObj, "PowerMw", &powerObj);
        }
        rc = AddMeanMaxProperties(powerObj, ENCJSENT("Total"), totalMean,
                                  static_cast<UINT32>(m_TotalPwr.Max()));

        for (UINT32 i = 0; i < m_PerChannelPwr.size(); i++)
        {
            const UINT32 mask = 1<<i;
            if (!(sensorMask & mask))
                continue;
            const double mean = m_PerChannelPwr[i].Mean();
            const UINT32 rail = pPower->GetPowerRail(mask);
            const char * name = pPower->PowerRailToStr(rail);
            rc = AddMeanMaxProperties(powerObj, name,
                                      mean,
                                      static_cast<UINT32>(m_PerChannelPwr[i].Max()));
        }
    }

    return rc;
}

void GpuTest::PausePowerThread(bool Pause)
{
    m_PausePwrThread = Pause;
}

void GpuTest::GpuHung(bool doExit)
{
    Printf(Tee::PriError, "The GPU may be in a bad state. Performing a dirty exit!\n");
    Tee::FlushToDisk();
    if (doExit)
    {
        RC rc = RC::CORE_GRAPHICS_CALL_HUNG;
        ErrorMap error(rc);
        Log::SetFirstError(error);
        JavaScriptPtr pJs;
        JSObject* pGlobObj = pJs->GetGlobalObject();
        pJs->CallMethod(pGlobObj, "PrintErrorWrapperPostTeardown");
        Tee::FlushToDisk();
        Utility::ExitMods(rc, Utility::ExitQuickAndDirty);
    }
}

//------------------------------------------------------------------------------
// Fire the ISM Trigger callback, just incase there is an experiment running.
RC GpuTest::FireIsmExpCallback(string subtestName)
{
    CallbackArguments args;
    args.Push(GetBoundGpuSubdevice()->GetGpuInst());
    args.Push(GetName());   // test name
    args.Push(subtestName);
    args.Push(GetLoop());
    return FireCallbacks(ModsTest::ISM_EXP, Callbacks::RUN_ON_ERROR,
                         args, "Ism Exp callback");
}

//------------------------------------------------------------------------------
void GpuTest::VerbosePrintf(const char* format, ...) const
{
    va_list args;
    va_start(args, format);

    ModsExterlwAPrintf(
            GetVerbosePrintPri(),
            Tee::GetTeeModuleCoreCode(),
            Tee::SPS_NORMAL,
            format,
            args);

    va_end(args);
}

//-----------------------------------------------------------------------------
RC GpuTest::SysmemUsesLwLink(bool* pbSysmemUsesLwLink)
{
    MASSERT(pbSysmemUsesLwLink);

    *pbSysmemUsesLwLink = false;

#if defined(INCLUDE_LWLINK)
    TestDevicePtr pTestDevice = GetBoundTestDevice();
    if (!pTestDevice->SupportsInterface<LwLink>())
        return OK;

    vector<LwLinkRoutePtr> routes;
    RC rc;
    CHECK_RC(LwLinkDevIf::TopologyManager::GetRoutesOnDevice(pTestDevice, &routes));

    for (auto& route : routes)
    {
        if (route->IsSysmem())
        {
            *pbSysmemUsesLwLink = true;
            break;
        }
    }
#endif

    return OK;
}

//------------------------------------------------------------------------------
// JS linkage
//------------------------------------------------------------------------------
JS_CLASS_VIRTUAL_INHERIT(GpuTest, ModsTest, "GPU test base class");

void GpuTest::PropertyHelp(JSContext *pCtx, regex *pRegex)
{
    JSObject* pLwrJObj = JS_NewObject(pCtx, &GpuTest_class, 0, 0);
    JS_SetPrivate(pCtx, pLwrJObj, this);
    Help::JsPropertyHelp(pLwrJObj, pCtx, pRegex, 0);
    JS_SetPrivate(pCtx, pLwrJObj, nullptr);
    ModsTest::PropertyHelp(pCtx, pRegex);
}

CLASS_PROP_READONLY(GpuTest, DeviceId, UINT32,
                    "LW DeviceId for bound GPU device.");
CLASS_PROP_READONLY(GpuTest, TestsAllSubdevices, bool,
                    "This test operates on all subdevices of an SLI device.");
CLASS_PROP_READONLY(GpuTest, Bps, double,
                    "Bytes read or written per second in last Run.");
CLASS_PROP_READWRITE(GpuTest, DisableRcWatchdog, bool,
                    "Disable robust channel watchdog.");
CLASS_PROP_READWRITE(GpuTest, ForcedElpgMask, UINT32,
                    "Enable/Disable a partilwalr power gating mask");
CLASS_PROP_READWRITE(GpuTest, PwrSampleIntervalMs, UINT32,
                    "Interval in miliseconds between power readings");
CLASS_PROP_READWRITE(GpuTest, TargetLwSwitch, bool,
                     "The GpuTest target is an lwswitch instead of a GPU(s)");
CLASS_PROP_READWRITE(GpuTest, CheckPickers, bool,
                     "Verify fancy pickers.");
CLASS_PROP_READWRITE(GpuTest, IssueRateOverride, string,
                     "Array of overrides for Volta-953. Provide the Unit to override followed "
                     "by how much to divide the issue rate by ( e.g. \"FFMA, 1, IMLA0, 2\" )\n");

JS_STEST_BIND_NO_ARGS(GpuTest, HookupPerfCallbacks,
            "Hookup the callbacks to verify perf settings post run");

JS_STEST_LWSTOM(GpuTest,
                BindRmClient,
                1,
                "Bind the GPU test to a particular RM client instance.")
{
    MASSERT(pContext != 0);
    MASSERT(pObject != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJs;

    // Check the arguments.
    UINT32 Client;
    if (1 != NumArguments
        || (OK != pJs->FromJsval(pArguments[0], &Client))
        )
    {
        JS_ReportError(pContext,
                       "Usage: GpuTest.BindRmClient(ClientInstance)");
        return JS_FALSE;
    }

    if (Client >= LwRmPtr::GetValidClientCount())
    {
        JS_ReportError(pContext, "Client %d is not defined.", Client);
        RETURN_RC(RC::WAS_NOT_INITIALIZED);
    }

    GpuTest *pTest;
    if ((pTest = JS_GET_PRIVATE(GpuTest, pContext, pObject, NULL)) != 0)
    {
        pTest->BindRmClient(LwRmPtr(Client).Get());
        RETURN_RC(OK);
    }

    return JS_FALSE;
}

JS_STEST_LWSTOM(GpuTest,
                BindTestDevice,
                1,
                "Bind the GPU test to a particular testdevice instance.")
{
    STEST_HEADER(1, 1, "Usage: GpuTest.BindTestDevice(testdevice)");
    STEST_ARG(0, JSObject*, devObj);

    RC rc;
    JsTestDevice* pJsTestDevice = nullptr;
    if ((pJsTestDevice = JS_GET_PRIVATE(JsTestDevice, pContext, devObj, "TestDevice")) == nullptr)
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    STEST_PRIVATE(GpuTest, pTest, NULL);

    TestDevicePtr pTestDevice = pJsTestDevice->GetTestDevice();
    if (pTestDevice.get() == nullptr)
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    ErrorMap::SetDevInst(pTestDevice->DevInst());
    C_CHECK_RC(pTest->BindTestDevice(pTestDevice, pContext, pObject));

    RETURN_RC(OK);
}

JS_STEST_LWSTOM(GpuTest,
                BindLwSwitchDevice,
                1,
                "Bind the Gpu test to a particular LwSwitch device instance.")
{
    STEST_HEADER(1, 1, "Usage: GpuTest.BindLwSwitchDevice(LwSwitchDeviceInstance)");
    STEST_PRIVATE(GpuTest, pTest, NULL);
    STEST_ARG(0, UINT32, devInst);

    static Deprecation depr("GpuTest.BindLwSwitchDevice", "3/1/2018",
                            "BindLwSwitchDevice is deprecated. Please use BindTestDevice.\n");
    if (!depr.IsAllowed(pContext))
        return JS_FALSE;

    if (DevMgrMgr::d_TestDeviceMgr == 0)
    {
        JS_ReportWarning(pContext, "TestDeviceMgr is not initialized.");
        RETURN_RC(RC::WAS_NOT_INITIALIZED);
    }
    TestDevicePtr pTestDevice;
    TestDeviceMgr* pTestDeviceMgr = static_cast<TestDeviceMgr*>(DevMgrMgr::d_TestDeviceMgr);
    if (OK == pTestDeviceMgr->GetDevice(TestDevice::TYPE_LWIDIA_LWSWITCH, devInst, &pTestDevice))
    {
        pTest->BindTestDevice(pTestDevice, pContext, pObject);
    }
    // Don't fail if we can't find one because there are lots of systems that
    // will not have an LwSwitch device. The caller should make sure
    // pObject.BoundLwLinkDev != "undefined".
    RETURN_RC(OK);
}

JS_STEST_LWSTOM(GpuTest,
                BindGpuDevice,
                1,
                "Bind the GPU test to a particular GPU device instance.")
{
    MASSERT(pContext != 0);
    MASSERT(pObject != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);
    RC rc;
    JavaScriptPtr pJs;
    // Check the arguments.
    UINT32 Dev;
    if (1 != NumArguments
        || (OK != pJs->FromJsval(pArguments[0], &Dev))
        )
    {
        JS_ReportError(pContext,
                       "Usage: GpuTest.BindGpuDevice(DeviceInstance)");
        return JS_FALSE;
    }

    static Deprecation depr("GpuTest.BindGpuDevice", "3/1/2018",
                            "BindGpuDevice is deprecated. Please use BindTestDevice.\n");
    if (!depr.IsAllowed(pContext))
        return JS_FALSE;

    if (DevMgrMgr::d_GraphDevMgr == 0)
    {
        JS_ReportWarning(pContext,
                         "GpuDevMgr is not initialized.");
        RETURN_RC(RC::WAS_NOT_INITIALIZED);
    }
    if (DevMgrMgr::d_TestDeviceMgr == 0)
    {
        JS_ReportWarning(pContext, "TestDeviceMgr is not initialized.");
        RETURN_RC(RC::WAS_NOT_INITIALIZED);
    }
    TestDeviceMgr* pTestDeviceMgr = static_cast<TestDeviceMgr*>(DevMgrMgr::d_TestDeviceMgr);

    GpuTest *pTest;
    if ((pTest = JS_GET_PRIVATE(GpuTest, pContext, pObject, NULL)) != 0)
    {
        GpuDevice *GpuDev = 0;
        C_CHECK_RC(DevMgrMgr::d_GraphDevMgr->GetDevice(Dev, (Device**)&GpuDev));
        TestDevicePtr pTestDevice;
        C_CHECK_RC(pTestDeviceMgr->GetDevice(GpuDev->GetSubdevice(0)->DevInst(), &pTestDevice));
        ErrorMap::SetDevInst(pTestDevice->DevInst());
        pTest->BindTestDevice(pTestDevice, pContext, pObject);
        RETURN_RC(OK);
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(GpuTest,
                BindGpuSubdevice,
                1,
                "Bind the GPU test to a particular GPU Subdevice instance.")
{
    MASSERT(pContext != 0);
    MASSERT(pObject != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);
    JavaScriptPtr pJs;
    RC rc;
    // Check the arguments.
    UINT32 SubDev;
    if (1 != NumArguments
        || (OK != pJs->FromJsval(pArguments[0], &SubDev))
        )
    {
        JS_ReportError(pContext,
                       "Usage: GpuTest.BindGpuSubdevice(DeviceInstance)");
        return JS_FALSE;
    }

    static Deprecation depr("GpuTest.BindGpuSubdevice", "3/1/2018",
                            "BindGpuSubdevice is deprecated. Please use BindTestDevice.\n");
    if (!depr.IsAllowed(pContext))
        return JS_FALSE;

    if (DevMgrMgr::d_GraphDevMgr == 0)
    {
        JS_ReportWarning(pContext,
                         "GpuDevMgr is not initialized.");
        RETURN_RC(RC::WAS_NOT_INITIALIZED);
    }
    if (DevMgrMgr::d_TestDeviceMgr == 0)
    {
        JS_ReportWarning(pContext, "TestDeviceMgr is not initialized.");
        RETURN_RC(RC::WAS_NOT_INITIALIZED);
    }
    TestDeviceMgr* pTestDeviceMgr = static_cast<TestDeviceMgr*>(DevMgrMgr::d_TestDeviceMgr);

    GpuTest *pTest;
    if ((pTest = JS_GET_PRIVATE(GpuTest, pContext, pObject, NULL)) != 0)
    {
        if (SubDev >= pTest->GetBoundGpuDevice()->GetNumSubdevices())
        {
            JS_ReportWarning(pContext,
                             "No such device number.");
            RETURN_RC(RC::ILWALID_DEVICE_ID);
        }
        GpuSubdevice *GpuSubdev = 0;
        GpuSubdev = pTest->GetBoundGpuDevice()->GetSubdevice(SubDev);
        TestDevicePtr pTestDevice;
        C_CHECK_RC(pTestDeviceMgr->GetDevice(GpuSubdev->DevInst(), &pTestDevice));
        pTest->BindTestDevice(pTestDevice, pContext, pObject);
        RETURN_RC(OK);
    }
    return JS_FALSE;
}
