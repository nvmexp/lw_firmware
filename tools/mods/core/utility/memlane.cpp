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

#include "core/include/memlane.h"

#ifdef MATS_STANDALONE
#   include "fakemods.h"
#else
#   include "core/include/massert.h"
#   include "core/include/utility.h"
#endif

#include <algorithm>

using namespace Memory;

namespace
{
    constexpr UINT32 dwordBitWidth = 32;
}

/******************************************************************************/
// Lane

//!
//! \brief Colwert the given lane type to a string.
//!
//! \param laneType Lane to colwert.
//! \return String representation of the lane type. Return UNKNOWN if the
//! colwersion is unknown.
//!
string Lane::GetLaneTypeString(Type laneType)
{
    switch (laneType)
    {
        case Type::DATA:
            return "DATA";

        case Type::DBI:
            return "DBI";

        case Type::DM:
            return "DM";

        case Type::ADDR:
            return "ADDR";

        default:
            Printf(Tee::PriWarn, "GetLaneTypeString received unhandled value\n");
            // [[fallthrough]] @c++17
        case Type::UNKNOWN:
            return "UNKNOWN";
    }
}

//!
//! \brief Colwert the given string to a lane type.
//!
//! \param pStr C string to colwert.
//! \return Lane type represented by the string. "UNKNOWN" if the colwersion
//! is unknown.
//!
Lane::Type Lane::GetLaneTypeFromString(const char* pStr)
{
    MASSERT(pStr);

    return GetLaneTypeFromString(string(pStr));
}

//!
//! \brief Colwert the given string to a lane type.
//!
//! \param str String to colwert.
//! \return Lane type represented by the string. "UNKNOWN" if the colwersion
//! is unknown.
//!
Lane::Type Lane::GetLaneTypeFromString(string str)
{
    std::transform(str.begin(), str.end(), str.begin(),
                   [](unsigned char c) { return std::toupper(c); });

    if (str == "DATA")
    {
        return Type::DATA;
    }
    else if (str == "DBI")
    {
        return Type::DBI;
    }
    else if (str == "DM" || str == "ECC") // Share the same physical lane
    {
        return Type::DM;
    }
    else if (str == "ADDR" || str == "CMD") // Interchangable terms
    {
        return Type::ADDR;
    }
    else if (str == "UNKNOWN")
    {
        return Type::UNKNOWN;
    }

    Printf(Tee::PriWarn, "GetLaneTypeFromString received unhandled value");
    return Type::UNKNOWN;
}

/******************************************************************************/
// FbpaLane

//!
//! \brief Colwenience constructor for when the lane bit does not matter.
//!
//! \param hwFbpa Hardware FBPA.
//! \param subp FBPA subpartition.
//! \param subpBusWidth Size of the subpartition in bits.
//! \param dword Dword within the subpartition.
//!
FbpaLane::FbpaLane(HwFbpa hwFbpa, FbpaSubp subp, UINT32 subpBusWidth, UINT32 dword)
    : Lane(dword * dwordBitWidth + subp * subpBusWidth, Type::UNKNOWN), hwFbpa(hwFbpa)
{
    MASSERT(subpBusWidth % 32 == 0); // Should be some multiple of a dword
}

FbpaLane& FbpaLane::operator=(const FbpaLane& o)
{
    Lane::operator=(o);
    hwFbpa = o.hwFbpa;
    return *this;
}

FbpaLane& FbpaLane::operator=(FbpaLane&& o)
{
    Lane::operator=(std::move(o));
    hwFbpa = std::move(o.hwFbpa);
    return *this;
}

//!
//! \brief Return string representation of the lane.
//!
//! Format:
//! <fbpa_letter><fbpa_lane> <repair_type>
//! such that 'fbpa_lane' is the failing lane on FBPA letter 'fbpa_letter'.
//!
//! Ex: 'P032 DATA' => Data lane error on FBPA P, lane bit 32.
//!
string FbpaLane::ToString() const
{
    return Utility::StrPrintf("%c%03u %s", hwFbpa.Letter(), laneBit,
                              GetLaneTypeString(laneType).c_str());
}

/******************************************************************************/
// FbioLane

//!
//! \brief Colwenience constructor for when the lane bit does not matter.
//!
//! \param hwFbio Hardware FBIO.
//! \param subp FBIO subpartition.
//! \param subpBusWidth Size of the subpartition in bits.
//! \param dword Dword within the subpartition.
//!
FbioLane::FbioLane(HwFbio hwFbio, FbioSubp subp, UINT32 subpBusWidth, UINT32 dword)
    : Lane(dword * dwordBitWidth + subp * subpBusWidth, Type::UNKNOWN), hwFbio(hwFbio)
{
    MASSERT(subpBusWidth % 32 == 0); // Should be some multiple of a dword
}

FbioLane& FbioLane::operator=(const FbioLane& o)
{
    Lane::operator=(o);
    hwFbio = o.hwFbio;
    return *this;
}

FbioLane& FbioLane::operator=(FbioLane&& o)
{
    Lane::operator=(std::move(o));
    hwFbio = std::move(o.hwFbio);
    return *this;
}
