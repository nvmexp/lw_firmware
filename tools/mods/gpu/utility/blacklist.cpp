/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2017,2019,2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "blacklist.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/jsonlog.h"
#include "core/include/utility.h"

#include <algorithm>

//--------------------------------------------------------------------
//! constructor
//!
Blacklist::Blacklist() : m_pGpuSubdevice(NULL)
{
    memset(&m_Blacklist, 0, sizeof(m_Blacklist));
}

//--------------------------------------------------------------------
//! \brief Load the current blacklist from the RM.
//!
//! This method returns an error if it is called on a board that
//! doesn't support blacklisting.
//!
RC Blacklist::Load(const GpuSubdevice *pGpuSubdevice)
{
    MASSERT(pGpuSubdevice);
    RC rc;

    m_pGpuSubdevice = pGpuSubdevice;
    memset(&m_Blacklist, 0, sizeof(m_Blacklist));
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
                pGpuSubdevice, LW2080_CTRL_CMD_FB_GET_OFFLINED_PAGES,
                &m_Blacklist, sizeof(m_Blacklist)));
    return rc;
}

//--------------------------------------------------------------------
//! \brief Load the current blacklist from the RM.
//!
//! This works like Load(), except that if the board does not support
//! blacklists, this method loads an empty blacklist and returns OK.
//!
RC Blacklist::LoadIfSupported(const GpuSubdevice *pGpuSubdevice)
{
    RC rc = Load(pGpuSubdevice);
    if ((rc == RC::LWRM_NOT_SUPPORTED) || (rc == RC::LWRM_OBJECT_NOT_FOUND))
    {
        memset(&m_Blacklist, 0, sizeof(m_Blacklist));
        rc.Clear();
    }
    return rc;
}

// Used by Blacklist::Subtract() to compare two blacklist entries
namespace
{
    class CmpFunc
    {
    public:
        bool operator()
        (
            const LW2080_CTRL_FB_OFFLINED_ADDRESS_INFO &lhs,
            const LW2080_CTRL_FB_OFFLINED_ADDRESS_INFO &rhs
        ) const
        {
            return memcmp(&lhs, &rhs, sizeof(lhs)) < 0;
        }
    };
}

//--------------------------------------------------------------------
//! \brief Find the difference between two blacklists
//!
//! This method takes all blacklist entries in oldBlacklist, and
//! removes them from this blacklist.  Essentially, it finds the
//! entries that were added since oldBlacklist was loaded.
//!
void Blacklist::Subtract(const Blacklist &oldBlacklist)
{
    size_t oldBlacklistSize = oldBlacklist.m_Blacklist.validEntries;
    set<LW2080_CTRL_FB_OFFLINED_ADDRESS_INFO, CmpFunc> oldAddrs(
            &oldBlacklist.m_Blacklist.offlined[0],
            &oldBlacklist.m_Blacklist.offlined[oldBlacklistSize]);

    UINT32 dstIndex = 0;
    for (UINT32 srcIndex = 0; srcIndex < m_Blacklist.validEntries; ++srcIndex)
    {
        if (oldAddrs.count(m_Blacklist.offlined[srcIndex]) == 0)
        {
            if (dstIndex < srcIndex)
            {
                m_Blacklist.offlined[dstIndex] =
                    m_Blacklist.offlined[srcIndex];
            }
            ++dstIndex;
        }
    }
    m_Blacklist.validEntries = dstIndex;
}

//--------------------------------------------------------------------
//! \brief Print the blacklist
//!
RC Blacklist::Print(Tee::Priority pri) const
{
    UINT32 pageSize;
    vector<AddrInfo> addrInfo;
    RC rc;

    CHECK_RC(Decode(&pageSize, &addrInfo));
    Printf(pri, "  Page Size (KB)   : %dk\n", pageSize / 1024);
    Printf(pri, "  Blacklisted Pages: %d\n",
           static_cast<UINT32>(addrInfo.size()));

    if (!addrInfo.empty())
    {
        Printf(pri,
        "    Reason  EccOnPageAddr  EccOffPageAddr  RBC Address         Timestamp\n");
        Printf(pri,
        "    ------  -------------  --------------  -----------  ------------------------\n");
    }
    for (const AddrInfo& info: addrInfo)
    {
        Printf(pri, "    %-7s %13s  %14s   0x%08x %25s\n",
               ToString(info.source),
               info.eccOnAddrString.c_str(),
               info.eccOffAddrString.c_str(),
               info.rbcAddress,
               Utility::ColwertEpochToGMT(info.timestamp).c_str());
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Write the blacklist to the JSON log, if any
//!
RC Blacklist::WriteJson(const char *pTag) const
{
    UINT32 pageSize;
    vector<AddrInfo> addrInfo;
    RC rc;

    CHECK_RC(Decode(&pageSize, &addrInfo));
    for (vector<AddrInfo>::const_iterator pAddrInfo = addrInfo.begin();
         pAddrInfo != addrInfo.end(); ++pAddrInfo)
    {
        JsonItem jsonItem;
        jsonItem.SetTag(pTag);
        jsonItem.SetField("gpu_inst", m_pGpuSubdevice->GetGpuInst());
        jsonItem.SetField("page_size", pageSize);
        jsonItem.SetField("source", ToString(pAddrInfo->source));
        jsonItem.SetField("ecc_on_page_addr",
                          pAddrInfo->eccOnAddrString.c_str());
        jsonItem.SetField("ecc_off_page_addr",
                          pAddrInfo->eccOffAddrString.c_str());
        jsonItem.SetField("timestamp", pAddrInfo->timestamp);
        CHECK_RC(jsonItem.WriteToLogfile());
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Decode the blacklist data read from the RM
//!
//! This method is used by Print() and WriteJson() to translate the
//! data read from the RM into a form that's easier to output.
//!
RC Blacklist::Decode(UINT32 *pPageSize, vector<AddrInfo> *pAddrInfo) const
{
    MASSERT(m_pGpuSubdevice);
    MASSERT(pPageSize);
    MASSERT(pAddrInfo);
    RC rc;

    *pPageSize = 0x1000;

    pAddrInfo->resize(m_Blacklist.validEntries);
    if (!pAddrInfo->empty())
    {
        for (UINT32 ii = 0; ii < pAddrInfo->size(); ++ii)
        {
            const LW2080_CTRL_FB_OFFLINED_ADDRESS_INFO &srcInfo =
                m_Blacklist.offlined[ii];
            AddrInfo *pDstInfo = &(*pAddrInfo)[ii];

            if (srcInfo.pageAddressWithEccOn == ILWALID_ADDR)
            {
                pDstInfo->eccOnAddr = ILWALID_ADDR;
                pDstInfo->eccOnAddrString = "Invalid";
            }
            else
            {
                pDstInfo->eccOnAddr =
                    srcInfo.pageAddressWithEccOn * *pPageSize;
                pDstInfo->eccOnAddrString = Utility::StrPrintf(
                        "0x%010llx", pDstInfo->eccOnAddr);
            }

            if (srcInfo.pageAddressWithEccOff == ILWALID_ADDR)
            {
                pDstInfo->eccOffAddr = ILWALID_ADDR;
                pDstInfo->eccOffAddrString = "Invalid";
            }
            else
            {
                pDstInfo->eccOffAddr =
                    srcInfo.pageAddressWithEccOff * *pPageSize;
                pDstInfo->eccOffAddrString = Utility::StrPrintf(
                        "0x%010llx", pDstInfo->eccOffAddr);
            }

            switch (srcInfo.source)
            {
                case LW2080_CTRL_FB_OFFLINED_PAGES_SOURCE_STATIC_OR_USER:
                    pDstInfo->source = Blacklist::Source::STATIC;
                    break;
                case LW2080_CTRL_FB_OFFLINED_PAGES_SOURCE_MODS_SBE:
                    pDstInfo->source = Blacklist::Source::MODS_SBE;
                    break;
                case LW2080_CTRL_FB_OFFLINED_PAGES_SOURCE_DPR_MULTIPLE_SBE:
                    pDstInfo->source = Blacklist::Source::DPR_SBE;
                    break;
                case LW2080_CTRL_FB_OFFLINED_PAGES_SOURCE_MODS_DBE:
                    pDstInfo->source = Blacklist::Source::MODS_DBE;
                    break;
                case LW2080_CTRL_FB_OFFLINED_PAGES_SOURCE_DPR_DBE:
                    pDstInfo->source = Blacklist::Source::DPR_DBE;
                    break;
                case LW2080_CTRL_FB_OFFLINED_PAGES_SOURCE_MODS_MEM_ERROR:
                    pDstInfo->source = Blacklist::Source::MEM_TEST;
                    break;
                default:
                    MASSERT(!"Unknown blacklist source");
                    break;
            }

            pDstInfo->rbcAddress = srcInfo.rbcAddress;
            pDstInfo->timestamp = srcInfo.timestamp;
        }
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Populate the list of blacklisted pages
//!
RC Blacklist::GetPages(vector<AddrInfo> *pAddrInfo) const
{
    RC rc;
    UINT32 pageSize;
    CHECK_RC(Decode(&pageSize, pAddrInfo));
    return rc;
}

//--------------------------------------------------------------------
//! \brief Sort and populate the blacklisted pages based on timestamps
//!
RC Blacklist::GetSortedPages(vector<AddrInfo> *pAddrInfo) const
{
    RC rc;

    UINT32 pageSize;
    CHECK_RC(Decode(&pageSize, pAddrInfo));

    struct AddrInfoTimestampComparator
    {
        bool operator()(const AddrInfo& a, const AddrInfo& b)
        {
            return a.timestamp < b.timestamp;
        }
    };
    std::sort(pAddrInfo->begin(), pAddrInfo->end(), AddrInfoTimestampComparator());

    return rc;
}

//--------------------------------------------------------------------
//! \brief Return strings of source enums
//!
const char* Blacklist::ToString(Blacklist::Source source) const
{
    switch (source)
    {
        case Blacklist::Source::STATIC:    return "Static";
        case Blacklist::Source::MODS_SBE:  return "ModsSbe";
        case Blacklist::Source::MODS_DBE:  return "ModsDbe";
        case Blacklist::Source::DPR_SBE:   return "DprSbe";
        case Blacklist::Source::DPR_DBE:   return "DprDbe";
        case Blacklist::Source::MEM_TEST:  return "MemTest";
        default:                           return "Unknown";
    }
}
