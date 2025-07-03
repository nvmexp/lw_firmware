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

//! \file glgpugoldensurfaces.h
//! \brief Declares the GLGpuGoldenSurfaces class.
#ifndef INCLUDED_GLGPUGOLDENSURFACES_H
#define INCLUDED_GLGPUGOLDENSURFACES_H

#include "glgoldensurfaces.h"

//------------------------------------------------------------------------------
//! \brief Mods OpenGL- and GPU-specific derived class of GoldenSurfaces.
//!
//! Any mods Gpu GL test that needs to do DAC CRCs (i.e. glrandom) must use
//! this class rather than GLGoldenSurfaces.
//
class GLGpuGoldenSurfaces : public GLGoldenSurfaces
{
public:
    GLGpuGoldenSurfaces(GpuDevice * gpuDev);
    virtual ~GLGpuGoldenSurfaces();

    virtual RC FetchAndCallwlate
    (
        int surfNum,
        UINT32 subdevNum,
        Goldelwalues::BufferFetchHint bufFetchHint,
        UINT32 numCodeBins,
        Goldelwalues::Code calcMethod,
        UINT32 * pRtnCodeValues,
        vector<UINT08> *surfDumpBuffer
    );

private:
    GpuDevice *m_pGpuDev;
};
#endif //INCLUDED_GLGPUGOLDENSURFACES_H

