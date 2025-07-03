/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _SURFACE_TYPE_H
#define _SURFACE_TYPE_H

#ifdef MAKEFOURCC
#undef MAKEFOURCC
#endif

////////////////////////////////////////////////////////////////////////////////////
// WARNING : Mdiag developer, beware of making changes in this file!
//
// This file is shared with the HIC capture tool (HICCapture.exe/HICShell.exe)
// and changes in it might break the build of that tool and/or ilwalidate existing
// HIC traces!!!
//
// No change of the existing formats should be made without consulting HIC
// developers.
////////////////////////////////////////////////////////////////////////////////////

#define MAKEFOURCC(ch0, ch1, ch2, ch3)                              \
            ((unsigned long int)(unsigned char)(ch0) | ((unsigned long int)(unsigned char)(ch1) << 8) |       \
            ((unsigned long int)(unsigned char)(ch2) << 16) | ((unsigned long int)(unsigned char)(ch3) << 24 ))

enum SurfaceType
{
    SURFACE_TYPE_UNKNOWN = MAKEFOURCC('U','N','K','N'),
    SURFACE_TYPE_SEMAPHORE = MAKEFOURCC('S','M','P','H'),
    SURFACE_TYPE_NOTIFIER = MAKEFOURCC('N','T','F','Y'),

    // Types used by 3D classes
    SURFACE_TYPE_COLOR = MAKEFOURCC('C','L','O','R'),
    SURFACE_TYPE_COLORA = MAKEFOURCC('C','L','O','R'),
    SURFACE_TYPE_COLORB = MAKEFOURCC('C','L','R','B'),
    SURFACE_TYPE_COLORC = MAKEFOURCC('C','L','R','C'),
    SURFACE_TYPE_COLORD = MAKEFOURCC('C','L','R','D'),
    SURFACE_TYPE_COLORE = MAKEFOURCC('C','L','R','E'),
    SURFACE_TYPE_COLORF = MAKEFOURCC('C','L','R','F'),
    SURFACE_TYPE_COLORG = MAKEFOURCC('C','L','R','G'),
    SURFACE_TYPE_COLORH = MAKEFOURCC('C','L','R','H'),
    SURFACE_TYPE_Z = MAKEFOURCC('Z','E','T','A'),
    SURFACE_TYPE_VERTEX_BUFFER = MAKEFOURCC('V','R','T','X'),
    SURFACE_TYPE_TEXTURE = MAKEFOURCC('T','X','T','R'),
    SURFACE_TYPE_CLIPID = MAKEFOURCC('C','L','I','P'),
    SURFACE_TYPE_ZLWLL = MAKEFOURCC('Z','C','U','L'),
    SURFACE_TYPE_INDEX_BUFFER = MAKEFOURCC('I','N','D','X'),

    SURFACE_TYPE_PUSHBUFFER = MAKEFOURCC('P','B','U','F'),

    SURFACE_TYPE_STENCIL = MAKEFOURCC('S','T','E','N'),
};

inline bool IsSurfaceTypeColor3D(SurfaceType t)
{
    return (t==SURFACE_TYPE_COLOR  ||
            t==SURFACE_TYPE_COLORB ||
            t==SURFACE_TYPE_COLORC ||
            t==SURFACE_TYPE_COLORD ||
            t==SURFACE_TYPE_COLORE ||
            t==SURFACE_TYPE_COLORF ||
            t==SURFACE_TYPE_COLORG ||
            t==SURFACE_TYPE_COLORH);
}

inline bool IsSurfaceTypeColorZ3D(SurfaceType t)
{
    return (IsSurfaceTypeColor3D(t) ||
            t==SURFACE_TYPE_Z);
}

//--------------------------------------------------------------------------
// Get the color index based on the surface type.
// All non-color surface types get -1.
//
inline int GetSurfaceTypeIndex(SurfaceType surfaceType)
{
    if (IsSurfaceTypeColor3D(surfaceType))
    {
        switch (surfaceType)
        {
            case SURFACE_TYPE_COLORA:
                return 0;

            case SURFACE_TYPE_COLORB:
                return 1;

            case SURFACE_TYPE_COLORC:
                return 2;

            case SURFACE_TYPE_COLORD:
                return 3;

            case SURFACE_TYPE_COLORE:
                return 4;

            case SURFACE_TYPE_COLORF:
                return 5;

            case SURFACE_TYPE_COLORG:
                return 6;

            case SURFACE_TYPE_COLORH:
                return 7;

            default:
                return -1;
        }
    }

    return -1;
}

#undef MAKEFOURCC

#endif // _SURFACE_TYPE_H
