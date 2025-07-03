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

#include "gpu/fuse/iffrecord.h"
#include "gpu/fuse/fuseutils.h"

IffRecord::IffRecord(UINT32 firstRecord)
{
    if ((firstRecord & s_TypeMask) == 0)
    {
        Printf(FuseUtils::GetWarnPriority(), "Read IFF with invalid type mask\n");
    }
    m_Data.push_back(firstRecord);
    switch (static_cast<IffType>(firstRecord & s_OpCodeMask))
    {
        case IffType::NULL_RECORD: m_IffType = IffType::NULL_RECORD; break;
        case IffType::MODIFY_DW:   m_IffType = IffType::MODIFY_DW; break;
        case IffType::WRITE_DW:    m_IffType = IffType::WRITE_DW; break;
        case IffType::SET_FIELD:   m_IffType = IffType::SET_FIELD; break;
        case IffType::POLL_DW:     m_IffType = IffType::POLL_DW; break;
        case IffType::ILWALIDATED: m_IffType = IffType::ILWALIDATED; break;
        default:
            m_IffType = IffType::UNKNOWN;
            Printf(FuseUtils::GetWarnPriority(), "Unknown IFF record: 0x%08x\n", firstRecord);
            break;
    }
}

/* virtual */ UINT32 IffRecord::GetIffNumRows() const
{
    return GetIffNumRows(UINT32(m_IffType));
}

/* virtual */ UINT32 IffRecord::GetIffNumRows(const UINT32 opCode) const
{
    UINT32 opCodeRows = 1;
    switch (static_cast<IffType>(opCode))
    {
        case IffType::ILWALIDATED:
        case IffType::NULL_RECORD:
        case IffType::SET_FIELD:
            break;
        case IffType::WRITE_DW:
            opCodeRows += 1;
            break;
        case IffType::MODIFY_DW:
            opCodeRows += 2;
            break;
        case IffType::POLL_DW:
            opCodeRows += 2;
            break;
        default:
            Printf(FuseUtils::GetWarnPriority(), "Unknown IFF record: %#x\n", opCode);
            return RC::FUSE_VALUE_OUT_OF_RANGE;
    }
    return opCodeRows;
}
