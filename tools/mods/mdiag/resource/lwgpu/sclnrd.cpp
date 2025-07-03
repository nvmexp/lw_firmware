/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "sclnrdim.h"
#include "gpu/utility/surfrdwr.h"
#include "mdiag/utils/IGpuSurfaceMgr.h"
#include "core/include/utility.h"
#include "mdiag/sysspec.h"
#include "gpu/include/gpudev.h"
#include "core/include/rc.h"
#include "mdiag/utils/mdiagsurf.h"

RC CreateScanlineReader(
    unique_ptr<ScanlineReader>* scanlineReader,
    SurfaceUtils::ISurfaceReader& surfReader,
    IGpuSurfaceMgr& surfManager,
    PartialMappings *partialMappings,
    bool forcePitch,
    bool blocklinearToPitch)
{
    RC rc;

    if (forcePitch)
    {
        scanlineReader->reset(
            new ScanlineReaderForPitchSurfaces(surfReader, surfManager));
    }

    switch (surfReader.GetSurface().GetLayout())
    {
    case Surface2D::Swizzled:
        MASSERT(!"Swizzled is not supported anymore");
        rc = RC::SOFTWARE_ERROR;
        break;

    case Surface2D::BlockLinear:
        if (blocklinearToPitch)
        {
            scanlineReader->reset(
                new ScanlineReaderBlockLinearToPitch(surfReader, surfManager));
        }
        else if ((partialMappings != 0) && (partialMappings->size() > 0))
        {
            scanlineReader->reset(
                new ScanlineReaderForPartiallyMappedBlockLinearSurfaces(
                    surfReader, surfManager, partialMappings));
        }
        else
        {
            scanlineReader->reset(
                new ScanlineReaderForBlockLinearSurfaces(
                    surfReader, surfManager));
        }
        break;

    case Surface2D::Pitch:
        if ((partialMappings != 0) && (partialMappings->size() > 0))
        {
            scanlineReader->reset(
                new ScanlineReaderForPartiallyMappedPitchSurfaces(
                    surfReader, surfManager, partialMappings));
        }
        else
        {
            scanlineReader->reset(
                new ScanlineReaderForPitchSurfaces(surfReader, surfManager));
        }
        break;

    default:
        MASSERT(!"Invalid surface layout");
        rc = RC::SOFTWARE_ERROR;
        break;
    }
    return rc;
}

void ScanlineReader::Restart(UINT32 subdev)
{
    m_SurfaceReader.SetTargetSubdevice(subdev);
    m_Subdev = subdev;
    RestartImpl();
}

const Surface2D& ScanlineReader::GetSurface() const
{
    return m_SurfaceReader.GetSurface();
}

UINT32 ScanlineReader::GetSurfaceWidth() const
{
    return GetSurface().GetWidth();
}

UINT32 ScanlineReader::GetSurfaceHeight() const
{
    if (m_pOrigSurface)
        return m_pOrigSurface->GetHeight();
    else
        return GetSurface().GetHeight();
}

UINT32 ScanlineReader::GetWindowX() const
{
    if(GetSurface().GetViewportOffsetExist())
        return GetSurface().GetViewportOffsetX();
    else
        return GetSurface().GetWindowOffsetX();
}

UINT32 ScanlineReader::GetWindowY() const
{
    if(GetSurface().GetViewportOffsetExist())
        return GetSurface().GetViewportOffsetY();
    else
        return GetSurface().GetWindowOffsetY();
}

UINT32 ScanlineReader::GetGranularity() const
{
    LW50Raster& raster = *m_SurfaceManager.GetRaster(GetSurface().GetColorFormat());
    return raster.Granularity();
}

ScanlineReaderForBlockLinearSurfaces::ScanlineReaderForBlockLinearSurfaces(
    SurfaceUtils::ISurfaceReader& surfReader, IGpuSurfaceMgr& surfManager)
: ScanlineReader(surfReader, surfManager),
  m_CoordMapper(
        surfReader.GetSurface().GetAllocWidth(),
        surfReader.GetSurface().GetAllocHeight(),
        surfReader.GetSurface().GetAllocDepth(),
        surfReader.GetSurface().GetLogBlockWidth(),
        surfReader.GetSurface().GetLogBlockHeight(),
        surfReader.GetSurface().GetLogBlockDepth(),
        surfReader.GetSurface().GetBytesPerPixel(),
        surfReader.GetSurface().GetGpuDev())
{
    m_SubdevInCache = ~0U;
    RestartImpl();
}

ScanlineReaderForBlockLinearSurfaces::~ScanlineReaderForBlockLinearSurfaces()
{
}

void ScanlineReaderForBlockLinearSurfaces::RestartImpl()
{
    m_ScanlineIndex = 0;
    if (GetLwrrentSubdev() != m_SubdevInCache)
    {
        m_Cache.clear();
    }
    m_SurfOffset = 0;
    m_CacheArrayIndex = ~0U;
}

RC ScanlineReaderForBlockLinearSurfaces::ReadScanline(
    UINT08* pBuf, UINT32 x, UINT32 width)
{
    const UINT64 MAX_CACHE_SIZE = 0x1000000;

    // Callwlate current position from scanline
    UINT32 arrayIndex;
    UINT32 depthIndex;
    UINT32 y;
    CalcPosition(m_ScanlineIndex, &arrayIndex, &depthIndex, &y);

    UINT64 minOffset;
    UINT64 cacheSize;

    x += GetWindowX();
    y += GetWindowY();
    const UINT32 endX = x + width;
    const UINT32 bpp = GetSurface().GetBytesPerPixel();

    // If the size of an array element is greater than the maximum cache size,
    // then instead of storing an entire array element in the cache, store
    // enough data so that at least this scanline can be read, plus all of
    // the scanlines that are in the same row of blocks.
    if (GetSurface().GetArrayPitch() > MAX_CACHE_SIZE)
    {
        // Callwlate the offsets of the first pixel of the scanline and
        // the last pixel of the scanline.
        minOffset = m_CoordMapper.Address(x, y, depthIndex);
        UINT64 maxOffset = m_CoordMapper.Address(endX - 1, y, depthIndex) + bpp;
        UINT64 blockSize = GetSurface().GetGpuDev()->GobSize() *
            (1 << m_CoordMapper.Log2BlockWidthGob()) *
            (1 << m_CoordMapper.Log2BlockHeightGob()) *
            (1 << m_CoordMapper.Log2BlockDepthGob());
        minOffset = ALIGN_DOWN(minOffset, blockSize);
        maxOffset = ALIGN_UP(maxOffset, blockSize);
        cacheSize = maxOffset - minOffset;
        minOffset += arrayIndex * GetSurface().GetArrayPitch();
    }
    else
    {
        minOffset = arrayIndex * GetSurface().GetArrayPitch();
        cacheSize = GetSurface().GetArrayPitch();
    }

    // The cache needs to be filled if it's empty, or if it has data from a
    // different array element then the current one, or if the starting offset
    // is different from the current requested offset, or if the amount
    // of requested data is greater than the amount of data in the cache.
    if (m_Cache.empty() ||
        (m_CacheArrayIndex != arrayIndex) ||
        (m_SurfOffset != minOffset) ||
        (cacheSize > m_Cache.size()))
    {
        m_SurfOffset = minOffset;
        m_Cache.resize(cacheSize);

        RC rc;
        CHECK_RC(GetReader().ReadBytes(m_SurfOffset, &m_Cache[0], cacheSize));

        if (GetOrigSurface()->MemAccessNeedsColwersion(nullptr, MdiagSurf::RdSurf))
        {
            PixelFormatTransform::ColwertRawToNaiveSurf(GetOrigSurface(), &m_Cache[0], cacheSize);
        }

        m_SubdevInCache = GetLwrrentSubdev();
        m_CacheArrayIndex = arrayIndex;
    }

    if ((GetSurface().GetWidth() == 0) || (GetSurface().GetHeight() == 0))
    {
        const UINT08* pSrc = &m_Cache[0];
        memcpy(pBuf, pSrc, cacheSize);
        InfoPrintf("%s, Skip naive bl to pitch swizzle for surface %s because of zero width/height.\n",
	           __FUNCTION__, GetSurface().GetName().c_str());
    }
    else // Swizzle from naive bl to pitch
    {
        // Copy subsequent pixels

        for (; x != endX; x++)
        {
            const UINT64 srcOffset = m_CoordMapper.Address(x, y, depthIndex) +
                arrayIndex * GetSurface().GetArrayPitch();
            if (srcOffset + bpp - m_SurfOffset > m_Cache.size())
            {
                ErrPrintf("attempt to access pixel (%u,%u) outside the cache.  "
                        "srcOffset = 0x%llx  bpp = %u  m_SurfOffset = 0x%llx  cache size = %u\n",
                        x, y, srcOffset, bpp, m_SurfOffset, m_Cache.size());
                return RC::SOFTWARE_ERROR;
            }
            const UINT08* pSrc = &m_Cache[srcOffset - m_SurfOffset];
            int i = bpp;
            do
            {
                *(pBuf++) = *(pSrc++);
            }
            while (--i);
        }
    }
    m_ScanlineIndex++;
    return OK;
}

ScanlineReaderForPitchSurfaces::ScanlineReaderForPitchSurfaces(
    SurfaceUtils::ISurfaceReader& surfReader,
    IGpuSurfaceMgr& surfManager)
: ScanlineReader(surfReader, surfManager)
{
    RestartImpl();
}

ScanlineReaderForPitchSurfaces::~ScanlineReaderForPitchSurfaces()
{
}

void ScanlineReaderForPitchSurfaces::RestartImpl()
{
    m_ScanlineIndex = 0;
}

void ScanlineReader::CalcPosition(UINT32 scanline, UINT32* iSubSurf, UINT32* iLayer, UINT32* y)
{
    const UINT32 numLayers = GetSurface().GetDepth();
    UINT32 surfHeight = GetSurfaceHeight();
    if (surfHeight == 0) surfHeight = 1;
    const UINT32 nLayer = scanline / surfHeight;
    *iSubSurf = nLayer / numLayers;
    *iLayer = nLayer % numLayers;
    *y = scanline % surfHeight;
}

RC ScanlineReaderForPitchSurfaces::ReadScanline(
    UINT08* pBuf, UINT32 x, UINT32 width)
{
    // Get surface data
    const Surface2D& surf = GetSurface();
    UINT32 depth = surf.GetBytesPerPixel();
    UINT64 pitch = surf.GetPitch();
    UINT32 allocHeight = surf.GetAllocHeight();
    if (pitch == 0)
    {
        pitch = surf.GetSize();
        depth = 1;
        allocHeight = 1;
    }

    // Callwlate current position from scanline
    UINT32 iSubSurf, iLayer, y;
    CalcPosition(m_ScanlineIndex, &iSubSurf, &iLayer, &y);

    // Callwlate current scanline's first byte offset
    const UINT64 startBytePos =
        // window offset from surface manager
        GetWindowY() * pitch +
        GetWindowX() * depth +
        // subsurface index
        iSubSurf * surf.GetArrayPitch() +
        // layer index
        iLayer * pitch * allocHeight +
        // scanline
        y * pitch;

    // Read desired pixels
    RC rc;
    CHECK_RC(GetReader().ReadBytes(x*depth + startBytePos, pBuf, width*depth));

    // Update current position
    m_ScanlineIndex++;
    return OK;
}

ScanlineReaderForPartiallyMappedBlockLinearSurfaces::ScanlineReaderForPartiallyMappedBlockLinearSurfaces
(
    SurfaceUtils::ISurfaceReader& surfReader,
    IGpuSurfaceMgr& surfManager,
    PartialMappings *partialMappings
)
:   ScanlineReader(surfReader, surfManager),
    m_CoordMapper(
        surfReader.GetSurface().GetAllocWidth(),
        surfReader.GetSurface().GetAllocHeight(),
        surfReader.GetSurface().GetAllocDepth(),
        surfReader.GetSurface().GetLogBlockWidth(),
        surfReader.GetSurface().GetLogBlockHeight(),
        surfReader.GetSurface().GetLogBlockDepth(),
        surfReader.GetSurface().GetBytesPerPixel(),
        surfReader.GetSurface().GetGpuDev()),
    m_PartialMappings(partialMappings)
{
    m_SubdevInCache = ~0U;
    RestartImpl();
}

ScanlineReaderForPartiallyMappedBlockLinearSurfaces::~ScanlineReaderForPartiallyMappedBlockLinearSurfaces()
{
}

void ScanlineReaderForPartiallyMappedBlockLinearSurfaces::RestartImpl()
{
    m_ScanlineIndex = 0;
    m_CacheMinOffset = 0;
    m_CacheMaxOffset = 0;
    m_Cache.clear();
    m_MappingReader.reset(0);
}

RC ScanlineReaderForPartiallyMappedBlockLinearSurfaces::ReadScanline
(
    UINT08* pBuf,
    UINT32 x,
    UINT32 width
)
{
    RC rc;

    // Callwlate current position from scanline
    UINT32 arrayIndex;
    UINT32 depthIndex;
    UINT32 y;
    CalcPosition(m_ScanlineIndex, &arrayIndex, &depthIndex, &y);

    UINT64 arrayOffset = arrayIndex * GetSurface().GetArrayPitch();
    x += GetWindowX();
    y += GetWindowY();
    UINT32 endX = x + width;
    UINT32 bpp = GetSurface().GetBytesPerPixel();

    UINT64 minOffset = m_CoordMapper.Address(x, y, depthIndex) + arrayOffset;

    // Fill cache if needed
    if (m_Cache.empty() ||
        (minOffset < m_CacheMinOffset) ||
        (minOffset >= m_CacheMaxOffset))
    {
        CHECK_RC(UpdateCache(minOffset));
    }

    // Copy subsequent pixels
    UINT64 offset;

    for (; x != endX; x++)
    {
        offset = m_CoordMapper.Address(x, y, depthIndex) + arrayOffset;

        if (offset + bpp - m_CacheMinOffset > m_Cache.size())
        {
            CHECK_RC(UpdateCache(offset));
        }
        const UINT08* pSrc = &m_Cache[offset - m_CacheMinOffset];
        int i = bpp;
        do
        {
            *(pBuf++) = *(pSrc++);
        }
        while (--i);
    }
    ++m_ScanlineIndex;
    return OK;
}

RC ScanlineReaderForPartiallyMappedBlockLinearSurfaces::UpdateCache
(
    UINT64 requestedOffset
)
{
    RC rc;
    PartialMappings::iterator iter;

    for (iter = m_PartialMappings->begin();
         iter != m_PartialMappings->end();
         ++iter)
    {
        PartialMapData *Mapdata = (*iter);
        SurfaceMapDatas surfaceData = Mapdata->surfaceMapDatas;
        for(SurfaceMapDatas::iterator siter = surfaceData.begin();siter != surfaceData.end();++siter)
        {
            if(requestedOffset >= (*siter)->minOffset && requestedOffset < (*siter)->maxOffset)
            {
                m_CacheMinOffset = (*siter)->minOffset;
                m_CacheMaxOffset = (*siter)->maxOffset;
                UINT64 cacheSize = (*siter)->maxOffset - (*siter)->minOffset;
                m_Cache.resize(cacheSize);

                m_MappingReader = SurfaceUtils::CreateMappingSurfaceReader((*siter)->map);
                m_MappingReader->SetTargetSubdevice(GetLwrrentSubdev());
                CHECK_RC(m_MappingReader->ReadBytes(0, &m_Cache[0], cacheSize));

                if (GetOrigSurface()->MemAccessNeedsColwersion(nullptr, MdiagSurf::RdSurf))
                {
                    PixelFormatTransform::ColwertRawToNaiveSurf(GetOrigSurface(), &m_Cache[0], cacheSize);
                }

                m_SubdevInCache = GetLwrrentSubdev();
                return rc;
            }
        }
    }

    ErrPrintf("No partial mapping found for offset 0x%llx.\n",
        requestedOffset);

    return RC::SOFTWARE_ERROR;
}

ScanlineReaderForPartiallyMappedPitchSurfaces::ScanlineReaderForPartiallyMappedPitchSurfaces
(
    SurfaceUtils::ISurfaceReader& surfReader,
    IGpuSurfaceMgr& surfManager,
    PartialMappings *partialMappings
)
:   ScanlineReader(surfReader, surfManager),
    m_PartialMappings(partialMappings)
{
    m_MappedMinOffset = ~0x0;
    m_MappedMaxOffset = 0;
    RestartImpl();
}

ScanlineReaderForPartiallyMappedPitchSurfaces::~ScanlineReaderForPartiallyMappedPitchSurfaces()
{
}

void ScanlineReaderForPartiallyMappedPitchSurfaces::RestartImpl()
{
    m_ScanlineIndex = 0;
    m_MappingReader.reset(0);
}

RC ScanlineReaderForPartiallyMappedPitchSurfaces::ReadScanline(
    UINT08* pBuf, UINT32 x, UINT32 width)
{
    RC rc;

    // Get surface data
    const Surface2D& surf = GetSurface();
    UINT32 bpp = surf.GetBytesPerPixel();
    UINT64 pitch = surf.GetPitch();
    UINT32 allocHeight = surf.GetAllocHeight();
    if (pitch == 0)
    {
        pitch = surf.GetSize();
        bpp = 1;
        allocHeight = 1;
    }

    // Callwlate current position from scanline
    UINT32 arrayIndex;
    UINT32 depthIndex;
    UINT32 y;
    CalcPosition(m_ScanlineIndex, &arrayIndex, &depthIndex, &y);

    // Callwlate current scanline's first byte offset
    UINT64 minOffset = (UINT64)(x + GetWindowX()) * bpp;
    minOffset += (y + GetWindowY()) * pitch;
    minOffset += arrayIndex * surf.GetArrayPitch();
    minOffset += depthIndex * pitch * allocHeight;
    UINT64 maxOffset = minOffset + width * bpp;

    if ((m_MappingReader.get() == 0) ||
        (minOffset < m_MappedMinOffset) ||
        (minOffset >= m_MappedMaxOffset))
    {
        CHECK_RC(UpdateMappingReader(minOffset));
    }

    if (minOffset < m_MappedMinOffset || maxOffset > m_MappedMaxOffset)
    {
        ErrPrintf("scanline covers offset range 0x%llx to 0x%llx but "
            "partial mapping only covers offset range 0x%llx to 0x%llx.\n",
            minOffset, maxOffset, m_MappedMinOffset, m_MappedMaxOffset);
        return RC::SOFTWARE_ERROR;
    }

    CHECK_RC(m_MappingReader->ReadBytes(minOffset - m_MappedMinOffset,
            pBuf, width * bpp));

    // Update current position
    ++m_ScanlineIndex;

    return rc;
}

RC ScanlineReaderForPartiallyMappedPitchSurfaces::UpdateMappingReader
(
    UINT64 requestedOffset
)
{
    // Find the partial mapping that covers this scanline.
    PartialMappings::iterator iter;

    for (iter = m_PartialMappings->begin();
         iter != m_PartialMappings->end();
         ++iter)
    {
        PartialMapData *Mapdata = (*iter);
        vector<surfaceMapData *> surfaceData = Mapdata->surfaceMapDatas;
        for(vector<surfaceMapData *>::iterator siter = surfaceData.begin();siter != surfaceData.end();siter++)
        {
            if(requestedOffset >= (*siter)->minOffset && requestedOffset < (*siter)->maxOffset)
            {
                m_MappingReader = SurfaceUtils::CreateMappingSurfaceReader((*siter)->map);
                m_MappingReader->SetTargetSubdevice(GetLwrrentSubdev());
                m_MappedMinOffset = (*siter)->minOffset;
                m_MappedMaxOffset = (*siter)->maxOffset;
                return OK;
            }
        }
    }

    ErrPrintf("No partial mapping found for offset 0x%llx.\n",
        requestedOffset);

    return RC::SOFTWARE_ERROR;
}

ScanlineReaderBlockLinearToPitch::ScanlineReaderBlockLinearToPitch(
    SurfaceUtils::ISurfaceReader& surfReader, IGpuSurfaceMgr& surfManager)
: ScanlineReader(surfReader, surfManager),
  m_CoordMapper(
        surfReader.GetSurface().GetAllocWidth(),
        surfReader.GetSurface().GetAllocHeight(),
        surfReader.GetSurface().GetAllocDepth(),
        surfReader.GetSurface().GetLogBlockWidth(),
        surfReader.GetSurface().GetLogBlockHeight(),
        surfReader.GetSurface().GetLogBlockDepth(),
        surfReader.GetSurface().GetBytesPerPixel(),
        surfReader.GetSurface().GetGpuDev())
{
    m_SubdevInCache = ~0U;
    RestartImpl();
}

ScanlineReaderBlockLinearToPitch::~ScanlineReaderBlockLinearToPitch()
{
}

void ScanlineReaderBlockLinearToPitch::RestartImpl()
{
    m_ScanlineIndex = 0;
    if (GetLwrrentSubdev() != m_SubdevInCache)
    {
        m_Cache.clear();
    }
    m_SurfOffset = 0;
    m_CacheArrayIndex = ~0U;
    m_CacheDepthIndex = ~0U;
    m_CacheFirstY = ~0U;
    m_CacheLastY = ~0U;
}

RC ScanlineReaderBlockLinearToPitch::ReadScanline
(
    UINT08* pBuf,
    UINT32 x,
    UINT32 width
)
{
    RC rc;

    // Callwlate current position from scanline
    UINT32 arrayIndex;
    UINT32 depthIndex;
    UINT32 y;
    CalcPosition(m_ScanlineIndex, &arrayIndex, &depthIndex, &y);

    x += GetWindowX();
    y += GetWindowY();
    const UINT32 endX = x + width;
    const UINT32 bpp = GetSurface().GetBytesPerPixel();

    UINT64 lineSize = GetSurface().GetAllocWidth() * bpp;
    UINT64 depthLayerSize = lineSize * GetSurface().GetAllocHeight();
    UINT64 arrayElementSize = depthLayerSize * GetSurface().GetDepth();

    // Read data into the cache if the current scanline isn't there.
    if (m_Cache.empty() ||
        (m_CacheArrayIndex != arrayIndex) ||
        (m_CacheDepthIndex != depthIndex) ||
        (y < m_CacheFirstY) ||
        (y > m_CacheLastY))
    {
        // Read the remaining lines in the current layer unless the total size
        // would exceed the maximum cache size, in which case read as many
        // lines as would fit in the cache.

        const UINT64 MAX_CACHE_SIZE = 0x1000000;
        UINT32 linesInCache = GetSurface().GetAllocHeight() - y;
        if (linesInCache * lineSize > MAX_CACHE_SIZE)
        {
            linesInCache = MAX_CACHE_SIZE / lineSize;
        }
        UINT64 cacheSize = linesInCache * lineSize;

        m_Cache.resize(cacheSize);

        CHECK_RC(GetReader().ReadBlocklinearLines(m_CoordMapper, y,
            depthIndex, arrayIndex, &m_Cache[0], linesInCache));

        m_SubdevInCache = GetLwrrentSubdev();
        m_CacheArrayIndex = arrayIndex;
        m_CacheDepthIndex = depthIndex;
        m_CacheFirstY = y;
        m_CacheLastY = y + linesInCache - 1;

        m_SurfOffset = arrayIndex * arrayElementSize +
            depthIndex * depthLayerSize +
            static_cast<UINT64>(y) * GetSurface().GetAllocWidth() * bpp;
    }

    // Copy subsequent pixels

    for (; x != endX; x++)
    {
        const UINT64 srcOffset = x * bpp + y * lineSize +
            depthIndex * depthLayerSize + arrayIndex * arrayElementSize;

        if (srcOffset + bpp - m_SurfOffset > m_Cache.size())
        {
            ErrPrintf("attempt to access pixel outside the cache.  "
                "srcOffset = 0x%llx  "
                "x = %u  "
                "y = %u  "
                "bpp = %u  "
                "depth index = %u  "
                "array index = %u  "
                "m_SurfOffset = 0x%llx  "
                "cache size = %u\n",
                srcOffset,
                x,
                y,
                bpp,
                depthIndex,
                arrayIndex,
                m_SurfOffset,
                m_Cache.size());

            return RC::SOFTWARE_ERROR;
        }
        const UINT08* pSrc = &m_Cache[srcOffset - m_SurfOffset];
        int i = bpp;
        do
        {
            *(pBuf++) = *(pSrc++);
        }
        while (--i);
    }
    m_ScanlineIndex++;

    return rc;
}
