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

#include "lwlbws_test_mode.h"
#include "lwlbws_test_route.h"
#include "lwlbws_alloc_mgr.h"
#include "core/include/channel.h"
#include "gpu/tests/gputestc.h"
#include "class/cla06fsubch.h"
#include "gpu/include/dmawrap.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/utility.h"
#include "class/clc0b5.h"
#include "class/clc1b5.h"
#include "class/clc3b5.h"
#include "class/clc5b5.h"
#include "class/clc6b5.h"
#include "class/clc7b5.h"
#include "class/clc8b5.h" // HOPPER_DMA_COPY_A
#include "class/clc9b5.h" // BLACKWELL_DMA_COPY_A
#include "class/cl902d.h"
#include "core/include/platform.h"
#include "core/include/tasker.h"
#include "gpu/utility/surf2d.h"
#include "core/include/cnslmgr.h"
#include "core/include/console.h"
#include "core/include/xp.h"
#include "gpu/lwlink/lwlinkimpl.h"
#include "core/include/cpu.h"
#include "gpu/include/dmawrap.h"
#include <set>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <boost/mpl/if.hpp>

namespace
{
    const UINT64 PROGRESS_INDICATOR_SEC = 5;
    const UINT32 COPY_START_SIGNAL = 0xdeadbeef;

    RC GetPceMask(GpuSubdevice *pSubdev, UINT32 engineIndex, UINT32 *pceMask)
    {
        RC rc;
        LW2080_CTRL_CE_GET_CE_PCE_MASK_PARAMS pceMaskParams = { };
        pceMaskParams.ceEngineType = LW2080_ENGINE_TYPE_COPY(engineIndex);
        CHECK_RC(LwRmPtr()->ControlBySubdevice(pSubdev,
                                               LW2080_CTRL_CMD_CE_GET_CE_PCE_MASK,
                                               &pceMaskParams,
                                               sizeof(pceMaskParams)));
        *pceMask = pceMaskParams.pceMask;
        return rc;
    }
}

using namespace LwLinkDataTestHelper;

//------------------------------------------------------------------------------
//! \brief Constructor
//!
TestModeCe2d::TestModeCe2d
(
    GpuTestConfiguration *pTestConfig
   ,AllocMgrPtr pAllocMgr
   ,UINT32 numErrorsToPrint
   ,UINT32 copyLoopCount
   ,FLOAT64 bwThresholdPct
   ,bool bShowBandwidthStats
   ,UINT32 pauseMask
   ,Tee::Priority pri
) : TestMode(pTestConfig,
             pAllocMgr,
             numErrorsToPrint,
             copyLoopCount,
             bwThresholdPct,
             bShowBandwidthStats,
             pauseMask,
             pri)
{
}

//------------------------------------------------------------------------------
//! \brief Destructor
//!
TestModeCe2d::~TestModeCe2d()
{
    ReleaseResources();
}

//------------------------------------------------------------------------------
//! \brief Acquire resources on both the "dut" and any remote devices in order
//! that the test mode can execute all necessary copies.  Resources are acquired
//! from the allocation manager which "lazy allocates"
//!
//! \return OK if successful, not OK if resource acquire failed
/* virtual */ RC TestModeCe2d::AcquireResources()
{
    RC rc;

    Utility::CleanupOnReturn<TestModeCe2d, RC> releaseResources(this,
                                                                &TestModeCe2d::ReleaseResources);

    const bool bAutoAdjustCopyLoops = (GetCopyLoopCount() == 0);
    for (auto & testMapEntry : GetTestData())
    {
        TestDirectionDataCe2d *pTdCe2d;
        TestRouteCe2d *pTestRouteCe2d = static_cast<TestRouteCe2d *>(testMapEntry.first.get());

        Printf(GetPrintPri(),
               "%s : Acquiring resources for test route:\n"
               "%s\n",
               __FUNCTION__,
               pTestRouteCe2d->GetString("    ").c_str());

        GpuDevice *pLocDev = pTestRouteCe2d->GetLocalSubdevice()->GetParentDevice();
        GpuDevice *pRemDev = nullptr;
        if (pTestRouteCe2d->GetTransferType() == TT_P2P)
            pRemDev = pTestRouteCe2d->GetRemoteSubdevice()->GetParentDevice();

        if (m_CopyTrigSem.pAddress == nullptr)
        {
            AllocMgrCe2d * pAllocMgr = static_cast<AllocMgrCe2d *>(GetAllocMgr().get());
            CHECK_RC(pAllocMgr->AcquireSemaphore(pLocDev, &m_CopyTrigSem));
        }

        pTdCe2d = static_cast<TestDirectionDataCe2d *>(testMapEntry.second.pDutInTestData.get());
        CHECK_RC(pTdCe2d->Acquire(m_SrcLayout,
                                  pTestRouteCe2d->GetTransferType(),
                                  m_CopyTrigSem,
                                  pLocDev,
                                  pRemDev,
                                  GetTestConfig(),
                                  m_SurfaceSizeFactor));
        CHECK_RC(pTdCe2d->SetupTestSurfaces(GetTestConfig(), m_SrcSurfModePercent));
        if (bAutoAdjustCopyLoops)
            AutoAdjustCopyLoopcount(pTestRouteCe2d, pTdCe2d);

        pTdCe2d = static_cast<TestDirectionDataCe2d *>(testMapEntry.second.pDutOutTestData.get());
        CHECK_RC(pTdCe2d->Acquire(m_SrcLayout,
                                  pTestRouteCe2d->GetTransferType(),
                                  m_CopyTrigSem,
                                  pLocDev,
                                  pRemDev,
                                  GetTestConfig(),
                                  m_SurfaceSizeFactor));
        CHECK_RC(pTdCe2d->SetupTestSurfaces(GetTestConfig(), m_SrcSurfModePercent));
        if (bAutoAdjustCopyLoops)
            AutoAdjustCopyLoopcount(pTestRouteCe2d, pTdCe2d);

        if (m_UseLwdaCrc)
        {
            AllocMgrCe2d   * pAllocMgr = static_cast<AllocMgrCe2d *>(GetAllocMgr().get());
            LwSurfRoutines * pLwSurfRoutines;
            CHECK_RC(pAllocMgr->AcquireLwdaUtils(pLocDev, &pLwSurfRoutines));
            DEFER() { pAllocMgr->ReleaseLwdaUtils(&pLwSurfRoutines); };

            // Need to compute the CRCs with the same size across all GPUs since
            // the LWCA CRCs will be compared across multiple devices
            if (pLwSurfRoutines->IsSupported())
                m_LwCrcSize.Min(pLwSurfRoutines->GetCrcSize());

            if (pRemDev != nullptr)
            {
                LwSurfRoutines * pRemLwSurfRoutines;
                CHECK_RC(pAllocMgr->AcquireLwdaUtils(pRemDev, &pRemLwSurfRoutines));
                DEFER() { pAllocMgr->ReleaseLwdaUtils(&pRemLwSurfRoutines); };
                if (pRemLwSurfRoutines->IsSupported())
                    m_LwCrcSize.Min(pRemLwSurfRoutines->GetCrcSize());
            }
        }

        pTestRouteCe2d->SetCeCopyWidth(m_CeCopyWidth);
    }

    // If the CRC data is valid then free it so that CRC data from a previous run of this test
    // mode is not used
    if (m_LwGoldCrcData.GetSize())
        m_LwGoldCrcData.Free();
    if (m_LwLwrCrcData.GetSize())
        m_LwLwrCrcData.Free();

    CHECK_RC(TestMode::AcquireResources());
    releaseResources.Cancel();

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Assign the copy engines to each route.  Each route will
//! be assigned a single logical copy engine that is recommended by RM for lwlink
//! traffic between each endpoint of the route.  MODS relies on RM assigning
//! suffient PCEs to each LCE such that the link will be saturated
//!
//! \return OK if successful, not OK on failure or if unable to assign a LCE
//! on any particular route/direction
RC TestModeCe2d::AssignCopyEngines()
{
    RC rc;
    map<GpuSubdevice *, UINT32> assignedCeMasks;

    Utility::CleanupOnReturn<TestModeCe2d, void> resetTestData(this, &TestModeCe2d::ResetTestData);

    CHECK_RC(AssignForcedLocalCes(&assignedCeMasks));
    CHECK_RC(AssignP2PCes(&assignedCeMasks));

    m_bTwoDTranfers = false;
    resetTestData.Cancel();
    m_bCopyEnginesAssigned = true;
    m_OnePcePerLce = false;
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Assign the copy engines to each route.  Each route will
//! be assigned the number of copy engines required to saturate the raw
//! bandwidth of all the LwLinks in the route in all directions being
//! tested.  This method of assignment is only used if all GPUs in the system
//! have exactly one PCE assigned to each LCE
//!
//! \return OK if successful, not OK on failure or if unable to assign enough
//! copy engines to saturate all links of all routes being tested in all
//! directions
RC TestModeCe2d::AssignCesOnePcePerLce()
{
    RC rc;
    map<GpuSubdevice *, pair<INT32, UINT32>> avaiableCes;

    Utility::CleanupOnReturn<TestModeCe2d, void> resetTestData(this, &TestModeCe2d::ResetTestData);

    CHECK_RC(AssignOneToOneForcedLocalCes(&avaiableCes));
    CHECK_RC(AssignOneToOneP2PCes(&avaiableCes));

    m_bTwoDTranfers = false;
    resetTestData.Cancel();
    m_bCopyEnginesAssigned = true;
    m_OnePcePerLce = true;
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Assign the twod engine for testing of a route.  For twod testing
//! is restricted to a single route in a single direction at a time
//!
//! \return OK if successful, not OK on failure or if the current test mode
//! cannot be supported via TwoD
RC TestModeCe2d::AssignTwod()
{
    if (!OnlyLoopback() && (GetTransferDir() == TD_IN_OUT))
    {
        Printf(Tee::PriError,
               "Bidirectional transfer only supported with loopback routes using TwoD\n");
        return RC::UNSUPPORTED_SYSTEM_CONFIG;
    }

    set<GpuSubdevice *> twoDAssigned;
    for (auto & testMapEntry : GetTestData())
    {
        testMapEntry.second.pDutInTestData->Reset();
        testMapEntry.second.pDutOutTestData->Reset();

        GpuSubdevice *pLocSub = testMapEntry.first->GetLocalSubdevice();
        if (twoDAssigned.find(pLocSub) == twoDAssigned.end())
        {
            twoDAssigned.insert(pLocSub);
        }
        else
        {
            Printf(Tee::PriError,
                   "Only one route per subdevice may be tested at a time with TwoD\n");
            return RC::UNSUPPORTED_SYSTEM_CONFIG;
        }

        TestDirectionDataCe2d *pDutDataCe2d;
        if (OnlyLoopback())
        {
            if (GetLoopbackInput())
            {
                pDutDataCe2d =
                    static_cast<TestDirectionDataCe2d *>(testMapEntry.second.pDutInTestData.get());
            }
            else
            {
                pDutDataCe2d =
                    static_cast<TestDirectionDataCe2d *>(testMapEntry.second.pDutOutTestData.get());
            }
        }
        else if (GetTransferDir() != TD_OUT_ONLY)
        {
            pDutDataCe2d =
                static_cast<TestDirectionDataCe2d *>(testMapEntry.second.pDutInTestData.get());
        }
        else
        {
            pDutDataCe2d =
                static_cast<TestDirectionDataCe2d *>(testMapEntry.second.pDutOutTestData.get());
        }

        pDutDataCe2d->SetHwLocal(true);
        pDutDataCe2d->SetTransferHw(LwLinkImpl::HT_TWOD);
        pDutDataCe2d->SetNumCopies(1);
        m_TotalCopies = 1;
    }

    m_bTwoDTranfers = true;
    return OK;
}

//------------------------------------------------------------------------------
//! \brief Dump surfaces for the specified frame number
//!
//! \param fnamePrefix : filename prefix to use
//!
//! \return OK if dump was successful, not OK otherwise
RC TestModeCe2d::DumpSurfaces(string fnamePrefix)
{
    RC rc;
    for (auto testMapEntry : GetTestData())
    {
        string fnameBase =
            fnamePrefix + "_" + testMapEntry.first->GetFilenameString();

        string fname;
        TestDirectionDataCe2d *pTdCe2d;
        pTdCe2d =
            static_cast<TestDirectionDataCe2d *>(testMapEntry.second.pDutInTestData.get());
        if (pTdCe2d->IsInUse())
        {
            CHECK_RC(pTdCe2d->WritePngs(fnameBase + "_in"));
        }

        pTdCe2d =
            static_cast<TestDirectionDataCe2d *>(testMapEntry.second.pDutOutTestData.get());
        if (pTdCe2d->IsInUse())
        {
            CHECK_RC(pTdCe2d->WritePngs(fnameBase + "_out"));
        }
    }
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Returns the maximum number of copy engines present on any GPU on any
//!        route
//!
//! \return Maximum number of copy engines present on any GPU on any route
UINT32 TestModeCe2d::GetMaxCopyEngines()
{
    UINT32 maxCopyEngines = 0;
    for (auto const & testRteData : GetTestData())
    {
        TestRouteCe2d *pTrCe2d;
        pTrCe2d = static_cast<TestRouteCe2d *>(testRteData.first.get());

        maxCopyEngines = max(pTrCe2d->GetLocalCeCount(),
                             max(pTrCe2d->GetRemoteCeCount(),
                                 maxCopyEngines));
    }
    return maxCopyEngines;
}

//------------------------------------------------------------------------------
//! \brief Print the details of the test mode
//!
//! \param pri    : Priority to print at
//! \param prefix : Prefix for each line of the print
//!
//! \return OK if initialization was successful, not OK otherwise
void TestModeCe2d::Print(Tee::Priority pri, string prefix)
{
    string testModeStr = prefix + "Dir    Sysmem Dir    # Rte    HwType\n" +
                         prefix + "----------------------\n";

    testModeStr += prefix +
        Utility::StrPrintf("%3s    %10s    %5u    %6s\n",
               TransferDirStr(GetTransferDir()).c_str(),
               TransferDirStr(GetSysmemTransferDir()).c_str(),
               GetNumRoutes(),
               m_bTwoDTranfers ? "TwoD" : "CE");

    UINT32 linkColWidth = 0;
    UINT32 ttColWidth = 0;
    UINT32 inCeColWidth = 0;
    UINT32 outCeColWidth = 0;
    for (auto const & testRteData : GetTestData())
    {
        TestRoutePtr pTestRoute = testRteData.first;
        size_t tempSize = pTestRoute->GetLocalDevString().size() +
                          3 +
                          pTestRoute->GetLinkString(false).size();
        linkColWidth = max(static_cast<UINT32>(tempSize), linkColWidth);
        tempSize = pTestRoute->GetRemoteDevString().size() +
                   3 +
                   pTestRoute->GetLinkString(true).size();
        linkColWidth = max(static_cast<UINT32>(tempSize), linkColWidth);

        tempSize = TransferTypeStr(pTestRoute->GetTransferType()).size();
        if ((pTestRoute->GetTransferType() == TT_SYSMEM) && GetSysmemLoopback())
            tempSize += 3;
        ttColWidth   = max(static_cast<UINT32>(tempSize), ttColWidth);

        if (!m_bTwoDTranfers && m_bCopyEnginesAssigned)
        {
            TestDirectionDataCe2d *pDutInCe2d, *pDutOutCe2d;
            pDutInCe2d  =
                static_cast<TestDirectionDataCe2d *>(testRteData.second.pDutInTestData.get());
            pDutOutCe2d =
                static_cast<TestDirectionDataCe2d *>(testRteData.second.pDutOutTestData.get());

            tempSize = pDutInCe2d->GetCeString().size();
            inCeColWidth  = max(static_cast<UINT32>(tempSize), inCeColWidth);
            tempSize = pDutOutCe2d->GetCeString().size();
            outCeColWidth = max(static_cast<UINT32>(tempSize), outCeColWidth);
        }
    }

    // Ensure that the max Link field width is at least 7 (length of "Link(s)")
    static const string locLinkStr = "Loc Link(s)";
    static const string remLinkStr = "Rem Link(s)";
    if (linkColWidth < locLinkStr.size())
        linkColWidth = static_cast<UINT32>(locLinkStr.size());

    // Ensure that the max input ce field width is at least 6 (length of "Out CEs")
    static const string typeStr = "Type";
    if (ttColWidth < typeStr.size())
        ttColWidth = static_cast<UINT32>(typeStr.size());

    static const string inCeHdrStr = "In LCE[PCEs]";
    static const string outCeHdrStr = "Out LCE[PCEs]";
    if (!m_bTwoDTranfers && m_bCopyEnginesAssigned)
    {
        // Ensure that the max input ce field width is large enough to fit the header
        if (inCeColWidth < inCeHdrStr.size())
            inCeColWidth = static_cast<UINT32>(inCeHdrStr.size());

        // Ensure that the max input ce field width is large enough to fit the header
        if (outCeColWidth < outCeHdrStr.size())
            outCeColWidth = static_cast<UINT32>(outCeHdrStr.size());
    }

    // Construct the header and the line under it, padding the lines with
    // appropriate characters based on the maximum link field width
    string hdrStr = prefix;
    hdrStr += "  ";
    hdrStr += string(linkColWidth - locLinkStr.size(), ' ');
    hdrStr += locLinkStr + "   ";
    hdrStr += string(ttColWidth - typeStr.size(), ' ');
    hdrStr += typeStr + "   ";
    hdrStr += string(linkColWidth - remLinkStr.size(), ' ');
    hdrStr += remLinkStr;
    if (!m_bTwoDTranfers && m_bCopyEnginesAssigned)
    {
        hdrStr += "   " + string(inCeColWidth - inCeHdrStr.size(), ' ');
        hdrStr += inCeHdrStr;
        hdrStr += "   " + string(outCeColWidth - outCeHdrStr.size(), ' ');
        hdrStr += outCeHdrStr;
    }
    UINT32 dashCount = hdrStr.size() - prefix.size();
    testModeStr += hdrStr + "\n";
    testModeStr += prefix;
    testModeStr += "  ";
    testModeStr += string(dashCount, '-');
    testModeStr += "\n";

    for (auto const & testRteData : GetTestData())
    {
        TestRoutePtr pTestRoute = testRteData.first;
        const string locLinkStr = pTestRoute->GetLocalDevString() +
                                  " : " +
                                  pTestRoute->GetLinkString(false);
        const string remLinkStr = pTestRoute->GetRemoteDevString() +
                                  " : " +
                                  pTestRoute->GetLinkString(true);

        string typeStr = TransferTypeStr(pTestRoute->GetTransferType());

        if ((pTestRoute->GetTransferType() == TT_SYSMEM) && GetSysmemLoopback())
            typeStr += "(L)";

        testModeStr += Utility::StrPrintf(
                "%s  %*s   %*s   %*s",
                prefix.c_str(),
                linkColWidth,
                locLinkStr.c_str(),
                ttColWidth,
                typeStr.c_str(),
                linkColWidth,
                remLinkStr.c_str());
        if (!m_bTwoDTranfers && m_bCopyEnginesAssigned)
        {
            TestDirectionDataCe2d *pDutInCe2d, *pDutOutCe2d;
            pDutInCe2d =
                static_cast<TestDirectionDataCe2d *>(testRteData.second.pDutInTestData.get());
            pDutOutCe2d =
                static_cast<TestDirectionDataCe2d *>(testRteData.second.pDutOutTestData.get());

            const string inCeStr = pDutInCe2d->GetCeString();
            const string outCeStr = pDutOutCe2d->GetCeString();
            testModeStr += Utility::StrPrintf("   %*s   %*s",
                                              inCeColWidth,
                                              inCeStr.c_str(),
                                              outCeColWidth,
                                              outCeStr.c_str());
        }
        testModeStr += "\n";
    }
    Printf(pri, "\n%s\n", testModeStr.c_str());
}

//------------------------------------------------------------------------------
//! \brief Release resources on both the "dut" and any remote devices.  Note
//! that this only releases resource usage and does not actually free the
//! resources since the resources may be used in future test modes
//!
//! \return OK if successful, not OK if resource release failed
RC TestModeCe2d::ReleaseResources()
{
    StickyRC rc = TestMode::ReleaseResources();

    for (auto & testRteData : GetTestData())
    {
        TestRouteCe2d *pTrCe2d;
        pTrCe2d = static_cast<TestRouteCe2d *>(testRteData.first.get());

        pTrCe2d->RestoreCeCopyWidth();
    }

    if (m_CopyTrigSem.pAddress)
    {
        AllocMgrCe2d *pAllocMgr = static_cast<AllocMgrCe2d *>(GetAllocMgr().get());
        rc = pAllocMgr->ReleaseSemaphore(m_CopyTrigSem.pAddress);
        m_CopyTrigSem = AllocMgrCe2d::SemaphoreData();
    }

    if (m_LwGoldCrcData.GetSize())
        m_LwGoldCrcData.Free();
    if (m_LwLwrCrcData.GetSize())
        m_LwLwrCrcData.Free();

    return rc;
}

//------------------------------------------------------------------------------
// TestModeCe2d protected implementation
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//! \brief Reset test data assignments to a default state
void TestModeCe2d::ResetTestData()
{
    TestMode::ResetTestData();
    m_TotalCopies = 0;
    m_bCopyEnginesAssigned = false;
}

//------------------------------------------------------------------------------
// TestModeCe2d private implementation
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
RC TestModeCe2d::AssignCeToTestDir
(
    TestDirectionDataPtr   pTestDir,
    GpuSubdevice *         pSubdev,
    bool                   bLocalHw,
    UINT32                 ceIdx
)
{
    RC rc;
    auto pCe2d = static_cast<TestDirectionDataCe2d *>(pTestDir.get());

    if (ceIdx != LW2080_ENGINE_TYPE_COPY_SIZE)
    {
        pCe2d->SetHwLocal(bLocalHw);
        pCe2d->SetTransferHw(LwLinkImpl::HT_CE);
        pCe2d->SetNumCopies(1);
        pCe2d->SetCeId(0, ceIdx);

        UINT32 pceMask = 0;
        CHECK_RC(GetPceMask(pSubdev, ceIdx, &pceMask));
        pCe2d->SetPceMask(0, pceMask);

        m_TotalCopies++;
    }
    else
    {
        pCe2d->Reset();
    }
    return rc;
}

//------------------------------------------------------------------------------
void TestModeCe2d::AssignOneToOneCesToTestDir
(
    TestDirectionDataPtr   pTestDir,
    bool                   bLocalHw,
    UINT32                 firstCe,
    UINT32                 ceCount
)
{
    auto pCe2d = static_cast<TestDirectionDataCe2d *>(pTestDir.get());
    if (ceCount)
    {
        pCe2d->SetHwLocal(bLocalHw);
        pCe2d->SetTransferHw(LwLinkImpl::HT_CE);
        pCe2d->SetNumCopies(ceCount);
        m_TotalCopies += ceCount;

        for (size_t ii = 0; ii < ceCount; ++ii, ++firstCe)
        {
            pCe2d->SetCeId(ii, firstCe);
            pCe2d->SetPceMask(ii, 1U << firstCe);
        }
    }
    else
    {
        pCe2d->Reset();
    }
}

//------------------------------------------------------------------------------
RC TestModeCe2d::AssignForcedLocalCes(map<GpuSubdevice *, UINT32> *pAssignedCeMasks)
{
    RC rc;

    for (auto & testMapEntry : GetTestData())
    {
        // Eradicate any previous assignments
        testMapEntry.second.pDutInTestData->Reset();
        testMapEntry.second.pDutOutTestData->Reset();

        auto pTestRouteCe2d = static_cast<TestRouteCe2d *>(testMapEntry.first.get());

        GpuSubdevice *pLocSub = pTestRouteCe2d->GetLocalSubdevice();
        if (pAssignedCeMasks->find(pLocSub) == pAssignedCeMasks->end())
        {
            pAssignedCeMasks->emplace(pLocSub, 0);
        }

        if (pTestRouteCe2d->GetTransferType() == TT_P2P)
        {
            GpuSubdevice *pRemSub = pTestRouteCe2d->GetRemoteSubdevice();
            if (pAssignedCeMasks->find(pRemSub) == pAssignedCeMasks->end())
            {
                pAssignedCeMasks->emplace(pRemSub, 0);
            }
            continue;
        }

        UINT32 usableInCeMask;
        UINT32 usableOutCeMask;
        CHECK_RC(pTestRouteCe2d->GetUsableCeMasks(true,
                                                  &usableInCeMask,
                                                  &usableOutCeMask));

        usableInCeMask  &= ~pAssignedCeMasks->at(pLocSub);
        usableOutCeMask &= ~pAssignedCeMasks->at(pLocSub);

        UINT32 inputCe  = LW2080_ENGINE_TYPE_COPY_SIZE;
        UINT32 outputCe = LW2080_ENGINE_TYPE_COPY_SIZE;
        bool bCeNotFound = false;
        if (pTestRouteCe2d->GetTransferType() == TT_SYSMEM)
        {
            // If testing system memory via loopback (i.e. with source and
            // destination both system memory).  Prefer the output CE mask since
            // typically system memory throughput is throttled based on write
            // performance.  Choose to store the CE to use in the input CE.
            // Unlike normal loopback it doesnt matter whether in or out is
            // chosen since both reads and writes will occur through lwlink
            // regardless of the choice
            if (GetSysmemLoopback())
            {
                if (usableOutCeMask)
                    inputCe = static_cast<UINT32>(Utility::BitScanForward(usableOutCeMask));
                else if (usableInCeMask)
                    inputCe = static_cast<UINT32>(Utility::BitScanForward(usableInCeMask));
                else
                    bCeNotFound = true;
            }
            else 
            {
                TransferDir td = GetSysmemTransferDir();
                if (td != TD_OUT_ONLY)
                {
                    bCeNotFound = bCeNotFound || (usableInCeMask == 0);
                    if (!bCeNotFound)
                    {
                        inputCe = static_cast<UINT32>(Utility::BitScanForward(usableInCeMask));

                        // Disallow the input CE from being used for output as well
                        usableOutCeMask &= ~(1 << inputCe);
                    }
                }

                if (td != TD_IN_ONLY)
                {
                    bCeNotFound = bCeNotFound || (usableOutCeMask == 0);
                    if (!bCeNotFound)
                        outputCe = static_cast<UINT32>(Utility::BitScanForward(usableOutCeMask));
                }
            }
        }

        // For loopback, use in/out CE depending upon setting.  Input means that
        // reads will occur over lwlink, output means that writes will occur over lwlink
        if (pTestRouteCe2d->GetTransferType() == TT_LOOPBACK)
        {
            bCeNotFound = GetLoopbackInput() ? (usableInCeMask == 0) : (usableOutCeMask == 0);
            if (!bCeNotFound)
            {
                if (GetLoopbackInput())
                    inputCe = static_cast<UINT32>(Utility::BitScanForward(usableInCeMask));
                else
                    outputCe = static_cast<UINT32>(Utility::BitScanForward(usableOutCeMask));
            }
        }

        // If there are not enough CEs on the DUT remaining to saturate the
        // current route then fail
        if (bCeNotFound)
        {
            Printf(GetPrintPri(),
                   "%s : Copy engine assignment failed on route :\n%s\n",
                   __FUNCTION__, pTestRouteCe2d->GetString("    ").c_str());
            return RC::UNSUPPORTED_SYSTEM_CONFIG;
        }

        CHECK_RC(AssignCeToTestDir(testMapEntry.second.pDutInTestData,
                                   pLocSub,
                                   true,
                                   inputCe));
        pAssignedCeMasks->at(pLocSub) |= (1U << inputCe);

        CHECK_RC(AssignCeToTestDir(testMapEntry.second.pDutOutTestData,
                                   pLocSub,
                                   true,
                                   outputCe));
        pAssignedCeMasks->at(pLocSub) |= (1U << outputCe);

        if ((testMapEntry.second.pDutInTestData->GetTransferHw() == LwLinkImpl::HT_NONE) &&
            (testMapEntry.second.pDutOutTestData->GetTransferHw() == LwLinkImpl::HT_NONE))
        {
            Printf(Tee::PriError,
                   "%s : No copy engines assigned on route :\n%s\n",
                   __FUNCTION__, pTestRouteCe2d->GetString("    ").c_str());
            return RC::SOFTWARE_ERROR;
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
RC TestModeCe2d::AssignOneToOneForcedLocalCes(map<GpuSubdevice *, pair<INT32, UINT32>> *pAvailCes)
{
    RC rc;

    for (auto & testMapEntry : GetTestData())
    {
        // Eradicate any previous assignments
        testMapEntry.second.pDutInTestData->Reset();
        testMapEntry.second.pDutOutTestData->Reset();

        TestRouteCe2d *pTestRouteCe2d;
        pTestRouteCe2d = static_cast<TestRouteCe2d *>(testMapEntry.first.get());

        GpuSubdevice *pLocSub = pTestRouteCe2d->GetLocalSubdevice();
        if (pAvailCes->find(pLocSub) == pAvailCes->end())
        {
            pair<UINT32, UINT32> ceInfo(0, pTestRouteCe2d->GetLocalCeCount());
            pAvailCes->emplace(pLocSub, ceInfo);
        }

        if (pTestRouteCe2d->GetTransferType() == TT_P2P)
        {
            GpuSubdevice *pRemSub = pTestRouteCe2d->GetRemoteSubdevice();
            if (pAvailCes->find(pRemSub) == pAvailCes->end())
            {
                pair<UINT32, UINT32> ceInfo(0, pTestRouteCe2d->GetRemoteCeCount());
                pAvailCes->emplace(pRemSub, ceInfo);
            }
            continue;
        }

        // Sysmem and loopback routes require CEs on the local GPU so
        // assign them first
        TransferDir td = GetTransferDir();
        if (pTestRouteCe2d->GetTransferType() == TT_SYSMEM)
            td = GetSysmemTransferDir();

        UINT32 inCeCount;
        UINT32 outCeCount;
        CHECK_RC(pTestRouteCe2d->GetRequiredCes(td,
                                                true,
                                                &inCeCount,
                                                &outCeCount));

        // If testing system memory via loopback (i.e. with source and
        // destination both system memory).  Use the minimum CE count
        // and choose to put it into the input CE count - unlike normal loopback
        // it doesnt matter whether in or out is chosen since both reads and
        // writes will occur through lwlink regardless of the choice
        if ((pTestRouteCe2d->GetTransferType() == TT_SYSMEM) &&
            GetSysmemLoopback())
        {
            inCeCount  = min(inCeCount, outCeCount);
            outCeCount = 0;
        }

        // For loopback, use in/out CE depending upon setting.  Input means that
        // reads will occur over lwlink, output means that writes will occur
        if (pTestRouteCe2d->GetTransferType() == TT_LOOPBACK)
        {
            const UINT32 minCes = min(inCeCount, outCeCount);
            inCeCount  = GetLoopbackInput() ? minCes : 0;
            outCeCount = GetLoopbackInput() ? 0 : minCes;
        }

        // If there are not enough CEs on the DUT remaining to saturate the
        // current route then fail
        if ((pAvailCes->at(pLocSub).first + inCeCount + outCeCount) > pAvailCes->at(pLocSub).second) //$
        {
            Printf(GetPrintPri(),
                   "%s : Copy engine assignment failed on route :\n%s\n",
                   __FUNCTION__, pTestRouteCe2d->GetString("    ").c_str());
            return RC::UNSUPPORTED_SYSTEM_CONFIG;
        }

        AssignOneToOneCesToTestDir(testMapEntry.second.pDutInTestData,
                                   true, 
                                   pAvailCes->at(pLocSub).first,
                                   inCeCount);
        pAvailCes->at(pLocSub).first += inCeCount;

        AssignOneToOneCesToTestDir(testMapEntry.second.pDutOutTestData,
                                   true, 
                                   pAvailCes->at(pLocSub).first,
                                   outCeCount);
        pAvailCes->at(pLocSub).first += outCeCount;

        if ((testMapEntry.second.pDutInTestData->GetTransferHw() == LwLinkImpl::HT_NONE) &&
            (testMapEntry.second.pDutOutTestData->GetTransferHw() == LwLinkImpl::HT_NONE))
        {
            Printf(Tee::PriError,
                   "%s : No copy engines assigned on route :\n%s\n",
                   __FUNCTION__, pTestRouteCe2d->GetString("    ").c_str());
            return RC::SOFTWARE_ERROR;
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
RC TestModeCe2d::AssignOneToOneP2PCes(map<GpuSubdevice *, pair<INT32, UINT32>> *pAvailCes)
{
    RC rc;

    for (auto & testMapEntry : GetTestData())
    {
        TestRouteCe2d *pTestRouteCe2d;
        pTestRouteCe2d = static_cast<TestRouteCe2d *>(testMapEntry.first.get());

        if (pTestRouteCe2d->GetTransferType() != TT_P2P)
            continue;

        P2PAllocMode p2pAlloc   = m_P2PAllocMode;
        UINT32 dutInLocCeCount  = 0;
        UINT32 dutOutLocCeCount = 0;
        UINT32 dutInRemCeCount  = 0;
        UINT32 dutOutRemCeCount = 0;

        CHECK_RC(pTestRouteCe2d->GetRequiredCes(GetTransferDir(),
                                                true,
                                                &dutInLocCeCount,
                                                &dutOutLocCeCount));
        CHECK_RC(pTestRouteCe2d->GetRequiredCes(GetTransferDir(),
                                                false,
                                                &dutInRemCeCount,
                                                &dutOutRemCeCount));

        GpuSubdevice *pLocSub = pTestRouteCe2d->GetLocalSubdevice();
        GpuSubdevice *pRemSub = pTestRouteCe2d->GetRemoteSubdevice();

        if (p2pAlloc == P2P_ALLOC_DEFAULT)
        {
            // Assign Copy Engines for P2P transfer based on the following priority:
            //
            //    1. If all necessary CEs for generating output P2P traffic from the
            //       DUT can be allocated on the DUT, then do so
            //    2. If all necessary CEs for generating input P2P traffic to the
            //       DUT can be allocated on the DUT, then do so
            //    3. If all necessary CEs generating P2P traffic for all
            //       directions under test can be allocated on the DUT, then do so
            //    4. Allocate all CEs for generating output P2P traffic for all
            //       directions under test on the remote peer device
            //
            // Note that is important not to assume that the local and remote CE
            // counts are identical since the GPUs may not be 100% symmetrical (for
            // instace the CE bandwidth will change depending upon the sysclk on
            // each GPU)
            if ((GetTransferDir() != TD_IN_ONLY) &&
                ((pAvailCes->at(pLocSub).first + dutOutLocCeCount) <= pAvailCes->at(pLocSub).second) &&  //$
                ((pAvailCes->at(pRemSub).first + dutInRemCeCount) <= pAvailCes->at(pRemSub).second))     //$
            {
                p2pAlloc = P2P_ALLOC_OUT_LOCAL;
            }
            else if ((GetTransferDir() != TD_OUT_ONLY) &&
                     ((pAvailCes->at(pLocSub).first + dutInLocCeCount) <= pAvailCes->at(pLocSub).second) &&  //$
                     ((pAvailCes->at(pRemSub).first + dutOutRemCeCount) <= pAvailCes->at(pRemSub).second))   //$
            {
                p2pAlloc = P2P_ALLOC_IN_LOCAL;
            }
            else if ((pAvailCes->at(pLocSub).first + dutInLocCeCount + dutOutLocCeCount) <= pAvailCes->at(pLocSub).second) //$
            {
                p2pAlloc = P2P_ALLOC_ALL_LOCAL;
            }
            else
            {
                p2pAlloc = P2P_ALLOC_ALL_REMOTE;
            }
        }

        bool bInTransferLocal    = false;
        bool bOutTransferLocal   = false;
        UINT32 dutInCeCount      = 0;
        UINT32 dutOutCeCount     = 0;
        UINT32 dutLocalCeCount   = 0;
        UINT32 dutRemoteCeCount  = 0;
        switch (p2pAlloc)
        {
            case P2P_ALLOC_ALL_LOCAL:
                bInTransferLocal  = true;
                bOutTransferLocal = true;
                dutInCeCount      = dutInLocCeCount;
                dutOutCeCount     = dutOutLocCeCount;
                dutLocalCeCount   = dutInLocCeCount + dutOutLocCeCount;
                dutRemoteCeCount  = 0;
                break;
            case P2P_ALLOC_OUT_LOCAL:
                bInTransferLocal  = false;
                bOutTransferLocal = true;
                dutInCeCount      = dutInRemCeCount;
                dutOutCeCount     = dutOutLocCeCount;
                dutLocalCeCount   = dutOutCeCount;
                dutRemoteCeCount  = dutInCeCount;
                break;
            case P2P_ALLOC_IN_LOCAL:
                bInTransferLocal  = true;
                bOutTransferLocal = false;
                dutInCeCount      = dutInLocCeCount;
                dutOutCeCount     = dutOutRemCeCount;
                dutLocalCeCount   = dutInCeCount;
                dutRemoteCeCount  = dutOutCeCount;
                break;
            case P2P_ALLOC_ALL_REMOTE:
                bInTransferLocal  = false;
                bOutTransferLocal = false;
                dutInCeCount      = dutInRemCeCount;
                dutOutCeCount     = dutOutRemCeCount;
                dutLocalCeCount   = 0;
                dutRemoteCeCount  = dutInCeCount + dutOutCeCount;
                break;
            default:
                Printf(Tee::PriError, "%s : Unknown P2P allocation mode %d on route :\n%s\n",
                       __FUNCTION__, p2pAlloc, pTestRouteCe2d->GetString("    ").c_str());
                return RC::SOFTWARE_ERROR;
        }

        // Ensure a valid configuration
        if (((pAvailCes->at(pLocSub).first + dutLocalCeCount) > pAvailCes->at(pLocSub).second) ||
            ((pAvailCes->at(pRemSub).first + dutRemoteCeCount) > pAvailCes->at(pRemSub).second))

        {
            Printf(GetPrintPri(),
                   "%s : Copy engine assignment failed on route :\n%s\n",
                   __FUNCTION__, pTestRouteCe2d->GetString("    ").c_str());
            return RC::UNSUPPORTED_SYSTEM_CONFIG;
        }

        AssignOneToOneCesToTestDir(testMapEntry.second.pDutInTestData,
                                   bInTransferLocal, 
                                   pAvailCes->at(bInTransferLocal ? pLocSub : pRemSub).first,
                                   dutInCeCount);
        pAvailCes->at(bInTransferLocal ? pLocSub : pRemSub).first += dutInCeCount;


        AssignOneToOneCesToTestDir(testMapEntry.second.pDutOutTestData,
                                   bOutTransferLocal, 
                                   pAvailCes->at(bOutTransferLocal ? pLocSub : pRemSub).first,
                                   dutOutCeCount);
        pAvailCes->at(bOutTransferLocal ? pLocSub : pRemSub).first += dutOutCeCount;

        if ((testMapEntry.second.pDutInTestData->GetTransferHw() == LwLinkImpl::HT_NONE) &&
            (testMapEntry.second.pDutOutTestData->GetTransferHw() == LwLinkImpl::HT_NONE))
        {
            Printf(Tee::PriError,
                   "%s : No copy engines assigned on route :\n%s\n",
                   __FUNCTION__, pTestRouteCe2d->GetString("    ").c_str());
            return RC::SOFTWARE_ERROR;
        }
    }
    return rc;
}

//------------------------------------------------------------------------------
RC TestModeCe2d::AssignP2PCes(map<GpuSubdevice *, UINT32> *pAssignedCeMasks)
{
    RC rc;

    for (auto & testMapEntry : GetTestData())
    {
        TestRouteCe2d *pTestRouteCe2d;
        pTestRouteCe2d = static_cast<TestRouteCe2d *>(testMapEntry.first.get());

        if (pTestRouteCe2d->GetTransferType() != TT_P2P)
            continue;

        P2PAllocMode p2pAlloc   = m_P2PAllocMode;
        UINT32 dutInLocCeMask  = 0;
        UINT32 dutOutLocCeMask = 0;
        UINT32 dutInRemCeMask  = 0;
        UINT32 dutOutRemCeMask = 0;

        CHECK_RC(pTestRouteCe2d->GetUsableCeMasks(true,
                                                  &dutInLocCeMask,
                                                  &dutOutLocCeMask));
        CHECK_RC(pTestRouteCe2d->GetUsableCeMasks(false,
                                                  &dutInRemCeMask,
                                                  &dutOutRemCeMask));

        GpuSubdevice *pLocSub = pTestRouteCe2d->GetLocalSubdevice();
        GpuSubdevice *pRemSub = pTestRouteCe2d->GetRemoteSubdevice();

        dutInLocCeMask  &= ~pAssignedCeMasks->at(pLocSub);
        dutOutLocCeMask &= ~pAssignedCeMasks->at(pLocSub);
        dutInRemCeMask  &= ~pAssignedCeMasks->at(pRemSub);
        dutOutRemCeMask &= ~pAssignedCeMasks->at(pRemSub);

        bool   bCeNotFound = false;
        if (p2pAlloc == P2P_ALLOC_DEFAULT)
        {
            // Assign Copy Engines for P2P transfer based on the following priority:
            //
            //    1. For unidirectional traffic prefer to allocate CE on the DUT
            //    2. For bidirectional traffic preference is in the following order
            //           a. Bidirectional writes
            //           b. Bidirectional reads
            //           c. Read/Write from DUT
            //           d. Read/Write from Remote device
            if (GetTransferDir() == TD_IN_ONLY)
            {
                if (dutInLocCeMask)
                    p2pAlloc = P2P_ALLOC_IN_LOCAL;
                else if (dutInRemCeMask)
                    p2pAlloc = P2P_ALLOC_OUT_LOCAL;  // Out local implies in remote
                else
                    bCeNotFound = true;
            }
            else if (GetTransferDir() == TD_OUT_ONLY)
            {
                if (dutOutLocCeMask)
                    p2pAlloc = P2P_ALLOC_OUT_LOCAL;
                else if (dutOutRemCeMask)
                    p2pAlloc = P2P_ALLOC_IN_LOCAL;  // In local implies out remote
                else
                    bCeNotFound = true;
            }
            else
            {
                // Bidirectional writes - local CE generating output traffic and
                // remote CE generating input traffic
                if (dutOutLocCeMask && dutInRemCeMask)
                {
                    p2pAlloc = P2P_ALLOC_OUT_LOCAL;
                }
                // Bidirectional reads - local CE generating input traffic and
                // remote CE generating output traffic
                else if (dutInLocCeMask && dutOutRemCeMask)
                {
                    p2pAlloc = P2P_ALLOC_IN_LOCAL;
                }
                // Read/Write - local CEs generating both input and output traffic
                else if (dutInLocCeMask && dutOutLocCeMask)
                {
                    p2pAlloc = P2P_ALLOC_ALL_LOCAL;
                }
                // Read/Write - remote CEs generating both input and output traffic
                else if (dutOutRemCeMask && dutInRemCeMask)
                {
                    p2pAlloc = P2P_ALLOC_ALL_REMOTE;
                }
                else
                {
                    bCeNotFound = true;
                }
            }
        }

        bool   bInTransferLocal    = false;
        bool   bOutTransferLocal   = false;
        UINT32 inCeMask  = 0;
        UINT32 outCeMask = 0;
        if (!bCeNotFound)
        {
            switch (p2pAlloc)
            {
                case P2P_ALLOC_ALL_LOCAL:
                    bInTransferLocal  = true;
                    bOutTransferLocal = true;
                    inCeMask  = dutInLocCeMask;
                    outCeMask = dutOutLocCeMask;
                    break;
                case P2P_ALLOC_OUT_LOCAL:
                    bInTransferLocal  = false;
                    bOutTransferLocal = true;
                    inCeMask  = dutInRemCeMask;
                    outCeMask = dutOutLocCeMask;
                    break;
                case P2P_ALLOC_IN_LOCAL:
                    bInTransferLocal  = true;
                    bOutTransferLocal = false;
                    inCeMask          = dutInLocCeMask;
                    outCeMask         = dutOutRemCeMask;
                    break;
                case P2P_ALLOC_ALL_REMOTE:
                    bInTransferLocal  = false;
                    bOutTransferLocal = false;
                    inCeMask          = dutInRemCeMask;
                    outCeMask         = dutOutRemCeMask;
                    break;
                default:
                    Printf(Tee::PriError, "%s : Unknown P2P allocation mode %d on route :\n%s\n",
                           __FUNCTION__, p2pAlloc, pTestRouteCe2d->GetString("    ").c_str());
                    return RC::SOFTWARE_ERROR;
            }
        }

        UINT32 inputCe   = LW2080_ENGINE_TYPE_COPY_SIZE;
        UINT32 outputCe  = LW2080_ENGINE_TYPE_COPY_SIZE;
        if (GetTransferDir() == TD_IN_ONLY)
        {
            bCeNotFound = (inCeMask == 0);
            if (!bCeNotFound)
                inputCe = static_cast<UINT32>(Utility::BitScanForward(inCeMask));
        }
        else if (GetTransferDir() == TD_OUT_ONLY)
        {
            bCeNotFound = (outCeMask == 0);
            if (!bCeNotFound)
                outputCe = static_cast<UINT32>(Utility::BitScanForward(outCeMask));
        }
        else
        {
            if (bInTransferLocal == bOutTransferLocal)
                bCeNotFound = Utility::CountBits(inCeMask | outCeMask) < 2;
            else
                bCeNotFound = !inCeMask || !outCeMask;
            if (!bCeNotFound)
            {
                inputCe = static_cast<UINT32>(Utility::BitScanForward(inCeMask));
                if (bInTransferLocal == bOutTransferLocal)
                    outCeMask &= ~(1 << inputCe);
                outputCe = static_cast<UINT32>(Utility::BitScanForward(outCeMask));
            }

        }

        // Ensure a valid configuration
        if (bCeNotFound)
        {
            Printf(GetPrintPri(),
                   "%s : Copy engine assignment failed on route :\n%s\n",
                   __FUNCTION__, pTestRouteCe2d->GetString("    ").c_str());
            return RC::UNSUPPORTED_SYSTEM_CONFIG;
        }

        CHECK_RC(AssignCeToTestDir(testMapEntry.second.pDutInTestData,
                                   bInTransferLocal ? pLocSub : pRemSub,
                                   bInTransferLocal,
                                   inputCe));
        pAssignedCeMasks->at(bInTransferLocal ? pLocSub : pRemSub) |= (1U << inputCe);

        CHECK_RC(AssignCeToTestDir(testMapEntry.second.pDutOutTestData,
                                   bOutTransferLocal ? pLocSub : pRemSub,
                                   bOutTransferLocal,
                                   outputCe));
        pAssignedCeMasks->at(bOutTransferLocal ? pLocSub : pRemSub) |= (1U << outputCe);

        if ((testMapEntry.second.pDutInTestData->GetTransferHw() == LwLinkImpl::HT_NONE) &&
            (testMapEntry.second.pDutOutTestData->GetTransferHw() == LwLinkImpl::HT_NONE))
        {
            Printf(Tee::PriError,
                   "%s : No copy engines assigned on route :\n%s\n",
                   __FUNCTION__, pTestRouteCe2d->GetString("    ").c_str());
            return RC::SOFTWARE_ERROR;
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
void TestModeCe2d::AutoAdjustCopyLoopcount(TestRouteCe2d *pTrCe2d, TestDirectionDataCe2d *pTdCe2d)
{
    if (!pTdCe2d->IsInUse())
        return;

    const UINT64 minBytesPerCopy =
        max(1ULL, GetMinimumBytesPerCopy(pTrCe2d->GetMaxBandwidthKiBps()));
    const UINT32 copyLoopCount =
        static_cast<UINT32>(CEIL_DIV(minBytesPerCopy, pTdCe2d->GetBytesPerLoop()));
    if (copyLoopCount > GetCopyLoopCount())
    {
        SetCopyLoopCount(copyLoopCount);
        Printf(GetPrintPri(),
               "%s : Auto adjusted CopyLoopCount to %u in order to meet bandwidth on route\n"
               "%s\n",
               __FUNCTION__,
               copyLoopCount,
               pTrCe2d->GetString("    ").c_str());
    }
}

//------------------------------------------------------------------------------
//! \brief Verify the destination surface for the specific test
//! route/direction
//!
//! \param pRoute   : Pointer to the test route
//! \param pTestDir : Pointer to the test direction
//!
//! \return OK if verification was successful, not OK otherwise
RC TestModeCe2d::CheckDestinationSurface
(
    TestRoutePtr pRoute,
    TestDirectionDataPtr pTestDir
)
{
    if (!pTestDir->IsInUse())
        return OK;

    Printf(GetPrintPri(),
           "%s : Checking %s destination surface of test route:\n"
           "%s\n",
           __FUNCTION__,
           pTestDir->IsInput() ? "input" : "output",
           pRoute->GetString("    ").c_str());

    string failStrBase = "Error : Mismatch at (%d, %d) on %s destination surface\n"
                         "        Got 0x%x, expected 0x%x\n";
    if (pTestDir->GetTransferHw() == LwLinkImpl::HT_CE)
    {
        if (pTestDir->IsHwLocal())
        {
            failStrBase += "        Local Ce      : %u\n";
        }
        else
        {
            string remDevStr = pRoute->GetRemoteDevString();
            failStrBase += Utility::StrPrintf("        Remote Device : %s\n",
                                              remDevStr.c_str());
            failStrBase += "        Remote Ce     : %u\n";
        }
    }
    failStrBase += Utility::StrPrintf("        Route         :\n%s\n\n",
                                      pRoute->GetString("          ").c_str());

    TestDirectionDataCe2d * pTdCe2d;
    pTdCe2d =  static_cast<TestDirectionDataCe2d *>(pTestDir.get());

    return pTdCe2d->CheckDestinationSurface(GetTestConfig(),
                                            m_PixelComponentMask,
                                            GetNumErrorsToPrint(),
                                            failStrBase,
                                            m_LwCrcSize,
                                            &m_LwGoldCrcData,
                                            &m_LwLwrCrcData);
}

//------------------------------------------------------------------------------
/* virtual */ TestMode::TestDirectionDataPtr TestModeCe2d::CreateTestDirectionData()
{
    auto tdData = make_unique<TestDirectionDataCe2d>(GetAllocMgr());
    tdData->SetAddCpuTraffic(GetAddCpuTraffic());
    tdData->SetSysmemLoopback(GetSysmemLoopback());
    tdData->SetNumCpuTrafficThreads(GetNumCpuTrafficThreads());
    tdData->SetTestFullFabric(m_TestFullFabric);
    tdData->SetUseLwdaCrc(m_UseLwdaCrc);
    return TestDirectionDataPtr(move(tdData));
}

//------------------------------------------------------------------------------
string TestModeCe2d::GetBandwidthFailStringImpl
(
    TestRoutePtr pRoute,
    TestMode::TestDirectionDataPtr pTestDir
)
{
    TestDirectionDataCe2d *pCe2d = static_cast<TestDirectionDataCe2d *>(pTestDir.get());
    if (pCe2d->IsInUse() &&
        (pCe2d->GetTransferHw() == LwLinkImpl::HT_CE) &&
        (pCe2d->GetTotalPces() < pRoute->GetWidth()))
    {
        return Utility::StrPrintf("\n*******    Bandwidth failure may occur if fewer PCEs are assigned than    *******\n" //$
                                  "******* Links ilwolved in the connection, check the LCE/PCE configuration *******\n"   //$
                                  "*******                      Links : %2u    PCEs : %2u                      *******",      //$
                                  pRoute->GetWidth(), pCe2d->GetTotalPces());
    }
    return "";
}

//------------------------------------------------------------------------------
/* virtual */ UINT64 TestModeCe2d::GetMinimumBytesPerCopy(UINT32 maxBwKiBps)
{
    // Given the experimentally determined rampup for each subsequent CE as
    // 1.5us and an also experimentally determined 30x requirement on transfer
    // size for the rampup time to not dominate the bandwidth callwlation, determine
    // the mimimum number of bytes per copy
    //
    // Be conservative here and use a channel rampup time of 1.5us
    const FLOAT64 CHANNEL_RAMPUP_TIME_S  = 0.0000015;
    const FLOAT64 MIN_RAMPUP_FACTOR = 30.0;
    return static_cast<UINT64>(maxBwKiBps * 1024.0 *
                               (m_TotalCopies - 1) * MIN_RAMPUP_FACTOR *
                               CHANNEL_RAMPUP_TIME_S);
}

//------------------------------------------------------------------------------
//! \brief Setup the copies for the given test direction (i.e. write the
//! necessary methods to the pushbuffer for performing all the copies in order
//! to saturate the links).
//!
//! Each test direction has a source/destination pair of surfaces as well as a
//! list of copies that will be performed.  There is one copy for each copy
//! engine assigned to the (route, direction) tuple.  The source and
//! destination surfaces are divided into a number of regions equal to the
//! number of copy engines.  Each region is further subdivided into 2
//! sub-regions and the copies switch their source for the sub-region each time
//! a copy is performed to keep the data changing:
//!
//!            SRC                                           DST
//!    __________________                            __________________
//!   | Region 0         |                          | Region 0         |
//!   |   Subregion 0    |                          |   Subregion 0    |
//!   |    ----------    |                          |    ----------    |
//!   |   Subregion 1    |                          |   Subregion 1    |
//!   |------------------|                          |------------------|
//!   | Region 1         |                          | Region 1         |
//!   |   Subregion 0    |                          |   Subregion 0    |
//!   |    ----------    |                          |    ----------    |
//!   |   Subregion 1    |                          |   Subregion 1    |
//!   |------------------|                          |------------------|
//!   |         .        |                          |         .        |
//!   |         .        |                          |         .        |
//!   |         .        |                          |         .        |
//!   |------------------|                          |------------------|
//!   | Region N         |                          | Region N         |
//!   |   Subregion 0    |                          |   Subregion 0    |
//!   |    ----------    |                          |    ----------    |
//!   |   Subregion 1    |                          |   Subregion 1    |
//!   |__________________|                          |__________________|
//!
//! For example : The first time data is copied, CE0 copies source region (0, 0)
//! to destination region (0, 0) and source region (0, 1) to destination region
//! (0, 1).  The second time data is copied, CE0 copies source region (0, 1) to
//! destination region (0, 0)  and source region (0, 0) to destination region
//! (0, 1)
//!
//! \param pTestDir : Pointer to test direction to setup copies for
//!
//! \return OK if successful, not OK if setting up the channels failed
RC TestModeCe2d::SetupCopies(TestDirectionDataPtr pTestDir)
{
    if (!pTestDir->IsInUse())
        return OK;

    TestDirectionDataCe2d * pTdCe2d;
    pTdCe2d =  static_cast<TestDirectionDataCe2d *>(pTestDir.get());

    // Zero out the global start semaphore
    MEM_WR32(m_CopyTrigSem.pAddress, 0);
    Platform::FlushCpuWriteCombineBuffer();

    return pTdCe2d->SetupCopies(GetCopyLoopCount(),
                                m_bCopyByLine,
                                m_PixelComponentMask);
}

//------------------------------------------------------------------------------
/* virtual */ RC TestModeCe2d::TriggerAllCopies()
{
    MEM_WR32(m_CopyTrigSem.pAddress, COPY_START_SIGNAL);
    return OK;
}

//------------------------------------------------------------------------------
RC TestModeCe2d::WaitForCopies(FLOAT64 timeoutMs)
{
    return POLLWRAP_HW(PollCopiesComplete, this, timeoutMs);
}

//------------------------------------------------------------------------------
//! \brief Wait for all copies to complete.
//!
//! \param pvTestMode : Pointer to the test mode
//!
//! \return true if all copies are complete, false otherwise
/* static */ bool TestModeCe2d::PollCopiesComplete(void *pvTestMode)
{
    TestModeCe2d *pTestMode = static_cast<TestModeCe2d *>(pvTestMode);

    if (pTestMode->GetShowCopyProgress())
    {
        static const UINT32 progressTimeTicks =
            Xp::QueryPerformanceFrequency() * PROGRESS_INDICATOR_SEC;
        UINT64 lwrTick = Xp::QueryPerformanceCounter();
        if ((lwrTick - pTestMode->GetProgressPrintTick()) > progressTimeTicks)
        {
            Printf(Tee::PriNormal, ".");
            pTestMode->SetProgressPrintTick(lwrTick);
        }
    }

    // Read all semaphores, as soon as one fails, return false
    for (auto const & lwrRteTestData : pTestMode->GetTestData())
    {
        TestDirectionDataCe2d *pTdCe2d;
        pTdCe2d =
            static_cast<TestDirectionDataCe2d *>(lwrRteTestData.second.pDutInTestData.get());
        if (!pTdCe2d->AreCopiesComplete())
            return false;
        pTdCe2d =
            static_cast<TestDirectionDataCe2d *>(lwrRteTestData.second.pDutOutTestData.get());
        if (!pTdCe2d->AreCopiesComplete())
            return false;
    }
    return true;
}

//------------------------------------------------------------------------------
// TestDirectionDataCe2d public implementation
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
TestModeCe2d::TestDirectionDataCe2d::TestDirectionDataCe2d
(
    AllocMgrPtr pAllocMgr
) : TestDirectionData(pAllocMgr)
{
    m_pAllocMgr = static_cast<AllocMgrCe2d *>(pAllocMgr.get());
}

//------------------------------------------------------------------------------
RC TestModeCe2d::TestDirectionDataCe2d::Acquire
(
    Surface2D::Layout                   srcLayout
   ,TransferType                        tt
   ,const AllocMgrCe2d::SemaphoreData & copyTrigSem
   ,GpuDevice            *              pLocDev
   ,GpuDevice            *              pRemDev
   ,GpuTestConfiguration *              pTestConfig
   ,UINT32                              surfSizeFactor
)
{
    RC rc;

    if (!IsInUse())
        return OK;

    m_TimeoutMs = pTestConfig->TimeoutMs();

    const UINT32 surfaceWidth  = pTestConfig->SurfaceWidth();
    const UINT32 surfaceHeight = pTestConfig->SurfaceHeight() * surfSizeFactor;

    const AllocMgrCe2d::SurfaceType type = (GetTestFullFabric()) ? AllocMgrCe2d::ST_FULLFABRIC
                                                                 : AllocMgrCe2d::ST_NORMAL;

    // First acquire and map the surfaces as necessary
    switch (tt)
    {
        case TT_SYSMEM:
            {
                // DUT Input testing requires source surface in sysmem and
                // destination in FB, DUT Output testing is the reverse
                Memory::Location srcLoc = Memory::NonCoherent;
                Memory::Location dstLoc = Memory::NonCoherent;
                if (!GetSysmemLoopback())
                {
                    if (IsInput())
                        dstLoc = Memory::Fb;
                    else
                        srcLoc = Memory::Fb;
                }
                CHECK_RC(m_pAllocMgr->AcquireSurface(pLocDev
                                                     ,srcLoc
                                                     ,srcLayout
                                                     ,type
                                                     ,surfaceWidth
                                                     ,surfaceHeight
                                                     ,&m_pSrcSurf));
                CHECK_RC(m_pAllocMgr->AcquireSurface(pLocDev
                                                     ,dstLoc
                                                     ,Surface2D::Pitch
                                                     ,type
                                                     ,surfaceWidth
                                                     ,surfaceHeight
                                                     ,&m_pDstSurf));
            }
            break;
        case TT_LOOPBACK:
            // For loopback both surfaces are in FB
            CHECK_RC(m_pAllocMgr->AcquireSurface(pLocDev
                                                 ,Memory::Fb
                                                 ,srcLayout
                                                 ,type
                                                 ,surfaceWidth
                                                 ,surfaceHeight
                                                 ,&m_pSrcSurf));
            CHECK_RC(m_pAllocMgr->AcquireSurface(pLocDev
                                                 ,Memory::Fb
                                                 ,Surface2D::Pitch
                                                 ,type
                                                 ,surfaceWidth
                                                 ,surfaceHeight
                                                 ,&m_pDstSurf));

            break;
        case TT_P2P:
            // For P2P testing, Input testing requires source be acquired on
            // the remote device and destination be acquired on the DUT.
            // Output testing is the reverse
            CHECK_RC(m_pAllocMgr->AcquireSurface(IsInput() ? pRemDev : pLocDev
                                                 ,Memory::Fb
                                                 ,srcLayout
                                                 ,type
                                                 ,surfaceWidth
                                                 ,surfaceHeight
                                                 ,&m_pSrcSurf));
            CHECK_RC(m_pAllocMgr->AcquireSurface(IsInput() ? pLocDev : pRemDev
                                                 ,Memory::Fb
                                                 ,Surface2D::Pitch
                                                 ,type
                                                 ,surfaceWidth
                                                 ,surfaceHeight
                                                 ,&m_pDstSurf));

            break;
        default:
            Printf(Tee::PriError,
                   "%s : Unknown transfer type %d!\n",
                   __FUNCTION__, tt);
            return RC::SOFTWARE_ERROR;
    }

    GpuDevice *pHwDev = pRemDev;
    if (IsHwLocal())
        pHwDev = pLocDev;

    if (pHwDev == nullptr)
    {
        Printf(Tee::PriError,
               "%s : Cannot acquire transfer HW on non-existent remote device!\n",
               __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    CHECK_RC(InitializeCopyData(tt, pHwDev));

    AllocMgrCe2d::SemaphoreData semData;
    CHECK_RC(m_pAllocMgr->ShareSemaphore(copyTrigSem, pHwDev, &semData));
    m_CopyTrigSemCtxOffset = semData.ctxDmaOffsetGpu;

    SetTotalTimeNs(0ULL);
    SetTotalBytes(0ULL);

    for (size_t ii = 0; ii < m_Copies.size(); ii++)
    {
        CopyRegionData &copyData = m_Copies[ii];
        UINT32 ei;

        ei = (GetTransferHw() == LwLinkImpl::HT_CE) ? m_Copies[ii].ceId : 0;

        // Acquire a channel and associated transfer hw as well as start and
        // end semaphores used for timing the m_Copies
        CHECK_RC(m_pAllocMgr->AcquireChannel(pHwDev,
                                             GetTransferHw(),
                                             ei,
                                             &copyData.pCh,
                                             &copyData.hCh,
                                             &copyData.copyClass));
        CHECK_RC(m_pAllocMgr->AcquireSemaphore(pHwDev, &copyData.startSem));
        CHECK_RC(m_pAllocMgr->AcquireSemaphore(pHwDev, &copyData.endSem));

        // Bind all test surfaces to the channel
        CHECK_RC(m_pSrcSurf->BindGpuChannel(copyData.hCh));
        CHECK_RC(m_pDstSurf->BindGpuChannel(copyData.hCh));
        CHECK_RC(copyData.startSem.pSemSurf->BindGpuChannel(copyData.hCh));
        CHECK_RC(copyData.endSem.pSemSurf->BindGpuChannel(copyData.hCh));
        CHECK_RC(semData.pSemSurf->BindGpuChannel(copyData.hCh));

        // Setup the offset data for the subregions of the current copy
        copyData.startY[0] = m_CopyHeight * ii * 2;
        copyData.startY[1] = copyData.startY[0] + m_CopyHeight;
    }

    CHECK_RC(TestMode::TestDirectionData::Acquire(tt, pLocDev, pTestConfig));

    return rc;
}

//------------------------------------------------------------------------------
bool TestModeCe2d::TestDirectionDataCe2d::AreCopiesComplete() const
{
    for (auto const & lwrCopy : m_Copies)
    {
        if (MEM_RD32(lwrCopy.endSem.pAddress) != (lwrCopy.copyCount + 1))
            return false;
    }
    return true;
}

//------------------------------------------------------------------------------
RC TestModeCe2d::TestDirectionDataCe2d::CheckDestinationSurface
(
    GpuTestConfiguration *pTestConfig,
    UINT32 pixelComponentMask,
    UINT32 numErrorsToPrint,
    const string &failStrBase,
    const LwSurfRoutines::CrcSize &crcSize,
    LwSurfRoutines::LwSurf *pGoldCrcData, 
    LwSurfRoutines::LwSurf *pLwrCrcData 
)
{
    StickyRC rc;

    const bool bLwdaCrcDataPresent = pGoldCrcData->GetSize();
    LwSurfRoutines *pLwSurfRoutines = nullptr;
    DEFER()
    {
        if (pLwSurfRoutines)
        {
            m_pAllocMgr->ReleaseLwdaUtils(&pLwSurfRoutines);
        }
    };

    if (m_bUseLwdaCrc)
    {
        CHECK_RC(m_pAllocMgr->AcquireLwdaUtils(m_pDstSurf->GetGpuDev(), &pLwSurfRoutines));

        // If the LWCA CRC data is present then compute and compare using LWCA, only fall
        // through to the per-pixel compare if the comparison fails
        if (bLwdaCrcDataPresent)
        {
            // For crc data to be valid the surface routines must have been supported
            MASSERT(pLwSurfRoutines->IsSupported());

            CHECK_RC(pLwSurfRoutines->ComputeCrcs<UINT64>(*m_pDstSurf.get(), crcSize, pLwrCrcData));
            {
                const RC rc = pLwSurfRoutines->CompareBuffers<UINT64>(*pGoldCrcData, *pLwrCrcData);
                if (rc != RC::GOLDEN_VALUE_MISCOMPARE)
                    return rc;
            }
        }
    }

    // Start by assuming the surfaces are already in sysmem
    Surface2DPtr pGoldSurf = m_pSrcSurf;
    Surface2DPtr pTestSurf = m_pDstSurf;

    const AllocMgrCe2d::SurfaceType type = (GetTestFullFabric()) ? AllocMgrCe2d::ST_FULLFABRIC
                                                                 : AllocMgrCe2d::ST_NORMAL;

    // If either the source or destination surface is not in sysmem, or is in
    // sysmem but block linear (and would therefore require reflected access),
    // acquire a new sysmem surface to copy the data to for verification
    if ((m_pSrcSurf->GetLocation() == Memory::Fb) ||
        (m_pSrcSurf->GetLayout() == Surface2D::BlockLinear))
    {
        CHECK_RC(m_pAllocMgr->AcquireSurface(m_pSrcSurf->GetGpuDev(),
                                             Memory::NonCoherent,
                                             Surface2D::Pitch,
                                             type,
                                             m_pSrcSurf->GetWidth(),
                                             m_pSrcSurf->GetHeight(),
                                             &pGoldSurf));
    }
    Utility::CleanupOnReturnArg<AllocMgrCe2d,
                                RC,
                                Surface2DPtr>
        releaseGold(m_pAllocMgr, &AllocMgr::ReleaseSurface, pGoldSurf);

    if ((m_pDstSurf->GetLocation() == Memory::Fb) ||
        (m_pDstSurf->GetLayout() == Surface2D::BlockLinear))
    {
        CHECK_RC(m_pAllocMgr->AcquireSurface(m_pDstSurf->GetGpuDev(),
                                             Memory::NonCoherent,
                                             Surface2D::Pitch,
                                             type,
                                             m_pDstSurf->GetWidth(),
                                             m_pDstSurf->GetHeight(),
                                             &pTestSurf));
    }
    Utility::CleanupOnReturnArg<AllocMgrCe2d,
                                RC,
                                Surface2DPtr>
        releaseTest(m_pAllocMgr, &AllocMgr::ReleaseSurface, pTestSurf);

    // Copy the appropriate surfaces to sysmem
    if (m_pSrcSurf != pGoldSurf)
        CHECK_RC(CopyToSysmem(pTestConfig, m_pSrcSurf, pGoldSurf));
    else
        releaseGold.Cancel();
    if (m_pDstSurf != pTestSurf)
        CHECK_RC(CopyToSysmem(pTestConfig, m_pDstSurf, pTestSurf));
    else
        releaseTest.Cancel();

    CHECK_RC(pGoldSurf->Map());
    CHECK_RC(pTestSurf->Map());

    UINT32 errorCount = 0;

    UINT32 pixelMask = ~0U;
    if (pixelComponentMask != ALL_COMPONENTS)
    {
        if (!(pixelComponentMask & 0x1))
            pixelMask &= ~0xFF;
        if (!(pixelComponentMask & 0x2))
            pixelMask &= ~0xFF00;
        if (!(pixelComponentMask & 0x4))
            pixelMask &= ~0xFF0000;
        if (!(pixelComponentMask & 0x8))
            pixelMask &= ~0xFF000000;
    }

    {
        Tasker::DetachThread detachThread;

        // The full height of the surface isnt always copied.  Only check the portion
        // of the surface that the m_Copies affect
        MASSERT(m_CopyHeight);
        const UINT32 checkHeight = m_CopyHeight * m_Copies.size() * 2;
        for (UINT32 lwrY = 0; lwrY < checkHeight && (errorCount < numErrorsToPrint); lwrY++)
        {
            const size_t pixels = pGoldSurf->GetWidth();
            vector<UINT32> goldLine(pixels);
            vector<UINT32> testLine(pixels);

            UINT64 goldStartOffset = pGoldSurf->GetPixelOffset(0, lwrY);
            const UINT64 testStartOffset = pTestSurf->GetPixelOffset(0, lwrY);
            const UINT32 lwrCeId = m_Copies[(lwrY / (m_CopyHeight * 2))].ceId;

            // If bIlwertSrcDstRegions this means that the last actual copy that was
            // performed was an ilwerted copy so adjust the position to compare from
            // the source surface appropriately
            if (m_bIlwertSrcDstSubReg)
            {
                if ((lwrY % (m_CopyHeight * 2)) >= m_CopyHeight)
                    goldStartOffset = pTestSurf->GetPixelOffset(0, lwrY - m_CopyHeight);
                else
                    goldStartOffset = pTestSurf->GetPixelOffset(0, lwrY + m_CopyHeight);
            }

            // Compare the surface data line by line
            Platform::VirtualRd((UINT08*)pGoldSurf->GetAddress() + goldStartOffset,
                                &goldLine[0],
                                pixels*sizeof(UINT32));
            Platform::VirtualRd((UINT08*)pTestSurf->GetAddress() + testStartOffset,
                                &testLine[0],
                                pixels*sizeof(UINT32));

            for (UINT32 lwrX = 0; (lwrX < pixels) && (errorCount < numErrorsToPrint); lwrX++)
            {
                const UINT32 goldPixel = goldLine[lwrX] & pixelMask;
                const UINT32 testPixel = testLine[lwrX] & pixelMask;

                if (goldPixel != testPixel)
                {
                    if (GetTransferHw() == LwLinkImpl::HT_CE)
                    {
                        Printf(Tee::PriError,
                               failStrBase.c_str(),
                               lwrX, lwrY,
                               IsInput() ? "input" : "output",
                               testPixel,
                               goldPixel,
                               lwrCeId);
                    }
                    else
                    {
                        Printf(Tee::PriError,
                               failStrBase.c_str(),
                               lwrX, lwrY,
                               IsInput() ? "input" : "output",
                               testPixel,
                               goldPixel);
                    }
                    rc = RC::BUFFER_MISMATCH;
                    errorCount++;
                }
            }
        }
    }

    pGoldSurf->Unmap();
    pTestSurf->Unmap();

    // If the LWCA surface routines are possible and no LWCA CRC data was present
    // when the function started, then compute and save the LWCA based CRC which
    // will be used on subsequent checks and the CPU compare only used if the LWCA
    // compare fails
    if (m_bUseLwdaCrc && pLwSurfRoutines && pLwSurfRoutines->IsSupported() && !bLwdaCrcDataPresent)
    {
        CHECK_RC(pLwSurfRoutines->ComputeCrcs<UINT64>(*m_pDstSurf.get(), crcSize, pGoldCrcData));
    }
    return rc;
}

//------------------------------------------------------------------------------
/* virtual */ UINT64 TestModeCe2d::TestDirectionDataCe2d::GetBytesPerLoop() const
{
    return GetTransferWidth() * m_CopyHeight * 2 * m_Copies.size();
}

//------------------------------------------------------------------------------
//! \brief Get a string containing the Copy Engine(s) for the specified test
//! direction
//!
//! \param pTestDir : Test direction to get the CE string for
//!
//! \return String containing the Copy Engine(s))
string TestModeCe2d::TestDirectionDataCe2d::GetCeString() const
{
    if (!IsInUse() || (GetTransferHw() != LwLinkImpl::HT_CE))
        return "N/A";

    string ceStr = "";
    ceStr += IsHwLocal() ? "L(" : "R(";
    for (size_t ii = 0; ii < m_Copies.size(); ii++)
    {
        ceStr += Utility::StrPrintf("%u", m_Copies[ii].ceId);
        if (m_Copies[ii].pceMask != 0)
        {
            ceStr += "[";
            INT32 lwrBit = Utility::BitScanForward(m_Copies[ii].pceMask);
            while (lwrBit != -1)
            {
                ceStr += Utility::StrPrintf("%u", static_cast<UINT32>(lwrBit));
                lwrBit = Utility::BitScanForward(m_Copies[ii].pceMask, lwrBit + 1);
                if (lwrBit != -1)
                    ceStr += ",";
            }
            ceStr += "]";
        }
        if (ii != (m_Copies.size() - 1))
            ceStr += ",";
    }
    ceStr += ")";
    return ceStr;
}

//------------------------------------------------------------------------------
UINT32 TestModeCe2d::TestDirectionDataCe2d::GetTotalPces()
{
    if (!IsInUse() || (GetTransferHw() != LwLinkImpl::HT_CE))
        return 0;

    UINT32 totalPces = 0;
    for (auto const & lwrCopy : m_Copies)
    {
        totalPces += Utility::CountBits(lwrCopy.pceMask);
    }
    return totalPces;
}

//------------------------------------------------------------------------------
/* virtual */ UINT32 TestModeCe2d::TestDirectionDataCe2d::GetTransferWidth() const
{
    return m_pSrcSurf->GetWidth() * m_pSrcSurf->GetBytesPerPixel();
}

//------------------------------------------------------------------------------
/* virtual */ bool TestModeCe2d::TestDirectionDataCe2d::IsInUse() const
{
    return TestDirectionData::IsInUse() || !m_Copies.empty();
}

//------------------------------------------------------------------------------
/* virtual */ RC TestModeCe2d::TestDirectionDataCe2d::Release
(
    bool *pbResourcesReleased
)
{
    if (!IsInUse())
        return OK;

    StickyRC rc;
    if (m_pSrcSurf.get() != nullptr)
    {
        rc = m_pAllocMgr->ReleaseSurface(m_pSrcSurf);
        m_pSrcSurf.reset();
        *pbResourcesReleased = true;
    }
    if (m_pDstSurf.get() != nullptr)
    {
        rc = m_pAllocMgr->ReleaseSurface(m_pDstSurf);
        m_pDstSurf.reset();
        *pbResourcesReleased = true;
    }

    for (auto & crData : m_Copies)
    {
        if (nullptr != crData.pCh)
            rc = m_pAllocMgr->ReleaseChannel(crData.pCh);
        if (nullptr != crData.startSem.pAddress)
            rc = m_pAllocMgr->ReleaseSemaphore(crData.startSem.pAddress);
        if (nullptr != crData.endSem.pAddress)
            rc = m_pAllocMgr->ReleaseSemaphore(crData.endSem.pAddress);

        if ((nullptr != crData.pCh) ||
            (nullptr != crData.startSem.pAddress) ||
            (nullptr != crData.endSem.pAddress))
        {

            *pbResourcesReleased = true;
        }

        crData.pCh = nullptr;
        crData.startSem = AllocMgrCe2d::SemaphoreData();
        crData.endSem = AllocMgrCe2d::SemaphoreData();
    }

    rc = TestMode::TestDirectionData::Release(pbResourcesReleased);

    return rc;
}

//------------------------------------------------------------------------------
/* virtual */ void TestModeCe2d::TestDirectionDataCe2d::Reset()
{
    TestDirectionData::Reset();
    m_Copies.clear();
}

//------------------------------------------------------------------------------
RC TestModeCe2d::TestDirectionDataCe2d::SetupCopies
(
    UINT32 numLoops
   ,bool bCopyByLine
   ,UINT32 pixelComponentMask
)
{
    RC rc;
    bool bIlwertSrcDstSubReg = !m_bIlwertSrcDstSubReg;
    for (size_t ii = 0; ii < m_Copies.size(); ii++)
    {
        Channel *pCh = m_Copies[ii].pCh;

        // Allow copies to start closer together by triggering them all at once
        // through a CPU write.  Without syncronizing in this manner rampup
        // latencies between 2 channels is ~12us.  This number would scale based
        // on the number of channels launched so in a DGX 8 GPU system where
        // there are 24 (6 per gpu / 2 since we dont want to double count
        // physical wires) active links and TestAllGpus is used which would
        // launch 48 channel (bidirectional testing) this could
        // result in a (47 * 12us) = 564us rampup time from the first channel
        //  starting to the last
        //
        // Using a semaphore acquire to launch channels reduces the rampup time
        // between 2 channels to ~1.3us.
        //
        // The Copies still need to be long enough to not have this latency
        // affect BW and a ballpark figure for that is that a transfer should
        // take at least 30x the rampup time (30 * (<num channels> - 1)).
        //
        // See GetMinimumBytesPerCopy
        //
        CHECK_RC(pCh->SetSemaphoreOffset(m_CopyTrigSemCtxOffset));
        CHECK_RC(pCh->SemaphoreAcquire(COPY_START_SIGNAL));

        // First we release a host semaphore with time.  This is so that the
        // copy may be aclwrately timed without multithreading issues affecting
        // the results
        pCh->SetSemaphoreReleaseFlags(Channel::FlagSemRelWithTime);
        CHECK_RC(pCh->SetSemaphoreOffset(m_Copies[ii].startSem.ctxDmaOffsetGpu));
        CHECK_RC(pCh->SemaphoreRelease(0));

        UINT32 startY0 = m_Copies[ii].startY[0];
        UINT32 startY1 = m_Copies[ii].startY[1];

        // Always start each copy of the copy loop with the same setting for
        // ilwert src/dest
        //
        // Ilwert the current setting so data is different than the last time
        bIlwertSrcDstSubReg = !m_bIlwertSrcDstSubReg;

        for (UINT32 i = 0; i < numLoops; i++)
        {
            // Copy the first half of the region ilwerting the source as necessary
            UINT64 srcY = startY0;
            if (bIlwertSrcDstSubReg)
                srcY = startY1;

            CHECK_RC(WriteCopyMethods(pCh,
                                      m_Copies[ii].copyClass,
                                      m_pSrcSurf,
                                      m_pDstSurf,
                                      m_SrcCtxDmaOffset,
                                      srcY,
                                      m_DstCtxDmaOffset,
                                      startY0,
                                      0,
                                      0,
                                      m_CopyHeight,
                                      0,
                                      bCopyByLine,
                                      pixelComponentMask));

            // Copy the second half of the region ilwerting the source as necessary
            if (bIlwertSrcDstSubReg)
                srcY = startY0;
            else
                srcY = startY1;

            Surface2DPtr pSemSurf;
            UINT64 semOffset64 = 0;
            UINT32 semPayload  = 0;
            if (i == (numLoops - 1))
            {
                pSemSurf    = m_Copies[ii].endSem.pSemSurf;
                semOffset64 = m_Copies[ii].endSem.ctxDmaOffsetGpu;
                semPayload  = m_Copies[ii].copyCount + 1;
            }

            CHECK_RC(WriteCopyMethods(pCh,
                                      m_Copies[ii].copyClass,
                                      m_pSrcSurf,
                                      m_pDstSurf,
                                      m_SrcCtxDmaOffset,
                                      srcY,
                                      m_DstCtxDmaOffset,
                                      startY1,
                                      pSemSurf,
                                      semOffset64,
                                      m_CopyHeight,
                                      semPayload,
                                      bCopyByLine,
                                      pixelComponentMask));

            // On every loop but the last, ilwert the src/dest before doing the
            // copy so data is different than the last time.  Skip the last
            // loop so that when the function exits the state of the last
            // performed copy is indicated by bIlwertSrcDstSubReg
            if (i != (numLoops - 1))
            {
                bIlwertSrcDstSubReg = !bIlwertSrcDstSubReg;
            }
        }
    }

    // At the end of all m_Copies, update the test direction copy of ilwersion
    // Since all instances of the copy loop started with the same ilwersion
    // setting, all should end with the same ilwersion setting so it is only
    // necessary to update it on exit
    m_bIlwertSrcDstSubReg = bIlwertSrcDstSubReg;

    return OK;
}

//------------------------------------------------------------------------------
//! \brief Setup the test surfaces for use in the particular test direction
//!
//! \param pTestDir : Pointer to the test direction
//!
//! \return OK if setup was successful, not OK otherwise
RC TestModeCe2d::TestDirectionDataCe2d::SetupTestSurfaces
(
    GpuTestConfiguration *pTestConfig,
    UINT32 surfPercent
)
{
    if (!IsInUse())
        return OK;

    RC rc;

    // Rebind the GpuDevice temporarily so that DmaWrapper will operate on the
    // correct GPU device
    GpuDevice *pOrigGpuDev = pTestConfig->GetGpuDevice();
    GpuDevice *pSrcGpuDev  = m_pSrcSurf->GetGpuDev();
    Utility::CleanupOnReturnArg<GpuTestConfiguration,
                                void,
                                GpuDevice *>
        restoreGpuDev(pTestConfig,
                      &GpuTestConfiguration::BindGpuDevice,
                      pOrigGpuDev);
    pTestConfig->BindGpuDevice(pSrcGpuDev);

    // Initialize source surfaces in a striped pattern.  All sub-region 0 areas
    // will be initialized with data from GoldSurfA all sub-region 1 areas will
    // be initialized with data from GoldSurfB
    Surface2DPtr pGoldSurfA = m_pAllocMgr->GetGoldASurface();
    Surface2DPtr pGoldSurfB = m_pAllocMgr->GetGoldBSurface();

    UINT64 goldACtxDmaOffset = pGoldSurfA->GetCtxDmaOffsetGpu();
    if (pGoldSurfA->GetGpuDev() != pSrcGpuDev)
    {
        if (!pGoldSurfA->IsMapShared(pSrcGpuDev))
        {
            CHECK_RC(pGoldSurfA->MapShared(pSrcGpuDev));
        }
        goldACtxDmaOffset = pGoldSurfA->GetCtxDmaOffsetGpuShared(pSrcGpuDev);
    }

    UINT64 goldBCtxDmaOffset = pGoldSurfB->GetCtxDmaOffsetGpu();
    if (pGoldSurfB->GetGpuDev() != pSrcGpuDev)
    {
        if (!pGoldSurfB->IsMapShared(pSrcGpuDev))
        {
            CHECK_RC(pGoldSurfB->MapShared(pSrcGpuDev));
        }
        goldBCtxDmaOffset = pGoldSurfB->GetCtxDmaOffsetGpuShared(pSrcGpuDev);
    }
    
    const UINT32 surfAHeight = m_CopyHeight * ((FLOAT32)surfPercent / 100.0f);
    const UINT32 surfWidth = m_pSrcSurf->GetWidth();

    if (GetUseDmaInit())
    {
        // Use appropriate engine to ensure that we do not conflict with the actual
        // engines that may be used for test transfers
        DmaWrapper dmaWrap;
        CHECK_RC(dmaWrap.Initialize(pTestConfig,
                                    Memory::NonCoherent,
                                    (GetTransferHw() == LwLinkImpl::HT_CE) ?
                                        DmaWrapper::TWOD : DmaWrapper::COPY_ENGINE));
    
        Utility::CleanupOnReturn<DmaWrapper, RC>
            dmaCleanup(&dmaWrap, &DmaWrapper::Cleanup);
    
        for (size_t ii = 0; ii < m_Copies.size(); ii++)
        {
            for (size_t jj = 0; jj < 2; jj++)
            {
                UINT32 startY = m_Copies[ii].startY[jj];
                UINT32 copyHeight = surfAHeight;
                if (copyHeight != 0)
                {
                    dmaWrap.SetSurfaces(pGoldSurfA.get(), m_pSrcSurf.get());
                    dmaWrap.SetCtxDmaOffsets(goldACtxDmaOffset,
                                            m_pSrcSurf->GetCtxDmaOffsetGpu());
                    CHECK_RC(dmaWrap.Copy(0,                        // Starting src X, in bytes not pixels.
                                        jj*copyHeight,            // Starting src Y, in lines.
                                        pGoldSurfA->GetPitch(),   // Width of copied rect, in bytes.
                                        copyHeight,               // Height of copied rect, in bytes
                                        0,                        // Starting dst X, in bytes not pixels.
                                        startY,                   // Starting dst Y, in lines.
                                        pTestConfig->TimeoutMs(),
                                        0,
                                        true));
                }
    
                startY += copyHeight;
                copyHeight = m_CopyHeight - copyHeight;
                if (copyHeight != 0)
                {
                    dmaWrap.SetSurfaces(pGoldSurfB.get(), m_pSrcSurf.get());
                    dmaWrap.SetCtxDmaOffsets(goldBCtxDmaOffset,
                                            m_pSrcSurf->GetCtxDmaOffsetGpu());
                    CHECK_RC(dmaWrap.Copy(0,                        // Starting src X, in bytes not pixels.
                                        jj*copyHeight,            // Starting src Y, in lines.
                                        pGoldSurfB->GetPitch(),   // Width of copied rect, in bytes.
                                        copyHeight,               // Height of copied rect, in bytes
                                        0,                        // Starting dst X, in bytes not pixels.
                                        startY,                   // Starting dst Y, in lines.
                                        pTestConfig->TimeoutMs(),
                                        0,
                                        true));
                }
            }
        }
    
        // Fill surface uses dmawrapper as well, need to cleanup the existing one in
        // order to avoid resource conflicts
        CHECK_RC(dmaWrap.Cleanup());
        dmaCleanup.Cancel();
    }
    else // !UseDmaInit
    {
        CHECK_RC(m_pSrcSurf->Map());
        CHECK_RC(pGoldSurfA->Map());
        CHECK_RC(pGoldSurfB->Map());
        
        vector<UINT32> goldLine(surfWidth);
        vector<UINT32> srcLine(surfWidth);
        
        for (size_t ii = 0; ii < m_Copies.size(); ii++)
        {
            for (size_t jj = 0; jj < 2; jj++)
            {
                UINT32 startY = m_Copies[ii].startY[jj];
                UINT32 copyHeight = surfAHeight;
                if (copyHeight != 0)
                {
                    for (size_t lwrY = 0; lwrY < copyHeight; lwrY++)
                    {
                        const UINT64 goldStartOffset = pGoldSurfA->GetPixelOffset(0, lwrY);
                        const UINT64 srcStartOffset = m_pSrcSurf->GetPixelOffset(0, lwrY + startY);

                        // Copy the surface data line by line
                        Platform::VirtualRd(static_cast<UINT08*>(pGoldSurfA->GetAddress()) + goldStartOffset,
                                            &goldLine[0],
                                            surfWidth*sizeof(UINT32));
                        Platform::VirtualWr(static_cast<UINT08*>(m_pSrcSurf->GetAddress()) + srcStartOffset,
                                            &srcLine[0],
                                            surfWidth*sizeof(UINT32));
                    }
                }
    
                startY += copyHeight;
                copyHeight = m_CopyHeight - copyHeight;
                if (copyHeight != 0)
                {
                    for (size_t lwrY = 0; lwrY < copyHeight; lwrY++)
                    {
                        const UINT64 goldStartOffset = pGoldSurfB->GetPixelOffset(0, lwrY);
                        const UINT64 srcStartOffset = m_pSrcSurf->GetPixelOffset(0, lwrY + startY);

                        // Copy the surface data line by line
                        Platform::VirtualRd(static_cast<UINT08*>(pGoldSurfB->GetAddress()) + goldStartOffset,
                                            &goldLine[0],
                                            surfWidth*sizeof(UINT32));
                        Platform::VirtualWr(static_cast<UINT08*>(m_pSrcSurf->GetAddress()) + srcStartOffset,
                                            &srcLine[0],
                                            surfWidth*sizeof(UINT32));
                    }
                }
            }
        }
        
        m_pSrcSurf->Unmap();
        pGoldSurfA->Unmap();
        pGoldSurfB->Unmap();
    }

    // Clear the destination surface
    CHECK_RC(m_pAllocMgr->FillSurface(m_pDstSurf,
                                      AllocMgr::PT_SOLID,
                                      0, 0,
                                      (GetTransferHw() == LwLinkImpl::HT_CE)));

    CHECK_RC(TestMode::TestDirectionData::SetupTestSurfaces(pTestConfig));

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Start all copies on a test direction (i.e. flush all channels).
//!
//! \param pTestDir    : Pointer to test direction to flush channels on
//!
//! \return OK if successful, not OK if flushes failed
RC TestModeCe2d::TestDirectionDataCe2d::StartCopies()
{
    RC rc;

    if (!IsInUse())
        return OK;

    CHECK_RC(TestMode::TestDirectionData::StartCopies());
    for (auto & lwrCopy : m_Copies)
    {
        CHECK_RC(lwrCopy.pCh->Flush());
    }

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Update copy statistics for the specified test direction
//!
//! \param pTestDir : Pointer to the test direction
//!
RC TestModeCe2d::TestDirectionDataCe2d::UpdateCopyStats(UINT32 numLoops)
{
    if (!IsInUse())
        return OK;

    // Increment the total bytes written so far for this direction
    AddCopyBytes(GetBytesPerLoop() * numLoops);

    // Read the start/end GPU times for each of the m_Copies and save off the
    // time segments
    vector< pair<UINT64, UINT64> > timeSegments;
    for (auto & copyData : m_Copies)
    {
        UINT08 *pSemStart = static_cast<UINT08 *>(copyData.startSem.pAddress);
        UINT08 *pSemEnd = static_cast<UINT08 *>(copyData.endSem.pAddress);
        const UINT64 startNs = MEM_RD64(static_cast<UINT08 *>(pSemStart) + 8);
        const UINT64 endNs = MEM_RD64(static_cast<UINT08 *>(pSemEnd) + 8);

        MASSERT(endNs > startNs);

        timeSegments.push_back(pair<UINT64, UINT64>(startNs, endNs));

        // Increment the copy count (controls semaphore release values)
        copyData.copyCount++;
    }

    // Collapse overlapping segments (which would indicate parralellism) into
    // a list of non-overlapping start/end pairs.  Theoretically this should
    // always be a single start/end pair since all m_Copies for a particular
    // route/direction should start/finish at roughly the same time
    vector< pair<UINT64, UINT64> >::iterator pLwrSeg = timeSegments.begin();
    vector< pair<UINT64, UINT64> >::iterator pCheckSeg = pLwrSeg + 1;
    while ((pLwrSeg != timeSegments.end()) && (pCheckSeg != timeSegments.end()))
    {
        // If the current segment being checked does not overlap with the
        // checked segment, then move to the next segment to check it against
        if ((pLwrSeg->first > pCheckSeg->second) ||
            (pLwrSeg->second < pCheckSeg->first))
        {
            pCheckSeg++;

            // If the checked segment has reached the end then the current
            // segment has not overlapped with any other segments in the list
            // so shift the lwrernt segment and start checking again
            if (pCheckSeg == timeSegments.end())
            {
                pLwrSeg++;
                pCheckSeg = pLwrSeg + 1;
            }
        }
        else
        {
            // If the current segment overlaps, then update the checked segment
            // as appropriate and remove the current segment from the list
            if (pLwrSeg->first < pCheckSeg->first)
                pCheckSeg->first = pLwrSeg->first;
            if (pLwrSeg->second > pCheckSeg->second)
                pCheckSeg->second = pLwrSeg->second;
            timeSegments.erase(pLwrSeg);

            // Reset the current/checked segment and start over
            pLwrSeg = timeSegments.begin();
            pCheckSeg = pLwrSeg + 1;
        }
    }

    // Theoretically there will be only a single segment here, but update the
    // total time as necessary
    for (size_t ii = 0; ii < timeSegments.size(); ii++)
    {
        AddTimeNs(timeSegments[ii].second - timeSegments[ii].first);
    }
    return OK;
}

//------------------------------------------------------------------------------
RC TestModeCe2d::TestDirectionDataCe2d::WritePngs(string fnameBase) const
{
    RC rc;
    string fname;

    fname = fnameBase + "_src.png";
    CHECK_RC(m_pSrcSurf->WritePng(fname.c_str()));
    fname = fnameBase + "_dst.png";
    CHECK_RC(m_pDstSurf->WritePng(fname.c_str()));
    return rc;
}

//------------------------------------------------------------------------------
// TestDirectionDataCe2d private implementation
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//! \brief Copy the specified surface to system memory
//!
//! \param pSrcSurf : Pointer to the source surface
//! \param pSysSurf : Pointer to the system memory surface
//!
//! \return OK if verification was successful, not OK otherwise
RC TestModeCe2d::TestDirectionDataCe2d::CopyToSysmem
(
    GpuTestConfiguration *pTestConfig,
    Surface2DPtr pSrcSurf,
    Surface2DPtr pSysSurf
)
{
    RC rc;

    if (GetUseDmaInit())
    {
        // Rebind the GpuDevice temporarily so that DmaWrapper will operate on the
        // correct GPU device
        GpuDevice *pOrigGpuDev = pTestConfig->GetGpuDevice();
        Utility::CleanupOnReturnArg<GpuTestConfiguration,
                                    void,
                                    GpuDevice *>
            restoreGpuDev(pTestConfig,
                        &GpuTestConfiguration::BindGpuDevice,
                        pOrigGpuDev);
        pTestConfig->BindGpuDevice(pSrcSurf->GetGpuDev());
    
        UINT64 sysCtxDmaOffset = pSysSurf->GetCtxDmaOffsetGpu();
        if (pSysSurf->GetGpuDev() != pSrcSurf->GetGpuDev())
        {
            if (!pSysSurf->IsMapShared(pSrcSurf->GetGpuDev()))
            {
                CHECK_RC(pSysSurf->MapShared(pSrcSurf->GetGpuDev()));
            }
            sysCtxDmaOffset = pSysSurf->GetCtxDmaOffsetGpuShared(pSrcSurf->GetGpuDev());
        }
        // Use appropriate engine to ensure that we do not conflict with the actual
        // engines that may be used for test transfers
        DmaWrapper dmaWrap;
        CHECK_RC(dmaWrap.Initialize(pTestConfig,
                                    Memory::NonCoherent,
                                    (GetTransferHw() == LwLinkImpl::HT_CE) ?
                                        DmaWrapper::TWOD : DmaWrapper::COPY_ENGINE));
        Utility::CleanupOnReturn<DmaWrapper, RC>
            dmaCleanup(&dmaWrap, &DmaWrapper::Cleanup);
    
        dmaWrap.SetSurfaces(pSrcSurf.get(), pSysSurf.get());
        dmaWrap.SetCtxDmaOffsets(pSrcSurf->GetCtxDmaOffsetGpu(), sysCtxDmaOffset);
        CHECK_RC(dmaWrap.Copy(0,                               // Starting src X, in bytes not pixels.
                            0,                               // Starting src Y, in lines.
                            pSysSurf->GetPitch(),            // Width of copied rect, in bytes.
                            pSysSurf->GetHeight(),           // Height of copied rect, in bytes
                            0,                               // Starting dst X, in bytes not pixels.
                            0,                               // Starting dst Y, in lines.
                            pTestConfig->TimeoutMs(),
                            0,
                            true));
    }
    else // !UseDmaInit
    {
        pSrcSurf->Map();
        pSysSurf->Map();
        
        const UINT32 surfHeight = pSrcSurf->GetHeight();
        const UINT32 surfWidth = pSrcSurf->GetWidth();
        vector<UINT32> srcLine(surfWidth);
        vector<UINT32> sysLine(surfWidth);
        
        for (size_t lwrY = 0; lwrY < surfHeight; lwrY++)
        {
            const UINT64 srcStartOffset = pSrcSurf->GetPixelOffset(0, lwrY);
            const UINT64 sysStartOffset = pSysSurf->GetPixelOffset(0, lwrY);

            // Copy the surface data line by line
            Platform::VirtualRd(static_cast<UINT08*>(pSrcSurf->GetAddress()) + srcStartOffset,
                                &srcLine[0],
                                surfWidth*sizeof(UINT32));
            Platform::VirtualWr(static_cast<UINT08*>(pSysSurf->GetAddress()) + sysStartOffset,
                                &sysLine[0],
                                surfWidth*sizeof(UINT32));
        }
        
        pSrcSurf->Unmap();
        pSysSurf->Unmap();
    }
    
    return rc;
}

//------------------------------------------------------------------------------
// Initialize all the copy data once the surfaces have been allocated
RC TestModeCe2d::TestDirectionDataCe2d::InitializeCopyData
(
    TransferType tt
   ,GpuDevice   *pPeerDev
)
{
    RC rc;

    switch (tt)
    {
        case TT_SYSMEM:
            m_SrcCtxDmaOffset = m_pSrcSurf->GetCtxDmaOffsetGpu();
            m_DstCtxDmaOffset = m_pDstSurf->GetCtxDmaOffsetGpu();
            break;
        case TT_LOOPBACK:
            // For loopback input testing, the source surface is being read and
            // therefore, it should be loopback mapped and the GPU needs to read
            // the source surface through the peer interface, for output testing
            // the reverse is true
            if (IsInput())
            {
                CHECK_RC(m_pSrcSurf->MapLoopback());
                m_SrcCtxDmaOffset = m_pSrcSurf->GetCtxDmaOffsetGpuPeer(0);
                m_DstCtxDmaOffset = m_pDstSurf->GetCtxDmaOffsetGpu();
            }
            else
            {
                CHECK_RC(m_pDstSurf->MapLoopback());
                m_SrcCtxDmaOffset = m_pSrcSurf->GetCtxDmaOffsetGpu();
                m_DstCtxDmaOffset = m_pDstSurf->GetCtxDmaOffsetGpuPeer(0);
            }
            break;
        case TT_P2P:
            // The offsets being used depend upon direction as well as the where
            // the CEs are acquired for testing
            if (IsInput())
            {
                if (IsHwLocal())
                {
                    // When testing DUT input and the CEs are acquired on the
                    // DUT, the source surface needs to be peer mapped on the
                    // DUT and the peer GPU virtual needs to be used to access
                    // the source surface (DUT pulls data from remote)
                    CHECK_RC(m_pSrcSurf->MapPeer(pPeerDev));
                    m_SrcCtxDmaOffset = m_pSrcSurf->GetCtxDmaOffsetGpuPeer(0, pPeerDev, 0);
                    m_DstCtxDmaOffset = m_pDstSurf->GetCtxDmaOffsetGpu();
                }
                else
                {
                    // When testing DUT input and the CEs are acquired on the
                    // remote device, the destination surface needs to be peer
                    // mapped on the remote device and the peer GPU virtual
                    // needs to be used to access the destination surface
                    // (Remote device pushes data to the DUT)
                    CHECK_RC(m_pDstSurf->MapPeer(pPeerDev));
                    m_SrcCtxDmaOffset = m_pSrcSurf->GetCtxDmaOffsetGpu();
                    m_DstCtxDmaOffset = m_pDstSurf->GetCtxDmaOffsetGpuPeer(0, pPeerDev, 0);
                }
            }
            else
            {
                if (IsHwLocal())
                {
                    // When testing DUT output and the CEs are acquired on the
                    // DUT device, the destination surface needs to be peer
                    // mapped on the DUT device and the peer GPU virtual
                    // needs to be used to access the destination surface
                    // (DUT pushes data to the remote device)
                    CHECK_RC(m_pDstSurf->MapPeer(pPeerDev));
                    m_SrcCtxDmaOffset = m_pSrcSurf->GetCtxDmaOffsetGpu();
                    m_DstCtxDmaOffset = m_pDstSurf->GetCtxDmaOffsetGpuPeer(0, pPeerDev, 0);
                }
                else
                {
                    // When testing DUT output and the CEs are acquired on the
                    // remote device, the source surface needs to be peer mapped
                    // on the remote device and the peer GPU virtual needs to be
                    // used to access the source surface (remote device pulls
                    // data from the DUT)
                    CHECK_RC(m_pSrcSurf->MapPeer(pPeerDev));
                    m_SrcCtxDmaOffset = m_pSrcSurf->GetCtxDmaOffsetGpuPeer(0, pPeerDev, 0);
                    m_DstCtxDmaOffset = m_pDstSurf->GetCtxDmaOffsetGpu();
                }
            }
            break;
        default:
            Printf(Tee::PriError,
                   "%s : Unknown transfer type %d!\n",
                   __FUNCTION__, tt);
            return RC::SOFTWARE_ERROR;
    }

    // Each copy is the height of the subregion, which is half the region
    m_CopyHeight = m_pSrcSurf->GetHeight() / m_Copies.size() / 2;
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Write the actual methods to perform the copy via the CE.
//!
//! \param pCh                 : Pointer to channel to write methods on
//! \param ceClass             : Copy engine class used for the copy
//! \param pSrcSurf            : Pointer to the source surface
//! \param srcOffset64         : Source GPU virtual offset for the copy (for block
//!                              linear formats points to the closest line that starts
//!                              on a GOB boundary)
//! \param yOffset             : y offset (in lines) from the line pointed to by
//!                              srcOffset64 for the copy to start at
//! \param dstOffset64         : Destination GPU virtual offset for the copy
//! \param semOffset64         : Semaphore GPU virtual offset for the copy, if zero then
//!                              a semaphore is not written
//! \param height              : Height of the copy
//! \param semPayload          : Semaphore payload to write (unused if semaphore not
//!                              written)
//! \param pixelComponentMask  : Mask of pixel components to write (XYZW)
//!
//! \return OK if successful, not OK if writing copy methods failed
RC TestModeCe2d::TestDirectionDataCe2d::WriteCeMethods
(
     Channel *pCh
    ,UINT32 ceClass
    ,Surface2DPtr pSrcSurf
    ,UINT64 srcOffset64
    ,UINT32 yOffset
    ,UINT64 dstOffset64
    ,UINT64 semOffset64
    ,UINT32 height
    ,UINT32 semPayload
    ,UINT32 pixelComponentMask
)
{
    RC rc;
    const UINT32 pitch = pSrcSurf->GetPitch();

    UINT32 launchDma =
        DRF_DEF(C0B5, _LAUNCH_DMA, _SRC_TYPE, _VIRTUAL) |
        DRF_DEF(C0B5, _LAUNCH_DMA, _DST_TYPE, _VIRTUAL) |
        DRF_DEF(C0B5, _LAUNCH_DMA, _DST_MEMORY_LAYOUT, _PITCH) |
        DRF_DEF(C0B5, _LAUNCH_DMA, _INTERRUPT_TYPE, _NONE) |
        DRF_DEF(C0B5, _LAUNCH_DMA, _DATA_TRANSFER_TYPE, _PIPELINED) |
        DRF_DEF(C0B5, _LAUNCH_DMA, _MULTI_LINE_ENABLE, _TRUE);

    switch (ceClass)
    {
        case PASCAL_DMA_COPY_A:
        case PASCAL_DMA_COPY_B:
        case VOLTA_DMA_COPY_A:
        case TURING_DMA_COPY_A:
            launchDma |= DRF_DEF(C0B5, _LAUNCH_DMA, _SRC_BYPASS_L2, _FORCE_VOLATILE) |
                         DRF_DEF(C0B5, _LAUNCH_DMA, _DST_BYPASS_L2, _FORCE_VOLATILE);
            break;
        case AMPERE_DMA_COPY_A:
        case AMPERE_DMA_COPY_B:
        case HOPPER_DMA_COPY_A:
        case BLACKWELL_DMA_COPY_A:
        default:
            break;
    }

    UINT32 width = pSrcSurf->GetWidth();

    const UINT32 subch = LWA06F_SUBCHANNEL_COPY_ENGINE;
    if (pixelComponentMask == ALL_COMPONENTS)
    {
        launchDma |= DRF_DEF(C0B5, _LAUNCH_DMA, _REMAP_ENABLE, _FALSE);

        // When not remapping width is in bytes instead of pixels
        width *= pSrcSurf->GetBytesPerPixel();
    }
    else
    {
        const UINT32 bpp = pSrcSurf->GetBytesPerPixel();

        launchDma |= DRF_DEF(C0B5, _LAUNCH_DMA, _REMAP_ENABLE, _TRUE);

        UINT32 remapComponent = 0;
        remapComponent |= DRF_DEF(C0B5, _SET_REMAP_COMPONENTS, _COMPONENT_SIZE, _ONE);
        remapComponent |= DRF_NUM(C0B5, _SET_REMAP_COMPONENTS, _NUM_SRC_COMPONENTS, bpp - 1);
        remapComponent |= DRF_NUM(C0B5, _SET_REMAP_COMPONENTS, _NUM_DST_COMPONENTS, bpp - 1);

        (pixelComponentMask & 0x1) ?
            remapComponent |= DRF_DEF(C0B5, _SET_REMAP_COMPONENTS, _DST_X, _SRC_X):
            remapComponent |= DRF_DEF(C0B5, _SET_REMAP_COMPONENTS, _DST_X, _NO_WRITE);
        (pixelComponentMask & 0x2) ?
            remapComponent |= DRF_DEF(C0B5, _SET_REMAP_COMPONENTS, _DST_Y, _SRC_Y):
            remapComponent |= DRF_DEF(C0B5, _SET_REMAP_COMPONENTS, _DST_Y, _NO_WRITE);
        (pixelComponentMask & 0x4) ?
            remapComponent |= DRF_DEF(C0B5, _SET_REMAP_COMPONENTS, _DST_Z, _SRC_Z):
            remapComponent |= DRF_DEF(C0B5, _SET_REMAP_COMPONENTS, _DST_Z, _NO_WRITE);
        (pixelComponentMask & 0x8) ?
            remapComponent |= DRF_DEF(C0B5, _SET_REMAP_COMPONENTS, _DST_W, _SRC_W):
            remapComponent |= DRF_DEF(C0B5, _SET_REMAP_COMPONENTS, _DST_W, _NO_WRITE);

        CHECK_RC(pCh->Write(subch, LWC0B5_SET_REMAP_COMPONENTS, remapComponent));
    }

    CHECK_RC(pCh->Write(subch, LWC0B5_OFFSET_IN_UPPER, 8,
                        DRF_NUM(C0B5, _OFFSET_IN_UPPER, _UPPER,
                                (UINT32)(srcOffset64 >> 32)),    // LWC0B5_OFFSET_IN_UPPER
                        (UINT32)srcOffset64,                     // LWC0B5_OFFSET_IN_LOWER
                        DRF_NUM(C0B5, _OFFSET_OUT_UPPER, _UPPER,
                                (UINT32)(dstOffset64 >> 32)),    // LWC0B5_OFFSET_OUT_UPPER
                        (UINT32)dstOffset64,                     // LWC0B5_OFFSET_OUT_LOWER
                        pitch,                                   // LWC0B5_PITCH_IN
                        pitch,                                   // LWC0B5_PITCH_OUT
                        width,                                   // LWC0B5_LINE_LENGTH_IN
                        height));                                // LWC0B5_LINE_COUNT

    if (pSrcSurf->GetLayout() == Surface2D::BlockLinear)
    {
        launchDma |= DRF_DEF(C0B5, _LAUNCH_DMA, _SRC_MEMORY_LAYOUT, _BLOCKLINEAR);
        CHECK_RC(pCh->Write(subch, LWC0B5_SET_SRC_BLOCK_SIZE,
            DRF_NUM(C0B5, _SET_SRC_BLOCK_SIZE, _WIDTH,  pSrcSurf->GetLogBlockWidth()) |
            DRF_NUM(C0B5, _SET_SRC_BLOCK_SIZE, _HEIGHT, pSrcSurf->GetLogBlockHeight()) |
            DRF_NUM(C0B5, _SET_SRC_BLOCK_SIZE, _DEPTH,  pSrcSurf->GetLogBlockDepth()) |
            DRF_DEF(C0B5, _SET_SRC_BLOCK_SIZE, _GOB_HEIGHT, _GOB_HEIGHT_FERMI_8)));

        CHECK_RC(pCh->Write(subch, LWC0B5_SET_SRC_WIDTH, 5,
                            width,                                         // LWC0B5_SET_SRC_WIDTH
                            pSrcSurf->GetHeight(),                         // LWC0B5_SET_SRC_HEIGHT
                            pSrcSurf->GetDepth(),                          // LWC0B5_SET_SRC_DEPTH
                            0,                                             // LWC0B5_SET_SRC_LAYER
                            DRF_NUM(C0B5, _SET_SRC_ORIGIN, _Y, yOffset))); // LWC0B5_SET_SRC_ORIGIN
    }
    else
    {
        launchDma |= DRF_DEF(C0B5, _LAUNCH_DMA, _SRC_MEMORY_LAYOUT, _PITCH);
    }

    if (semOffset64)
    {
        CHECK_RC(pCh->Write(subch, LWC0B5_SET_SEMAPHORE_A, 3,
                            DRF_NUM(C0B5, _SET_SEMAPHORE_A, _UPPER,
                                    (UINT32)(semOffset64 >> 32)),
                            (UINT32)semOffset64,
                            semPayload));
        launchDma |=
            DRF_DEF(C0B5, _LAUNCH_DMA, _FLUSH_ENABLE, _TRUE) |
            DRF_DEF(C0B5, _LAUNCH_DMA, _SEMAPHORE_TYPE, _RELEASE_FOUR_WORD_SEMAPHORE);

        if (ceClass != PASCAL_DMA_COPY_A)
            launchDma |= DRF_DEF(C3B5, _LAUNCH_DMA, _FLUSH_TYPE, _SYS);
    }
    else
    {
        launchDma |= DRF_DEF(C0B5, _LAUNCH_DMA, _FLUSH_ENABLE, _FALSE) |
                     DRF_DEF(C0B5, _LAUNCH_DMA, _SEMAPHORE_TYPE, _NONE);
    }
    CHECK_RC(pCh->Write(subch, LWC0B5_LAUNCH_DMA, launchDma));

    // NOTE : DO NOT FLUSH - all channels should be flushed as close together
    // as possible
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Write the actual methods to perform the copy.
//!
//! \param pCh         : Pointer to channel to write methods on
//! \param copyClass   : GPU copy class being used
//! \param pSrcSurf    : Pointer to the source surface
//! \param pDstSurf    : Pointer to the destination surface
//! \param srcOffset64 : Source GPU virtual offset of the start of the surface
//! \param srcY        : Source Y offset within the surface
//! \param dstOffset64 : Destination GPU virtual offset of the start of the surface
//! \param dstY        : Destination Y offset within the surface
//! \param semOffset64 : Semaphore GPU virtual offset for the copy, if zero then
//!                      a semaphore is not written
//! \param height      : Height of the copy
//! \param semPayload  : Semaphore payload to write (unused if semaphore not
//!                      written)
//!
//! \return OK if successful, not OK if writing copy methods failed
RC TestModeCe2d::TestDirectionDataCe2d::WriteCopyMethods
(
     Channel *pCh
    ,UINT32 copyClass
    ,Surface2DPtr pSrcSurf
    ,Surface2DPtr pDstSurf
    ,UINT64 srcOffset64
    ,UINT32 srcY
    ,UINT64 dstOffset64
    ,UINT32 dstY
    ,Surface2DPtr pSemSurf
    ,UINT64 semOffset64
    ,UINT32 height
    ,UINT32 semPayload
    ,bool   bCopyByLine
    ,UINT32 pixelComponentMask
)
{
    RC rc;
    const UINT32 copyCount  = bCopyByLine ? height : 1;
    const UINT32 copyHeight = bCopyByLine ? 1 : height;
    const UINT32 gobHeight = pSrcSurf->GetGpuDev()->GobHeight();

    UINT64 lwrSrcOffset;
    UINT64 lwrDstOffset;
    UINT32 ySrcOffset = 0;
    for (UINT32 lwrCopy = 0; lwrCopy < copyCount; lwrCopy++)
    {
        // In block linear mode, source offsets must start at the beginning of
        // a GOB, callwlate the source offset and a y offset from the line
        if (pSrcSurf->GetLayout() == Surface2D::BlockLinear)
        {
            ySrcOffset = (srcY + (lwrCopy * copyHeight)) % gobHeight;
            lwrSrcOffset = srcOffset64 +
                 pSrcSurf->GetPixelOffset(0,
                                          srcY + (lwrCopy * copyHeight) - ySrcOffset);
        }
        else
        {
            lwrSrcOffset = srcOffset64 +
                           pSrcSurf->GetPixelOffset(0, srcY + (lwrCopy * copyHeight));
        }
        lwrDstOffset = dstOffset64 + pDstSurf->GetPixelOffset(0, dstY + (lwrCopy * copyHeight));

        if (GetTransferHw() == LwLinkImpl::HT_TWOD)
        {
            CHECK_RC(WriteTwodMethods(pCh,
                                      pSrcSurf,
                                      lwrSrcOffset,
                                      ySrcOffset,
                                      lwrDstOffset,
                                      semOffset64,
                                      copyHeight,
                                      (lwrCopy == (copyCount - 1)) ? semPayload : 0));
        }
        else
        {
            CHECK_RC(WriteCeMethods(pCh,
                                    copyClass,
                                    pSrcSurf,
                                    lwrSrcOffset,
                                    ySrcOffset,
                                    lwrDstOffset,
                                    semOffset64,
                                    copyHeight,
                                    (lwrCopy == (copyCount - 1)) ? semPayload : 0,
                                    pixelComponentMask));
        }
    }
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Write the actual methods to perform the copy.
//!
//! \param pCh         : Pointer to channel to write methods on
//! \param pSrcSurf    : Pointer to the source surface
//! \param srcOffset64 : Source GPU virtual offset for the copy (for block
//!                      linear formats points to the closest line that starts
//!                      on a GOB boundary)
//! \param yOffset     : y offset (in lines) from the line pointed to by
//!                      srcOffset64 for the copy to start at
//! \param dstOffset64 : Destination GPU virtual offset for the copy
//! \param semOffset64 : Semaphore GPU virtual offset for the copy, if zero then
//!                      a semaphore is not written
//! \param height      : Height of the copy
//! \param semPayload  : Semaphore payload to write (unused if semaphore not
//!                      written)
//!
//! \return OK if successful, not OK if writing copy methods failed
RC TestModeCe2d::TestDirectionDataCe2d::WriteTwodMethods
(
     Channel *pCh
    ,Surface2DPtr pSrcSurf
    ,UINT64 srcOffset64
    ,UINT32 yOffset
    ,UINT64 dstOffset64
    ,UINT64 semOffset64
    ,UINT32 height
    ,UINT32 semPayload
)
{
    RC rc;
    const UINT32 subch = LWA06F_SUBCHANNEL_2D;
    const UINT32 pitch = pSrcSurf->GetPitch();
    const UINT32 bpp   = pSrcSurf->GetBytesPerPixel();

    // Set dst methods
    CHECK_RC(pCh->Write(subch, LW902D_SET_DST_FORMAT, 2,
                        LW902D_SET_DST_FORMAT_V_A8R8G8B8,
                        LW902D_SET_DST_MEMORY_LAYOUT_V_PITCH));
    CHECK_RC(pCh->Write(subch, LW902D_SET_DST_PITCH, 5,
                        pitch,
                        pitch / bpp,
                        height,
                        DRF_NUM(902D, _SET_DST_OFFSET_UPPER, _V,
                                static_cast<UINT32>(dstOffset64 >> 32)),
                        static_cast<UINT32>(dstOffset64)));
    CHECK_RC(pCh->Write(subch, LW902D_SET_PIXELS_FROM_MEMORY_DST_WIDTH, 2,
                        pitch / bpp,
                        height));

    // Set src methods
    CHECK_RC(pCh->Write(subch, LW902D_SET_SRC_FORMAT, LW902D_SET_DST_FORMAT_V_A8R8G8B8));
    CHECK_RC(pCh->Write(subch, LW902D_SET_SRC_MEMORY_LAYOUT,
                        (Surface::BlockLinear == pSrcSurf->GetLayout())
                            ? LW902D_SET_SRC_MEMORY_LAYOUT_V_BLOCKLINEAR
                            : LW902D_SET_SRC_MEMORY_LAYOUT_V_PITCH));

    // The srcOffset64 points to a line that starts on a GOB boundary for block
    // linear and the actual line to copy (as an offset from the GOB start line)
    // is provided in the yOffset parameter.  This is because the SRC_OFFSET
    // method must point to the start of a GOB for block linear formats
    //
    // For TwoD m_Copies, pretend that the height of the surface we are copying
    // copying from is enough to span all the lines being copied
    //
    // For non-block linear using the copy height is sufficient
    //
    // For block linear, the height needs to be a multiple of GobHeight and
    // contain enough lines to span the entire copy
    //
    UINT32 surfHeight = height;
    if (Surface::BlockLinear == pSrcSurf->GetLayout())
    {
        surfHeight = ALIGN_UP(yOffset + height, pSrcSurf->GetGpuDev()->GobHeight());
    }

    CHECK_RC(pCh->Write(subch, LW902D_SET_SRC_PITCH, 5,
                        pitch,
                        pitch / bpp,
                        surfHeight,
                        DRF_NUM(902D, _SET_SRC_OFFSET_UPPER, _V,
                                static_cast<UINT32>(srcOffset64 >> 32)),
                        static_cast<UINT32>(srcOffset64)));
    if (Surface::BlockLinear == pSrcSurf->GetLayout())
    {
        CHECK_RC(pCh->Write(subch, LW902D_SET_SRC_BLOCK_SIZE,
                            DRF_NUM(902D, _SET_SRC_BLOCK_SIZE, _HEIGHT,
                                    pSrcSurf->GetLogBlockHeight()) |
                            DRF_NUM(902D, _SET_SRC_BLOCK_SIZE, _DEPTH,
                                    pSrcSurf->GetLogBlockDepth())));
    }

    // Previous methods only describe the surface itself, not how to perform the
    // transfer.  Setup the transfer so that one pixel on input equals one pixel
    // on the output
    CHECK_RC(pCh->Write(subch,
                        LW902D_SET_PIXELS_FROM_MEMORY_DST_X0,
                        11,
                        0,            // LW902D_SET_PIXELS_FROM_MEMORY_DST_X0
                        0,            // LW902D_SET_PIXELS_FROM_MEMORY_DST_Y0
                        pitch / bpp,  // LW902D_SET_PIXELS_FROM_MEMORY_DST_WIDTH
                        height,       // LW902D_SET_PIXELS_FROM_MEMORY_DST_HEIGHT
                        0,            // LW902D_SET_PIXELS_FROM_MEMORY_DU_DX_FRAC
                        1,            // LW902D_SET_PIXELS_FROM_MEMORY_DU_DX_INT
                        0,            // LW902D_SET_PIXELS_FROM_MEMORY_DV_DY_FRAC
                        1,            // LW902D_SET_PIXELS_FROM_MEMORY_DV_DY_INT
                        0,            // LW902D_SET_PIXELS_FROM_MEMORY_SRC_X0_FRAC
                        0,            // LW902D_SET_PIXELS_FROM_MEMORY_SRC_X0_INT
                        0));          // LW902D_SET_PIXELS_FROM_MEMORY_SRC_Y0_FRAC

    CHECK_RC(pCh->Write(subch, LW902D_SET_ROP, 0xCC)); // SRCCOPY - see BitBlt in MSDN
    CHECK_RC(pCh->Write(subch, LW902D_SET_OPERATION, LW902D_SET_OPERATION_V_SRCCOPY));
    CHECK_RC(pCh->Write(subch, LW902D_PIXELS_FROM_MEMORY_SRC_Y0_INT, yOffset));

    if (semOffset64)
    {
        pCh->SetSemaphoreReleaseFlags(Channel::FlagSemRelDefault);
        CHECK_RC(pCh->SetSemaphoreOffset(semOffset64));
        CHECK_RC(pCh->SemaphoreRelease(semPayload));
    }

    // NOTE : DO NOT FLUSH - all channels should be flushed as close together
    // as possible
    return rc;
}
