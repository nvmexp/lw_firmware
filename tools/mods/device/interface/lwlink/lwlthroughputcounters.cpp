/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2021 by LWPU Corporation. All rights reserved. All information
 * contained herein is proprietary and confidential to LWPU Corporation. Any
 * use, reproduction, or disclosure without the written permission of LWPU
 * Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include <string>

#include "lwlthroughputcounters.h"
#include "core/include/massert.h"
#include "core/include/platform.h"
#include "core/include/types.h"
#include "core/include/utility.h"
#include "device/interface/lwlink.h"
#include "lwmisc.h"

//------------------------------------------------------------------------------
/* static */ string LwLinkThroughputCount::GetString
(
    const LwLinkThroughputCount::Config &tc
   ,string prefix
)
{
    string counterStr = prefix;
    switch (tc.m_Cid)
    {
        case CI_TX_COUNT_0:
        case CI_TX_COUNT_1:
        case CI_TX_COUNT_2:
        case CI_TX_COUNT_3:
            counterStr += "TX ";
            break;
        case CI_RX_COUNT_0:
        case CI_RX_COUNT_1:
        case CI_RX_COUNT_2:
        case CI_RX_COUNT_3:
            counterStr += "RX ";
            break;
        default:
            MASSERT(!"Unknown TL Counter ID");
            return "Unknown Counter ID";
    }

    switch (tc.m_Ut)
    {
        case UT_CYCLES:
            counterStr += "Cycles";
            break;
        case UT_PACKETS:
            counterStr += "Packets";
            break;
        case UT_FLITS:
            counterStr += "Flits";
            break;
        case UT_BYTES:
            counterStr += "Bytes";
            break;
        default:
            MASSERT(!"Unknown TL Counter Unit Type");
            counterStr += "Unknown Metric";
            break;
    }

    return counterStr;
}

//------------------------------------------------------------------------------
/* static */ string LwLinkThroughputCount::GetDetailedString
(
    const LwLinkThroughputCount::Config &tc
   ,string prefix
)
{
    string detailedStr = GetString(tc, prefix) + ":\n";

    if (tc.m_Ut == UT_FLITS)
    {
        detailedStr += prefix + "    Flit Filter      : ";
        if (tc.m_Ff == FF_ALL)
        {
            detailedStr += "ALL";
        }
        else
        {
            INT32 lwrBit = Utility::BitScanForward(tc.m_Ff);
            while (lwrBit != -1)
            {
                switch (static_cast<FlitFilter>(1 << lwrBit))
                {
                    case FF_HEAD:
                        detailedStr += "HEAD";
                        break;
                    case FF_AE:
                       detailedStr += "AE";
                       break;
                    case FF_BE:
                       detailedStr += "BE";
                       break;
                    case FF_DATA:
                       detailedStr += "DATA";
                       break;
                    default:
                        MASSERT(!"Unknown Flit Filter Type");
                        detailedStr += Utility::StrPrintf("UNKNOWN(%d)", (1 << lwrBit));
                        break;
                }
                lwrBit = Utility::BitScanForward(tc.m_Ff, lwrBit + 1);
                if (lwrBit != -1)
                    detailedStr += ",";
            }
        }
        detailedStr += "\n";
    }

    if (tc.m_Ut != UT_CYCLES)
    {
        detailedStr += prefix + "    Packet Filter    : ";
        switch (static_cast<PacketFilter>(tc.m_Pf))
        {
            case PF_ALL:
                detailedStr += "ALL";
                break;
            case PF_ALL_EXCEPT_NOP:
               detailedStr += "ALL_EXCEPT_NOP";
               break;
            case PF_ONLY_NOP:
               detailedStr += "NOP";
               break;
            case PF_DATA_ONLY:
               detailedStr += "DATA_ONLY";
               break;
            case PF_WR_RSP_ONLY:
               detailedStr += "WR_RSP_ONLY";
               break;
            default:
                MASSERT(!"Unknown Packet Filter Type");
                detailedStr += Utility::StrPrintf("UNKNOWN(%d)", tc.m_Pf);
                break;
        }
        detailedStr += "\n";
    }
    detailedStr += prefix + Utility::StrPrintf("    PM Size          : %u\n",
                                               1U << tc.m_PmSize);
    return detailedStr;
}

//-----------------------------------------------------------------------------
// Throughput counter interface
//----------------------------------------------------------------------------

//------------------------------------------------------------------------------
RC LwLinkThroughputCounters::ClearThroughputCounters(UINT64 linkMask)
{
    RC rc;

    CHECK_RC(CheckLinkMask(linkMask, "when clearing throughput counters"));

    INT32 lwrLink = Utility::BitScanForward(linkMask, 0);
    while (lwrLink != -1)
    {
        MASSERT(m_ThroughputSetup.count(lwrLink));
        WriteThroughputClearRegs(lwrLink, m_ThroughputSetup[lwrLink].first);
        lwrLink = Utility::BitScanForward(linkMask, lwrLink + 1);
    }

    return rc;
}

//------------------------------------------------------------------------------
bool LwLinkThroughputCounters::AreThroughputCountersSetup(UINT64 linkMask) const
{
    if (RC::OK != CheckLinkMask(linkMask, "when querying throughput counter setup"))
        return false;

    for (auto const & lwrSetup : m_ThroughputSetup)
    {
        linkMask &= ~(1ULL << lwrSetup.first);
    }
    INT32 lwrLink = Utility::BitScanForward(linkMask);
    while (lwrLink != -1)
    {
        lwrLink = Utility::BitScanForward(linkMask, lwrLink + 1);
    }
    return !linkMask;
}

//------------------------------------------------------------------------------
RC LwLinkThroughputCounters::SetupThroughputCounters
(
    UINT64 linkMask
   ,const vector<LwLinkThroughputCount::Config> &configs
)
{
    RC rc;

    CHECK_RC(CheckLinkMask(linkMask, "when setting up throughput counters"));

    INT32 lwrLink = Utility::BitScanForward(linkMask, 0);
    while (lwrLink != -1)
    {
        if (m_ThroughputSetup.count(lwrLink) && m_ThroughputSetup[lwrLink].second)
        {
            Printf(Tee::PriError,
                   "Stop throughput counters on link mask 0x%llx before re-configuring\n",
                   linkMask);
            return RC::SOFTWARE_ERROR;
        }
        lwrLink = Utility::BitScanForward(linkMask, lwrLink + 1);
    }

    UINT32 countMask = 0;
    for (const auto & lwrConfig : configs)
    {
        countMask |= (1 << lwrConfig.m_Cid);
    }
    lwrLink = Utility::BitScanForward(linkMask);
    while (lwrLink != -1)
    {
        WriteThroughputSetupRegs(lwrLink, configs);
        m_ThroughputSetup[lwrLink] = pair<UINT32, bool>(countMask, false);
        lwrLink = Utility::BitScanForward(linkMask, lwrLink + 1);
    }

    return rc;
}

//------------------------------------------------------------------------------
RC LwLinkThroughputCounters::StartThroughputCounters(UINT64 linkMask)
{
    RC rc;

    CHECK_RC(CheckLinkMask(linkMask, "when starting throughput counters"));

    INT32 lwrLink = Utility::BitScanForward(linkMask, 0);
    while (lwrLink != -1)
    {
        MASSERT(m_ThroughputSetup.count(lwrLink));
        WriteThroughputStartRegs(lwrLink, m_ThroughputSetup[lwrLink].first, true);
        m_ThroughputSetup[lwrLink].second = true;
        lwrLink = Utility::BitScanForward(linkMask, lwrLink + 1);
    }

    return rc;
}

//------------------------------------------------------------------------------
RC LwLinkThroughputCounters::StopThroughputCounters(UINT64 linkMask)
{
    RC rc;

    CHECK_RC(CheckLinkMask(linkMask, "when stopping throughput counters"));

    INT32 lwrLink = Utility::BitScanForward(linkMask, 0);
    while (lwrLink != -1)
    {
        MASSERT(m_ThroughputSetup.count(lwrLink));
        WriteThroughputStartRegs(lwrLink, m_ThroughputSetup[lwrLink].first, false);
        m_ThroughputSetup[lwrLink].second = false;
        lwrLink = Utility::BitScanForward(linkMask, lwrLink + 1);
    }

    return rc;
}

//------------------------------------------------------------------------------
RC LwLinkThroughputCounters::GetThroughputCounters
(
    UINT64 linkMask
   ,map<UINT32, vector<LwLinkThroughputCount>> *pCounts
)
{
    RC rc;

    CHECK_RC(CheckLinkMask(linkMask, "when getting throughput counters"));

    INT32 lwrLink = Utility::BitScanForward(linkMask, 0);
    while (lwrLink != -1)
    {
        if (!m_ThroughputSetup.count(lwrLink))
        {
            Printf(Tee::PriError, "Configure throughput counters on link %u before reading\n",
                   lwrLink);
            return RC::SOFTWARE_ERROR;
        }

        vector<LwLinkThroughputCount> counts;
        ReadThroughputCounts(lwrLink, m_ThroughputSetup[lwrLink].first, &counts);
        pCounts->emplace(lwrLink, counts);

        lwrLink = Utility::BitScanForward(linkMask, lwrLink + 1);
    }
    return rc;
}

//------------------------------------------------------------------------------
UINT32 LwLinkThroughputCount::CounterIdToIndex(CounterId counterId)
{
    switch (counterId)
    {
        case LwLinkThroughputCount::CI_RX_COUNT_0:
        case LwLinkThroughputCount::CI_TX_COUNT_0:
            return 0;
        case LwLinkThroughputCount::CI_RX_COUNT_1:
        case LwLinkThroughputCount::CI_TX_COUNT_1:
            return 1;
        case LwLinkThroughputCount::CI_RX_COUNT_2:
        case LwLinkThroughputCount::CI_TX_COUNT_2:
            return 2;
        case LwLinkThroughputCount::CI_RX_COUNT_3:
        case LwLinkThroughputCount::CI_TX_COUNT_3:
            return 3;
        default:
             MASSERT(!"Unknown througput counter");
             break;
    }
    return 0;
}
