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

#include "gpu/fuse/encoding/hopperffencoder.h"
#include "gpu/fuse/fuseutils.h"

RC HopperFuselessFuseEncoder::Initialize
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
    CHECK_RC(GA10xFuselessFuseEncoder::Initialize(fuseDefs, fuselessStart, fuselessEnd));
    return rc;
}

RC HopperFuselessFuseEncoder::EncodeFuseValsImpl
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

    vector<HopperFFRecord> oldRecords = m_Records;
    vector<HopperFFRecord> newRecords;

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
                // If there's no 1->0 transition, we can reuse the record (if one exists)
                if ((oldData & ~newData) == 0)
                {
                    // Find the last record at our chainId/blockIndex location as its
                    // value is the one the hardware will use.
                    auto oldRecord = find_if(oldRecords.rbegin(), oldRecords.rend(),
                        [=](HopperFFRecord& rec)
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
                newRecords.emplace_back(chainId, blockIndex, newData, false);
            }
        }
    }

    // Sort the new records to minimize read time in the hardware
    sort(newRecords.begin(), newRecords.end());
    if (GetForceOrderedFFRecords())
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
        HopperFFRecord rec(row);
        if (rec.chainId == HopperFFRecord::ilwalidatedChainId)
        {
            // Find a new row for the given record number
            fuseRow++;
            continue;
        }
        if (rec.isIff)
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

RC HopperFuselessFuseEncoder::CheckFFEmptyRows
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
        // Hopper +, In order to check for ilwalidated IFF records, simply checking if
        // chain/segment is ilwalidated is sufficient, we don't need check if the whole
        // row is ilwalidated
        HopperFFRecord rec(row);
        if (rec.isIff && rec.chainId != HopperFFRecord::ilwalidatedChainId)
        {
            iffStart = i;
            break;
        }
    }

    if (lastUsedFFRowNum + GetNumSpareFFRows() >= iffStart)
    {
        Printf(FuseUtils::GetErrorPriority(), "HopperFuselessFuseEncoder:: Insufficient number of "
                "rows for the fuseless fuse records");
        return RC::FUSE_VALUE_OUT_OF_RANGE;
    }

    return RC::OK;
}

//-----------------------------------------------------------------------------
// Given a fuse's location on the Jtag chains decode a fuse's value
UINT32 HopperFuselessFuseEncoder::ExtractFuseVal
(
    const list<FuseUtil::FuseLoc>& fuseLocList
)
{
    UINT32 rtn = 0;
    UINT32 rtnLsb = 0;
    for (auto& loc : fuseLocList)
    {
        JtagChainData& jtagChain = m_JtagChains[loc.ChainId];
        if (jtagChain.size() <= loc.Msb / HopperFFRecord::numDataBits)
        {
            jtagChain.resize((loc.Msb / HopperFFRecord::numDataBits) + 1, 0x0);
        }
        UINT32 lsb = loc.Lsb;
        while (loc.Msb >= lsb)
        {
            UINT32 blockId = lsb / HopperFFRecord::numDataBits;
            UINT32 tempLsb = lsb % HopperFFRecord::numDataBits;
            UINT32 tempMsb = (loc.Msb >= (blockId + 1) * HopperFFRecord::numDataBits) ?
                              HopperFFRecord::numDataBits - 1 :
                              loc.Msb % HopperFFRecord::numDataBits;
            UINT32 numBits = tempMsb - tempLsb + 1;
            UINT32 mask = (1 << numBits) - 1;
            UINT16 block = jtagChain[blockId];
            rtn |= ((block >> tempLsb) & mask) << rtnLsb;
            rtnLsb += numBits;
            lsb = (blockId + 1) * HopperFFRecord::numDataBits;
        }
    }
    return rtn;
}

//------------------------------------------------------------------------------
void HopperFuselessFuseEncoder::InsertFuseVal
(
    const list<FuseUtil::FuseLoc>& fuseLoc,
    UINT32 newVal,
    map<JtagChainId, JtagChainData>* pNewJtagChain
)
{
    for (auto& loc : fuseLoc)
    {
        JtagChainData& jtagChain = (*pNewJtagChain)[loc.ChainId];
        if (jtagChain.size() <= loc.Msb / HopperFFRecord::numDataBits)
        {
            jtagChain.resize((loc.Msb / HopperFFRecord::numDataBits) + 1, 0x0);
        }
        UINT32 lsb = loc.Lsb;
        while (loc.Msb >= lsb)
        {
            UINT32 blockId = lsb / HopperFFRecord::numDataBits;
            UINT32 tempLsb = lsb % HopperFFRecord::numDataBits;
            UINT32 tempMsb = (loc.Msb >= (blockId + 1) * HopperFFRecord::numDataBits) ?
                              HopperFFRecord::numDataBits - 1 :
                              loc.Msb % HopperFFRecord::numDataBits;
            UINT32 numBits = tempMsb - tempLsb + 1;
            UINT32 mask = (1 << numBits) - 1;
            UINT16& block = jtagChain[blockId];
            // preserving the part of the block outside [tempMsb, tempLsb]
            block &= ~(mask << tempLsb);
            // applying new fuse val
            block |= (newVal & mask) << tempLsb;
            newVal >>= numBits;
            lsb = (blockId + 1) * HopperFFRecord::numDataBits;
        }
    }
}

//------------------------------------------------------------------------------
// Read the record information from the fuse block into the Jtag chains
void HopperFuselessFuseEncoder::DecodeJtagChains(const vector<UINT32>& rawFuses)
{
    m_JtagChains.clear();
    m_Records.clear();
    MASSERT(rawFuses.size() > m_FuselessEnd);
    for (UINT32 row = m_FuselessStart; row <= m_FuselessEnd; row++)
    {
        UINT32 rowVal = rawFuses[row];
        HopperFFRecord record(rowVal);
        if (record.chainId == HopperFFRecord::ilwalidatedChainId) 
        {
            // Ilwalidated row => ilwalidatedChainId (Hopper +), ignore
            continue;
        }
        if (record.isIff)
        {
            // IFF record
            // Everything after the first IFF record is considered IFF
            break;
        }
        m_Records.push_back(record);
        if (record.chainId == HopperFFRecord::nullChainId)
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
// Sort all records
// Also remove any multiple writes to the same location in the JTAG chain
void HopperFuselessFuseEncoder::SortAndIlwalidateRecords
(
    vector<HopperFFRecord> *pOldRecords,
    vector<HopperFFRecord> *pNewRecords
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
        if (oldRecords[i].chainId != HopperFFRecord::nullChainId &&
            oldRecords[i].chainId != HopperFFRecord::ilwalidatedChainId)
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
        vector<HopperFFRecord> temp;
        temp.reserve(oldRecords.size());

        // Ilwalidate all old records
        for (auto &rcd : oldRecords)
        {
            if (rcd.chainId != HopperFFRecord::nullChainId &&
                rcd.chainId != HopperFFRecord::ilwalidatedChainId)
            {
                temp.push_back(rcd);
                rcd.chainId = HopperFFRecord::ilwalidatedChainId;
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
            oldRecords[lastValidOldIndex].chainId = HopperFFRecord::ilwalidatedChainId;
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
