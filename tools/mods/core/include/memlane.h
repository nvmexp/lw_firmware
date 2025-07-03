/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019, 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "core/include/memtypes.h"

#include <tuple>

namespace Memory
{
    //!
    //! \brief A single lane to memory.
    //!
    //! All data is in reference to the hardware FBPA.
    //!
    struct Lane
    {
        //! Lane type
        enum class Type : UINT32
        {
            DATA,
            DBI,
            DM,
            ADDR,
            UNKNOWN = _UINT32_MAX
        };
        using RepairType [[deprecated("Legacy code compatibility")]] = Type;

        UINT32 laneBit;                //!< Lane bit
        Type laneType = Type::UNKNOWN; //!< Lane type

        Lane() = default;
        Lane(UINT32 lane, Type laneType) : laneBit(lane), laneType(laneType) {}

        static string GetLaneTypeString(Type laneType);
        static Type GetLaneTypeFromString(const char* pStr);
        static Type GetLaneTypeFromString(string str);

        //!
        //! \brief Equality operator. Unknown lane types can never be equal to
        //! another lane.
        //!
        friend bool operator==(const Lane& l, const Lane& r)
        {
            return (l.laneType != Type::UNKNOWN && r.laneType != Type::UNKNOWN)
                && (l.laneType == r.laneType)
                && (l.laneBit == r.laneBit);
        }
        friend bool operator!=(const Lane& l, const Lane& r) { return !(l == r); }

        friend bool operator<(const Lane& l, const Lane& r)
        {
            return std::tie(l.laneType, l.laneBit) < std::tie(r.laneType, r.laneBit);
        }
    };

    //!
    //! \brief GPU FBPA lane.
    //!
    //! The 'laneBit' field represents an FBPA lane.
    //!
    struct FbpaLane : public Lane
    {
        HwFbpa hwFbpa;  //!< Hardware FBPA

        FbpaLane() : Lane(0, Type::UNKNOWN), hwFbpa(0) {}

        //!
        //! \brief Constructor.
        //!
        //! \param hwFbpa Hardware FBPA.
        //! \param laneBit FBPA lane bit.
        //! \param laneType Type of lane.
        //!
        FbpaLane(HwFbpa hwFbpa, UINT32 laneBit, Type laneType)
            : Lane(laneBit, laneType), hwFbpa(hwFbpa) {}
        FbpaLane(HwFbpa hwFbpa, FbpaSubp subp, UINT32 subpBusWidth, UINT32 dword);

        FbpaLane(const FbpaLane& o) : Lane(o), hwFbpa(o.hwFbpa) {}
        FbpaLane(FbpaLane&& o) : Lane(std::move(o)), hwFbpa(std::move(o.hwFbpa)) {}

        string ToString() const;

        FbpaLane& operator=(const FbpaLane& o);
        FbpaLane& operator=(FbpaLane&& o);

        //!
        //! \brief Equality operator. Unknown lane types can never be equal to
        //! another lane.
        //!
        friend bool operator==(const FbpaLane& l, const FbpaLane& r)
        {
            return l.hwFbpa == r.hwFbpa
                && static_cast<const Lane&>(l) == static_cast<const Lane&>(r);
        }
        friend bool operator!=(const FbpaLane& l, const FbpaLane& r) { return !(l == r); }

        friend bool operator<(const FbpaLane& l, const FbpaLane& r)
        {
            Lane lLane = static_cast<const Lane&>(l);
            Lane rLane = static_cast<const Lane&>(r);
            return (std::tie(lLane.laneType, l.hwFbpa, lLane.laneBit) <
                    std::tie(rLane.laneType, r.hwFbpa, rLane.laneBit));
        }

        friend ostream& operator<<(ostream& os, const FbpaLane& lane)
        {
            return os << lane.ToString();
        }
    };

    //!
    //! \brief GPU FBIO lane.
    //!
    //! The 'laneBit' field represents an FBIO lane.
    //!
    struct FbioLane : public Lane
    {
        HwFbio hwFbio;  //!< Hardware FBIO

        FbioLane() : Lane(0, Type::UNKNOWN), hwFbio(0) {}

        //!
        //! \brief Constructor.
        //!
        //! \param hwFbio Hardware FBIO.
        //! \param laneBit FBIO lane bit.
        //! \param laneType Type of lane.
        //!
        FbioLane(HwFbio hwFbio, UINT32 laneBit, Type laneType)
            : Lane(laneBit, laneType), hwFbio(hwFbio)
        {}
        FbioLane(HwFbio hwFbio, FbioSubp subp, UINT32 subpBusWidth, UINT32 dword);

        FbioLane(const FbioLane& o) : Lane(o), hwFbio(o.hwFbio) {}
        FbioLane(FbioLane&& o) : Lane(std::move(o)), hwFbio(std::move(o.hwFbio)) {}

        FbioLane& operator=(const FbioLane& o);
        FbioLane& operator=(FbioLane&& o);

        //!
        //! \brief Equality operator. Unknown lane types can never be equal to
        //! another lane.
        //!
        friend bool operator==(const FbioLane& l, const FbioLane& r)
        {
            return l.hwFbio == r.hwFbio
                && static_cast<const Lane&>(l) == static_cast<const Lane&>(r);
        }
        friend bool operator!=(const FbioLane& l, const FbioLane& r) { return !(l == r); }

        friend bool operator<(const FbioLane& l, const FbioLane& r)
        {
            return l.hwFbio < r.hwFbio
                && static_cast<const Lane&>(l) < static_cast<const Lane&>(r);
        }
    };

} // namespace Memory
