/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "surfasm.h"
#include "sclnrd.h"
#include "mdiag/utils/surfutil.h"
#include "mdiag/utils/IGpuSurfaceMgr.h"
#include "core/include/platform.h"
#include "core/include/utility.h"
#include "mdiag/sysspec.h"
#include "core/include/rc.h"

SurfaceAssembler::Rectangle::Rectangle(const ScanlineReader& reader)
: x(0), y(0), width(reader.GetSurfaceWidth()), height(reader.GetSurfaceHeight())
{
}

bool SurfaceAssembler::Rectangle::IsNull() const
{
    return (width == 0) || (height == 0);
}

const SurfaceAssembler::Rectangle g_NullRectangle(0, 0, 0, 0);

RC SurfaceAssembler::AddRect(UINT32 subdev, const Rectangle& src, UINT32 x, UINT32 y)
{
    struct CheckRect
    {
        UINT32 x1, y1, x2, y2;

        static bool Intersect(const CheckRect& r1, const CheckRect& r2)
        {
            return
                (r1.x2 > r2.x1) &&
                (r1.x1 < r2.x2) &&
                (r1.y2 > r2.y1) &&
                (r1.y1 < r2.y2);
        }
    };

    // Check if rectangles do not intersect
    const CheckRect src1 =  {src.x, src.y, src.x+src.width, src.y+src.height};
    const CheckRect dest1 = {x,     y,     x+src.width,     y+src.height};
    Sources::const_iterator srcIt = m_Sources.begin();
    for (; srcIt != m_Sources.end(); ++srcIt)
    {
        const Rectangle& rc = srcIt->src;
        const CheckRect src2 =  {rc.x,     rc.y,     rc.x+rc.width,     rc.y+rc.height};
        const CheckRect dest2 = {srcIt->x, srcIt->y, srcIt->x+rc.width, srcIt->y+rc.height};
        if (((subdev==srcIt->subdev) && CheckRect::Intersect(src1, src2))
            || CheckRect::Intersect(dest1, dest2))
        {
            if (CheckRect::Intersect(dest1, dest2))
            {
                ErrPrintf("Destination rectangles intersect: [%u %u %u %u] and [%u %u %u %u]\n",
                    dest1.x1, dest1.y1, dest1.x2, dest1.y2,
                    dest2.x1, dest2.y1, dest2.x2, dest2.y2);
            }
            else
            {
                ErrPrintf("Source rectangles intersect: [%u %u %u %u] and [%u %u %u %u]\n",
                    src1.x1, src1.y1, src1.x2, src1.y2,
                    src2.x1, src2.y1, src2.x2, src2.y2);
            }
            return RC::SOFTWARE_ERROR;
        }
    }

    // Remember rectangle
    const Source source = {subdev, src, x, y};
    m_Sources.push_back(source);
    return OK;
}

RC SurfaceAssembler::Assemble(ScanlineReader& reader, UINT32 size)
{
    // Compute sizes and allocate output buffer
    UINT32 width = reader.GetSurfaceWidth();
    UINT32 height = reader.GetSurfaceHeight();
    UINT32 depth = reader.GetSurface().GetBytesPerPixel();
    const UINT32 numLayers = reader.GetSurface().GetDepth();
    const UINT32 arraySize = reader.GetSurface().GetArraySize();
    if (width == 0)
    {
        width = reader.GetSurface().GetSize();
        height = 1;
        depth = 1;
    }
    size > 0 ? m_Buf.resize(size) :
               m_Buf.resize(width * depth * height * numLayers * arraySize);

    // Copy subsequent rectangles
    Sources::const_iterator srcIt = m_Sources.begin();
    for (; srcIt != m_Sources.end(); ++srcIt)
    {
        // Read rectangle copy specification
        const UINT32 srcX = srcIt->src.x;
        const UINT32 srcY = srcIt->src.y;
        const UINT32 srcWidth = srcIt->src.width;
        const UINT32 srcHeight = srcIt->src.height;
        const UINT32 destX = srcIt->x;
        const UINT32 destY = srcIt->y;

        // Position at the first source scanline
        reader.Restart(srcIt->subdev);
        reader.SkipScanlines(srcY);

        // Iterate over subsurfaces and layers
        for (UINT32 iSubSurf=0; iSubSurf < arraySize; iSubSurf++)
        {
            for (UINT32 iLayer=0; iLayer < numLayers; iLayer++)
            {
                // Callwlate destination buffer initial position and relative shift
                const UINT32 initY = m_MirrorY ? (destY+srcHeight-1) : destY;
                const UINT32 destWidth = size > 0 ? srcWidth : width;
                const UINT32 destHeight= size > 0 ? srcHeight : height;
                UINT32 destPos =
                    depth * (
                    destX + destWidth * (
                    initY + destHeight * (
                    iLayer + numLayers *
                    iSubSurf
                    )));
                const UINT32 destShift = m_MirrorY ? (0U-(depth*destWidth)) : (depth*destWidth);

                // Read subsequent scanlines
                for (UINT32 iScanline=0; iScanline < srcHeight; iScanline++)
                {
                    RC rc;
                    CHECK_RC(reader.ReadScanline(&m_Buf[destPos], srcX, srcWidth));
                    destPos += destShift;
                }
                reader.SkipScanlines(height - srcHeight);
            }
        }
    }
    return OK;
}
