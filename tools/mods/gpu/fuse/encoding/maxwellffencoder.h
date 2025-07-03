/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "subencoder.h"

struct FFRecord;

// The Maxwell+ format for Fuseless Fuses is to specify a 16-bit-aligned
// offset on one of the JTAG chains, and then overwrite the data there
// with 16-bits of data. (Note: Bit 5 (Type) is 0 for Fuseless Fuses)
//              [31:16]                   [15:6]            [4:0]
// +-------------------------------+-------------------+-+---------+
// |             DATA              | 16b Block Offset  |T| ChainId |
// +-------------------------------+-------------------+-+---------+

class MaxwellFuselessFuseEncoder : public FuselessFuseEncoder
{
public:
    RC Initialize(const FuseUtil::FuseDefMap& fuseDefs, UINT32 fuselessStart, 
        UINT32 fuselessEnd) override;

    void ForceOrderedFFRecords(bool ordered) override
    {
        m_ForceOrdered = ordered;
    }

    RC EncodeFuseValsImpl
    (
        FuseValues* pFuseRequest,
        const FuseValues& lwrrentVals,
        vector<UINT32>* pRawFuses
    ) override;

    RC DecodeFuseValsImpl
    (
        const vector<UINT32>& rawFuses,
        FuseValues* pFuseData
    ) override;

    RC EncodeReworkFuseValsImpl
    (
        FuseValues*  pSrcFuses,
        FuseValues*  pDestFuses,
        ReworkRequest* pRework
    ) override;

    RC AddReworkFuseInfo
    (
        const string &name,
        UINT32 newVal,
        vector<ReworkFFRecord> &fuseRecords
    ) const override;
    bool IsFsFuse(const string &fsFuseName) const override;
    void AppendFsFuseList(const string& fsFuseName) override;
    void RemoveFromFsFuseList(const string &fsFuseName) override;

private:
    UINT32 m_FuselessStart = 0;
    UINT32 m_FuselessEnd   = 0;
    FuseUtil::FuseDefMap m_FuseDefs;
    map<UINT08, vector<UINT16>> m_JtagChains;
    vector<FFRecord> m_Records;
    bool m_ForceOrdered = false;

    set<string> m_FsFuses;

    void DecodeJtagChains(const vector<UINT32>& rawFuses);
    UINT32 ExtractFuseVal(const list<FuseUtil::FuseLoc>& fuseLoc);
    void InsertFuseVal
    (
        const list<FuseUtil::FuseLoc>& fuseLoc,
        UINT32 newVal,
        map<UINT08, vector<UINT16>>* pNewJtagChain
    );
    void SortAndIlwalidateRecords
    (
        vector<FFRecord> *pOldRecords,
        vector<FFRecord> *pNewRecords
    );
};

struct FFRecord
{
    static constexpr UINT08 chainIdMask = 0x1f;
    static constexpr UINT08 typeBit = 5; // While bit<5> == 0, the records are fuseless fuses
    static constexpr UINT16 offsetMask = 0xffc0;
    static constexpr UINT08 offsetLsb = 6;
    static constexpr UINT08 dataLsb = 16;

    static constexpr UINT08 nullChainId = 0;
    static constexpr UINT08 ilwalidatedChainId = 0x1f;
    static constexpr UINT32 ilwalidatedRow = ~0U;

    UINT08 chainId = 0;
    UINT16 offset = 0;
    UINT16 data = 0;

    FFRecord(UINT32 rawRecord)
    {
        MASSERT((rawRecord & (1 << typeBit)) == 0);
        data = static_cast<UINT16>(rawRecord >> dataLsb);
        offset = static_cast<UINT16>((rawRecord & offsetMask) >> offsetLsb);
        chainId = static_cast<UINT08>(rawRecord & chainIdMask);
    }

    FFRecord(UINT08 chainId, UINT16 offset, UINT16 data)
        : chainId(chainId), offset(offset), data(data)
    { }

    bool operator<(const FFRecord& other) const
    {
        if (chainId != other.chainId)
        {
            return chainId < other.chainId;
        }
        return offset < other.offset;
    }
    
    bool operator==(const FFRecord& other) const
    {
        return (chainId == other.chainId) && (offset == other.offset);
    }

    UINT32 Encode()
    {
        UINT32 rtn = 0;
        rtn |= data << dataLsb;
        rtn |= (offset << offsetLsb) & offsetMask;
        rtn |= chainId & chainIdMask;
        return rtn;
    }

    bool IsBlank()
    {
        return data == 0 && offset == 0 && chainId == 0;
    }
};
