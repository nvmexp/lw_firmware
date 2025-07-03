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

//! @file    gpuwm.cpp
//! @brief   Implement GpuWinMgr -- MODS Window Manager, GPU specific version.

#include "gpu/include/gpuwm.h"
#include "core/include/display.h"
#include "surf2d.h"
#include "gpu/include/gpudev.h"
#include "core/include/lwrm.h"
#include "core/include/platform.h"
#include "lwos.h"
#include "gpu/tests/gputestc.h"
#include "core/include/utility.h"

//----------------------------------------------------------------------------
GpuWinMgr::GpuWinMgr
(
    Surface2D * pSurf,
    UINT32 xbloat,
    UINT32 ybloat
) :
    m_pSurface2D(pSurf),
    m_XBloat(xbloat),
    m_YBloat(ybloat)
{
    MASSERT(m_pSurface2D);
}

//----------------------------------------------------------------------------
/* virtual */
GpuWinMgr::~GpuWinMgr()
{
}

//----------------------------------------------------------------------------
bool GpuWinMgr::AnyWindowsAllocated() const
{
    return ! m_AllocedWindows.empty();
}

//----------------------------------------------------------------------------
UINT32 GpuWinMgr::GetXBloat() const
{
    return m_XBloat;
}

//----------------------------------------------------------------------------
UINT32 GpuWinMgr::GetYBloat() const
{
    return m_YBloat;
}

//----------------------------------------------------------------------------
RC GpuWinMgr::AllocWindow
(
    UINT32 width,
    UINT32 height,
    GpuWinMgr::AllocReqs reqs,
    GpuWinMgr::Rect * pWindow
)
{
    MASSERT(pWindow);

    if (width == 0 || height == 0)
    {
        // Test requested a 0-size window.  Can do!
        // Don't store degenerate windows in the allocated window list.
        (*pWindow).left = 0;
        (*pWindow).top = 0;
        (*pWindow).width = width;
        (*pWindow).height = height;
        return OK;
    }

    if ((reqs == REQUIRE_FULL_SCREEN) &&
        ((!m_AllocedWindows.empty()) ||
         m_pSurface2D->GetWidth()/m_XBloat != width ||
         m_pSurface2D->GetHeight()/m_YBloat != height))
    {
        // Test is requesting full-screen mode, but either
        //  - Primary window is the wrong size OR
        //  - Another test has a window allocated.
        return RC::CANNOT_ALLOCATE_MEMORY;
    }

    //
    // Find best-fit free "window".
    //

    list<Rect>::iterator iBest = m_FreeWindows.begin();
    list<Rect>::iterator i = iBest;

    if (i != m_FreeWindows.end())
        ++i;

    for (/**/; i != m_FreeWindows.end(); ++i)
    {
        INT32 wMarginBest = (*iBest).width - width;
        INT32 hMarginBest = (*iBest).height - height;
        INT32 wMargin = (*i).width - width;
        INT32 hMargin = (*i).height - height;

        if (wMarginBest >= 0 && hMarginBest >= 0)
        {
            // Current best-fit fits (is GE requested window size).

            if ((wMargin >= 0 && hMargin >= 0) &&
                (wMargin * hMargin < wMarginBest * hMarginBest))
            {
                // New free window is also big enough, but smaller than previous
                // best-fit window.
                iBest = i;
            }
        }
        else if (wMargin >= 0 && hMargin >= 0)
        {
            // Current best-fit window is too small, but new free window fits.

            iBest = i;
        }
        else
        {
            // Both windows are too small.
            // Choose this win as new "best fit" if it lwts off less of window.

            INT32 bestLwtoff = MIN(wMarginBest, 0) + MIN(hMarginBest, 0);
            INT32 cutoff = MIN(wMargin, 0) + MIN(hMargin, 0);

            if (cutoff > bestLwtoff)
                iBest = i;
        }
    }

    //
    // Did we get an adequate window?
    //

    if (iBest == m_FreeWindows.end())
    {
        // Not even one pixel is free.

        if (reqs == REQUIRE_NOTHING)
        {
            // Don't store 0-size windows in the allocated window list.
            (*pWindow).left = 0;
            (*pWindow).top = 0;
            (*pWindow).width = 0;
            (*pWindow).height = 0;
            return OK;
        }
        else
        {
            return RC::PRIMARY_SURFACE_IN_USE;
        }
    }
    if ((*iBest).width < width || (*iBest).height < height)
    {
        // Biggest free area can't fit requested window.

        switch (reqs)
        {
            case REQUIRE_FULL_SCREEN:
            case REQUIRE_FIXED_SIZE:
                return RC::PRIMARY_SURFACE_IN_USE;

            case REQUIRE_MIN_SIZE:
                if (((*iBest).width < width && (*iBest).width < 64) ||
                    ((*iBest).height < height && (*iBest).height < 64))
                {
                    return RC::PRIMARY_SURFACE_IN_USE;
                }
                break;

            case REQUIRE_NOTHING:
                break;
        }
    }

    //
    // We have an adequate window.
    //

    (*pWindow).left = (*iBest).left;
    (*pWindow).top = (*iBest).top;
    (*pWindow).width = MIN((*iBest).width, width);
    (*pWindow).height = MIN((*iBest).height, height);

    m_AllocedWindows.push_back(*pWindow);

    //
    // Update our bookkeeping for free "windows".
    //

    if ((width < (*iBest).width) && (height < (*iBest).height))
    {
        // New window is neither the full width nor height, leaving two free
        // "windows" to right and below the new window.
        // Try to leave the largest free blocks possible.

        Rect freeRight;
        Rect freeBottom;

        freeRight.left = (*iBest).left + (*pWindow).width;
        freeRight.top  = (*iBest).top;
        freeRight.width = (*iBest).width - (*pWindow).width;

        freeBottom.left = (*iBest).left;
        freeBottom.top  = (*iBest).top - (*pWindow).height;
        freeBottom.height = (*iBest).height - (*pWindow).height;

        if (((*iBest).width - width) > ((*iBest).height - height))
        {
            // Largest free space is at the right.
            freeRight.height = (*iBest).height;
            freeBottom.width = (*pWindow).width;
        }
        else
        {
            // Largest free space is at the bottom.
            freeRight.height = (*pWindow).height;
            freeBottom.width = (*iBest).width;
        }
        m_FreeWindows.push_back(freeRight);
        m_FreeWindows.push_back(freeRight);
    }
    else if (width < (*iBest).width)
    {
        // New window leaves free space to the right.
        Rect freeRight;

        freeRight.left = (*iBest).left + (*pWindow).width;
        freeRight.top  = (*iBest).top;
        freeRight.width = (*iBest).width - (*pWindow).width;
        freeRight.height = (*iBest).height;

        m_FreeWindows.push_back(freeRight);
    }
    else if (height < (*iBest).height)
    {
        // New window leaves free space at the bottom.
        Rect freeBottom;

        freeBottom.left = (*iBest).left;
        freeBottom.top  = (*iBest).top - (*pWindow).height;
        freeBottom.width = (*iBest).width;
        freeBottom.height = (*iBest).height - (*pWindow).height;

        m_FreeWindows.push_back(freeBottom);
    }

    m_FreeWindows.erase(iBest);

    return OK;
}

//----------------------------------------------------------------------------
void GpuWinMgr::FreeWindow
(
    const GpuWinMgr::Rect * pWindow
)
{
    MASSERT(NULL != pWindow);
    if ((*pWindow).width == 0 || (*pWindow).height == 0)
    {
        // We don't store degenerate windows in the allocated list.
        return;
    }

    //
    // Find the window in the allocated list and free it.
    //

    list<Rect>::iterator i;
    for (i = m_AllocedWindows.begin(); i != m_AllocedWindows.end(); ++i)
    {
        if ((*i).left == (*pWindow).left && (*i).top == (*pWindow).top)
        {
            break;
        }
    }

    if (i == m_AllocedWindows.end())
    {
        MASSERT(!"Attempt to free unallocated window");
        return;
    }
    MASSERT((*i).width == (*pWindow).width);
    MASSERT((*i).height == (*pWindow).height);

    m_AllocedWindows.erase(i);

    m_FreeWindows.push_back(*pWindow);

    //
    // Walk the free list until we have merged all adjacent free windows that
    // can be merged.
    // This is not an efficient algorithm but is fairly clear and we expect
    // only a small number of windows.
    //

    bool foundMergeThisPass;
    do
    {
        foundMergeThisPass = false;

        for (list<Rect>::iterator iA = m_FreeWindows.begin();
                iA != m_FreeWindows.end(); ++iA)
        {
            for (list<Rect>::iterator iB = m_FreeWindows.begin();
                    iB != m_FreeWindows.end(); ++iB)
            {
                if (iA == iB)
                    continue;

                if ((*iA).width == (*iB).width &&
                    (*iA).left  == (*iB).left &&
                    ((*iA).top == (*iB).top + (*iB).height ||
                     (*iA).top + (*iA).height == (*iB).top))
                {
                    // Merge up/down.
                    (*iA).height += (*iB).height;
                    (*iA).top = MIN((*iA).top, (*iB).top);
                    m_FreeWindows.erase(iB);
                    foundMergeThisPass = true;
                    break;
                }

                if ((*iA).height == (*iB).height &&
                    (*iA).top  == (*iB).top &&
                    ((*iA).left == (*iB).left + (*iB).width ||
                     (*iA).left + (*iA).width == (*iB).left))
                {
                    // Merge left/right.
                    (*iA).width += (*iB).width;
                    (*iA).left = MIN((*iA).left, (*iB).left);
                    m_FreeWindows.erase(iB);
                    foundMergeThisPass = true;
                    break;
                }
            }
        }
    }
    while (foundMergeThisPass);
}

