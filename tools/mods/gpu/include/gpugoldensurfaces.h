/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2011, 2016 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! \file gpugoldensurfaces.h
//! \brief Declares the GpuGoldenSurfaces class.

#ifndef INCLUDED_GPUGOLDENSURFACES_H
#define INCLUDED_GPUGOLDENSURFACES_H

#ifndef INCLUDED_GOLDEN_H
#include "core/include/golden.h"
#endif
#ifndef INCLUDED_LWRM_H
#include "core/include/lwrm.h"
#endif

class Surface2D;
class DmaWrapper;
class Channel;
class GpuDevice;
class GpuTestConfiguration;
class CrcGoldenInterface;

//------------------------------------------------------------------------------
//! /brief Gpu-specific derived class of GoldenSurfaces.
//!
//! This class is implemented using Surface2D objects.
//! It is the right surfaces adaptor for most 2d and video manufacturing tests.
//!
//! Non-Gpu versions of mods (i.e. WMP) should use WmpGoldenSurfaces.
//! OpenGL tests should use GlGpuGoldenSurfaces.
//!
class GpuGoldenSurfaces : public GoldenSurfaces
{
public:
    //! /brief Constructor allows specifying device for DMA
    GpuGoldenSurfaces(GpuDevice *gpudev);
    virtual ~GpuGoldenSurfaces();

    //! \brief Attach a Surface2D object.
    //!
    //! Surfaces are appended to the list of surfaces.
    //! \return the assigned surface index.
    int AttachSurface
    (
        Surface2D * pSurf,  //!< The surface to be managed.
        const char * name,  //!< The name of the surface, i.e. "C" or "Z".
        UINT32      display,//!< Display mask for this surface.
        CrcGoldenInterface *pExtCrcDev = nullptr //!< External CRC device for the surface
    );

    //! \brief Clear all the surface2D objects.
    //!
    void ClearSurfaces();

    //! \brief Replace a Surfac2D object.
    //!
    //! Replaces the Surface2D pointer for the given index.
    //! Useful for tests that render to different surfaces in different loops,
    //! but only want to test one surface per call to Goldelwalues.
    //! The Random2d test uses this interface.
    void ReplaceSurface
    (
        int surfIndex,
        Surface2D * pSurf
    );

    // Implement GoldenSurfaces interfaces:
    virtual int NumSurfaces() const;
    virtual const string & Name (int surfNum) const;
    //! subdevNum is 0-indexed (subdevice instance)
    virtual RC CheckAndReportDmaErrors(UINT32 subdevNum);
    virtual void * GetCachedAddress(int surfNum, Goldelwalues::BufferFetchHint bufFetchHint, UINT32 subdevNum, vector<UINT08> *surfDumpBuffer);
    virtual void Ilwalidate();
    virtual INT32 Pitch(int surfNum) const;
    virtual UINT32 Width(int surfNum) const;
    virtual UINT32 Height(int surfNum) const;
    virtual ColorUtils::Format Format(int surfNum) const;
    virtual UINT32 Display(int surfNum) const;
    virtual CrcGoldenInterface *CrcGoldenInt(int surfNum) const;

    GpuDevice *GetGpuDev() const;

    virtual RC FetchAndCallwlate
    (
        int surfNum,
        UINT32 subdevNum,
        Goldelwalues::BufferFetchHint bufFetchHint,
        UINT32 numCodeBins,
        Goldelwalues::Code calcMethod,
        UINT32 * pRtnCodeValues,
        vector<UINT08> *surfDumpBuffer
    );

    virtual bool ReduceOptimization(bool reduce);

    virtual RC GetPitchAlignRequirement(UINT32 *pitch);

protected:

private:
    //! \brief Info about one of our surfaces.
    struct Surf
    {
        Surface2D *  pSurf;
        string       Name;
        UINT32       Display;
        CrcGoldenInterface *pExtCrcDev;
    };

    //! Array of all surfaces.
    vector<Surf> m_Surfaces;

    //! Which surf is cached now (index into m_Surfaces).
    int          m_LwrCachedSurface;

    //! Pointer to m_pCacheSurf or m_pPitchSurface or to real HW surf, depending.
    void *       m_CachedAddress;

    //! Pitch that corresponds to whatever m_CachedAddress points to.
    INT32        m_CachedPitch;

    //! Coherent host memory copy of surface (GPU writable, has ctxdma).
    Surface2D *  m_pCacheSurf;
    static const Memory::Location m_CacheSurfLocation;

    //! DmaWrapper used to copy surface2D in Cache()
    DmaWrapper* m_pDmaWrapper;
    GpuTestConfiguration * m_pTestConfig; // Used for DmaWrapper

    //! Coherent host memory (no ctxdma, only CPU addressable), used for
    //! BlockLinear to PitchLinear colwersions of m_pCacheSurf.
    char *       m_pPitchSurface;

    //! Keep track of the size of m_pPitchSurface, in case we might have to
    //! cache a larger surface later, after ReplaceSurface is called on us.
    UINT32       m_PitchSurfaceSize;

    //! Timeout for Dma operations.
    FLOAT64      m_TimeoutMs;

    //! Flag for whether we should reduce test optimization (disable DMA)
    bool         m_ReduceOptimization;

    RC GetDmaWrapper(DmaWrapper **dmaWrapper);
    RC SetupCache();
    RC SetupPitchSurface();
    //! subdevNum is 0-indexed (subdevice instance)
    RC Cache (int surfNum, Goldelwalues::BufferFetchHint bufFetchHint, UINT32 subdevNum);
    bool UseDmaForCache(int surfNum, Goldelwalues::BufferFetchHint bufFetchHint, UINT32 subdevNum);

    GpuDevice *m_pGpuDev;
};

#endif // INCLUDED_GPUGOLDENSURFACES_H
