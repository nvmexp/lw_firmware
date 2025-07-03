/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2014,2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef _SCANLINE_READER_IMPL_H_
#define _SCANLINE_READER_IMPL_H_

#include "sclnrd.h"
#include "gpu/utility/blocklin.h"
#include "gpu/utility/surf2d.h"
#include "mdiag/utils/IGpuSurfaceMgr.h"
#include "gpu/utility/surfrdwr.h"

//! Creates scanline reader.
//! \param scanlineReader Pointer to the return object.
//! \param surfReader Surface reader holding the surface.
//! \param surfManager Surface manager.
//! \param partialMappings Information about partial mappings, if they exist.
//! \param forcePitch Force the surface to be interpreted as pitch layout.
//! \param blocklinearToPitch Copy blocklinear to pitch instead of the default
//!                           pitch to pitch copy.
RC CreateScanlineReader(
    unique_ptr<ScanlineReader>* scanlineReader,
    SurfaceUtils::ISurfaceReader& surfReader,
    IGpuSurfaceMgr& surfManager,
    PartialMappings *partialMappings,
    bool forcePitch,
    bool blocklinearToPitch);

//! Implementation of scanline reader for block-linear surfaces.
//!
//! Internally caches subsequent layers, because on block-linear surfaces
//! pixels are scattered around the surface and we don't know how to fetch
//! subsequent pixels in a linear fashion. Thus we need to read whole layers
//! into the cache and then easily address them.
class ScanlineReaderForBlockLinearSurfaces: public ScanlineReader
{
public:
    ScanlineReaderForBlockLinearSurfaces(
            SurfaceUtils::ISurfaceReader& surfReader,
            IGpuSurfaceMgr& surfManager);
    virtual ~ScanlineReaderForBlockLinearSurfaces();
    virtual RC ReadScanline(UINT08* pBuf, UINT32 x, UINT32 width);

protected:
    virtual void RestartImpl();

    UINT64 m_SurfOffset;            //!< Current offset into the surface.
    BlockLinear m_CoordMapper;      //!< Maps pixel coordinates to offset in
                                    //!  the surface.
    vector<UINT08> m_Cache;         //!< Layer cache.
    UINT32 m_SubdevInCache;         //!< Index of the subdevice whose layer is in cache.
    UINT32 m_CacheArrayIndex;       //!< Index of the array element in the cache.
};

//! Implementation of scanline reader for pitch surfaces.
class ScanlineReaderForPitchSurfaces: public ScanlineReader
{
public:
    ScanlineReaderForPitchSurfaces(
            SurfaceUtils::ISurfaceReader& surfReader,
            IGpuSurfaceMgr& surfManager);
    virtual ~ScanlineReaderForPitchSurfaces();
    virtual RC ReadScanline(UINT08* pBuf, UINT32 x, UINT32 width);

private:
    virtual void RestartImpl();
};

//! Implementation of scanline reader for block-linear surfaces.
//!
//! Internally caches subsequent layers, because on block-linear surfaces
//! pixels are scattered around the surface and we don't know how to fetch
//! subsequent pixels in a linear fashion. Thus we need to read whole layers
//! into the cache and then easily address them.
class ScanlineReaderForPartiallyMappedBlockLinearSurfaces: public ScanlineReader
{
public:
    ScanlineReaderForPartiallyMappedBlockLinearSurfaces(
            SurfaceUtils::ISurfaceReader& surfReader,
            IGpuSurfaceMgr& surfManager,
            PartialMappings *partialMappings);
    virtual ~ScanlineReaderForPartiallyMappedBlockLinearSurfaces();
    virtual RC ReadScanline(UINT08* pBuf, UINT32 x, UINT32 width);

private:
    virtual void RestartImpl();
    RC UpdateCache(UINT64 requestedAddress);

    vector<UINT08> m_Cache;
    BlockLinear m_CoordMapper;
    PartialMappings *m_PartialMappings;
    UINT64 m_CacheMinOffset;
    UINT64 m_CacheMaxOffset;
    UINT32 m_SubdevInCache;
    unique_ptr<SurfaceUtils::MappingSurfaceReader> m_MappingReader;
};

//! Implementation of scanline reader for pitch surfaces.
class ScanlineReaderForPartiallyMappedPitchSurfaces: public ScanlineReader
{
public:
    ScanlineReaderForPartiallyMappedPitchSurfaces(
            SurfaceUtils::ISurfaceReader& surfReader,
            IGpuSurfaceMgr& surfManager,
            PartialMappings *partialMappings);
    virtual ~ScanlineReaderForPartiallyMappedPitchSurfaces();
    virtual RC ReadScanline(UINT08* pBuf, UINT32 x, UINT32 width);

private:
    virtual void RestartImpl();
    RC UpdateMappingReader(UINT64 requestedOffset);

    PartialMappings *m_PartialMappings;
    unique_ptr<SurfaceUtils::MappingSurfaceReader> m_MappingReader;
    UINT64 m_MappedMinOffset;
    UINT64 m_MappedMaxOffset;
};

//! Implementation of scanline reader for block-linear surfaces.
//!
//! Internally caches subsequent layers, because on block-linear surfaces
//! pixels are scattered around the surface and we don't know how to fetch
//! subsequent pixels in a linear fashion. Thus we need to read whole layers
//! into the cache and then easily address them.
class ScanlineReaderBlockLinearToPitch: public ScanlineReader
{
public:
    ScanlineReaderBlockLinearToPitch(
            SurfaceUtils::ISurfaceReader& surfReader,
            IGpuSurfaceMgr& surfManager);
    virtual ~ScanlineReaderBlockLinearToPitch();
    virtual RC ReadScanline(UINT08* pBuf, UINT32 x, UINT32 width);

protected:
    virtual void RestartImpl();

    UINT64 m_SurfOffset;        //!< Current offset into the surface.
    BlockLinear m_CoordMapper;  //!< Maps pixel coordinates to offset in the surface
    vector<UINT08> m_Cache;     //!< Layer cache.
    UINT32 m_SubdevInCache;     //!< Index of the subdevice whose layer is in cache.
    UINT32 m_CacheArrayIndex;   //!< Index of the array element in the cache.
    UINT32 m_CacheDepthIndex;   //!< Index of the depth element in the cache.
    UINT32 m_CacheFirstY;       //!< Y coordinate of the first line in the cache
    UINT32 m_CacheLastY;        //!< Y coordinate of the first line in the cache
};

#endif // _SCANLINE_READER_IMPL_H_
