/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/tests/lwca/lwda_lwlink.h"
#include "gpu/tests/lwca/lwlwl/lwlwl_alloc_mgr.h"
#include "gpu/tests/lwca/lwlwl/lwlwl_test_route.h"
#include "gpu/tests/lwca/lwlwl/lwlwl_test_mode.h"

#include "gpu/utility/chanwmgr.h"

#include "core/include/utility.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/testdevice.h"

using namespace MfgTest;
using namespace LwLinkDataTestHelper;

//------------------------------------------------------------------------------
//! \brief Constructor
//!
LwdaLwLink::LwdaLwLink()
{
    SetName("LwdaLwLink");
}

//------------------------------------------------------------------------------
//! \brief Destructor
//!
LwdaLwLink::~LwdaLwLink()
{
}

//------------------------------------------------------------------------------
//! \brief Check if the test is supported.
//!
//! \return true if supported, false otherwise
/* virtual */ bool LwdaLwLink::IsSupported()
{
    if (!LwLinkDataTest::IsSupported())
        return false;

    if (GetBoundTestDevice()->IsSOC())
        return false;

    if (ChannelWrapperMgr::Instance()->UsesRunlistWrapper())
        return false;

    if (!m_LwdaDevice.IsValid())
        return false;

    // The test requires UVA for both P2P access and GPU access to sysmem
    if (!m_LwdaDevice.GetAttribute(LW_DEVICE_ATTRIBUTE_UNIFIED_ADDRESSING))
        return false;

    // The kernel is compiled only for compute 6.x+
    if (m_LwdaDevice.GetCapability() <= Lwca::Capability::SM_60)
        return false;

    return true;
}

//------------------------------------------------------------------------------
RC LwdaLwLink::BindTestDevice(TestDevicePtr pTestDevice, JSContext* cx, JSObject* obj)
{
    RC rc;
    CHECK_RC(LwLinkDataTest::BindTestDevice(pTestDevice, cx, obj));

    auto pSubdev = pTestDevice->GetInterface<GpuSubdevice>();
    if (OK == m_Lwda.Init() && pSubdev)
    {
        m_Lwda.FindDevice(*(pSubdev->GetParentDevice()), &m_LwdaDevice);
    }
    return OK;
}

//------------------------------------------------------------------------------
/* virtual */ RC LwdaLwLink::Setup()
{
    RC rc;
    CHECK_RC(m_Lwda.InitContext(m_LwdaDevice));
    CHECK_RC(LwLinkDataTest::Setup());
    return rc;
}

//------------------------------------------------------------------------------
RC LwdaLwLink::Cleanup()
{
    RC rc = LwLinkDataTest::Cleanup();
    m_Lwda.Free();
    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Print the test properties
//!
void LwdaLwLink::PrintJsProperties(Tee::Priority pri)
{
    Printf(pri, "%s Js Properties:\n", ModsTest::GetName().c_str());
    Printf(pri, "\tP2PSmAllocMode:        %u\n", m_P2PSmAllocMode);
    string tempStr = "ALL";
    if (m_MaxPerRtePerDirSms != TestRouteLwda::ALL_SMS)
        tempStr = Utility::StrPrintf("%u", m_MaxPerRtePerDirSms);
    Printf(pri, "\tMaxPerRtePerDirSms:    %s\n", tempStr.c_str());
    tempStr = "ALL";
    if (m_MinPerRtePerDirSms != TestRouteLwda::ALL_SMS)
        tempStr = Utility::StrPrintf("%u", m_MinPerRtePerDirSms);
    Printf(pri, "\tMinPerRtePerDirSms:    %s\n", tempStr.c_str());
    Printf(pri, "\tAutoSmLimits:          %s\n", m_AutoSmLimits? "true" : "false");
    Printf(pri, "\tDataTransferWidth:     %u\n", m_DataTransferWidth);
    Printf(pri, "\tWarmupSec:             %u\n", m_WarmupSec);

    switch (m_SubtestType)
    {
        case TestModeLwda::ST_GUPS:
            tempStr = "GUPS";
            break;
        case TestModeLwda::ST_CE2D:
            tempStr = "CE2D";
            break;
        default:
            tempStr = "UNKNOWN";
            break;
    }
    Printf(pri, "\tSubtestType:           %s\n", tempStr.c_str());

    if (m_SubtestType == TestModeLwda::ST_CE2D)
    {
        Printf(pri, "\tCe2dMemorySizeFactor:  %u\n", m_Ce2dMemorySizeFactor);
    }
    if (m_SubtestType == TestModeLwda::ST_GUPS)
    {
        Printf(pri, "\tGupsUnroll:            %u\n", m_GupsUnroll);
        Printf(pri, "\tGupsAtomicWrite:       %s\n", m_GupsAtomicWrite ? "true" : "false");
        Printf(pri, "\tGupsMemIdxType:        %s\n",
               m_GupsMemIdxType == LwLinkKernel::LINEAR ? "Linear" : "Random");
        Printf(pri, "\tGupsMemorySize:        %u\n", m_GupsMemorySize);
    }

    LwLinkDataTest::PrintJsProperties(pri);
}

//------------------------------------------------------------------------------
// Assign the hardware to perform the copies later.  Note this doesn't actually
// perform any allocation but rather determines what HW is necessary to accomplish
// all the data transfers specified by the test mode
/* virtual */ RC LwdaLwLink::AssignTestTransferHw
(
    TestModePtr pTm
)
{
    // This dynamic cast should always return non-null since test modes used in
    // this test will have been created from CreateTestMode()
    TestModeLwda *pLwdaLwlTm = dynamic_cast<TestModeLwda *>(pTm.get());
    MASSERT(pLwdaLwlTm);

    return AssignSms(pLwdaLwlTm);
}

//------------------------------------------------------------------------------
/* virtual */ AllocMgrPtr LwdaLwLink::CreateAllocMgr()
{
    return AllocMgrPtr(new AllocMgrLwda(&m_Lwda,
                                        GpuTest::GetTestConfiguration(),
                                        GpuTest::GetVerbosePrintPri(),
                                        true));
}

//------------------------------------------------------------------------------
/* virtual */ TestRoutePtr LwdaLwLink::CreateTestRoute
(
    TestDevicePtr  pLwLinkDev,
    LwLinkRoutePtr pRoute
)
{
    return TestRoutePtr(new TestRouteLwda(&m_Lwda,
                                          pLwLinkDev,
                                          pRoute,
                                          GpuTest::GetVerbosePrintPri()));
}

//------------------------------------------------------------------------------
/* virtual */ TestModePtr LwdaLwLink::CreateTestMode()
{
    return TestModePtr(new TestModeLwda(&m_Lwda,
                                        GpuTest::GetTestConfiguration(),
                                        GetAllocMgr(),
                                        GetNumErrorsToPrint(),
                                        GetCopyLoopCount(),
                                        GetBwThresholdPct(),
                                        GetShowBandwidthData(),
                                        GetPauseTestMask(),
                                        GpuTest::GetVerbosePrintPri()));
}

//------------------------------------------------------------------------------
/* virtual */  RC LwdaLwLink::InitializeAllocMgr()
{
    return OK;
}

//------------------------------------------------------------------------------
/* virtual */ void LwdaLwLink::ConfigureTestMode
(
    TestModePtr pTm,
    TransferDir td,
    TransferDir sysmemTd,
    bool        bLoopbackInput
)
{
    MfgTest::LwLinkDataTest::ConfigureTestMode(pTm, td, sysmemTd, bLoopbackInput);

    // This dynamic cast should always return non-null since test modes used in
    // this test will have been created from CreateTestMode()
    TestModeLwda *pLwdaTm = dynamic_cast<TestModeLwda *>(pTm.get());
    MASSERT(pLwdaTm);

    pLwdaTm->SetP2PAllocMode(static_cast<TestModeLwda::P2PAllocMode>(m_P2PSmAllocMode));
    pLwdaTm->SetCe2dMemorySizeFactor(m_Ce2dMemorySizeFactor);
    pLwdaTm->SetTransferWidth(static_cast<TestModeLwda::TransferWidth>(m_DataTransferWidth));
    pLwdaTm->SetRuntimeSec(GetPerModeRuntimeSec());
    pLwdaTm->SetWarmupSec(m_WarmupSec);
    if (GetPerModeRuntimeSec() && (GetCopyLoopCount() == 0))
        pLwdaTm->SetCopyLoopCount(1);

    const TestModeLwda::SubtestType st = static_cast<TestModeLwda::SubtestType>(m_SubtestType);
    pLwdaTm->SetSubtestType(st);
    if (st == TestModeLwda::ST_GUPS)
    {
        pLwdaTm->ConfigureGups(m_GupsUnroll,
                               m_GupsAtomicWrite,
                               static_cast<TestModeLwda::GupsMemIndexType>(m_GupsMemIdxType),
                               m_GupsMemorySize);
    }
}

//------------------------------------------------------------------------------
/* virtual */ RC LwdaLwLink::ValidateTestSetup()
{
    StickyRC rc;

    rc = LwLinkDataTest::ValidateTestSetup();

    const TestModeLwda::SubtestType st = static_cast<TestModeLwda::SubtestType>(m_SubtestType);
    switch (static_cast<TestModeLwda::TransferWidth>(m_DataTransferWidth))
    {
        case TestModeLwda::TW_4BYTES:
        case TestModeLwda::TW_8BYTES:
        case TestModeLwda::TW_16BYTES:
            if ((st != TestModeLwda::ST_GUPS) || (m_GupsMemIdxType != LwLinkKernel::RANDOM))
            {
                Printf(Tee::PriError,
                       "%s : 4, 8, and 16 byte transfers are only supported in GUPS with "
                       "random access!\n",
                       ModsTest::GetName().c_str());
                rc = RC::BAD_PARAMETER;
            }
            break;
        case TestModeLwda::TW_128BYTES:
            if ((st == TestModeLwda::ST_GUPS) && (m_GupsMemIdxType != LwLinkKernel::LINEAR))
            {
                Printf(Tee::PriError,
                       "%s : 128 byte transfers in GUPS is only supported with linear indexing!\n",
                       ModsTest::GetName().c_str());
                rc = RC::BAD_PARAMETER;
            }
            if (st == TestModeLwda::ST_GUPS)
            {
                Printf(Tee::PriWarn,
                       "%s : 128 byte transfers in GUPS are not perfectly batched, bandwidth "
                       "will likely fail!\n",
                       ModsTest::GetName().c_str());
            }
            break;
        default:
            Printf(Tee::PriError, "%s : Invalid transfer width %u!\n",
                   ModsTest::GetName().c_str(), m_DataTransferWidth);
            rc = RC::BAD_PARAMETER;
            break;
    }

    switch (st)
    {
        case TestModeLwda::ST_GUPS:
            if ((m_GupsUnroll == 0) || (m_GupsUnroll > 8) || (m_GupsUnroll & (m_GupsUnroll - 1)))
            {
                Printf(Tee::PriError,
                       "%s : GUPS testing only supports unroll factors of 1, 2, 4, 8\n",
                       __FUNCTION__);
                rc = RC::BAD_PARAMETER;
            }
            if ((m_GupsMemIdxType != LwLinkKernel::RANDOM) &&
                (m_GupsMemIdxType != LwLinkKernel::LINEAR))
            {
                Printf(Tee::PriError, "%s : Unknown GUPS memory indexing value %d\n",
                       __FUNCTION__, m_GupsMemIdxType);
                rc = RC::BAD_PARAMETER;
            }
            break;
        case TestModeLwda::ST_CE2D:
            Printf(Tee::PriError, "%s : CE 2D emulation subtest not lwrrently supported!\n",
                   ModsTest::GetName().c_str());
            rc = RC::BAD_PARAMETER;
            break;
        default:
            Printf(Tee::PriError, "%s : Invalid subtest type %u!\n",
                   ModsTest::GetName().c_str(), m_SubtestType);
            rc = RC::BAD_PARAMETER;
            break;
    }

    return rc;
}

//------------------------------------------------------------------------------
RC LwdaLwLink::AssignSms(LwLinkDataTestHelper::TestModeLwda * pTm)
{
    bool bSmLimitsExceeded;
    RC  rc = pTm->AssignSms(m_MinPerRtePerDirSms,
                            m_MaxPerRtePerDirSms,
                            &bSmLimitsExceeded);

    if ((rc != OK) && m_AutoSmLimits && bSmLimitsExceeded)
    {
        if (m_MaxPerRtePerDirSms == TestRouteLwda::ALL_SMS)
            m_MaxPerRtePerDirSms = pTm->GetMaxSms();
        else
            m_MaxPerRtePerDirSms --;
        m_MinPerRtePerDirSms = 1;
        while (m_MaxPerRtePerDirSms && (rc != OK) && bSmLimitsExceeded)
        {
            Printf(GpuTest::GetVerbosePrintPri(),
                   "%s : Auto adjusting SM assignment limits, minimum SMs=%u,"
                   " maximum SMs=%u\n",
                   ModsTest::GetName().c_str(),
                   m_MinPerRtePerDirSms,
                   m_MaxPerRtePerDirSms);
            rc.Clear();
            rc = pTm->AssignSms(m_MinPerRtePerDirSms,
                                m_MaxPerRtePerDirSms,
                                &bSmLimitsExceeded);
            if (rc != OK)
                m_MaxPerRtePerDirSms --;
        }
        if (rc != OK)
        {
            Printf(Tee::PriWarn, "%s : No auto adjusted SM limit found!\n",
                   ModsTest::GetName().c_str());
        }
    }

    if (rc != OK)
    {
        Printf(Tee::PriNormal,
               "%s : Unable to assign SMs, skipping test mode!\n",
               ModsTest::GetName().c_str());
        pTm->Print(Tee::PriNormal, "   ");
    }

    return rc;
}

//-----------------------------------------------------------------------------
// Exposed JS properties
JS_CLASS_INHERIT(LwdaLwLink, LwLinkDataTest, "Lwca based lwlink data transer test");

CLASS_PROP_READWRITE(LwdaLwLink, Ce2dMemorySizeFactor, UINT32,
    "Size of transfer - multiplier to TestConfiguration.SurfaceHeight for CE2D subtest (default = 10)"); //$
CLASS_PROP_READWRITE(LwdaLwLink, P2PSmAllocMode, UINT32,
                     "Allocation mode for P2P SMs");
CLASS_PROP_READWRITE(LwdaLwLink, MinPerRtePerDirSms, UINT32,
                     "Minimum per-route per-direction SMs to use (default = 1)");
CLASS_PROP_READWRITE(LwdaLwLink, MaxPerRtePerDirSms, UINT32,
                     "Maximum per-route per-direction SMs to use (default = ALL_SMS)");
CLASS_PROP_READWRITE(LwdaLwLink, AutoSmLimits, bool,
               "Automatically adjust SM limits if SM assignment fails (default = false)");
CLASS_PROP_READWRITE(LwdaLwLink, DataTransferWidth, UINT32,
                     "Set data transer width (default = TW_8BYTES)");
CLASS_PROP_READWRITE(LwdaLwLink, SubtestType, UINT32,
                     "Lwca LwLink subtest to run (default = ST_GUPS)");
CLASS_PROP_READWRITE(LwdaLwLink, GupsUnroll, UINT32,
                     "Unroll factor for GPUS test (1, 2, 4, or 8) (default = 8)");
CLASS_PROP_READWRITE(LwdaLwLink, GupsAtomicWrite, bool,
              "True to use atomics for writing during GUPS rather than stores (default = false)");
CLASS_PROP_READWRITE(LwdaLwLink, GupsMemIdxType, UINT32,
    "GUPS memory indexing type : GUPS_IDX_LINEAR or GUPS_IDX_RANDOM (default = GUPS_IDX_RANDOM)");
CLASS_PROP_READWRITE(LwdaLwLink, GupsMemorySize, UINT32,
    "Amount of memory to use in the GUPS test, 0 = auto set (default = 0)");
CLASS_PROP_READWRITE(LwdaLwLink, WarmupSec, UINT32,
                     "Warmup time for transfers (default = 0)");

PROP_CONST(LwdaLwLink, ALL_SMS,              TestRouteLwda::ALL_SMS);
PROP_CONST(LwdaLwLink, TW_4BYTES,            TestModeLwda::TW_4BYTES);
PROP_CONST(LwdaLwLink, TW_8BYTES,            TestModeLwda::TW_8BYTES);
PROP_CONST(LwdaLwLink, TW_16BYTES,           TestModeLwda::TW_16BYTES);
PROP_CONST(LwdaLwLink, TW_128BYTES,          TestModeLwda::TW_128BYTES);
PROP_CONST(LwdaLwLink, ST_GUPS,              TestModeLwda::ST_GUPS);
PROP_CONST(LwdaLwLink, ST_CE2D,              TestModeLwda::ST_CE2D);
PROP_CONST(LwdaLwLink, GUPS_IDX_LINEAR,      LwLinkKernel::LINEAR);
PROP_CONST(LwdaLwLink, GUPS_IDX_RANDOM,      LwLinkKernel::RANDOM);
PROP_CONST(LwdaLwLink, P2P_ALLOC_ALL_LOCAL,  TestModeLwda::P2P_ALLOC_ALL_LOCAL);  //$
PROP_CONST(LwdaLwLink, P2P_ALLOC_ALL_REMOTE, TestModeLwda::P2P_ALLOC_ALL_REMOTE); //$
PROP_CONST(LwdaLwLink, P2P_ALLOC_IN_LOCAL,   TestModeLwda::P2P_ALLOC_IN_LOCAL);   //$
PROP_CONST(LwdaLwLink, P2P_ALLOC_OUT_LOCAL,  TestModeLwda::P2P_ALLOC_OUT_LOCAL);  //$
PROP_CONST(LwdaLwLink, P2P_ALLOC_DEFAULT,    TestModeLwda::P2P_ALLOC_DEFAULT);    //$

void LwdaLwLink::PropertyHelp(JSContext *pCtx, regex *pRegex)
{
    JSObject *pLwrJObj = JS_NewObject(pCtx, &LwdaLwLink_class, 0, 0);
    JS_SetPrivate(pCtx, pLwrJObj, this);
    Help::JsPropertyHelp(pLwrJObj, pCtx, pRegex, 0);
    JS_SetPrivate(pCtx, pLwrJObj, nullptr);
    GpuTest::PropertyHelp(pCtx, pRegex);
}
