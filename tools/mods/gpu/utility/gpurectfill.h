/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2013,2015-2016,2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "core/include/rc.h"
#include "core/include/memoryif.h"

#include<memory>

class Channel;
class Surface2D;
class GpuRectFillImpl;
class GpuTestConfiguration;

//! \class GpuRectFill
//!
//! \brief Fill rectangles within a surface using GPU rectangle fills.
//!
class GpuRectFill
{
public:
    GpuRectFill();
    ~GpuRectFill();
    RC Cleanup();
    RC Initialize(GpuTestConfiguration* pTestConfig);

    RC SetSurface(Surface2D* pSurf);
    RC SetSurface(Surface2D* pSurf, ColorUtils::Format format);
    RC SetSurface
    (
        Surface2D*         pSurf,
        ColorUtils::Format format,
        UINT32             width,
        UINT32             height,
        UINT32             pitch
    );
    RC Fill
    (
        UINT32 x,
        UINT32 y,
        UINT32 width,
        UINT32 height,
        UINT32 color,
        bool   shouldWait
    );
    RC FillRgbBars(bool shouldWait);
    RC Wait();

private:
    bool                        m_Initialized = false; //!< true if initialized
    unique_ptr<GpuRectFillImpl> m_pGpuRectFillImpl;    //!< Gpu fill implementer

    //! Test configuration for the fill, used to allocate channels if necessary
    Surface2D *        m_pSurface   = nullptr; //!< Surface to fill
    ColorUtils::Format m_SurfFormat = ColorUtils::LWFMT_NONE; //!< Format override for the surface
    UINT32             m_SurfWidth  = 0; //!< Width override for the surface
    UINT32             m_SurfHeight = 0; //!< Height override for the surface
    UINT32             m_SurfPitch  = 0; //!< Pitch override for the surface

    //! Synchronisation bookkeeping. We keep track of number of waits/flushes
    //! we had, keeping then them in sync. We are in stationary state when
    //! number of waits equals number of flushes (duh.)

    UINT32 m_NumFlushes = 0; //!< Num times write operation has been inited
    UINT32 m_NumWaits   = 0; //!< Num times we waited on a write to finish
};
