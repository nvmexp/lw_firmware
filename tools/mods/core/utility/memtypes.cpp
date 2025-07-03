/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/memtypes.h"

const char* Memory::GetMemoryLocationName(Memory::Location Loc)
{
    switch (Loc)
    {
        case Memory::Fb:
            return "Memory::Fb";
        case Memory::Coherent:
            return "Memory::Coherent";
        case Memory::NonCoherent:
            return "Memory::NonCoherent";
        case Memory::CachedNonCoherent:
            return "Memory::CachedNonCoherent";
        case Memory::Optimal:
            return "Memory::Optimal";
        default:
            MASSERT(0);
            return "illegal value";
    }
}

const char* Memory::GetMemoryAddressModelName(Memory::AddressModel Model)
{
    switch (Model)
    {
        case Memory::Segmentation:
            return "Memory::Segmentation";
        case Memory::Paging:
            return "Memory::Paging";
        case Memory::OptimalAddress:
            return "Memory::OptimalAddress";
        default:
            MASSERT(0);
            return "illegal value";
    }
}

string Memory::ToString(ErrorType errType)
{
    switch (errType)
    {
        case ErrorType::NONE: return "none";
        case ErrorType::SBE:  return "SBE";
        case ErrorType::DBE:  return "DBE";
        default:
            MASSERT(!"Unsupported memory error type\n");
            return "unknown";
    }
}

string Memory::ToString(RepairType repairType)
{
    switch (repairType)
    {
        case RepairType::HARD: return "hard";
        case RepairType::SOFT: return "soft";
        default:
            MASSERT(!"Unknown repair type");
            // [[fallthrough]] @c++17
        case RepairType::UNKNOWN: return "unknown";
    }
}
