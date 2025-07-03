/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwldt_test_mode.h"

#include "core/include/cnslmgr.h"
#include "core/include/console.h"
#include "core/include/cpu.h"
#include "core/include/platform.h"
#include "core/include/tasker.h"
#include "core/include/utility.h"
#include "core/include/xp.h"
#include "device/interface/pcie.h"
#include "device/interface/lwlink/lwlthroughputcounters.h"
#include "device/interface/lwlink/lwllatencybincounters.h"
#include "device/interface/lwlink/lwlrecal.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/lwlink/lwlinkimpl.h"
#include "gpu/tests/gputestc.h"
#include "gpu/utility/surf2d.h"
#include "gpu/utility/thermalslowdown.h"
#include "gpu/utility/thermalthrottling.h"
#include "lwldt_test_route.h"

#include <set>

#include <boost/lambda/lambda.hpp>
#include <boost/lambda/bind.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <boost/range/iterator_range.hpp>
#include <boost/range/algorithm/copy.hpp>
#include <boost/range/adaptor/filtered.hpp>

namespace
{
    const UINT64 PROGRESS_INDICATOR_SEC = 5;

    const FLOAT64 THERMAL_SLOWDOWN_GUARD_PCT = 0.05;

    const vector<LwLinkThroughputCount::Config> s_TpcConfig =
    {
        { LwLinkThroughputCount::CI_TX_COUNT_0, LwLinkThroughputCount::UT_PACKETS,   LwLinkThroughputCount::FF_ALL, LwLinkThroughputCount::PF_WR_RSP_ONLY, LwLinkThroughputCount::PS_1 }  //$
       ,{ LwLinkThroughputCount::CI_TX_COUNT_1, LwLinkThroughputCount::UT_PACKETS, LwLinkThroughputCount::FF_ALL, LwLinkThroughputCount::PF_DATA_ONLY, LwLinkThroughputCount::PS_1 }  //$
       ,{ LwLinkThroughputCount::CI_RX_COUNT_0, LwLinkThroughputCount::UT_PACKETS,   LwLinkThroughputCount::FF_ALL, LwLinkThroughputCount::PF_WR_RSP_ONLY, LwLinkThroughputCount::PS_1 }  //$
       ,{ LwLinkThroughputCount::CI_RX_COUNT_1, LwLinkThroughputCount::UT_PACKETS, LwLinkThroughputCount::FF_ALL, LwLinkThroughputCount::PF_DATA_ONLY, LwLinkThroughputCount::PS_1 }  //$
    };

    LwLink::TransferType GetTransferType
    (
         bool                            bDataIn
        ,LwLinkImpl::HwType              inHw
        ,bool                            bInHwLocal
        ,LwLinkImpl::HwType              outHw
        ,bool                            bOutHwLocal
    )
    {
        if (inHw == LwLinkImpl::HT_NONE)
        {
            return bOutHwLocal ? LwLink::TT_UNIDIR_WRITE : LwLink::TT_UNIDIR_READ;
        }
        else if (outHw == LwLinkImpl::HT_NONE)
        {
            return bInHwLocal ? LwLink::TT_UNIDIR_READ : LwLink::TT_UNIDIR_WRITE;
        }
        else if ((bInHwLocal == bOutHwLocal) && (inHw == outHw))
        {
            return LwLink::TT_READ_WRITE;
        }
        else
        {
            if (bDataIn)
                return bInHwLocal ? LwLink::TT_BIDIR_READ : LwLink::TT_BIDIR_WRITE;
            else
                return bOutHwLocal ? LwLink::TT_BIDIR_WRITE : LwLink::TT_BIDIR_READ;
        }
    }
}

using namespace LwLinkDataTestHelper;

//------------------------------------------------------------------------------
// TestMode public implementation
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//! \brief Constructor
//!
TestMode::TestMode
(
    GpuTestConfiguration *pTestConfig
   ,AllocMgrPtr pAllocMgr
   ,UINT32 numErrorsToPrint
   ,UINT32 copyLoopCount
   ,FLOAT64 bwThresholdPct
   ,bool bShowBandwidthStats
   ,UINT32 pauseMask
   ,Tee::Priority pri
)
  : m_pTestConfig(pTestConfig)
   ,m_TransferDir(TD_ILWALID)
   ,m_SysmemTransferDir(TD_ILWALID)
   ,m_TransferTypes(0)
   ,m_NumErrorsToPrint(numErrorsToPrint)
   ,m_CopyLoopCount(copyLoopCount)
   ,m_BwThresholdPct(bwThresholdPct)
   ,m_bSysmemLoopback(false)
   ,m_bLoopbackInput(true)
   ,m_bShowBandwidthStats(bShowBandwidthStats)
   ,m_bFailOnSysmemBw(false)
   ,m_bShowCopyProgress(false)
   ,m_ProgressPrintTick(0UL)
   ,m_PauseMask(pauseMask)
   ,m_PrintPri(pri)
   ,m_bAddCpuTraffic(false)
   ,m_NumCpuTrafficThreads(0)
   ,m_PowerStateToggle(false)
   ,m_PSToggleThreadId(-1)
   ,m_PSToggleStartTime(0)
   ,m_PSToggleThreadCont(false)
   ,m_PSToggleIntervalUs(100)
   ,m_PSToggleTimeout(1000)
   ,m_DoOneCopyCount(0)
   ,m_SwLPEntryCount(0)
   ,m_MinLPCount(0)
   ,m_MakeLowPowerCopies(false)
   ,m_InsertIdleAfterCopy(false)
   ,m_IdleAfterCopyMs(1)
   ,m_ThermalThrottling(false)
   ,m_ThermalThrottlingOnCount(0)
   ,m_ThermalThrottlingOffCount(0)
   ,m_ThermalSlowdown(false)
   ,m_ThermalSlowdownPeriodUs(500000)
   ,m_ThermalSlowdownStartTime(0)
   ,m_pAllocMgr(pAllocMgr)
{
}

//------------------------------------------------------------------------------
//! \brief Destructor
//!
TestMode::~TestMode()
{
    ReleaseResources();
}

//------------------------------------------------------------------------------
//! \brief Acquire resources on both the "dut" and any remote devices in order
//! that the test mode can execute all necessary copies.  Resources are acquired
//! from the allocation manager which "lazy allocates"
//!
//! \return OK if successful, not OK if resource acquire failed
/* virtual */ RC TestMode::AcquireResources()
{
    RC rc;

    if (GetPowerStateToggle() || GetThermalSlowdown() || GetInsertIdleAfterCopy() || GetShowLowPowerCheck())
    {
        // Ensure the test starts in FB mode before capturing the counters
        if (GetPowerStateToggle() || GetThermalSlowdown())
        {
            CHECK_RC(RequestPowerState(LwLinkPowerState::SLS_PWRM_FB, true));
        }
        SetSwLPEntryCount(0);

        set<LPCounter> lpCounters;
        CHECK_RC(ForEachLink(
            [&](TestDevicePtr pDev, UINT32 linkId)->RC
            {
                if (pDev->SupportsInterface<LwLinkPowerStateCounters>())
                {
                    vector<LwLink::LinkStatus> linkStatus;
                    CHECK_RC(pDev->GetInterface<LwLink>()->GetLinkStatus(&linkStatus));

                    // This should exclude links that are intentionally not trained to
                    // active and therefore can't report LP entry/exit, eg. TREX links.
                    if (linkStatus[linkId].linkState == LwLink::LS_ACTIVE ||
                        linkStatus[linkId].linkState == LwLink::LS_SLEEP)
                    {
                        auto pCounters = pDev->GetInterface<LwLinkPowerStateCounters>();
                        CHECK_RC(pCounters->ClearLPCounts(linkId));
                        lpCounters.emplace(pDev, linkId, 0);
                    }
                }

                return rc;
            },
            nullptr
        ));
        m_LastHwLPCounters.clear();
        m_LastHwLPCounters.insert(m_LastHwLPCounters.end(), lpCounters.begin(), lpCounters.end());
    }

    SetDoOneCopyCount(0);

    return rc;
}

RC TestMode::ClearLPCounts()
{
    RC rc;

    CHECK_RC(ForEachLink(
        [&](TestDevicePtr pDev, UINT32 linkId)->RC
        {
            if (pDev->SupportsInterface<LwLinkPowerStateCounters>())
            {
                vector<LwLink::LinkStatus> linkStatus;
                CHECK_RC(pDev->GetInterface<LwLink>()->GetLinkStatus(&linkStatus));

                // This should exclude links that are intentionally not trained to
                // active and therefore can't report LP entry/exit, eg. TREX links.
                if (linkStatus[linkId].linkState == LwLink::LS_ACTIVE ||
                    linkStatus[linkId].linkState == LwLink::LS_SLEEP)
                {
                    auto pCounters = pDev->GetInterface<LwLinkPowerStateCounters>();
                    CHECK_RC(pCounters->ClearLPCounts(linkId));
                }
            }
            return rc;
        },
        nullptr
    ));

    return rc;
}

//------------------------------------------------------------------------------
RC TestMode::CallwlateConnectionThroughput()
{
    RC rc;

    map<LwLinkConnectionPtr, FLOAT64> connectionEfficiency;

    for (auto routeIt : m_TestData)
    {
        TestRoutePtr pRoute = routeIt.first;

        vector<TestDirectionDataPtr> directions = {routeIt.second.pDutInTestData, routeIt.second.pDutOutTestData};
        for (UINT32 dirIdx = 0; dirIdx < directions.size(); dirIdx++)
        {
            TestDirectionDataPtr pTestDir = directions[dirIdx];
            TestDirectionDataPtr pOtherDir = directions[dirIdx^1];

            if (!pTestDir->IsInUse())
                continue;


            const bool bDataIn = pTestDir->IsInput();
            const LwLinkImpl::HwType inHw = bDataIn ? pTestDir->GetTransferHw() : pOtherDir->GetTransferHw();
            const bool bInHwLocal = bDataIn ? pTestDir->IsHwLocal() : pOtherDir->IsHwLocal();
            const LwLinkImpl::HwType outHw = bDataIn ? pOtherDir->GetTransferHw() : pTestDir->GetTransferHw();
            const bool bOutHwLocal = bDataIn ? pOtherDir->IsHwLocal() : pTestDir->IsHwLocal();

            TestDevicePtr pOriginDev = pRoute->GetLocalLwLinkDev();
            LwLink::DataType dt;
            if (bDataIn)
            {
                dt = bInHwLocal ? LwLink::DT_RESPONSE : LwLink::DT_REQUEST;
                if (dt == LwLink::DT_REQUEST)
                    pOriginDev = pRoute->GetRemoteLwLinkDev();
            }
            else
            {
                dt = bOutHwLocal ? LwLink::DT_REQUEST : LwLink::DT_RESPONSE;
                if (dt == LwLink::DT_RESPONSE)
                    pOriginDev = pRoute->GetRemoteLwLinkDev();
            }

            FLOAT64 readDataFlits = 0.0;
            FLOAT64 writeDataFlits = 0.0;
            UINT32  totalReadFlits = 0;
            UINT32  totalWriteFlits = 0;

            auto pLwLink = pRoute->GetLocalLwLinkDev()->GetInterface<LwLink>();
            pLwLink->GetFlitInfo(pTestDir->GetTransferWidth(),
                                 bDataIn ? inHw : outHw,
                                 (pRoute->GetTransferType() == TT_SYSMEM),
                                 &readDataFlits,
                                 &writeDataFlits,
                                 &totalReadFlits,
                                 &totalWriteFlits);

            auto pathIt = pRoute->GetRoute()->PathsBegin(pOriginDev, dt);
            auto pathEnd = pRoute->GetRoute()->PathsEnd(pOriginDev, dt);

            FLOAT64 minPacketEfficiency = 1.0;
            for (; pathIt != pathEnd; ++pathIt)
            {
                auto peIt = pathIt->PathEntryBegin(pOriginDev);
                auto peEnd = LwLinkPath::PathEntryIterator();
                for (; peIt != peEnd; ++peIt)
                {
                    LwLinkConnectionPtr pCon = peIt->pCon;
                    TestDevicePtr       pDev = peIt->pSrcDev;

                    if (!connectionEfficiency.count(pCon))
                    {
                        FLOAT64 packetEfficiency;
                        CHECK_RC(GetConnectionEfficiency(pRoute, pCon, pDev,
                                                         readDataFlits,
                                                         writeDataFlits,
                                                         static_cast<FLOAT64>(totalReadFlits),
                                                         static_cast<FLOAT64>(totalWriteFlits),
                                                         &packetEfficiency));
                        connectionEfficiency[pCon] = packetEfficiency;
                    }
                    minPacketEfficiency = min(connectionEfficiency[pCon], minPacketEfficiency);
                }
            }

            pTestDir->SetPacketEfficiency(minPacketEfficiency);
            pTestDir->SetMaxDataBwKiBps(pRoute->GetRoute()->GetMaxDataBwKiBps(pOriginDev, dt));
        }
    }

    return OK;
}

//------------------------------------------------------------------------------
// Callwlate the bandwidth between the HSHUB and the FB as on some GPUs (GA100)
// this can become the limiting factor quite easily in the ability to achieve
// SOL lwlink bandwidth
RC TestMode::CallwlateXbarBandwidth()
{
    struct GpuAccessStatus
    {
        bool    bWriting           = false;
        bool    bWrittenTo         = false;
        bool    bReading           = false;
        bool    bReadFrom          = false;
        bool    bXbarBwSet         = false;
        UINT32  xbarBandwidthKiBps = 0.0;
        GpuAccessStatus()          = default;
    };
    map<GpuSubdevice *, GpuAccessStatus> gpuAccessStatus;

    // Determine the access pattern for each GPU for all routes being tested
    for (const auto & routeIt : m_TestData)
    {
        GpuSubdevice *pLocalSubdev  = routeIt.first->GetLocalSubdevice();
        GpuSubdevice *pRemoteSubdev =
            (routeIt.first->GetTransferType() == TT_LOOPBACK) ? pLocalSubdev :
                                                                routeIt.first->GetRemoteSubdevice();

        if (pLocalSubdev && (gpuAccessStatus.count(pLocalSubdev) == 0))
            gpuAccessStatus[pLocalSubdev] = GpuAccessStatus();
        if (pRemoteSubdev && (gpuAccessStatus.count(pRemoteSubdev) == 0))
            gpuAccessStatus[pRemoteSubdev] = GpuAccessStatus();

        if (routeIt.second.pDutInTestData->IsInUse())
        {
            const bool bHwLocal = routeIt.second.pDutInTestData->IsHwLocal();
            if (bHwLocal)
            {
                if (pLocalSubdev)
                    gpuAccessStatus[pLocalSubdev].bReading    = true;
                if (pRemoteSubdev)
                    gpuAccessStatus[pRemoteSubdev].bReadFrom  = true;
            }
            else
            {
                if (pLocalSubdev)
                    gpuAccessStatus[pLocalSubdev].bWrittenTo = true;
                if (pRemoteSubdev)
                    gpuAccessStatus[pRemoteSubdev].bWriting  = true;
            }
        }
        if (routeIt.second.pDutOutTestData->IsInUse())
        {
            const bool bHwLocal = routeIt.second.pDutOutTestData->IsHwLocal();
            if (bHwLocal)
            {
                if (pLocalSubdev)
                    gpuAccessStatus[pLocalSubdev].bWriting    = true;
                if (pRemoteSubdev)
                    gpuAccessStatus[pRemoteSubdev].bWrittenTo = true;
            }
            else
            {
                if (pLocalSubdev)
                    gpuAccessStatus[pLocalSubdev].bReadFrom = true;
                if (pRemoteSubdev)
                    gpuAccessStatus[pRemoteSubdev].bReading = true;
            }


            GpuSubdevice *pWritingDevice   = bHwLocal ? pLocalSubdev  : pRemoteSubdev;
            GpuSubdevice *pWrittenToDevice = bHwLocal ? pRemoteSubdev : pLocalSubdev;

            if (pWritingDevice)
                gpuAccessStatus[pWritingDevice].bWriting     = true;
            if (pWrittenToDevice)
                gpuAccessStatus[pWrittenToDevice].bWrittenTo = true;
        }
    }

    RC rc;
    for (auto & lwrGpuAccess : gpuAccessStatus)
    {
        auto pLwLink = lwrGpuAccess.first->GetInterface<LwLink>();
        rc = pLwLink->GetXbarBandwidth(lwrGpuAccess.second.bWriting,
                                       lwrGpuAccess.second.bReading,
                                       lwrGpuAccess.second.bWrittenTo,
                                       lwrGpuAccess.second.bReadFrom,
                                       &lwrGpuAccess.second.xbarBandwidthKiBps);
        if (rc == RC::UNSUPPORTED_FUNCTION)
        {
            rc.Clear();
            continue;
        }
        CHECK_RC(rc);
        lwrGpuAccess.second.bXbarBwSet = true;
    }

    for (const auto & routeIt : m_TestData)
    {
        GpuSubdevice *pLocalSubdev  = routeIt.first->GetLocalSubdevice();
        GpuSubdevice *pRemoteSubdev = routeIt.first->GetRemoteSubdevice();

        UINT32 xbarBw = 0.0;
        if (pLocalSubdev && gpuAccessStatus[pLocalSubdev].bXbarBwSet)
        {
            xbarBw = gpuAccessStatus[pLocalSubdev].xbarBandwidthKiBps;
            if (pRemoteSubdev && gpuAccessStatus[pRemoteSubdev].bXbarBwSet)
                xbarBw = min(xbarBw, gpuAccessStatus[pRemoteSubdev].xbarBandwidthKiBps);
        }
        else if (pRemoteSubdev && gpuAccessStatus[pRemoteSubdev].bXbarBwSet)
        {
            xbarBw = gpuAccessStatus[pRemoteSubdev].xbarBandwidthKiBps;
        }
        if (routeIt.second.pDutInTestData->IsInUse())
            routeIt.second.pDutInTestData->SetXbarBwKiBps(xbarBw);
        if (routeIt.second.pDutOutTestData->IsInUse())
            routeIt.second.pDutOutTestData->SetXbarBwKiBps(xbarBw);
    }

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Verify that the bandwidth for each route in all tested
//! directions was within tolerance
//!
//! \return OK if successful, not OK if bandwidth check failed
RC TestMode::CheckBandwidth(vector<TestMode::BandwidthInfo>* pBwFailureInfos)
{
    StickyRC rc;
    map<TestRoutePtr, TestData>::iterator pRteTestData = m_TestData.begin();

    CHECK_RC(CallwlateConnectionThroughput());
    CHECK_RC(CallwlateXbarBandwidth());

    // The test mode has successfully run, check and report all bandwidth errors
    // from the test mode
    for (; pRteTestData != m_TestData.end(); pRteTestData++)
    {
        rc = CheckBandwidth(pRteTestData->first,
                            pRteTestData->second.pDutInTestData,
                            pRteTestData->second.pDutOutTestData->GetTransferHw(),
                            pRteTestData->second.pDutOutTestData->IsHwLocal(),
                            pBwFailureInfos);
        rc = CheckBandwidth(pRteTestData->first,
                            pRteTestData->second.pDutOutTestData,
                            pRteTestData->second.pDutInTestData->GetTransferHw(),
                            pRteTestData->second.pDutInTestData->IsHwLocal(),
                            pBwFailureInfos);
    }
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Verify HW LP entry counters correspond to the SW counter
//!
//! \return OK if successful, not OK if LP entry count check failed
RC TestMode::CheckLPCount(UINT32 tol, FLOAT64 pctTol)
{
    StickyRC counterRC;
    Tee::Priority lpPrintPri = (GetShowLowPowerCheck()) ? Tee::PriNormal : m_PrintPri;

    UINT32 controlLPCntValue = GetSwLPEntryCount();

    // Return an error if SW LP entry count is zero only for SW initiated
    // switches. For HW initiated switches this counter is updated inside
    // DoOneCopy and must always be above zero.
    if (GetInsertIdleAfterCopy())
    {
        MASSERT(0 < controlLPCntValue);
    }
    if (0 == controlLPCntValue && GetPowerStateToggle())
    {
        Printf(Tee::PriError,
            "Software didn't have time to switch the power state, please increase the amount\n"
            " of data transferred or decrease the switch time\n");
        return RC::SOFTWARE_ERROR;
    }

    if ((controlLPCntValue < GetTestConfig()->Loops() * 2) && GetThermalSlowdown())
    {
        Printf(Tee::PriError,
            "Software didn't have time to go into and out of Thermal Slowdown, please increase the amount\n"
            " of data transferred or decrease the slowdown period\n");
        return RC::SOFTWARE_ERROR;
    }

    bool checkMin = false;
    if (0 != GetMinLPCount())
    {
        checkMin = true;
        controlLPCntValue = GetMinLPCount();
    }

    bool bAnyCounterOverflowed = false;
    for (const LPCounter& lpCounter : m_LastHwLPCounters)
    {
        bAnyCounterOverflowed = bAnyCounterOverflowed || lpCounter.overflow;

        string outStr = Utility::StrPrintf("Hardware switched %s, link id %u to LP %u times%s",
                                           lpCounter.pDev->GetName().c_str(),
                                           lpCounter.linkId,
                                           lpCounter.count,
                                           lpCounter.overflow ? " (Counter Overflowed)" : "");

        UINT32 lpCountValue = controlLPCntValue;
        if (lpCounter.pDev->SupportsInterface<LwLinkRecal>())
        {
            if (GetPowerStateToggle() || GetThermalSlowdown())
            {
                if (!checkMin && pctTol < 0.0)
                {
                    // Periodic Recal makes it virtually impossible to callwlate an exact L1 entry count
                    // Add a default tolerance of 10% if one isn't already defined.
                    pctTol = 0.1;
                }

                LwLinkRecal* pRecal = lpCounter.pDev->GetInterface<LwLinkRecal>();
                const FLOAT64 recalStartTime = static_cast<FLOAT64>(pRecal->GetRecalStartTimeUs(lpCounter.linkId));
                const FLOAT64 recalMinTime = static_cast<FLOAT64>(pRecal->GetMinRecalTimeUs(lpCounter.linkId));
                const FLOAT64 recalWakeTime = static_cast<FLOAT64>(pRecal->GetRecalWakeTimeUs(lpCounter.linkId));
                const FLOAT64 recalTime = recalMinTime + 6.0;

                // TODO: This callwlation needs further refinement. It seems to work well when
                // GetPSToggleIntervalMs() < recalTime || GetPSToggleIntervalMs() >= (recalTime + recalWakeTime)
                // But in between those ranges it goes awry
                FLOAT64 intervalUs = GetPowerStateToggle() ? GetPSToggleIntervalMs() * 1000.0
                                                           : GetThermalSlowdownPeriodUs();
                FLOAT64 totalIntervalUs = (intervalUs * 2.0) + 6.0;
                FLOAT64 missedLP = recalTime / totalIntervalUs;
                FLOAT64 lpRatio = recalTime / (recalTime + recalStartTime);
                FLOAT64 missedLPRatio = missedLP * lpRatio;
                lpCountValue -= ((static_cast<FLOAT64>(controlLPCntValue) / 2.0) * missedLPRatio);
                if (intervalUs >= (recalTime+ recalWakeTime))
                {
                    FLOAT64 extraLP = intervalUs / (recalTime + recalWakeTime);
                    lpCountValue += (static_cast<FLOAT64>(controlLPCntValue) * extraLP);
                }
            }
            else if (GetInsertIdleAfterCopy())
            {
                if (!checkMin && pctTol < 0.0)
                {
                    // Periodic Recal makes it virtually impossible to callwlate an exact L1 entry count
                    // Just ensure L1 is entered at least once during the idle period
                    checkMin = true;
                }
            }
        }

        if (pctTol >= 0.0)
        {
            const UINT32 tolFromPct =
                static_cast<UINT32>(ceil(static_cast<FLOAT64>(lpCountValue) * pctTol));
            Printf(Tee::PriLow,
                "Using LPCountCheckTolPct (%.2f) instead of LPCountCheckTol (%u), new tolerance %u\n",
                pctTol, tol, tolFromPct);
            tol = tolFromPct;
        }

        bool fail = false;
        if (checkMin)
        {
            if (((tol + lpCountValue) > lpCounter.count) || lpCounter.overflow)
            {
                fail = true;
                counterRC = RC::UNEXPECTED_RESULT;
                string tolStr = "";
                if (tol > 0)
                    tolStr = Utility::StrPrintf(" + tolerance %u", tol);
                outStr += Utility::StrPrintf(" < expected %u%s", lpCountValue, tolStr.c_str());
            }
        }
        else
        {
            INT32 diff = static_cast<INT32>(lpCounter.count) -
                         static_cast<INT32>(lpCountValue);
            if ((static_cast<UINT32>(abs(diff)) > tol) || lpCounter.overflow)
            {
                fail = true;
                counterRC = RC::UNEXPECTED_RESULT;
                string tolStr = "";
                if (tol > 0)
                    tolStr = Utility::StrPrintf(" +/- tolerance %u", tol);
                outStr += Utility::StrPrintf(" != expected %u%s", lpCountValue, tolStr.c_str());
            }
        }

        if (!fail)
            outStr += Utility::StrPrintf(" (expected %u)", lpCountValue);

        if (GetPowerStateToggle())
        {
            outStr += Utility::StrPrintf(
                ", average per one copy was %lf",
                static_cast<FLOAT64>(lpCountValue) / GetDoOneCopyCount()
            );
        }

        outStr += "\n";

        Printf(fail ? Tee::PriError : lpPrintPri, outStr.c_str());
    }

    if (bAnyCounterOverflowed)
    {
        Printf(Tee::PriError,
               "********** LP Counter overflow detected\n"
               "********** Reduce the amount of data copied per loop!\n");
    }

    return counterRC;
}

//------------------------------------------------------------------------------
//! \brief Verify that all links are in the active state
//!
//! \return OK if successful, not OK if link state check failed
RC TestMode::CheckLinkState()
{
    StickyRC rc;
    map<TestRoutePtr, TestData>::iterator pRteTestData = m_TestData.begin();

    // Called before running the test mode to ensure all links are in a valid
    // state
    for (; pRteTestData != m_TestData.end(); pRteTestData++)
    {
        rc = pRteTestData->first->CheckLinkState();
    }
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Verify that all destination surfaces contain the correct data
//!
//! \return OK if successful, not OK if surface check failed
RC TestMode::CheckSurfaces()
{
    StickyRC rc;
    map<TestRoutePtr, TestData>::iterator pRteTestData = m_TestData.begin();

    for (; pRteTestData != m_TestData.end(); pRteTestData++)
    {
        // The test mode has successfully run, check and report all surface
        // errors from the test mode
        rc = CheckDestinationSurface(pRteTestData->first,
                                     pRteTestData->second.pDutInTestData);
        rc = CheckDestinationSurface(pRteTestData->first,
                                     pRteTestData->second.pDutOutTestData);
    }
    return rc;
}

//------------------------------------------------------------------------------
bool TestMode::ContainsLwSwitch(TestRoute &route, TestDirectionData &testDir)
{
    bool containsLwSwitch = false;

    testDir.ForEachDevice(route,
        [&containsLwSwitch](TestDevicePtr dev) -> RC
        {
            containsLwSwitch = containsLwSwitch
                || (dev->GetType() == TestDevice::TYPE_LWIDIA_LWSWITCH);
            return OK;
        }
    );

    return containsLwSwitch;
}

//------------------------------------------------------------------------------
//! \brief Execute a single copy for all routes.  Copies on each route
//! in each direction are all started as close together as possible
//!
//! \return OK if successful, not OK if resource release failed
RC TestMode::DoOneCopy(FLOAT64 timeoutMs)
{
    RC rc;

    // Ensure that all copies start as close together as possible by
    // setting up all copies together (write push buffer but not flushing) and
    // then starting all copies together (flushing all channels)
    for (auto& rteTestData : m_TestData)
    {
        CHECK_RC(SetupCopies(rteTestData.second.pDutInTestData));
        CHECK_RC(SetupCopies(rteTestData.second.pDutOutTestData));
    }

    if (GetThermalThrottling())
    {
        CHECK_RC(StartThermalThrottling());
    }

    auto stopCopies = [&]() -> RC
    {
        StickyRC rc;
        for (auto& rteTestData : m_TestData)
        {
            rc = rteTestData.second.pDutInTestData->StopCopies();
            rc = rteTestData.second.pDutOutTestData->StopCopies();
        }
        if (GetThermalThrottling())
        {
            rc = StopThermalThrottling();
        }
        return rc;
    };

    DEFERNAME(deferStopCopies)
    {
        stopCopies();
    };

    for (auto& rteTestData : m_TestData)
    {
        CHECK_RC(rteTestData.second.pDutInTestData->StartCopies());
        CHECK_RC(rteTestData.second.pDutOutTestData->StartCopies());
    }

    Pause(PAUSE_BEFORE_COPIES);
    CHECK_RC(TriggerAllCopies());

    if (m_bShowCopyProgress)
    {
        Printf(Tee::PriNormal, "LwLink : waiting for copies...");
        m_ProgressPrintTick = Xp::QueryPerformanceCounter();
    }

    if (GetInsertIdleAfterCopy())
    {
        CHECK_RC(ClearLPCounts());
    }

    DEFERNAME(stopPowerStateToggle)
    {
        StopPowerStateToggle();
    };

    if (GetPowerStateToggle())
    {
        CHECK_RC(StartPowerStateToggle(timeoutMs));
    }
    else
    {
        stopPowerStateToggle.Cancel();
    }

    DEFERNAME(stopThermalSlowdownToggle)
    {
        StopThermalSlowdownToggle();
    };

    if (GetThermalSlowdown())
    {
        CHECK_RC(StartThermalSlowdownToggle());
    }
    else
    {
        stopThermalSlowdownToggle.Cancel();
    }


    RC pollResult = WaitForCopies(timeoutMs);

    if (GetPowerStateToggle())
    {
        stopPowerStateToggle.Cancel();
        CHECK_RC(StopPowerStateToggle());
    }

    if (GetThermalSlowdown())
    {
        stopThermalSlowdownToggle.Cancel();
        CHECK_RC(StopThermalSlowdownToggle());
    }

    if (GetInsertIdleAfterCopy())
    {
        Platform::SleepUS(GetIdleAfterCopyMs() * 1000);
        SetSwLPEntryCount(GetSwLPEntryCount() + 1);
    }

    if (GetPowerStateToggle() || GetThermalSlowdown() || GetInsertIdleAfterCopy() || GetShowLowPowerCheck())
    {
        CHECK_RC(UpdateLPEntryOrExitCount(true, m_LastHwLPCounters));
    }

    if (GetInsertL2AfterCopy())
    {
        CHECK_RC(RequestSleepState(LwLinkSleepState::L2, true));
        if (m_GCxBubble)
        {
            CHECK_RC(m_GCxBubble->ActivateBubble(GetL2IntervalMs()));
        }
        else
        {
            Platform::SleepUS(GetL2IntervalMs() * 1000);
        }
        CHECK_RC(RequestSleepState(LwLinkSleepState::L0, true));
    }

    if (m_bShowCopyProgress)
    {
        if (pollResult == OK)
            Printf(Tee::PriNormal, "complete!\n");
        else
            Printf(Tee::PriNormal, "FAILED (%u)!\n", static_cast<UINT32>(pollResult));
    }
    CHECK_RC(pollResult);

    deferStopCopies.Cancel();
    CHECK_RC(stopCopies());

    Pause(PAUSE_AFTER_COPIES);

    for (auto& rteTestData : m_TestData)
    {
        CHECK_RC(rteTestData.second.pDutInTestData->UpdateCopyStats(m_CopyLoopCount));
        CHECK_RC(rteTestData.second.pDutOutTestData->UpdateCopyStats(m_CopyLoopCount));
    }

    SetDoOneCopyCount(GetDoOneCopyCount() + 1);

    return rc;
}

//-----------------------------------------------------------------------------
RC TestMode::GetConnectionEfficiency
(
    TestRoutePtr        pTestRoute,
    LwLinkConnectionPtr pCon,
    TestDevicePtr       pConSrcDev,
    FLOAT64             readDataFlits,
    FLOAT64             writeDataFlits,
    FLOAT64             readFlits,
    FLOAT64             writeFlits,
    FLOAT64 *           pPacketEfficiency
) const
{
    MASSERT(pPacketEfficiency);
    RC rc;

    *pPacketEfficiency   = 1.0;

    FLOAT64 totalWriteDataFlits = 0.0;
    FLOAT64 totalReadDataFlits = 0.0;
    FLOAT64 totalWriteFlits = 0.0;
    FLOAT64 totalReadFlits = 0.0;

    TestDevicePtr pFirstDev = pCon->GetDevices().first;
    TestDevicePtr pSecondDev = pCon->GetDevices().second;

    // Check this connection against every other route being tested to see if it overlaps
    for (auto routeIt : m_TestData)
    {
        if (!routeIt.first->GetRoute()->UsesConnection(pCon))
            continue;

        // If both test directions are in use then it is only necessary to check one
        // as it will correctly callwlate efficiency for the current route if both
        // test directions are in use and both of them are used the flits would
        // be counted twice
        //
        //
        // If only one test direction is in use it as the basis for the efficiency callwlation
        TestDevicePtr    pDataInOriginDev;
        LwLink::DataType inDataType = LwLink::DT_REQUEST;
        TestDevicePtr    pDataOutOriginDev;
        LwLink::DataType outDataType = LwLink::DT_REQUEST;

        auto pDutInTestData  = routeIt.second.pDutInTestData;
        auto pDutOutTestData = routeIt.second.pDutOutTestData;
        if (pDutInTestData->IsInUse())
        {
            inDataType = pDutInTestData->IsHwLocal() ? LwLink::DT_RESPONSE : LwLink::DT_REQUEST;
            pDataInOriginDev = (inDataType == LwLink::DT_REQUEST) ?
                routeIt.first->GetRemoteLwLinkDev() : routeIt.first->GetLocalLwLinkDev();
        }
        if (pDutOutTestData->IsInUse())
        {
            outDataType = pDutOutTestData->IsHwLocal() ? LwLink::DT_REQUEST : LwLink::DT_RESPONSE;
            pDataOutOriginDev = (outDataType == LwLink::DT_RESPONSE) ?
                routeIt.first->GetRemoteLwLinkDev() : routeIt.first->GetLocalLwLinkDev();
        }

        // Routes can use different connections depending on origin device and
        // data transfer type, when callwlating efficiency we only want to
        // consider connections that are actually in use for the current test
        // mode configuration.  Also since we are only callwlating based on one
        // direction (see above comment), ensure that the direction used is the
        // direction that actually contains the connection under consideration
        const bool bConUsedForDataIn =
            pDataInOriginDev && routeIt.first->GetRoute()->UsesConnection(pDataInOriginDev, pCon, inDataType);
        const bool bConUsedForDataOut =
            pDataOutOriginDev && routeIt.first->GetRoute()->UsesConnection(pDataOutOriginDev, pCon, outDataType);
        if (!bConUsedForDataIn && !bConUsedForDataOut)
            continue;

        LwLink::TransferType tt =
            GetTransferType(bConUsedForDataIn,
                            pDutInTestData->GetTransferHw(),
                            pDutInTestData->IsHwLocal(),
                            pDutOutTestData->GetTransferHw(),
                            pDutOutTestData->IsHwLocal());

        const bool bLoopback = (routeIt.first->GetTransferType() & TT_LOOPBACK);
        if (bLoopback && tt != LwLink::TT_UNIDIR_WRITE && tt != LwLink::TT_UNIDIR_READ)
        {
            Printf(Tee::PriError,
                   "%s : Loopback transfers must be either unidirectional write or read\n",
                   __FUNCTION__);
            return RC::SOFTWARE_ERROR;
        }

        // Non-posted writes require a write response packet
        FLOAT64 wrRspHdr = (pFirstDev->GetInterface<LwLink>()->HasNonPostedWrites() ||
                            pSecondDev->GetInterface<LwLink>()->HasNonPostedWrites())
                            ? 1.0 : 0.0;

        // Collapsed responses limit the effects of the write response
        if (pFirstDev->GetInterface<LwLink>()->HasCollapsedResponses() &&
            pSecondDev->GetInterface<LwLink>()->HasCollapsedResponses())
        {
            // While compression/collapsing can theoretically reach 64:1 in reality
            // we never achieve more than ~16:1
            wrRspHdr /= 16.0;
        }

        // Find the connection source device for the current route/test direction
        TestDevicePtr    pLwrRteConSrcDev;
        TestDevicePtr    pOriginDev =  bConUsedForDataIn ? pDataInOriginDev : pDataOutOriginDev;
        LwLink::DataType dt = bConUsedForDataIn ? inDataType : outDataType;
        auto pPaths = routeIt.first->GetRoute()->PathsBegin(pOriginDev, dt);
        auto pPathsEnd = routeIt.first->GetRoute()->PathsEnd(pOriginDev, dt);
        for (; (pPaths != pPathsEnd) && !pLwrRteConSrcDev; ++pPaths)
        {
            auto peIt = pPaths->PathEntryBegin(pOriginDev);
            auto peEnd = LwLinkPath::PathEntryIterator();
            for (; (peIt != peEnd) && !pLwrRteConSrcDev; ++peIt)
            {
                if (peIt->pCon == pCon)
                {
                    pLwrRteConSrcDev = peIt->pSrcDev;
                }
            }
        }
        switch (tt)
        {
            case LwLink::TT_READ_WRITE:
                // Simultaneous read/write from the same device
                // Write direction is WrHdr, RdReqHdr, Write Data Flits
                // Read direction is RdRspHdr + WrRspHdr(?) + Read Data Flits
                if (pLwrRteConSrcDev == pConSrcDev)
                {
                    totalWriteDataFlits += writeDataFlits;
                    totalReadDataFlits += readDataFlits;
                    totalWriteFlits += (writeFlits + 2.0);
                    totalReadFlits += (readFlits + 1.0 + wrRspHdr);
                }
                else
                {
                    // If this route's source of data on the connection doesn't equal the main
                    // route's source of data, then it's pointing the opposite direction and
                    // its write direction is this route's read direction and vice versa.
                    totalWriteDataFlits += readDataFlits;
                    totalReadDataFlits += writeDataFlits;
                    totalWriteFlits += (readFlits + 1.0 + wrRspHdr);
                    totalReadFlits += (writeFlits + 2.0);
                }
                break;
            case LwLink::TT_BIDIR_READ:
                // Write direction is RdReqHdr + RdRspHdr + Write Data Flits
                // Read direction is RdReqHdr + RdRspHdr + Read Data Flits
                totalWriteDataFlits += readDataFlits;
                totalReadDataFlits += readDataFlits;
                totalWriteFlits += (readFlits + 2.0);
                totalReadFlits += (readFlits + 2.0);
                break;
            case LwLink::TT_BIDIR_WRITE:
                // Write direction is WrReqHdr + WrRspHdr(?) + Write Data Flits
                // Read direction is WrReqHdr + WrRspHdr(?) + Read Data Flits
                totalWriteDataFlits += writeDataFlits;
                totalReadDataFlits += writeDataFlits;
                totalWriteFlits += (writeFlits + 1.0 + wrRspHdr);
                totalReadFlits += (writeFlits + 1.0 + wrRspHdr);
                break;
            case LwLink::TT_UNIDIR_READ:
                // Write direction is just a RdReqHdr packet
                // Read direction is RdRspHdr + Read Data Flits
                if (bLoopback || (pLwrRteConSrcDev == pConSrcDev))
                {
                    totalReadDataFlits += readDataFlits;
                    totalWriteFlits += 1.0;
                    totalReadFlits += (readFlits + 1.0);
                }
                // Loopback routes basically act like bidirectional routes
                // with traffic in both directions
                if (bLoopback || (pLwrRteConSrcDev != pConSrcDev))
                {
                    totalWriteDataFlits += readDataFlits;
                    totalWriteFlits += (readFlits + 1.0);
                    totalReadFlits += 1.0;
                }
                break;
            case LwLink::TT_UNIDIR_WRITE:
                // Write direciton is WrReqHdr + Write Data Flits
                // Read direction is just potentially a WrRspHdr packet
                if (bLoopback || (pLwrRteConSrcDev == pConSrcDev))
                {
                    totalWriteDataFlits += writeDataFlits;
                    totalWriteFlits += (writeFlits + 1.0);
                    totalReadFlits += wrRspHdr;
                }
                if (bLoopback || (pLwrRteConSrcDev != pConSrcDev))
                {
                    totalReadDataFlits += writeDataFlits;
                    totalWriteFlits += wrRspHdr;
                    totalReadFlits += (writeFlits + 1.0);
                }
                break;
            default:
                Printf(Tee::PriError, "%s : Unknown transfer type\n", __FUNCTION__);
                return RC::BAD_PARAMETER;
        }
    }

    // Ignore any directions that do not have any data flits.  It is possible through test
    // arguments to have data flowing only in the read or write direction, in fact this is the
    // default on NPU systems
    if ((totalWriteDataFlits == 0) && (totalReadDataFlits == 0))
    {
        Printf(Tee::PriError, "%s : Total read and write data flits both zero!\n", __FUNCTION__);
        return RC::BAD_PARAMETER;
    }
    else if (totalWriteDataFlits == 0)
    {
        *pPacketEfficiency = totalReadDataFlits / totalReadFlits;
    }
    else if (totalReadDataFlits == 0)
    {
        *pPacketEfficiency = totalWriteDataFlits / totalWriteFlits;
    }
    else
    {
        *pPacketEfficiency = min(totalWriteDataFlits / totalWriteFlits,
                                 totalReadDataFlits / totalReadFlits);
    }

    // The WAR for bug 2380136 on Turing requires that the non one-to-one LCE
    // to PCE configuration makes use of a LCE configured with one HSHUB PCE and
    // one FBHUB PCE.  The FBHUB PCE is limited to LwLink packets containing only
    // 8 data flits so when doing 16 flit transfers it will only generate half
    // the bandwidth of the HSHUB PCE.  Factor this limitation into the total
    // callwlated efficiency.  Note that Bug 2380136 (and MODS setting of the
    // bug) only oclwrs on TU102 where both links are connected to the same remote
    // device, so each PCE will contribute 50% to the actual efficiency
    if ((pFirstDev->HasBug(2380136) || pSecondDev->HasBug(2380136)) &&
        ((readDataFlits == 16.0) || (writeDataFlits == 16.0)))
    {
        const FLOAT64 readOverhead = readFlits - readDataFlits;
        const FLOAT64 writeOverhead = writeFlits - writeDataFlits;

        FLOAT64 fbHubReducedEfficiency;
        CHECK_RC(GetConnectionEfficiency(pTestRoute,
                                         pCon,
                                         pConSrcDev,
                                         8.0,
                                         8.0,
                                         8.0 + readOverhead,
                                         8.0 + writeOverhead,
                                         &fbHubReducedEfficiency));

        *pPacketEfficiency = (*pPacketEfficiency + fbHubReducedEfficiency) / 2.0;
    }


    return OK;
}

//------------------------------------------------------------------------------
//! \brief Returns LP entry or exit counters
//!
//! \param entry     : If true the function returns entry counters
//! \param pCounters : Output vector that gets all LP entry/exit counters
//!
//! \return OK if getting the counters was successful, not OK otherwise
RC TestMode::UpdateLPEntryOrExitCount
(
    bool entry,
    list<LPCounter>& counters
)
{
    RC rc;

    for (LPCounter& counter : counters)
    {
        TestDevicePtr pDev = counter.pDev;
        UINT32 linkId = counter.linkId;

        vector<LwLink::LinkStatus> linkStatus;
        CHECK_RC(pDev->GetInterface<LwLink>()->GetLinkStatus(&linkStatus));

        // This should exclude links that are intentionally not trained to
        // active and therefore can't report LP entry/exit, eg. TREX links.
        if (linkStatus[linkId].linkState == LwLink::LS_ACTIVE ||
            linkStatus[linkId].linkState == LwLink::LS_SLEEP)
        {
            auto pCounters = pDev->GetInterface<LwLinkPowerStateCounters>();
            UINT32 count = 0;
            bool bOverflow = false;
            CHECK_RC(pCounters->GetLPEntryOrExitCount(linkId, entry, &count, &bOverflow));
            CHECK_RC(pCounters->ClearLPCounts(linkId));

            counter.count += count;

            if (bOverflow)
            {
                counter.overflow = true;
            }
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Returns true if the test mode has any loopback routes
//!
//! \return true if the test mode has a loopback route
bool TestMode::HasLoopback()
{
    return ((GetTransferTypes() & TT_LOOPBACK) ||
            ((GetTransferTypes() & TT_SYSMEM) && m_bSysmemLoopback));
}

//------------------------------------------------------------------------------
//! \brief Initialize the test mode
//!
//! \param pRoutes    : List of all test routes on the DUT
//! \param routeMask  : Bit mask of routes to test for this test mode
//!
//! \return OK if initialization was successful, not OK otherwise
RC TestMode::Initialize
(
    const vector<TestRoutePtr> &routes
   ,UINT32 routeMask
)
{
    m_TransferTypes = 0;
    m_TestData.clear();
    for (size_t ii = 0; ii < routes.size(); ii++)
    {
        if (!(routeMask & (1 << ii)))
            continue;
        m_TestData[routes[ii]].pDutInTestData = CreateTestDirectionData();
        m_TestData[routes[ii]].pDutInTestData->SetInput(true);
        m_TestData[routes[ii]].pDutOutTestData = CreateTestDirectionData();
        m_TestData[routes[ii]].pDutOutTestData->SetInput(false);

        m_TransferTypes |= routes[ii]->GetTransferType();
    }
    return OK;
}

//------------------------------------------------------------------------------
//! \brief Returns true if the test mode has only loopback routes
//!
//! \return true if the test mode has only loopback route
bool TestMode::OnlyLoopback()
{
    if (GetTransferTypes() & TT_P2P)
        return false;
    if ((GetTransferTypes() & TT_SYSMEM) && !m_bSysmemLoopback)
        return false;
    return true;
}

//------------------------------------------------------------------------------
//! \brief Release resources on both the "dut" and any remote devices.  Note
//! that this only releases resource usage and does not actually free the
//! resources since the resources may be used in future test modes
//!
//! \return OK if successful, not OK if resource release failed
RC TestMode::ReleaseResources()
{
    StickyRC rc;

    if (-1 != m_PSToggleThreadId)
    {
        m_PSToggleThreadCont = false;
        rc = Tasker::Join(m_PSToggleThreadId, m_PSToggleTimeout);
        m_PSToggleThreadId = -1;
    }

    rc = StopPortStats();

    for (auto & rteTestData : m_TestData)
    {
        bool bAnyResourcesReleased = false;
        rc = rteTestData.second.pDutInTestData->Release(&bAnyResourcesReleased);
        rc = rteTestData.second.pDutOutTestData->Release(&bAnyResourcesReleased);
        if (bAnyResourcesReleased)
        {
            Printf(m_PrintPri,
                   "%s : Released resources for testing of test route:\n"
                   "%s\n",
                   __FUNCTION__,
                   rteTestData.first->GetString("    ").c_str());
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Request all data transfers to switch to either low power (LP) or full
//! bandwidth mode (FB)
//!
//! \return OK if successful, not OK if resource release failed
RC TestMode::RequestPowerState
(
    LwLinkPowerState::SubLinkPowerState state
   ,bool bWaitState
)
{
    RC rc;

    CHECK_RC(ForEachLink(
        [state](TestDevicePtr dev, UINT32 linkId)->RC
        {
            if (!dev->SupportsInterface<LwLinkPowerState>())
                return OK;
            auto powerState = dev->GetInterface<LwLinkPowerState>();
            return powerState->RequestPowerState(linkId, state, false);
        },
        nullptr
    ));

    if (bWaitState)
    {
        CHECK_RC(ForEachLink(
            [](TestDevicePtr dev, UINT32 linkId)->RC
            {
                if (!dev->SupportsInterface<LwLinkPowerState>())
                    return OK;

                return Tasker::PollHw(1000, [&]() -> bool
                {
                    auto powerState = dev->GetInterface<LwLinkPowerState>();
                    LwLinkPowerState::LinkPowerStateStatus ps;
                    powerState->GetLinkPowerStateStatus(linkId, &ps);
                    return ps.rxSubLinkLwrrentPowerState == LwLinkPowerState::SLS_PWRM_FB &&
                           ps.txSubLinkLwrrentPowerState == LwLinkPowerState::SLS_PWRM_FB;
                }, __FUNCTION__);
            },
            nullptr
        ));
    }

    return rc;
}

//------------------------------------------------------------------------------
RC TestMode::RequestSleepState
(
    LwLinkSleepState::SleepState state
  , bool waitForState
)
{
    using namespace boost::multi_index;

    struct LinkToSet
    {
        LinkToSet(LwLinkSleepState *sleepStateIntf, UINT32 linkId)
          : sleepStateIntf(sleepStateIntf)
          , linkId(linkId)
        {}
        LwLinkSleepState *sleepStateIntf = nullptr;
        UINT32            linkId = 0;
    };

    typedef multi_index_container<
        LinkToSet
      , indexed_by<
            ordered_unique< // this container will have unique pairs {sleepStateIntf, linkId}
                composite_key<
                    LinkToSet
                  , member<LinkToSet, LwLinkSleepState *, &LinkToSet::sleepStateIntf>
                  , member<LinkToSet, UINT32, &LinkToSet::linkId>
                  >
              >
          , ordered_non_unique< // index to extract all links for every interface
                member<LinkToSet, LwLinkSleepState *, &LinkToSet::sleepStateIntf>
              >
          >
      > LinksToSet;

    RC rc;

    LinksToSet linksToSet;

    // First extract all pairs {LwLinkSleepState *, link id}
    CHECK_RC(ForEachLink(
        [&linksToSet, state](TestDevicePtr dev, UINT32 linkId)->RC
        {
            if (!dev->SupportsInterface<LwLinkSleepState>())
                return OK;
            auto sleepState = dev->GetInterface<LwLinkSleepState>();
            linksToSet.insert({sleepState, linkId});
            return OK;
        },
        [&linksToSet, state](TestDevicePtr dev, UINT32 linkId)->RC
        {
            if (!dev->SupportsInterface<LwLinkSleepState>())
                return OK;
            auto sleepState = dev->GetInterface<LwLinkSleepState>();
            linksToSet.insert({sleepState, linkId});
            return OK;
        }
    ));

    // Consolidate links by device and callwlate masks
    vector<tuple<LwLinkSleepState *, UINT64>> linkMasks;
    auto &intfIdx = linksToSet.get<1>();
    for (auto intfIt = intfIdx.begin();
         intfIdx.end() != intfIt;
         intfIt = intfIdx.upper_bound(intfIt->sleepStateIntf))
    {
        auto links = boost::make_iterator_range(intfIdx.equal_range(intfIt->sleepStateIntf));
        UINT64 mask = 0;
        for (const auto &l : links)
        {
            mask |= 1ULL << l.linkId;
        }
        linkMasks.emplace_back(intfIt->sleepStateIntf, mask);
    }

    // Request sleep state
    for (const auto &l : linkMasks)
    {
        rc = get<0>(l)->Set(get<1>(l), state);
        // Do not consider LWRM_MORE_PROCESSING_REQUIRED an error. RM returns
        // it until we send requests to all links.
        if (!(RC::LWRM_MORE_PROCESSING_REQUIRED == rc || OK == rc))
        {
            return rc;
        }
    }
    // If the last request also returned LWRM_MORE_PROCESSING_REQUIRED, return
    // it as an error.
    if (RC::LWRM_MORE_PROCESSING_REQUIRED == rc)
    {
        return rc;
    }

    if (waitForState)
    {
        auto &intfIdx = linksToSet.get<1>();
        // iterate over devices
        for (auto intfIt = intfIdx.begin();
             intfIdx.end() != intfIt;
             intfIt = intfIdx.upper_bound(intfIt->sleepStateIntf))
        {
            // all links that belong to intfIdx
            auto links = boost::make_iterator_range(intfIdx.equal_range(intfIt->sleepStateIntf));
            for (const auto &l : links)
            {
                Tasker::PollHw(1000, [&]() -> bool
                {
                    LwLinkSleepState::SleepState ss;
                    intfIt->sleepStateIntf->Get(l.linkId, &ss);
                    return ss == state;
                }, __FUNCTION__);
            }
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
/*static*/ void TestMode::RunCpuTrafficThread(void* args)
{
    Tasker::DetachThread detachThread;

    auto* pBgCopyData = static_cast<TestDirectionData::BgCopyData*>(args);
    Tasker::SetCpuAffinity(pBgCopyData->threadID, pBgCopyData->localCpus);

    const auto srcAddr = static_cast<const UINT32*>(pBgCopyData->pSrcAddr);
    const auto dstAddr = static_cast<UINT32*>(pBgCopyData->pDstAddr);

    UINT64 startTime = Xp::QueryPerformanceCounter();
    while (Cpu::AtomicRead(&pBgCopyData->running))
    {
        for (UINT64 offset = 0;
             offset < (pBgCopyData->pSrcSurf->GetSize()/sizeof(UINT32))
                 && offset < (pBgCopyData->pDstSurf->GetSize()/sizeof(UINT32))
                 && Cpu::AtomicRead(&pBgCopyData->running);
             offset++)
        {
            MEM_WR32(dstAddr + offset, MEM_RD32(srcAddr + offset));
            pBgCopyData->pDstSurf->FlushCpuCache();
            pBgCopyData->pSrcSurf->FlushCpuCache();
            pBgCopyData->totalBytes += sizeof(UINT32);
        }
    }
    UINT64 endTime = Xp::QueryPerformanceCounter();

    UINT64 deltaTime = endTime - startTime;

    // make sure deltaTime is in nanoseconds
    deltaTime *= (1000000000ULL / Xp::QueryPerformanceFrequency());

    pBgCopyData->totalTimeNs = deltaTime;
}

//------------------------------------------------------------------------------
void TestMode::SetLatencyBins(UINT32 low, UINT32 mid, UINT32 high)
{
    m_LatencyBins.lowThresholdNs  = low;
    m_LatencyBins.midThresholdNs  = mid;
    m_LatencyBins.highThresholdNs = high;
}

//------------------------------------------------------------------------------
RC TestMode::ShowPortStats()
{
    StickyRC rc;
    map<TestDevicePtr, UINT64> testedLinks;

    CHECK_RC(ForEachLink(
        [this, &testedLinks](TestDevicePtr pDev1, UINT32 linkId1, TestDevicePtr pDev2, UINT32 linkId2)->RC
        {
            if (pDev1->SupportsInterface<LwLinkLatencyBinCounters>() ||
                (this->AllowThroughputCounterPortStats() &&
                 pDev1->SupportsInterface<LwLinkThroughputCounters>()))
            {
                if (!testedLinks.count(pDev1))
                    testedLinks[pDev1] = 0;
                testedLinks[pDev1] |= (1ULL << linkId1);
            }
            if (pDev2->SupportsInterface<LwLinkLatencyBinCounters>() ||
                (this->AllowThroughputCounterPortStats() &&
                 pDev2->SupportsInterface<LwLinkThroughputCounters>()))
            {
                if (!testedLinks.count(pDev2))
                    testedLinks[pDev2] = 0;
                testedLinks[pDev2] |= (1ULL << linkId2);
            }
            return OK;
        },
        nullptr
    ));

    for (auto const & lwrDevLinks : testedLinks)
    {
        if (lwrDevLinks.first->SupportsInterface<LwLinkLatencyBinCounters>())
        {
            LwLinkLatencyBinCounters * pLatencyCounter =
                lwrDevLinks.first->GetInterface<LwLinkLatencyBinCounters>();
            vector<vector<LwLinkLatencyBinCounters::LatencyCounts>> latencyCounts;
            rc = pLatencyCounter->GetLatencyBinCounts(&latencyCounts);
            if (rc != OK)
            {
                Printf(Tee::PriError, "Failed to get link latency bin counters on %s\n",
                       lwrDevLinks.first->GetName().c_str());
                break;
            }

            Printf(Tee::PriNormal, "%s latency counters :\n",
                   lwrDevLinks.first->GetName().c_str());
            for (auto const & lwrVcCounts : latencyCounts)
            {
                bool bVcPrinted = false;
                for (size_t lwrPort = 0; lwrPort < lwrVcCounts.size(); lwrPort++)
                {
                    if (!(lwrDevLinks.second & (1 << lwrPort)))
                        continue;

                    if (!bVcPrinted)
                    {
                        Printf(Tee::PriNormal, "  Virtual Channel %u :\n",
                               lwrVcCounts[lwrPort].virtualChannel);
                        bVcPrinted = true;
                    }
                    Printf(Tee::PriNormal, "    Link %u :\n"
                                           "      Elapsed Time (ms) : %llu\n"
                                           "      Low               : %llu\n"
                                           "      Mid               : %llu\n"
                                           "      High              : %llu\n"
                                           "      Panic             : %llu\n",
                           static_cast<UINT32>(lwrPort),
                           lwrVcCounts[lwrPort].elapsedTimeMs,
                           lwrVcCounts[lwrPort].low,
                           lwrVcCounts[lwrPort].mid,
                           lwrVcCounts[lwrPort].high,
                           lwrVcCounts[lwrPort].panic);
                }
            }
        }

        if (AllowThroughputCounterPortStats() &&
            lwrDevLinks.first->SupportsInterface<LwLinkThroughputCounters>())
        {
            map<UINT32, vector<LwLinkThroughputCount>> counts;
            auto pTpc = lwrDevLinks.first->GetInterface<LwLinkThroughputCounters>();
            rc = pTpc->GetThroughputCounters(lwrDevLinks.second, &counts);
            if (rc != OK)
            {
                Printf(Tee::PriError, "Failed to get throughput counters on %s\n",
                       lwrDevLinks.first->GetName().c_str());
                break;
            }
            Printf(Tee::PriNormal, "%s throughput counters :\n",
                   lwrDevLinks.first->GetName().c_str());
            for (auto const &lwrLinkCounts : counts)
            {
                Printf(Tee::PriNormal, "  Link %u :\n", lwrLinkCounts.first);

                for (auto const &lwrCount : lwrLinkCounts.second)
                {
                    Printf(Tee::PriNormal, "%-15s : %llu\n",
                           LwLinkThroughputCount::GetString(lwrCount.config, "    ").c_str(),
                           lwrCount.countValue);
                }
            }
        }
    }
    return rc;
}

//------------------------------------------------------------------------------
RC TestMode::CheckPortStats()
{
    return RC::OK;
}

//------------------------------------------------------------------------------
RC TestMode::StartPortStats()
{
    set<TestDevicePtr> devicesSetup;
    return ForEachDevice(
        [this, &devicesSetup] (TestDevicePtr pDev) -> RC
        {
            if (devicesSetup.count(pDev))
                return OK;
            devicesSetup.insert(pDev);

            RC rc;
            if (pDev->SupportsInterface<LwLinkLatencyBinCounters>())
            {
                LwLinkLatencyBinCounters * pLatencyCounter =
                    pDev->GetInterface<LwLinkLatencyBinCounters>();
                CHECK_RC(pLatencyCounter->SetupLatencyBins(m_LatencyBins));
                CHECK_RC(pLatencyCounter->ClearLatencyBinCounts());
            }

            if (this->AllowThroughputCounterPortStats() &&
                pDev->SupportsInterface<LwLinkThroughputCounters>())
            {
                UINT64 linkMask = 0;
                auto pTpc = pDev->GetInterface<LwLinkThroughputCounters>();
                auto pLwLink = pDev->GetInterface<LwLink>();
                for (UINT32 lwrLink = 0; lwrLink < pLwLink->GetMaxLinks(); lwrLink++)
                    linkMask |= pLwLink->IsLinkActive(lwrLink) ? (1ULL << lwrLink) : 0;

                CHECK_RC(pTpc->SetupThroughputCounters(linkMask, s_TpcConfig));
                CHECK_RC(pTpc->StartThroughputCounters(linkMask));
                CHECK_RC(pTpc->ClearThroughputCounters(linkMask));
            }
            return rc;
        }
    );
}

//------------------------------------------------------------------------------
RC TestMode::StopPortStats()
{
    return ForEachDevice(
        [this] (TestDevicePtr pDev) -> RC
        {
            RC rc;
            if (this->AllowThroughputCounterPortStats() &&
                pDev->SupportsInterface<LwLinkThroughputCounters>())
            {
                UINT64 linkMask = 0;
                auto pLwLink = pDev->GetInterface<LwLink>();
                auto pTpc = pDev->GetInterface<LwLinkThroughputCounters>();
                for (UINT32 lwrLink = 0; lwrLink < pLwLink->GetMaxLinks(); lwrLink++)
                    linkMask |= pLwLink->IsLinkActive(lwrLink) ? (1ULL << lwrLink) : 0;

                if (pTpc->AreThroughputCountersSetup(linkMask))
                {
                    CHECK_RC(pTpc->StopThroughputCounters(linkMask));
                }
            }
            return rc;
        }
    );
}

//------------------------------------------------------------------------------
// TestMode protected implemenation
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//! \brief Reset test data assignments to a default state
void TestMode::ResetTestData()
{
    for (auto & testEntry : m_TestData)
    {
        testEntry.second.pDutInTestData = CreateTestDirectionData();
        testEntry.second.pDutInTestData->SetInput(true);
        testEntry.second.pDutOutTestData = CreateTestDirectionData();
        testEntry.second.pDutOutTestData->SetInput(false);
    }
}

//------------------------------------------------------------------------------
// TestMode private implemenation
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//! \brief Check the bandwidth for the specified test route/direction
//!
//! \param pRoute     : Pointer to the test route
//! \param pTestDir : Pointer to the test direction
//!
//! \return OK if verification was successful, not OK otherwise
RC TestMode::CheckBandwidth
(
    TestRoutePtr pRoute
   ,TestDirectionDataPtr pTestDir
   ,LwLinkImpl::HwType otherDirHw
   ,bool bOtherHwLocal
   ,vector<TestMode::BandwidthInfo>* pBwFailureInfos
)
{
    if (!pTestDir->IsInUse())
        return OK;

    string remDevStr = pRoute->GetRemoteDevString() + ", Link(s) " + pRoute->GetLinkString(true);
    if ((pRoute->GetTransferType() & TT_SYSMEM) &&
        (m_bSysmemLoopback || (pRoute->GetTransferType() & TT_LOOPBACK)))
    {
        remDevStr += " (Loopback)";
    }

    FLOAT64 actualBwKiBps = static_cast<FLOAT64>(pTestDir->GetTotalBytes() * 1000ULL) /
                          pTestDir->GetTotalTimeNs();

    // Colwert to KiBps
    actualBwKiBps *= 1000ULL * 1000ULL / 1024ULL;

    LwLink::TransferType tt;
    if (otherDirHw == LwLinkImpl::HT_NONE)
    {
        tt = (pTestDir->IsInput() == pTestDir->IsHwLocal()) ? LwLink::TT_UNIDIR_READ :
            LwLink::TT_UNIDIR_WRITE;
    }
    else if (bOtherHwLocal == pTestDir->IsHwLocal())
    {
        tt = LwLink::TT_READ_WRITE;
    }
    else
    {
        tt = (pTestDir->IsInput() == pTestDir->IsHwLocal()) ? LwLink::TT_BIDIR_READ :
                                                              LwLink::TT_BIDIR_WRITE;
    }

    FLOAT64 packetEfficiency   = pTestDir->GetPacketEfficiency();
    FLOAT64 theoreticalBwKiBps = packetEfficiency * pTestDir->GetMaxDataBwKiBps();
    FLOAT64 bwThresholdPct = min(m_BwThresholdPct, pRoute->GetMaxObservableBwPct(tt));

    FLOAT64 expectedBwKiBps = theoreticalBwKiBps;
    if (GetMakeLowPowerCopies())
    {
        expectedBwKiBps /= pRoute->GetSublinkWidth();
    }
    else if (GetThermalSlowdown())
    {
        expectedBwKiBps *= ((1.0/pRoute->GetSublinkWidth()) + THERMAL_SLOWDOWN_GUARD_PCT) / (1.0 + THERMAL_SLOWDOWN_GUARD_PCT);
    }

    const bool thermalThrottling = GetThermalThrottling() && ContainsLwSwitch(*pRoute, *pTestDir);
    if (thermalThrottling)
    {
        const FLOAT64 onCount  = static_cast<FLOAT64>(GetThermalThrottlingOnCount());
        const FLOAT64 offCount = static_cast<FLOAT64>(GetThermalThrottlingOffCount());
        expectedBwKiBps *= (1.0 - (onCount/(onCount + offCount)));
        MASSERT(bwThresholdPct < 1.0);
    }

    const TransferDir td = (pRoute->GetTransferType() & TT_SYSMEM) ?
        m_SysmemTransferDir :
        m_TransferDir;
    string bwStr = Utility::StrPrintf("Testing LwLink on %s, Link(s) %s\n",
                                      pRoute->GetLocalDevString().c_str(),
                                      pRoute->GetLinkString(false).c_str());
    bwStr += "---------------------------------------------------\n";
    bwStr += Utility::StrPrintf("  Remote Device                  = %s\n",
                                remDevStr.c_str());
    bwStr += Utility::StrPrintf("  Transfer Method                = %s%s\n",
                                (pRoute->GetTransferType() & TT_LOOPBACK) ? "Loopback " : "",
                                LwLink::TransferTypeString(tt).c_str());
    if ((tt == LwLink::TT_UNIDIR_READ) ||
        (tt == LwLink::TT_UNIDIR_READ) ||
        (tt == LwLink::TT_UNIDIR_WRITE))
    {
        bwStr += Utility::StrPrintf("  Transfer Source                = %s\n",
                                    pTestDir->IsHwLocal() ? pRoute->GetLocalDevString().c_str() :
                                                            pRoute->GetRemoteDevString().c_str());
    }
    else
    {
        bwStr += Utility::StrPrintf("  Transfer Sources               = %s, %s\n",
                                    pRoute->GetLocalDevString().c_str(),
                                    pRoute->GetRemoteDevString().c_str());
    }
    bwStr += Utility::StrPrintf("  Transfer Direction             = %s\n",
                                TransferDirStr(td).c_str());
    bwStr += Utility::StrPrintf("  Transfer Width                 = %u\n",
                                pTestDir->GetTransferWidth());
    bwStr += Utility::StrPrintf("  Measurement Direction          = %c\n",
                                pTestDir->IsInput() ? 'I' : 'O');
    if (pTestDir->AreBytesCopied())
    {
        const UINT64 bytesPerCopy = pTestDir->GetBytesPerLoop() * m_CopyLoopCount;
        bwStr += Utility::StrPrintf("  Bytes Per Copy                 = %lld\n",
                                    bytesPerCopy);
    }
    bwStr += Utility::StrPrintf("  Total Bytes Transferred        = %lld\n",
                                pTestDir->GetTotalBytes());
    bwStr += Utility::StrPrintf("  Elapsed Time                   = %8.3f ms\n",
                                static_cast<FLOAT64>(pTestDir->GetTotalTimeNs()) / 1000000.0);
    bwStr += Utility::StrPrintf("  Raw Bandwidth                  = %u KiBps\n",
                                pRoute->GetRawBandwidthKiBps());
    bwStr += Utility::StrPrintf("  Maximum Data Bandwidth         = %u KiBps\n",
                                pTestDir->GetMaxDataBwKiBps());
    bwStr += Utility::StrPrintf("  Packet Overhead Efficiency     = %5.3f%%\n",
                                packetEfficiency * 100.0);
    bwStr += Utility::StrPrintf("  Theoretical Bandwidth          = %u KiBps\n",
                                static_cast<UINT32>(theoreticalBwKiBps));

    if (theoreticalBwKiBps != expectedBwKiBps && !thermalThrottling)
    {
        bwStr += Utility::StrPrintf("  Expected Bandwidth             = %u KiBps\n",
                                    static_cast<UINT32>(expectedBwKiBps));
    }
    else if (thermalThrottling)
    {
        bwStr += Utility::StrPrintf("  Expected Maximum Bandwidth     = %u KiBps\n",
                                    static_cast<UINT32>(expectedBwKiBps));
    }

    if (bwThresholdPct != m_BwThresholdPct)
    {
        bwStr += Utility::StrPrintf("  Bandwidth Threshold (original) = %3.1f%%\n",
                                    m_BwThresholdPct * 100.0);
        bwStr += Utility::StrPrintf("  Bandwidth Threshold (adjusted) = %3.1f%%\n",
                                    bwThresholdPct * 100.0);
    }
    else
    {
        bwStr += Utility::StrPrintf("  Bandwidth Threshold            = %3.1f%%\n",
                                    bwThresholdPct * 100.0);
    }

    bool bBandwidthXbarLimited = false;
    UINT32 minimumBw = (expectedBwKiBps * bwThresholdPct);
    if (!thermalThrottling)
    {
        const UINT32 xbarBwKiBps = pTestDir->GetXbarBwKiBps();
        const UINT32 minXbarBwKiBps = xbarBwKiBps * bwThresholdPct;
        if ((xbarBwKiBps != 0) && (minXbarBwKiBps < minimumBw))
        {
            bwStr += Utility::StrPrintf("  Minimum LwLink Bandwidth       = %u KiBps\n",
                                        minimumBw);
            bwStr += Utility::StrPrintf("  Available XBAR Bandwidth       = %u KiBps\n",
                                        xbarBwKiBps);
            bwStr += Utility::StrPrintf("  Minimum XBAR Bandwidth         = %u KiBps\n",
                                        minXbarBwKiBps);
            theoreticalBwKiBps = expectedBwKiBps = xbarBwKiBps;
            minimumBw = minXbarBwKiBps;
            bBandwidthXbarLimited = true;
        }

        bwStr += Utility::StrPrintf("  Minimum Bandwidth              = %u KiBps\n", minimumBw);
    }


    bwStr += Utility::StrPrintf("  Actual Bandwidth               = %u KiBps\n",
                                static_cast<UINT32>(actualBwKiBps));
    bwStr += Utility::StrPrintf("  Percent Theoretical Bandwidth  = %3.1f%%\n",
                                (actualBwKiBps * 100.0) / theoreticalBwKiBps);
    const FLOAT64 percentExpectedBw = (actualBwKiBps * 100.0) / expectedBwKiBps;
    if (theoreticalBwKiBps != expectedBwKiBps && !thermalThrottling)
    {
        bwStr += Utility::StrPrintf("  Percent Expected Bandwidth     = %3.1f%%\n",
                                    percentExpectedBw);
    }

    // With low power copies, bandwidth can be slightly over 100 allow a 2% margin
    FLOAT64 maximumBw = 0;
    if (thermalThrottling)
    {
        maximumBw = expectedBwKiBps * (2.0 - bwThresholdPct);
    }
    else if (GetMakeLowPowerCopies())
    {
        maximumBw = expectedBwKiBps * 1.02;
    }
    else if (bBandwidthXbarLimited)
    {
        // XBAR bandwidth has a built in 10% ARB inefficiency that may or may not
        // occur so theoretically when XBAR limitied if the ARB inefficiency was not
        // actually present, then bandwidth could be as high as 112%
        //
        // In addition since there is already a 10% factor build into the XBAR bandwidth
        // callwlation, the minimum should be at least the expected
        maximumBw = expectedBwKiBps * 1.12;
    }
    else
    {
        // Under normal condidtions, the tolerance for lwlink refclk can result in a +/- 100PPM
        // deviation which needs to be taken into account when checking whether the bandwidth
        // is high.
        maximumBw = expectedBwKiBps * (1.0 + (100.0 / 1.0e6));
    }
    const bool bBwHigh = actualBwKiBps > maximumBw;
    const bool bBwLow  = !thermalThrottling && (actualBwKiBps < minimumBw);
    const bool bFailedBw = bBwHigh || bBwLow;
    const Tee::Priority bwSummaryPri =
        (m_bShowBandwidthStats || bFailedBw || bBandwidthXbarLimited) ?
            Tee::PriNormal : m_PrintPri;

    if (bFailedBw)
    {
        if (pRoute->GetTransferType() & TT_SYSMEM)
        {
            bwStr +=
                "******* System memory bandwidth may be limited by CPU memory bandwidth *******\n"
                "*******     Consult LWPU AE Team about expected memory bandwidth     *******\n";
        }
        else
        {
            if (bBwHigh)
                bwStr += "******* Bandwidth too high! *******\n";
            else // bBwLow
            {
                bwStr += "******* Bandwidth too low! *******\n";
            }
        }
        if (pTestDir->AreBytesCopied())
        {
            const UINT64 bytesPerCopy = pTestDir->GetBytesPerLoop() * m_CopyLoopCount;
            if (bytesPerCopy < GetMinimumBytesPerCopy(pRoute->GetMaxBandwidthKiBps()))
            {
                bwStr +=
                    Utility::StrPrintf("\n******* Number of bytes per copy not enough to overcome startup latency *******\n" //$
                                       "*******          Mimimum number of bytes per copy is %-10llu         *******\n"      //$
                                       "*******                Please adjust your test parameters               *******\n",  //$
                                       GetMinimumBytesPerCopy(pRoute->GetMaxBandwidthKiBps()));
            }
        }
        bwStr += GetBandwidthFailStringImpl(pRoute, pTestDir);
        bwStr += pTestDir->GetBandwidthFailString();
    }

    if (bBandwidthXbarLimited)
        bwStr += "\n******* Minimum bandwidth limited by GPU XBAR Bandwidth! *******\n";

    Printf(bwSummaryPri, "%s\n", bwStr.c_str());

    RC rc = (bFailedBw &&
            (!(pRoute->GetTransferType() & TT_SYSMEM) || m_bFailOnSysmemBw)) ?
                RC::BANDWIDTH_OUT_OF_RANGE : RC::OK;

    if (rc == RC::BANDWIDTH_OUT_OF_RANGE)
    {
        BandwidthInfo bwInfo = BandwidthInfo
        {
            static_cast<INT32>(pRoute->GetLocalLwLinkDev()->DevInst()),
            static_cast<INT32>(pRoute->GetRemoteLwLinkDev()->DevInst()),
            pRoute->GetLinkMask(false),
            pRoute->GetLinkMask(true),
            tt,
            static_cast<FLOAT32>(actualBwKiBps),
            bBwLow ? static_cast<FLOAT32>(minimumBw) : static_cast<FLOAT32>(maximumBw),
            bBwLow
        };
        pBwFailureInfos->push_back(bwInfo);
    }

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Pause test exelwtion for the following point
//!
//! \param pausePoint : Point to pause for
//!
void TestMode::Pause(UINT32 pausePoint)
{
    if (!(m_PauseMask & pausePoint))
        return;

    string pauseMsg = "********************************\n"
                      "* Test paused ";
    switch (pausePoint)
    {
        case PAUSE_BEFORE_COPIES:
            pauseMsg += "BEFORE copies\n";
            break;
        case PAUSE_AFTER_COPIES:
            pauseMsg += "AFTER copies\n";
            break;
        default:
            pauseMsg += "for an unknown reason\n";
            break;
    }

    pauseMsg += "* Menu:\n"
                "*     d) Delete this pause point\n"
                "*     D) Delete all pause points\n*\n"
                "* Any other key continues test\n ";
    Printf(Tee::PriNormal, "%s", pauseMsg.c_str());
    Tee::TeeFlushQueuedPrints(m_pTestConfig->TimeoutMs());

    ConsoleManager::ConsoleContext consoleCtx;
    Console *pConsole = consoleCtx.AcquireRealConsole(true);

    while (!pConsole->KeyboardHit())
        Tasker::Sleep(100);
    Console::VirtualKey key = pConsole->GetKey();

    switch (key)
    {
        case Console::VKC_CAPITAL_D:
            m_PauseMask = 0;
            break;
        case Console::VKC_LOWER_D:
            m_PauseMask &= ~pausePoint;
            break;
        default:
            break;
    }
}

//------------------------------------------------------------------------------
void TestMode::PowerStateToggleThread()
{
    Tasker::DetachThread detachThread;

    auto lwrState = LwLinkPowerState::SLS_PWRM_FB;

    const RC rc = RequestPowerState(lwrState, false);
    if (rc != RC::OK)
    {
        Printf(Tee::PriError,
               "Error LwLinkBwStress power state toggle thread - %s, exiting\n",
               rc.Message());
        return;
    }

    m_PSToggleStartTime = Xp::GetWallTimeUS();
    UINT64 prevTimeUs   = m_PSToggleStartTime;
    size_t count        = 0;

    while (m_PSToggleThreadCont)
    {
        lwrState = LwLinkPowerState::SLS_PWRM_FB == lwrState ?
            LwLinkPowerState::SLS_PWRM_LP : LwLinkPowerState::SLS_PWRM_FB;

        const RC rc = RequestPowerState(lwrState, false);
        if (rc != RC::OK)
        {
            Printf(Tee::PriError,
                   "Error LwLinkBwStress power state toggle thread - %s, exiting\n",
                   rc.Message());
            return;
        }
        ++count;
        if (LwLinkPowerState::SLS_PWRM_LP == lwrState)
        {
            SetSwLPEntryCount(GetSwLPEntryCount() + 1);
        }

        if (!m_PSToggleThreadCont)
            break;

        if (Platform::Hardware != Platform::GetSimulationMode())
        {
            // Platform::SleepUS will do practically the same, but it's not
            // interruptible. Since waiting for simulated ticks can take
            // considerable time, if we don't interrupt the sleep,
            // StopPowerStateToggle will timeout waiting for us to wake.
            UINT64 now = Platform::GetSimulatorTime();
            double nsPerTick = Platform::GetSimulatorTimeUnitsNS();
            double usPerTick = nsPerTick / 1000.0;
            double nowInUsec = now * usPerTick;
            double thenInUsec = nowInUsec + m_PSToggleIntervalUs;

            do
            {
                Tasker::Yield();

                now = Platform::GetSimulatorTime();
                nowInUsec = now * usPerTick;
            } while (m_PSToggleThreadCont && nowInUsec < thenInUsec);

            ++prevTimeUs;
        }
        else
        {
            UINT64 nextTimeUs = prevTimeUs + m_PSToggleIntervalUs;
            const UINT64 lwrTimeUs = Xp::GetWallTimeUS();
            if (lwrTimeUs < nextTimeUs)
            {
                const UINT64 sleepTimeUs = nextTimeUs - lwrTimeUs;
                Tasker::Sleep(sleepTimeUs * 0.001);
            }
            else
            {
                // When toggle interval is too short, the flipping of the
                // power state takes longer and we can never catch up.
                const UINT64 overshotUs = lwrTimeUs - nextTimeUs;
                if (overshotUs > m_PSToggleIntervalUs)
                {
                    nextTimeUs = lwrTimeUs;
                }
            }
            prevTimeUs = nextTimeUs;
        }
    }

    const UINT64 durationUs = Xp::GetWallTimeUS() - m_PSToggleStartTime;
    const double switchFreqkHz = (1000.0 * count) / durationUs;
    Printf(m_PrintPri,
           "Power state switch frequency between FB and LP was "
           "%.3f kHz, total %zu switches in %llu us\n",
           switchFreqkHz, count, durationUs);

    // Restore full bandwidth state
    if (lwrState != LwLinkPowerState::SLS_PWRM_FB)
    {
        const RC rc = RequestPowerState(LwLinkPowerState::SLS_PWRM_FB, false);
        if (rc != RC::OK)
        {
            Printf(Tee::PriError,
                   "Error LwLinkBwStress power state toggle thread - %s during exit\n",
                   rc.Message());
        }
    }
}

//------------------------------------------------------------------------------
//! \brief Creates and starts a thread that with some frequency requests LWLink
//! sublinks to be in either low power (LP) or full bandwidth (FB) power states.
//!
//! \return OK if successful, not OK if resource release failed
RC TestMode::StartPowerStateToggle(FLOAT64 timeoutMs)
{
    RC rc;

    bool bHwPowerToggle = false;
    CHECK_RC(ForEachLink(
        [&](TestDevicePtr pDev, UINT32 linkId)->RC
        {
            if (!pDev->SupportsInterface<LwLinkPowerState>())
                return RC::OK;
            auto powerState = pDev->GetInterface<LwLinkPowerState>();
            if (!powerState->SupportsLpHwToggle())
                return RC::OK;
            bHwPowerToggle = true;
            return powerState->StartPowerStateToggle(linkId, m_PSToggleIntervalUs, m_PSToggleIntervalUs);
        },
        nullptr
    ));
    m_PSToggleStartTime = Xp::GetWallTimeUS();

    if (!bHwPowerToggle)
    {
        if (-1 != m_PSToggleThreadId)
        {
            return OK;
        }

        m_PSToggleTimeout = timeoutMs;
        m_PSToggleThreadCont = true;
        m_PSToggleThreadId = Tasker::CreateThread(
            PowerStateToggleThreadDispatch,
            this,
            Tasker::MIN_STACK_SIZE,
            "LwLink power state toggle"
        );

        if (-1 == m_PSToggleThreadId)
        {
            Printf(Tee::PriError, "Failed to create LwLink power state toggle thread\n");
            return RC::SOFTWARE_ERROR;
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
RC TestMode::StartThermalThrottling()
{
    RC rc;

    CHECK_RC(ForEachDevice(
        [this] (TestDevicePtr dev) -> RC
        {
            RC rc;

            auto pTT = dev->GetInterface<ThermalThrottling>();
            if (!pTT)
                return OK;

            CHECK_RC(pTT->StartThermalThrottling(
                GetThermalThrottlingOnCount(),
                GetThermalThrottlingOffCount()
            ));

            return rc;
        }
    ));

    return rc;
}

//------------------------------------------------------------------------------
RC TestMode::StartThermalSlowdownToggle()
{
    RC rc;

    ForEachDevice([&](TestDevicePtr pDev) -> RC
    {
        RC rc;
        if (!pDev->SupportsInterface<ThermalSlowdown>())
            return RC::OK;
        ThermalSlowdown* pSlowdown = pDev->GetInterface<ThermalSlowdown>();
        CHECK_RC(pSlowdown->StartThermalSlowdown(GetThermalSlowdownPeriodUs()));

        return rc;
    });

    m_ThermalSlowdownStartTime = Xp::GetWallTimeUS();

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Signals the thread started by StartPowerStateToggle to stop and waits
//! for its completion.
//!
//! \return OK if successful, not OK if resource release failed
RC TestMode::StopPowerStateToggle()
{
    RC rc;

    bool bHwPowerToggle = false;
    CHECK_RC(ForEachLink(
        [&](TestDevicePtr pDev, UINT32 linkId)->RC
        {
            if (!pDev->SupportsInterface<LwLinkPowerState>())
                return RC::OK;
            auto powerState = pDev->GetInterface<LwLinkPowerState>();
            if (!powerState->SupportsLpHwToggle())
                return RC::OK;
            bHwPowerToggle = true;
            // Request FB to stop toggling
            return powerState->RequestPowerState(linkId, LwLinkPowerState::SLS_PWRM_FB, false);
        },
        nullptr
    ));

    if (bHwPowerToggle)
    {
        const UINT64 durationUs = Xp::GetWallTimeUS() - m_PSToggleStartTime;

        UINT32 count = static_cast<UINT32>(durationUs / (m_PSToggleIntervalUs*2.0));
        SetSwLPEntryCount(GetSwLPEntryCount() + count);

        const double switchFreqkHz = (1000.0 * count) / durationUs;
        Printf(m_PrintPri,
               "Power state switch frequency between FB and LP was "
               "%.3f kHz, total %u switches in %llu us\n",
               switchFreqkHz, count, durationUs);
    }
    else
    {
        m_PSToggleThreadCont = false;
        CHECK_RC(Tasker::Join(m_PSToggleThreadId, m_PSToggleTimeout));
        m_PSToggleThreadId = -1;
    }

    return rc;
}

//------------------------------------------------------------------------------
RC TestMode::StopThermalThrottling()
{
    RC rc;

    CHECK_RC(ForEachDevice(
        [](TestDevicePtr dev) -> RC
        {
            RC rc;
            auto pTT = dev->GetInterface<ThermalThrottling>();
            if (!pTT)
                return OK;

            CHECK_RC(pTT->StopThermalThrottling());

            return rc;
        }
    ));

    return rc;
}

//------------------------------------------------------------------------------
RC TestMode::StopThermalSlowdownToggle()
{
    RC rc;

    ForEachDevice([&](TestDevicePtr pDev) -> RC
    {
        RC rc;
        if (!pDev->SupportsInterface<ThermalSlowdown>())
            return RC::OK;
        ThermalSlowdown* pSlowdown = pDev->GetInterface<ThermalSlowdown>();
        CHECK_RC(pSlowdown->StopThermalSlowdown());
        return rc;
    });

    const UINT64 durationUs = Xp::GetWallTimeUS() - m_ThermalSlowdownStartTime;

    UINT32 count = static_cast<UINT32>(durationUs / (GetThermalSlowdownPeriodUs()*2.0));
    SetSwLPEntryCount(GetSwLPEntryCount() + count);

    return rc;
}

//------------------------------------------------------------------------------
void TestMode::PowerStateToggleThreadDispatch(void *pThis)
{
    static_cast<TestMode *>(pThis)->PowerStateToggleThread();
}

//------------------------------------------------------------------------------
// TestDirectionData public implementaion
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
/* virtual */ RC TestMode::TestDirectionData::Release
(
    bool *pbResourcesReleased
)
{
    if (!IsInUse())
        return OK;

    StickyRC rc;

    for (BgCopyData& bgCopy : m_BgCopies)
    {
        if (bgCopy.pSrcSurf.get() != nullptr)
        {
            UnmapSurface(bgCopy.pSrcSurf.get(), &bgCopy.pSrcAddr);
            rc = m_pAllocMgr->ReleaseSurface(bgCopy.pSrcSurf);
            bgCopy.pSrcSurf.reset();
            *pbResourcesReleased = true;
        }
        if (bgCopy.pDstSurf.get() != nullptr)
        {
            UnmapSurface(bgCopy.pDstSurf.get(), &bgCopy.pDstAddr);
            rc = m_pAllocMgr->ReleaseSurface(bgCopy.pDstSurf);
            bgCopy.pDstSurf.reset();
            *pbResourcesReleased = true;
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
/* virtual */ void TestMode::TestDirectionData::Reset()
{
    m_TransferHw = LwLinkImpl::HT_NONE;
    m_TotalTimeNs = 0ULL;
    m_TotalBytes = 0ULL;
}

//------------------------------------------------------------------------------
//! \brief Setup the test surfaces for use in the particular test direction
//!
//! \param pTestDir : Pointer to the test direction
//!
//! \return OK if setup was successful, not OK otherwise
RC TestMode::TestDirectionData::SetupTestSurfaces
(
    GpuTestConfiguration *pTestConfig
)
{
    if (!IsInUse())
        return OK;

    RC rc;
    for (BgCopyData& bgCopy : m_BgCopies)
    {
        CHECK_RC(m_pAllocMgr->FillSurface(bgCopy.pSrcSurf,
                                          AllocMgr::PT_RANDOM,
                                          0, 0,
                                          GetTransferHw() != LwLinkImpl::HT_TWOD));
        CHECK_RC(m_pAllocMgr->FillSurface(bgCopy.pDstSurf,
                                          AllocMgr::PT_SOLID,
                                          0, 0,
                                          GetTransferHw() != LwLinkImpl::HT_TWOD));
    }

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Start all copies on a test direction (i.e. flush all channels).
//!
//! \param pTestDir    : Pointer to test direction to flush channels on
//!
//! \return OK if successful, not OK if flushes failed
RC TestMode::TestDirectionData::StartCopies()
{
    RC rc;
    if (!IsInUse())
        return OK;

    for (size_t ii = 0; ii < m_BgCopies.size(); ii++)
    {
        BgCopyData &bgCopy = m_BgCopies[ii];
        Cpu::AtomicWrite(&bgCopy.running, 1);
        bgCopy.threadID =
            Tasker::CreateThread(bgCopy.copyFunc,
                                 &bgCopy,
                                 0,
                                 Utility::StrPrintf("BgCopy%s-%u",
                                                    IsInput() ?"In":"Out",
                                                    static_cast<UINT32>(ii)).c_str());
    }

    return OK;
}

//------------------------------------------------------------------------------
RC TestMode::TestDirectionData::StopCopies()
{
    StickyRC rc;

    for (auto & bgCopy : m_BgCopies)
    {
        Cpu::AtomicWrite(&bgCopy.running, 0);
        if (bgCopy.threadID != Tasker::NULL_THREAD)
        {
            rc = Tasker::Join(bgCopy.threadID, Tasker::NO_TIMEOUT);
            bgCopy.threadID = Tasker::NULL_THREAD;
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
// TestDirectionData protected implementation
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
RC TestMode::TestDirectionData::Acquire
(
    TransferType tt
   ,GpuDevice *pDev
   ,GpuTestConfiguration *pTestConfig
)
{
    RC rc;
    if (!IsInUse() || !(tt & TT_SYSMEM)  || !GetAddCpuTraffic())
        return OK;

    MASSERT(pDev->GetNumSubdevices() == 1);
    GpuSubdevice *const pSubdev = pDev->GetSubdevice(0);
    auto pPcie = pSubdev->GetInterface<Pcie>();

    for (UINT32 i = 0; i < GetNumCpuTrafficThreads(); i++)
    {
        TestDirectionData::BgCopyData cpuCopy;
        cpuCopy.copyFunc = &RunCpuTrafficThread;
        CHECK_RC(Xp::GetDeviceLocalCpus(pPcie->DomainNumber(), pPcie->BusNumber(),
                                        pPcie->DeviceNumber(), pPcie->FunctionNumber(),
                                        &cpuCopy.localCpus));

        // DUT Input testing requires source surface in sysmem and
        // destination in FB, DUT Output testing is the reverse
        CHECK_RC(m_pAllocMgr->AcquireSurface(pDev
                                             ,IsInput() ? Memory::NonCoherent : Memory::Fb
                                             ,Surface2D::Pitch
                                             ,pTestConfig->SurfaceWidth() / 4
                                             ,pTestConfig->SurfaceHeight() / 4
                                             ,&cpuCopy.pSrcSurf));
        CHECK_RC(MapSurface(cpuCopy.pSrcSurf.get(), &cpuCopy.pSrcAddr, pSubdev));

        CHECK_RC(m_pAllocMgr->AcquireSurface(pDev
                                             ,IsInput() ? Memory::Fb : Memory::NonCoherent
                                             ,Surface2D::Pitch
                                             ,pTestConfig->SurfaceWidth() / 4
                                             ,pTestConfig->SurfaceHeight() / 4
                                             ,&cpuCopy.pDstSurf));
        CHECK_RC(MapSurface(cpuCopy.pDstSurf.get(), &cpuCopy.pDstAddr, pSubdev));

        m_BgCopies.push_back(cpuCopy);
    }

    return rc;
}

RC TestMode::TestDirectionData::MapSurface
(
    Surface2D*    pSurf,
    void**        pOutAddr,
    GpuSubdevice* pSubdev
)
{
    RC rc;
    if (pSubdev->IsSOC())
    {
        MASSERT(!pSurf->IsMapped());

        const auto size = static_cast<size_t>(pSurf->GetSize());
        if (size > Platform::GetPageSize())
        {
            Printf(Tee::PriError, "Surface bigger than page size, unable to map!\n");
            return RC::TEGRA_TEST_ILWALID_SETUP;
        }
        PHYSADDR pa = 0;
        CHECK_RC(pSurf->GetPhysAddress(0, &pa));

        // On T194, set bit 37 for loopback mode, LWLink aperture starts at 128GB PA
        pa |= 128_GB;

        CHECK_RC(Platform::MapDeviceMemory(pOutAddr, pa, size, Memory::WC, Memory::ReadWrite));
    }
    else
    {
        CHECK_RC(pSurf->Map(pSubdev->GetSubdeviceInst()));
        *pOutAddr = pSurf->GetAddress();
    }
    return rc;
}

void TestMode::TestDirectionData::UnmapSurface
(
    Surface2D* pSurf,
    void**     pAddr
)
{
    if (pSurf->IsMapped())
    {
        pSurf->Unmap();
    }
    else
    {
        Platform::UnMapDeviceMemory(*pAddr);
    }
    *pAddr = nullptr;
}
