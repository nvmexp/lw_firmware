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

#include "lwlbws_trex_test_mode.h"
#include "core/include/mgrmgr.h"
#include "device/interface/lwlink/lwlthroughputcounters.h"
#include "device/interface/lwlink/lwltrex.h"
#include "gpu/include/testdevicemgr.h"
#include "gpu/tests/gputestc.h"
#include "lwlbws_trex_test_route.h"
#include "random.h"

using namespace LwLinkDataTestHelper;

namespace
{
    const vector<LwLinkThroughputCount::Config> s_TpcConfig =
    {
        { LwLinkThroughputCount::CI_TX_COUNT_0, LwLinkThroughputCount::UT_PACKETS, LwLinkThroughputCount::FF_ALL, LwLinkThroughputCount::PF_REQ_ONLY, LwLinkThroughputCount::PS_1 }  //$
       ,{ LwLinkThroughputCount::CI_TX_COUNT_1, LwLinkThroughputCount::UT_PACKETS, LwLinkThroughputCount::FF_ALL, LwLinkThroughputCount::PF_RSP_ONLY, LwLinkThroughputCount::PS_1 }  //$
       ,{ LwLinkThroughputCount::CI_RX_COUNT_0, LwLinkThroughputCount::UT_PACKETS, LwLinkThroughputCount::FF_ALL, LwLinkThroughputCount::PF_REQ_ONLY, LwLinkThroughputCount::PS_1 }  //$
       ,{ LwLinkThroughputCount::CI_RX_COUNT_1, LwLinkThroughputCount::UT_PACKETS, LwLinkThroughputCount::FF_ALL, LwLinkThroughputCount::PF_RSP_ONLY, LwLinkThroughputCount::PS_1 }  //$
    };
}

//------------------------------------------------------------------------------
//! \brief Constructor
//!
TestModeTrex::TestModeTrex
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

//-----------------------------------------------------------------------------
RC TestModeTrex::AssignTrex()
{
    RC rc;

    for (auto & testMapEntry : GetTestData())
    {
        // Eradicate any previous assignments
        testMapEntry.second.pDutInTestData->Reset();
        testMapEntry.second.pDutOutTestData->Reset();

        TestRouteTrex* pTestRoute = dynamic_cast<TestRouteTrex*>(testMapEntry.first.get());

        TestDirectionDataTrex* pDataTrex = nullptr;
        if (GetTransferDir() == TD_IN_ONLY)
        {
            MASSERT(!OnlyLoopback()); // Loopback is always IN/OUT
            pDataTrex =
                static_cast<TestDirectionDataTrex *>(testMapEntry.second.pDutInTestData.get());
            pDataTrex->SetHwLocal(true);
            pDataTrex->SetInput(true);
            pDataTrex->SetTransferHw(LwLink::HT_TREX);
            pDataTrex->SetNumReads(m_NumPackets);
            pDataTrex->SetNumWrites(0);
        }
        else if (GetTransferDir() == TD_OUT_ONLY)
        {
            MASSERT(!OnlyLoopback()); // Loopback is always IN/OUT
            pDataTrex = dynamic_cast<TestDirectionDataTrex*>(testMapEntry.second.pDutOutTestData.get());
            pDataTrex->SetHwLocal(true);
            pDataTrex->SetInput(false);
            pDataTrex->SetTransferHw(LwLink::HT_TREX);
            pDataTrex->SetNumReads(0);
            pDataTrex->SetNumWrites(m_NumPackets);
        }
        else // IN_OUT
        {
            if (OnlyLoopback())
            {
                if (GetLoopbackInput())
                {
                    pDataTrex =
                        static_cast<TestDirectionDataTrex *>(testMapEntry.second.pDutInTestData.get());
                    pDataTrex->SetInput(true);
                    pDataTrex->SetNumReads(m_NumPackets);
                    pDataTrex->SetNumWrites(0);
                }
                else
                {
                    pDataTrex =
                        static_cast<TestDirectionDataTrex *>(testMapEntry.second.pDutOutTestData.get());
                    pDataTrex->SetInput(false);
                    pDataTrex->SetTransferHw(LwLink::HT_TREX);
                    pDataTrex->SetNumReads(0);
                    pDataTrex->SetNumWrites(m_NumPackets);
                }
                pDataTrex->SetHwLocal(true);
                pDataTrex->SetTransferHw(LwLink::HT_TREX);
            }
            else
            {
                pDataTrex = dynamic_cast<TestDirectionDataTrex*>(testMapEntry.second.pDutInTestData.get());
                pDataTrex->SetHwLocal(false);
                pDataTrex->SetInput(true);
                pDataTrex->SetTransferHw(LwLink::HT_TREX);
                pDataTrex->SetNumReads(0);
                pDataTrex->SetNumWrites(m_NumPackets);

                pDataTrex = dynamic_cast<TestDirectionDataTrex*>(testMapEntry.second.pDutOutTestData.get());
                pDataTrex->SetHwLocal(true);
                pDataTrex->SetInput(false);
                pDataTrex->SetTransferHw(LwLink::HT_TREX);
                pDataTrex->SetNumReads(0);
                pDataTrex->SetNumWrites(m_NumPackets);
            }
        }

        if ((testMapEntry.second.pDutInTestData->GetTransferHw() == LwLinkImpl::HT_NONE) &&
            (testMapEntry.second.pDutOutTestData->GetTransferHw() == LwLinkImpl::HT_NONE))
        {
            Printf(Tee::PriError,
                   "%s : No TREX assigned on route :\n%s\n",
                   __FUNCTION__, pTestRoute->GetString("    ").c_str());
            return RC::SOFTWARE_ERROR;
        }
    }

    return RC::OK;
}

//-----------------------------------------------------------------------------
RC TestModeTrex::AcquireResources()
{
    RC rc;

    CHECK_RC(TestMode::AcquireResources());

    for (auto & testMapEntry : GetTestData())
    {
        TestDirectionDataTrex *pTd = nullptr;
        TestRouteTrex *pTestRoute = dynamic_cast<TestRouteTrex *>(testMapEntry.first.get());

        TestDevicePtr locDev = pTestRoute->GetLocalLwLinkDev();
        TestDevicePtr remDev = pTestRoute->GetLocalLwLinkDev();
        if (pTestRoute->GetTransferType() == TT_P2P)
            remDev = pTestRoute->GetRemoteLwLinkDev();

        pTd = dynamic_cast<TestDirectionDataTrex *>(testMapEntry.second.pDutInTestData.get());
        CHECK_RC(pTd->Initialize(locDev, remDev));

        set<TestDevicePtr> devs = pTd->GetSwitchDevs();
        m_SwitchDevs.insert(devs.begin(), devs.end());

        pTd = dynamic_cast<TestDirectionDataTrex *>(testMapEntry.second.pDutOutTestData.get());
        CHECK_RC(pTd->Initialize(remDev, locDev));

        devs = pTd->GetSwitchDevs();
        m_SwitchDevs.insert(devs.begin(), devs.end());
    }

    for (auto pSwitchDev : m_SwitchDevs)
    {
        CHECK_RC(pSwitchDev->GetInterface<LwLink>()->SetCompressedResponses(false));
        CHECK_RC(pSwitchDev->GetInterface<LwLink>()->SetCollapsedResponses(0x0));
        CHECK_RC(pSwitchDev->GetInterface<LwLink>()->SetDbiEnable(false));
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC TestModeTrex::ReleaseResources()
{
    StickyRC rc;

    rc = TestMode::ReleaseResources();

    for (auto pSwitchDev : m_SwitchDevs)
    {
        rc = pSwitchDev->GetInterface<LwLink>()->SetCompressedResponses(true);
        rc = pSwitchDev->GetInterface<LwLink>()->SetCollapsedResponses(0x3);
        rc = pSwitchDev->GetInterface<LwLink>()->SetDbiEnable(true);
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC TestModeTrex::DumpSurfaces(string fnamePrefix)
{
    return RC::OK;
}

//-----------------------------------------------------------------------------
void TestModeTrex::Print(Tee::Priority pri, string prefix)
{
    string testModeStr = prefix + "Dir    Sysmem Dir    # Rte    HwType\n" +
                         prefix + "----------------------\n";

    testModeStr += prefix +
        Utility::StrPrintf("%3s    %10s    %5u    %6s\n",
               TransferDirStr(GetTransferDir()).c_str(),
               TransferDirStr(GetSysmemTransferDir()).c_str(),
               GetNumRoutes(),
               "TREX");

    UINT32 linkColWidth = 0;
    UINT32 ttColWidth = 0;
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
    }

    // Ensure that the max Link field width is at least 7 (length of "Link(s)")
    static const string locLinkStr = "Loc Link(s)";
    static const string remLinkStr = "Rem Link(s)";
    if (linkColWidth < locLinkStr.size())
        linkColWidth = static_cast<UINT32>(locLinkStr.size());

    static const string typeStr = "Type";
    if (ttColWidth < typeStr.size())
        ttColWidth = static_cast<UINT32>(typeStr.size());

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
        testModeStr += "\n";
    }
    Printf(pri, "\n%s\n", testModeStr.c_str());
}

//-----------------------------------------------------------------------------
RC TestModeTrex::SetupCopies(TestDirectionDataPtr pTestDir)
{
    if (!pTestDir->IsInUse())
        return RC::OK;

    TestDirectionDataTrex * pTd;
    pTd = dynamic_cast<TestDirectionDataTrex *>(pTestDir.get());

    return pTd->SetupCopies(GetCopyLoopCount(), GetTestConfig()->Seed());
}

//------------------------------------------------------------------------------
/* virtual */ TestMode::TestDirectionDataPtr TestModeTrex::CreateTestDirectionData()
{
    auto tdData = make_unique<TestDirectionDataTrex>(GetAllocMgr());
    tdData->SetDataLength(m_DataLen);
    return TestDirectionDataPtr(move(tdData));
}

//-----------------------------------------------------------------------------
RC TestModeTrex::WaitForCopies(FLOAT64 timeoutMs)
{
    StickyRC rc;

    rc = Tasker::PollHw(timeoutMs, [&]()->bool
    {
        if (GetShowCopyProgress())
        {
            static const UINT32 progressTimeTicks =
                Xp::QueryPerformanceFrequency() * 5;
            UINT64 lwrTick = Xp::QueryPerformanceCounter();
            if ((lwrTick - GetProgressPrintTick()) > progressTimeTicks)
            {
                Printf(Tee::PriNormal, ".");
                SetProgressPrintTick(lwrTick);
            }
        }

        for (auto const & lwrRteTestData : GetTestData())
        {
            vector<TestDirectionDataTrex*> tds =
                {dynamic_cast<TestDirectionDataTrex *>(lwrRteTestData.second.pDutInTestData.get()),
                 dynamic_cast<TestDirectionDataTrex *>(lwrRteTestData.second.pDutOutTestData.get())};
            for (auto pTd : tds)
            {
                if (!pTd->AreCopiesComplete())
                    return false;
            }
        }
        return true;
    }, __FUNCTION__);

    for (auto switchDev : m_SwitchDevs)
        rc = switchDev->GetInterface<LwLink>()->WaitForIdle(timeoutMs);
    UINT64 endTime = Xp::GetWallTimeNS();

    for (auto const & lwrRteTestData : GetTestData())
    {
        vector<TestDirectionDataTrex*> tds =
            {dynamic_cast<TestDirectionDataTrex *>(lwrRteTestData.second.pDutInTestData.get()),
             dynamic_cast<TestDirectionDataTrex *>(lwrRteTestData.second.pDutOutTestData.get())};
        for (auto pTd : tds)
        {
            pTd->SetEndTime(endTime);
        }
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC TestModeTrex::CheckDestinationSurface
(
    TestRoutePtr pRoute,
    TestDirectionDataPtr pTestDir)
{
    return RC::OK;
}

UINT64 TestModeTrex::GetMinimumBytesPerCopy(UINT32 maxBwKiBps)
{
    return 0LLU;
}

//-----------------------------------------------------------------------------
RC TestModeTrex::TriggerAllCopies()
{
    RC rc;

    UINT64 startTime = Xp::GetWallTimeNS();

    set<TestDevicePtr> uniqueSwitches(m_SwitchDevs.begin(), m_SwitchDevs.end());

    for (auto switchDev : uniqueSwitches)
        CHECK_RC(switchDev->GetInterface<LwLinkTrex>()->StartTrex());

    for (auto const & lwrRteTestData : GetTestData())
    {
        vector<TestDirectionDataTrex*> tds =
            {dynamic_cast<TestDirectionDataTrex *>(lwrRteTestData.second.pDutInTestData.get()),
             dynamic_cast<TestDirectionDataTrex *>(lwrRteTestData.second.pDutOutTestData.get())};
        for (auto pTd : tds)
        {
            pTd->SetStartTime(startTime);
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
RC TestModeTrex::ShowPortStats()
{
    StickyRC rc;

    for (auto const & lwrRteTestData : GetTestData())
    {
        vector<TestDirectionDataTrex*> tds =
            {dynamic_cast<TestDirectionDataTrex *>(lwrRteTestData.second.pDutInTestData.get()),
             dynamic_cast<TestDirectionDataTrex *>(lwrRteTestData.second.pDutOutTestData.get())};
        for (auto pTd : tds)
        {
            rc = pTd->ShowPortStats();
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
RC TestModeTrex::CheckPortStats()
{
    StickyRC rc;

    for (auto const & lwrRteTestData : GetTestData())
    {
        vector<TestDirectionDataTrex*> tds =
            {dynamic_cast<TestDirectionDataTrex *>(lwrRteTestData.second.pDutInTestData.get()),
             dynamic_cast<TestDirectionDataTrex *>(lwrRteTestData.second.pDutOutTestData.get())};
        for (auto pTd : tds)
        {
            rc = pTd->CheckPortStats();
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
RC TestModeTrex::StartPortStats()
{
    RC rc;

    for (auto switchDev : m_SwitchDevs)
    {
        if (this->AllowThroughputCounterPortStats() &&
            switchDev->SupportsInterface<LwLinkThroughputCounters>())
        {
            UINT64 linkMask = 0;
            auto pTpc = switchDev->GetInterface<LwLinkThroughputCounters>();
            auto pLwLink = switchDev->GetInterface<LwLink>();
            for (UINT32 lwrLink = 0; lwrLink < pLwLink->GetMaxLinks(); lwrLink++)
                linkMask |= pLwLink->IsLinkActive(lwrLink) ? (1ULL << lwrLink) : 0;

            CHECK_RC(pTpc->SetupThroughputCounters(linkMask, s_TpcConfig));
            CHECK_RC(pTpc->ClearThroughputCounters(linkMask));
            CHECK_RC(pTpc->StartThroughputCounters(linkMask));
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
RC TestModeTrex::StopPortStats()
{
    RC rc;

    for (auto switchDev : m_SwitchDevs)
    {
        if (this->AllowThroughputCounterPortStats() &&
                switchDev->SupportsInterface<LwLinkThroughputCounters>())
        {
            UINT64 linkMask = 0;
            auto pLwLink = switchDev->GetInterface<LwLink>();
            auto pTpc = switchDev->GetInterface<LwLinkThroughputCounters>();
            for (UINT32 lwrLink = 0; lwrLink < pLwLink->GetMaxLinks(); lwrLink++)
                linkMask |= pLwLink->IsLinkActive(lwrLink) ? (1ULL << lwrLink) : 0;

            if (pTpc->AreThroughputCountersSetup(linkMask))
            {
                CHECK_RC(pTpc->StopThroughputCounters(linkMask));
            }
        }
    }

    return rc;
}

//-----------------------------------------------------------------------------
// TestDirectionDataTrex
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
TestModeTrex::TestDirectionDataTrex::TestDirectionDataTrex(AllocMgrPtr pAllocMgr)
: TestDirectionData(pAllocMgr)
{
}

//-----------------------------------------------------------------------------
RC TestModeTrex::TestDirectionDataTrex::Initialize
(
    TestDevicePtr pLocDev,
    TestDevicePtr pRemDev
)
{
    RC rc;

    m_pLocDev = pLocDev;
    m_pRemDev = pRemDev;

    TestDeviceMgr* pTestDeviceMgr = static_cast<TestDeviceMgr*>(DevMgrMgr::d_TestDeviceMgr);
    MASSERT(pTestDeviceMgr);

    LwLink* pLwLink = pLocDev->GetInterface<LwLink>();

    for (UINT32 linkId = 0; linkId < pLwLink->GetMaxLinks(); linkId++)
    {
        if (!pLwLink->IsLinkActive(linkId))
            continue;

        LwLink::EndpointInfo epInfo;
        CHECK_RC(pLwLink->GetRemoteEndpoint(linkId, &epInfo));

        TestDevicePtr pRemSwitch;
        for (UINT32 devInst = 0; devInst < pTestDeviceMgr->NumDevices(); devInst++)
        {
            TestDevicePtr pDev;
            CHECK_RC(pTestDeviceMgr->GetDevice(devInst, &pDev));
            if (pDev->GetId() == epInfo.id)
            {
                m_SwitchDevs[linkId] = pDev;
                break;
            }
        }

        MASSERT(m_SwitchDevs[linkId]);
    }

    SetTotalTimeNs(0ULL);
    SetTotalBytes(0ULL);

    return rc;
}

//-----------------------------------------------------------------------------
RC TestModeTrex::TestDirectionDataTrex::ShowPortStats()
{
    RC rc;

    if (!IsInUse())
        return RC::OK;

    LwLink* pLwLink = m_pLocDev->GetInterface<LwLink>();

    for (UINT32 linkId = 0; linkId < pLwLink->GetMaxLinks(); linkId++)
    {
        if (!pLwLink->IsLinkActive(linkId))
            continue;

        LwLink::EndpointInfo epInfo;
        CHECK_RC(pLwLink->GetRemoteEndpoint(linkId, &epInfo));

        if (m_SwitchDevs[linkId]->SupportsInterface<LwLinkThroughputCounters>())
        {
            map<UINT32, vector<LwLinkThroughputCount>> counts;
            auto pTpc = m_SwitchDevs[linkId]->GetInterface<LwLinkThroughputCounters>();
            CHECK_RC_MSG(pTpc->GetThroughputCounters(1LLU << epInfo.linkNumber, &counts),
                         "Failed to get throughput counters on %s\n",
                         m_SwitchDevs[linkId]->GetName().c_str());

            Printf(Tee::PriNormal, "%s throughput counters :\n",
                   m_SwitchDevs[linkId]->GetName().c_str());
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

//-----------------------------------------------------------------------------
RC TestModeTrex::TestDirectionDataTrex::CheckPortStats()
{
    RC rc;
    StickyRC resultrc;

    if (!IsInUse())
        return RC::OK;

    LwLink* pLwLink = m_pLocDev->GetInterface<LwLink>();

    for (UINT32 linkId = 0; linkId < pLwLink->GetMaxLinks(); linkId++)
    {
        if (!pLwLink->IsLinkActive(linkId))
            continue;

        LwLink::EndpointInfo epInfo;
        CHECK_RC(pLwLink->GetRemoteEndpoint(linkId, &epInfo));

        if (m_SwitchDevs[linkId]->SupportsInterface<LwLinkThroughputCounters>() &&
            (m_SwitchDevs[linkId]->GetInterface<LwLinkTrex>()->GetTrexSrc() == LwLinkTrex::SRC_RX ||
             m_SwitchDevs[linkId]->GetInterface<LwLinkTrex>()->GetTrexDst() == LwLinkTrex::DST_TX))
        {
            map<UINT32, vector<LwLinkThroughputCount>> counts;
            auto pTpc = m_SwitchDevs[linkId]->GetInterface<LwLinkThroughputCounters>();
            CHECK_RC_MSG(pTpc->GetThroughputCounters(1LLU << epInfo.linkNumber, &counts),
                         "Failed to get throughput counters on %s\n",
                         m_SwitchDevs[linkId]->GetName().c_str());

            for (auto const &lwrLinkCounts : counts)
            {
                for (auto const &lwrCount : lwrLinkCounts.second)
                {
                    if (lwrCount.countValue != m_TotalPackets)
                    {
                        bool request = (lwrCount.config.m_Pf == LwLinkThroughputCount::PF_REQ_ONLY);
                        Printf(Tee::PriError,
                               "%s : %s : Mismatch TREX throughput %s packet count. Link = %d, Actual = %llu, Expected = %llu\n",
                               __FUNCTION__,
                               m_SwitchDevs[linkId]->GetName().c_str(),
                               request ? "request" : "response",
                               epInfo.linkNumber,
                               lwrCount.countValue,
                               m_TotalPackets);
                        resultrc = RC::UNEXPECTED_RESULT;
                    }
                }
            }
        }

        if (!m_SwitchDevs[linkId]->HasBug(2597550))
        {
            UINT64 actualReqPackets = 0LLU;
            UINT64 actualRspPackets = 0LLU;
            CHECK_RC(m_SwitchDevs[linkId]->GetInterface<LwLinkTrex>()->GetPacketCounts(epInfo.linkNumber,
                                                                                    &actualReqPackets,
                                                                                    &actualRspPackets));

            if (actualReqPackets != m_TotalPackets)
            {
                Printf(Tee::PriError,
                    "%s : %s : Mismatch TREX portstat request packet count. Link = %d, Actual = %llu, Expected = %llu\n",
                    __FUNCTION__, m_SwitchDevs[linkId]->GetName().c_str(), epInfo.linkNumber, actualReqPackets, m_TotalPackets);
                resultrc = RC::UNEXPECTED_RESULT;
            }

            if (actualRspPackets != m_TotalPackets)
            {
                Printf(Tee::PriError,
                    "%s : %s : Mismatch TREX portstat response packet count. Link = %d, Actual = %llu, Expected = %llu\n",
                    __FUNCTION__, m_SwitchDevs[linkId]->GetName().c_str(), epInfo.linkNumber, actualRspPackets, m_TotalPackets);
                resultrc = RC::UNEXPECTED_RESULT;
            }
        }
    }

    return resultrc;
}

//-----------------------------------------------------------------------------
RC TestModeTrex::TestDirectionDataTrex::SetupCopies
(
    UINT32 numLoops
   ,UINT32 randomSeed
)
{
    RC rc;
    Random random;

    if (!IsInUse())
        return RC::OK;

    random.SeedRandom(randomSeed);

    LwLink* pLwLink = m_pLocDev->GetInterface<LwLink>();

    vector<LwLink::FabricAperture> fas;
    CHECK_RC(m_pRemDev->GetInterface<LwLink>()->GetFabricApertures(&fas));
    const UINT32 addrRoute = fas[0].first / fas[0].second; // Just use the first fabric address index

    UINT64 numPackets = static_cast<UINT64>(m_NumReads + m_NumWrites);
    m_TotalPackets += numPackets * numLoops;

    for (UINT32 linkId = 0; linkId < pLwLink->GetMaxLinks(); linkId++)
    {
        if (!pLwLink->IsLinkActive(linkId))
            continue;

        LwLinkTrex* pTrex = m_SwitchDevs[linkId]->GetInterface<LwLinkTrex>();

        UINT32 ramDepth = 2 * pTrex->GetRamDepth();
        if (numPackets > ramDepth)
        {
            Printf(Tee::PriError,
                   "%s : Num packets requested exceeds TREX RAM depth\n",
                   __FUNCTION__);
            return RC::SOFTWARE_ERROR;
        }

        UINT32 readCount = 0;
        UINT32 writeCount = 0;

        vector<LwLinkTrex::RamEntry> dyn0;
        vector<LwLinkTrex::RamEntry> dyn1;
        dyn0.reserve(numPackets/2);
        dyn1.reserve(numPackets/2);
        for (UINT32 i = 0, remPackets = numPackets; remPackets > 0; i++, remPackets--)
        {
            vector<LwLinkTrex::RamEntry>& ramSel = (i % 2) ? dyn1 : dyn0;

            vector<UINT32> selection;
            if (readCount < m_NumReads)
                selection.push_back(0);
            if (writeCount < m_NumWrites)
                selection.push_back(1);

            LwLinkTrex::RamEntry entry;
            const UINT32 sel = random.GetRandom(selection[0], selection[selection.size()-1]);
            switch (sel)
            {
            case 0:
                CHECK_RC(pTrex->CreateRamEntry(LwLinkTrex::ET_READ, addrRoute, m_DataLen, &entry));
                readCount++;
                break;
            case 1:
                CHECK_RC(pTrex->CreateRamEntry(LwLinkTrex::ET_WRITE, addrRoute, m_DataLen, &entry));
                writeCount++;
                break;
            default:
                MASSERT(false);
                break;
            }

            ramSel.push_back(entry);
        }

        LwLink::EndpointInfo epInfo;
        CHECK_RC(pLwLink->GetRemoteEndpoint(linkId, &epInfo));

        CHECK_RC(pTrex->WriteTrexRam(epInfo.linkNumber, LwLinkTrex::TR_REQ0, dyn0));
        CHECK_RC(pTrex->SetRamWeight(epInfo.linkNumber, LwLinkTrex::TR_REQ0, 1));
        CHECK_RC(pTrex->WriteTrexRam(epInfo.linkNumber, LwLinkTrex::TR_REQ1, dyn1));
        CHECK_RC(pTrex->SetRamWeight(epInfo.linkNumber, LwLinkTrex::TR_REQ1, 1));
        CHECK_RC(pTrex->SetQueueRepeatCount(epInfo.linkNumber, numLoops));
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC TestModeTrex::TestDirectionDataTrex::StartCopies()
{
    // Copies are started in TriggerAllCopies
    return RC::OK;
}

//-----------------------------------------------------------------------------
RC TestModeTrex::TestDirectionDataTrex::StopCopies()
{
    RC rc;

    if (!IsInUse())
        return RC::OK;

    for (auto switchDevPair : m_SwitchDevs)
        CHECK_RC(switchDevPair.second->GetInterface<LwLinkTrex>()->StopTrex());

    return rc;
}

//-----------------------------------------------------------------------------
bool TestModeTrex::TestDirectionDataTrex::AreCopiesComplete()
{
    if (!IsInUse())
        return true;

    LwLink* pLwLink = m_pLocDev->GetInterface<LwLink>();

    for (UINT32 linkId = 0; linkId < pLwLink->GetMaxLinks(); linkId++)
    {
        if (!pLwLink->IsLinkActive(linkId))
            continue;

        LwLink::EndpointInfo epInfo;
        if (RC::OK != pLwLink->GetRemoteEndpoint(linkId, &epInfo))
            continue;

        if (!m_SwitchDevs[linkId]->GetInterface<LwLinkTrex>()->IsTrexDone(epInfo.linkNumber))
        {
            return false;
        }
    }

    return true;
}

//-----------------------------------------------------------------------------
UINT64 TestModeTrex::TestDirectionDataTrex::GetBytesPerLoop() const
{
    LwLink* pLwLink = m_pLocDev->GetInterface<LwLink>();

    UINT32 activeLinks = 0;
    for (UINT32 linkId = 0; linkId < pLwLink->GetMaxLinks(); linkId++)
    {
        if (pLwLink->IsLinkActive(linkId))
            activeLinks++;
    }

    return GetTransferWidth() * activeLinks * (m_NumReads + m_NumWrites);
}

//-----------------------------------------------------------------------------
UINT32 TestModeTrex::TestDirectionDataTrex::GetTransferWidth() const
{
    return m_DataLen;
}

//-----------------------------------------------------------------------------
set<TestDevicePtr> TestModeTrex::TestDirectionDataTrex::GetSwitchDevs()
{
    set<TestDevicePtr> devs;
    for (auto switchDevPair : m_SwitchDevs)
        devs.insert(switchDevPair.second);
    return devs;
}

//-----------------------------------------------------------------------------
bool TestModeTrex::TestDirectionDataTrex::IsInUse() const
{
    if (GetTransferHw() == LwLinkImpl::HT_NONE)
        return false;

    if (m_NumReads == 0 && m_NumWrites == 0)
        return false;

    return true;
}

//-----------------------------------------------------------------------------
RC TestModeTrex::TestDirectionDataTrex::UpdateCopyStats(UINT32 numLoops)
{
    if (!IsInUse())
        return RC::OK;

    AddCopyBytes(GetBytesPerLoop() * numLoops);

    MASSERT(m_EndTime > m_StartTime);
    AddTimeNs(m_EndTime - m_StartTime);

    return RC::OK;
}
