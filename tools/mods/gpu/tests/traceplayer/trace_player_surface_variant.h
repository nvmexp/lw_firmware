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

#pragma once

namespace MfgTracePlayer
{
    /// Which surface variant to use.
    ///
    /// Note: Each surface is split into three Surface2D objects,
    /// one for virtual allocation, one for physical allocation and
    /// one for the mapping.  This enum lets us select which Surface2D
    /// object is being modified/allocated/etc.
    enum class Variant : unsigned
    {
        Virtual  = 1 << 0,
        Physical = 1 << 1,
        Map      = 1 << 2,
        All      = Virtual | Physical | Map
    };

    inline constexpr bool operator&(Variant a, Variant b)
    {
        return !! (static_cast<unsigned>(a) & static_cast<unsigned>(b));
    }
}
