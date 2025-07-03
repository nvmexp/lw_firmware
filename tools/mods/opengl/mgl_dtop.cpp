/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2021 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! \file mgl_dtop.cpp

#include "mgl_dtop.h"
#include "core/include/mgrmgr.h"
#include "core/include/display.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/gpudev.h"

map<GpuDevice *, mglDesktop*> mglDesktop::s_Desktops;

//------------------------------------------------------------------------------
mglDesktop::mglDesktop
(
    GpuDevice * pDev,
    UINT32      displayWidth,
    UINT32      displayHeight,
    UINT32      width,
    UINT32      height,
    UINT32      depth,
    FSAAMode    fsaaMode,
    bool        compress,
    bool        blockLinear,
    GLuint      aaMethod,
    GLuint      downsampleHint,
    GpuDevice::GpuFamily gpuFamily,
    bool        isQuadro
)
:   m_pGpuDev(pDev),
    m_DisplayWidth(displayWidth),
    m_DisplayHeight(displayHeight),
    m_Width(width),
    m_Height(height),
    m_Depth(depth),
    m_FsaaMode(fsaaMode),
    m_Compress(compress),
    m_BlockLinear(blockLinear),
    m_AaMethod(aaMethod),
    m_DownsampleHint(downsampleHint),
    m_pTestContext(0),
    m_GpuFamily(gpuFamily),
    m_IsQuadro(isQuadro),
    m_pOwner(0),
    m_RefCount(0)
{
}

//------------------------------------------------------------------------------
mglDesktop::~mglDesktop()
{
    MASSERT(m_RefCount == 0);
    delete m_pTestContext;
}

//-----------------------------------------------------------------------------
//! \brief Set up the mglDesktop for the given GpuDevice.
//!
//! Allocs appropriate Surface2D(s) and describes them to the GL driver.
//!
//! Pseudo-code:
//!
//! if (no mglDesktops exist)
//!   disable user interface
//!   alloc mglDesktop for this GpuDevice
//!   alloc mglDesktop for all other GpuDevices with same {Family,Lwdqro}
//! else
//!   if (mglDesktop for this GpuDevice not present)
//!     fail, because different {Family,Lwdqro} is already running
//!   if (present but doesn't match requested w,h,etc)
//!     if (unused)
//!       free and realloc this mglDesktop
//!     else: present, wrong, and in-use
//!       set *pRtnMustDrawOffscreen = true;
//!
RC mglDesktop::Setup
(
    GpuDevice * pThisDev,
    UINT32      displayWidth,
    UINT32      displayHeight,
    UINT32      width,
    UINT32      height,
    UINT32      depth,
    FSAAMode    fsaaMode,
    bool        compress,
    bool        blockLinear,
    GLuint      aaMethod,
    GLuint      downsampleHint,
    bool *      pRtnMustDrawOffscreen
)
{
    MASSERT(pThisDev);
    MASSERT(pRtnMustDrawOffscreen);

    GpuDevMgr *pGpuDevMgr  = (GpuDevMgr *)DevMgrMgr::d_GraphDevMgr;

    // Assume we can draw on the frontbuffer for now.
    *pRtnMustDrawOffscreen = false;

    RC rc;

    GpuDevice::GpuFamily gpuFamily = pThisDev->GetFamily();
    bool      isQuadro  = pThisDev->CheckCapsBit(GpuDevice::QUADRO_GENERIC);

    if (! s_Desktops.size())
    {
        // GL isn't running yet.

        // Set up a desktop on the requested device and on all other devices
        // with matching family/lwdqro.

        for (GpuDevice * pDev = pGpuDevMgr->GetFirstGpuDevice();
                pDev;
                    pDev = pGpuDevMgr->GetNextGpuDevice(pDev))
        {
            if ((gpuFamily == pDev->GetFamily()) &&
                (isQuadro  == pDev->CheckCapsBit(GpuDevice::QUADRO_GENERIC)))
            {
                s_Desktops[pDev] = new mglDesktop(
                        pDev,
                        displayWidth,
                        displayHeight,
                        width,
                        height,
                        depth,
                        fsaaMode,
                        compress,
                        blockLinear,
                        aaMethod,
                        downsampleHint,
                        gpuFamily,
                        isQuadro);
            }
        }

        // Build an array of resman device-instance numbers to tell GL
        // which devices it should use.

        GLuint numDevices = pGpuDevMgr->NumDevices();
        GLuint * pDeviceInst = new GLuint [numDevices];

        UINT32 devIndex = 0;
        for (GpuDevice *pGpuDev = pGpuDevMgr->GetFirstGpuDevice();
             pGpuDev != NULL; pGpuDev = pGpuDevMgr->GetNextGpuDevice(pGpuDev))
        {
            pDeviceInst[devIndex++] = pGpuDev->GetDeviceInst();
        }
        MASSERT(devIndex == numDevices);
        rc = mglErrorToRC( dglCreateDevices(numDevices, pDeviceInst) );
        delete [] pDeviceInst;
        CHECK_RC(rc);

        // Alloc the surfaces for all the devices.
        // As a side-effect, disables user interface.
        map<GpuDevice *, mglDesktop*>::iterator it;
        for (it = s_Desktops.begin(); it != s_Desktops.end(); ++it)
        {
            CHECK_RC(it->second->AllocSurface());
        }

        // Start up GLS.
        CHECK_RC( mglErrorToRC( dglStartServer() ) );
    }
    else
    {
        if (s_Desktops.find(pThisDev) == s_Desktops.end())
        {
            // Other desktops exist, they must be of differing GpuFamily or
            // Lwdqro-ness.  Multithreading test failure.
            Printf(Tee::PriHigh,
                    "Can't start GL on device %d, GpuFamily or Lwdqro mismatch.\n",
                    pThisDev->GetDeviceInst());
            return RC::PRIMARY_SURFACE_IN_USE;
        }

        mglDesktop * pDesktop = s_Desktops[pThisDev];

        if ((pDesktop->m_Width != width) ||
            (pDesktop->m_Height != height) ||
            (pDesktop->m_Depth != depth) ||
            (pDesktop->m_FsaaMode != fsaaMode) ||
            (pDesktop->m_Compress != compress) ||
            (pDesktop->m_BlockLinear != blockLinear) ||
            (pDesktop->m_AaMethod != aaMethod) ||
            (pDesktop->m_DownsampleHint != downsampleHint))
        {
            if (0 == pDesktop->m_RefCount)
            {
                // Desktop exists, but is wrong, and is unused.
                // Free it, and reallocate it to match requirements.
                delete pDesktop;
                s_Desktops[pThisDev] = new mglDesktop(
                        pThisDev,
                        displayWidth,
                        displayHeight,
                        width,
                        height,
                        depth,
                        fsaaMode,
                        compress,
                        blockLinear,
                        aaMethod,
                        downsampleHint,
                        gpuFamily,
                        isQuadro);

                pDesktop = s_Desktops[pThisDev];

                CHECK_RC(pDesktop->AllocSurface());
            }
            else
            {
                // Desktop exists, but is wrong, and is in use.
                *pRtnMustDrawOffscreen = true;
            }
        }
        else if (pDesktop->m_pOwner)
        {
            // Exists, is right, but a test is using the frontbuffer.
            *pRtnMustDrawOffscreen = true;
        }
    }

    mglDesktop * pDesktop = s_Desktops[pThisDev];

    pDesktop->m_RefCount++;

    return rc;
}

//-----------------------------------------------------------------------------
Surface2D * mglDesktop::GetSurface2D() const
{
    if (m_pTestContext)
        return m_pTestContext->GetSurface2D();
    return 0;
}

//-----------------------------------------------------------------------------
Display * mglDesktop::GetDisplay() const
{
    if (m_pTestContext)
        return m_pTestContext->GetDisplay();

    return 0;
}

//-----------------------------------------------------------------------------
UINT32 mglDesktop::GetPrimaryDisplay() const
{
    if (m_pTestContext)
        return m_pTestContext->GetPrimaryDisplay();

    return 1;
}

//-----------------------------------------------------------------------------
RC mglDesktop::UpdateDisplay(UINT32 refreshRate)
{
    return m_pTestContext->SetModeAndImage(m_DisplayWidth, m_DisplayHeight, refreshRate);
}

//-----------------------------------------------------------------------------
RC mglDesktop::AllocSurface()
{
    // We should not double-alloc.
    MASSERT(0 == m_pTestContext);

    RC rc;

    CHECK_RC( m_pGpuDev->GetDisplayMgr()->SetupTest(
            DisplayMgr::RequireNullDisplay,
            m_DisplayWidth,
            m_DisplayHeight,
            m_Width,
            m_Height,
            ColorUtils::ColorDepthToFormat(m_Depth),
            m_FsaaMode,
            60, // refresh rate (unused here)
            m_Compress,
            m_BlockLinear,
            NULL,
            NULL,
            NULL,
            &m_pTestContext));

    Surface2D * pSurf = m_pTestContext->GetSurface2D();

    UINT32 pitch = pSurf->GetPitch();
    if (0 == pitch)
    {
        // BlockLinear surfaces report 0 pitch, but GL asserts on 0.
        pitch = pSurf->GetWidth() * pSurf->GetBytesPerPixel();
    }

    GLuint filterTaps = m_pTestContext->GetDacFilterTaps();

    if (m_pGpuDev->GetDisplay()->GetDisplayClassFamily() != Display::EVO)
    {
        // Set an invalid Filter-On-Scanout FSAA filter-taps value to
        // tell GL not to do any display operations.
        filterTaps = 0;
    }

    // For pre-Tesla GPUs we used to pass GetVidMemOffset(),
    // but we don't support pre-Tesla GPUs anymore.
    UINT64 offset = 0;

    CHECK_RC(mglErrorToRC( dglCreateDesktop(
            GLint(m_pGpuDev->GetDeviceInst()),
            pSurf->GetWidth()  / m_pTestContext->GetXBloat(),
            pSurf->GetHeight() / m_pTestContext->GetYBloat(),
            pSurf->GetBytesPerPixel(),
            mglColor2TexFmt(pSurf->GetColorFormat()),
            pitch,
            filterTaps,
            pSurf->GetLayout() == Surface2D::BlockLinear,
            pSurf->GetLogBlockWidth(),
            pSurf->GetLogBlockHeight(),
            0,  // log2 gobs per block in Z, always 0 for Surface2D at present
            m_pTestContext->GetDacColorCompress(),
            pSurf->GetSize(),
            pSurf->GetAddress(),
            static_cast<GLuint>(offset),
            LwRmPtr()->GetClientHandle(),
            pSurf->GetMemHandle(),
            m_AaMethod,
            m_DownsampleHint)));

    return rc;
}

//-----------------------------------------------------------------------------
//! Alloc the existing desktop for use by this mglTestContext.
//!
//! Through DisplayMgr, alloc a Display object (real or Null) and possibly
//! do a modeset and SetImage call.
//!
//! Only one mglTestContext can have the front-buffer at once, the one that
//! has the real Display object.  Others can draw offscreen to an FBO object
//! so long as they don't RequireRealDisplay.
//!
/* static */
RC mglDesktop::Alloc
(
    GpuDevice *              pDev,
    const mglTestContext *   pTestCtx,
    DisplayMgr::Requirements reqs,
    UINT32                   displayWidth,
    UINT32                   displayHeight,
    UINT32                   refreshRate,
    mglDesktop **            ppRtnDesktop,
    bool *                   pRtnHasFrontbuffer
)
{
    MASSERT(pDev);
    MASSERT(pTestCtx);
    MASSERT(ppRtnDesktop);
    MASSERT(pRtnHasFrontbuffer);

    RC rc = OK;
    mglDesktop * pDesktop = s_Desktops[pDev];
    MASSERT(pDesktop);

    *ppRtnDesktop = pDesktop;
    *pRtnHasFrontbuffer = false;

    if ((reqs == DisplayMgr::RequireNullDisplay) ||
        (pDesktop->m_pOwner))

    {
        // Nothing to do here, the test is going to run offscreen.
        return rc;
    }

    // Reserve the display for this test.
    CHECK_RC(pDesktop->m_pTestContext->AllocDisplay(reqs));

    // Reserve the primary surface for this test.
    pDesktop->m_pOwner = pTestCtx;
    *pRtnHasFrontbuffer = true;

    // Set mode and image.
    CHECK_RC(pDesktop->m_pTestContext->SetModeAndImage(
                displayWidth, displayHeight, refreshRate));

    if (reqs != DisplayMgr::RequireRealDisplay)
    {
        // Allow stealing now that the modeset/setimage is complete.
        pDesktop->m_pTestContext->UnlockDisplay(NULL, NULL, NULL);
    }

    return rc;
}

//-----------------------------------------------------------------------------
//! Mark the mglDesktop as no longer used by this mglTestContext.
//!
//! If all mglDesktops are now unused:
//!  - free all mglDesktops
//!  - shut down the GL driver
//!  - enable user interface
//!
/* static */
void mglDesktop::Free
(
    mglDesktop *           pDesktop,
    const mglTestContext * pTestCtx
)
{
    MASSERT(pDesktop);
    MASSERT(pTestCtx);

    if (pDesktop->m_pOwner == pTestCtx)
    {
        // Release the primary surface for other tests to use.
        pDesktop->m_pOwner = 0;

        // Release the display for other tests to use.
        pDesktop->m_pTestContext->UnlockDisplay(NULL, NULL, NULL);
    }

    if (pDesktop->m_RefCount)
    {
        pDesktop->m_RefCount--;
    }

    UINT32 totalRefs = 0;
    map<GpuDevice *, mglDesktop*>::iterator it;

    for (it = s_Desktops.begin(); it != s_Desktops.end(); ++it)
    {
        totalRefs += it->second->m_RefCount;
    }
    if (0 == totalRefs)
    {
        // Tell the GL driver to release all persistent surfaces.
        dglStopServer();

        // Free all desktops.
        for (it = s_Desktops.begin(); it != s_Desktops.end(); ++it)
        {
            delete it->second;
        }
        s_Desktops.clear();
    }

}
