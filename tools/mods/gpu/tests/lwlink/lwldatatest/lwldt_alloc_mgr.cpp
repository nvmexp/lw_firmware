/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2018,2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwldt_alloc_mgr.h"
#include "core/include/utility.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/tests/gputestc.h"
#include "gpu/utility/lwsurf.h"
#include "gpu/utility/surf2d.h"
#include "core/include/color.h"
#include "gpu/include/dmawrap.h"
#include "core/include/tee.h"
#include "core/include/lwrm.h"
#include "core/include/platform.h"

namespace
{
    const UINT32 MIN_2D_PITCH = 128;
};

using namespace LwLinkDataTestHelper;

//------------------------------------------------------------------------------
//! \brief Constructor
//!
AllocMgr::AllocMgr
(
    GpuTestConfiguration * pTestConfig,
    Tee::Priority pri,
    bool bFailRelease
)
 :  m_pTestConfig(pTestConfig)
   ,m_PrintPri(pri)
   ,m_ReleaseFailPri(pri)
   ,m_ReleaseFailRc(OK)
   ,m_bUseLwda(false)
{
    if (bFailRelease)
    {
        m_ReleaseFailPri = Tee::PriError;
        m_ReleaseFailRc  = RC::SOFTWARE_ERROR;
    }
}

//------------------------------------------------------------------------------
//! \brief Acquire a set of LWCA surface utilities on the GPU
//!
//! \param pGpuDev     : GPU device to acquire the lwca surface utilities on
//! \param ppLwdaUtils : Returned lwca surface utilities pointer
//!
//! \return OK if successful, not OK if acquisition failed
RC AllocMgr::AcquireLwdaUtils
(
    GpuDevice      *pGpuDev,
    LwSurfRoutines **ppLwdaUtils
)
{
    RC rc;
    if (!m_LwdaSurfaceUtils.count(pGpuDev))
    {
        LwdaSurfUtilData lwdaUtilData;
        lwdaUtilData.pLwSurfUtils = make_shared<LwSurfRoutines>();
        CHECK_RC(lwdaUtilData.pLwSurfUtils->Initialize(pGpuDev));
        m_LwdaSurfaceUtils[pGpuDev] = lwdaUtilData;
    }

    *ppLwdaUtils = m_LwdaSurfaceUtils[pGpuDev].pLwSurfUtils.get();
    m_LwdaSurfaceUtils[pGpuDev].lwSurfUtilsUseCount++;
    return OK;

}

//------------------------------------------------------------------------------
//! \brief Acquire a surface on the specified GPU device
//!
//! \param pGpuDev       : GPU device to acquire the surface on
//! \param loc           : Location of the surface
//! \param layout        : Layout of the surface
//! \param surfaceWidth  : Width of the surface (pixels)
//! \param surfaceHeight : Height of the surface (lines)
//! \param pSurf         : Pointer to returned surface pointer
//!
//! \return OK if successful, not OK if acquisition failed
RC AllocMgr::AcquireSurface
(
     GpuDevice *pGpuDev
    ,Memory::Location loc
    ,Surface2D::Layout layout
    ,UINT32 surfaceWidth
    ,UINT32 surfaceHeight
    ,Surface2DPtr *pSurf
)
{
    // First try to find an unused surface in the correct location
    if (m_Surfaces.find(pGpuDev) != m_Surfaces.end())
    {
        for (size_t ii = 0; ii < m_Surfaces[pGpuDev].size(); ii++)
        {
            if (!m_Surfaces[pGpuDev][ii].bAcquired &&
                (m_Surfaces[pGpuDev][ii].pSurf->GetWidth() == surfaceWidth) &&
                (m_Surfaces[pGpuDev][ii].pSurf->GetHeight() == surfaceHeight) &&
                (m_Surfaces[pGpuDev][ii].pSurf->GetLocation() == loc) &&
                (m_Surfaces[pGpuDev][ii].pSurf->GetLayout() == layout))
            {
                *pSurf = m_Surfaces[pGpuDev][ii].pSurf;
                m_Surfaces[pGpuDev][ii].bAcquired = true;
                Printf(m_PrintPri,
                       "%s : Acquired surface for GPU(%u, 0) at offset 0x%llx\n",
                       __FUNCTION__,
                       pGpuDev->GetDeviceInst(),
                       m_Surfaces[pGpuDev][ii].pSurf->GetCtxDmaOffsetGpu());
                return OK;
            }
        }
    }
    else
    {
        vector<SurfaceAlloc> emptyAlloc;
        m_Surfaces[pGpuDev] = emptyAlloc;
    }

    RC rc;
    SurfaceAlloc newAlloc;
    newAlloc.bAcquired = true;
    newAlloc.pSurf.reset(new Surface2D);
    CHECK_RC(AllocSurface(newAlloc.pSurf, loc, layout, surfaceWidth, surfaceHeight, pGpuDev));
    m_Surfaces[pGpuDev].push_back(newAlloc);
    *pSurf = newAlloc.pSurf;

    Printf(m_PrintPri,
           "%s : Created surface for GPU(%u, 0) at offset 0x%llx\n",
           __FUNCTION__,
           pGpuDev->GetDeviceInst(),
           newAlloc.pSurf->GetCtxDmaOffsetGpu());
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Fill a surface with the specified pattern.  The surfaces are quite
//! large (>50Mb), so filling the memory using CPU is going to be slow. Attempt
//! to do this faster using twod engine.  First use the CPU to write a 1/4
//! surface base height version of the pattern and then copy the pattern to the
//! rest of the surface using the twod engine
//!
//! \param pSurf       : Surface to fill
//! \param pt          : Pattern type to fill with
//! \param patternData : Additional data associated with the pattern
//!
//! \return OK if successful, not OK if fill failed failed
RC AllocMgr::FillSurface
(
    Surface2DPtr pSurf
   ,PatternType pt
   ,UINT32 patternData
   ,UINT32 patternData2
   ,bool bTwoDFill
)
{
    RC rc;

    if (m_bUseLwda && ((pt == PT_SOLID) || (pt == PT_RANDOM)))
    {
        LwSurfRoutines *pLwSurfRoutines = nullptr;
        DEFER()
        {
            ReleaseLwdaUtils(&pLwSurfRoutines);
        };

        CHECK_RC(AcquireLwdaUtils(pSurf->GetGpuDev(), &pLwSurfRoutines));
        if (pLwSurfRoutines->IsSupported())
        {
            if (pt == PT_SOLID)
            {
                // UINT64 is slightly faster
                UINT64 u64data = (static_cast<UINT64>(patternData) << 32) | patternData;
                CHECK_RC(pLwSurfRoutines->FillConstant<UINT64>(*pSurf.get(), u64data));
            }
            else
            {
                CHECK_RC(pLwSurfRoutines->FillRandom<UINT64>(*pSurf.get(), patternData));
            }
            return rc;
        }
    }

    Surface2DPtr pPatSurf = pSurf;
    UINT32 patternFillHeight = m_pTestConfig->SurfaceHeight() / 4;
    if (patternFillHeight > pSurf->GetHeight())
        patternFillHeight = pSurf->GetHeight();

    const bool isTegra = pSurf->GetGpuSubdev(0)->IsSOC();

    // Avoid reflected writes by creating a pitch linear version of the surface
    // where the pattern is written with CPU writes
    if (((pSurf->GetLayout() == Surface2D::BlockLinear) && (pSurf->GetLocation() != Memory::Fb))
        // Also avoid mmaping a huge surface on CheetAh
        || isTegra)
    {
        Surface2DPtr pTempSurf(new Surface2D);
        pTempSurf->SetWidth(pPatSurf->GetWidth());
        pTempSurf->SetHeight(patternFillHeight);
        pTempSurf->SetLocation(pPatSurf->GetLocation());
        pTempSurf->SetColorFormat(pPatSurf->GetColorFormat());
        pTempSurf->SetProtect(Memory::ReadWrite);
        pTempSurf->SetLayout(Surface2D::Pitch);
        if (isTegra)
        {
            pTempSurf->SetVASpace(Surface2D::TegraVASpace);
        }
        CHECK_RC(pTempSurf->Alloc(pPatSurf->GetGpuDev()));

        pPatSurf = pTempSurf;
    }

    {
        Tasker::DetachThread detachThread;
        switch (pt)
        {
            case PT_BARS:
                CHECK_RC(pPatSurf->FillPatternRect(0,
                                                   0,
                                                   pSurf->GetWidth(),
                                                   patternFillHeight,
                                                   1,
                                                   1,
                                                   "bars",
                                                   "100",
                                                   "horizontal"));
                break;
            case PT_LINES:
                for (UINT32 lwrY = 0; lwrY < patternFillHeight; lwrY+=8)
                {
                    CHECK_RC(pPatSurf->FillPatternRect(0,
                                                       lwrY,
                                                       pSurf->GetWidth(),
                                                       8,
                                                       1,
                                                       1,
                                                       "bars",
                                                       "100",
                                                       "vertical"));
                }
                break;
            case PT_RANDOM:
                {
                    string seed = Utility::StrPrintf("seed=%u", patternData);
                    CHECK_RC(pPatSurf->FillPatternRect(0,
                                                       0,
                                                       pSurf->GetWidth(),
                                                       patternFillHeight,
                                                       1,
                                                       1,
                                                       "random",
                                                       seed.c_str(),
                                                       nullptr));
                }
                break;
            case PT_SOLID:
                CHECK_RC(pPatSurf->FillRect(0,
                                            0,
                                            pSurf->GetWidth(),
                                            patternFillHeight,
                                            patternData));
                break;
            case PT_ALTERNATE:
            {
                bool patternSelect = false;
                for (UINT32 lwrY = 0; lwrY < patternFillHeight; lwrY++)
                {
                    for (UINT32 lwrX = 0; lwrX < pSurf->GetWidth(); lwrX += 4)
                    {
                        patternSelect = !patternSelect;
                        CHECK_RC(pPatSurf->FillRect(lwrX,
                                                    lwrY,
                                                    4,
                                                    1,
                                                    (patternSelect) ? patternData : patternData2));
                    }
                }
                break;
            }
            default:
                Printf(Tee::PriError, "%s : Unknown pattern type %d\n",
                       __FUNCTION__, pt);
                return RC::SOFTWARE_ERROR;
        }
    }
    
    if (GetUseDmaInit())
    {
        DmaWrapper dmaWrap;

        // Temporarily rebind the GPU so that DmaWrapper uses the correct device
        GpuDevice *pOrigGpuDev = m_pTestConfig->GetGpuDevice();
        Utility::CleanupOnReturnArg<GpuTestConfiguration,
                                    void,
                                    GpuDevice *>
            restoreGpuDev(m_pTestConfig,
                        &GpuTestConfiguration::BindGpuDevice,
                        pOrigGpuDev);
        m_pTestConfig->BindGpuDevice(pSurf->GetGpuDev());
    
        // Use the specified copy method to ensure there are no conflicts
        const DmaWrapper::PREF_TRANS_METH transMethod =
                isTegra   ? DmaWrapper::VIC  :
                bTwoDFill ? DmaWrapper::TWOD :
                            DmaWrapper::COPY_ENGINE;
        CHECK_RC(dmaWrap.Initialize(m_pTestConfig, Memory::NonCoherent, transMethod));
    
        Utility::CleanupOnReturn<DmaWrapper, RC>
            dmaCleanup(&dmaWrap, &DmaWrapper::Cleanup);
    
        // Copy the pattern to the surface to be filled if the pattern surface is
        // different from the surface to be filled (used to avoid reflected writes)
        if (pPatSurf != pSurf)
        {
            dmaWrap.SetSurfaces(pPatSurf.get(), pSurf.get());
            CHECK_RC(dmaWrap.Copy(0,                        // Starting src X, in bytes not pixels.
                                0,                        // Starting src Y, in lines.
                                pPatSurf->GetPitch(),     // Width of copied rect, in bytes.
                                patternFillHeight,        // Height of copied rect, in bytes
                                0,                        // Starting dst X, in bytes not pixels.
                                0,                        // Starting dst Y, in lines.
                                m_pTestConfig->TimeoutMs(),
                                0));
        }
    
        dmaWrap.SetSurfaces(pSurf.get(), pSurf.get());
        for (UINT32 y = 0;
            y < (pSurf->GetHeight() - patternFillHeight);
            y += patternFillHeight)
        {
            CHECK_RC(dmaWrap.Copy(0,                        // Starting src X, in bytes not pixels.
                                y,                        // Starting src Y, in lines.
                                pSurf->GetPitch(),        // Width of copied rect, in bytes.
                                patternFillHeight,        // Height of copied rect, in bytes
                                0,                        // Starting dst X, in bytes not pixels.
                                y+patternFillHeight,      // Starting dst Y, in lines.
                                m_pTestConfig->TimeoutMs(),
                                0));
        }
    }
    else // !UseDmaInit
    {
        CHECK_RC(pPatSurf->Map());
        if (pPatSurf != pSurf)
        {
            CHECK_RC(pSurf->Map());
        }
        
        const UINT32 surfWidth = pSurf->GetWidth();
        vector<UINT32> patLine(surfWidth);
        vector<UINT32> surfLine(surfWidth);
        
        for (UINT32 y = 0;
            y < (pSurf->GetHeight() - patternFillHeight);
            ++y)
        {
            const UINT64 patStartOffset = pPatSurf->GetPixelOffset(0, y);
            const UINT64 surfStartOffset = pSurf->GetPixelOffset(0, y + patternFillHeight);

            // Copy the surface data line by line
            Platform::VirtualRd(static_cast<UINT08*>(pPatSurf->GetAddress()) + patStartOffset,
                                &patLine[0],
                                surfWidth*sizeof(UINT32));
            Platform::VirtualWr(static_cast<UINT08*>(pSurf->GetAddress()) + surfStartOffset,
                                &surfLine[0],
                                surfWidth*sizeof(UINT32));
        }
        
        pPatSurf->Unmap();
        if (pPatSurf != pSurf)
            pSurf->Unmap();
    }

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Release a lwca surface utilities
//!
//! \param ppLwdaUtils : Pointer to a lwca surface utilities pointer to release
//!
//! \return OK if successful, not OK if release failed
RC AllocMgr::ReleaseLwdaUtils(LwSurfRoutines **ppLwdaUtils)
{
    GpuDevice *pGpuDev = (*ppLwdaUtils)->GetGpuDevice();

    MASSERT(m_LwdaSurfaceUtils[pGpuDev].pLwSurfUtils.get() == *ppLwdaUtils);
    MASSERT(m_LwdaSurfaceUtils[pGpuDev].lwSurfUtilsUseCount);
    m_LwdaSurfaceUtils[pGpuDev].lwSurfUtilsUseCount--;
    *ppLwdaUtils = nullptr;
    return OK;
}

//------------------------------------------------------------------------------
//! \brief Release a surface
//!
//! \param pSurf   : Pointer to surface for release
//!
//! \return OK if successful, not OK if release failed
RC AllocMgr::ReleaseSurface(Surface2DPtr pSurf)
{
    GpuDevice *pGpuDev = pSurf->GetGpuDev();
    if (m_Surfaces.find(pGpuDev) == m_Surfaces.end())
    {
        Printf(m_ReleaseFailPri, "%s : No surfaces allocated on Gpu(%u, 0)\n",
               __FUNCTION__, pGpuDev->GetDeviceInst());
        return m_ReleaseFailRc;
    }

    for (size_t ii = 0; ii < m_Surfaces[pGpuDev].size(); ii++)
    {
        if (m_Surfaces[pGpuDev][ii].pSurf == pSurf)
        {
            m_Surfaces[pGpuDev][ii].bAcquired = false;
            Printf(m_PrintPri,
                   "%s : Released surface for GPU(%u, 0) at offset 0x%llx\n",
                   __FUNCTION__,
                   pGpuDev->GetDeviceInst(),
                   m_Surfaces[pGpuDev][ii].pSurf->GetCtxDmaOffsetGpu());
            return OK;
        }
    }
    Printf(m_ReleaseFailPri, "%s : Surface not found on Gpu(%u, 0)\n",
           __FUNCTION__, pGpuDev->GetDeviceInst());
    return m_ReleaseFailRc;
}

//------------------------------------------------------------------------------
//! \brief Shutdown the allocation manager
//!
//! \return OK if successful, not OK if shutdown failed
RC AllocMgr::Shutdown()
{
    StickyRC rc;

    map<GpuDevice *, vector<SurfaceAlloc> >::iterator pGpuSurfs = m_Surfaces.begin();
    for (; pGpuSurfs != m_Surfaces.end(); pGpuSurfs++)
    {
        for (size_t ii = 0; ii < pGpuSurfs->second.size(); ii++)
        {
            if (!pGpuSurfs->second[ii].pSurf.unique() ||
                pGpuSurfs->second[ii].bAcquired)
            {
                Printf(Tee::PriError,
                       "Multiple references to a surface on GpuDevice(%u) "
                       "during Cleanup\n",
                       pGpuSurfs->first->GetDeviceInst());
                rc = RC::SOFTWARE_ERROR;
            }
            pGpuSurfs->second[ii].pSurf->Free();
        }
    }
    m_Surfaces.clear();

    for (auto & lwdaSurfUtils : m_LwdaSurfaceUtils)
    {
        if (lwdaSurfUtils.second.pLwSurfUtils)
        {
            if (lwdaSurfUtils.second.lwSurfUtilsUseCount)
            {
                Printf(Tee::PriError,
                       "Multiple references (%u) to lwca surface utilities on GpuDevice(%u) "
                       "during Cleanup\n",
                       lwdaSurfUtils.second.lwSurfUtilsUseCount,
                       lwdaSurfUtils.first->GetDeviceInst());
                rc = RC::SOFTWARE_ERROR;
            }
            lwdaSurfUtils.second.pLwSurfUtils->Free();
        }
    }
    m_LwdaSurfaceUtils.clear();

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Allocate a test surface
//!
//! \param pSurf         : Pointer to surface to allocate
//! \param loc           : Location of surface to allocate
//! \param layout        : Layout of the surface to allocate
//! \param surfaceWidth  : Width of the surface (pixels)
//! \param surfaceHeight : Height of the surface (lines)
//! \param pGpuDev       : Pointer to GPU device to allocate the surface on
//!
//! \return OK if successful, not OK if allocation failed
RC AllocMgr::AllocSurface
(
    Surface2DPtr pSurf,
    Memory::Location loc,
    Surface2D::Layout layout,
    UINT32 surfaceWidth,
    UINT32 surfaceHeight,
    GpuDevice *pGpuDev
)
{
    RC rc;

    pSurf->SetWidth(surfaceWidth);
    pSurf->SetHeight(surfaceHeight);
    pSurf->SetLocation(loc);
    pSurf->SetColorFormat(ColorUtils::A8R8G8B8);
    pSurf->SetProtect(Memory::ReadWrite);
    pSurf->SetLayout(layout);
    pSurf->SetLogBlockWidth(0);
    pSurf->SetLogBlockHeight(0);
    pSurf->SetP2PGpuCacheMode(Surface2D::GpuCacheOff);
    pSurf->SetGpuCacheMode(Surface2D::GpuCacheOff);
    if ((layout == Surface2D::Pitch) &&
        ((surfaceWidth * pSurf->GetBytesPerPixel()) < MIN_2D_PITCH))
    {
        pSurf->SetPitch(MIN_2D_PITCH);
    }

    CHECK_RC(pSurf->Alloc(pGpuDev));
    return rc;
}
