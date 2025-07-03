/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2022 by LWPU Corporation. All rights reserved. All information
 * contained herein is proprietary and confidential to LWPU Corporation. Any
 * use, reproduction, or disclosure without the written permission of LWPU
 * Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "core/include/rc.h"

class LwLinkTrex
{
public:
    struct RamEntry
    {
        UINT32 data0 = 0x0;
        UINT32 data1 = 0x0;
        UINT32 data2 = 0x0;
        UINT32 data3 = 0x0;
        UINT32 data4 = 0x0;
    };

    enum RamEntryType
    {
        ET_READ,
        ET_WRITE,
        ET_ATOMIC,
        ET_MC_READ,
        ET_MC_WRITE,
        ET_MC_ATOMIC
    };

    enum ReductionOp
    {
        RO_ILWALID,
        RO_S16_FADD,
        RO_S16_FADD_MP,
        RO_S16_FMIN,
        RO_S16_FMAX,
        RO_S32_IMIN,
        RO_S32_IMAX,
        RO_S32_IXOR,
        RO_S32_IAND,
        RO_S32_IOR,
        RO_S32_IADD,
        RO_S32_FADD,
        RO_S64_IMIN,
        RO_S64_IMAX,
        RO_U16_FADD,
        RO_U16_FADD_MP,
        RO_U16_FMIN,
        RO_U16_FMAX,
        RO_U32_IMIN,
        RO_U32_IMAX,
        RO_U32_IXOR,
        RO_U32_IAND,
        RO_U32_IOR,
        RO_U32_IADD,
        RO_U64_IMIN,
        RO_U64_IMAX,
        RO_U64_IXOR,
        RO_U64_IAND,
        RO_U64_IOR,
        RO_U64_IADD,
        RO_U64_FADD
    };

    enum TrexRam
    {
        TR_REQ0,
        TR_REQ1
    };

    enum NcisocSrc
    {
        SRC_EGRESS = 1,
        SRC_RX     = 2
    };

    enum NcisocDst
    {
        DST_INGRESS = 1,
        DST_TX      = 2
    };

    enum DataPattern
    {
        DP_ALL_ZEROS,
        DP_ALL_ONES,
        DP_0_F,
        DP_A_5
    };

    virtual ~LwLinkTrex() {}

    RC CreateRamEntry(RamEntryType type, UINT32 route, UINT32 datalen, RamEntry* pEntry)
        { return DoCreateRamEntry(type, RO_ILWALID, route, datalen, pEntry); }
    RC CreateMCRamEntry(RamEntryType type, ReductionOp op, UINT32 route, UINT32 datalen, RamEntry* pEntry)
        { return DoCreateRamEntry(type, op, route, datalen, pEntry); }
    RC GetPacketCounts(UINT32 linkId, UINT64* pReqCount, UINT64* pRspCount)
        { return DoGetPacketCounts(linkId, pReqCount, pRspCount); }
    UINT32 GetRamDepth() const
        { return DoGetRamDepth(); }
    NcisocDst GetTrexDst() const
        { return DoGetTrexDst(); }
    NcisocSrc GetTrexSrc() const
        { return DoGetTrexSrc(); }
    bool IsTrexDone(UINT32 linkId)
        { return DoIsTrexDone(linkId); }
    RC SetLfsrEnabled(UINT32 linkId, bool bEnabled)
        { return DoSetLfsrEnabled(linkId, bEnabled); }
    RC SetQueueRepeatCount(UINT32 linkId, UINT32 repeatCount)
        { return DoSetQueueRepeatCount(linkId, repeatCount); }
    RC SetRamWeight(UINT32 linkId, TrexRam ram, UINT32 weight)
        { return DoSetRamWeight(linkId, ram, weight); }
    RC SetTrexDataPattern(DataPattern pattern)
        { return DoSetTrexDataPattern(pattern); }
    RC SetTrexDst(NcisocDst dst)
        { return DoSetTrexDst(dst); }
    RC SetTrexSrc(NcisocSrc src)
        { return DoSetTrexSrc(src); }
    RC StartTrex()
        { return DoSetTrexRun(true); }
    RC StopTrex()
        { return DoSetTrexRun(false);}
    RC WriteTrexRam(UINT32 linkId, TrexRam ram, const vector<RamEntry>& entries)
        { return DoWriteTrexRam(linkId, ram, entries); }

    static ReductionOp StringToReductionOp(const string& str);

private:
    virtual RC DoCreateRamEntry(RamEntryType type, ReductionOp op, UINT32 route, UINT32 datalen, RamEntry* pEntry) = 0;
    virtual RC DoGetPacketCounts(UINT32 linkId, UINT64* pReqCount, UINT64* pRspCount) = 0;
    virtual UINT32 DoGetRamDepth() const = 0;
    virtual NcisocDst DoGetTrexDst() const = 0;
    virtual NcisocSrc DoGetTrexSrc() const = 0;
    virtual bool DoIsTrexDone(UINT32 linkId) = 0;
    virtual RC DoSetLfsrEnabled(UINT32 linkId, bool bEnabled) = 0;
    virtual RC DoSetRamWeight(UINT32 linkId, TrexRam ram, UINT32 weight) = 0;
    virtual RC DoSetTrexDataPattern(DataPattern pattern) = 0;
    virtual RC DoSetTrexDst(NcisocDst dst) = 0;
    virtual RC DoSetTrexRun(bool bEnabled) = 0;
    virtual RC DoSetTrexSrc(NcisocSrc src) = 0;
    virtual RC DoSetQueueRepeatCount(UINT32 linkId, UINT32 repeatCount) = 0;
    virtual RC DoWriteTrexRam(UINT32 linkId, TrexRam ram, const vector<RamEntry>& entries) = 0;
};
