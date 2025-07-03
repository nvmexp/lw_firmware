/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011-2011 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_DIRECTFBMGR_H
#define INCLUDED_DIRECTFBMGR_H

#include "core/include/rc.h"
#include "core/include/color.h"
#include <directfb.h>

class DirectFBManager
{
public:
    DirectFBManager();
    ~DirectFBManager();
    RC Initialize(int argc, char* argv[]);
    RC Write(void* data, UINT32 pitch, UINT32 widthInBytes, UINT32 height);
    RC Clear();
    ColorUtils::Format GetPixelFormat() const;
    unsigned GetScreenWidth() const { return static_cast<unsigned>(m_ScreenWidth); }
    unsigned GetScreenHeight() const { return static_cast<unsigned>(m_ScreenHeight); }

private:
    DirectFBManager(const DirectFBManager&);
    DirectFBManager& operator=(const DirectFBManager&);

    void* m_Library;
    IDirectFB* m_pDirectFB;
    IDirectFBDisplayLayer* m_pLayer;
    IDirectFBSurface* m_pSurface;
    DFBSurfacePixelFormat m_PixelFormat;
    int m_ScreenWidth;
    int m_ScreenHeight;
};

#endif // INCLUDED_DIRECTFBMGR_H
