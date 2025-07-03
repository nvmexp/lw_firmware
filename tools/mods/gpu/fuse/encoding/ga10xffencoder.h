/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "subencoder.h"

struct GA10xFFRecord;

// The GA10x+ format for Fuseless Fuses is as follows:
// 11 bit data field, 13 bit block index field, 7 bit chain id field.
// This allows us to support 127 fuseless/repair chains each up to 88K bits long.
// (Note: Bit 5 (Type) is 0 for Fuseless Fuses).
// Chain ID = { 31:30, 4:0 }
//   [31:30]         [29:19]              [18:6]                      [4:0]
// +----------+--------------------+------------------------------+-+---------+
// | ChainId  |         DATA       | 13bit Block blockIndex       |T| ChainId |
// +----------+--------------------+------------------------------+-+---------+

class GA10xFuselessFuseEncoder : public FuselessFuseEncoder
{
public:
    using JtagChainId = UINT08;
    using JtagChainData = vector<UINT16>;
    RC Initialize(const FuseUtil::FuseDefMap& fuseDefs, UINT32 fuselessStart,
        UINT32 fuselessEnd) override;

    void ForceOrderedFFRecords(bool ordered) override
    {
        m_ForceOrdered = ordered;
    }

    // TODO: Move  the func further up in the hierarchy
    bool GetForceOrderedFFRecords()
    {
         return m_ForceOrdered;
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

    virtual UINT32 ExtractFuseVal(const list<FuseUtil::FuseLoc>& fuseLoc);
    virtual void DecodeJtagChains(const vector<UINT32>& rawFuses);
    virtual RC CheckFFEmptyRows
    (
        UINT32 lastUsedFFRowNum,
        const vector <UINT32> & rawFuses
    );

private:
    UINT32 m_FuselessStart = 0;
    UINT32 m_FuselessEnd   = 0;
    FuseUtil::FuseDefMap m_FuseDefs;
    map<JtagChainId, JtagChainData> m_JtagChains;
    vector<GA10xFFRecord> m_Records;
    bool m_UseCachedRecords = true;
    bool m_ForceOrdered = false;

    set<string> m_FsFuses;
    void InsertFuseVal
    (
        const list<FuseUtil::FuseLoc>& fuseLoc,
        UINT32 newVal,
        map<JtagChainId, JtagChainData>* pNewJtagChain
    );

    void SortAndIlwalidateRecords
    (
        vector<GA10xFFRecord> *pOldRecords,
        vector<GA10xFFRecord> *pNewRecords
    );
};

struct GA10xFFRecord
{
    // Chain ID = { 31:30, 4:0 } <=> ChainIdA = [31:30], ChainIdB = [4:0]
    static constexpr UINT32 chainIdMaskA = 0xc0000000; // [31:30]
    static constexpr UINT32 chainIdMaskB = 0x0000001f; // [4:0]

    static constexpr UINT08 typeBit = 5; // While bit<5> == 0, the records are fuseless fuses
    static constexpr UINT32 blockIndexMask = 0x7ffc0; // Block index field = [18:6]
    static constexpr UINT32 dataMask = 0x3ff80000;
    static constexpr UINT08 blockIndexLsb = 6;
    static constexpr UINT08 dataLsb = 19;
    static constexpr UINT08 chainIdALsb = 30; // 11 bits for data [19:29]

    static constexpr UINT08 nullChainId = 0;
    static constexpr UINT08 ilwalidatedChainId = 0x7f;
    static constexpr UINT32 ilwalidatedRow = ~0U;

    UINT08 chainId = 0;
    UINT32 blockIndex = 0;
    UINT16 data = 0;

    static constexpr UINT32 numDataBits = 11;

    GA10xFFRecord(UINT32 rawRecord)
    {
        MASSERT((rawRecord & (1 << typeBit)) == 0);
        data = static_cast<UINT16>((rawRecord & dataMask) >> dataLsb);
        blockIndex = static_cast<UINT16>((rawRecord & blockIndexMask) >> blockIndexLsb);
        // Shifting [31:30] to [6:5] and appending left of [4:0]
        chainId = static_cast<UINT08>(((rawRecord & chainIdMaskA) >> (chainIdALsb - typeBit)) |
                                         (rawRecord & chainIdMaskB));
    }

    GA10xFFRecord(UINT08 chainId, UINT32 blockIndex, UINT16 data)
        : chainId(chainId), blockIndex(blockIndex), data(data)
    { }

    bool operator<(const GA10xFFRecord& other) const
    {
        if (chainId != other.chainId)
        {
            return chainId < other.chainId;
        }
        return blockIndex < other.blockIndex;
    }

    bool operator==(const GA10xFFRecord& other) const
    {
        return (chainId == other.chainId) && (blockIndex == other.blockIndex);
    }

    UINT32 Encode()
    {
        UINT32 rtn = 0;
        UINT32 chainIdA = (chainId << (chainIdALsb - typeBit)) & chainIdMaskA;
        UINT32 chainIdB = chainId & chainIdMaskB;
        rtn |= data << dataLsb;
        rtn |= (blockIndex << blockIndexLsb) & blockIndexMask;
        rtn |= chainIdA;
        rtn |= chainIdB;
        return rtn;
    }

    bool IsBlank()
    {
        return data == 0 && blockIndex == 0 && chainId == 0;
    }
};
