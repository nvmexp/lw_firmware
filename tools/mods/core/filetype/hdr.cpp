/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

// Read pixels from .hdr image files.
// Colwert pixels to FP16 format
//

#include "core/include/imagefil.h"
#include "core/include/memoryif.h"
#include "core/include/utility.h"
#include "core/include/fileholder.h"
#include "core/include/rgbe.h"

#include <stdio.h>
#include <string.h>
#include <vector>

namespace ImageFile
{
   // Here's some declarations private to this file.
}

using namespace ImageFile;

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
RC ImageFile::ReadHdr
(
    const char *   FileName,
    vector<UINT08> *buf,
    UINT32         width,
    UINT32         height,
    UINT32         pitch
)
{
    RC rc;
    rgbe_header_info header;
    FileHolder fp;
    INT32 wt, ht;

    CHECK_RC(fp.Open(FileName, "rb"));

    if (RGBE_ReadHeader(fp.GetFile(), &wt, &ht, &header))
    {
        return RC::FILE_UNKNOWN_ERROR;
    }

    vector<float> data(wt*ht*3);

    if (RGBE_ReadPixels_RLE(fp.GetFile(), data.data(), wt, ht))
    {
        return RC::FILE_UNKNOWN_ERROR;
    }

    float rgba[4];
    UINT32 j = 0;
    UINT16 fp16Value;
    UINT16 hmov = 0;
    UINT16 wmov = 0;

    for (UINT32 h = hmov ; h < (height + hmov) ; h++)
    {
        for (UINT32 w = wmov; w < (width + wmov); w++)
        {
            // Get a Pixel and Colwert to rgba
            rgba[0] = data[w * 3 + wt*h*3 + 0];
            rgba[1] = data[w * 3 + wt*h*3 + 1];
            rgba[2] = data[w * 3 + wt*h*3 + 2];
            // Programming constant Alpha Value
            rgba[3] = 1.0f;

            for (UINT32 i = 0; i < 4; i++)
            {
                // Colwert it to FP16
                fp16Value = Utility::Float32ToFloat16(rgba[i]);
                // Load it into UINT08 buffer
                buf->at(j)      = fp16Value & 0x00FF;
                buf->at(j + 1)  = ((fp16Value & 0xFF00) >> 8);
                j               = j + 2;
            }
        }
    }
/*
    // Print the Pixel Values
    UINT32 i = 0;
    for (UINT32 j = 0; j < (width * height); i++,j++)
    {
        Printf(Tee::PriHigh,
        "Pixel Num %d: R16 = [0x%x%x],G16 = [0x%x%x],B16 =" 
        "[0x%x%x],A16 = [0x%x%x]  \n",i/8, buf[i],buf[i+1],
        buf[i+2],buf[i+3],buf[i+4],buf[i+5],buf[i+6],buf[i+7]);
        i = i + 7;
    }
*/
    return rc;
}

