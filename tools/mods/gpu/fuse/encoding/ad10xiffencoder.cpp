/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/fuse/encoding/ad10xiffencoder.h"
#include "gpu/reghal/reghal.h"
#include "ada/ad102/dev_bus.h"

/* virtual */ RC AD10xIffEncoder::CheckIfRecordValid(const shared_ptr<IffRecord> pRecord)
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

/* virtual */ RC AD10xIffEncoder::DecodeIffRecord
(
    const shared_ptr<IffRecord>& pRecord,
    string &output,
    bool* pIsValid
)
{
    RC rc;
    MASSERT(pIsValid);

    *pIsValid = true;
    switch (pRecord->GetIffType())
    {
        case IffRecord::IffType::POLL_DW:
        {
            MASSERT(pRecord->GetDataList().size() == 3);

            UINT32 addr = DRF_VAL(_PBUS_FUSE_FMT_IFF_RECORD, _CMD_POLL_DW, _ADDR,
                                  pRecord->GetData(0));
            // Not a typo, the addr shift is same across both instructions
            addr <<= LW_PBUS_FUSE_FMT_IFF_RECORD_CMD_MODIFY_DW_ADDR_BYTESHIFT;

            UINT32 mask = DRF_VAL(_PBUS_FUSE_FMT_IFF_RECORD, _POLL_DW_MASK, _DATA,
                                 pRecord->GetData(1));
            UINT32 val  = DRF_VAL(_PBUS_FUSE_FMT_IFF_RECORD, _POLL_DW_VALUE, _DATA,
                                 pRecord->GetData(2));
            output += Utility::StrPrintf
                        (
                            "Poll retry until equal: ([0x%06x] & 0x%08x) == (0x%08x & 0x%08x)",
                            addr, mask, val, mask
                        );
        }
        break;
        default:
            return IffEncoder::DecodeIffRecord(pRecord, output, pIsValid);
    }
    return rc;
}
