/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/fuse/encoding/rirencoder.h"
#include "gpu/fuse/fuseutils.h"

RC RirEncoder::Initialize(const FuseUtil::FuseDefMap& fuseDefs)
{
    m_FuseDefs = fuseDefs;
    m_IsInitialized = true;
    return OK;
}

//-----------------------------------------------------------------------------
// Encodes the bits requiring RIR into binary for fusing
RC RirEncoder::EncodeFuseValsImpl
(
    FuseValues* pFuseRequest,
    const FuseValues& lwrrentVals,
    vector<UINT32>* pRir
)
{
    RC rc;
    MASSERT(pFuseRequest);
    MASSERT(pRir);

    // Decode current RIR info
    m_RirRecords.clear();
    const size_t numRecords = pRir->size();
    m_RirRecords.reserve(numRecords);
    for (size_t i = 0; i < numRecords; i++)
    {
        UINT32 rirRow = (*pRir)[i];
        m_RirRecords.push_back(static_cast<UINT16>(rirRow & 0xffff));
        m_RirRecords.push_back(static_cast<UINT16>(rirRow >> 16));
    }

    vector<string> handledFuses;
    for (const auto& nameValPair : pFuseRequest->m_NamedValues)
    {
        const string& name = nameValPair.first;
        const UINT32& requestedVal = nameValPair.second;

        if (m_FuseDefs.count(name) == 0 ||
            pFuseRequest->m_NeedsRir.count(name) == 0)
        {
            // Handled by a different encoder
            continue;
        }

        handledFuses.push_back(name);
        auto& fuseDef = m_FuseDefs[name];

        CHECK_RC(AddRirRecord(lwrrentVals, fuseDef, requestedVal));
    }

    // Update RIR values
    for (UINT32 i = 0; i < m_RirRecords.size() / 2; i++)
    {
        UINT32 rir = m_RirRecords[2 * i];
        rir       |= m_RirRecords[2 * i + 1] << 16;
        (*pRir)[i] = rir;
    }

    // Remove the fuse from the fuse request so we know we've handled it
    for (const auto& handledFuse : handledFuses)
    {
        pFuseRequest->m_NamedValues.erase(handledFuse);
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC RirEncoder::DecodeFuseValsImpl
(
    const vector<UINT32>& rawFuses,
    FuseValues* pFuseData
)
{
    // RIR does not encode entire fuses, so decoding doesn't make much sense
    return RC::UNSUPPORTED_FUNCTION;
}

//-----------------------------------------------------------------------------
RC RirEncoder::EncodeReworkFuseValsImpl
(
    FuseValues*  pSrcFuses,
    FuseValues*  pDestFuses,
    ReworkRequest* pRework
)
{
    MASSERT(pSrcFuses);
    MASSERT(pDestFuses);
    MASSERT(pRework);
    RC rc;

    m_RirRecords.resize(pRework->rirRows.size() * 2, 0);

    vector<string> handledFuses;
    for (const auto& nameValPair : pDestFuses->m_NamedValues)
    {
        const string& name = nameValPair.first;
        const UINT32& requestedVal = nameValPair.second;

        if (m_FuseDefs.count(name) == 0 ||
            pDestFuses->m_NeedsRir.count(name) == 0)
        {
            // Handled by a different encoder
            continue;
        }

        pRework->requiresRir = true;

        handledFuses.push_back(name);
        auto& fuseDef = m_FuseDefs[name];

        // If the src fuse doesn't have a defined value for the fuse,
        // why are we using RIR ? 
        if (pSrcFuses->m_NamedValues.count(name) == 0)
        {
            return RC::UNSUPPORTED_FUNCTION;
        }

        CHECK_RC(AddRirRecord(*pSrcFuses, fuseDef, requestedVal));
    }

    // Update RIR values
    for (UINT32 i = 0; i < m_RirRecords.size() / 2; i++)
    {
        UINT32 rir = m_RirRecords[2 * i];
        rir       |= m_RirRecords[2 * i + 1] << 16;
        pRework->rirRows[i] = rir;
    }

    // Remove the fuse from the fuse request so we know we've handled it
    for (const auto& handledFuse : handledFuses)
    {
        pDestFuses->m_NamedValues.erase(handledFuse);
    }

    return rc;
}

//-----------------------------------------------------------------------------
//
RC RirEncoder::AddRirRecord
(
    const FuseValues& lwrrentVals,
    const FuseUtil::FuseDef &fuseDef,
    UINT32 valToBurn
)
{
    RC rc;

    if (m_RirRecords.empty())
    {
        Printf(Tee::PriError, "RIR records not found !\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    UINT32 lwrVal    = lwrrentVals.m_NamedValues.at(fuseDef.Name);
    UINT32 bitsToRir = lwrVal & ~valToBurn;

    INT32 bitToFlip  = -1;
    while ((bitToFlip = Utility::BitScanForward(bitsToRir)) != -1)
    {
        CHECK_RC(InsertRirRecord(bitToFlip, fuseDef.FuseNum));
        if (!fuseDef.RedundantFuse.empty())
        {
            CHECK_RC(InsertRirRecord(bitToFlip, fuseDef.RedundantFuse));
        }
        bitsToRir ^= 1 << bitToFlip;
    }

    return rc;
}

//-----------------------------------------------------------------------------
// Inserts a new RIR record in the existing list
RC RirEncoder::InsertRirRecord
(
    INT32 bitToFlip,
    const list<FuseUtil::FuseLoc>& fuseNums
)
{
    RC rc;

    UINT32 rirIndex = 0;
    auto blankRow = find(m_RirRecords.begin(), m_RirRecords.end(), 0);
    if (blankRow == m_RirRecords.end())
    {
        Printf(Tee::PriError, "No remaining RIR records to write to !\n");
        return RC::FUSE_VALUE_OUT_OF_RANGE;
    }

    CHECK_RC(GetAbsoluteBitIndex(bitToFlip, fuseNums, &rirIndex));
    *blankRow = CreateRirRecord(rirIndex, false);

    return rc;
}

//-----------------------------------------------------------------------------
// Given the index of a bit in a fuse and the fuse's location, returns the
// absolute index of that bit
RC RirEncoder::GetAbsoluteBitIndex
(
    INT32 bitIndex,
    const list<FuseUtil::FuseLoc>& fuseNums,
    UINT32* absIndex
) const
{
    UINT32 startingBits = 0;
    for (const auto& fuseNum : fuseNums)
    {
        UINT32 numBits = fuseNum.Msb - fuseNum.Lsb + 1;
        if (startingBits + numBits < static_cast<UINT32>(bitIndex))
        {
            startingBits += numBits;
            continue;
        }

        UINT32 bitInRow = fuseNum.Lsb + bitIndex - startingBits;
        *absIndex = bitInRow + fuseNum.Number * 32;
        return OK;
    }

    // If we've gotten here, the fuse JSON's FuseNumbers
    // don't match the fuse descriptions.
    return RC::SOFTWARE_ERROR;
}

//-----------------------------------------------------------------------------
//
UINT32 RirEncoder::CreateRirRecord(UINT32 bit, bool shouldSet) const
{
    // Max of 13 address bits
    MASSERT((bit & 0xffffe000) == 0);

    // Bit assignments for a 16 bit RIR record
    // From TSMC Electrical Fuse Datasheet (TEF16FGL192X32HD18_PHENRM_C140218)
    // 15:      Disable
    // 14-10:   Addr[4-0]
    // 9-8:     Addr[6-5]
    // 7-2:     Addr[12-7]
    // 1:       Data
    // 0:       Enable
    UINT16 newRecord = 1;
    newRecord |= (shouldSet ? 1 : 0) << 1;
    newRecord |= ((bit & 0x1f80) >> 7) << 2;
    newRecord |= ((bit & 0x0060) >> 5) << 8;
    newRecord |= ((bit & 0x001f) >> 0) << 10;

    return newRecord;
}
