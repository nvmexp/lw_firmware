/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011,2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "directfbmgr.h"
#include "core/include/massert.h"
#include "core/include/platform.h"
#include <dlfcn.h>

DirectFBManager::DirectFBManager()
: m_Library(0)
, m_pDirectFB(0)
, m_pLayer(0)
, m_pSurface(0)
, m_PixelFormat(DSPF_UNKNOWN)
, m_ScreenWidth(0)
, m_ScreenHeight(0)
{
}

DirectFBManager::~DirectFBManager()
{
    if (m_pSurface)
    {
        m_pSurface->Release(m_pSurface);
        m_pSurface = 0;
    }
    if (m_pLayer)
    {
        m_pLayer->Release(m_pLayer);
        m_pLayer = 0;
    }
    if (m_pDirectFB)
    {
        m_pDirectFB->Release(m_pDirectFB);
        m_pDirectFB = 0;
    }
    if (m_Library)
    {
        dlclose(m_Library);
        m_Library = 0;
    }
}

RC DirectFBManager::Initialize(int argc, char* argv[])
{
    // Open the DirectFB library
    m_Library = dlopen("libdirectfb.so", RTLD_NOW | RTLD_LOCAL);
    if (!m_Library)
    {
        Printf(Tee::PriNormal, "DirectFB: Unable to load libdirectfb.so\n");
        return RC::SOFTWARE_ERROR;
    }

    // Get DirectFBInit function
    typedef DFBResult (*DirectFBInitFn)(int*, char* (*argv[]));
    DirectFBInitFn directFBInit =
        reinterpret_cast<DirectFBInitFn>(dlsym(m_Library, "DirectFBInit"));
    if (!directFBInit)
    {
        Printf(Tee::PriNormal, "DirectFB: Unable to get address of DirectFBInit\n");
        return RC::SOFTWARE_ERROR;
    }

    // Get DirectFBCreate function
    typedef DFBResult (*DirectFBCreateFn)(IDirectFB**);
    DirectFBCreateFn directFBCreate =
        reinterpret_cast<DirectFBCreateFn>(dlsym(m_Library, "DirectFBCreate"));
    if (!directFBCreate)
    {
        Printf(Tee::PriNormal, "DirectFB: Unable to get address of DirectFBCreate\n");
        return RC::SOFTWARE_ERROR;
    }

    // Initialize DirectFB
    if (DFB_OK != directFBInit(&argc, &argv))
    {
        Printf(Tee::PriNormal, "DirectFB: Unable to initialize DirectFB\n");
        return RC::SOFTWARE_ERROR;
    }

    // Create main DirectFB interface
    if (DFB_OK != directFBCreate(&m_pDirectFB))
    {
        Printf(Tee::PriNormal, "DirectFB: Unable to create IDirectFB object\n");
        return RC::SOFTWARE_ERROR;
    }

    // Get primary display layer
    if (DFB_OK != m_pDirectFB->GetDisplayLayer(m_pDirectFB, DLID_PRIMARY, &m_pLayer))
    {
        Printf(Tee::PriNormal, "DirectFB: Unable to obtain primary display layer\n");
        return RC::SOFTWARE_ERROR;
    }

    // Get layer's description
    DFBDisplayLayerDescription desc;
    if (DFB_OK != m_pLayer->GetDescription(m_pLayer, &desc))
    {
        Printf(Tee::PriNormal, "DirectFB: Unable to obtain description of the primary display layer\n");
        return RC::SOFTWARE_ERROR;
    }

    // Check if the layer supports drawing
    if (((desc.type & DLTF_GRAPHICS) == 0) ||
        ((desc.caps & DLCAPS_SURFACE) == 0))
    {
        Printf(Tee::PriNormal, "DirectFB: Unsupported primary display layer, missing drawing capabilities\n");
        return RC::SOFTWARE_ERROR;
    }

    // Disable cursor
    if ((DFB_OK != m_pLayer->SetCooperativeLevel(m_pLayer, DLSCL_EXCLUSIVE)) ||
        (DFB_OK != m_pLayer->EnableLwrsor(m_pLayer, 0)))
    {
        Printf(Tee::PriLow, "DirectFB: Unable to disable cursor\n");
    }

    // Set layer's cooperative level
    if (DFB_OK != m_pLayer->SetCooperativeLevel(m_pLayer, DLSCL_SHARED))
    {
        Printf(Tee::PriNormal, "DirectFB: Unable to get access to primary display layer\n");
        return RC::SOFTWARE_ERROR;
    }

    // Get layer's surface
    if (DFB_OK != m_pLayer->GetSurface(m_pLayer, &m_pSurface))
    {
        Printf(Tee::PriNormal, "DirectFB: Unable to obtain drawing surface\n");
        return RC::SOFTWARE_ERROR;
    }

    // Get surface pixel format
    if (DFB_OK != m_pSurface->GetPixelFormat(m_pSurface, &m_PixelFormat))
    {
        Printf(Tee::PriNormal, "DirectFB: Unable to obtain surface pixel format\n");
        return RC::SOFTWARE_ERROR;
    }

    // Get surface dimensions
    if (DFB_OK != m_pSurface->GetSize(m_pSurface, &m_ScreenWidth, &m_ScreenHeight))
    {
        Printf(Tee::PriNormal, "DirectFB: Unable to obtain size of drawing surface\n");
        return RC::SOFTWARE_ERROR;
    }

    return OK;
}

RC DirectFBManager::Write(void* data, UINT32 pitch, UINT32 widthInBytes, UINT32 height)
{
    MASSERT(m_pSurface);
    MASSERT(data);

    // Lock the surface for drawing
    void* ptr = 0;
    int surfacePitch = 0;
    if (DFB_OK != m_pSurface->Lock(m_pSurface, static_cast<DFBSurfaceLockFlags>(DSLF_READ | DSLF_WRITE), &ptr, &surfacePitch))
    {
        Printf(Tee::PriError, "DirectFB: Unable to lock the surface\n");
        return RC::SOFTWARE_ERROR;
    }

    // Copy data to the surface
    char* src = static_cast<char*>(data);
    char* dest = static_cast<char*>(ptr);
    const int drawPitch = min(surfacePitch,
            min(static_cast<int>(pitch), static_cast<int>(widthInBytes)));
    const int drawHeight = min(static_cast<int>(height), m_ScreenHeight);
    for (int y=0; y < drawHeight; y++)
    {
        Platform::VirtualRd(src, dest, drawPitch);
        src += pitch;
        dest += surfacePitch;
    }

    // Unlock the surface and ensure it's displayed on the screen
    m_pSurface->Unlock(m_pSurface);
    m_pSurface->Flip(m_pSurface, 0, DSFLIP_NONE);
    return OK;
}

RC DirectFBManager::Clear()
{
    MASSERT(m_pSurface);
    if (DFB_OK != m_pSurface->Clear(m_pSurface, 0, 0, 0, 0xFFU))
    {
        Printf(Tee::PriNormal, "DirectFB: Surface clear failed\n");
        return RC::SOFTWARE_ERROR;
    }
    return OK;
}

ColorUtils::Format DirectFBManager::GetPixelFormat() const
{
    switch (m_PixelFormat)
    {
        case DSPF_ARGB1555:     return ColorUtils::A1R5G5B5;
        case DSPF_RGB16:        return ColorUtils::R5G6B5;
        case DSPF_RGB24:        return ColorUtils::B8_G8_R8;
        case DSPF_RGB32:        return ColorUtils::X8R8G8B8;
        case DSPF_ARGB:         return ColorUtils::A8R8G8B8;
        case DSPF_A8:           return ColorUtils::A8;
        case DSPF_RGB555:       return ColorUtils::X1R5G5B5;
        default:                return ColorUtils::LWFMT_NONE;
    }
}
