/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2019,2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwlbwstress_tegra.h"

#include "gpu/tests/lwlink/lwlbwstress_tegra/lwlbwst_alloc_mgr.h"
#include "gpu/tests/lwlink/lwlbwstress_tegra/lwlbwst_test_route.h"
#include "gpu/tests/lwlink/lwlbwstress_tegra/lwlbwst_test_mode.h"

#include "gpu/include/gpusbdev.h"

#include "cheetah/dev/qualeng/qualengctrl.h"
#include <vector>

using namespace MfgTest;
using namespace LwLinkDataTestHelper;

namespace
{
    const UINT32 MEMORY_SIZE_KB_CHUNK = 16;
}

//------------------------------------------------------------------------------
//! \brief Constructor
//!
MfgTest::LwLinkBwStressTegra::LwLinkBwStressTegra()
 :  m_MemorySizeKb(2048)
   ,m_MemoryPattern(CHECKER_BOARD)
{
    SetName("LwLinkBwStressTegra");
    SetFailOnSysmemBw(true);
}

//------------------------------------------------------------------------------
//! \brief Destructor
//!
MfgTest::LwLinkBwStressTegra::~LwLinkBwStressTegra()
{
}

//------------------------------------------------------------------------------
//! \brief Check if the test is supported.
//!
//! \return true if supported, false otherwise
/* virtual */ bool MfgTest::LwLinkBwStressTegra::IsSupported()
{
    if (!GetBoundGpuSubdevice()->IsSOC())
        return false;

    if (!LwLinkDataTest::IsSupported())
        return false;

    if (!Xp::HasFeature(Xp::HAS_KERNEL_LEVEL_ACCESS))
        return false;

    vector<LwLinkRoutePtr> routes;
    if (OK != GetLwLinkRoutes(GetBoundTestDevice(), &routes))
    {
        Printf(Tee::PriError, "%s failed get LwLink routes.\n", GetName().c_str());
        return false;
    }

    if (routes.empty())
    {
        Printf(Tee::PriWarn, "No LwLink routes found\n");
        return true;
    }

    if ((routes.size() > 1) || (!routes[0]->IsLoop()))
    {
        Printf(Tee::PriError, "%s only supported on single route loopback connections.\n",
               GetName().c_str());
        MASSERT(!"See previous error");
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------------
//! \brief Print the test properties
//!
void MfgTest::LwLinkBwStressTegra::PrintJsProperties(Tee::Priority pri)
{
    Printf(pri, "%s Js Properties:\n", GetName().c_str());
    Printf(pri, "\tMemorySizeKb:            %u\n", m_MemorySizeKb);
    Printf(pri, "\tMemoryPattern:           %u\n", m_MemoryPattern);
    LwLinkDataTest::PrintJsProperties(pri);
}

//------------------------------------------------------------------------------
// Assign the hardware to perform the copies later.
/* virtual */ RC MfgTest::LwLinkBwStressTegra::AssignTestTransferHw(TestModePtr pTm)
{
    // This dynamic cast should always return non-null since test modes used in
    // this test will have been created from CreateTestMode()
    TestModeTegra *pLwlbwsTm = dynamic_cast<TestModeTegra *>(pTm.get());
    MASSERT(pLwlbwsTm);

    return pLwlbwsTm->AssignQualEng();
}

//------------------------------------------------------------------------------
/* virtual */ AllocMgrPtr MfgTest::LwLinkBwStressTegra::CreateAllocMgr()
{
    return make_shared<AllocMgrTegra>(GetTestConfiguration(), GetVerbosePrintPri(), true);
}

//------------------------------------------------------------------------------
/* virtual */ TestRoutePtr MfgTest::LwLinkBwStressTegra::CreateTestRoute
(
    TestDevicePtr  pLwLinkDev,
    LwLinkRoutePtr pRoute
)
{
    return make_shared<TestRouteTegra>(pLwLinkDev,
                                       pRoute,
                                       GetVerbosePrintPri());
}

//------------------------------------------------------------------------------
/* virtual */ TestModePtr MfgTest::LwLinkBwStressTegra::CreateTestMode()
{
    return make_shared<TestModeTegra>(GetTestConfiguration(),
                                      GetAllocMgr(),
                                      GetNumErrorsToPrint(),
                                      GetCopyLoopCount(),
                                      GetBwThresholdPct(),
                                      GetShowBandwidthData(),
                                      GetPauseTestMask(),
                                      GetVerbosePrintPri());
}

//------------------------------------------------------------------------------
/* virtual */  RC MfgTest::LwLinkBwStressTegra::InitializeAllocMgr()
{
    return OK;
}

//------------------------------------------------------------------------------
/* virtual */ void MfgTest::LwLinkBwStressTegra::ConfigureTestMode
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
    TestModeTegra *pLwlbwsTm = dynamic_cast<TestModeTegra *>(pTm.get());
    MASSERT(pLwlbwsTm);

    pLwlbwsTm->SetMemorySize(m_MemorySizeKb * 1024);
    pLwlbwsTm->SetMemoryPattern(static_cast<QUAL_ENG_PATTERN>(m_MemoryPattern));
    pLwlbwsTm->SetFailOnSysmemBw(true);
}

//------------------------------------------------------------------------------
/* virtual */ bool MfgTest::LwLinkBwStressTegra::IsRouteValid(LwLinkRoutePtr pRoute)
{
    // Only sysmem loopback routes are supported
    if (!pRoute->IsLoop() || !pRoute->IsSysmem())
    {
        Printf(GetVerbosePrintPri(),
               "%s : Skipping route that is not a sysmem loopback route:\n%s",
               GetName().c_str(),
               pRoute->GetString(pRoute->GetEndpointDevices().first,
                                 LwLink::DT_REQUEST,
                                 "    ").c_str());
        return false;
    }
    return MfgTest::LwLinkDataTest::IsRouteValid(pRoute);
}

//----------------------------------------------------------------------------
// Check for validity of the test mode
// Returns true if the test mode is valid and can be tested, false otherwise
/* virtual */ bool MfgTest::LwLinkBwStressTegra::IsTestModeValid(TestModePtr pTm)
{
    if (!MfgTest::LwLinkDataTest::IsTestModeValid(pTm))
        return false;

    // Only loopback test modes are supported
    if (!pTm->OnlyLoopback())
        return false;

    return true;
}

//------------------------------------------------------------------------------
/* virtual */ RC MfgTest::LwLinkBwStressTegra::ValidateTestSetup()
{
    RC rc;

    CHECK_RC(LwLinkDataTest::ValidateTestSetup());

    if (GetCopyLoopCount() > 1)
    {
        Printf(Tee::PriError, "%s does not support CopyLoopCount, use MemorySizeKb instead!\n",
               GetName().c_str());
        return RC::BAD_PARAMETER;
    }

    if (GetSysmemLoopback())
    {
        Printf(Tee::PriError, "%s does not support SysmemLoopback!\n",
               GetName().c_str());
        return RC::BAD_PARAMETER;
    }

    if (!GetFailOnSysmemBw())
    {
        Printf(Tee::PriError, "%s does not support FailOnSysmemBw!\n",
               GetName().c_str());
        return RC::BAD_PARAMETER;
    }

    if (m_MemorySizeKb % MEMORY_SIZE_KB_CHUNK)
    {
        Printf(Tee::PriError, "%s MemorySizeKb must be a multiple of 16, rounding up!\n",
               GetName().c_str());
        m_MemorySizeKb += MEMORY_SIZE_KB_CHUNK - (m_MemorySizeKb % MEMORY_SIZE_KB_CHUNK);
    }
    return rc;
}

//-----------------------------------------------------------------------------
// Exposed JS properties
JS_CLASS_INHERIT(LwLinkBwStressTegra, LwLinkDataTest, "CheetAh LwLink bandwidth stress test");

CLASS_PROP_READWRITE(LwLinkBwStressTegra, MemorySizeKb, UINT32,
    "Memory size to transfer in kbytes - must be a multiple of 16 (default = 2048)");
CLASS_PROP_READWRITE(LwLinkBwStressTegra, MemoryPattern, UINT32,
    "Memory pattern to transfer from the qual engine (see wiki page for details)");

PROP_CONST(LwLinkBwStressTegra, NO_PATTERN,      NO_PATTERN);
PROP_CONST(LwLinkBwStressTegra, TEST_PATTERN,    TEST_PATTERN);
PROP_CONST(LwLinkBwStressTegra, LONG_MATS,       LONG_MATS);
PROP_CONST(LwLinkBwStressTegra, WALKING_ONES,    WALKING_ONES);
PROP_CONST(LwLinkBwStressTegra, WALKING_ZEROS,   WALKING_ZEROS);
PROP_CONST(LwLinkBwStressTegra, SUPER_ZERO_ONE,  SUPER_ZERO_ONE);
PROP_CONST(LwLinkBwStressTegra, CHECKER_BOARD,   CHECKER_BOARD);
PROP_CONST(LwLinkBwStressTegra, SOLID_ZERO,      SOLID_ZERO);
PROP_CONST(LwLinkBwStressTegra, SOLID_ONE,       SOLID_ONE);
PROP_CONST(LwLinkBwStressTegra, WALK_SWIZZLE,    WALK_SWIZZLE);
PROP_CONST(LwLinkBwStressTegra, DOUBLE_ZERO_ONE, DOUBLE_ZERO_ONE);
PROP_CONST(LwLinkBwStressTegra, TRIPLE_SUPER,    TRIPLE_SUPER);
PROP_CONST(LwLinkBwStressTegra, QUAD_ZERO_ONE,   QUAD_ZERO_ONE);
PROP_CONST(LwLinkBwStressTegra, TRIPLE_WHAMMY,   TRIPLE_WHAMMY);
PROP_CONST(LwLinkBwStressTegra, ISOLATED_ONES,   ISOLATED_ONES);
PROP_CONST(LwLinkBwStressTegra, ISOLATED_ZEROS,  ISOLATED_ZEROS);
PROP_CONST(LwLinkBwStressTegra, SLOW_FALLING,    SLOW_FALLING);
PROP_CONST(LwLinkBwStressTegra, SLOW_RISING,     SLOW_RISING);
PROP_CONST(LwLinkBwStressTegra, MAX_CROSSTALK,   MAX_CROSSTALK);
