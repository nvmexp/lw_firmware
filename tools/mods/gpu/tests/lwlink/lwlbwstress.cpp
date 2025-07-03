/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwlbwstress.h"

#include "core/include/mgrmgr.h"
#include "core/include/platform.h"
#include "core/include/registry.h"
#include "core/include/utility.h"
#include "ctrl/ctrl0000.h"
#include "device/interface/lwlink/lwlfla.h"

#include "gpu/include/gpudev.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/gralloc.h"
#include "gpu/include/testdevice.h"

#include "gpu/lwlink/lwlinkimpl.h"

#include "gpu/tests/gputestc.h"
#include "gpu/tests/lwlink/lwlbwstress/lwlbws_alloc_mgr.h"
#include "gpu/tests/lwlink/lwlbwstress/lwlbws_test_mode.h"
#include "gpu/tests/lwlink/lwlbwstress/lwlbws_test_route.h"

#include "gpu/utility/lwsurf.h"
#include "gpu/utility/surf2d.h"

#include "lwRmReg.h"

#include <array>
#include <vector>

#include <boost/range/algorithm.hpp>
using namespace MfgTest;
using namespace LwLinkDataTestHelper;

//------------------------------------------------------------------------------
//! \brief Constructor
//!
MfgTest::LwLinkBwStress::LwLinkBwStress()
{
    SetName("LwLinkBwStress");
}

//------------------------------------------------------------------------------
//! \brief Destructor
//!
MfgTest::LwLinkBwStress::~LwLinkBwStress()
{
}

//------------------------------------------------------------------------------
RC MfgTest::LwLinkBwStress::InitFromJs()
{
    RC rc;
    CHECK_RC(GpuTest::InitFromJs());

    if (m_Use2D)
        m_Engine = LwLink::HT_TWOD;

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Check if the test is supported.
//!
//! \return true if supported, false otherwise
/* virtual */ bool MfgTest::LwLinkBwStress::IsSupported()
{
    if (!LwLinkDataTest::IsSupported())
        return false;

    // LwLinkBwStressTegra should be used instead
    if (GetBoundTestDevice()->IsSOC())
        return false;

    if (m_TestFullFabric)
    {
        vector<LwLink::FabricAperture> fabricApertures;
        GetBoundTestDevice()->GetInterface<LwLink>()->GetFabricApertures(&fabricApertures);
        if (fabricApertures.size() < FULL_FABRIC_RANGE)
            return false;
    }

    return true;
}

//------------------------------------------------------------------------------
/* virtual */ RC MfgTest::LwLinkBwStress::Setup()
{
    RC rc;
    CHECK_RC(GetPropP2PSupported(&m_bPropP2PSupported));

    // OneToOne mode must be setup before setting up LwLinkDataTest::Setup as it
    // will be used during that call
    UINT32 oneToOne = 0;
    if (OK == Registry::Read("ResourceManager", LW_REG_STR_RM_CE_ONE_TO_ONE_MAP, &oneToOne))
    {
        m_OnePcePerLce = (oneToOne == LW_REG_STR_RM_CE_ONE_TO_ONE_MAP_TRUE);
    }

    auto pLwLinkFla = GetBoundTestDevice()->GetInterface<LwLinkFla>();
    if (m_FlaSetFromJs && m_UseFla &&
        (!pLwLinkFla || !pLwLinkFla->GetFlaEnabled()))
    {
        Printf(Tee::PriError, "%s : FLA explicitly requested but is not supported\n",
               GetName().c_str());
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }
    else if (!m_FlaSetFromJs)
    {
        m_UseFla = pLwLinkFla && pLwLinkFla->GetFlaEnabled();
    }

    CHECK_RC(LwLinkDataTest::Setup());

    // The routes checks were moved from LwLinkBwStress::IsSupported to avoid a
    // test escape when we don't have supported test routes, but they should be.
    // See bug 2188798.

    vector<LwLinkRoutePtr> routes;
    CHECK_RC(GetLwLinkRoutes(GetBoundTestDevice(), &routes));

    bool bAtLeastOneSysmemRoute = false;
    for (size_t ii = 0; ii < routes.size(); ii++)
    {
        if (routes[ii]->IsSysmem())
        {
            bAtLeastOneSysmemRoute = true;
        }
    }

    if (m_Engine != LwLink::HT_CE && !bAtLeastOneSysmemRoute)
    {
        bool bPropP2PSupported = false;
        RC rc = GetPropP2PSupported(&bPropP2PSupported);
        if (OK != rc)
        {
            Printf
            (
                Tee::PriError,
                "%s failed get PROP P2P support capability.\n",
                GetName().c_str()
            );
            return RC::HW_ERROR;
        }

        // If there are no system memory routes and PROP P2P traffic is not
        // supported then there are no routes that can be tested with
        // the 2D engine so fail the test
        if (!bPropP2PSupported)
            return RC::HW_ERROR;
    }

    if (m_CeTransferWidth != 0)
    {
        LwLink::CeWidth newCeWidth = m_CeTransferWidth == 128 ?
            LwLink::CEW_128_BYTE : LwLink::CEW_256_BYTE;
        auto testedDevices = GetTestedDevs();
        for (auto pLwrDev : testedDevices)
        {
            auto pLwLink = pLwrDev->GetInterface<LwLink>();
            LwLink::CeWidth p2pCeWidth = pLwLink->GetCeCopyWidth(LwLink::CET_PEERMEM);
            LwLink::CeWidth sysCeWidth = pLwLink->GetCeCopyWidth(LwLink::CET_SYSMEM);
            if ((p2pCeWidth != newCeWidth) || (sysCeWidth != newCeWidth))
            {
                DEFER()
                {
                    pLwLink->SetCeCopyWidth(LwLink::CET_PEERMEM, p2pCeWidth);
                    pLwLink->SetCeCopyWidth(LwLink::CET_SYSMEM, sysCeWidth);
                };

                pLwLink->SetCeCopyWidth(LwLink::CET_PEERMEM, newCeWidth);
                pLwLink->SetCeCopyWidth(LwLink::CET_SYSMEM, newCeWidth);

                if ((pLwLink->GetCeCopyWidth(LwLink::CET_PEERMEM) != newCeWidth) ||
                    (pLwLink->GetCeCopyWidth(LwLink::CET_SYSMEM) != newCeWidth))
                {
                    Printf(Tee::PriError,
                           "%s - Could not change copy engine transfer width to %u\n",
                           pLwrDev->GetName().c_str(), m_CeTransferWidth);
                    return RC::UNSUPPORTED_HARDWARE_FEATURE;
                }
            }
        }
    }

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Print the test properties
//!
void MfgTest::LwLinkBwStress::PrintJsProperties(Tee::Priority pri)
{
    Printf(pri, "%s Js Properties:\n", GetName().c_str());
    Printf(pri, "\tSurfSizeFactor:            %u\n", m_SurfSizeFactor);
    Printf(pri, "\tSrcSurfMode:               %u\n", m_SrcSurfMode);
    Printf(pri, "\tSrcSurfModePercent:        %u\n", m_SrcSurfModePercent);
    Printf(pri, "\tUse2D:                     %s\n", m_Use2D ? "true" : "false");
    Printf(pri, "\t[Engine used]:             %s\n", m_Engine == LwLink::HT_CE ? "CE" : m_Engine == LwLink::HT_TWOD ? "2D" : "VIC");
    Printf(pri, "\tCopyByLine:                %s\n", m_CopyByLine ? "true" : "false");
    Printf(pri, "\tSrcSurfLayout:             %s\n",
           (m_SrcSurfLayout == Surface2D::Pitch) ? "Pitch" : "BlockLinear");
    Printf(pri, "\tCeTransferWidth:           %u\n", m_CeTransferWidth);
    Printf(pri, "\tPixelComponentMask:        0x%02x\n", m_PixelComponentMask);
    Printf(pri, "\tP2PCeAllocMode:            %u\n", m_P2PCeAllocMode);
    Printf(pri, "\tTestFullFabric:            %s\n", m_TestFullFabric ? "true" : "false");
    Printf(pri, "\tUseLwda:                   %s\n", m_UseLwda ? "true" : "false");
    Printf(pri, "\tUseDmaInit:                %s\n", m_UseDmaInit ? "true" : "false");
    Printf(pri, "\tUseLwdaCrc:                %s\n", m_UseLwdaCrc ? "true" : "false");
    Printf(pri, "\tFbSurfPageSizeKb:          %u\n", m_FbSurfPageSizeKb);
    Printf(pri, "\tUseFla:                    %s\n", m_UseFla ? "true" : "false");
    Printf(pri, "\tFlaPageSizeKb:             %u\n", m_FlaPageSizeKb);

    LwLinkDataTest::PrintJsProperties(pri);
}

//------------------------------------------------------------------------------
bool MfgTest::LwLinkBwStress::GetUseFla() const
{
    return m_UseFla;
}

//------------------------------------------------------------------------------
RC MfgTest::LwLinkBwStress::SetUseFla(bool bUseFla)
{
    m_UseFla = bUseFla;
    m_FlaSetFromJs = true;
    return RC::OK;
}

//------------------------------------------------------------------------------
// Assign the hardware to perform the copies later.  Note this doesn't actually
// perform any allocation but rather determines what HW is necessary to accomplish
// all the data transfers specified by the test mode
/* virtual */ RC MfgTest::LwLinkBwStress::AssignTestTransferHw
(
    TestModePtr pTm
)
{
    // This dynamic cast should always return non-null since test modes used in
    // this test will have been created from CreateTestMode()
    TestModeCe2d *pLwlbwsTm = dynamic_cast<TestModeCe2d *>(pTm.get());
    MASSERT(pLwlbwsTm);

    RC rc;
    switch (m_Engine)
    {
        case LwLink::HT_QUAL_ENG:
            Printf(Tee::PriError,
                   "Qual engine not supported in LwLinkBwStress, use LwLinkBwStressTegra!");
            return RC::BAD_PARAMETER;

        case LwLink::HT_TWOD:
            CHECK_RC(pLwlbwsTm->AssignTwod());
            break;

        default:
            MASSERT(m_Engine == LwLink::HT_CE);
            CHECK_RC(AssignCopyEngines(pLwlbwsTm));
            break;
    }
    return rc;
}

//------------------------------------------------------------------------------
/* virtual */ AllocMgrPtr MfgTest::LwLinkBwStress::CreateAllocMgr()
{
    UINT32 numSemaphores = 0;
    if (OK != GetMaxRequiredSemaphores(&numSemaphores))
        return AllocMgrPtr();
    return AllocMgrPtr(new AllocMgrCe2d(GetTestConfiguration(),
                                        numSemaphores,
                                        GetVerbosePrintPri(),
                                        true));
}

//------------------------------------------------------------------------------
/* virtual */ TestRoutePtr MfgTest::LwLinkBwStress::CreateTestRoute
(
    TestDevicePtr  pLwLinkDev,
    LwLinkRoutePtr pRoute
)
{
    return TestRoutePtr(new TestRouteCe2d(pLwLinkDev,
                                          pRoute,
                                          GetVerbosePrintPri()));
}

//------------------------------------------------------------------------------
/* virtual */ TestModePtr MfgTest::LwLinkBwStress::CreateTestMode()
{
    return TestModePtr(new TestModeCe2d(GetTestConfiguration(),
                                        GetAllocMgr(),
                                        GetNumErrorsToPrint(),
                                        GetCopyLoopCount(),
                                        GetBwThresholdPct(),
                                        GetShowBandwidthData(),
                                        GetPauseTestMask(),
                                        GetVerbosePrintPri()));
}

//------------------------------------------------------------------------------
/* virtual */  RC MfgTest::LwLinkBwStress::InitializeAllocMgr()
{
    AllocMgrCe2d *pLwlbwsAm = dynamic_cast<AllocMgrCe2d *>(GetAllocMgr().get());
    MASSERT(pLwlbwsAm);
    GpuTestConfiguration *pTestConfig = GetTestConfiguration();
    const UINT32 surfaceWidth  = pTestConfig->SurfaceWidth();
    const UINT32 surfaceHeight = pTestConfig->SurfaceHeight() * m_SurfSizeFactor;

    pLwlbwsAm->SetFbSurfPageSizeKb(m_FbSurfPageSizeKb);
    pLwlbwsAm->SetUseFla(m_UseFla);
    pLwlbwsAm->SetUseLwda(m_UseLwda);
    pLwlbwsAm->SetUseDmaInit(m_UseDmaInit);
    pLwlbwsAm->SetFlaPageSizeKb(m_FlaPageSizeKb);
    return pLwlbwsAm->Initialize(surfaceWidth,
                                 surfaceHeight,
                                 m_SrcSurfMode);
}

//------------------------------------------------------------------------------
/* virtual */ void MfgTest::LwLinkBwStress::ConfigureTestMode
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
    TestModeCe2d *pLwlbwsTm = dynamic_cast<TestModeCe2d *>(pTm.get());
    MASSERT(pLwlbwsTm);

    pLwlbwsTm->SetCeCopyWidth(m_CeTransferWidth);
    pLwlbwsTm->SetPixelComponentMask(m_PixelComponentMask);

    TestModeCe2d::P2PAllocMode p2pMode =
        static_cast<TestModeCe2d::P2PAllocMode>(m_P2PCeAllocMode);
    pLwlbwsTm->SetP2PAllocMode(p2pMode);
    pLwlbwsTm->SetSurfaceSizeFactor(m_SurfSizeFactor);
    pLwlbwsTm->SetSrcSurfModePercent(m_SrcSurfModePercent);

    pLwlbwsTm->SetTestFullFabric(m_TestFullFabric);
    pLwlbwsTm->SetUseLwdaCrc(m_UseLwdaCrc); 
}

//------------------------------------------------------------------------------
/* virtual */ bool MfgTest::LwLinkBwStress::IsRouteValid(LwLinkRoutePtr pRoute)
{
    // If PROP P2P is not supported and using the 2D engine, then any non
    // system memory routes need to be skipped as unsupported
    if (m_Engine != LwLink::HT_CE && !pRoute->IsSysmem() && !m_bPropP2PSupported)
        return false;
    return MfgTest::LwLinkDataTest::IsRouteValid(pRoute);
}

//----------------------------------------------------------------------------
// Check for validity of the test mode
// Returns true if the test mode is valid and can be tested, false otherwise
/* virtual */ bool MfgTest::LwLinkBwStress::IsTestModeValid(TestModePtr pTm)
{
    if (!MfgTest::LwLinkDataTest::IsTestModeValid(pTm))
        return false;

    // Skip non-loopback bi-directional test modes when using the 2D
    // for transfers
    if (m_Engine != LwLink::HT_CE && !pTm->OnlyLoopback() && (pTm->GetTransferDir() == TD_IN_OUT))
    {
        return false;
    }

    return true;
}

//------------------------------------------------------------------------------
/* virtual */ RC MfgTest::LwLinkBwStress::ValidateTestSetup()
{
    RC rc;

    CHECK_RC(LwLinkDataTest::ValidateTestSetup());

    GpuTestConfiguration *pTestConfig = GetTestConfiguration();

    // Optimal CE performance requires a specific surface byte width
    const UINT32 surfWidthBytes = pTestConfig->SurfaceWidth() *
                                  ColorUtils::PixelBytes(ColorUtils::A8R8G8B8);

    if ((m_CeTransferWidth != 0) && (m_CeTransferWidth != 256) && (m_CeTransferWidth != 128))
    {
        Printf(Tee::PriError,
               "%s : CeTransferWidth must be either 0, 128 or 256!!\n",
               GetName().c_str());
        return RC::BAD_PARAMETER;
    }

    if (m_Engine != LwLink::HT_CE &&
        ((pTestConfig->SurfaceHeight() * m_SurfSizeFactor / 2) > TwodAlloc::MAX_COPY_HEIGHT))
    {
        Printf(Tee::PriError,
               "%s : Maximum 2D transfer height exceeded.  SurfaceHeight * SurfSizeFactor"
               "must be less than %u!!\n",
               GetName().c_str(),
               TwodAlloc::MAX_COPY_HEIGHT * 2);
        return RC::BAD_PARAMETER;
    }

    if (m_PixelComponentMask != TestMode::ALL_COMPONENTS)
    {
        bool badArg = false;
        if (m_Engine != LwLink::HT_CE)
        {
            Printf(Tee::PriError,
                   "%s : PixelComponentMask must select all components when using 2D!!\n",
                   GetName().c_str());
            badArg = true;
        }
        if ((m_PixelComponentMask == 0) || (m_PixelComponentMask > 0xf))
        {
            Printf(Tee::PriError,
                   "%s : PixelComponentMask must be between 0x1 and 0xf!!\n",
                   GetName().c_str());
            badArg = true;
        }
        if (badArg)
            return RC::BAD_PARAMETER;
    }

    if ((m_SrcSurfLayout != Surface2D::Pitch) &&
        (m_SrcSurfLayout != Surface2D::BlockLinear))
    {
        Printf(Tee::PriNormal, "%s : Unsupported source surface layout %u!!\n",
               GetName().c_str(), m_SrcSurfLayout);
        return RC::BAD_PARAMETER;
    }

    if (m_SrcSurfModePercent > 100)
    {
        Printf(Tee::PriError, "%s : SrcSurfModePercent must be an integer 0-100\n",
               GetName().c_str());
        return RC::BAD_PARAMETER;
    }

    if (surfWidthBytes % 256)
    {
        if ((m_SrcSurfLayout == Surface2D::BlockLinear) || (m_Engine != LwLink::HT_CE && m_Engine != LwLink::HT_TREX))
        {
            Printf(Tee::PriError,
                   "%s : SurfaceWidth must be a multiple of 256 bytes with "
                   "block linear or 2D transfers!!\n",
                   GetName().c_str());
            return RC::BAD_PARAMETER;
        }

        Printf(Tee::PriWarn,
               "%s : For peak CE bandwidth, SurfaceWidth must be a multiple of "
               "256 bytes!!\n",
               GetName().c_str());
    }

    // Ensure that variables are set correctly when using 2D engine
    if (m_Engine != LwLink::HT_CE &&
        ((GetMaxSimulRoutes() != 1) || (GetMinSimulRoutes() != 1)))
    {
        Printf(Tee::PriError,
               "%s : Only a single route may be tested at a time with 2D!!\n",
               GetName().c_str());
        return RC::BAD_PARAMETER;
    }

    bool perf0Max = true;
    if (m_Engine == LwLink::HT_CE)
    {
        // Set ce limit adustment if not running at 0.max
        Perf* pPerf = GetBoundGpuSubdevice()->GetPerf();
        if (pPerf->HasPStates())
        {
            Perf::PerfPoint perfPt(0, Perf::GpcPerf_MAX);
            if (OK != pPerf->GetLwrrentPerfPoint(&perfPt))
            {
                Printf(Tee::PriLow,
                       "%s : Getting current perf point failed, assuming sufficient "
                       "clock speeds\n",
                       GetName().c_str());
            }

            if ((perfPt.PStateNum != 0) || (perfPt.SpecialPt != Perf::GpcPerf_MAX))
            {
                perf0Max = false;
            }
        }
    }

    if (!perf0Max && !GetSkipBandwidthCheck())
    {
        Printf(Tee::PriWarn, "%s : Bandwidth check may fail when not in 0.max!\n",
               GetName().c_str());
    }

    if (m_TestFullFabric &&
        (pTestConfig->SurfaceWidth() != 0x1000 || //4K
         pTestConfig->SurfaceHeight() != FULL_FABRIC_RANGE ||
         m_SurfSizeFactor != 1 ||
         (m_FbSurfPageSizeKb != 0 && m_FbSurfPageSizeKb != 4)))
    {
        Printf(Tee::PriError, "%s : FullFabric must use surfaces that are"
                              "4096 x 8192 and SurfSizeFactor = 1 with 4kb pages\n",
                              GetName().c_str());
        return RC::BAD_PARAMETER;
    }

    if (m_UseLwdaCrc && !m_UseLwda)
    {
        Printf(Tee::PriWarn, "%s : Overriding UseLwdaCrc=true since UseLwda=false\n",
               GetName().c_str());
        m_UseLwdaCrc = false;
    }
    if (m_UseLwdaCrc)
    {
        LwSurfRoutines lwSurf;
        if (!lwSurf.IsSupported())
        {
            Printf(Tee::PriWarn,
                   "%s : LWCA CRC not supported falling back to CPU checking!\n",
                   GetName().c_str());
            m_UseLwdaCrc = false;
        }
    }
    
    if (!m_UseDmaInit && Platform::GetSimulationMode() == Platform::Hardware)
    {
        Printf(Tee::PriError, "%s : UseDmaInit=false is not supported on silicon\n",
                              GetName().c_str());
        return RC::BAD_PARAMETER;
    }

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Assign the copy engines to a test mode
//!
//! \param pTm : Pointer to test mode to assign copy engines on
//!
//! \return true if copy engines were successfully assigned, false otherwise
RC MfgTest::LwLinkBwStress::AssignCopyEngines(TestModeCe2d * pTm)
{
    RC rc;
    if (m_OnePcePerLce)
        rc = pTm->AssignCesOnePcePerLce();
    else
        rc = pTm->AssignCopyEngines();

    if (rc != OK)
    {
        Printf(Tee::PriNormal,
               "%s : Unable to assign copy engines, skipping test mode!\n",
               GetName().c_str());
        pTm->Print(Tee::PriNormal, "   ");
    }

    return rc;
}

//------------------------------------------------------------------------------
/* virtual */ RC MfgTest::LwLinkBwStress::GetMaxRequiredSemaphores(UINT32 *pReqSemaphores)
{
    RC rc;
    DmaCopyAlloc ceAlloc;
    const UINT32 *pClassList;
    UINT32  numClasses;
    ceAlloc.GetClassList(&pClassList, &numClasses);
    vector<UINT32> engines;
    vector<UINT32> classIds(pClassList, pClassList + numClasses);
    CHECK_RC(GetBoundGpuDevice()->GetPhysicalEnginesFilteredByClass(classIds, &engines));

    // Each link has one start/end semaphore per transfer direction
    static const UINT32 SEM_PER_LINK = 4;
    *pReqSemaphores = GetBoundTestDevice()->GetInterface<LwLink>()->GetMaxLinks() * SEM_PER_LINK;

    // Normally there would only be one CE assigned per-link and scaling the
    // number of semaphores by engine size would not be necessary, however low
    // clock speeds or command line options could assign more than one CE per
    // link so to ensure enough semaphores are available, scale the number of
    // semaphores by the number of copy engines
    *pReqSemaphores *= engines.size();

    if (GetTestAllGpus() || GetUseDataFlowObj())
    {
        GpuDevMgr * pMgr = (GpuDevMgr*) DevMgrMgr::d_GraphDevMgr;
        *pReqSemaphores *= pMgr->NumGpus();
    }

    // Add one semaphore which is used as a global semaphore to kick off all data
    // transfers simultaneously
    *pReqSemaphores += 1;
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Get whether PROP transfers are supported on P2P route
//!
//! \param pbSupported : Pointer to returned support flag
//!
//! \return OK if successful, not OK otherwise
RC MfgTest::LwLinkBwStress::GetPropP2PSupported(bool *pbSupported)
{
    MASSERT(pbSupported);
    RC rc;

    if (GetBoundTestDevice()->GetType() != TestDevice::TYPE_LWIDIA_GPU)
    {
        *pbSupported = false;
        return RC::OK;
    }

    vector<LwLinkRoutePtr> routes;
    CHECK_RC(GetLwLinkRoutes(GetBoundTestDevice(), &routes));

    LW0000_CTRL_SYSTEM_GET_P2P_CAPS_PARAMS  p2pCapsParams = { };
    p2pCapsParams.gpuIds[0] = GetBoundGpuSubdevice()->GetGpuId();
    p2pCapsParams.gpuCount++;
    for (size_t ii = 0; ii < routes.size(); ii++)
    {
        if (routes[ii]->IsP2P())
        {
            TestDevicePtr pRemLwLinkDev = routes[ii]->GetRemoteEndpoint(GetBoundTestDevice());
            GpuSubdevice *pRemDev = pRemLwLinkDev->GetInterface<GpuSubdevice>();

            if (pRemDev->GetParentDevice()->GetNumSubdevices() == 1)
            {
                p2pCapsParams.gpuIds[p2pCapsParams.gpuCount] = pRemDev->GetGpuId();
                p2pCapsParams.gpuCount++;
            }
        }
    }
    CHECK_RC(LwRmPtr()->Control(LwRmPtr()->GetClientHandle(),
                                LW0000_CTRL_CMD_SYSTEM_GET_P2P_CAPS,
                                &p2pCapsParams,
                                sizeof(p2pCapsParams)));

    *pbSupported = FLD_TEST_DRF(0000_CTRL,
                                _SYSTEM_GET_P2P_CAPS,
                                _PROP_SUPPORTED,
                                _TRUE,
                                p2pCapsParams.p2pCaps);

    return rc;
}

//-----------------------------------------------------------------------------
// Exposed JS properties
JS_CLASS_INHERIT(LwLinkBwStress, LwLinkDataTest, "LwLink bandwidth stress test");

CLASS_PROP_READWRITE(LwLinkBwStress, SurfSizeFactor, UINT32,
    "Size of transfer - multiplier to TestConfiguration.SurfaceHeight (default = 10)");
CLASS_PROP_READWRITE(LwLinkBwStress, SrcSurfMode, UINT32,
                     "Source surface mode (0 = default = random & bars, 1 = all random, 2 = all zeroes,"
                     " 3 = alternate 0x0 & 0xFFFFFFFF, 4 = alternate 0xAAAAAAAA & 0x55555555,"
                     " 5 = random & zeroes, 6 = alternate 0x0F0F0F0F & 0xF0F0F0F0");
CLASS_PROP_READWRITE(LwLinkBwStress, SrcSurfModePercent, UINT32,
                     "For modes with two types of data, what percent of the source surface is filled with the first type. (default = 50)");
CLASS_PROP_READWRITE(LwLinkBwStress, Use2D, bool,
                     "Use 2D engine instead of CE (default = false)");
CLASS_PROP_READWRITE(LwLinkBwStress, CopyByLine, bool,
                     "Copy only one line at a time (default = false)");
CLASS_PROP_READWRITE(LwLinkBwStress, SrcSurfLayout, UINT32,
                     "Source surface layout (default = Pitch)");
CLASS_PROP_READWRITE(LwLinkBwStress, CeTransferWidth, UINT32,
                     "CE transfer width - 0 (system default), 128 or 256 (default = 0)");
CLASS_PROP_READWRITE(LwLinkBwStress, PixelComponentMask, UINT32,
                     "Mask of pixel components to copy (default = ~0)");
CLASS_PROP_READWRITE(LwLinkBwStress, P2PCeAllocMode, UINT32,
                     "Allocation mode for P2P CEs");
CLASS_PROP_READWRITE(LwLinkBwStress, TestFullFabric, bool,
                     "Test using special surface dimensions to full test all 8K fabric addresses");
CLASS_PROP_READWRITE(LwLinkBwStress, UseLwdaCrc, bool,
                     "Use LWCA to generate CRCs for test surfaces (default=true)");
CLASS_PROP_READWRITE(LwLinkBwStress, UseLwda, bool,
                     "Use LWCA within lwlink tests - controls both CRC and surface fills (default=true)");
CLASS_PROP_READWRITE(LwLinkBwStress, UseDmaInit, bool,
                     "Use DMA during surface fills (default=true)");
CLASS_PROP_READWRITE(LwLinkBwStress, FbSurfPageSizeKb, UINT32,
                     "Set the page size to use when allocating framebuffer surfaces (default=0 - RM default)"); //$
CLASS_PROP_READWRITE(LwLinkBwStress, UseFla, bool,
                     "Enable use of FLA addresses rather than GPA addresses for P2P surfaces (default=false)"); //$
CLASS_PROP_READWRITE(LwLinkBwStress, FlaPageSizeKb, UINT32,
                     "Set the page size to use when exporting a surface via FLA (default=0 - RM default)"); //$

PROP_CONST(LwLinkBwStress, ALL_COMPONENTS,       TestModeCe2d::ALL_COMPONENTS);
PROP_CONST(LwLinkBwStress, P2P_ALLOC_ALL_LOCAL,  TestModeCe2d::P2P_ALLOC_ALL_LOCAL);  //$
PROP_CONST(LwLinkBwStress, P2P_ALLOC_ALL_REMOTE, TestModeCe2d::P2P_ALLOC_ALL_REMOTE); //$
PROP_CONST(LwLinkBwStress, P2P_ALLOC_IN_LOCAL,   TestModeCe2d::P2P_ALLOC_IN_LOCAL);   //$
PROP_CONST(LwLinkBwStress, P2P_ALLOC_OUT_LOCAL,  TestModeCe2d::P2P_ALLOC_OUT_LOCAL);  //$
PROP_CONST(LwLinkBwStress, P2P_ALLOC_DEFAULT,    TestModeCe2d::P2P_ALLOC_DEFAULT);    //$

