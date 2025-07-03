/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "ga10xffencoder.h"

struct HopperFFRecord;

// The Hopper+ format for Fuseless Fuses is as follows:
// 11 bit data field, 10 bit block index field, 10 bit segment id field (designated
// as chain id till ga10x).
// (Note: Bit 5 (Type) is 0 for Fuseless Fuses).
// Segment ID = { 31:27, 4:0 }
//   [31:27]         [26:16]              [15:6]                      [4:0]
// +----------+--------------------+------------------------------+-+---------+
// | SegId    |         DATA       | 10bit BlockIndex             |T| SegId   |
// +----------+--------------------+------------------------------+-+---------+

class HopperFuselessFuseEncoder : public  GA10xFuselessFuseEncoder
{
public:
    using JtagChainId = UINT16;
    using JtagChainData = vector<UINT16>;
    RC Initialize(const FuseUtil::FuseDefMap& fuseDefs, UINT32 fuselessStart,
        UINT32 fuselessEnd) override;

    RC EncodeFuseValsImpl
    (
        FuseValues* pFuseRequest,
        const FuseValues& lwrrentVals,
        vector<UINT32>* pRawFuses
    ) override;

private:
    UINT32 m_FuselessStart = 0;
    UINT32 m_FuselessEnd   = 0;
    FuseUtil::FuseDefMap m_FuseDefs;
    map<JtagChainId, JtagChainData> m_JtagChains;
    vector<HopperFFRecord> m_Records;
    bool m_UseCachedRecords = true;

    void DecodeJtagChains(const vector<UINT32>& rawFuses);
    UINT32 ExtractFuseVal(const list<FuseUtil::FuseLoc>& fuseLoc);
    // TODO : MH-374 make chain id UINT16/32 accross entire ff hierarchy
    void InsertFuseVal
    (
        const list<FuseUtil::FuseLoc>& fuseLoc,
        UINT32 newVal,
        map<JtagChainId, JtagChainData>* pNewJtagChain
    );
    void SortAndIlwalidateRecords
    (
        vector<HopperFFRecord> *pOldRecords,
        vector<HopperFFRecord> *pNewRecords
    );

    RC CheckFFEmptyRows
    (
        UINT32 lastUsedFFRowNum,
        const vector <UINT32> & rawFuses
    ) override;
};

// TODO: MH-374 Refactoring of  FF encoder classes and supporting code
struct HopperFFRecord
{
    // Chain/Segment ID = { 31:27, 4:0 } <=> ChainIdA = [31:27], ChainIdB = [4:0]
    static constexpr UINT32 chainIdMaskA = 0xf8000000; // [31:27]
    static constexpr UINT32 chainIdMaskB = 0x0000001f; // [4:0]

    static constexpr UINT08 typeBit = 5; // While bit<5> == 0, the records are fuseless fuses
    static constexpr UINT32 blockIndexMask = 0x0ffc0; // Block index field = [15:6]
    static constexpr UINT32 dataMask = 0x7ff0000;  // Data field = [26:16]
    static constexpr UINT08 blockIndexLsb = 6;
    static constexpr UINT08 dataLsb = 16;
    static constexpr UINT08 chainIdALsb = 27; // 11 bits for data [16:26]

    static constexpr UINT08 nullChainId = 0;
    static constexpr UINT16 ilwalidatedChainId = 0x3ff;
    static constexpr UINT32 ilwalidatedRow = ~0U;

    UINT16 chainId = 0;
    UINT32 blockIndex = 0;
    UINT16 data = 0;
    bool isIff = false;

    static constexpr UINT32 numDataBits = 11;

    HopperFFRecord(UINT32 rawRecord)
    {
        data = static_cast<UINT16>((rawRecord & dataMask) >> dataLsb);
        blockIndex = static_cast<UINT16>((rawRecord & blockIndexMask) >> blockIndexLsb);
        // Shifting [31:27] to [9:5] and appending left of [4:0]
        chainId = static_cast<UINT16>(((rawRecord & chainIdMaskA) >> (chainIdALsb - typeBit)) |
                                         (rawRecord & chainIdMaskB));
        isIff = (rawRecord & (1 << typeBit)) ? true : false;
    }

    HopperFFRecord(UINT16 chainId, UINT32 blockIndex, UINT16 data, bool isIff)
        : chainId(chainId), blockIndex(blockIndex), data(data), isIff(isIff)
    { }

    bool operator<(const HopperFFRecord& other) const
    {
        if (chainId != other.chainId)
        {
            return chainId < other.chainId;
        }
        return blockIndex < other.blockIndex;
    }

    bool operator==(const HopperFFRecord& other) const
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
