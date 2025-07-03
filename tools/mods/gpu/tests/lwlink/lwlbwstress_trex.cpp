/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwlbwstress_trex.h"

#include "device/interface/lwlink/lwltrex.h"
#include "gpu/include/testdevice.h"
#include "gpu/tests/lwlink/lwlbwstress_trex/lwlbws_trex_alloc_mgr.h"
#include "gpu/tests/lwlink/lwlbwstress_trex/lwlbws_trex_test_mode.h"
#include "gpu/tests/lwlink/lwlbwstress_trex/lwlbws_trex_test_route.h"

using namespace MfgTest;
using namespace LwLinkDataTestHelper;

//------------------------------------------------------------------------------
//! \brief Constructor
//!
MfgTest::LwLinkBwStressTrex::LwLinkBwStressTrex()
: LwLinkDataTest()
{
    SetName("LwLinkBwStressTrex");
}

//------------------------------------------------------------------------------
//! \brief Destructor
//!
MfgTest::LwLinkBwStressTrex::~LwLinkBwStressTrex()
{
}

//------------------------------------------------------------------------------
//! \brief Check if the test is supported.
//!
//! \return true if supported, false otherwise
/* virtual */ bool MfgTest::LwLinkBwStressTrex::IsSupported()
{
    if (!LwLinkDataTest::IsSupported())
        return false;

    TestDevicePtr pTestDevice = GetBoundTestDevice();

    if (pTestDevice->GetType() != TestDevice::TYPE_LWIDIA_LWSWITCH)
        return false;

    if (!pTestDevice->SupportsInterface<LwLinkTrex>())
        return false;

    bool foundTrex = false;
    for (UINT32 linkId = 0; linkId < pTestDevice->GetInterface<LwLink>()->GetMaxLinks(); linkId++)
    {
        if (!pTestDevice->GetInterface<LwLink>()->IsLinkActive(linkId))
            continue;

        LwLink::EndpointInfo remEndpoint;
        pTestDevice->GetInterface<LwLink>()->GetRemoteEndpoint(linkId, &remEndpoint);
        if (remEndpoint.devType == TestDevice::TYPE_TREX)
        {
            foundTrex = true;
            break;
        }
    }
    if (!foundTrex)
        return false;

    return true;
}

//------------------------------------------------------------------------------
/* virtual */ RC MfgTest::LwLinkBwStressTrex::Setup()
{
    RC rc;

    CHECK_RC(LwLinkDataTest::Setup());

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Print the test properties
//!
void MfgTest::LwLinkBwStressTrex::PrintJsProperties(Tee::Priority pri)
{
    Printf(pri, "%s Js Properties:\n", GetName().c_str());
    Printf(pri, "\tDataLength:                %u\n", m_DataLength);
    Printf(pri, "\tNumPackets:                %u\n", m_NumPackets);

    LwLinkDataTest::PrintJsProperties(pri);
}

//------------------------------------------------------------------------------
// Assign the hardware to perform the copies later.  Note this doesn't actually
// perform any allocation but rather determines what HW is necessary to accomplish
// all the data transfers specified by the test mode
/* virtual */ RC MfgTest::LwLinkBwStressTrex::AssignTestTransferHw
(
    TestModePtr pTm
)
{
    RC rc;

    // This dynamic cast should always return non-null since test modes used in
    // this test will have been created from CreateTestMode()
    TestModeTrex *pLwlbwsTm = dynamic_cast<TestModeTrex *>(pTm.get());
    MASSERT(pLwlbwsTm);

    CHECK_RC(pLwlbwsTm->AssignTrex());

    return RC::OK;
}

//------------------------------------------------------------------------------
/* virtual */ AllocMgrPtr MfgTest::LwLinkBwStressTrex::CreateAllocMgr()
{
    return AllocMgrPtr(new AllocMgrTrex(GetTestConfiguration(),
                                        GetVerbosePrintPri(),
                                        true));
}

//------------------------------------------------------------------------------
/* virtual */ TestRoutePtr MfgTest::LwLinkBwStressTrex::CreateTestRoute
(
    TestDevicePtr  pLwLinkDev,
    LwLinkRoutePtr pRoute
)
{
    return TestRoutePtr(new TestRouteTrex(pLwLinkDev,
                                          pRoute,
                                          GetVerbosePrintPri()));
}

//------------------------------------------------------------------------------
/* virtual */ TestModePtr MfgTest::LwLinkBwStressTrex::CreateTestMode()
{
    return TestModePtr(new TestModeTrex(GetTestConfiguration(),
                                        GetAllocMgr(),
                                        GetNumErrorsToPrint(),
                                        GetCopyLoopCount(),
                                        GetBwThresholdPct(),
                                        GetShowBandwidthData(),
                                        GetPauseTestMask(),
                                        GetVerbosePrintPri()));
}

//------------------------------------------------------------------------------
/* virtual */  RC MfgTest::LwLinkBwStressTrex::InitializeAllocMgr()
{
    return RC::OK;
}

//------------------------------------------------------------------------------
/* virtual */ void MfgTest::LwLinkBwStressTrex::ConfigureTestMode
(
    TestModePtr pTm,
    TransferDir td,
    TransferDir sysmemTd,
    bool        bLoopbackInput
)
{
    LwLinkDataTest::ConfigureTestMode(pTm, td, sysmemTd, bLoopbackInput);

    // This dynamic cast should always return non-null since test modes used in
    // this test will have been created from CreateTestMode()
    TestModeTrex *pLwlbwsTm = dynamic_cast<TestModeTrex *>(pTm.get());
    MASSERT(pLwlbwsTm);

    pLwlbwsTm->SetDataLength(m_DataLength);
    pLwlbwsTm->SetNumPackets(m_NumPackets);
}

//------------------------------------------------------------------------------
/* virtual */ bool MfgTest::LwLinkBwStressTrex::IsRouteValid(LwLinkRoutePtr pRoute)
{
    return LwLinkDataTest::IsRouteValid(pRoute);
}

//------------------------------------------------------------------------------
/* virtual */ RC MfgTest::LwLinkBwStressTrex::ValidateTestSetup()
{
    RC rc;

    CHECK_RC(LwLinkDataTest::ValidateTestSetup());

    static set<UINT32> validDataLen = {1, 2, 4, 8, 16, 32, 64, 96, 128, 256};
    if (validDataLen.find(m_DataLength) == validDataLen.end())
    {
        Printf(Tee::PriError,
               "%s : Invalid DataLength parameter!\n",
               GetName().c_str());
        return RC::BAD_PARAMETER;
    }

    return rc;
}

//-----------------------------------------------------------------------------
// Exposed JS properties
JS_CLASS_INHERIT(LwLinkBwStressTrex, LwLinkDataTest, "LwLink bandwidth stress test TREX");
CLASS_PROP_READWRITE(LwLinkBwStressTrex, DataLength, UINT32, "Data length of TREX packets");
CLASS_PROP_READWRITE(LwLinkBwStressTrex, NumPackets, UINT32, "Number of TREX packets to create");
