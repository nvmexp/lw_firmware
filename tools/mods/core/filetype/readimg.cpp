/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2004-2007 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

// Read image file.
// Tries to guess what format the file is based on file extension.
// If that doesn't work, resorts to trial-and-error.
// Fails if nothing works.

#include "core/include/imagefil.h"
#include "core/include/massert.h"
#include "core/include/utility.h"
#include <string.h>

#define FORMAT_TGA  0x1
#define FORMAT_PNG  0x2
#define FORMAT_ANY  (FORMAT_TGA | FORMAT_PNG)

RC ImageFile::Read
(
    const char *        FileName,
    bool                DoTile,
    ColorUtils::Format  MemColorFormat,
    void *              MemAddress,
    UINT32              MemWidth,
    UINT32              MemHeight,
    UINT32              MemPitch
)
{
    MASSERT(FileName);

    // Check the file extension to decide which format(s) to try.

    RC rc = OK;
    UINT32 FormatMask = 0;
    const char * extension = FileName + strlen(FileName) - 4;

    if (0 == Utility::strcasecmp(extension, ".tga"))
        FormatMask = FORMAT_TGA;
    else if (0 == Utility::strcasecmp(extension, ".png"))
        FormatMask = FORMAT_PNG;
    else
        FormatMask = FORMAT_ANY;

    // Try each possible format.

    if (FormatMask & FORMAT_TGA)
    {
        rc = ImageFile::ReadTga(FileName, DoTile, MemColorFormat, MemAddress,
                MemWidth, MemHeight, MemPitch);
        if (OK == rc)
            return OK;

        FormatMask &= ~FORMAT_TGA;
    }

    if (FormatMask & FORMAT_PNG)
    {
        rc = ImageFile::ReadPng(FileName, DoTile, MemColorFormat, MemAddress,
                MemWidth, MemHeight, MemPitch);
        if (OK == rc)
            return OK;

        FormatMask &= ~FORMAT_PNG;
    }

    MASSERT(0 == FormatMask);

    return rc;
}

