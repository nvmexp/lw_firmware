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

#include "gpu/fuse/encoding/ga10xffencoder.h"
#include "gpu/fuse/fuseutils.h"

RC GA10xFuselessFuseEncoder::Initialize
(
    const FuseUtil::FuseDefMap& fuseDefs,
    UINT32 fuselessStart,
    UINT32 fuselessEnd
)
{
    MASSERT(fuselessStart <= fuselessEnd);
    RC rc;
    m_FuseDefs = fuseDefs;
    m_FuselessStart  = fuselessStart;
    m_FuselessEnd    = fuselessEnd;
    m_IsInitialized = true;
    // Sanity check the all the fuse defs to make sure they're all fuseless
    return rc;
}

RC GA10xFuselessFuseEncoder::EncodeFuseValsImpl
(
    FuseValues* pFuseRequest,
    const FuseValues& lwrrentVals,
    vector<UINT32>* pRawFuses
)
{
    MASSERT(pRawFuses != nullptr);
    MASSERT(pFuseRequest != nullptr);
    RC rc;

    if (!m_UseCachedRecords)
    {
        DecodeJtagChains(*pRawFuses);
    }

    vector<string> handledFuses;
    map<JtagChainId, JtagChainData> newJtagChains = m_JtagChains;
    for (const auto& nameValPair : pFuseRequest->m_NamedValues)
    {
        const string& name = nameValPair.first;
        const UINT32& newVal = nameValPair.second;

        if (m_FuseDefs.count(name) == 0)
        {
            // Handled by a different encoder
            continue;
        }

        handledFuses.push_back(name);
        auto& fuseDef = m_FuseDefs[name];

        InsertFuseVal(fuseDef.FuseNum, newVal, &newJtagChains);
    }

    // Remove the fuse from the fuse request so we know we've handled it
    for (const auto& handledFuse : handledFuses)
    {
        pFuseRequest->m_NamedValues.erase(handledFuse);
    }

    vector<GA10xFFRecord> oldRecords = m_Records;
    vector<GA10xFFRecord> newRecords;

    for (const auto& chain : newJtagChains)
    {
        const auto& chainId = chain.first;
        const auto& chainData = chain.second;
        const auto& oldChainData = m_JtagChains[chainId];
        for (UINT16 blockIndex = 0; blockIndex < chainData.size(); blockIndex++)
        {
            UINT16 newData = chainData[blockIndex];
            UINT16 oldData = 0;
            // If data already exists at this location, see if we can reuse old records
            if (blockIndex < oldChainData.size())
            {
                oldData = oldChainData[blockIndex];
                // If there's no 1->0 transition, we can reuse an record (if one exists)
                if ((oldData & ~newData) == 0)
                {
                    // Find the last record at our chainId/blockIndex location as its
                    // value is the one the hardware will use.
                    auto oldRecord = find_if(oldRecords.rbegin(), oldRecords.rend(),
                        [=](GA10xFFRecord& rec)
                        {
                            return rec.chainId == chainId && rec.blockIndex == blockIndex;
                        });
                    if (oldRecord != oldRecords.rend())
                    {
                        oldRecord->data = newData;
                        continue;
                    }
                }
            }
            // If there wasn't an old record, or we had a 1->0 transition,
            // write a new record if there's new data
            if ((newData != 0) || (newData == 0 && oldData != 0))
            {
                newRecords.emplace_back(chainId, blockIndex, newData);
            }
        }
    }

    // Sort the new records to minimize read time in the hardware
    sort(newRecords.begin(), newRecords.end());
    if (m_ForceOrdered)
    {
        SortAndIlwalidateRecords(&oldRecords, &newRecords);
    }
    else
    {
        oldRecords.insert(oldRecords.end(), newRecords.begin(), newRecords.end());
    }

    UINT32 fuseRow = m_FuselessStart;
    UINT32 rcdNo = 0;
    while (rcdNo < oldRecords.size())
    {
        if (fuseRow > m_FuselessEnd)
        {
            Printf(FuseUtils::GetErrorPriority(), "Out of fuse records!\n");
            return RC::FUSE_VALUE_OUT_OF_RANGE;
        }

        UINT32& row = (*pRawFuses)[fuseRow];
        UINT32 newVal = oldRecords[rcdNo].Encode();
        if (row == GA10xFFRecord::ilwalidatedRow)
        {
            // Find a new row for the given record number
            fuseRow++;
            continue;
        }
        if ((row & (1 << GA10xFFRecord::typeBit)) != 0)
        {
            Printf(FuseUtils::GetErrorPriority(), "Reached IFF section. Out of fuse records!\n");
            return RC::FUSE_VALUE_OUT_OF_RANGE;
        }
        if ((row & ~newVal) != 0)
        {
            Printf(FuseUtils::GetErrorPriority(), "Encoded a 1->0 transition\n");
            return RC::SOFTWARE_ERROR;
        }
        row = newVal;
        fuseRow++;
        rcdNo++;
    }

    // After the current ff records have been encoded check for extra empty ff rows
    // for special requests like MATHS/IST use case : Bug 2737459
    CHECK_RC(CheckFFEmptyRows(fuseRow -1, *pRawFuses));

    m_UseCachedRecords = false;

    return rc;
}

RC GA10xFuselessFuseEncoder::CheckFFEmptyRows
(
    UINT32 lastUsedFFRowNum,
    const vector <UINT32> & rawFuses
)
{
    // find the row number where iff records start
    UINT32 iffStart = m_FuselessEnd + 1;
    for (UINT32 i =  m_FuselessStart; i <= m_FuselessEnd; i++)
    {
        UINT32 row = rawFuses[i];
        if ((row & (1 << GA10xFFRecord::typeBit)) != 0 &&
             row != GA10xFFRecord::ilwalidatedRow)
        {
            iffStart = i;
            break;
        }
    }

    if (lastUsedFFRowNum + GetNumSpareFFRows() >= iffStart)
    {
        Printf(FuseUtils::GetErrorPriority(), "GA10xFuselessFuseEncoder:: Insufficient number of "
                "rows for the fuseless fuse records");
        return RC::FUSE_VALUE_OUT_OF_RANGE;
    }

    return RC::OK;
}

RC GA10xFuselessFuseEncoder::DecodeFuseValsImpl
(
    const vector<UINT32>& rawFuses,
    FuseValues* pFuseData
)
{
    MASSERT(pFuseData != nullptr);
    RC rc;

    DecodeJtagChains(rawFuses);

    for (const auto& nameDefPair : m_FuseDefs)
    {
        const string& name = nameDefPair.first;
        const FuseUtil::FuseDef& fuseDef = nameDefPair.second;
        UINT32 fuseVal = ExtractFuseVal(fuseDef.FuseNum);
        pFuseData->m_NamedValues[name] = fuseVal;
    }
    return rc;
}

RC GA10xFuselessFuseEncoder::EncodeReworkFuseValsImpl
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

    pRework->fuselessRowStart = m_FuselessStart;
    pRework->fuselessRowEnd   = m_FuselessEnd;

    vector<string> handledFuses;
    for (const auto& nameValPair : pDestFuses->m_NamedValues)
    {
        const string& name = nameValPair.first;
        const UINT32& requestedVal = nameValPair.second;

        if (m_FuseDefs.count(name) == 0)
        {
            // Handled by a different encoder
            continue;
        }

        auto& fuseDef = m_FuseDefs[name];
        UINT32 val = requestedVal;
        for (auto& loc : fuseDef.FuseNum)
        {
            pRework->newFFRecords.push_back(ReworkFFRecord(loc, val));
            val >>= (loc.Msb - loc.Lsb + 1);
        }
        handledFuses.push_back(name);
    }

    // Add floorsweeping fuses
    for (const auto& fusename: m_FsFuses)
    {
        if (pDestFuses->m_NamedValues.count(fusename) != 0)
        {
            // Not marked as tracking
            continue;
        }

        auto& fuseDef = m_FuseDefs[fusename];
        for (auto& loc : fuseDef.FuseNum)
        {
            pRework->fsFFRecords.push_back(ReworkFFRecord(loc, 0));
        }
    }

    // Remove the fuse from the fuse request so we know we've handled it
    for (const auto& handledFuse : handledFuses)
    {
        pDestFuses->m_NamedValues.erase(handledFuse);
    }

    return rc;
}

//-----------------------------------------------------------------------------
// Given a fuse's location on the Jtag chains decode a fuse's value
UINT32 GA10xFuselessFuseEncoder::ExtractFuseVal
(
    const list<FuseUtil::FuseLoc>& fuseLocList
)
{
    UINT32 rtn = 0;
    UINT32 rtnLsb = 0;
    for (auto& loc : fuseLocList)
    {
        JtagChainData& jtagChain = m_JtagChains[loc.ChainId];
        if (jtagChain.size() <= loc.Msb / GA10xFFRecord::numDataBits)
        {
            jtagChain.resize((loc.Msb / GA10xFFRecord::numDataBits) + 1, 0x0);
        }
        UINT32 lsb = loc.Lsb;
        while (loc.Msb >= lsb)
        {
            UINT32 blockId = lsb / GA10xFFRecord::numDataBits;
            UINT32 tempLsb = lsb % GA10xFFRecord::numDataBits;
            UINT32 tempMsb = (loc.Msb >= (blockId + 1) * GA10xFFRecord::numDataBits) ?
                              GA10xFFRecord::numDataBits - 1 :
                              loc.Msb % GA10xFFRecord::numDataBits;
            UINT32 numBits = tempMsb - tempLsb + 1;
            UINT32 mask = (1 << numBits) - 1;
            UINT16 block = jtagChain[blockId];
            rtn |= ((block >> tempLsb) & mask) << rtnLsb;
            rtnLsb += numBits;
            lsb = (blockId + 1) * GA10xFFRecord::numDataBits;
        }
    }
    return rtn;
}

//------------------------------------------------------------------------------
void GA10xFuselessFuseEncoder::InsertFuseVal
(
    const list<FuseUtil::FuseLoc>& fuseLoc,
    UINT32 newVal,
    map<JtagChainId, JtagChainData>* pNewJtagChain
)
{
    for (auto& loc : fuseLoc)
    {
        JtagChainData& jtagChain = (*pNewJtagChain)[loc.ChainId];
        if (jtagChain.size() <= loc.Msb / GA10xFFRecord::numDataBits)
        {
            jtagChain.resize((loc.Msb / GA10xFFRecord::numDataBits) + 1, 0x0);
        }
        UINT32 lsb = loc.Lsb;
        while (loc.Msb >= lsb)
        {
            UINT32 blockId = lsb / GA10xFFRecord::numDataBits;
            UINT32 tempLsb = lsb % GA10xFFRecord::numDataBits;
            UINT32 tempMsb = (loc.Msb >= (blockId + 1) * GA10xFFRecord::numDataBits) ?
                              GA10xFFRecord::numDataBits - 1 :
                              loc.Msb % GA10xFFRecord::numDataBits;
            UINT32 numBits = tempMsb - tempLsb + 1;
            UINT32 mask = (1 << numBits) - 1;
            UINT16& block = jtagChain[blockId];
            block &= ~(mask << tempLsb);
            block |= (newVal & mask) << tempLsb;
            newVal >>= numBits;
            lsb = (blockId + 1) * GA10xFFRecord::numDataBits;
        }
    }
}

//------------------------------------------------------------------------------
// Read the record information from the fuse block into the Jtag chains
void GA10xFuselessFuseEncoder::DecodeJtagChains(const vector<UINT32>& rawFuses)
{
    m_JtagChains.clear();
    m_Records.clear();
    MASSERT(rawFuses.size() > m_FuselessEnd);
    for (UINT32 row = m_FuselessStart; row <= m_FuselessEnd; row++)
    {
        UINT32 rowVal = rawFuses[row];
        if (rowVal == GA10xFFRecord::ilwalidatedRow)
        {
            // Ilwalidated row, ignore
            continue;
        }
        if ((rowVal & (1 << GA10xFFRecord::typeBit)) != 0)
        {
            // IFF record
            // Everything after the first IFF record is considered IFF
            break;
        }
        GA10xFFRecord record(rowVal);
        m_Records.push_back(record);
        if (record.chainId == GA10xFFRecord::nullChainId ||
            record.chainId == GA10xFFRecord::ilwalidatedChainId)
        {
            continue;
        }
        JtagChainData& jtagChain = m_JtagChains[record.chainId];
        if (jtagChain.size() <= record.blockIndex)
        {
            jtagChain.resize(record.blockIndex + 1, 0x0);
        }
        jtagChain[record.blockIndex] = record.data;
    }

    // Remove any blank records from the end of our list so
    // we know how many records we actually have
    while (!m_Records.empty() && m_Records.back().IsBlank())
    {
        m_Records.pop_back();
    }
}

//------------------------------------------------------------------------------
RC GA10xFuselessFuseEncoder::AddReworkFuseInfo
(
    const string &name,
    UINT32 newVal,
    vector<ReworkFFRecord> &fuseRecords
) const
{
    if (m_FuseDefs.count(name) == 0)
    {
        return RC::ILWALID_ARGUMENT;
    }

    const auto& fuseDef = m_FuseDefs.at(name);
    UINT32 val = newVal;
    for (const auto& loc : fuseDef.FuseNum)
    {
        fuseRecords.push_back(ReworkFFRecord(loc, val));
        val >>= (loc.Msb - loc.Lsb + 1);
    }
    return RC::OK;
}

//------------------------------------------------------------------------------
bool GA10xFuselessFuseEncoder::IsFsFuse(const string& fsFuseName) const
{
    return m_FsFuses.count(Utility::ToUpperCase(fsFuseName)) != 0;
}

void GA10xFuselessFuseEncoder::AppendFsFuseList(const string& fsFuseName)
{
    m_FsFuses.insert(Utility::ToUpperCase(fsFuseName));
}

void GA10xFuselessFuseEncoder::RemoveFromFsFuseList(const string &fsFuseName)
{
    auto it = m_FsFuses.find(Utility::ToUpperCase(fsFuseName));
    if (it != m_FsFuses.end())
    {
        m_FsFuses.erase(it);
    }
}

//------------------------------------------------------------------------------
// Sort all records
// Also remove any multiple writes to the same location in the JTAG chain
void GA10xFuselessFuseEncoder::SortAndIlwalidateRecords
(
    vector<GA10xFFRecord> *pOldRecords,
    vector<GA10xFFRecord> *pNewRecords
)
{
    // NOTE : We assume that the old records are already sorted and unique
    // Also assuming old and new records are individually sorted

    auto &oldRecords = *pOldRecords;
    auto &newRecords = *pNewRecords;

    if (newRecords.empty())
    {
        // Nothing to sort since old records are already sorted and unique
        return;
    }

    // Find last valid record
    const UINT32 ILWALID_IDX = ~0U;
    UINT32 lastValidOldIndex = ILWALID_IDX;
    for (INT32 i = static_cast<INT32>(oldRecords.size() - 1); i >= 0; i--)
    {
        if (oldRecords[i].chainId != GA10xFFRecord::nullChainId &&
            oldRecords[i].chainId != GA10xFFRecord::ilwalidatedChainId)
        {
            lastValidOldIndex = i;
            break;
        }
    }

    // Find if records are lwrrently in order
    bool isOutOfOrder = false;
    if (lastValidOldIndex != ILWALID_IDX)
    {
        if (newRecords[0] < oldRecords[lastValidOldIndex])
        {
            isOutOfOrder = true;
        }
    }
    if (isOutOfOrder)
    {
        vector<GA10xFFRecord> temp;
        temp.reserve(oldRecords.size());

        // Ilwalidate all old records
        for (auto &rcd : oldRecords)
        {
            if (rcd.chainId != GA10xFFRecord::nullChainId &&
                rcd.chainId != GA10xFFRecord::ilwalidatedChainId)
            {
                temp.push_back(rcd);
                rcd.chainId = GA10xFFRecord::ilwalidatedChainId;
                lastValidOldIndex = ILWALID_IDX;
            }
        }
        newRecords.insert(newRecords.begin(), temp.begin(), temp.end());
    }

    // Sort all the records, retaining the order for records with the same location
    stable_sort(newRecords.begin(), newRecords.end());

    // Remove duplicates (keeping the last of the duplicate records)
    if (lastValidOldIndex != ILWALID_IDX)
    {
        // Since old and new records are individually sorted
        if (oldRecords[lastValidOldIndex] == newRecords[0])
        {
            oldRecords[lastValidOldIndex].chainId = GA10xFFRecord::ilwalidatedChainId;
        }
    }
    for (INT32 i = static_cast<INT32>(newRecords.size() - 2); i >= 0; i--)
    {
        if (newRecords[i] == newRecords[i + 1])
        {
            newRecords.erase(newRecords.begin() + i);
        }
    }

    oldRecords.insert(oldRecords.end(), newRecords.begin(), newRecords.end());
}
