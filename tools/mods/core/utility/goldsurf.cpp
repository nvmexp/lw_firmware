/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2011 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! \file goldsurf.cpp
//! \brief Implements the GoldenSurfaces class.

#include "core/include/golden.h"
#include "core/include/crc.h"

//******************************************************************************
// Default implementation for golden surface checksum & crc callwlation methods.
// This implementation assumes the data has been cached into the CPU's memory
// address space and the checksum / crc callwlation is done using the CPU.

/*virtual*/
RC GoldenSurfaces::FetchAndCallwlate
(
    int surfNum,
    UINT32 subdevNum,
    Goldelwalues::BufferFetchHint bufFetchHint,
    UINT32 numCodeBins,
    Goldelwalues::Code calcMethod,
    UINT32 * pRtnCodeValues,
    vector<UINT08> *surfDumpBuffer
)
{
    MASSERT(pRtnCodeValues);

    RC rc;
    SetSurfaceCheckMethod(surfNum, calcMethod);
    void * addr = GetCachedAddress(surfNum, bufFetchHint, subdevNum, surfDumpBuffer);
    MASSERT(addr);

    UINT32 width    = Width(surfNum);
    UINT32 height   = Height(surfNum);
    ColorUtils::Format fmt = Format(surfNum);

    // The CRC etc. code below doesn't understand Y-ilwerted surfaces!
    UINT32 absPitch = abs(Pitch(surfNum));

    // We handle only one callwlation at a time, calcMethod should have
    // only one bit set.
    MASSERT(0 == (calcMethod & (calcMethod -1)));

    // The CRC shader returns a different checksummed/CRCed surface
    GetModifiedSurfProp(surfNum, &width, &height, &absPitch);

    switch (calcMethod)
    {
        case Goldelwalues::CheckSums:
        {
            CheckSums::Callwlate (
                    addr,
                    width,
                    height,
                    fmt,
                    absPitch,
                    numCodeBins,
                    pRtnCodeValues);

            break;
        }
        case Goldelwalues::Crc:
        {
            Crc::Callwlate (
                    addr,
                    width,
                    height,
                    fmt,
                    absPitch,
                    numCodeBins,
                    pRtnCodeValues);
            break;
        }
        case Goldelwalues::OldCrc:
        {
            OldCrc::Callwlate (
                    addr,
                    width,
                    height,
                    fmt,
                    absPitch,
                    numCodeBins,
                    pRtnCodeValues);
            break;
        }
        default:
        {
            rc = RC::UNSUPPORTED_FUNCTION;
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
// Return the buffer ID if the surface is a SBO, otherwise return zero
UINT32 GoldenSurfaces::BufferId(int surfNum) const
{
    return 0;
}

