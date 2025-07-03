/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013,2015,2017,2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef INCLUDED_BLACKLIST_H
#define INCLUDED_BLACKLIST_H

#ifndef INCLUDED_FRAMEBUF_H
#include "core/include/framebuf.h"
#endif

#ifndef _ctrl208f_h_
#include "ctrl/ctrl2080.h"
#endif

class GpuSubdevice;

//--------------------------------------------------------------------
//! \brief Class to hold a list of blacklisted pages
//!
//! This class holds a list of blacklisted pages.  It has methods to
//! load the current blacklist from the RM, compare blacklists, and
//! output the blacklist to the screen, JSON, or a JavaScript object.
//!
class Blacklist
{
public:
    Blacklist();
    RC Load(const GpuSubdevice *pGpuSubdevice);
    RC LoadIfSupported(const GpuSubdevice *pGpuSubdevice);
    void Subtract(const Blacklist &oldBlacklist);
    UINT32 GetSize() const { return m_Blacklist.validEntries; }
    RC Print(Tee::Priority pri) const;
    RC WriteJson(const char *pTag) const;

    enum class Source : UINT08
    {
        STATIC,
        MODS_SBE,
        MODS_DBE,
        DPR_SBE,
        DPR_DBE,
        MEM_TEST
    };
    const char* ToString(Source source) const;

    struct AddrInfo
    {
        UINT64 eccOnAddr;
        string eccOnAddrString;
        UINT64 eccOffAddr;
        string eccOffAddrString;
        Source source;
        UINT32 rbcAddress;
        UINT32 timestamp;
    };
    RC GetPages(vector<AddrInfo> *pAddrInfo) const;
    RC GetSortedPages(vector<AddrInfo> *pAddrInfo) const;

private:
    RC Decode(UINT32 *pPageSize, vector<AddrInfo> *pAddrInfo) const;

    const GpuSubdevice *m_pGpuSubdevice;
    LW2080_CTRL_FB_GET_OFFLINED_PAGES_PARAMS m_Blacklist;
    static const UINT64 ILWALID_ADDR =
        LW2080_CTRL_FB_OFFLINED_PAGES_ILWALID_ADDRESS;
};

#endif // INCLUDED_BLACKLIST_H
