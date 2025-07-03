/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2007 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! @file    gpuwm.h
//! @brief   Declare GpuWinMgr -- MODS Window Manager, GPU specific version.

#ifndef INCLUDED_GPUWM_H
#define INCLUDED_GPUWM_H

#ifndef INCLUDED_TYPES_H
#include "core/include/types.h"
#endif
#ifndef INCLUDED_RC_H
#include "core/include/rc.h"
#endif
#ifndef INCLUDED_STL_LIST
#include <list>
#define INCLUDED_STL_LIST
#endif

class Surface2D;

//!
//! @class(GpuWinMgr)  Surface2D 2d-suballocator.
//!
//! This class manages simple window allocations withing a primary surface.
//!
class GpuWinMgr
{
public:
    GpuWinMgr(Surface2D * pSurf2D, UINT32 xbloat, UINT32 ybloat);
    ~GpuWinMgr();

    //! \brief One nonoverlapping rectangular region of the primary surface.
    //!
    //! Note: all units are "pixels" as opposed to "samples".
    //!
    //! In a Filter-On-Scanout Full-Scene-Anti-Aliasing surface, the physical
    //! surface is 2x or 4x larger than the number of pixels to accommodate
    //! the 2 or 4 samples per pixel.
    //!
    //! In non-FOS FSAA modes, we assume the test will be rendering to an
    //! oversized back-buffer somewhere and then doing a downsample-filter-blit
    //! to our normal-sized primary surface to display it.
    //!
    struct Rect
    {
        UINT32 left;
        UINT32 top;
        UINT32 width;
        UINT32 height;

        Rect(UINT32 l, UINT32 t, UINT32 w, UINT32 h)
        : left(l), top(t), width(w), height(h)
        { }
        Rect()
        : left(0), top(0), width(0), height(0)
        { }
        Rect(const Rect & rhs)
        : left(rhs.left), top(rhs.top), width(rhs.width), height(rhs.height)
        { }
    };

    enum AllocReqs
    {
        //! Fail if window is not full-surface.
        //! Tests that use golden-value DAC CRCs need to set this,
        //! as do tests that aren't smart enough to deal with not starting at
        //! offset 0,0 of the primary surface.
        REQUIRE_FULL_SCREEN,

        //! Fail if actual window size != requested.
        //! Tests that use golden-value surface-CRCs or checksums must set this.
        //! They must be smart enough to cope with not starting at offset 0,0,
        //! and with the surface having pitch > window-width*bpp.
        REQUIRE_FIXED_SIZE,

        //! Fail if actual window size < 64x64.
        //! Tests that need *some* surface area, but of no particular size
        //! and that don't use golden values can set this (i.e. dma tests).
        REQUIRE_MIN_SIZE,

        //! Allow a 0-size window.
        //! Tests that can render offscreen and only use the primary surface
        //! for displaying pretty stuff to the end-user (i.e. double-buffered
        //! GL tests, configuration tests, etc) can set this.
        REQUIRE_NOTHING
    };

    //! Alloc part of the primary surface.
    //!
    //! Fails if:
    //!  - The primary surface does not contain a "free" Rect of the requested
    //!    width and height, and reqs == REQUIRE_FIXED_SIZE.
    //!  - The primary surface does not contain any "free" Rect and
    //!    reqs == REQUIRE_MIN_SIZE.
    //!
    //! See the Note: on struct Rect above -- all units are in "pixels" not
    //! "samples", you must multiply by GetXBloat and GetYBloat to colwert to
    //! offsets in the Surface2D.
    RC AllocWindow
    (
        UINT32 width,
        UINT32 height,
        AllocReqs reqs,
        Rect * pWindow
    );

    //! Free an allocated window.
    //! Note that windows cannot move in ModsWinMgr, so we don't need to
    //! keep a "handle" per window, just its location.
    void FreeWindow
    (
        const Rect * pWindow
    );

    //! Return true if windows are lwrrently allocated in the primary surface.
    //! If true, this implies that the surface may not be freed by a test that
    //! would like to reallocate it with a different size, color format, etc.
    bool AnyWindowsAllocated() const;

    //! Get X-bloat factor:
    //! This is 1 normally, but is 2 when using a 2x or 4x filter-on-scanout
    //! FSAA mode.  Surface and window width in pixels is multipled by X-bloat
    //! to get the width in samples.
    UINT32 GetXBloat() const;

    //! Get Y-bloat factor:
    //! This is 1 normally, but is 2 when using a 4x filter-on-scanout
    //! FSAA mode.  Surface and window height in pixels is multipled by Y-bloat
    //! to get the height in samples.
    UINT32 GetYBloat() const;

private:
    Surface2D * m_pSurface2D;       //!< The current primary surface, if any.
    UINT32      m_XBloat;           //!< Muliplier for X for FOS FSAA surfaces.
    UINT32      m_YBloat;           //!< Muliplier for Y for FOS FSAA surfaces.
    list<Rect>  m_AllocedWindows;
    list<Rect>  m_FreeWindows;
};

#endif // INCLUDED_GPUWM_H

