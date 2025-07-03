/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2016,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! \file gpugsurf.cpp
//! \brief Implements the GpuGoldenSurfaces class.

#include "gpu/include/gpugoldensurfaces.h"
#include "surf2d.h"
#include "gpu/include/dmawrap.h"
#include "core/include/platform.h"
#include "core/include/utility.h"
#include "lwos.h"
#include "blocklin.h"
#include "core/include/channel.h"
#include "gpu/tests/gputestc.h"
#include "gpu/include/gpudev.h"
#include "core/include/display.h"
#include "core/include/extcrcdev.h"

static const UINT32 NO_CACHED_SURFACE = 0xffffffff;

const Memory::Location GpuGoldenSurfaces::m_CacheSurfLocation = Memory::Coherent;

//------------------------------------------------------------------------------
GpuGoldenSurfaces::GpuGoldenSurfaces(GpuDevice *gpudev)
{
    m_LwrCachedSurface      = NO_CACHED_SURFACE;
    m_pCacheSurf = NULL;
    m_pPitchSurface = NULL;
    m_PitchSurfaceSize = 0;
    m_pTestConfig = 0;
    m_TimeoutMs = 0;
    m_pDmaWrapper = nullptr;
    m_ReduceOptimization = false;

    MASSERT(gpudev);
    m_pGpuDev = gpudev;
}

//------------------------------------------------------------------------------
GpuGoldenSurfaces::~GpuGoldenSurfaces()
{
    if (m_pCacheSurf)
    {
        m_pCacheSurf->Free();
        delete m_pCacheSurf;
        m_pCacheSurf = NULL;
    }
    if (m_pPitchSurface)
    {
        delete [] m_pPitchSurface;
        m_pPitchSurface = NULL;
    }
    if (nullptr != m_pDmaWrapper)
    {
        delete m_pDmaWrapper;
    }
    if (m_pTestConfig)
    {
        delete m_pTestConfig;
    }
}

//------------------------------------------------------------------------------
int GpuGoldenSurfaces::NumSurfaces() const
{
    return int(m_Surfaces.size());
}

//------------------------------------------------------------------------------
const string & GpuGoldenSurfaces::Name (int surfNum) const
{
    MASSERT(surfNum < int(m_Surfaces.size()));

    return m_Surfaces[surfNum].Name;
}

bool GpuGoldenSurfaces::UseDmaForCache
(
    int surfNum,
    Goldelwalues::BufferFetchHint bufFetchHint,
    UINT32 subdevNum
)
{
    bool ret = (bufFetchHint == Goldelwalues::opCpuDma);
    if (m_ReduceOptimization)
    {
        ret = false;
    }
    else if (Memory::Coherent == m_Surfaces[surfNum].pSurf->GetLocation() &&
             Surface2D::Pitch == m_Surfaces[surfNum].pSurf->GetLayout() &&
             false            == m_Surfaces[surfNum].pSurf->GetCompressed() &&
             false            == m_Surfaces[surfNum].pSurf->GetEncryption())
    {
        // Surface is already plain old cached sysmem, no gain from DMA-ing it.
        ret = false;
    }
    // DMA on simulated SOC is slow
    else if (GetGpuDev()->GetSubdevice(subdevNum)->IsSOC() &&
        Platform::GetSimulationMode() != Platform::Hardware)
    {
        ret = false;
    }
    else if (m_Surfaces[surfNum].pSurf->GetTiled() &&
        m_Surfaces[surfNum].pSurf->GetLocation() != Memory::Fb)
    {
        // Bug 236624 WAR, we force DMA for tiled sysmem surfaces (pre-g80).
        ret = true;
    }

    return ret;
}

RC GpuGoldenSurfaces::GetPitchAlignRequirement(UINT32 *pitch)
{
    MASSERT(pitch);

    RC rc;

    DmaWrapper *dmaWrapper = nullptr;
    CHECK_RC(GetDmaWrapper(&dmaWrapper));
    *pitch = dmaWrapper->GetPitchAlignRequirement();

    return rc;
}

//------------------------------------------------------------------------------
//! subdevNum Should be 0-indexed (subdevice instance)
RC GpuGoldenSurfaces::Cache
(
    int surfNum,
    Goldelwalues::BufferFetchHint bufFetchHint,
    UINT32 subdevNum
)
{
    RC rc = OK;
    MASSERT(surfNum < int(m_Surfaces.size()));

    if (UseDmaForCache(surfNum, bufFetchHint, subdevNum))
    {
        // Use MemoryToMemory to DMA the surface back to host memory and
        // do the blocklinear to pitch colwersion.
        CHECK_RC(SetupCache());

        UINT32 lines = m_Surfaces[surfNum].pSurf->GetHeight();
        UINT32 widthBytes = m_Surfaces[surfNum].pSurf->GetWidth() *
                ColorUtils::PixelBytes(m_Surfaces[surfNum].pSurf->GetColorFormat());

        UINT32 pitchAlign;
        CHECK_RC(GetPitchAlignRequirement(&pitchAlign));

        m_pCacheSurf->SetWidth(m_Surfaces[surfNum].pSurf->GetWidth());
        m_pCacheSurf->SetHeight(m_Surfaces[surfNum].pSurf->GetHeight());
        m_pCacheSurf->SetColorFormat(m_Surfaces[surfNum].pSurf->GetColorFormat());
        m_pCacheSurf->SetPitch(Utility::AlignUp(widthBytes, pitchAlign));

        DmaWrapper *dmaWrapper = nullptr;
        CHECK_RC(GetDmaWrapper(&dmaWrapper));
        dmaWrapper->SetSurfaces(m_Surfaces[surfNum].pSurf, m_pCacheSurf);
        rc = dmaWrapper->Copy(0, 0, widthBytes, lines, 0, 0, m_TimeoutMs, subdevNum);

        if (OK != rc)
        {
            Printf(Tee::PriHigh, "GpuGoldenSurfaces::Cache failed: %d\n",
                    UINT32(rc));
            return rc;
        }
        m_CachedAddress = m_pCacheSurf->GetAddress();
        m_CachedPitch   = m_pCacheSurf->GetPitch();
    }
    else
    {
        // Make sure the surface is mapped to the CPU's address space.
        m_CachedAddress = m_Surfaces[surfNum].pSurf->GetAddress();
        bool needUnmap = false;
        if (0 == m_CachedAddress)
        {
            CHECK_RC(m_Surfaces[surfNum].pSurf->Map(subdevNum));
            needUnmap = true;
            m_CachedAddress = m_Surfaces[surfNum].pSurf->GetAddress();
        }
        m_CachedPitch = m_Surfaces[surfNum].pSurf->GetPitch();

        // Construct a pitch image for Goldelwalues to examine.
        CHECK_RC(SetupPitchSurface());

        UINT08 * pSrc = (UINT08*)m_CachedAddress;
        UINT08 * pDst = (UINT08*)m_pPitchSurface;

        UINT32 w = m_Surfaces[surfNum].pSurf->GetWidth();
        UINT32 h = m_Surfaces[surfNum].pSurf->GetHeight();
        UINT32 bpp = ColorUtils::PixelBytes(m_Surfaces[surfNum].pSurf->GetColorFormat());

        if (Surface2D::BlockLinear == m_Surfaces[surfNum].pSurf->GetLayout())
        {
            BlockLinear bl(
                w,
                h,
                1, // depth
                m_Surfaces[surfNum].pSurf->GetLogBlockWidth(),
                m_Surfaces[surfNum].pSurf->GetLogBlockHeight(),
                0, // log2 block depth
                bpp,
                GetGpuDev());

            for (UINT32 y = 0; y < h; y++)
            {
                for (UINT32 x = 0; x < w; x++)
                {
                    UINT64 readOffset = bl.Address(x,y,0);
                    Platform::MemCopy(pDst, pSrc + readOffset, bpp);
                    pDst += bpp;
                }
            }
        }
        else
        {
            // In theory we don't have to do this, since the CPU could address
            // the FB memory directly, and we could give the mapped FB address
            // directly to Goldelwalues.
            // We're doing the copy anyway, to keep the byte-swapping code
            // below simpler.
            for (UINT32 y = 0; y < h; y++)
            {
                UINT64 readOffset = y * m_CachedPitch;
                Platform::MemCopy(pDst, pSrc + readOffset, bpp*w);
                pDst += bpp*w;
            }
        }
        m_CachedAddress = m_pPitchSurface;
        m_CachedPitch   = w * bpp;
        if (needUnmap)
            m_Surfaces[surfNum].pSurf->Unmap();
    }

    if (0 == m_CachedPitch)
    {
        // Some Surface2D types don't have a valid Pitch, fix this.
        m_CachedPitch = INT32( m_Surfaces[surfNum].pSurf->GetSize() /
                m_Surfaces[surfNum].pSurf->GetHeight() );
    }

    m_LwrCachedSurface = surfNum;
    return rc;
}

RC GpuGoldenSurfaces::GetDmaWrapper(DmaWrapper **dmaWrapper)
{
    RC rc;

    MASSERT(dmaWrapper);

    if (nullptr == m_pDmaWrapper)
    {
        // assuming that a Golden surface is associated with one device.
        if (!m_pTestConfig)
        {
            m_pTestConfig = new GpuTestConfiguration;
            m_pTestConfig->BindGpuDevice(m_pGpuDev);
            CHECK_RC(m_pTestConfig->InitFromJs());
        }
        m_pDmaWrapper = new DmaWrapper();
        CHECK_RC(m_pDmaWrapper->Initialize(m_pTestConfig, m_CacheSurfLocation));
    }

    *dmaWrapper = m_pDmaWrapper;

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Allocate a host-memory surface to cache the largest FB surface.
RC GpuGoldenSurfaces::SetupCache()
{
    RC rc;
    UINT32 maxSize = 0;
    UINT32 surfIdx;

    UINT32 pitchAlign;
    CHECK_RC(GetPitchAlignRequirement(&pitchAlign));

    for(surfIdx = 0; surfIdx < m_Surfaces.size(); surfIdx++)
    {
        Surface2D *inSurf = m_Surfaces[surfIdx].pSurf;
        if (Surface2D::Pitch == inSurf->GetLayout() && 0 != inSurf->GetPitch() % pitchAlign)
        {
            Printf(Tee::PriHigh,
                "ERROR: Golden implementation has requirements on alignment of the surface pitch.\n"
                "Pitch alignment requirement = %u\n"
                "Input surface (\"%s\") pitch = %u\n",
                pitchAlign,
                inSurf->GetName().c_str(),
                inSurf->GetPitch()
            );
            return RC::SOFTWARE_ERROR;
        }

        UINT64 size = inSurf->GetSize();
        UINT32 pixelBytes = ColorUtils::PixelBytes(inSurf->GetColorFormat());
        UINT64 dataSize =
            inSurf->GetHeight() * Utility::AlignUp(inSurf->GetWidth() * pixelBytes, pitchAlign);

        if (dataSize > 0 && dataSize < size)
            size = dataSize;

        if (size > maxSize)
        {
            if (size > 0xFFFFFFFF)
                return RC::DATA_TOO_LARGE;
            maxSize = (UINT32)size;
        }
    }
    if (m_pCacheSurf && m_pCacheSurf->GetSize() >= maxSize)
        return OK;

    if (m_pCacheSurf)
    {
        // Free old, too-small surface.
        m_pCacheSurf->Free();
        delete m_pCacheSurf;
    }

    m_pCacheSurf = new Surface2D();
    m_pCacheSurf->SetPitch(maxSize);
    m_pCacheSurf->SetHeight(1);
    m_pCacheSurf->SetColorFormat(ColorUtils::A8R8G8B8);
    m_pCacheSurf->SetWidth(maxSize/4);
    m_pCacheSurf->SetLocation(m_CacheSurfLocation);

    CHECK_RC(m_pCacheSurf->Alloc(GetGpuDev()));
    CHECK_RC(m_pCacheSurf->Map(0));

    if (0 == m_TimeoutMs)
    {
        // Get the global timeout parameter.
        GpuTestConfiguration tc;
        tc.BindGpuDevice(GetGpuDev());
        CHECK_RC(tc.InitFromJs());
        m_TimeoutMs = tc.TimeoutMs();
    }
    return OK;
}

//------------------------------------------------------------------------------
//! \brief Allocate memory for a pitch image of block-linear surfaces.
//!
//! Unlike the surface set up by SetupCache(), this one doesn't need to be
//! visible to the GPU.  It will only be read/written to by the CPU.
RC GpuGoldenSurfaces::SetupPitchSurface()
{
    UINT32 maxSize = 0;
    UINT32 surfIdx;
    for(surfIdx = 0; surfIdx < m_Surfaces.size(); surfIdx++)
    {
        UINT64 size = m_Surfaces[surfIdx].pSurf->GetSize();
        UINT64 dataSize = m_Surfaces[surfIdx].pSurf->GetHeight() *
                m_Surfaces[surfIdx].pSurf->GetWidth() *
                ColorUtils::PixelBytes(m_Surfaces[surfIdx].pSurf->GetColorFormat());

        if (dataSize > 0 && dataSize < size)
            size = dataSize;

        if (size > maxSize)
        {
            if (size > 0xFFFFFFFF)
                return RC::DATA_TOO_LARGE;
            maxSize = (UINT32)size;
        }
    }
    if (m_pPitchSurface && m_PitchSurfaceSize >= maxSize)
        return OK;

    if (m_pPitchSurface)
        delete [] m_pPitchSurface;

    m_pPitchSurface = new char [maxSize];
    m_PitchSurfaceSize = maxSize;

    return OK;
}

//------------------------------------------------------------------------------
//! \brief Verify the DMA of the most recently Cached surface.
//!
//! Do a byte-by-byte compare of the cached copy of the surface to the
//! original and report any errors that were due to the HW-accellerated
//! readback to the user.
//! Return OK if surfaces match, RC::MEM_TO_MEM_RESULT_NOT_MATCH otherwise.

RC GpuGoldenSurfaces::CheckAndReportDmaErrors(UINT32 subdevNum)
{
    MASSERT(m_LwrCachedSurface < (int) m_Surfaces.size());
    MASSERT(m_CachedAddress != NULL);

    RC rc;
    Surface2D * pGpuSurface = m_Surfaces[m_LwrCachedSurface].pSurf;
    UINT08 * gpuAddr = (UINT08*) pGpuSurface->GetAddress();
    bool needsUnmap = false;
    if (NULL == gpuAddr)
    {
        // Map the gpu surface into the CPU's address space for direct reads.
        needsUnmap = true;
        CHECK_RC( pGpuSurface->Map(subdevNum));
        gpuAddr = (UINT08*) pGpuSurface->GetAddress();
    }

    // A buffer big enough to hold the fattest float-128 pixel.
    UINT08 gpuPixelBuf[16];
    UINT32 bpp = pGpuSurface->GetBytesPerPixel();
    MASSERT(sizeof(gpuPixelBuf) >= bpp);

    // Very slowly confirm each pixel was dma'd correctly the first time.
    UINT32 copyErrors = 0;
    UINT32 w = pGpuSurface->GetWidth();
    UINT32 h = pGpuSurface->GetHeight();

    for (UINT32 y = 0; y < h; y++)
    {
        for (UINT32 x = 0; x < w; x++)
        {
            UINT08 * cachedPixel = (UINT08*)m_CachedAddress +
                    y*m_CachedPitch + x*bpp;

            UINT64 gpuPixelOffset = pGpuSurface->GetPixelOffset(x,y);
            Platform::MemCopy(gpuPixelBuf, gpuAddr + gpuPixelOffset, bpp);

            UINT32 b;
            bool miscompare = false;
            for (b = 0; b < bpp; b++)
            {
                if (cachedPixel[b] != gpuPixelBuf[b])
                    miscompare = true;
            }
            if (miscompare)
            {
                copyErrors++;
                if (1 == copyErrors)
                {
                    // Print header
                    Printf(Tee::PriHigh,
    "Found miscompares between DMA'd copy of surface and original!\n");
                    Printf(Tee::PriHigh,
                            "    offset,   x,   y, %*s, %*s\n",
                            bpp*3, "orig", bpp*3, "copy");
                }
                if ( copyErrors < 25)
                {
                    // Print error details
                    Printf(Tee::PriHigh,
                            "0x%08llx,%4d,%4d,",
                            gpuPixelOffset,
                            x,
                            y);
                    for (b = 0; b < bpp; b++)
                        Printf(Tee::PriHigh, " %02x", gpuPixelBuf[b]);
                    Printf(Tee::PriHigh, ",");
                    for (b = 0; b < bpp; b++)
                        Printf(Tee::PriHigh, " %02x", cachedPixel[b]);
                    Printf(Tee::PriHigh, "\n");
                }
                // Else that's enough spew.
            }
        }
    }
    if (copyErrors)
    {
        Printf(Tee::PriHigh,
        "Found %d miscomparing pixels between cached and original surface.\n",
                copyErrors);
    }

    if (needsUnmap)
        pGpuSurface->Unmap();

    if (copyErrors)
        return RC::MEM_TO_MEM_RESULT_NOT_MATCH;
    else
        return OK;
}

//------------------------------------------------------------------------------
//! \brief Return the address of cached data for this surface.
//!
//! This function may return a NULL pointer if caching is required and it fails.
void * GpuGoldenSurfaces::GetCachedAddress
(
    int surfNum,
    Goldelwalues::BufferFetchHint bufFetchHint,
    UINT32 subdevNum,
    vector<UINT08> *surfDumpBuffer
)
{
    RC rc;
    MASSERT(surfNum < int(m_Surfaces.size()));
    if (surfNum != m_LwrCachedSurface)
    {
        rc = Cache(surfNum, bufFetchHint, subdevNum);
        if (OK == rc && surfDumpBuffer)
        {
            UINT08 *pSurf = static_cast<UINT08*>(m_CachedAddress);
            UINT64 surfSize = abs(m_CachedPitch) * m_Surfaces[surfNum].pSurf->GetHeight();
            surfDumpBuffer->reserve(surfSize);
            surfDumpBuffer->assign(pSurf, pSurf + surfSize);
        }
    }

    if ( OK == rc)
        return m_CachedAddress;

    return NULL;
}

void GpuGoldenSurfaces::Ilwalidate()
{
    m_LwrCachedSurface = NO_CACHED_SURFACE;
}

//------------------------------------------------------------------------------
//! \brief Get the pitch of the surface.
//!
//! This is the pitch of the cached/colwerted copy, in the case that this
//! differs from the pitch of the original HW surface.
//! Pitch means offset in bytes from pixel (x, y) to (x, y+1).
//!
//! Note that Width * BPP will be <= Pitch.  (Pitch may be padded).
//!
//! Note that Pitch may be negative!
//! This oclwrs in Y-ilwerted (open-GL) surfaces where the earliest memory
//! address is pixel (x_min, y_max).
INT32 GpuGoldenSurfaces::Pitch(int surfNum) const
{
    MASSERT(surfNum < int(m_Surfaces.size()));
    MASSERT(surfNum == m_LwrCachedSurface);

    return m_CachedPitch;
}

//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//! \brief Get width of the surface in pixels (i.e. number of columns).
UINT32 GpuGoldenSurfaces::Width(int surfNum) const
{
    MASSERT(surfNum < int(m_Surfaces.size()));

    return m_Surfaces[surfNum].pSurf->GetWidth();
}

//------------------------------------------------------------------------------
//! \brief Get the height of the surface in pixels (i.e. number of rows).
UINT32 GpuGoldenSurfaces::Height(int surfNum) const
{
    MASSERT(surfNum < int(m_Surfaces.size()));

    return m_Surfaces[surfNum].pSurf->GetHeight();
}

//------------------------------------------------------------------------------
//! \brief Get the pixel format of the surface, an enum value.
//!
//! See the functions in ColorUtils for callwlating bytes-per-pixel from
//! Format.
ColorUtils::Format GpuGoldenSurfaces::Format(int surfNum) const
{
    MASSERT(surfNum < int(m_Surfaces.size()));

    return m_Surfaces[surfNum].pSurf->GetColorFormat();
}

//------------------------------------------------------------------------------
//! \brief Get the display mask for this "surface" for DAC CRCs.
//!
//! In theory a multi-display DAC CRC test could treat the different
//! displays as different surfaces, so that Goldelwalues can store
//! separate crc data for each, per loop.
UINT32 GpuGoldenSurfaces::Display(int surfNum) const
{
    MASSERT(surfNum < int(m_Surfaces.size()));

    return m_Surfaces[surfNum].Display;
}

//------------------------------------------------------------------------------
//! \brief Get the external CRC device for this "surface" for external CRCs.
//!
CrcGoldenInterface *GpuGoldenSurfaces::CrcGoldenInt(int surfNum) const
{
    MASSERT(surfNum < int(m_Surfaces.size()));

    return m_Surfaces[surfNum].pExtCrcDev;
}

//------------------------------------------------------------------------------
//! \brief Attach a Surface2D object.
//!
//! Also, check that the surface is set up correctly for use with GoldeValues.
int GpuGoldenSurfaces::AttachSurface
(
    Surface2D * pSurf,  //!< The surface to be managed.
    const char * name,  //!< The name of the surface, i.e. "C" or "Z".
    UINT32      display,//!< Display mask for this surface.
    CrcGoldenInterface *pExtCrcDev //!< External CRC device for the surface
)
{
    MASSERT(pSurf);
    MASSERT(name);
    MASSERT(pSurf->GetWidth());
    MASSERT(pSurf->GetHeight());

    Surf s = { pSurf, name, display, pExtCrcDev};
    m_Surfaces.push_back(s);

    return (int) m_Surfaces.size() - 1;
}

//------------------------------------------------------------------------------
//! \brief Clear all Surface2D objects.
//!
void GpuGoldenSurfaces::ClearSurfaces()
{
    m_Surfaces.clear();

    if (m_pCacheSurf)
    {
        m_pCacheSurf->Free();
        delete m_pCacheSurf;
        m_pCacheSurf = NULL;
    }
    if (m_pPitchSurface)
    {
        delete [] m_pPitchSurface;
        m_pPitchSurface = NULL;
    }
    if (m_pDmaWrapper)
    {
        delete m_pDmaWrapper;
        m_pDmaWrapper = NULL;
    }
}

//------------------------------------------------------------------------------
//! \brief Replace a Surfac2D object.
//!
//! Replaces the Surface2D pointer for the given index.
//! Useful for tests that render to different surfaces in different loops,
//! but only want to test one surface per call to Goldelwalues.
//! The Random2d test uses this interface.
void GpuGoldenSurfaces::ReplaceSurface
(
    int surfIndex,
    Surface2D * pSurf
)
{
    MASSERT(surfIndex < int(m_Surfaces.size()));
    MASSERT(pSurf);
    m_Surfaces[surfIndex].pSurf = pSurf;
}

//! Return the bound GPU device, asserting that one is indeed bound
GpuDevice *GpuGoldenSurfaces::GetGpuDev() const
{
    MASSERT(m_pGpuDev);
    return m_pGpuDev;
}

/* virtual */
RC GpuGoldenSurfaces::FetchAndCallwlate
(
    int surfNum,
    UINT32 subdevNum,
    Goldelwalues::BufferFetchHint bufFetchHint,
    UINT32 numCodeBins,
    Goldelwalues::Code calcMethod,
    UINT32 * pRtnCodeValues,
    vector<UINT08> *surfDumpBuffer
)
{
    RC rc;

    // We handle only one callwlation at a time, calcMethod should have
    // only one bit set.
    MASSERT(0 == (calcMethod & (calcMethod -1)));

    MASSERT(pRtnCodeValues);

    ::Display * pDisp = GetGpuDev()->GetDisplay();

    switch (calcMethod)
    {
        case Goldelwalues::DacCrc:
        {
            rc = pDisp->GetCrcValues(
                    Display(surfNum),
                    pRtnCodeValues + 0,
                    pRtnCodeValues + 1,
                    pRtnCodeValues + 2);
            break;
        }
        case Goldelwalues::TmdsCrc:
        {
            rc = pDisp->GetTmdsCrcValues(
                    Display(surfNum),
                    pRtnCodeValues);
            break;
        }
        case Goldelwalues::ExtCrc:
        {
            CrcGoldenInterface *pExtCrcDev = CrcGoldenInt(surfNum);
            rc = pExtCrcDev->GetCrcValues(pRtnCodeValues);
            break;
        }
        default:
        {
            rc = GoldenSurfaces::FetchAndCallwlate(
                    surfNum,
                    subdevNum,
                    bufFetchHint,
                    numCodeBins,
                    calcMethod,
                    pRtnCodeValues,
                    surfDumpBuffer);
            break;
        }
    }
    return rc;
}

/*virtual*/
bool GpuGoldenSurfaces::ReduceOptimization(bool reduce)
{
    m_ReduceOptimization = reduce;
    return true;
}
