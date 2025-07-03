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

#include "gpu/fuse/hopperiffrecord.h"
#include "gpu/fuse/fuseutils.h"

HopperIffRecord::HopperIffRecord(UINT32 firstRecord)
{
    if ((firstRecord & s_TypeMask) == 0)
    {
        Printf(FuseUtils::GetWarnPriority(), "Read IFF with invalid type mask\n");
    }
    IffRecord::AddData(firstRecord);
    m_IffType = static_cast<HopperIffType>(firstRecord & s_OpCodeMask);
}

/* virtual */ UINT32 HopperIffRecord::GetIffNumRows() const
{
    return GetIffNumRows(UINT32(m_IffType));
}

/* virtual */ UINT32 HopperIffRecord::GetIffNumRows(const UINT32 opCode) const
{
    UINT32 opCodeRows = 1;
    switch (static_cast<HopperIffType>(opCode))
    {
        case HopperIffType::NULL_RECORD :
        case HopperIffType::CONTROL_SET_IFF_EXELWTION_FILTER :
        case HopperIffType::CONTROL_POLL_UNTIL_NO_PRI_ERROR :
        case HopperIffType::WRITE_SET_FIELD :
            break;
        case HopperIffType::POLL_RETRY_UNTIL_EQUAL :
        case HopperIffType::POLL_RETRY_UNTIL_LESS_THAN :
        case HopperIffType::POLL_RETRY_UNTIL_GREATER_THAN :
        case HopperIffType::POLL_RETRY_UNTIL_NOT_EQUAL :
        case HopperIffType::POLL_ABORT_UNTIL_EQUAL :
        case HopperIffType::POLL_ABORT_UNTIL_LESS_THAN :
        case HopperIffType::POLL_ABORT_UNTIL_GREATER_THAN :
        case HopperIffType::POLL_ABORT_UNTIL_NOT_EQUAL :
        case HopperIffType::WRITE_MODIFY_DW :
            opCodeRows += 2; break;
        case HopperIffType::WRITE_WRITE_DW :
            opCodeRows += 1; break;
        default:
            Printf(FuseUtils::GetErrorPriority(), "Unknown IFF record: 0x%08x\n", opCode);
            return RC::FUSE_VALUE_OUT_OF_RANGE;
    }
    return opCodeRows;
}