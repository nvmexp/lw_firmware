/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/fuse/encoding/iffencoder.h"
#include "ampere/ga100/dev_bus.h"
#include "gpu/fuse/fuseutils.h"
#include "gpu/reghal/reghal.h"

// Decode information found in dev_bus.ref (most up-to-date) and
// //hw/doc/gpu/maxwell/Ilwestigation_phase/Host/Maxwell_Host_IFF_uArch.doc
// (a bit easier to read with MS Word formatting instead of ASCII,
// but not the authoritative reference)

RC IffEncoder::Initialize
(
    UINT32 fuselessStart,
    UINT32 fuselessEnd
)
{
    RC rc;
    MASSERT(fuselessStart <= fuselessEnd);
    m_FuselessStart = fuselessStart;
    m_FuselessEnd   = fuselessEnd;
    m_IsInitialized = true;
    return rc;
}

RC IffEncoder::EncodeFuseValsImpl
(
    FuseValues* pFuseRequest,
    const FuseValues& lwrrentVals,
    vector<UINT32>* pRawFuses
)
{
    MASSERT(pRawFuses != nullptr);
    MASSERT(pFuseRequest != nullptr);
    RC rc;
    if (!pFuseRequest->m_HasIff)
    {
        return rc;
    }

    vector<UINT32>& rawFuses = *pRawFuses;

    UINT32 firstAvailableRow  = m_FuselessStart;
    UINT32 iffStart           = m_FuselessEnd + 1;
    // size of the entire rawFuses block should be greater than or equal to size of
    // fuseless fuse records section
    MASSERT(rawFuses.size() > m_FuselessEnd);
    for (UINT32 i = m_FuselessStart; i <= m_FuselessEnd; i++)
    {
        UINT32 row = rawFuses[i];
        if (IsIffRow(row) && !IsIlwalidatedRow(row))
        {
            iffStart = i;
            break;
        }

        if (row != 0)
        {
            firstAvailableRow = i + 1;
        }
    }

    // IFF rows go at the end of the fuseless records section of the fuse macro.
    // If it's possible to colwert some of the existing rows, then do that,
    // otherwise ilwalidate the rows that don't match the IFF patches to burn,
    // and add them at the end.
    //
    // Note: IFF commands can't change order, so we have to find a match for
    // command N before N+1. Even if command N+1 is already in the fuses,
    // if command N hasn't been written yet, we have to ilwalidate those rows.

    UINT32 lwrrentRow     = m_FuselessEnd;
    UINT32 newRows        = 0;

    while (!pFuseRequest->m_IffRecords.empty() && lwrrentRow >= m_FuselessStart)
    {
        shared_ptr<IffRecord> nextRecord = pFuseRequest->m_IffRecords.back();
        CHECK_RC(CheckIfRecordValid(nextRecord));

        bool hasOneToZero = false;
        // Check for 1 -> 0 transistions
        for (UINT32 i = 0; i < nextRecord->GetDataList().size(); i++)
        {
            // Look at the end of the fuse block and work backwards
            UINT32 newRow = nextRecord->GetData(
                static_cast<UINT32>(nextRecord->GetDataList().size()) - 1 - i);
            UINT32 oldRow = rawFuses[lwrrentRow - i];
            if ((~newRow & oldRow) != 0)
            {
                hasOneToZero = true;
                break;
            }
        }

        if (hasOneToZero)
        {
            // Ilwalidate this row and try the next one(s)
            // If the row to ilwalidate is 0, we don't need to change it
            // Keep it in case the IFFs change again and we can reuse this row
            if (rawFuses[lwrrentRow] != 0)
            {
                rawFuses[lwrrentRow] = GetIlwalidatedRow();
            }
            newRows++;
            lwrrentRow--;
            continue;
        }

        for (UINT32 i = 0; i < nextRecord->GetDataList().size(); i++)
        {
            rawFuses[lwrrentRow] = nextRecord->GetData(
                static_cast<UINT32>(nextRecord->GetDataList().size()) - 1 - i);
            lwrrentRow--;
            newRows++;
        }
        pFuseRequest->m_IffRecords.pop_back();
    }

    // Make sure we finished and didn't overlap into fuseless fuses (this is a
    // very unlikely scenario, but it would cause major issues if it happened)
    //
    // Bug 2737459 : Adding correction for extra fuseles fuse empty rows needed
    //               for MATHS/IST use case i.e GetNumSpareFFRows)

    UINT32 numFFRowsNeeded    = newRows + (firstAvailableRow - m_FuselessStart)
                                + GetNumSpareFFRows();
    UINT32 numFFRowsAvailable = m_FuselessEnd - m_FuselessStart + 1;
    if (!pFuseRequest->m_IffRecords.empty() || numFFRowsNeeded > numFFRowsAvailable)
    {
        Printf(FuseUtils::GetErrorPriority(), "Too many fuse records!\n");
        Printf(FuseUtils::GetErrorPriority(), "Needed: %u;  Available: %u\n",
                                               numFFRowsNeeded, numFFRowsAvailable);
        return RC::FUSE_VALUE_OUT_OF_RANGE;
    }

    // Lastly, ilwalidate any extra IFF rows that we don't want anymore
    for (UINT32 i = iffStart; i <= lwrrentRow; i++)
    {
        // Don't ilwalidate blank rows
        if (rawFuses[i] != 0)
        {
            rawFuses[i] = GetIlwalidatedRow();
        }
    }

    return rc;
}

RC IffEncoder::DecodeFuseValsImpl
(
    const vector<UINT32>& rawFuses,
    FuseValues* pFuseData
)
{
    MASSERT(pFuseData != nullptr);
    pFuseData->m_IffRecords.clear();

    UINT32 iffStart = m_FuselessEnd + 1;
    for (UINT32 i = m_FuselessStart; i <= m_FuselessEnd; i++)
    {
        UINT32 row = rawFuses[i];
        if (IsIffRow(row) && !IsIlwalidatedRow(row))
        {
            iffStart = i;
            break;
        }
    }

    for (UINT32 i = iffStart; i <= m_FuselessEnd; i++)
    {
        UINT32 row = rawFuses[i];
        shared_ptr<IffRecord> record = make_shared<IffRecord>(row);
        UINT32 opCode = record->GetOpCode(row);
        UINT32 extraRows = record->GetIffNumRows(opCode) - 1;
        if (i + extraRows >= m_FuselessEnd + 1)
        {
            Printf(FuseUtils::GetWarnPriority(),
                "Multi-row IFF record too close to end of fuseless fuse section in fuse block!\n");
            break;
        }
        for (UINT32 j = 0; j < extraRows; j++)
        {
            record->AddData(rawFuses[++i]);
        }
        pFuseData->m_IffRecords.push_back(record);
    }

    if (!pFuseData->m_IffRecords.empty())
    {
        pFuseData->m_HasIff = true;
    }

    return OK;
}

RC IffEncoder::EncodeSwFuseVals
(
    const FuseValues& fuseRequest,
    vector<UINT32>* pSwFuseRows
)
{
    MASSERT(pSwFuseRows  != nullptr);
    RC rc;

    if (!fuseRequest.m_HasIff)
    {
        return OK;
    }

    for (const auto record : fuseRequest.m_IffRecords)
    {
        CHECK_RC(CheckIfRecordValid(record));
        for (auto data : record->GetDataList())
        {
            pSwFuseRows->push_back(data);
        }
    }

    return rc;
}

RC IffEncoder::CheckIfRecordValid(const shared_ptr<IffRecord> pRecord)
{
    if (pRecord->IsIlwalidatedRecord() || pRecord->IsNullRecord() ||
        pRecord->IsUnknownRecord())
    {
        Printf(FuseUtils::GetErrorPriority(),
               "Malformed IFF Patch: 0x%08x\n", pRecord->GetData(0));
        Printf(FuseUtils::GetErrorPriority(),
               "Trying to blow Null/Ilwalidated/Unknown IFF\n");
        return RC::FUSE_VALUE_OUT_OF_RANGE;
    }
    else if (pRecord->GetIffType() == IffRecord::IffType::POLL_DW)
    {
        Printf(FuseUtils::GetErrorPriority(),
               "Unsupported IFF Type on this chip: Poll DW\n");
        return RC::SOFTWARE_ERROR;
    }

    return RC::OK;
}

RC IffEncoder::EncodeReworkFuseValsImpl
(
    FuseValues*  pSrcFuses,
    FuseValues*  pDestFuses,
    ReworkRequest* pRework
)
{
    MASSERT(pSrcFuses);
    MASSERT(pDestFuses);
    MASSERT(pRework);

    // Not handling IFF records as of now
    pDestFuses->m_IffRecords.clear();
    return RC::OK;
}

RC IffEncoder::CallwlateMacroIffCrc(const vector<UINT32> &rawFuses, UINT32 *pCrcVal)
{
    RC rc;
    MASSERT(pCrcVal);

    UINT32 iffStart = m_FuselessEnd + 1;
    for (UINT32 i = m_FuselessStart; i <= m_FuselessEnd; i++)
    {
        UINT32 row = rawFuses[i];
        if (IsIffRow(row) && !IsIlwalidatedRow(row))
        {
            iffStart = i;
            break;
        }
    }

    *pCrcVal = 0;
    if (iffStart <= m_FuselessEnd)
    {
        // +1 because we need to include the record at fuseless end as well
        vector<UINT32> iffRows(rawFuses.begin() + iffStart, rawFuses.begin() + m_FuselessEnd + 1);
        CHECK_RC(CallwlateCrc(iffRows, pCrcVal));
    }
    return rc;
}

//------------------------------------------------------------------------------
// Decode and print an IFF record in human-readable format
// Return true if it is a valid IFF record
/* virtual */ RC IffEncoder::DecodeIffRecord
(
    const shared_ptr<IffRecord>& pRecord,
    string &output,
    bool* pIsValid
)
{
    RC rc;
    MASSERT(pIsValid);

    // Record format details from -
    // https://sc-refmanuals.lwpu.com/~gpu_refmanuals/ga100/dev_bus.ref
    *pIsValid = true;
    switch (pRecord->GetIffType())
    {
        case IffRecord::IffType::SET_FIELD:
        {
            MASSERT(pRecord->GetDataList().size() == 1);
            UINT32 data = pRecord->GetData(0);

            UINT32 space     = DRF_VAL(_PBUS_FUSE_FMT_IFF_RECORD, _CMD_SET_FIELD, _SPACE,
                                       data);
            UINT32 addr      = DRF_VAL(_PBUS_FUSE_FMT_IFF_RECORD, _CMD_SET_FIELD, _ADDR,
                                       data);
            addr <<= LW_PBUS_FUSE_FMT_IFF_RECORD_CMD_SET_FIELD_ADDR_BYTESHIFT;
            UINT32 bitIndex  = DRF_VAL(_PBUS_FUSE_FMT_IFF_RECORD, _CMD_SET_FIELD, _BITINDEX,
                                       data);
            UINT32 fieldSpec = DRF_VAL(_PBUS_FUSE_FMT_IFF_RECORD, _CMD_SET_FIELD, _FIELDSPEC,
                                       data);

            // Most significant set bit of the field spec determines the size
            UINT32 fieldWidth = DRF_SIZE(LW_PBUS_FUSE_FMT_IFF_RECORD_CMD_SET_FIELD_FIELDSPEC) - 1;
            for (; fieldWidth > 0; fieldWidth--)
            {
                if ((fieldSpec >> fieldWidth) & 1)
                {
                    break;
                }
            }

            // Get AND and OR masks
            UINT32 orMask  = (fieldSpec & ~(1 << fieldWidth)) << bitIndex;
            UINT32 andMask = ~(((1 << fieldWidth) - 1) << bitIndex);

            output += Utility::StrPrintf("Set Field: (Space %2d) [0x%06x | Space Base Addr] = "
                                         "([0x%06x | Space Base Addr] & 0x%08x) | 0x%08x",
                                          space, addr, addr, andMask, orMask);
        }
        break;
        case IffRecord::IffType::WRITE_DW:
        {
            MASSERT(pRecord->GetDataList().size() == 2);

            UINT32 addr = DRF_VAL(_PBUS_FUSE_FMT_IFF_RECORD, _CMD_WRITE_DW, _ADDR,
                                  pRecord->GetData(0));
            addr <<= LW_PBUS_FUSE_FMT_IFF_RECORD_CMD_WRITE_DW_ADDR_BYTESHIFT;

            output += Utility::StrPrintf("Write: [0x%06x] = 0x%08x",
                                          addr, pRecord->GetData(1));
        }
        break;
        case IffRecord::IffType::MODIFY_DW:
        {
            MASSERT(pRecord->GetDataList().size() == 3);

            UINT32 addr = DRF_VAL(_PBUS_FUSE_FMT_IFF_RECORD, _CMD_MODIFY_DW, _ADDR,
                                  pRecord->GetData(0));
            addr <<= LW_PBUS_FUSE_FMT_IFF_RECORD_CMD_MODIFY_DW_ADDR_BYTESHIFT;

            output += Utility::StrPrintf("Modify: [0x%06x] = ([0x%06x] & 0x%08x) | 0x%08x",
                                          addr, addr, pRecord->GetData(1), pRecord->GetData(2));
        }
        break;
        case IffRecord::IffType::NULL_RECORD:
        case IffRecord::IffType::ILWALIDATED:
            Printf(Tee::PriLow, "Null or ilwalidated record\n");
            *pIsValid = false;
            break;
        default:
            Printf(Tee::PriError, "Unknown IFF record type\n");
            return RC::FUSE_VALUE_OUT_OF_RANGE;
    }
    return rc;
}

/* virtual */ RC IffEncoder::GetIffRecords
(
    const list<FuseUtil::IFFInfo>& iffPatches,
    vector<shared_ptr<IffRecord>>* pRecords
)
{
    MASSERT(pRecords);
    RC rc;

    for (auto iffPatchIter = iffPatches.rbegin();
         iffPatchIter != iffPatches.rend();
         iffPatchIter++)
    {
        for (auto iffRowIter = iffPatchIter->rows.rbegin();
             iffRowIter != iffPatchIter->rows.rend();
             iffRowIter++)
        {
            shared_ptr<IffRecord> iffRecord = make_shared<IffRecord>(iffRowIter->data);
            UINT32 extraRows = iffRecord->GetIffNumRows() - 1;
            for (; extraRows > 0; extraRows--)
            {
                iffRowIter++;
                if (iffRowIter == iffPatchIter->rows.rend())
                {
                    Printf(FuseUtils::GetErrorPriority(),
                        "Invalid IFF patch: %s\n", iffPatchIter->name.c_str());
                    return RC::FUSE_VALUE_OUT_OF_RANGE;
                }
                iffRecord->AddData(iffRowIter->data);
            }
            pRecords->push_back(iffRecord);
        }
    }
    return rc;
}

/* virtual */ RC IffEncoder::ValidateIffRecords
(
    const vector<shared_ptr<IffRecord>>& chipRecords,
    const vector<shared_ptr<IffRecord>>& skuRecords,
    bool* pDoesMatch
)
{
    RC rc;
    auto skuIffIter = skuRecords.begin();
    bool mismatch = false;
    auto chipIffIter = chipRecords.begin();

    while (chipIffIter != chipRecords.end())
    {
        if ((*chipIffIter)->IsIlwalidatedRecord() || (*chipIffIter)->IsNullRecord())
        {
            chipIffIter++;
            continue;
        }
        if (skuIffIter == skuRecords.end() || *(*chipIffIter) != *(*skuIffIter))
        {
            mismatch = true;
            break;
        }
        skuIffIter++;
        chipIffIter++;
    }
    if (mismatch || skuIffIter != skuRecords.end())
    {
        *pDoesMatch = false;
    }
    return rc;
}
