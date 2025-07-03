/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/script.h"
#include "device/interface/lwlink.h"
#include "device/interface/lwlink/lwlmulticast.h"
#include "device/interface/lwlink/lwlpowerstate.h"
#include "device/interface/lwlink/lwltrex.h"
#include "gpu/include/testdevice.h"
#include "gpu/tests/lwlink/lwlinktest.h"
#include "gpu/utility/lwlink/lwlinkdevif/lwl_topology_mgr.h"
#include "random.h"

#include <set>
#include <memory>
#include <algorithm>

class LwLinkMulticastTrex : public LwLinkTest
{
public:
    LwLinkMulticastTrex();
    virtual ~LwLinkMulticastTrex() = default;

    bool IsSupported() override;
    RC InitFromJs() override;
    RC Setup() override;
    RC RunTest() override;
    RC Cleanup() override;

    void PrintJsProperties(Tee::Priority pri) override;

    SETGET_PROP(DataLength, UINT32);
    SETGET_PROP(RepeatCount, UINT32);
    SETGET_PROP(SkipCheckCount, bool);
    SETGET_PROP(CollapsedResponses, UINT32);
    SETGET_PROP(EnableLfsr, bool);
    SETGET_PROP(InsertUCTraffic, bool);
    SETGET_PROP(MixedPackets, bool);

private:
    RC SetupTrexRams();

    RC StartResidencyTimers();
    RC StopResidencyTimers();
    RC CheckCounters();

    struct TrexDev
    {
        TestDevicePtr pDev;
        UINT32 mcGroupId = ~0U;
    };

private:
    Random m_Random;
    unique_ptr<LwLinkPowerState::Owner> m_pPsOwner;
    vector<TrexDev> m_TrexDevs;

    LwLinkTrex::ReductionOp m_ReduceOp = LwLinkTrex::RO_U16_FADD_MP;
    UINT32 m_DataLength = 0;
    UINT32 m_RepeatCount = 0;
    bool   m_SkipCheckCount = false;
    bool   m_CollapsedResponses = 0x3;
    bool   m_EnableLfsr = true;
    bool   m_InsertUCTraffic = false;
    bool   m_MixedPackets = true;
};

//-----------------------------------------------------------------------------
LwLinkMulticastTrex::LwLinkMulticastTrex()
{
    SetName("LwLinkMulticastTrex");
}

//-----------------------------------------------------------------------------
bool LwLinkMulticastTrex::IsSupported()
{
    if (!LwLinkTest::IsSupported())
        return false;

    TestDevicePtr pTestDevice = GetBoundTestDevice();

    m_Random.SeedRandom(GetTestConfiguration()->Seed());

    if (!pTestDevice->SupportsInterface<LwLinkTrex>() ||
        !pTestDevice->SupportsInterface<LwLinkMulticast>())
    {
        return false;
    }

    vector<LwLinkRoutePtr> routes;
    if (RC::OK != GetLwLinkRoutes(pTestDevice, &routes))
        return false;

    bool bTrexFound = false;
    for (auto& pRoute : routes)
    {
        LwLinkConnection::EndpointDevices epDevs = pRoute->GetEndpointDevices();
        if (epDevs.first->GetType() == TestDevice::TYPE_TREX ||
            epDevs.second->GetType() == TestDevice::TYPE_TREX)
        {
            bTrexFound = true;
            break;
        }
    }
    if (!bTrexFound)
        return false;

    return true;
}

//-----------------------------------------------------------------------------
RC LwLinkMulticastTrex::InitFromJs()
{
    RC rc;
    CHECK_RC(LwLinkTest::InitFromJs());

    JavaScriptPtr pJs;
    jsval jsvReduceOp;
    if (RC::OK == pJs->GetPropertyJsval(GetJSObject(), "ReduceOp", &jsvReduceOp))
    {
        if (JSVAL_IS_NUMBER(jsvReduceOp))
        {
            UINT32 reduceOp = ~0;
            CHECK_RC(pJs->FromJsval(jsvReduceOp, &reduceOp));
            m_ReduceOp = static_cast<LwLinkTrex::ReductionOp>(reduceOp);
        }
        else if (JSVAL_IS_STRING(jsvReduceOp))
        {
            string reduceOpStr;
            CHECK_RC(pJs->FromJsval(jsvReduceOp, &reduceOpStr));
            m_ReduceOp = LwLinkTrex::StringToReductionOp(reduceOpStr);
            if (m_ReduceOp == LwLinkTrex::RO_ILWALID)
            {
                Printf(Tee::PriError, "%s : Invalid ReduceOp: %s\n", GetName().c_str(), reduceOpStr.c_str());
                return RC::ILWALID_ARGUMENT;
            }
        }
        else
        {
            Printf(Tee::PriError, "%s : Invalid ReduceOp\n", GetName().c_str());
            return RC::ILWALID_ARGUMENT;
        }
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC LwLinkMulticastTrex::Setup()
{
    RC rc;

    TestDevicePtr pTestDevice = GetBoundTestDevice();
    LwLink* pLwLink = pTestDevice->GetInterface<LwLink>();

    if (m_DataLength == 0)
    {
        Printf(Tee::PriError, "%s : DataLength cannot be 0\n", GetName().c_str());
        return RC::ILWALID_ARGUMENT;
    }

    if (m_RepeatCount == 0)
    {
        Printf(Tee::PriError, "%s : RepeatCount cannot be 0\n", GetName().c_str());
        return RC::ILWALID_ARGUMENT;
    }

    if (m_MixedPackets && m_DataLength > 128)
    {
        Printf(Tee::PriError, "%s : DataLength cannot be greater than 128 with MixedPackets\n", GetName().c_str());
        return RC::ILWALID_ARGUMENT;
    }

    CHECK_RC(LwLinkTest::Setup());

    vector<LwLinkRoutePtr> routes;
    CHECK_RC(GetLwLinkRoutes(pTestDevice, &routes));

    set<TestDevicePtr> epDevices;
    for (auto& pRoute : routes)
    {
        LwLinkConnection::EndpointDevices epDevs = pRoute->GetEndpointDevices();
        epDevices.insert(epDevs.first);
        epDevices.insert(epDevs.second);
    }

    for (const TestDevicePtr& pDev : epDevices)
    {
        if (pDev->GetType() != TestDevice::TYPE_TREX)
        {
            Printf(Tee::PriError, "%s : Invalid endpoint type: %s. Must be TREX.\n",
                                  __FUNCTION__, TestDevice::TypeToString(pDev->GetType()).c_str());
            return RC::ILWALID_TOPOLOGY;
        }
    }

    if (epDevices.size() < 2)
    {
        Printf(Tee::PriError, "LwLinkMulticastTrex requires at least 2 GPUs defined in the topology\n");
        return RC::ILWALID_TOPOLOGY;
    }

    for (TestDevicePtr pDev : epDevices)
    {
        m_TrexDevs.push_back({pDev});
    }

    if (pTestDevice->SupportsInterface<LwLinkPowerState>())
    {
        m_pPsOwner.reset(new LwLinkPowerState::Owner(pTestDevice->GetInterface<LwLinkPowerState>()));
        CHECK_RC(m_pPsOwner->Claim(LwLinkPowerState::ClaimState::FULL_BANDWIDTH));

        for (UINT32 linkId = 0; linkId < pLwLink->GetMaxLinks(); linkId++)
        {
            if (!pLwLink->IsLinkActive(linkId))
                continue;
            auto powerState = pTestDevice->GetInterface<LwLinkPowerState>();
            CHECK_RC(Tasker::PollHw(GetTestConfiguration()->TimeoutMs(), [&]() -> bool
            {
                LwLinkPowerState::LinkPowerStateStatus ps;
                powerState->GetLinkPowerStateStatus(linkId, &ps);
                return ps.rxSubLinkLwrrentPowerState == LwLinkPowerState::SLS_PWRM_FB &&
                       ps.txSubLinkLwrrentPowerState == LwLinkPowerState::SLS_PWRM_FB;
            }, __FUNCTION__));
        }
    }

    CHECK_RC(pLwLink->SetCollapsedResponses(m_CollapsedResponses));

    auto pFMLibIf = LwLinkDevIf::TopologyManager::GetFMLibIf();
    for (auto& trexDev : m_TrexDevs)
    {
        CHECK_RC(pFMLibIf->AllocMulticastGroup(&trexDev.mcGroupId));
        CHECK_RC(pFMLibIf->SetMulticastGroup(trexDev.mcGroupId, false, false, epDevices));
    }

    CHECK_RC(SetupTrexRams());

    return rc;
}

//-----------------------------------------------------------------------------
RC LwLinkMulticastTrex::RunTest()
{
    RC rc;
    TestDevicePtr pTestDevice = GetBoundTestDevice();
    LwLink* pLwLink = pTestDevice->GetInterface<LwLink>();
    LwLinkTrex* pTrex = pTestDevice->GetInterface<LwLinkTrex>();
    FLOAT32 timeoutMs = GetTestConfiguration()->TimeoutMs();

    CHECK_RC(StartResidencyTimers());

    CHECK_RC(pTrex->StartTrex());

    CHECK_RC(Tasker::PollHw(timeoutMs, [&]()->bool
    {
        for (UINT32 linkId = 0; linkId < pLwLink->GetMaxLinks(); linkId++)
        {
            if (!pLwLink->IsLinkActive(linkId))
                continue;

            if (!pTrex->IsTrexDone(linkId))
                return false;
        }
        return true;
    }, __FUNCTION__));

    CHECK_RC(pLwLink->WaitForIdle(timeoutMs));

    CHECK_RC(StopResidencyTimers());

    if (!m_SkipCheckCount)
    {
        CHECK_RC(CheckCounters());
    }
    else
    {
        Printf(Tee::PriLow, "Skipping Checking Counters\n");
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC LwLinkMulticastTrex::Cleanup()
{
    StickyRC rc;
    TestDevicePtr pTestDevice = GetBoundTestDevice();
    LwLink* pLwLink = pTestDevice->GetInterface<LwLink>();
    LwLinkTrex* pTrex = pTestDevice->GetInterface<LwLinkTrex>();

    rc = LwLinkTest::Cleanup();

    for (auto trexDev : m_TrexDevs)
    {
        if (trexDev.mcGroupId != ~0U)
            rc = LwLinkDevIf::TopologyManager::GetFMLibIf()->FreeMulticastGroup(trexDev.mcGroupId);
    }
    m_TrexDevs.clear();

    rc = pLwLink->SetCollapsedResponses(0x3);
    for (UINT32 linkId = 0; linkId < pLwLink->GetMaxLinks(); linkId++)
    {
        if (!pLwLink->IsLinkActive(linkId))
            continue;

        rc = pTrex->SetLfsrEnabled(linkId, false);
    }

    if (m_pPsOwner != nullptr)
        rc = m_pPsOwner->Release();

    return rc;
}

//-----------------------------------------------------------------------------
void LwLinkMulticastTrex::PrintJsProperties(Tee::Priority pri)
{
    LwLinkTest::PrintJsProperties(pri);

    Printf(pri, "%s Js Properties:\n", GetName().c_str());
    Printf(pri, "\tDataLength:          %d\n", m_DataLength);
    Printf(pri, "\tRepeatCount:         %d\n", m_RepeatCount);
    Printf(pri, "\tSkipCheckCount       %s\n", m_SkipCheckCount ? "true" : "false");
    Printf(pri, "\tCollapsedResponses   0x%x\n", m_CollapsedResponses);
    Printf(pri, "\tEnableLfsr           %s\n", m_EnableLfsr ? "true" : "false");
    Printf(pri, "\tInsertUCTraffic      %s\n", m_InsertUCTraffic ? "true" : "false");
    Printf(pri, "\tMixedPackets         %s\n", m_MixedPackets ? "true" : "false");
}

//-----------------------------------------------------------------------------
RC LwLinkMulticastTrex::SetupTrexRams()
{
    RC rc;

    TestDevicePtr pTestDevice = GetBoundTestDevice();
    LwLink* pLwLink = pTestDevice->GetInterface<LwLink>();
    LwLinkTrex* pTrex = pTestDevice->GetInterface<LwLinkTrex>();
    auto pFMLibIf = LwLinkDevIf::TopologyManager::GetFMLibIf();

    for (auto trexDev : m_TrexDevs)
    {
        UINT64 mcBaseAddress = 0ULL;
        CHECK_RC(pFMLibIf->GetMulticastGroupBaseAddress(trexDev.mcGroupId, &mcBaseAddress));
        UINT32 mcRoute = static_cast<UINT32>(mcBaseAddress >>= pLwLink->GetFabricRegionBits());

        for (UINT32 linkId = 0; linkId < trexDev.pDev->GetInterface<LwLink>()->GetMaxLinks(); linkId++)
        {
            LwLink::EndpointInfo ep;
            CHECK_RC(trexDev.pDev->GetInterface<LwLink>()->GetRemoteEndpoint(linkId, &ep));

            if (m_EnableLfsr)
                CHECK_RC(pTrex->SetLfsrEnabled(ep.linkNumber, true));

            vector<LwLinkTrex::RamEntry> mcEntries;
            vector<LwLinkTrex::RamEntry> ucEntries;
            for (auto remoteTrexDev : m_TrexDevs)
            {
                LwLinkTrex::RamEntry entry;

                CHECK_RC(pTrex->CreateMCRamEntry(LwLinkTrex::ET_MC_READ, m_ReduceOp,
                                                 mcRoute, m_DataLength, &entry));
                mcEntries.push_back(entry);

                if (m_MixedPackets)
                {
                    CHECK_RC(pTrex->CreateMCRamEntry(LwLinkTrex::ET_MC_WRITE, m_ReduceOp,
                                                     mcRoute, m_DataLength, &entry));
                    mcEntries.push_back(entry);

                    CHECK_RC(pTrex->CreateMCRamEntry(LwLinkTrex::ET_MC_ATOMIC, m_ReduceOp,
                                                     mcRoute, m_DataLength, &entry));
                    mcEntries.push_back(entry);
                }

                if (m_InsertUCTraffic)
                {
                    vector<LwLink::FabricAperture> fas;
                    CHECK_RC(remoteTrexDev.pDev->GetInterface<LwLink>()->GetFabricApertures(&fas));
                    const UINT32 addrRoute = fas[0].first / fas[0].second;

                    CHECK_RC(pTrex->CreateRamEntry(LwLinkTrex::ET_READ,
                                                   addrRoute, m_DataLength, &entry));
                    ucEntries.push_back(entry);

                    if (m_MixedPackets)
                    {
                        CHECK_RC(pTrex->CreateRamEntry(LwLinkTrex::ET_WRITE,
                                                       addrRoute, m_DataLength, &entry));
                        ucEntries.push_back(entry);

                        CHECK_RC(pTrex->CreateMCRamEntry(LwLinkTrex::ET_ATOMIC, m_ReduceOp,
                                                       addrRoute, m_DataLength, &entry));
                        ucEntries.push_back(entry);
                    }
                }
            }

            auto Shuffle = [&](UINT32 i) -> UINT32 { return m_Random.GetRandom(0, i) % i; };
            random_shuffle(mcEntries.begin(), mcEntries.end(), Shuffle);
            random_shuffle(ucEntries.begin(), ucEntries.end(), Shuffle);

            vector<LwLinkTrex::RamEntry> req0;
            vector<LwLinkTrex::RamEntry> req1;
            if (m_InsertUCTraffic)
            {
                req0.insert(req0.begin(), ucEntries.begin(), ucEntries.end());
                req1.insert(req1.begin(), mcEntries.begin(), mcEntries.end());
            }
            else
            {
                const UINT32 middle = mcEntries.size() / 2;
                req0.insert(req0.begin(), mcEntries.begin(), mcEntries.begin() + middle);
                req1.insert(req1.begin(), mcEntries.begin() + middle, mcEntries.end());
            }

            CHECK_RC(pTrex->SetRamWeight(ep.linkNumber, LwLinkTrex::TR_REQ0, 1));
            CHECK_RC(pTrex->WriteTrexRam(ep.linkNumber, LwLinkTrex::TR_REQ0, req0));

            CHECK_RC(pTrex->SetRamWeight(ep.linkNumber, LwLinkTrex::TR_REQ1, 1));
            CHECK_RC(pTrex->WriteTrexRam(ep.linkNumber, LwLinkTrex::TR_REQ1, req1));

            CHECK_RC(pTrex->SetQueueRepeatCount(ep.linkNumber, m_RepeatCount));
        }
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC LwLinkMulticastTrex::StartResidencyTimers()
{
    RC rc;

    TestDevicePtr pTestDevice = GetBoundTestDevice();
    LwLink* pLwLink = pTestDevice->GetInterface<LwLink>();
    LwLinkTrex* pTrex = pTestDevice->GetInterface<LwLinkTrex>();
    LwLinkMulticast* pMulticast = pTestDevice->GetInterface<LwLinkMulticast>();

    for (UINT32 linkId = 0; linkId < pLwLink->GetMaxLinks(); linkId++)
    {
        if (!pLwLink->IsLinkActive(linkId))
            continue;

        CHECK_RC(pMulticast->StartResidencyTimers(linkId));

        CHECK_RC(pMulticast->ResetResponseCheckCounts(linkId));

        // Reset portstat on read
        UINT64 actualReqPackets = 0LLU;
        UINT64 actualRspPackets = 0LLU;
        CHECK_RC(pTrex->GetPacketCounts(linkId, &actualReqPackets, &actualRspPackets));
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC LwLinkMulticastTrex::StopResidencyTimers()
{
    RC rc;

    TestDevicePtr pTestDevice = GetBoundTestDevice();
    LwLink* pLwLink = pTestDevice->GetInterface<LwLink>();
    LwLinkMulticast* pMulticast = pTestDevice->GetInterface<LwLinkMulticast>();

    for (UINT32 linkId = 0; linkId < pLwLink->GetMaxLinks(); linkId++)
    {
        if (!pLwLink->IsLinkActive(linkId))
            continue;

        CHECK_RC(pMulticast->StopResidencyTimers(linkId));
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC LwLinkMulticastTrex::CheckCounters()
{
    RC rc;
    StickyRC resultrc;

    UINT64 totalMcPackets = 0;
    UINT64 totalRedPackets = m_TrexDevs.size() * m_RepeatCount;
    UINT64 totalRspPasses = totalRedPackets;
    UINT64 totalReqPackets = m_TrexDevs.size() * m_TrexDevs.size() * m_RepeatCount;
    UINT64 totalRspPackets = 0;

    if (m_MixedPackets)
    {
        totalMcPackets += m_TrexDevs.size() * m_RepeatCount * 2;
        totalReqPackets += m_TrexDevs.size() * m_TrexDevs.size() * m_RepeatCount * 2;
    }

    if (m_InsertUCTraffic)
    {
        totalRspPasses += m_TrexDevs.size() * m_RepeatCount;
        totalReqPackets += m_TrexDevs.size() * m_RepeatCount;
        totalRspPackets += m_TrexDevs.size() * m_RepeatCount;

        if (m_MixedPackets)
        {
            totalReqPackets += m_TrexDevs.size() * m_RepeatCount * 2;
            totalRspPackets += m_TrexDevs.size() * m_RepeatCount * 2;
        }
    }

    TestDevicePtr pTestDevice = GetBoundTestDevice();
    LwLinkMulticast* pMulticast = pTestDevice->GetInterface<LwLinkMulticast>();

    for (auto trexDev : m_TrexDevs)
    {
        for (UINT32 linkId = 0; linkId < trexDev.pDev->GetInterface<LwLink>()->GetMaxLinks(); linkId++)
        {
            LwLink::EndpointInfo ep;
            CHECK_RC(trexDev.pDev->GetInterface<LwLink>()->GetRemoteEndpoint(linkId, &ep));

            UINT64 mcCount = 0LLU;
            UINT64 redCount = 0LLU;
            CHECK_RC(pMulticast->GetResidencyCounts(ep.linkNumber, trexDev.mcGroupId, &mcCount, &redCount));

            if (mcCount != totalMcPackets)
            {
                Printf(Tee::PriError,
                "%s : %s : TREX mismatch multicast packet count. Link = %d, Actual = %llu, Expected = %llu\n",
                __FUNCTION__, pTestDevice->GetName().c_str(), ep.linkNumber, mcCount, totalMcPackets);
                resultrc = RC::UNEXPECTED_RESULT;
            }

            if (redCount != totalRedPackets)
            {
                Printf(Tee::PriError,
                "%s : %s : TREX mismatch reduction packet count. Link = %d, Actual = %llu, Expected = %llu\n",
                __FUNCTION__, pTestDevice->GetName().c_str(), ep.linkNumber, redCount, totalRedPackets);
                resultrc = RC::UNEXPECTED_RESULT;
            }

            UINT64 passCount = 0LLU;
            UINT64 failCount = 0LLU;
            CHECK_RC(pMulticast->GetResponseCheckCounts(ep.linkNumber, &passCount, &failCount));
            if (passCount != totalRspPasses)
            {
                Printf(Tee::PriError,
                "%s : %s : TREX response failures detected. Link = %d, Expected = %llu, Passes = %llu, Failures = %llu\n",
                __FUNCTION__, pTestDevice->GetName().c_str(), ep.linkNumber, totalRspPasses, passCount, failCount);
                resultrc = RC::UNEXPECTED_RESULT;
            }

            UINT64 actualReqPackets = 0LLU;
            UINT64 actualRspPackets = 0LLU;
            CHECK_RC(pTestDevice->GetInterface<LwLinkTrex>()->GetPacketCounts(ep.linkNumber,
                                                                              &actualReqPackets,
                                                                              &actualRspPackets));

            if (actualReqPackets != totalReqPackets)
            {
                Printf(Tee::PriError,
                    "%s : %s : Mismatch TREX portstat request packet count. Link = %d, Actual = %llu, Expected = %llu\n",
                    __FUNCTION__, pTestDevice->GetName().c_str(), ep.linkNumber, actualReqPackets, totalReqPackets);
                resultrc = RC::UNEXPECTED_RESULT;
            }

            if (actualRspPackets != totalRspPackets)
            {
                Printf(Tee::PriError,
                    "%s : %s : Mismatch TREX portstat response packet count. Link = %d, Actual = %llu, Expected = %llu\n",
                    __FUNCTION__, pTestDevice->GetName().c_str(), ep.linkNumber, actualRspPackets, totalRspPackets);
                resultrc = RC::UNEXPECTED_RESULT;
            }
        }
    }

    return resultrc;
}

//-----------------------------------------------------------------------------
// Exposed JS properties
JS_CLASS_INHERIT(LwLinkMulticastTrex, LwLinkTest, "LwLink multicast trex test");
CLASS_PROP_READWRITE(LwLinkMulticastTrex, DataLength, UINT32, "Number of bits for each TREX packet");
CLASS_PROP_READWRITE(LwLinkMulticastTrex, RepeatCount, UINT32, "Number of times to internally repeat TREX requests");
CLASS_PROP_READWRITE(LwLinkMulticastTrex, SkipCheckCount, bool, "Do not verify the residency/response counts (default = false)");
CLASS_PROP_READWRITE(LwLinkMulticastTrex, CollapsedResponses, UINT32, "Enable/disable collapsed responses on the device. 0 = Tx/Rx disabled, 1 = Tx disabled, 2 = Rx disabled, 3 = Tx/Rx enabled (default = 0x3");
CLASS_PROP_READWRITE(LwLinkMulticastTrex, EnableLfsr, bool, "Enable/disable Linear Feedback Shift Register in Multicast TREX packets (default = false)");
CLASS_PROP_READWRITE(LwLinkMulticastTrex, InsertUCTraffic, bool, "Insert Unicast traffic in between Multicast/Redution packets (default = false)");
CLASS_PROP_READWRITE(LwLinkMulticastTrex, MixedPackets, bool, "Insert Write and Atomic packets in addition to Read packets (default = true)");
