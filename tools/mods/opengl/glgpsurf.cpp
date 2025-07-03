/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2011, 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! \file glgpsurf.cpp
//! \brief Implements the GLGpuGoldenSurfaces class.

#include "glgpugoldensurfaces.h"
#include "gpu/include/gpudev.h"
#include "core/include/display.h"

GLGpuGoldenSurfaces::GLGpuGoldenSurfaces (GpuDevice * gpuDev)
:   GLGoldenSurfaces(),
    m_pGpuDev(gpuDev)
{
    MASSERT(m_pGpuDev);
}

GLGpuGoldenSurfaces::~GLGpuGoldenSurfaces()
{
}

//------------------------------------------------------------------------------
/* virtual */
RC GLGpuGoldenSurfaces::FetchAndCallwlate
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
    RC rc;

    // We handle only one callwlation at a time, calcMethod should have
    // only one bit set.
    MASSERT(0 == (calcMethod & (calcMethod -1)));

    MASSERT(pRtnCodeValues);

    ::Display * pDisplay = m_pGpuDev->GetDisplay();

    switch (calcMethod)
    {
        case Goldelwalues::DacCrc:
        {
            rc = pDisplay->GetCrcValues(
                    Display(surfNum),
                    pRtnCodeValues + 0,
                    pRtnCodeValues + 1,
                    pRtnCodeValues + 2);
            break;
        }
        case Goldelwalues::TmdsCrc:
        {
            rc = pDisplay->GetTmdsCrcValues(
                    Display(surfNum),
                    pRtnCodeValues);
            break;
        }
        default:
        {
            rc = GLGoldenSurfaces::FetchAndCallwlate(
                    surfNum,
                    subdevNum,
                    bufFetchHint,
                    numCodeBins,
                    calcMethod,
                    pRtnCodeValues,
                    surfDumpBuffer);
            break;
        }
    }
    return rc;
}

