/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "core/include/memlane.h"
#include "core/include/mle_protobuf.h"

namespace Mle
{
    inline Mle::HbmRepair::Type ToLane(Memory::Lane::Type laneType)
    {
        switch (laneType)
        {
            case Memory::Lane::Type::DATA:    return Mle::HbmRepair::data;
            case Memory::Lane::Type::DBI:     return Mle::HbmRepair::dbi;
            case Memory::Lane::Type::DM:      return Mle::HbmRepair::dm;
            case Memory::Lane::Type::ADDR:    return Mle::HbmRepair::addr;
            case Memory::Lane::Type::UNKNOWN: return Mle::HbmRepair::unknown_ty;
            default:
                MASSERT(!"Unknown RepairType colwersion");
                break;
        }
        return Mle::HbmRepair::unknown_ty;
    }
}
