/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2012,2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file mgl_dtop.h
//! \brief Per-GpuDevice "desktop" structure for use with dgl Opengl driver.
//!
//! For use with mgl_gpu.cpp only, should be completely invisible to
//! the rest of mods.
//!

#ifndef INCLUDED_MGL_DTOP_H
#define INCLUDED_MGL_DTOP_H

#ifndef INCLUDED_MODSGL_H
#include "modsgl.h"
#endif
#ifndef INCLUDED_STL_MAP
#include <map>
#define INCLUDED_STL_MAP
#endif
#include "gpu/include/gpudev.h"
//------------------------------------------------------------------------------
//! \class mglDesktop -- manage per-GpuDevice GL driver context.
//
class mglDesktop
{

private:
    mglDesktop
    (
        GpuDevice *             pDev,
        UINT32                  displayWidth,
        UINT32                  displayHeight,
        UINT32                  surfaceWidth,
        UINT32                  surfaceHeight,
        UINT32                  depth,
        FSAAMode                fsaaMode,
        bool                    compress,
        bool                    blockLinear,
        GLuint                  aaMethod,
        GLuint                  downsampleHint,
        GpuDevice::GpuFamily    gpuFamily,
        bool                    isQuadro
    );

public:
    ~mglDesktop();

    // Creates desktop(s) and increments a refcount.
    static RC Setup
    (
        GpuDevice * pDev,
        UINT32      displayWidth,
        UINT32      displayHeight,
        UINT32      surfaceWidth,
        UINT32      surfaceHeight,
        UINT32      depth,
        FSAAMode    fsaaMode,
        bool        compress,
        bool        blockLinear,
        GLuint      aaMethod,
        GLuint      downsampleHint,
        bool *      pRtnMustDrawOffscreen
    );

    // Reserve frontbuffer & display, SetMode, SetImage.
    static RC Alloc
    (
        GpuDevice *              pDev,
        const mglTestContext *   pTestCtx,
        DisplayMgr::Requirements reqs,
        UINT32                   displayWidth,
        UINT32                   displayHeight,
        UINT32                   refreshRate,
        mglDesktop **            ppRtnDesktop,
        bool *                   pRtnOwnsFrontbuffer
    );

    // Frees resources from Setup and Alloc.
    static void Free
    (
        mglDesktop *           pDesktop,
        const mglTestContext * pTestCtx
    );

    Surface2D * GetSurface2D() const;
    Display *   GetDisplay() const;
    UINT32      GetPrimaryDisplay() const;
    DisplayMgr::TestContext *   GetTestContext() const { return m_pTestContext; }
    RC          UpdateDisplay(UINT32 refreshRate);

private:
    RC AllocSurface();
    RC AllocDisplay();

    GpuDevice *                 m_pGpuDev;
    UINT32                      m_DisplayWidth;
    UINT32                      m_DisplayHeight;
    UINT32                      m_Width;
    UINT32                      m_Height;
    UINT32                      m_Depth;
    FSAAMode                    m_FsaaMode;
    bool                        m_Compress;
    bool                        m_BlockLinear;
    GLuint                      m_AaMethod;
    GLuint                      m_DownsampleHint;

    DisplayMgr::TestContext *   m_pTestContext;
    GpuDevice::GpuFamily        m_GpuFamily;
    bool                        m_IsQuadro;

    //! The mglTestContext (if any) lwrrently drawing to the primary
    //! surface.  All other mglTestContexts that share this Desktop must
    //! have m_UsingFbo set (i.e. be drawing offscreen).
    const mglTestContext * m_pOwner;

    //! Number of mglTestContexts (if any) sharing this Desktop.
    //! If 0, the surface may be freed and reallocated.
    //! If !0, freeing and reallocating this Desktop will crash
    //! even the offscreen mglTestContexts...
    UINT32 m_RefCount;

    static map<GpuDevice *, mglDesktop*> s_Desktops;
};

#endif // INCLUDED_MGL_DTOP_H
