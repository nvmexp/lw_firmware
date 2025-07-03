/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/fuse/encoding/hopperiffencoder.h"

#include "hopper/gh100/dev_master.h"
#include "gpu/fuse/fuseutils.h"
#include "gpu/fuse/hopperiffrecord.h"
#include "gpu/reghal/reghal.h"

// CRC Callwlation algorithm: https://p4hw-swarm.lwpu.com/reviews/51222008/v1/
bitset<16> HopperIffEncoder::CrcStep
(
    const bitset<16>& crcIn,
    const bitset<32>& dataBin
)
{
    bitset<16> crcOut;

    crcOut[15] = dataBin[31] ^ dataBin[30] ^ dataBin[29] ^ dataBin[28] ^ dataBin[27] ^ dataBin[26]
               ^ dataBin[25] ^ dataBin[24] ^ dataBin[23] ^ dataBin[22] ^ dataBin[21] ^ dataBin[20]
               ^ dataBin[19] ^ dataBin[18] ^ dataBin[16] ^ crcIn[15] ^ dataBin[15] ^ crcIn[14]
               ^ dataBin[14] ^ crcIn[13] ^ dataBin[13] ^ crcIn[12] ^ dataBin[12] ^ crcIn[11]
               ^ dataBin[11] ^ crcIn[10] ^ dataBin[10] ^ crcIn[9] ^ dataBin[9] ^ crcIn[8]
               ^ dataBin[8] ^ crcIn[7] ^ dataBin[7] ^ crcIn[6] ^ dataBin[6] ^ crcIn[5] ^ dataBin[5]
               ^ crcIn[4] ^ dataBin[4] ^ crcIn[1] ^ dataBin[1] ^ crcIn[0] ^ dataBin[0];

    crcOut[14] = dataBin[30] ^ dataBin[29] ^ dataBin[28] ^ dataBin[27] ^ dataBin[26] ^ dataBin[25]
               ^ dataBin[24] ^ dataBin[23] ^ dataBin[22] ^ dataBin[21] ^ dataBin[20] ^ dataBin[19]
               ^ dataBin[18] ^ dataBin[17] ^ crcIn[15] ^ dataBin[15] ^ crcIn[14] ^ dataBin[14]
               ^ crcIn[13] ^ dataBin[13] ^ crcIn[12] ^ dataBin[12] ^ crcIn[11] ^ dataBin[11]
               ^ crcIn[10] ^ dataBin[10] ^ crcIn[9] ^ dataBin[9] ^ crcIn[8] ^ dataBin[8] ^ crcIn[7]
               ^ dataBin[7] ^ crcIn[6] ^ dataBin[6] ^ crcIn[5] ^ dataBin[5] ^ crcIn[4] ^ dataBin[4]
               ^ crcIn[3] ^ dataBin[3] ^ crcIn[0] ^ dataBin[0];

    crcOut[13] = dataBin[31] ^ dataBin[30] ^ dataBin[17] ^ crcIn[15] ^ dataBin[15] ^ crcIn[3]
               ^ dataBin[3] ^ crcIn[2] ^ dataBin[2] ^ crcIn[1] ^ dataBin[1] ^ crcIn[0]
               ^ dataBin[0];

    crcOut[12] = dataBin[30] ^ dataBin[29] ^ dataBin[16] ^ crcIn[14] ^ dataBin[14] ^ crcIn[2]
               ^ dataBin[2] ^ crcIn[1] ^ dataBin[1] ^ crcIn[0] ^ dataBin[0];

    crcOut[11] = dataBin[29] ^ dataBin[28] ^ crcIn[15] ^ dataBin[15] ^ crcIn[13] ^ dataBin[13]
               ^ crcIn[1] ^ dataBin[1] ^ crcIn[0] ^ dataBin[0];

    crcOut[10] = dataBin[28] ^ dataBin[27] ^ crcIn[14] ^ dataBin[14] ^ crcIn[12] ^ dataBin[12]
               ^ crcIn[0] ^ dataBin[0];

    crcOut[9] = dataBin[27] ^ dataBin[26] ^ crcIn[13] ^ dataBin[13] ^ crcIn[11] ^ dataBin[11];

    crcOut[8] = dataBin[26] ^ dataBin[25] ^ crcIn[12] ^ dataBin[12] ^ crcIn[10] ^ dataBin[10];

    crcOut[7] = dataBin[25] ^ dataBin[24] ^ crcIn[11] ^ dataBin[11] ^ crcIn[9] ^ dataBin[9];

    crcOut[6] = dataBin[24] ^ dataBin[23] ^ crcIn[10] ^ dataBin[10] ^ crcIn[8] ^ dataBin[8];

    crcOut[5] = dataBin[23] ^ dataBin[22] ^ crcIn[9] ^ dataBin[9] ^ crcIn[7] ^ dataBin[7];

    crcOut[4] = dataBin[22] ^ dataBin[21] ^ crcIn[8] ^ dataBin[8] ^ crcIn[6] ^ dataBin[6];

    crcOut[3] = dataBin[21] ^ dataBin[20] ^ crcIn[7] ^ dataBin[7] ^ crcIn[5] ^ dataBin[5];

    crcOut[2] = dataBin[20] ^ dataBin[19] ^ crcIn[6] ^ dataBin[6] ^ crcIn[4] ^ dataBin[4];

    crcOut[1] = dataBin[19] ^ dataBin[18] ^ crcIn[5] ^ dataBin[5] ^ crcIn[3] ^ dataBin[3];

    crcOut[0] = dataBin[31] ^ dataBin[30] ^ dataBin[29] ^ dataBin[28] ^ dataBin[27] ^ dataBin[26]
              ^ dataBin[25] ^ dataBin[24] ^ dataBin[23] ^ dataBin[22] ^ dataBin[21] ^ dataBin[20]
              ^ dataBin[19] ^ dataBin[17] ^ dataBin[16] ^ crcIn[15] ^ dataBin[15] ^ crcIn[14]
              ^ dataBin[14] ^ crcIn[13] ^ dataBin[13] ^ crcIn[12] ^ dataBin[12] ^ crcIn[11]
              ^ dataBin[11] ^ crcIn[10] ^ dataBin[10] ^ crcIn[9] ^ dataBin[9] ^ crcIn[8]
              ^ dataBin[8] ^ crcIn[7] ^ dataBin[7] ^ crcIn[6] ^ dataBin[6] ^ crcIn[5] ^ dataBin[5]
              ^ crcIn[2] ^ dataBin[2] ^ crcIn[1] ^ dataBin[1] ^ crcIn[0] ^ dataBin[0];

    return crcOut;
}

RC HopperIffEncoder::CallwlateCrc(const vector<UINT32>& iffRows, UINT32* pCrcVal)
{
    MASSERT(pCrcVal != nullptr);

    bitset<16> crcIn;

    for (UINT32 iffRow: iffRows)
    {
        bitset<32> dataBin(iffRow);
        crcIn = CrcStep(crcIn, dataBin);
    }

    UINT32 finalValue = 0;
    for (int i = 0; i < 16; i++)
    {
        finalValue |= (crcIn[i] << i);
    }

    *pCrcVal = finalValue;

    return RC::OK;
}

//------------------------------------------------------------------------------
// Decode and print an IFF record in human-readable format
// Return true if it is a valid IFF record
/* virtual */ RC HopperIffEncoder::DecodeIffRecord
(
    const shared_ptr<IffRecord>& pRecord,
    string &output,
    bool* pIsValid
)
{
    // Record format details from -
    // https://p4hw-swarm.lwpu.com/files/hw/doc/gpu/hopper_info/gen_manuals/gh100/dev_bus.ref#5382

    shared_ptr<HopperIffRecord> pHopperIffRecord = dynamic_pointer_cast<HopperIffRecord>(pRecord);
    MASSERT(pHopperIffRecord);

    *pIsValid = true;
    switch (pHopperIffRecord->GetHopperIffType())
    {

        // LW_IFF_CMD_SET_IFF_EXELWTION_FILTER Command format details from -
        // https://p4hw-swarm.lwpu.com/files/hw/doc/gpu/hopper_info/gen_manuals/gh100/dev_pmc.ref#1876
        case HopperIffRecord::HopperIffType::CONTROL_SET_IFF_EXELWTION_FILTER:
        {
            MASSERT(pRecord->GetDataList().size() == 1);
            UINT32 mask = DRF_VAL(_IFF_CMD, _SET_IFF_EXELWTION_FILTER, _MASK, pRecord->GetData(0));

            output += Utility::StrPrintf
                        (
                            "Set IFF exection filter control: (Mask: 0x%02x)",
                            mask
                        );
            break;
        }
        // LW_IFF_CMD_POLL_UNTIL_NO_PRI_ERROR Command format details from -
        // https://p4hw-swarm.lwpu.com/files/hw/doc/gpu/hopper_info/gen_manuals/gh100/dev_pmc.ref#1930
        case HopperIffRecord::HopperIffType::CONTROL_POLL_UNTIL_NO_PRI_ERROR:
        {
            MASSERT(pRecord->GetDataList().size() == 1);
            UINT32 addr = DRF_VAL(_IFF_CMD, _POLL_UNTIL_NO_PRI_ERROR, _ADDR,
                                    pRecord->GetData(0));
            addr <<= LW_IFF_CONST_DEF_ADDR_SHIFT;

            output += Utility::StrPrintf
                        (
                            "Poll until no PRI error: ([0x%06x]",
                            addr
                        );
            break;
        }
        // LW_IFF_CMD_POLL Command format details from -
        // https://p4hw-swarm.lwpu.com/files/hw/doc/gpu/hopper_info/gen_manuals/gh100/dev_pmc.ref#1977
        case HopperIffRecord::HopperIffType::POLL_RETRY_UNTIL_EQUAL:
        {
            MASSERT(pRecord->GetDataList().size() == 3);
            UINT32 addr = DRF_VAL(_IFF_CMD, _POLL, _ADDR, pRecord->GetData(0));
            UINT32 mask = DRF_VAL(_IFF_CMD, _POLL, _MASK, pRecord->GetData(1));
            UINT32 value = DRF_VAL(_IFF_CMD, _POLL, _VALUE, pRecord->GetData(2));
            addr <<= LW_IFF_CONST_DEF_ADDR_SHIFT;

            output += Utility::StrPrintf
                        (
                            "Poll retry until equal: ([0x%06x] & 0x%08x) == (0x%08x & 0x%08x)",
                            addr, mask, value, mask
                        );
            break;
        }
        case HopperIffRecord::HopperIffType::POLL_RETRY_UNTIL_LESS_THAN:
        {
            MASSERT(pRecord->GetDataList().size() == 3);
            UINT32 addr = DRF_VAL(_IFF_CMD, _POLL, _ADDR, pRecord->GetData(0));
            UINT32 mask = DRF_VAL(_IFF_CMD, _POLL, _MASK, pRecord->GetData(1));
            UINT32 value = DRF_VAL(_IFF_CMD, _POLL, _VALUE, pRecord->GetData(2));
            addr <<= LW_IFF_CONST_DEF_ADDR_SHIFT;

            output += Utility::StrPrintf
                        (
                            "Poll retry until less than: ([0x%06x] & 0x%08x) < (0x%08x & 0x%08x)",
                            addr, mask, value, mask
                        );
            break;
        }
        case HopperIffRecord::HopperIffType::POLL_RETRY_UNTIL_GREATER_THAN:
        {
            MASSERT(pRecord->GetDataList().size() == 3);
            UINT32 addr = DRF_VAL(_IFF_CMD, _POLL, _ADDR, pRecord->GetData(0));
            UINT32 mask = DRF_VAL(_IFF_CMD, _POLL, _MASK, pRecord->GetData(1));
            UINT32 value = DRF_VAL(_IFF_CMD, _POLL, _VALUE, pRecord->GetData(2));
            addr <<= LW_IFF_CONST_DEF_ADDR_SHIFT;

            output += Utility::StrPrintf
                        (
                            "Poll retry until greater than: ([0x%06x] & 0x%08x) > "
                            "(0x%08x & 0x%08x)", addr, mask, value, mask
                        );
            break;
        }
        case HopperIffRecord::HopperIffType::POLL_RETRY_UNTIL_NOT_EQUAL:
        {
            MASSERT(pRecord->GetDataList().size() == 3);
            UINT32 addr = DRF_VAL(_IFF_CMD, _POLL, _ADDR, pRecord->GetData(0));
            UINT32 mask = DRF_VAL(_IFF_CMD, _POLL, _MASK, pRecord->GetData(1));
            UINT32 value = DRF_VAL(_IFF_CMD, _POLL, _VALUE, pRecord->GetData(2));
            addr <<= LW_IFF_CONST_DEF_ADDR_SHIFT;

            output += Utility::StrPrintf
                        (
                            "Poll retry until not equal: ([0x%06x] & 0x%08x) != (0x%08x & 0x%08x)",
                            addr, mask, value, mask
                        );
            break;
        }
        case HopperIffRecord::HopperIffType::POLL_ABORT_UNTIL_EQUAL:
        {
            MASSERT(pRecord->GetDataList().size() == 3);
            UINT32 addr = DRF_VAL(_IFF_CMD, _POLL, _ADDR, pRecord->GetData(0));
            UINT32 mask = DRF_VAL(_IFF_CMD, _POLL, _MASK, pRecord->GetData(1));
            UINT32 value = DRF_VAL(_IFF_CMD, _POLL, _VALUE, pRecord->GetData(2));
            addr <<= LW_IFF_CONST_DEF_ADDR_SHIFT;

            output += Utility::StrPrintf
                        (
                            "Poll abort until equal: ([0x%06x] & 0x%08x) == (0x%08x & 0x%08x)",
                            addr, mask, value, mask
                        );
            break;
        }
        case HopperIffRecord::HopperIffType::POLL_ABORT_UNTIL_LESS_THAN:
        {
            MASSERT(pRecord->GetDataList().size() == 3);
            UINT32 addr = DRF_VAL(_IFF_CMD, _POLL, _ADDR, pRecord->GetData(0));
            UINT32 mask = DRF_VAL(_IFF_CMD, _POLL, _MASK, pRecord->GetData(1));
            UINT32 value = DRF_VAL(_IFF_CMD, _POLL, _VALUE, pRecord->GetData(2));
            addr <<= LW_IFF_CONST_DEF_ADDR_SHIFT;

            output += Utility::StrPrintf
                        (
                            "Poll abort until less than: ([0x%06x] & 0x%08x) < (0x%08x & 0x%08x)",
                            addr, mask, value, mask
                        );
            break;
        }
        case HopperIffRecord::HopperIffType::POLL_ABORT_UNTIL_GREATER_THAN:
        {
            MASSERT(pRecord->GetDataList().size() == 3);
            UINT32 addr = DRF_VAL(_IFF_CMD, _POLL, _ADDR, pRecord->GetData(0));
            UINT32 mask = DRF_VAL(_IFF_CMD, _POLL, _MASK, pRecord->GetData(1));
            UINT32 value = DRF_VAL(_IFF_CMD, _POLL, _VALUE, pRecord->GetData(2));
            addr <<= LW_IFF_CONST_DEF_ADDR_SHIFT;

            output += Utility::StrPrintf
                        (
                            "Poll abort until greater than: ([0x%06x] & 0x%08x) > "
                            "(0x%08x & 0x%08x)", addr, mask, value, mask
                        );
            break;
        }
        case HopperIffRecord::HopperIffType::POLL_ABORT_UNTIL_NOT_EQUAL:
        {
            MASSERT(pRecord->GetDataList().size() == 3);
            UINT32 addr = DRF_VAL(_IFF_CMD, _POLL, _ADDR, pRecord->GetData(0));
            UINT32 mask = DRF_VAL(_IFF_CMD, _POLL, _MASK, pRecord->GetData(1));
            UINT32 value = DRF_VAL(_IFF_CMD, _POLL, _VALUE, pRecord->GetData(2));
            addr <<= LW_IFF_CONST_DEF_ADDR_SHIFT;

            output += Utility::StrPrintf
                        (
                            "Poll abort until not equal: ([0x%06x] & 0x%08x) != (0x%08x & 0x%08x)",
                            addr, mask, value, mask
                        );
            break;
        }
        // LW_IFF_CMD_MODIFY_DW Command format details from -
        // https://p4hw-swarm.lwpu.com/files/hw/doc/gpu/hopper_info/gen_manuals/gh100/dev_pmc.ref#2125
        case HopperIffRecord::HopperIffType::WRITE_MODIFY_DW:
        {
            MASSERT(pRecord->GetDataList().size() == 3);

            UINT32 addr = DRF_VAL(_IFF_CMD, _MODIFY_DW, _ADDR, pRecord->GetData(0));
            UINT32 andMask = DRF_VAL(_IFF_CMD, _MODIFY_DW, _AND_MASK, pRecord->GetData(1));
            UINT32 orMask = DRF_VAL(_IFF_CMD, _MODIFY_DW, _OR_MASK, pRecord->GetData(2));
            addr <<= LW_IFF_CONST_DEF_ADDR_SHIFT;

            output += Utility::StrPrintf
                        (
                            "Modify: [0x%06x] = ([0x%06x] & 0x%08x) | 0x%08x",
                            addr, addr, andMask, orMask
                        );
            break;
        }
        // LW_IFF_CMD_WRITE_DW Command format details from -
        // https://p4hw-swarm.lwpu.com/files/hw/doc/gpu/hopper_info/gen_manuals/gh100/dev_pmc.ref#2193
        case HopperIffRecord::HopperIffType::WRITE_WRITE_DW:
        {
            MASSERT(pRecord->GetDataList().size() == 2);

            UINT32 addr = DRF_VAL(_IFF_CMD, _WRITE_DW, _ADDR, pRecord->GetData(0));
            UINT32 data = DRF_VAL(_IFF_CMD, _WRITE_DW, _DATA, pRecord->GetData(1));
            addr <<= LW_IFF_CONST_DEF_ADDR_SHIFT;

            output += Utility::StrPrintf
                        (
                            "Write: [0x%06x] = 0x%08x",
                            addr, data
                        );
            break;
        }
        // LW_IFF_CMD_SET_FIELD Command format details from -
        // https://p4hw-swarm.lwpu.com/files/hw/doc/gpu/hopper_info/gen_manuals/gh100/dev_pmc.ref#2245
        case HopperIffRecord::HopperIffType::WRITE_SET_FIELD:
        {
            MASSERT(pRecord->GetDataList().size() == 1);
            UINT32 data = pRecord->GetData(0);
            UINT32 addrOffset = DRF_VAL(_IFF_CMD, _SET_FIELD, _ADDR_OFFSET, data);
            UINT32 priSpace = DRF_VAL(_IFF_CMD, _SET_FIELD, _PRI_SPACE_ID, data);
            UINT32 lsbOffset = DRF_VAL(_IFF_CMD, _SET_FIELD, _LSB_OFFSET, data);
            UINT32 widthAndValue = DRF_VAL(_IFF_CMD, _SET_FIELD, _WIDTH_AND_VALUE, data);

            addrOffset <<= LW_IFF_CONST_DEF_ADDR_SHIFT;
            // Most significant set bit of the width_and_value spec determines the width
            UINT32 width = DRF_SIZE(LW_IFF_CMD_SET_FIELD_WIDTH_AND_VALUE) - 1;
            for (; width > 0; width--)
            {
                if ((widthAndValue >> width) & 1)
                {
                    break;
                }
            }
            UINT32 mask = (1 << width) - 1;

            output += Utility::StrPrintf
                        (
                            "Set field: (Space %2d) [Space Base Addr + 0x%06x] = "
                            "([Space Base Addr + 0x%06x] & ~(0x%08x << 0x%08x)) | "
                            "((0x%08x & 0x%08x) << 0x%08x)",
                            priSpace, addrOffset, addrOffset, mask, lsbOffset,
                            widthAndValue, mask, lsbOffset
                        );
            break;
        }
        case HopperIffRecord::HopperIffType::NULL_RECORD:
        case HopperIffRecord::HopperIffType::ILWALIDATED:
            Printf(Tee::PriLow, "Null or ilwalidated record\n");
            *pIsValid = false;
            break;
        default:
            Printf(Tee::PriError, "Unknown IFF record type\n");
            return RC::FUSE_VALUE_OUT_OF_RANGE;
    }
    return RC::OK;
}

/* virtual */ RC HopperIffEncoder::GetIffRecords
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
            shared_ptr<HopperIffRecord> iffRecord = make_shared<HopperIffRecord>(iffRowIter->data);
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

/* virtual */RC HopperIffEncoder::DecodeFuseValsImpl
(
    const vector<UINT32>& rawFuses,
    FuseValues* pFuseData
)
{
    MASSERT(pFuseData != nullptr);
    RC rc;
    pFuseData->m_IffRecords.clear();

    UINT32 iffStart = GetFuselessEnd() + 1;
    for (UINT32 i = GetFuselessStart(); i <= GetFuselessEnd(); i++)
    {
        UINT32 row = rawFuses[i];
        if (IsIffRow(row) && !IsIlwalidatedRow(row))
        {
            iffStart = i;
            break;
        }
    }

    for (UINT32 i = iffStart; i <= GetFuselessEnd(); i++)
    {
        UINT32 row = rawFuses[i];
        shared_ptr<HopperIffRecord> record = make_shared<HopperIffRecord>(row);
        UINT32 opCode = record->GetOpCode(row);
        UINT32 extraRows = record->GetIffNumRows(opCode) - 1;
        if (i + extraRows >= GetFuselessEnd() + 1)
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

    return rc;
}

/* virtual */ RC HopperIffEncoder::ValidateIffRecords
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
        shared_ptr<HopperIffRecord> pChipHopperIffRecord =
            dynamic_pointer_cast<HopperIffRecord>(*chipIffIter);
        shared_ptr<HopperIffRecord> pSkuHopperIffRecord =
            dynamic_pointer_cast<HopperIffRecord>(*skuIffIter);
        if ((*chipIffIter)->IsIlwalidatedRecord() || (*chipIffIter)->IsNullRecord())
        {
            chipIffIter++;
            continue;
        }
        if (skuIffIter == skuRecords.end() || *pChipHopperIffRecord != *pSkuHopperIffRecord)
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

/*virtual */ RC HopperIffEncoder::CheckIfRecordValid(const shared_ptr<IffRecord> pRecord)
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

    return RC::OK;
}
