/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Image file read/write

#ifndef INCLUDED_IMAGEFIL_H
#define INCLUDED_IMAGEFIL_H

#ifndef INCLUDED_RC_H
#include "rc.h"
#endif
#ifndef INCLUDED_COLOR_H
#include "color.h"
#endif

namespace ImageFile
{
    //---------------------------------------------------------------------------
    // Read image file.
    // Tries to guess what format the file is based on file extension.
    // If that doesn't work, resorts to trial-and-error.
    // Fails if nothing works.
    //---------------------------------------------------------------------------
    RC Read
    (
        const char *        FileName,
        bool                DoTile,
        ColorUtils::Format  MemColorFormat,
        void *              MemAddress,
        UINT32              MemWidth,
        UINT32              MemHeight,
        UINT32              MemPitch
    );

    //---------------------------------------------------------------------------
    // Read/Write TGA image files.
    //
    // We only deal with one version of TGA file: 24 bit true-color,
    // run-length encoded.
    // We colwert to/from a few different screen color formats.
    // Added ForceRaw which saves the tga file in not Run Length Encoded format
    //---------------------------------------------------------------------------
    RC ReadTga
    (
        const char *        FileName,
        bool                DoTile,
        ColorUtils::Format  MemColorFormat,         // 16 or 32 bit RGB only
        void *              MemAddress,
        UINT32              MemWidth,
        UINT32              MemHeight,
        UINT32              MemPitch
    );
    RC WriteTga
    (
        const char *        FileName,
        ColorUtils::Format  MemColorFormat,         // 16 or 32 bit RGB only
        void *              MemAddress,
        UINT32              MemWidth,
        UINT32              MemHeight,
        UINT32              MemPitch,
        bool                AlphaToRgb = false,     // copy alpha to r,g,b
        bool                YIlwerted = false,      // true if Y0 is bottom
        bool                ColwertToA8R8G8B8=true,  // colwert to A8R8G8B8
        bool                ForceRaw=false
    );

    //---------------------------------------------------------------------------
    // Read/Write PNG image files.
    //
    // All png file formats and all screen color formats are supported.
    //---------------------------------------------------------------------------
    RC ReadPng
    (
        const char *        FileName,
        bool                DoTile,
        ColorUtils::Format  MemColorFormat,
        void *              MemAddress,
        UINT32              MemWidth,
        UINT32              MemHeight,
        UINT32              MemPitch
    );
    RC WritePng
    (
        const char *        FileName,
        ColorUtils::Format  MemColorFormat,
        void *              MemAddress,
        UINT32              MemWidth,
        UINT32              MemHeight,
        UINT32              MemPitch,
        bool                AlphaToRgb = false,     // copy alpha to r,g,b
        bool                YIlwerted = false
    );

    //---------------------------------------------------------------------------
    // Read/Write YUV image files.
    //
    // If DoSwapBytes is true, each pair of bytes is swapped on the way.
    // This colwerts between CR8YB8CB8YA8 and YB8CR8YA8CB8,
    // or between ECR8EYB8ECB8EYA8 and EYB8ECR8EYA8ECB8.
    //
    // Other than this, the functions do no data colwersion.
    //
    // Note that these YUV files are an lWpu invention, and are not
    // standardized even within lWpu.  The files themselves do not
    // contain any header data other than an ID string, width, and height.
    //
    // Extension      Format
    // .LC0           ITU-R BT.601 format YbCrYaCb  (bytes 3..0)
    // .LC1           same, but CrYbCbYa
    // .LC2           ITU-R BT.709 format YbCrYaCb
    // .LC3           same, but CrYbCbYa
    // .YUV           same as .LC0, probably
    // .BIN           binary, otherwise same as .LC0, probably
    //---------------------------------------------------------------------------

    RC ReadYuv     // Tries binary format, if that fails tries ascii.
    (
        const char *   FileName,
        bool           DoSwapBytes,
        bool           DoTile,
        void *         MemAddress,
        UINT32         MemWidth,
        UINT32         MemHeight,
        UINT32         MemPitch,
        UINT32 *       ImageWidth,
        UINT32 *       ImageHeight
    );
    RC WriteYuv
    (
        const char *   FileName,
        bool           DoSwapBytes,
        bool           DoAscii,
        void *         MemAddress,
        UINT32         MemWidth,
        UINT32         MemHeight,
        UINT32         MemPitch
    );

    RC ReadHdr
    (
       const char *   FileName,
       vector<UINT08> *buf,
       UINT32         width,
       UINT32         height,
       UINT32         pitch
    );
} // end of namespace ImageFile

#endif  // INCLUDED_IMAGEFIL_H
