/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2017,2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file  dmasurfr.cpp
 * @brief Implementation of surface reader which takes advantage of DMA transfers.
 */

#include "dmasurfr.h"
#include "gpu/utility/surf2d.h"
#include "gpu/utility/blocklin.h"
#include "gpu/include/gpudev.h"
#include "core/include/platform.h"
#include "dmard.h"
#include "mdiag/sysspec.h"

SurfaceUtils::DmaSurfaceReader::DmaSurfaceReader(const Surface2D& Surf, DmaReader& DmaMgr)
: m_Surf(Surf), m_DmaMgr(DmaMgr), m_Subdev(0)
{
}

SurfaceUtils::DmaSurfaceReader::~DmaSurfaceReader()
{
}

void SurfaceUtils::DmaSurfaceReader::SetTargetSubdevice(UINT32 Subdev)
{
    if (Subdev >= m_Surf.GetGpuDev()->GetNumSubdevices())
    {
        ErrPrintf("Invalid subdevice number!\n");
        MASSERT(0);
        return;
    }
    m_Subdev = Subdev;
}

void SurfaceUtils::DmaSurfaceReader::SetTargetSubdeviceForce(UINT32 Subdev)
{
    return SetTargetSubdevice(Subdev);
}

RC SurfaceUtils::DmaSurfaceReader::ReadBytes(UINT64 Offset, void* Buf, size_t Size)
{
    // Account for invisible part of the surface
    Offset += m_Surf.GetExtraAllocSize() + m_Surf.GetHiddenAllocSize();

    while (Size > 0)
    {
        // DMA reader cannot access memory not mapped into ctxdma
        if (Offset < m_Surf.GetHiddenAllocSize())
        {
            ErrPrintf("Unable to access unmapped \"hidden\" part of surface through DMA!\n");
            MASSERT(0);
            return RC::SOFTWARE_ERROR;
        }

        // Note: here we read directly from ctxdma, therefore
        // we don't care how memory has been mapped into ctxdma,
        // and we don't have to do anything special about split surfaces.

        // DMA transfer
        UINT32 CopySize = m_DmaMgr.GetBufferSize();
        if (CopySize > Size)
        {
            CopySize = static_cast<UINT32>(Size);
        }
        RC rc;
        CHECK_RC(m_DmaMgr.Read(
            m_Surf.GetCtxDmaHandleGpu(),
            Offset + m_Surf.GetCtxDmaOffsetGpu() - m_Surf.GetHiddenAllocSize(),
            CopySize,
            m_Subdev,
            &m_Surf));

        // Copy transferred surface contents
        Platform::MemCopy(Buf,
            reinterpret_cast<const void*>(m_DmaMgr.GetBuffer()), CopySize);

        // Advance pointers
        Buf = static_cast<UINT08*>(Buf) + CopySize;
        Offset += CopySize;
        Size -= CopySize;
    }
    return OK;
}

RC SurfaceUtils::DmaSurfaceReader::ReadBlocklinearLines
(
    const BlockLinear& blAddressMapper,
    UINT32 y,
    UINT32 z,
    UINT32 arrayIndex,
    void* destBuffer,
    UINT32 lineCount
)
{
    RC rc;

    UINT64 arrayOffset = arrayIndex * m_Surf.GetArrayPitch() +
        m_Surf.GetExtraAllocSize();
    UINT32 bpp = m_Surf.GetBytesPerPixel();

    // Callwlate the pitch size of an entire line of pixels.
    UINT32 lineSize = m_Surf.GetAllocWidth() * bpp;

    // If the maximum DMA copy size is less than the pitch size of
    // an entire line of pixels, each line will need to be read in segments.
    bool readPartialLine = (m_DmaMgr.GetBufferSize() < lineSize);

    // Keep copying lines of pixels until there are none left.
    while (lineCount > 0)
    {
        UINT32 linesToCopy;
        UINT32 lineLength;

        // Only read one line segment at a time if an entire line of pixels
        // is too large for the DMA copy buffer.
        if (readPartialLine)
        {
            linesToCopy = 1;
            lineLength = m_DmaMgr.GetBufferSize() / bpp;
        }

        // Otherwise, read as many lines as can fit within the DMA copy buffer.
        else
        {
            linesToCopy = min(m_DmaMgr.GetBufferSize() / lineSize, lineCount);
            lineLength = m_Surf.GetAllocWidth();
        }

        for (UINT32 x = 0; x < m_Surf.GetAllocWidth(); x += lineLength)
        {
            // Limit the current line segment to be read so that it doesn't
            // go past the surface width.
            if (x + lineLength > m_Surf.GetAllocWidth())
            {
                lineLength = m_Surf.GetAllocWidth() - x;
            }
            MASSERT(lineLength > 0);

            CHECK_RC(m_DmaMgr.ReadBlocklinearToPitch(
                arrayOffset + m_Surf.GetCtxDmaOffsetGpu(),
                x,
                y,
                z,
                lineLength,
                linesToCopy,
                m_Subdev,
                &m_Surf));

            if (readPartialLine)
            {
                Platform::MemCopy(static_cast<UINT08*>(destBuffer) + x * bpp,
                    reinterpret_cast<const void**>(m_DmaMgr.GetBuffer()),
                    lineLength * bpp);
            }
        }

        UINT32 copySize = linesToCopy * lineSize;

        if (!readPartialLine)
        {
            // Copy transferred surface contents
            Platform::MemCopy(destBuffer,
                reinterpret_cast<const void*>(m_DmaMgr.GetBuffer()), copySize);
        }

        // Advance pointers
        destBuffer = static_cast<UINT08*>(destBuffer) + copySize;
        y += linesToCopy;
        lineCount -= linesToCopy;
    }

    return rc;
}

const Surface2D& SurfaceUtils::DmaSurfaceReader::GetSurface() const
{
    return m_Surf;
}
