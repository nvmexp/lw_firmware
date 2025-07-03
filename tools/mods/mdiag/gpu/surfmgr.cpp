/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "mdiag/lwgpu.h"
#include "surfmgr.h"
#include "gpu/utility/blocklin.h"
#include "mdiag/tests/gpu/lwgpu_subchannel.h"
#include "mdiag/utils/perf_mon.h"
#include "gpu/utility/gpuutils.h"
#include "gpu/include/floorsweepimpl.h"
#include "gpu/utility/surfrdwr.h"
#include "core/include/chiplibtracecapture.h"
#include "mdiag/utils/buffinfo.h"
#include "zlwll.h"
#include "mdiag/utils/buf.h"
#include "mdiag/utils/surfutil.h"
#include "class/cl9097.h"
#include "mdiag/resource/lwgpu/dmaloader.h"
#include "mdiag/resource/lwgpu/verif/GpuVerif.h"
#include "mdiag/tests/gpu/trace_3d/trace_3d.h"
#include "mdiag/smc/smcengine.h"
#include "mdiag/utl/utl.h"

#include <numeric>

#define COLOR_ARGUMENT_ARRAY(arrayName, argStr) \
static const char *arrayName[MAX_RENDER_TARGETS] = \
{ \
    argStr "CA", \
    argStr "CB", \
    argStr "CC", \
    argStr "CD", \
    argStr "CE", \
    argStr "CF", \
    argStr "CG", \
    argStr "CH", \
}

COLOR_ARGUMENT_ARRAY(ColorLocArgNames, "-loc");
COLOR_ARGUMENT_ARRAY(ColorSplitLocArgNames, "-split_loc");
COLOR_ARGUMENT_ARRAY(ColorSplitArgNames, "-split");
COLOR_ARGUMENT_ARRAY(PageSizeArgNames, "-page_size");
COLOR_ARGUMENT_ARRAY(CtxDmaTypeArgNames, "-ctx_dma_type");
COLOR_ARGUMENT_ARRAY(CtxDmaAttrsArgNames, "-ctxdma_attrs");
COLOR_ARGUMENT_ARRAY(BlockWidthArgNames, "-block_width");
COLOR_ARGUMENT_ARRAY(BlockHeightArgNames, "-block_height");
COLOR_ARGUMENT_ARRAY(BlockDepthArgNames, "-block_depth");
COLOR_ARGUMENT_ARRAY(WidthArgNames, "-width");
COLOR_ARGUMENT_ARRAY(HeightArgNames, "-height");
COLOR_ARGUMENT_ARRAY(FormatCArgNames, "-format");
COLOR_ARGUMENT_ARRAY(ColorCompressArgNames, "-compress");
COLOR_ARGUMENT_ARRAY(ColorZbcModeArgNames, "-zbc_mode");
COLOR_ARGUMENT_ARRAY(ColorGpuCacheModeArgNames, "-gpu_cache_mode");
COLOR_ARGUMENT_ARRAY(ColorP2PGpuCacheModeArgNames, "-gpu_p2p_cache_mode");
COLOR_ARGUMENT_ARRAY(ColorSplitGpuCacheModeArgNames, "-split_gpu_cache_mode");
COLOR_ARGUMENT_ARRAY(ComptagStartArgNames, "-comptag_start");
COLOR_ARGUMENT_ARRAY(ComptagCovgArgNames, "-comptag_covg");
COLOR_ARGUMENT_ARRAY(PteKindArgNames, "-pte_kind");
COLOR_ARGUMENT_ARRAY(SplitPteKindArgNames, "-split_pte_kind");
COLOR_ARGUMENT_ARRAY(PartStrideArgNames, "-part_stride");
COLOR_ARGUMENT_ARRAY(PitchArgNames, "-pitch");
COLOR_ARGUMENT_ARRAY(ArrayPitchArgNames, "-array_pitch");
COLOR_ARGUMENT_ARRAY(VaReverseArgNames, "-va_reverse");
COLOR_ARGUMENT_ARRAY(PaReverseArgNames, "-pa_reverse");
COLOR_ARGUMENT_ARRAY(MaxCoalesceArgNames, "-max_coalesce");
COLOR_ARGUMENT_ARRAY(PhysAlignArgNames, "-phys_align");
COLOR_ARGUMENT_ARRAY(OffsetArgNames, "-offset");
COLOR_ARGUMENT_ARRAY(AdjustArgNames, "-adjust");
COLOR_ARGUMENT_ARRAY(LimitArgNames, "-limit");
COLOR_ARGUMENT_ARRAY(VirtAddrHintArgNames, "-virt_addr_hint");
COLOR_ARGUMENT_ARRAY(PhysAddrHintArgNames, "-phys_addr_hint");
COLOR_ARGUMENT_ARRAY(PhysAddrRangeArgNames, "-phys_addr_range");
COLOR_ARGUMENT_ARRAY(ComptagOffsetArgNames, "-comptag_offset");
COLOR_ARGUMENT_ARRAY(MemoryMappingArgNames, "-map_mode");
COLOR_ARGUMENT_ARRAY(MapRegionArgNames, "-map_region");
COLOR_ARGUMENT_ARRAY(VprArgNames, "-vpr");
COLOR_ARGUMENT_ARRAY(Acr1ArgNames, "-acr1");
COLOR_ARGUMENT_ARRAY(Acr2ArgNames, "-acr2");

#define MAX_RENDER_TARGET_WIDTH 0xffff
#define MAX_RENDER_TARGET_HEIGHT 0xffff


LWGpuSurfaceMgr::LWGpuSurfaceMgr(Trace3DTest* pTest, LWGpuResource *in_lwgpu, LWGpuSubChannel *in_sch, const ArgReader *params)
: m_LastIdTag(1),
  m_Params(params)
{
    m_pTest = pTest;
    lwgpu = in_lwgpu;
    ch = in_sch? in_sch->channel() : 0;
    m_subch = in_sch;
    refCount = 1;

    userSpecifiedWindowOffset = false;
    m_UnscaledWOffsetX = 0;
    m_UnscaledWOffsetY = 0;

    m_TargetDisplayWidth = 0;
    m_TargetDisplayHeight = 0;

    m_DefaultWidth = 640;
    m_DefaultHeight = 480;
    m_DefaultDepth = 1;
    m_DefaultArraySize = 1;

    m_MultiSampleOverride = false;
    m_MultiSampleOverrideValue = 0;
    m_NonMultisampledZOverride = false;
    m_AAUsed = false;

    m_ZlwllEnabled = m_SlwllEnabled = false;
    m_ZlwllMode = ZLWLL_REQUIRED;
    m_ComprMode = COMPR_REQUIRED;

    m_FastClearColor = false;
    m_FastClearDepth = false;
    m_SrgbWrite = false;

    m_surfaceLwr = m_RenderSurfaces.end();

    ForceVisible = false;
    m_DoZlwllSanityCheck = false;

    m_HasClearInit = false;
    m_IsSliSurfClip = false;
    m_ActiveColorSurfaces = 0;
    m_AllColorsFixed = false;
    m_NeedClipID = true;

    m_PoolOfSmallPages = 0;
    m_PoolOfBigPages = 0;
    m_SmallPagePoolSize = 0;
    m_BigPagePoolSize = 0;
    m_BigPageSize = 0;

    m_colorBuffConfigs = CreateColorMap();
    m_zetaBuffConfigs = CreateZetaMap();

    m_ProgZtAsCt0 = false;
    m_SendRTMethods = false;
}

LWGpuSurfaceMgr::~LWGpuSurfaceMgr()
{
    if (m_PoolOfSmallPages)
    {
        m_PoolOfSmallPages->Free();
        delete m_PoolOfSmallPages;
    }

    if (m_PoolOfBigPages)
    {
        m_PoolOfBigPages->Free();
        delete m_PoolOfBigPages;
    }

    SurfaceIterator surfaceIter;

    for (surfaceIter = m_RenderSurfaces.begin();
         surfaceIter != m_RenderSurfaces.end();
         ++surfaceIter)
    {
        SurfaceData *data = &(surfaceIter->second);
        vector<PartialMapData *>::iterator mapIter;

        for (mapIter = data->partialMaps.begin();
             mapIter != data->partialMaps.end();
             ++mapIter)
        {
            PartialMapData *Mapdata = (*mapIter);
            SurfaceMapDatas surfaceData = Mapdata->surfaceMapDatas;
            for(SurfaceMapDatas::iterator siter = surfaceData.begin();siter != surfaceData.end();++siter)
            {
                (*siter)->map.Free();
                (*siter)->physMem.Free();
            }
            delete *mapIter;
        }

        data->partialMaps.clear();
    }

    DeleteMap(&m_colorBuffConfigs);
    DeleteMap(&m_zetaBuffConfigs);
}

RC LWGpuSurfaceMgr::WriteSurface(const ArgReader *params, GpuVerif* gpuVerif, MdiagSurf& surf, 
            UINT08* data, size_t dataSize, UINT32 subdev, bool useCE)
{
    RC rc;

    if (useCE)
    {
        CHECK_RC(surf.DownloadWithCE(params, gpuVerif, data, dataSize, 0, true, subdev));
    }
    else
    {
        auto pWriter = SurfaceUtils::CreateMappingSurfaceWriter(&surf);
        pWriter->SetTargetSubdevice(subdev);
        for (UINT64 offset = 0; offset < surf.GetSize(); offset += dataSize)
       {
            const size_t copySize = min(dataSize,
                    static_cast<size_t>(surf.GetSize() - offset));
            CHECK_RC(pWriter->WriteBytes(offset, data, copySize, Surface2D::MapDefault));
       }
    }

    return OK;
}

RC LWGpuSurfaceMgr::ZeroSurface(const ArgReader *params, GpuVerif* gpuVerif, MdiagSurf& surf,
            UINT32 subdev, bool useCE)
{
    RC rc;

    if (useCE)
    {
        bool getPteKind = false;
        UINT32 fillPteKind;
        GpuSubdevice *pSubdev = lwgpu->GetGpuSubdevice(subdev);
        if (pSubdev->HasFeature(Device::GPUSUB_SUPPORTS_GENERIC_MEMORY))
        {
            getPteKind = pSubdev->GetPteKindFromName("GENERIC_MEMORY", &fillPteKind);
        }
        else
        {
            getPteKind = pSubdev->GetPteKindFromName("GENERIC_16BX2", &fillPteKind);
        }
        if (!getPteKind)
        {
            ErrPrintf("Neither GENERIC_MEMORY nor GENERIC_16BX2 "
                    "PTE kinds exist for this device.\n");
            return RC::SOFTWARE_ERROR;
        }
        
        SurfacePteModifier surfacePteModifier(&surf, fillPteKind);
        Utility::CleanupOnReturn<SurfacePteModifier, RC>
                restoreSurfacePte(&surfacePteModifier, &SurfacePteModifier::Restore);
        CHECK_RC(surfacePteModifier.ChangeTo());

        SurfaceProtAttrModifier surfaceProtAttrModifier(&surf, Memory::ReadWrite);
        Utility::CleanupOnReturn<SurfaceProtAttrModifier, RC>
                restoreSurfaceProtAttr(&surfaceProtAttrModifier, &SurfaceProtAttrModifier::Restore);
        if ((surf.GetProtect() & Memory::Writeable) == 0)        
        {
            CHECK_RC(surfaceProtAttrModifier.ChangeTo());
        }
        else
        {
            restoreSurfaceProtAttr.Cancel();
        }

        DmaLoader *loader = m_pTest->GetTrace()->GetDmaLoader(gpuVerif->Channel());
        if (loader == nullptr)
        {
            ErrPrintf("Failed to create a dma loader!\n");
            return RC::UNSUPPORTED_FUNCTION;
        }
        CHECK_RC(loader->AllocateNotifier(subdev));
        CHECK_RC(loader->FillSurface(&surf, 0, 0, surf.GetSize(), subdev));
    }
    else
    {
        vector<UINT08> data(surf.GetSize());

        auto pWriter = SurfaceUtils::CreateMappingSurfaceWriter(&surf);
        pWriter->SetTargetSubdevice(subdev);

        CHECK_RC(pWriter->WriteBytes(0, &data[0], data.size(), Surface2D::MapDirect));
    }

    DebugPrintf("%s: Clear surface %s with 0.\n", __FUNCTION__, surf.GetName().c_str());
    return OK;
}
            
void LWGpuSurfaceMgr::AddRef()
{
    ++refCount;
}

void LWGpuSurfaceMgr::Release()
{
    --refCount;
    if (!refCount)
    {
        delete this;
    }
}

MdiagSurf *LWGpuSurfaceMgr::EnableSurface(SurfaceType surfaceType,
    MdiagSurf *surface, bool loopback, vector<UINT32> peerIDs)
{
    SurfaceData *surfaceData = GetSurfaceDataByType(surfaceType);

    MASSERT(surfaceData != 0);
    MASSERT(!surfaceData->active);

    surfaceData->numSurfaces = 1;
    surfaceData->active = true;
    surfaceData->valid = true;
    surfaceData->surfaces[0] = *surface;

    // Need to reset the name as it will be used for finding
    // the surface later.
    surfaceData->surfaces[0].SetName(GetTypeName(surfaceType));

    SetMapSurfaceLoopback(&surfaceData->surfaces[0], loopback, peerIDs);

    return &surfaceData->surfaces[0];
}

void LWGpuSurfaceMgr::DisableSurface(SurfaceType surfaceType)
{
    SurfaceData *surfaceData = GetSurfaceDataByType(surfaceType);

    MASSERT(surfaceData != 0);
    MASSERT(surfaceData->active);

    for (UINT32 i = 0; i < surfaceData->numSurfaces; ++i)
    {
        MdiagSurf *surface = &(surfaceData->surfaces[i]);

        if (!surface->IsSurfaceView())
        {
            PagePoolMappings::iterator iter;

            for (iter = surfaceData->pagePoolMappings.begin();
                 iter != surfaceData->pagePoolMappings.end();
                 ++iter)
            {
                (*iter)->Free();
                delete (*iter);
            }
            surface->Free();
        }
    }

    surfaceData->active = false;
    surfaceData->valid = false;
}

RC LWGpuSurfaceMgr::AllocateSurface(SurfaceType surfaceType,
    BuffInfo *buffinfo)
{
    SurfaceData *surfaceData = GetSurfaceDataByType(surfaceType);

    return AllocateSurface(0, &(surfaceData->surfaces[0]), buffinfo);
}

RC LWGpuSurfaceMgr::AllocateSurface(const ArgReader *tparams,
    MdiagSurf *pSurf, BuffInfo* buffinfo)
{
    RC rc;

    pSurf->SetComponentPacking(true);

    if (pSurf->GetType() == LWOS32_TYPE_DEPTH)
    {
        if (Platform::GetSimulationMode() == Platform::Amodel)
        {
            // amodel does not support zlwll, always disable it
            pSurf->SetZLwllFlag(LWOS32_ATTR_ZLWLL_NONE);
        }
        else if (m_ZlwllEnabled || m_SlwllEnabled)
        {
            switch(m_ZlwllMode)
            {
                case ZLWLL_REQUIRED:
                    pSurf->SetZLwllFlag(LWOS32_ATTR_ZLWLL_REQUIRED);
                    break;
                case ZLWLL_ANY:
                    pSurf->SetZLwllFlag(LWOS32_ATTR_ZLWLL_ANY);
                    break;
                case ZLWLL_NONE:
                    pSurf->SetZLwllFlag(LWOS32_ATTR_ZLWLL_NONE);
                    break;
                case ZLWLL_CTXSW_SEPERATE_BUFFER:
                case ZLWLL_CTXSW_NOCTXSW:
                    break;
                default:
                    return RC::BAD_COMMAND_LINE_ARGUMENT;
            }
        }
    }

    if (pSurf->GetCompressed())
    {
        if (((tparams != 0) && tparams->ParamPresent("-compress_mode")) ||
            !pSurf->GetCreatedFromAllocSurface())
        {
            switch(m_ComprMode)
            {
                case COMPR_REQUIRED:
                    pSurf->SetCompressedFlag(LWOS32_ATTR_COMPR_REQUIRED);
                    break;
                case COMPR_ANY:
                    pSurf->SetCompressedFlag(LWOS32_ATTR_COMPR_ANY);
                    break;
                case COMPR_NONE:
                    pSurf->SetCompressedFlag(LWOS32_ATTR_COMPR_NONE);
                    break;
                default:
                    return RC::BAD_COMMAND_LINE_ARGUMENT;
            }
        }
    }

    if (ch && lwgpu->GetGpuDevice() && (ch->GetLWGpu()->GetGpuDevice() != lwgpu->GetGpuDevice()))
    {
        ErrPrintf("GpuDevice parameter and the GpuDevice associated with the "
                  "channel do not match\n");
        return RC::SOFTWARE_ERROR;
    }

    if (GetForceNull(pSurf) || !GetIsActiveSurface(pSurf))
    {
        SetValid(pSurf, false);
    }
    else
    {
        const UINT32 idtag = GetIDTag(pSurf);
        SurfaceData& surfData = m_RenderSurfaces[idtag];

        // Allocate one instance of a surface in system memory per subdevice
        if (pSurf->GetLocation() != Memory::Fb)
        {
            const UINT32 numSubdevices = lwgpu->GetGpuDevice()->GetNumSubdevices();
            if (surfData.numSurfaces != numSubdevices)
            {
                MASSERT(surfData.numSurfaces == 1);
                for (UINT32 i=1; i < numSubdevices; i++)
                {
                    surfData.surfaces[i] = surfData.surfaces[0];
                    m_IDTagMap[&surfData.surfaces[i]] = idtag;
                }
                surfData.numSurfaces = numSubdevices;
            }
        }

        bool gpu_cache_exists = false;
        CHECK_RC(GpuUtility::GetGpuCacheExist(
            lwgpu->GetGpuDevice(), &gpu_cache_exists, m_pTest->GetLwRmPtr()));

        bool must_cache_rop = GpuUtility::MustRopSysmemBeCached(
            lwgpu->GetGpuDevice(), m_pTest->GetLwRmPtr(), ch->ChannelHandle());

        // If either -phys_addr_hintC is used or -virt_addr_hintC is used,
        // all color surfaces will get the same address specification.
        // This will cause an allocation to fail if more than one
        // color surface is active.
        if (m_AllColorsFixed &&
            (m_ActiveColorSurfaces > 0) &&
            (pSurf->GetType() != LWOS32_TYPE_DEPTH))
        {
            ErrPrintf("Only one color surface may be active when either -phys_addr_hintC or -virt_addr_hintC is used.\n");

            return RC::SOFTWARE_ERROR;
        }

        bool disabledLocationOverride = false;
        INT32 oldLocationOverride = Surface2D::NO_LOCATION_OVERRIDE;

        if (tparams->ParamPresent("-disable_location_override_for_trace"))
        {
            oldLocationOverride = Surface2D::GetLocationOverride();
            Surface2D::SetLocationOverride(Surface2D::NO_LOCATION_OVERRIDE);
            disabledLocationOverride = true;
        }

        // Allocate surfaces for particular subdevices
        for (UINT32 i=0; i < surfData.numSurfaces; i++)
        {
            MdiagSurf& surf = surfData.surfaces[i];
            if ((surf.GetGpuCacheMode() == Surface2D::GpuCacheOff) &&
                (surf.GetLocation() != Memory::Fb))
            {
                // Gpu cache arguments can't turn off caching if
                // the surface requires it.
                if (must_cache_rop)
                {
                    ErrPrintf("Can't turn off GPU caching for ROP surfaces "
                              "with the current device.\n");

                    return RC::SOFTWARE_ERROR;
                }
            }
            else if ((surf.GetGpuCacheMode() == Surface2D::GpuCacheDefault) &&
                (pSurf->GetLocation() != Memory::Coherent) &&
                !lwgpu->GetGpuSubdevice(0)->IsFbBroken())
            {
                Surface2D::GpuCacheMode mode = gpu_cache_exists ?
                    Surface2D::GpuCacheOn: Surface2D::GpuCacheOff;

                surf.SetGpuCacheMode(mode);
            }

            // According to old LWSurface allocation:
            // NO_SCANOUT isn't entirely accurate, but this turns off some irritating
            // RM hardware bug workarounds.
            surf.SetScanout(false);

            // Override array size for pitch surfaces
            if (surf.GetLayout() == Surface::Pitch)
            {
                surf.SetArraySize(1);
            }

            if (!surf.IsSurfaceView())
            {
                if (Utl::HasInstance())
                {
                    Utl::Instance()->AddTestSurface(m_pTest, &surf);
                    Utl::Instance()->TriggerSurfaceAllocationEvent(&surf,
                        m_pTest);
                }

                CHECK_RC(surf.Alloc(lwgpu->GetGpuDevice(), m_pTest->GetLwRmPtr()));

                UINT64 poolSize;
                UINT64 pageSize;

                switch (surfData.pagePoolUsage)
                {
                    case NoPagePool:
                        // Nothing to do here.
                        break;

                    case SmallPagePool:
                        poolSize = m_SmallPagePoolSize;
                        pageSize = 4096;
                        if (m_PoolOfSmallPages == 0)
                        {
                            CHECK_RC(AllocPagePool(lwgpu->GetGpuDevice(),
                                    &m_PoolOfSmallPages,
                                    poolSize,
                                    pageSize,
                                    pSurf->GetLayout()));
                        }
                        CHECK_RC(MapSurfaceToPagePool(lwgpu->GetGpuDevice(),
                                &surf,
                                m_PoolOfSmallPages,
                                poolSize,
                                pageSize));
                        break;

                    case BigPagePool:
                        poolSize = m_BigPagePoolSize;
                        pageSize = m_BigPageSize;
                        if (m_PoolOfBigPages == 0)
                        {
                            CHECK_RC(AllocPagePool(lwgpu->GetGpuDevice(),
                                    &m_PoolOfBigPages,
                                    poolSize,
                                    pageSize,
                                    pSurf->GetLayout()));
                        }
                        CHECK_RC(MapSurfaceToPagePool(lwgpu->GetGpuDevice(),
                                &surf,
                                m_PoolOfBigPages,
                                poolSize,
                                pageSize));
                        break;

                    default:
                        MASSERT(!"Unrecognized page pool value");
                        break;
                }

                vector<UINT32> peerIDs;
                if (GetMapSurfaceLoopback(pSurf, &peerIDs))
                {
                    vector<UINT32>::iterator iter = peerIDs.begin();
                    while (iter != peerIDs.end())
                    {
                        CHECK_RC(surf.MapLoopback(*iter));
                        iter++;
                    }
                }
            }
            PrintParams(&surf);
            PerformanceMonitor::AddSurface(&surf);
            buffinfo->SetRenderSurface(surf);
        }

        SetValid(pSurf, true);

        if (pSurf->GetType() != LWOS32_TYPE_DEPTH)
        {
            ++m_ActiveColorSurfaces;
        }

        if (disabledLocationOverride)
        {
            Surface2D::SetLocationOverride(oldLocationOverride);
        }
    }

    return OK;
}

// This function maps the entire virtual allocation of a surface to it's
// physical allocation.  This function is only used in the presence
// of -map_region command-line arguments as normally the Surface2D::Alloc
// call will do a map of the entire surface.
RC LWGpuSurfaceMgr::DoFullMap(MdiagSurf *surface)
{
    RC rc = OK;

    SurfaceData *data = GetSurfaceDataBySurface(surface);

    data->fullMap.SetArrayPitch(surface->GetArrayPitch());
    data->fullMap.SetSpecialization(Surface2D::MapOnly);

    CHECK_RC(data->fullMap.MapVirtToPhys(lwgpu->GetGpuDevice(), surface,
        surface, 0, 0, m_pTest->GetLwRmPtr()));

    return rc;
}

RC LWGpuSurfaceMgr::AllocateSurfaces(const ArgReader *tparams, BuffInfo* buffinfo,
                                     UINT32 subdev /* = Gpu::UNSPECIFIED_SUBDEV */ )
{
    LwRm* pLwRm = m_pTest->GetLwRmPtr();
    RC rc;
    MdiagSurf *surface;

    for (int i = 0; i < MAX_RENDER_TARGETS; i++)
    {
        surface = GetSurface(ColorSurfaceTypes[i], subdev);

        // Surface views need to be handled later.
        if (!surface->IsSurfaceView())
        {
            CHECK_RC(AllocateSurface(tparams, surface, buffinfo));
            SetupColorCompress(surface);

            // If this render target isn't used, issue an error message
            // if either the corresponding width or height command-line
            // argument is specified.  Width and height command-line
            // arguments for disabled render targets can affect the CRC values
            // of the enabled render targets.  This happens when MODS sends
            // methods such as a clipping rectangle or GPU clear rectangle.
            // The width and height of these methods is determined by taking
            // the minimum width and height of all render targets, whether
            // or not they are enabled.  Ideally the minimum width and height
            // callwlation would not count disabled render targets, but
            // lwrrently this is too diffilwlt and risky a change.
            //
            // An exception is made if the corresponding -formatC argument
            // is specified as DISABLED as this argument can be processed
            // when the minimum rectangle is callwlated and the
            // -widthC* and -heightC* arguments are ignored in that case.
            if (!GetValid(surface) &&
                ((tparams->ParamPresent(FormatCArgNames[i]) == 0) ||
                (strcmp(tparams->ParamStr(FormatCArgNames[i]), "DISABLED") != 0)))
            {
                if (tparams->ParamPresent(WidthArgNames[i]) > 0)
                {
                    ErrPrintf("Argument %s specified but the corresponding color surface is not enabled in the trace.\n", WidthArgNames[i]);
                    return RC::BAD_COMMAND_LINE_ARGUMENT;
                }
                else if (tparams->ParamPresent(HeightArgNames[i]) > 0)
                {
                    ErrPrintf("Argument %s specified but the corresponding color surface is not enabled in the trace.\n", HeightArgNames[i]);
                    return RC::BAD_COMMAND_LINE_ARGUMENT;
                }
            }
        }
    }

    surface = GetSurface(SURFACE_TYPE_Z, subdev);

    // Surface views need to be handled later.
    if (!surface->IsSurfaceView())
    {
        CHECK_RC(AllocateSurface(tparams, surface, buffinfo));
        SetupZCompress(surface);

        GpuDevice *pGpuDev = lwgpu->GetGpuDevice();

        //"gpu clear" cannot be used for tir test.
        // Spec: "ClearSurface" method at //hw/lwgpu/class/mfs/class/3d/volta.mfs
        if (surface->FormatHasXBits() && !tparams->ParamPresent("-enable_tir") &&
            (!pGpuDev->GetSubdevice(subdev)->HasFeature(Device::GPUSUB_SUPPORTS_SYSMEM_REFLECTED_MAPPING) ||
             pGpuDev->GetSubdevice(subdev)->IsFakeBar1Enabled()))

        {
            InfoPrintf("use gpu clear for Z surface with X bits!\n");
            m_FastClearDepth = true;
        }

        // If this render target isn't used, issue an error message
        // if either the corresponding width or height command-line
        // argument is specified.  Width and height command-line
        // arguments for disabled render targets can affect the CRC values
        // of the enabled render targets.  This happens when MODS sends
        // methods such as a clipping rectangle or GPU clear rectangle.
        // The width and height of these methods is determined by taking
        // the minimum width and height of all render targets, whether
        // or not they are enabled.  Ideally the minimum width and height
        // callwlation would not count disabled render targets, but
        // lwrrently this is too diffilwlt and risky a change.
        //
        // An exception is made if -zt_count_0 is specified as this argument
        // can be read when the minimum rectangle is callwlated and the
        // -widthZ and -heightZ arguments are ignored in that case.
        if (!GetValid(surface) && (tparams->ParamPresent("-zt_count_0") == 0))
        {
            if (tparams->ParamPresent("-widthZ") > 0)
            {
                ErrPrintf("Argument -widthZ specified but the corresponding color surface is not enabled in the trace.\n");
                return RC::BAD_COMMAND_LINE_ARGUMENT;
            }
            else if (tparams->ParamPresent("-heightZ") > 0)
            {
                ErrPrintf("Argument -heightZ specified but the corresponding color surface is not enabled in the trace.\n");
                return RC::BAD_COMMAND_LINE_ARGUMENT;
            }
        }
    }

    CHECK_RC(DoPartialMaps());

    if (GetNeedClipID())
    {
        UINT32 ClipIDWidth  = m_Width  + GetUnscaledWindowOffsetX();
        UINT32 ClipIDHeight = m_Height + GetUnscaledWindowOffsetY();

        LW0080_CTRL_GR_GET_CAPS_V2_PARAMS grParams = { 0 };
   
        rc = pLwRm->Control(pLwRm->GetDeviceHandle(lwgpu->GetGpuDevice()),
                            LW0080_CTRL_CMD_GR_GET_CAPS_V2,
                            &grParams,
                            sizeof(grParams));

        if (rc != OK)
            return rc;

        if (LW0080_CTRL_FB_GET_CAP(grParams.capsTbl, LW0080_CTRL_GR_CAPS_CLIP_ID_BUG_236807))
        {
            ClipIDWidth = 0x2000;
            m_ClipID.SetVirtAlignment(0x4000000);
        }

        // Only allocate clip ID for gfx tests
        InfoPrintf("Allocating clip ID buffer ... \n");
        m_ClipID.SetColorFormat(ColorUtils::Y8);
        m_ClipID.SetForceSizeAlloc(true);
        m_ClipID.SetName(GetTypeName(SURFACE_TYPE_CLIPID));
        LwRm::Handle hVASpace = surface->GetGpuVASpace();
        m_ClipID.SetGpuVASpace(hVASpace);

        ClipIDWidth = (ClipIDWidth + lwgpu->GetGpuDevice()->GobWidth() - 1 ) &
            ~(lwgpu->GetGpuDevice()->GobWidth() - 1);
        ClipIDHeight = (ClipIDHeight + m_ClipBlockHeight*lwgpu->GetGpuDevice()->GobHeight() - 1) &
            ~(m_ClipBlockHeight*lwgpu->GetGpuDevice()->GobHeight() - 1);

        m_ClipID.SetWidth(ClipIDWidth);
        m_ClipID.SetPitch(ClipIDWidth);
        m_ClipID.SetHeight(ClipIDHeight);
        m_ClipID.SetLayout(Surface::BlockLinear);
        SetPteKind(m_ClipID, m_ClipIDPteKindName, lwgpu->GetGpuDevice());
        m_ClipID.SetAlignment(512);
        CHECK_RC(m_ClipID.Alloc(lwgpu->GetGpuDevice(), pLwRm));
        if (ch)
            CHECK_RC(m_ClipID.BindGpuChannel(ch->ChannelHandle()));

        buffinfo->SetDmaBuff("ClipID", m_ClipID, 0, subdev);
        string str = Utility::StrPrintf("%dx%dx%d", 1, m_ClipBlockHeight, 1);
        buffinfo->SetSizeBL(str.c_str());
        str = Utility::StrPrintf("%dx%d", ClipIDWidth, ClipIDHeight);
        buffinfo->SetSizePix(str.c_str());
        buffinfo->SetDescription("wid");
        PrintDmaBufferParams(m_ClipID);

        InfoPrintf("%dx%d pixels, 0x%x bytes are allocated for ClipID @0x%llx\n",
                ClipIDWidth, ClipIDHeight, m_ClipID.GetSize(), m_ClipID.GetCtxDmaOffsetGpu());
    }

    return OK;
}

//-----------------------------------------------------------------------------
// Handle color/z surfaces that were declared with SURFACE_VIEW in the
// trace header.  (These surfaces need too be processed after the trace
// surfaces are allocated as surface views are essentially pointers to
// other buffers.
//
RC LWGpuSurfaceMgr::ProcessSurfaceViews
(
    const ArgReader * tparams,
    BuffInfo *        buffinfo,
    UINT32            subdev /* = Gpu::UNSPECIFIED_SUBDEV */
)
{
    RC rc;
    MdiagSurf *surface;

    for (int i = 0; i < MAX_RENDER_TARGETS; i++)
    {
        surface = GetSurface(ColorSurfaceTypes[i], subdev);

        if (surface->IsSurfaceView())
        {
            CHECK_RC(AllocateSurface(tparams, surface, buffinfo));
        }
    }

    surface = GetSurface(SURFACE_TYPE_Z, subdev);

    if (surface->IsSurfaceView())
    {
        CHECK_RC(AllocateSurface(tparams, surface, buffinfo));
    }

    return OK;
}

void LWGpuSurfaceMgr::UseTraceSurfaces()
{
    int i;

    // Initialize surface pointers - not all chips have all of these
    for (i = 0; i < MAX_RENDER_TARGETS; i++)
    {
        MdiagSurf* const surf = AddSurface();
        SurfaceType surface_type = ColorSurfaceTypes[i];

        surf->SetLocation(Memory::Fb);

        // Extra color surfaces are not normally used unless specifically
        // asked for by a trace/runtime options, at which point they will
        // be marked non-null and/or active.
        SetIsActiveSurface(surf, false);

        surf->SetName(GetTypeName(surface_type));
    }

    MdiagSurf* const surf = AddSurface();

    surf->SetLocation(Memory::Fb);
    SetIsActiveSurface(surf, false);
    surf->SetType(LWOS32_TYPE_DEPTH);

    surf->SetName(GetTypeName(SURFACE_TYPE_Z));
}

BufferConfig* LWGpuSurfaceMgr::GetBufferConfig() const
{
    return new TeslaBufferConfig;
}

RC LWGpuSurfaceMgr::InitSurfaceParams(const ArgReader *tparams, UINT32 subdev)
{
    RC rc;
    GpuDevice *pGpuDev = lwgpu->GetGpuDevice();

    m_DefaultWidth = 160;
    m_DefaultHeight = 120;

    if (tparams->ParamPresent("-RawImageMode"))
        m_rawImageMode = (_RAWMODE)tparams->ParamUnsigned("-RawImageMode");
    else
        m_rawImageMode = RAWOFF;

    MdiagSurf *surfC[MAX_RENDER_TARGETS];
    SurfaceData *surfDataC[MAX_RENDER_TARGETS];
    int i;
    for (i = 0; i < MAX_RENDER_TARGETS; i++)
    {
        surfC[i] = GetSurface(ColorSurfaceTypes[i], subdev);
        surfDataC[i] = GetSurfaceDataByType(ColorSurfaceTypes[i]);
    }
    MdiagSurf *surfZ = GetSurface(SURFACE_TYPE_Z, subdev);
    SurfaceData *surfDataZ = GetSurfaceDataByType(SURFACE_TYPE_Z);

    // Clip ID buffer is just a standard DmaBuffer
    ParseDmaBufferArgs(m_ClipID, tparams, "wid", &m_ClipIDPteKindName, 0);

    _DMA_TARGET target = _DMA_TARGET_VIDEO;
    target = (_DMA_TARGET)tparams->ParamUnsigned("-loc", _DMA_TARGET_VIDEO);
    target = (_DMA_TARGET)tparams->ParamUnsigned("-locC", target);
    bool bLoopbackMode = tparams->ParamPresent("-sli_p2ploopback") > 0;
    for (i = 0; i < MAX_RENDER_TARGETS; i++)
    {
        if (tparams->ParamPresent("-loc")  > 0 ||
            tparams->ParamPresent("-locC") > 0 ||
            tparams->ParamPresent(ColorLocArgNames[i]) > 0)
        {
            _DMA_TARGET target_internal = target;
            if (tparams->ParamPresent(ColorLocArgNames[i]) > 0)
            {
                target_internal = (_DMA_TARGET)tparams->ParamUnsigned(ColorLocArgNames[i]);
            }
            if (target_internal == _DMA_TARGET_P2P)
            {
                if (bLoopbackMode)
                {
                    surfC[i]->SetLoopBack(true);
                    target_internal = _DMA_TARGET_VIDEO;
                }
                else
                {
                    ErrPrintf("Doesn't support RT in peer yet\n");
                    return RC::BAD_PARAMETER;
                }
            }
            surfC[i]->SetLocation(TargetToLocation(target_internal));
        }
        else if (!surfC[i]->GetCreatedFromAllocSurface())
        {
            surfC[i]->SetLocation(TargetToLocation(_DMA_TARGET_VIDEO));
        }
    }

    target = (_DMA_TARGET)tparams->ParamUnsigned("-loc", _DMA_TARGET_VIDEO);
    target = (_DMA_TARGET)tparams->ParamUnsigned("-locZ", target);
    if (tparams->ParamPresent("-loc")  > 0 ||
        tparams->ParamPresent("-locZ") > 0)
    {
        if (target == _DMA_TARGET_P2P)
        {
            if (bLoopbackMode)
            {
                surfZ->SetLoopBack(true);
                target = _DMA_TARGET_VIDEO;
            }
            else
            {
                ErrPrintf("Doesn't support Z in peer yet\n");
                return RC::BAD_PARAMETER;
            }
        }
        surfZ->SetLocation(TargetToLocation(target));
    }
    else if (!surfZ->GetCreatedFromAllocSurface())
    {
        surfZ->SetLocation(TargetToLocation(_DMA_TARGET_VIDEO));
    }

    for (i = 0; i < MAX_RENDER_TARGETS; i++)
    {
        if (tparams->ParamPresent(ColorSplitLocArgNames[i]))
            target = (_DMA_TARGET)tparams->ParamUnsigned(ColorSplitLocArgNames[i]);
        else
            target = (_DMA_TARGET)tparams->ParamUnsigned("-split_locC", _DMA_TARGET_VIDEO);
        surfC[i]->SetSplitLocation(TargetToLocation(target));
    }

    bool splitSurface;
    for (i = 0; i < MAX_RENDER_TARGETS; i++)
    {
        splitSurface = false;
        if (tparams->ParamPresent(ColorSplitArgNames[i]))
            splitSurface = true;
        else
            splitSurface = tparams->ParamPresent("-splitC") > 0;
        surfC[i]->SetSplit(splitSurface);
    }

    target = (_DMA_TARGET)tparams->ParamUnsigned("-split_locZ", _DMA_TARGET_VIDEO);
    surfZ->SetSplitLocation(TargetToLocation(target));
    splitSurface = tparams->ParamPresent("-splitZ") > 0;
    surfZ->SetSplit(splitSurface);

    for (i = 0; i < MAX_RENDER_TARGETS; i++)
    {
        if (tparams->ParamPresent(PageSizeArgNames[i]))
        {
            surfC[i]->SetPageSize(tparams->ParamUnsigned(PageSizeArgNames[i]));
        }
        else if (tparams->ParamPresent("-page_sizeC"))
        {
            surfC[i]->SetPageSize(tparams->ParamUnsigned("-page_sizeC"));
        }
        else if (tparams->ParamPresent("-page_size"))
        {
            surfC[i]->SetPageSize(tparams->ParamUnsigned("-page_size"));
        }
    }

    if (tparams->ParamPresent("-page_sizeZ"))
    {
        surfZ->SetPageSize(tparams->ParamUnsigned("-page_sizeZ"));
    }
    else if (tparams->ParamPresent("-page_size"))
    {
        surfZ->SetPageSize(tparams->ParamUnsigned("-page_size"));
    }

    _PAGE_LAYOUT Layout;
    for (i = 0; i < MAX_RENDER_TARGETS; i++)
    {
        if (tparams->ParamPresent(CtxDmaTypeArgNames[i]))
            Layout = (_PAGE_LAYOUT)tparams->ParamUnsigned(CtxDmaTypeArgNames[i]);
        else if (tparams->ParamPresent("-ctx_dma_typeC"))
            Layout = (_PAGE_LAYOUT)tparams->ParamUnsigned("-ctx_dma_typeC");
        else
            Layout = (_PAGE_LAYOUT)tparams->ParamUnsigned("-ctx_dma_type", PAGE_VIRTUAL);
        surfC[i]->SetPhysContig(Layout == PAGE_PHYSICAL);
    }
    if (tparams->ParamPresent("-ctx_dma_typeZ"))
        Layout = (_PAGE_LAYOUT)tparams->ParamUnsigned("-ctx_dma_typeZ");
    else
        Layout = (_PAGE_LAYOUT)tparams->ParamUnsigned("-ctx_dma_type", PAGE_VIRTUAL);
    surfZ->SetPhysContig(Layout == PAGE_PHYSICAL);

    bool CtxDmaAttrs;
    for (i = 0; i < MAX_RENDER_TARGETS; i++)
    {
        CtxDmaAttrs  = (tparams->ParamPresent(CtxDmaAttrsArgNames[i]) > 0);
        CtxDmaAttrs |= (tparams->ParamPresent("-ctxdma_attrsC") > 0);
        CtxDmaAttrs |= (tparams->ParamPresent("-ctxdma_attrs") > 0);
        surfC[i]->SetMemAttrsInCtxDma(CtxDmaAttrs);
    }
    CtxDmaAttrs  = (tparams->ParamPresent("-ctxdma_attrsZ") > 0);
    CtxDmaAttrs |= (tparams->ParamPresent("-ctxdma_attrs") > 0);
    surfZ->SetMemAttrsInCtxDma(CtxDmaAttrs);

    bool VaReverse, PaReverse;
    for (i = 0; i < MAX_RENDER_TARGETS; i++)
    {
        VaReverse  = (tparams->ParamPresent(VaReverseArgNames[i]) > 0) ||
                     (tparams->ParamPresent("-va_reverseC") > 0) ||
                     (tparams->ParamPresent("-va_reverse") > 0);
        surfC[i]->SetVaReverse(VaReverse);
        PaReverse  = (tparams->ParamPresent(PaReverseArgNames[i]) > 0) ||
                     (tparams->ParamPresent("-pa_reverseC") > 0) ||
                     (tparams->ParamPresent("-pa_reverse") > 0);
        surfC[i]->SetPaReverse(PaReverse);

    }
    VaReverse  = (tparams->ParamPresent("-va_reverseZ") > 0) ||
                 (tparams->ParamPresent("-va_reverse") > 0);
    surfZ->SetVaReverse(VaReverse);
    PaReverse  = (tparams->ParamPresent("-pa_reverseZ") > 0) ||
                 (tparams->ParamPresent("-pa_reverse") > 0);
    surfZ->SetPaReverse(PaReverse);

    UINT32 MaxCoalesce;
    for (i = 0; i < MAX_RENDER_TARGETS; i++)
    {
        if (tparams->ParamPresent(MaxCoalesceArgNames[i]))
            MaxCoalesce = tparams->ParamUnsigned(MaxCoalesceArgNames[i]);
        else if (tparams->ParamPresent("-max_coalesceC"))
            MaxCoalesce = tparams->ParamUnsigned("-max_coalesceC");
        else
            MaxCoalesce = tparams->ParamUnsigned("-max_coalesce", 0);
        surfC[i]->SetMaxCoalesce(MaxCoalesce);
    }
    if (tparams->ParamPresent("-max_coalesceZ"))
        MaxCoalesce = tparams->ParamUnsigned("-max_coalesceZ");
    else
        MaxCoalesce = tparams->ParamUnsigned("-max_coalesce", 0);
    surfZ->SetMaxCoalesce(MaxCoalesce);

    UINT32 extraWidth = 0;
    UINT32 extraHeight = 0;

    if (tparams->ParamPresent("-woffsetx"))
    {
        userSpecifiedWindowOffset = true;
        m_UnscaledWOffsetX += tparams->ParamSigned("-woffsetx");
        extraWidth += m_UnscaledWOffsetX;
    }
    if (tparams->ParamPresent("-woffsety"))
    {
        userSpecifiedWindowOffset = true;
        m_UnscaledWOffsetY += tparams->ParamSigned("-woffsety");
        extraHeight += m_UnscaledWOffsetY;
    }

    m_FastClearColor = m_FastClearDepth = (tparams->ParamPresent("-fastclear") > 0) ||
                                          (tparams->ParamPresent("-gpu_clear") > 0);
    if (m_FastClearColor || m_FastClearDepth)
    {
        //"gpu clear" cannot be used for tir test.
        // Spec: "ClearSurface" method at //hw/lwgpu/class/mfs/class/3d/volta.mfs
        if (tparams->ParamPresent("-enable_tir"))
        {
            MASSERT(!"gpu clear is not supported with tir.");
        }
    }

    m_SrgbWrite = (tparams->ParamPresent("-srgb_write") > 0);

    // Select color buffer format
    for (i = 0; i < MAX_RENDER_TARGETS; i++)
    {
        const char *color_format_name;
        if (tparams->ParamPresent(FormatCArgNames[i]))
            color_format_name = tparams->ParamStr(FormatCArgNames[i]);
        else if (tparams->ParamPresent("-formatC"))
            color_format_name = tparams->ParamStr("-formatC");
        else if (surfC[i]->GetCreatedFromAllocSurface())
            continue;
        else
            color_format_name = "A8R8G8B8";

        if (!strcmp(color_format_name, "DISABLED"))
        {
            SetForceNull(surfC[i], true);
            surfC[i]->SetColorFormat(ColorUtils::A8R8G8B8);
        }
        else
        {
            UINT32 color_format = ColorFormatFromName(color_format_name);
            if (color_format == _ILWALID_COLOR_FORMAT)
            {
                ErrPrintf("Color format %s not supported by GPU arch=0x%08x\n",
                    color_format_name, lwgpu->GetArchitecture());
                return RC::BAD_PARAMETER;
            }

            surfC[i]->SetColorFormat(FormatFromDeviceFormat(color_format));
        }
    }

    // Select Z buffer format
    const char *zeta_format_name;
    bool skipZetaFormat = surfZ->GetCreatedFromAllocSurface();
    if (tparams->ParamPresent("-formatZ"))
    {
        zeta_format_name = tparams->ParamStr("-formatZ");
        skipZetaFormat = false;
    }
    else
    {
        // always default to Z24S8, regardless of color depth
        zeta_format_name = "Z24S8"; //benzetahack - not used
    }

    if (!skipZetaFormat)
    {
        UINT32 zeta_format = ZetaFormatFromName(zeta_format_name);
        if (zeta_format == _ILWALID_ZETA_FORMAT)
        {
            ErrPrintf("Zeta format %s not supported by GPU arch=0x%08x\n",
                zeta_format_name, lwgpu->GetArchitecture());
            return RC::BAD_PARAMETER;
        }

        surfZ->SetColorFormat(FormatFromDeviceFormat(zeta_format));
    }

    // init our working display width/height vars
    UINT32 DisplayWidth = m_DefaultWidth;
    UINT32 DisplayHeight = m_DefaultHeight;

    // we need a surface to play in - let's allocate it
    bool userSpecifiedSize = false;
    if (tparams->ParamPresent("-width"))
    {
        DisplayWidth = tparams->ParamUnsigned("-width");
        userSpecifiedSize = true;
    }
    if (tparams->ParamPresent("-height"))
    {
        DisplayHeight = tparams->ParamUnsigned("-height");
        userSpecifiedSize = true;
    }

    if (tparams->ParamPresent("-scrnsize"))
    {
        userSpecifiedSize = true;
        switch (tparams->ParamUnsigned("-scrnsize"))
        {
        case SCRNSIZE_80x60:
            DisplayWidth = 80;
            DisplayHeight = 60;
            break;
        case SCRNSIZE_160x120:
            DisplayWidth = 160;
            DisplayHeight = 120;
            break;
        case SCRNSIZE_320x240:
            DisplayWidth = 320;
            DisplayHeight = 240;
            break;
        case SCRNSIZE_640x480:
            DisplayWidth = 640;
            DisplayHeight = 480;
            break;
        case SCRNSIZE_800x600:
            DisplayWidth = 800;
            DisplayHeight = 600;
            break;
        case SCRNSIZE_1024x768:
            DisplayWidth = 1024;
            DisplayHeight = 768;
            break;
        case SCRNSIZE_1280x1024:
            DisplayWidth = 1280;
            DisplayHeight = 1024;
            break;
        case SCRNSIZE_1600x1200:
            DisplayWidth = 1600;
            DisplayHeight = 1200;
            break;
        case SCRNSIZE_2048x1536:
            DisplayWidth = 2048;
            DisplayHeight = 1536;
            break;
        default:
            ErrPrintf("bad screen size parameter specified\n");
            return RC::BAD_PARAMETER;
        }
    }

    // check if we are trying to run the trace at a different resolution than intended
    if (((m_TargetDisplayWidth > 0) && (m_TargetDisplayWidth != DisplayWidth)) ||
        ((m_TargetDisplayHeight > 0) && (m_TargetDisplayHeight != DisplayHeight)))
    {
        if (userSpecifiedSize)
        {
            DebugPrintf("Trace requested size %dx%d is being overridden by user specified size %dx%d.\n",
                m_TargetDisplayWidth, m_TargetDisplayHeight, DisplayWidth, DisplayHeight);
        }
        else
        {
            // change to target size
            DebugPrintf("This test will execute at %dx%d rather than the default, %dx%d.\n",
                m_TargetDisplayWidth, m_TargetDisplayHeight, m_DefaultWidth, m_DefaultHeight);
            DisplayWidth = m_TargetDisplayWidth;
            DisplayHeight = m_TargetDisplayHeight;
        }
    }

    m_ZlwllEnabled = (tparams->ParamPresent("-zlwll") != 0);
    m_SlwllEnabled = (tparams->ParamPresent("-slwll") != 0);
    m_ZlwllMode    = (ZLwllEnum)tparams->ParamUnsigned("-zlwll_mode", ZLWLL_REQUIRED);

    m_ComprMode    = (Compression)tparams->ParamUnsigned("-compress_mode", COMPR_REQUIRED);

    // Need the target to have a value for scaling purposes later
    if (m_TargetDisplayWidth == 0)
    {
        m_TargetDisplayWidth = m_DefaultWidth;
    }
    if (m_TargetDisplayHeight == 0)
    {
        m_TargetDisplayHeight = m_DefaultHeight;
    }

    bool aaUsed = false;
    AAmodetype aaMode;

    // Set anti-aliasing sample mode
    if (tparams->ParamPresent("-aamode"))
    {
        aaMode = (AAmodetype)tparams->ParamUnsigned("-aamode");

        for (i = 0; i < MAX_RENDER_TARGETS; ++i)
        {
            SetSurfaceAAMode(surfC[i], aaMode);
        }

        SetSurfaceAAMode(surfZ, aaMode);
    }

    // If a TIR mode is specified on the command-line, it overrides
    // the AA mode of the Z buffer.
    aaMode = GetAAModeFromSurface(surfZ);
    aaMode = GetTirAAMode(tparams, aaMode);
    SetSurfaceAAMode(surfZ, aaMode);

    for (i = 0; i < MAX_RENDER_TARGETS; ++i)
    {
        aaMode = GetAAModeFromSurface(surfC[i]);
        CHECK_RC(CheckAAMode(aaMode, tparams));

        if (aaMode > AAMODE_1X1)
        {
            aaUsed = true;
        }
    }

    aaMode = GetAAModeFromSurface(surfZ);
    CHECK_RC(CheckAAMode(aaMode, tparams));

    if (aaMode > AAMODE_1X1)
    {
        aaUsed = true;
    }

    // if the framebuffer is bloated (has more than 1 sample)
    // then we force multisampling on, the -MSdisable option has to be used to disable it
    if (aaUsed)
    {
        m_MultiSampleOverride = true;
        m_MultiSampleOverrideValue = 1;
        m_AAUsed = true;
    }

    // Default color and Z surface layout - BlockLinear
    const Surface::Layout default_surface_pitch_typeC = Surface::BlockLinear;
    Surface::Layout surface_pitch_typeC = default_surface_pitch_typeC;
    Surface::Layout surface_pitch_typeZ =
        surfZ->GetCreatedFromAllocSurface() ?
            surfZ->GetLayout() : Surface::BlockLinear;

    // Override default surface layout from command line
    if (tparams->ParamPresent("-pitch"))
    {
        surface_pitch_typeC = Surface::Pitch;
        surface_pitch_typeZ = Surface::Pitch;
    }
    
    if (tparams->ParamPresent("-pitch") && 
        !strcmp("Z24S8", tparams->ParamStr("-pte_kindC", "")))
    {
        ErrPrintf("-pitch cannot be used when -pte_kindC Z24S8 is used.\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    switch (surface_pitch_typeC)
    {
        case Surface::Pitch:         DebugPrintf("Pitch surface for Color\n"); break;
        case Surface::BlockLinear:   DebugPrintf("Block linear surface for Color\n"); break;
        default:                     MASSERT(!"Invalid Layout for Color");
    }
    switch (surface_pitch_typeZ)
    {
        case Surface::Pitch:         DebugPrintf("Pitch surface for Z\n"); break;
        case Surface::BlockLinear:   DebugPrintf("Block linear surface for Z\n"); break;
        default:                     MASSERT(!"Invalid Layout for Z");
    }

    // Set gobs per block for each buffer
    UINT32 BlockWidth, BlockHeight, BlockDepth;
    UINT32 LogBlockWidth, LogBlockHeight, LogBlockDepth;
    for (i = 0; i < MAX_RENDER_TARGETS; i++)
    {
        if (tparams->ParamPresent(BlockWidthArgNames[i]))
            BlockWidth = tparams->ParamUnsigned(BlockWidthArgNames[i]);
        else if (tparams->ParamPresent("-block_widthC"))
            BlockWidth = tparams->ParamUnsigned("-block_widthC");
        else if (tparams->ParamPresent("-block_width"))
            BlockWidth = tparams->ParamUnsigned("-block_width");
        else if (surfC[i]->GetCreatedFromAllocSurface())
            BlockWidth = 1U << surfC[i]->GetLogBlockWidth();
        else
            BlockWidth = 1;

        if (tparams->ParamPresent(BlockHeightArgNames[i]))
            BlockHeight = tparams->ParamUnsigned(BlockHeightArgNames[i]);
        else if (tparams->ParamPresent("-block_heightC"))
            BlockHeight = tparams->ParamUnsigned("-block_heightC");
        else if (tparams->ParamPresent("-block_height"))
            BlockHeight = tparams->ParamUnsigned("-block_height");
        else if (surfC[i]->GetCreatedFromAllocSurface())
            BlockHeight = 1U << surfC[i]->GetLogBlockHeight();
        else
            BlockHeight = 16;

        if (tparams->ParamPresent(BlockDepthArgNames[i]))
            BlockDepth = tparams->ParamUnsigned(BlockDepthArgNames[i]);
        else if (tparams->ParamPresent("-block_depthC"))
            BlockDepth = tparams->ParamUnsigned("-block_depthC");
        else if (tparams->ParamPresent("-block_depth"))
            BlockDepth = tparams->ParamUnsigned("-block_depth");
        else if (surfC[i]->GetCreatedFromAllocSurface())
            BlockDepth = 1U << surfC[i]->GetLogBlockDepth();
        else
            BlockDepth = 1;

        LogBlockWidth  = log2(BlockWidth);
        LogBlockHeight = log2(BlockHeight);
        LogBlockDepth  = log2(BlockDepth);
        if ((BlockWidth  != (1U << LogBlockWidth)) ||
            (BlockHeight != (1U << LogBlockHeight)) ||
            (BlockDepth  != (1U << LogBlockDepth)))
        {
            ErrPrintf("block size must be a power of two\n");
            return RC::BAD_PARAMETER;
        }
        surfC[i]->SetLogBlockWidth(LogBlockWidth);
        surfC[i]->SetLogBlockHeight(LogBlockHeight);
        surfC[i]->SetLogBlockDepth(LogBlockDepth);
    }

    if (tparams->ParamPresent("-block_widthZ"))
        BlockWidth = tparams->ParamUnsigned("-block_widthZ");
    else if (tparams->ParamPresent("-block_width"))
        BlockWidth = tparams->ParamUnsigned("-block_width");
    else if (surfZ->GetCreatedFromAllocSurface())
        BlockWidth = 1U << surfZ->GetLogBlockWidth();
    else
        BlockWidth = 1;

    if (tparams->ParamPresent("-block_heightZ"))
        BlockHeight = tparams->ParamUnsigned("-block_heightZ");
    else if (tparams->ParamPresent("-block_height"))
        BlockHeight = tparams->ParamUnsigned("-block_height");
    else if (surfZ->GetCreatedFromAllocSurface())
        BlockHeight = 1U << surfZ->GetLogBlockHeight();
    else
        BlockHeight = 16;

    if (tparams->ParamPresent("-block_depthZ"))
        BlockDepth = tparams->ParamUnsigned("-block_depthZ");
    else if (tparams->ParamPresent("-block_depth"))
        BlockDepth = tparams->ParamUnsigned("-block_depth");
    else if (surfZ->GetCreatedFromAllocSurface())
        BlockDepth = 1U << surfZ->GetLogBlockDepth();
    else
        BlockDepth = 1;

    LogBlockWidth  = log2(BlockWidth);
    LogBlockHeight = log2(BlockHeight);
    LogBlockDepth  = log2(BlockDepth);
    if ((BlockWidth  != (1U << LogBlockWidth)) ||
        (BlockHeight != (1U << LogBlockHeight)) ||
        (BlockDepth  != (1U << LogBlockDepth)))
    {
        ErrPrintf("block size must be a power of two\n");
        return RC::BAD_PARAMETER;
    }
    surfZ->SetLogBlockWidth(LogBlockWidth);
    surfZ->SetLogBlockHeight(LogBlockHeight);
    surfZ->SetLogBlockDepth(LogBlockDepth);

    for (i = 0; i < MAX_RENDER_TARGETS; i++)
    {
        if (tparams->ParamPresent(WidthArgNames[i]))
            surfC[i]->SetWidth(tparams->ParamUnsigned(WidthArgNames[i]));
        else if (tparams->ParamPresent("-widthC"))
            surfC[i]->SetWidth(tparams->ParamUnsigned("-widthC"));
        else if (userSpecifiedSize || !surfC[i]->GetCreatedFromAllocSurface())
            surfC[i]->SetWidth(DisplayWidth);

        if (tparams->ParamPresent(HeightArgNames[i]))
            surfC[i]->SetHeight(tparams->ParamUnsigned(HeightArgNames[i]));
        else if (tparams->ParamPresent("-heightC"))
            surfC[i]->SetHeight(tparams->ParamUnsigned("-heightC"));
        else if (userSpecifiedSize || !surfC[i]->GetCreatedFromAllocSurface())
            surfC[i]->SetHeight(DisplayHeight);

        if (tparams->ParamPresent("-depthC"))
            surfC[i]->SetDepth(tparams->ParamUnsigned("-depthC"));
        else if (tparams->ParamPresent("-depth"))
            surfC[i]->SetDepth(tparams->ParamUnsigned("-depth"));
        else if (!surfC[i]->GetCreatedFromAllocSurface())
            surfC[i]->SetDepth(m_DefaultDepth);

        if (tparams->ParamPresent("-array_sizeC"))
            surfC[i]->SetArraySize(tparams->ParamUnsigned("-array_sizeC"));
        else if (tparams->ParamPresent("-array_size"))
            surfC[i]->SetArraySize(tparams->ParamUnsigned("-array_size"));
        else if (!surfC[i]->GetCreatedFromAllocSurface())
            surfC[i]->SetArraySize(m_DefaultArraySize);
    }

    if (tparams->ParamPresent("-widthZ"))
        surfZ->SetWidth(tparams->ParamUnsigned("-widthZ"));
    else if (userSpecifiedSize || !surfZ->GetCreatedFromAllocSurface())
        surfZ->SetWidth(DisplayWidth);

    if (tparams->ParamPresent("-heightZ"))
        surfZ->SetHeight(tparams->ParamUnsigned("-heightZ"));
    else if (userSpecifiedSize || !surfZ->GetCreatedFromAllocSurface())
        surfZ->SetHeight(DisplayHeight);

    if (tparams->ParamPresent("-depthZ"))
        surfZ->SetDepth(tparams->ParamUnsigned("-depthZ"));
    else if (tparams->ParamPresent("-depth"))
        surfZ->SetDepth(tparams->ParamUnsigned("-depth", m_DefaultDepth));
    else if (!surfZ->GetCreatedFromAllocSurface())
        surfZ->SetDepth(m_DefaultDepth);

    if (tparams->ParamPresent("-array_sizeZ"))
        surfZ->SetArraySize(tparams->ParamUnsigned("-array_sizeZ"));
    else if (tparams->ParamPresent("-array_size"))
        surfZ->SetArraySize(tparams->ParamUnsigned("-array_size", m_DefaultArraySize));
    else if (!surfZ->GetCreatedFromAllocSurface())
        surfZ->SetArraySize(m_DefaultArraySize);

    //reset block sizes, bug#193099
    //reset block_height for all surfaces
    if (!tparams->ParamPresent("-block_heightC"))
    {
        for (i = 0; i < MAX_RENDER_TARGETS; i++)
        {
            if (!tparams->ParamPresent(BlockHeightArgNames[i]))
            {
                m_Height = surfC[i]->GetHeight();
                BlockHeight=1 << (surfC[i]->GetLogBlockHeight());
                while((m_Height <= pGpuDev->GobHeight()*BlockHeight >> 1) &&
                        BlockHeight > 1)
                {
                    BlockHeight >>= 1;
                }
                DebugPrintf("Reset block for surfC[%d] BlockHeight=%d\n",i,BlockHeight);
                LogBlockHeight=log2(BlockHeight);
                surfC[i]->SetLogBlockHeight(LogBlockHeight);
            }
        }
    }
    if (!tparams->ParamPresent("-block_heightZ"))
    {
        m_Height = surfZ->GetHeight();
        BlockHeight=1 << (surfZ->GetLogBlockHeight());
        while((m_Height <= pGpuDev->GobHeight()*BlockHeight >> 1) &&
                BlockHeight > 1)
        {
             BlockHeight >>=1;
        }
        DebugPrintf("Reset block for surfZ, BlockHeight=%d\n",BlockHeight);
        LogBlockHeight=log2(BlockHeight);
        surfZ->SetLogBlockHeight(LogBlockHeight);
    }

    //reset block_width for all surfaces
    if (!tparams->ParamPresent("-block_widthC"))
    {
        for (i = 0; i < MAX_RENDER_TARGETS; i++)
        {
            if (!tparams->ParamPresent(BlockWidthArgNames[i]))
            {
                m_Width = surfC[i]->GetWidth();
                BlockWidth=1 << (surfC[i]->GetLogBlockWidth());
                while((m_Width <= pGpuDev->GobWidth()*BlockWidth >> 1) &&
                        BlockWidth > 1)
                {
                    BlockWidth >>= 1;
                }
                DebugPrintf("Reset block for surfC[%d] BlockWidth=%d\n",i,BlockWidth);
                LogBlockWidth=log2(BlockWidth);
                surfC[i]->SetLogBlockWidth(LogBlockWidth);
            }
        }
    }
    if (!tparams->ParamPresent("-block_widthZ"))
    {
        m_Width = surfZ->GetWidth();
        BlockWidth=1 << (surfZ->GetLogBlockWidth());
        while((m_Width <= pGpuDev->GobWidth()*BlockWidth >> 1) &&
                BlockWidth > 1)
        {
             BlockWidth >>=1;
        }
        DebugPrintf("Reset block for surfZ, BlockWidth=%d\n",BlockWidth);
        LogBlockWidth=log2(BlockWidth);
        surfZ->SetLogBlockWidth(LogBlockWidth);
    }

    //reset block_depth for all surfaces
    //note, gob depth is always 1
    if (!tparams->ParamPresent("-block_depthC"))
    {
        for (i = 0; i < MAX_RENDER_TARGETS; i++)
        {
            if (!tparams->ParamPresent(BlockDepthArgNames[i]))
            {
                UINT32 depth = surfC[i]->GetDepth();
                BlockDepth=1 << (surfC[i]->GetLogBlockDepth());
                while((depth <= BlockDepth >> 1) && BlockDepth > 1)
                {
                    BlockDepth >>= 1;
                }
                DebugPrintf("Reset block for surfC[%d] BlockDepth=%d\n",i,BlockDepth);
                LogBlockDepth=log2(BlockDepth);
                surfC[i]->SetLogBlockDepth(LogBlockDepth);
            }
        }
    }
    if (!tparams->ParamPresent("-block_depthZ"))
    {
        UINT32 depth = surfZ->GetDepth();
        BlockDepth=1 << (surfZ->GetLogBlockDepth());
        while((depth <= BlockDepth >> 1) && BlockDepth > 1)
        {
             BlockDepth >>=1;
        }
        DebugPrintf("Reset block for surfZ, BlockDepth=%d\n",BlockDepth);
        LogBlockDepth=log2(BlockDepth);
        surfZ->SetLogBlockDepth(LogBlockDepth);
    }

    if (tparams->ParamPresent("-optimal_block_size") > 0)
    {
        UINT32 block_size[3] = {1, 1, 1};
        // Bug 427305, it's legal to get 0 fb for UMA chips, so skip checking
        //MASSERT(lwgpu->GetGpuSubdevice(subdev)->FbMask() != Gpu::IlwalidFbMask);

        for (i = 0; i < MAX_RENDER_TARGETS; i++)
        {
            BlockLinear::OptimalBlockSize3D(block_size, surfC[i]->GetBytesPerPixel(),
                    surfC[i]->GetHeight(), surfC[i]->GetDepth(), 9,
                    Utility::CountBits(lwgpu->GetGpuSubdevice(subdev)->GetFsImpl()->FbMask()),
                    pGpuDev);
            surfC[i]->SetLogBlockWidth(log2(block_size[0]));
            surfC[i]->SetLogBlockHeight(log2(block_size[1]));
            surfC[i]->SetLogBlockDepth(log2(block_size[2]));
        }
        BlockLinear::OptimalBlockSize3D(block_size, surfZ->GetBytesPerPixel(),
                surfZ->GetHeight(), surfZ->GetDepth(), 9,
                Utility::CountBits(lwgpu->GetGpuSubdevice(subdev)->GetFsImpl()->FbMask()),
                pGpuDev);
        surfZ->SetLogBlockWidth(log2(block_size[0]));
        surfZ->SetLogBlockHeight(log2(block_size[1]));
        surfZ->SetLogBlockDepth(log2(block_size[2]));
    }

    // Compute the real width and height that the trace is going to run at.  This
    // is the minimum of the dimensions of all the color and Z buffers, and it does
    // not include any AA scaling or window offset.
    m_Width  = MAX_RENDER_TARGET_WIDTH;
    m_Height = MAX_RENDER_TARGET_HEIGHT;

    for (i = 0; i < MAX_RENDER_TARGETS; i++)
    {
        if ((tparams->ParamPresent(FormatCArgNames[i]) == 0) ||
            (strcmp(tparams->ParamStr(FormatCArgNames[i]), "DISABLED") != 0))
        {
            m_Width  = min(m_Width,  surfC[i]->GetWidth());
            m_Height = min(m_Height, surfC[i]->GetHeight());
        }
    }

    if (tparams->ParamPresent("-zt_count_0") == 0)
    {
        m_Width  = min(m_Width,  surfZ->GetWidth());
        m_Height = min(m_Height, surfZ->GetHeight());
    }

    if (m_Width == MAX_RENDER_TARGET_WIDTH)
    {
        m_Width = m_DefaultWidth;
    }

    if (m_Height == MAX_RENDER_TARGET_HEIGHT)
    {
        m_Height = m_DefaultHeight;
    }

    // If the rendertarget is going to be inflated and adjusted with
    // window offset methods, figure out what the window offset should be
    // based on the image dimensions from the trace and the parameters from
    // the command-line argument.
    if (tparams->ParamPresent("-inflate_rendertarget_and_offset_window") > 0)
    {
        // The full width and height represent what the allocated dimensions
        // of the image should be.
        UINT32 fullWidth = tparams->ParamNUnsigned("-inflate_rendertarget_and_offset_window", 0);
        UINT32 fullHeight = tparams->ParamNUnsigned("-inflate_rendertarget_and_offset_window", 1);

        // The offsets from the command-line argument represent the distance
        // from the lower-right corner of the drawn image to the lower-right
        // corner of the allocated image.
        UINT32 offsetX = tparams->ParamNUnsigned("-inflate_rendertarget_and_offset_window", 2);
        UINT32 offsetY = tparams->ParamNUnsigned("-inflate_rendertarget_and_offset_window", 3);

        if (fullWidth < surfC[0]->GetWidth() + offsetX)
        {
            ErrPrintf("width parameter of -inflate_rendertarget_and_offset_window is too small (%u < %u)\n", fullWidth, surfC[0]->GetWidth() + offsetX);
        }
        if (fullHeight < surfC[0]->GetHeight() + offsetY)
        {
            ErrPrintf("height parameter of -inflate_rendertarget_and_offset_window is too small (%u < %u)\n", fullHeight, surfC[0]->GetHeight() + offsetY);
        }

        m_UnscaledWOffsetX += fullWidth - surfC[0]->GetWidth() - offsetX;
        m_UnscaledWOffsetY += fullHeight - surfC[0]->GetHeight() - offsetY;
        extraWidth += fullWidth - surfC[0]->GetWidth();
        extraHeight += fullHeight - surfC[0]->GetHeight();
        userSpecifiedWindowOffset = true;
    }

    UINT32 viewportOffsetX = 0;
    UINT32 viewportOffsetY = 0;
    if (tparams->ParamPresent("-inflate_rendertarget_and_offset_viewport") > 0)
    {
        UINT32 fullWidth = tparams->ParamNUnsigned("-inflate_rendertarget_and_offset_viewport", 0);
        UINT32 fullHeight = tparams->ParamNUnsigned("-inflate_rendertarget_and_offset_viewport", 1);
        UINT32 offsetX = tparams->ParamNUnsigned("-inflate_rendertarget_and_offset_viewport", 2);
        UINT32 offsetY = tparams->ParamNUnsigned("-inflate_rendertarget_and_offset_viewport", 3);
        if (fullWidth < surfC[0]->GetWidth() + offsetX)
        {
            ErrPrintf("width parameter of -inflate_rendertarget_and_offset_viewport is too small (%u < %u)\n", fullWidth, surfC[0]->GetWidth() + offsetX);
        }
        if (fullHeight < surfC[0]->GetHeight() + offsetY)
        {
            ErrPrintf("height parameter of -inflate_rendertarget_and_offset_viewport is too small (%u < %u)\n", fullHeight, surfC[0]->GetHeight() + offsetY);
        }
        extraWidth += fullWidth - surfC[0]->GetWidth();
        extraHeight += fullHeight - surfC[0]->GetHeight();
        viewportOffsetX = fullWidth - surfC[0]->GetWidth() - offsetX;
        viewportOffsetY = fullHeight - surfC[0]->GetHeight() - offsetY;
    }

    // Set up scaled, offset dimensions of each buffer.  Alloc dimensions will
    // be tweaked later based on pitch/swizzle/BL.
    for (i = 0; i < MAX_RENDER_TARGETS; i++)
    {
        if (tparams->ParamPresent("-inflate_rendertarget_and_offset_viewport") > 0)
        {
            surfC[i]->SetViewportOffsetExist(true);
            surfC[i]->SetViewportOffsetX(viewportOffsetX * surfC[i]->GetAAWidthScale());
            surfC[i]->SetViewportOffsetY(viewportOffsetY * surfC[i]->GetAAHeightScale());
        }
        surfC[i]->SetWidth(surfC[i]->GetWidth() * surfC[i]->GetAAWidthScale());
        surfC[i]->SetHeight(surfC[i]->GetHeight() * surfC[i]->GetAAHeightScale());
        surfC[i]->SetWindowOffsetX(m_UnscaledWOffsetX * surfC[i]->GetAAWidthScale());
        surfC[i]->SetWindowOffsetY(m_UnscaledWOffsetY * surfC[i]->GetAAHeightScale());
        surfC[i]->SetExtraWidth(extraWidth * surfC[i]->GetAAWidthScale());
        surfC[i]->SetExtraHeight(extraHeight * surfC[i]->GetAAHeightScale());
    }

    if (tparams->ParamPresent("-inflate_rendertarget_and_offset_viewport") > 0)
    {
        surfZ->SetViewportOffsetExist(true);
        surfZ->SetViewportOffsetX(viewportOffsetX * surfZ->GetAAWidthScale());
        surfZ->SetViewportOffsetY(viewportOffsetY * surfZ->GetAAHeightScale());
    }
    surfZ->SetWidth(surfZ->GetWidth() * surfZ->GetAAWidthScale());
    surfZ->SetHeight(surfZ->GetHeight() * surfZ->GetAAHeightScale());
    surfZ->SetWindowOffsetX(m_UnscaledWOffsetX * surfZ->GetAAWidthScale());
    surfZ->SetWindowOffsetY(m_UnscaledWOffsetY * surfZ->GetAAHeightScale());
    surfZ->SetExtraWidth(extraWidth * surfZ->GetAAWidthScale());
    surfZ->SetExtraHeight(extraHeight * surfZ->GetAAHeightScale());

    if (surface_pitch_typeZ == Surface::Pitch)
    {
        // Round up to next legal pitch (multiple of 64B)
        UINT32 Pitch;

        if (surfZ->GetCreatedFromAllocSurface())
        {
            Pitch = surfZ->GetPitch();
        }
        else
        {
            Pitch = (surfZ->GetWidth() + surfZ->GetExtraWidth()) * surfZ->GetBytesPerPixel();
        }

        Pitch = (Pitch + 0x3F) & ~0x3F;
        surfZ->SetPitch(Pitch);
    }

    // Check for user override to pitch settings
    for (i = 0; i < MAX_RENDER_TARGETS; i++)
    {
        // Set the layout value for this specific surface to the default
        // for now.  It may change later.
        Surface2D::Layout layout;
        UINT32 default_pitch = (surfC[i]->GetWidth() + surfC[i]->GetExtraWidth()) *
            surfC[i]->GetBytesPerPixel();

        // Override default surface layout from ALLOC_SURFACE
        if (surfC[i]->GetCreatedFromAllocSurface()
            && (surface_pitch_typeC == default_surface_pitch_typeC))
        {
            layout = surfC[i]->GetLayout();
        }
        else
        {
            layout = surface_pitch_typeC;
        }

        // Bug 434967, fermi need 128b align in pitch
        default_pitch = (default_pitch + 0x7F) & ~0x7F;

        if (tparams->ParamPresent(PitchArgNames[i]) > 0)
        {
            if (layout != Surface2D::Pitch)
            {
                // The pitch arguments for specific color buffers now change
                // the layout type to Pitch.  (See bug 379135)
                layout = Surface2D::Pitch;

                DebugPrintf("Layout of color surface %u is now pitch\n", i);
            }

            UINT32 pitch = tparams->ParamUnsigned(PitchArgNames[i]);

            // A pitch argument of Zero is considered to indicate that
            // the default pitch width should be used.  (See bug 379135)
            if (pitch == 0)
            {
                pitch = default_pitch;
            }

            // bug# 334491, make sure pitch >= surface width * bytesperpixel
            else if (pitch < default_pitch)
            {
                ErrPrintf("Invalid %s argument: %d\n", PitchArgNames[i],
                    pitch);

                ErrPrintf("Pitch must be at least width(%d) x bpp(%d) for pitch surfaces\n",
                    surfC[i]->GetWidth() + surfC[i]->GetExtraWidth(),
                    surfC[i]->GetBytesPerPixel());

                return RC::BAD_PARAMETER;
            }

            surfC[i]->SetPitch(pitch);
        }
        else if (layout == Surface2D::Pitch)
        {
            if (surfC[i]->GetCreatedFromAllocSurface())
            {
                // If this surface was created from an ALLOC_SURFACE command
                // and the pitch wasn't set, use the default pitch.
                if (surfC[i]->GetPitch() == 0)
                {
                    surfC[i]->SetPitch(default_pitch);
                }

                // Otherwise, make sure that the pitch that was set
                // meets the minimum requirements.
                else if (surfC[i]->GetPitch() < default_pitch)
                {
                    ErrPrintf("Invalid PITCH value %d for ALLOC_SURFACE buffer\n", surfC[i]->GetPitch());

                    ErrPrintf("Pitch must be at least width(%d) x bpp(%d) for pitch surfaces\n",
                              surfC[i]->GetWidth() + surfC[i]->GetExtraWidth(),
                              surfC[i]->GetBytesPerPixel());

                    return RC::BAD_PARAMETER;
                }
            }
            else
            {
                surfC[i]->SetPitch(default_pitch);
            }
        }

        surfC[i]->SetLayout(layout);
    }

    // Check for user override to array pitches
    for (i = 0; i < MAX_RENDER_TARGETS; i++)
    {
        UINT64 pitch;

        if (tparams->ParamPresent(ArrayPitchArgNames[i]))
        {
            pitch = tparams->ParamUnsigned(ArrayPitchArgNames[i]);
        }
        else
        {
            pitch = surfC[i]->GetArrayPitch();
        }
        // Fermi reqires array pitch to be 512B aligned
        pitch = (pitch + 0x1ffull) & ~0x1ffull;
        surfC[i]->SetArrayPitchAlignment(512);
        surfC[i]->SetArrayPitch(pitch);
    }
    UINT64 pitch = tparams->ParamUnsigned("-array_pitchZ");
    if (tparams->ParamPresent("-array_pitchZ"))
    {
        pitch = tparams->ParamUnsigned("-array_pitchZ");
    }
    else
    {
        pitch = surfZ->GetArrayPitch();
    }
    // Fermi reqires array pitch to be 512B aligned
    pitch = (pitch + 0x1ffull) & ~0x1ffull;
    surfZ->SetArrayPitchAlignment(512);
    surfZ->SetArrayPitch(pitch);

    for (i = 0; i < MAX_RENDER_TARGETS; i++)
    {
        if ((surfC[i]->GetLocation() == Memory::Coherent)
            || (surfC[i]->GetLocation() == Memory::NonCoherent))
        {
            if (tparams->ParamPresent("-force_cont") > 0)
            {
                // Force contiguous attribute to true for sysmem allocation on
                // hardware platform, when the buffer size is bigger than 1M.
                // This can reduce the allocation time and avoid remap failure
                // on silicon.
                surfC[i]->SetPhysContig(true);
            }
        }
    }
    if ((surfZ->GetLocation() == Memory::Coherent)
        || (surfZ->GetLocation() == Memory::NonCoherent))
    {
        if (tparams->ParamPresent("-force_cont") > 0)
        {
            surfZ->SetPhysContig(true);
        }
    }

    // Memory alignment parameters
    for (i = 0; i < MAX_RENDER_TARGETS; i++)
    {
        if ( !surfC[i]->GetSplit() )
        {
            if (tparams->ParamPresent(PhysAlignArgNames[i]))
                surfC[i]->SetAlignment(tparams->ParamUnsigned(PhysAlignArgNames[i]));
            else if (tparams->ParamPresent("-phys_alignC"))
                surfC[i]->SetAlignment(tparams->ParamUnsigned("-phys_alignC"));
            else if (!surfC[i]->GetCreatedFromAllocSurface())
                surfC[i]->SetAlignment(0);

            if (tparams->ParamPresent(OffsetArgNames[i]))
                surfC[i]->SetExtraAllocSize(tparams->ParamUnsigned64(OffsetArgNames[i]));
            else
                surfC[i]->SetExtraAllocSize(tparams->ParamUnsigned64("-offsetC", 0));
            if (tparams->ParamPresent(AdjustArgNames[i]))
                surfC[i]->SetHiddenAllocSize(tparams->ParamUnsigned64(AdjustArgNames[i]));
            else
                surfC[i]->SetHiddenAllocSize(tparams->ParamUnsigned64("-adjustC", 0));
            if (tparams->ParamPresent(LimitArgNames[i]))
                surfC[i]->SetLimit(tparams->ParamSigned(LimitArgNames[i]));
            else
                surfC[i]->SetLimit(tparams->ParamSigned("-limitC", -1));
            if (tparams->ParamPresent(VirtAddrHintArgNames[i]))
            {
                surfC[i]->SetFixedVirtAddr(tparams->ParamUnsigned64(VirtAddrHintArgNames[i]));
            }
            else if (tparams->ParamPresent("-virt_addr_hintC"))
            {
                surfC[i]->SetFixedVirtAddr(tparams->ParamUnsigned64("-virt_addr_hintC"));
                if (surfC[i]->GetFixedVirtAddr() > 0)
                {
                    m_AllColorsFixed = true;
                }
            }

            if (tparams->ParamPresent(PhysAddrHintArgNames[i]))
            {
                surfC[i]->SetFixedPhysAddr(tparams->ParamUnsigned64(PhysAddrHintArgNames[i]));
            }
            else if (tparams->ParamPresent("-phys_addr_hintC"))
            {
                surfC[i]->SetFixedPhysAddr(tparams->ParamUnsigned64("-phys_addr_hintC"));
                m_AllColorsFixed = true;
            }
            else if (tparams->ParamPresent(PhysAddrRangeArgNames[i]))
            {
                surfC[i]->SetPhysAddrRange(
                    tparams->ParamNUnsigned64(PhysAddrRangeArgNames[i], 0),
                    tparams->ParamNUnsigned64(PhysAddrRangeArgNames[i], 1));
            }
            else if (tparams->ParamPresent("-phys_addr_rangeC"))
            {
                surfC[i]->SetPhysAddrRange(
                    tparams->ParamNUnsigned64("-phys_addr_rangeC", 0),
                    tparams->ParamNUnsigned64("-phys_addr_rangeC", 1));
            }
        }
        else
        {
            InfoPrintf("Color Surface %d is split; ignoring phys_align, offset, adjust and limit\n", i);
            surfC[i]->SetAlignment(0);
            surfC[i]->SetExtraAllocSize(0);
            surfC[i]->SetHiddenAllocSize(0);
            surfC[i]->SetLimit(-1);
        }
    }
    if ( !surfZ->GetSplit() )
    {
        if (tparams->ParamPresent("-phys_alignZ"))
        {
            surfZ->SetAlignment(tparams->ParamUnsigned("-phys_alignZ"));
        }
        else if (!surfZ->GetCreatedFromAllocSurface())
        {
            surfZ->SetAlignment(0);
        }

        surfZ->SetAlignment(tparams->ParamUnsigned("-phys_alignZ", 0));
        surfZ->SetExtraAllocSize(tparams->ParamUnsigned64("-offsetZ", 0));
        surfZ->SetHiddenAllocSize(tparams->ParamUnsigned64("-adjustZ", 0));
        surfZ->SetLimit(tparams->ParamSigned("-limitZ", -1));

        if (tparams->ParamPresent("-virt_addr_hintZ"))
        {
            surfZ->SetFixedVirtAddr(tparams->ParamUnsigned64("-virt_addr_hintZ"));
        }

        if (tparams->ParamPresent("-phys_addr_hintZ"))
        {
            surfZ->SetFixedPhysAddr(tparams->ParamUnsigned64("-phys_addr_hintZ"));
        }
        else if (tparams->ParamPresent("-phys_addr_rangeZ"))
        {
            surfZ->SetPhysAddrRange(
                tparams->ParamNUnsigned64("-phys_addr_rangeZ", 0),
                tparams->ParamNUnsigned64("-phys_addr_rangeZ", 1));
        }
    }
    else
    {
        InfoPrintf("Z Surface is split; ignoring phys_align, offset, adjust and limit\n");
        surfZ->SetAlignment(0);
        surfZ->SetExtraAllocSize(0);
        surfZ->SetHiddenAllocSize(0);
        surfZ->SetLimit(-1);
    }

    if (tparams->ParamPresent("-compress") ||
        tparams->ParamPresent("-compressZ") ||
        tparams->ParamPresent("-compress_stencil"))
    {
        surfZ->SetCompressed(true);

        // There are two different arguments that control compression
        // in the surface manager (-compress and -compress_mode) but
        // ALLOC_SURFACE only has one property to control compression
        // (COMPRESSION). To prevent inconsistencies, we'll override
        // the compressed flag from ALLOC_SURFACE if the -compress
        // argument is set.
        if (surfZ->GetCreatedFromAllocSurface())
        {
            surfZ->SetCompressedFlag(LWOS32_ATTR_COMPR_REQUIRED);
        }

        // If the -compress_stencil argument is specified, the Z buffer format must be S8 (stencil-only format).
        if (tparams->ParamPresent("-compress_stencil") &&
            (surfZ->GetColorFormat() != ColorUtils::S8))
        {
            ErrPrintf("Argument -compress_stencil is specified but the Z buffer format is not S8.\n");
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
        else if (tparams->ParamPresent("-compressZ") &&
            (surfZ->GetColorFormat() == ColorUtils::S8))
        {
            ErrPrintf("Argument -compressZ is specified but the Z buffer format is S8.\n");
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
    }
    else if (!surfZ->GetCreatedFromAllocSurface())
    {
        surfZ->SetCompressed(false);
    }

    surfZ->SetComptagStart(tparams->ParamUnsigned("-comptag_startZ", 0));
    const UINT32 percentage = tparams->ParamUnsigned("-comptag_covgZ", 100);
    surfZ->SetComptagCovMin(percentage);
    surfZ->SetComptagCovMax(percentage);
    surfZ->SetComptags(tparams->ParamUnsigned("-comptagsZ", 0));

    if (tparams->ParamPresent("-zbc_modeZ"))
    {
        if (tparams->ParamUnsigned("-zbc_modeZ") > 0)
        {
            surfZ->SetZbcMode(Surface2D::ZbcOn);
        }
        else
        {
            surfZ->SetZbcMode(Surface2D::ZbcOff);
        }
    }
    else if (tparams->ParamPresent("-zbc_mode"))
    {
        if (tparams->ParamUnsigned("-zbc_mode") > 0)
        {
            surfZ->SetZbcMode(Surface2D::ZbcOn);
        }
        else
        {
            surfZ->SetZbcMode(Surface2D::ZbcOff);
        }
    }

    if (tparams->ParamPresent("-gpu_cache_modeZ"))
    {
        if (tparams->ParamUnsigned("-gpu_cache_modeZ") > 0)
        {
            surfZ->SetGpuCacheMode(Surface2D::GpuCacheOn);
        }
        else
        {
            surfZ->SetGpuCacheMode(Surface2D::GpuCacheOff);
        }
    }
    else if (tparams->ParamPresent("-gpu_cache_mode"))
    {
        if (tparams->ParamUnsigned("-gpu_cache_mode") > 0)
        {
            surfZ->SetGpuCacheMode(Surface2D::GpuCacheOn);
        }
        else
        {
            surfZ->SetGpuCacheMode(Surface2D::GpuCacheOff);
        }
    }

    if (tparams->ParamPresent("-gpu_p2p_cache_modeZ"))
    {
        if (tparams->ParamUnsigned("-gpu_p2p_cache_modeZ") > 0)
        {
            surfZ->SetP2PGpuCacheMode(Surface2D::GpuCacheOn);
        }
        else
        {
            surfZ->SetP2PGpuCacheMode(Surface2D::GpuCacheOff);
        }
    }
    else if (tparams->ParamPresent("-gpu_p2p_cache_mode"))
    {
        if (tparams->ParamUnsigned("-gpu_p2p_cache_mode") > 0)
        {
            surfZ->SetP2PGpuCacheMode(Surface2D::GpuCacheOn);
        }
        else
        {
            surfZ->SetP2PGpuCacheMode(Surface2D::GpuCacheOff);
        }
    }

    if (tparams->ParamPresent("-split_gpu_cache_modeZ"))
    {
        if (tparams->ParamUnsigned("-split_gpu_cache_modeZ") > 0)
        {
            surfZ->SetSplitGpuCacheMode(Surface2D::GpuCacheOn);
        }
        else
        {
            surfZ->SetSplitGpuCacheMode(Surface2D::GpuCacheOff);
        }
    }
    else if (tparams->ParamPresent("-split_gpu_cache_mode"))
    {
        if (tparams->ParamUnsigned("-split_gpu_cache_mode") > 0)
        {
            surfZ->SetSplitGpuCacheMode(Surface2D::GpuCacheOn);
        }
        else
        {
            surfZ->SetSplitGpuCacheMode(Surface2D::GpuCacheOff);
        }
    }

    for (i = 0; i < MAX_RENDER_TARGETS; i++)
    {
        if (tparams->ParamPresent("-compress") ||
            tparams->ParamPresent("-compressC") ||
            tparams->ParamPresent(ColorCompressArgNames[i]))
        {
            surfC[i]->SetCompressed(true);

            // There are two different arguments that control compression
            // in the surface manager (-compress and -compress_mode) but
            // ALLOC_SURFACE only has one property to control compression
            // (COMPRESSION). To prevent inconsistencies, we'll override
            // the compressed flag from ALLOC_SURFACE if the -compress
            // argument is set.
            if (surfC[i]->GetCreatedFromAllocSurface())
            {
                surfC[i]->SetCompressedFlag(LWOS32_ATTR_COMPR_REQUIRED);
            }
        }
        else if (!surfC[i]->GetCreatedFromAllocSurface())
        {
            surfC[i]->SetCompressed(false);
        }

        if (tparams->ParamPresent(ComptagStartArgNames[i]))
            surfC[i]->SetComptagStart(tparams->ParamUnsigned(ComptagStartArgNames[i]));
        else
            surfC[i]->SetComptagStart(tparams->ParamUnsigned("-comptag_startC", 0));
        UINT32 percentage;
        if (tparams->ParamPresent(ComptagCovgArgNames[i]))
            percentage = tparams->ParamUnsigned(ComptagCovgArgNames[i]);
        else
            percentage = tparams->ParamUnsigned("-comptag_covgC", 100);
        surfC[i]->SetComptagCovMin(percentage);
        surfC[i]->SetComptagCovMax(percentage);

        if (tparams->ParamPresent(ColorZbcModeArgNames[i]))
        {
            if (tparams->ParamUnsigned(ColorZbcModeArgNames[i]) > 0)
            {
                surfC[i]->SetZbcMode(Surface2D::ZbcOn);
            }
            else
            {
                surfC[i]->SetZbcMode(Surface2D::ZbcOff);
            }
        }
        else if (tparams->ParamPresent("-zbc_modeC"))
        {
            if (tparams->ParamUnsigned("-zbc_modeC") > 0)
            {
                surfC[i]->SetZbcMode(Surface2D::ZbcOn);
            }
            else
            {
                surfC[i]->SetZbcMode(Surface2D::ZbcOff);
            }
        }
        else if (tparams->ParamPresent("-zbc_mode"))
        {
            if (tparams->ParamUnsigned("-zbc_mode") > 0)
            {
                surfC[i]->SetZbcMode(Surface2D::ZbcOn);
            }
            else
            {
                surfC[i]->SetZbcMode(Surface2D::ZbcOff);
            }
        }

        if (tparams->ParamPresent(ColorGpuCacheModeArgNames[i]))
        {
            if (tparams->ParamUnsigned(ColorGpuCacheModeArgNames[i]) > 0)
            {
                surfC[i]->SetGpuCacheMode(Surface2D::GpuCacheOn);
            }
            else
            {
                surfC[i]->SetGpuCacheMode(Surface2D::GpuCacheOff);
            }
        }
        else if (tparams->ParamPresent("-gpu_cache_modeC"))
        {
            if (tparams->ParamUnsigned("-gpu_cache_modeC") > 0)
            {
                surfC[i]->SetGpuCacheMode(Surface2D::GpuCacheOn);
            }
            else
            {
                surfC[i]->SetGpuCacheMode(Surface2D::GpuCacheOff);
            }
        }
        else if (tparams->ParamPresent("-gpu_cache_mode"))
        {
            if (tparams->ParamUnsigned("-gpu_cache_mode") > 0)
            {
                surfC[i]->SetGpuCacheMode(Surface2D::GpuCacheOn);
            }
            else
            {
                surfC[i]->SetGpuCacheMode(Surface2D::GpuCacheOff);
            }
        }

        if (tparams->ParamPresent(ColorP2PGpuCacheModeArgNames[i]))
        {
            if (tparams->ParamUnsigned(ColorP2PGpuCacheModeArgNames[i]) > 0)
            {
                surfC[i]->SetP2PGpuCacheMode(Surface2D::GpuCacheOn);
            }
            else
            {
                surfC[i]->SetP2PGpuCacheMode(Surface2D::GpuCacheOff);
            }
        }
        else if (tparams->ParamPresent("-gpu_p2p_cache_modeC"))
        {
            if (tparams->ParamUnsigned("-gpu_p2p_cache_modeC") > 0)
            {
                surfC[i]->SetP2PGpuCacheMode(Surface2D::GpuCacheOn);
            }
            else
            {
                surfC[i]->SetP2PGpuCacheMode(Surface2D::GpuCacheOff);
            }
        }
        else if (tparams->ParamPresent("-gpu_p2p_cache_mode"))
        {
            if (tparams->ParamUnsigned("-gpu_p2p_cache_mode") > 0)
            {
                surfC[i]->SetP2PGpuCacheMode(Surface2D::GpuCacheOn);
            }
            else
            {
                surfC[i]->SetP2PGpuCacheMode(Surface2D::GpuCacheOff);
            }
        }

        if (tparams->ParamPresent(ColorSplitGpuCacheModeArgNames[i]))
        {
            if (tparams->ParamUnsigned(ColorSplitGpuCacheModeArgNames[i]) > 0)
            {
                surfC[i]->SetSplitGpuCacheMode(Surface2D::GpuCacheOn);
            }
            else
            {
                surfC[i]->SetSplitGpuCacheMode(Surface2D::GpuCacheOff);
            }
        }
        else if (tparams->ParamPresent("-split_gpu_cache_modeC"))
        {
            if (tparams->ParamUnsigned("-split_gpu_cache_modeC") > 0)
            {
                surfC[i]->SetSplitGpuCacheMode(Surface2D::GpuCacheOn);
            }
            else
            {
                surfC[i]->SetSplitGpuCacheMode(Surface2D::GpuCacheOff);
            }
        }
        else if (tparams->ParamPresent("-split_gpu_cache_mode"))
        {
            if (tparams->ParamUnsigned("-split_gpu_cache_mode") > 0)
            {
                surfC[i]->SetSplitGpuCacheMode(Surface2D::GpuCacheOn);
            }
            else
            {
                surfC[i]->SetSplitGpuCacheMode(Surface2D::GpuCacheOff);
            }
        }
    }

    if (tparams->ParamPresent("-mscontrol"))
    {
        switch (tparams->ParamUnsigned("-mscontrol"))
        {
            case 0: // -MSdisable
                m_MultiSampleOverride = true;
                m_MultiSampleOverrideValue = 0 ;
                break;
            case 1: // -MSenable
                m_MultiSampleOverride = true;
                m_MultiSampleOverrideValue = 1;
                break;
            case 2: // -MSfreedom
                m_MultiSampleOverride = false;
                break;
            case 3: // -MSdisableZ
                m_MultiSampleOverride = true;
                m_MultiSampleOverrideValue = 0 ;
                m_NonMultisampledZOverride = true;
                break;
            case 4: // -MSenable_disableZ
                m_MultiSampleOverride = true;
                m_MultiSampleOverrideValue = 1;
                m_NonMultisampledZOverride = true;
                break;
            default:
                MASSERT(!"Invalid -mscontrol setting");
        }
    }

    // Look for overrides of the PTE kind
    const char *PteKindName;
    PteKindName = tparams->ParamStr("-pte_kindZ");
    if (PteKindName)
    {
        UINT32 PteKind;
        if (lwgpu->GetGpuSubdevice(subdev)->GetPteKindFromName(PteKindName, &PteKind))
        {
            surfZ->SetPteKind(PteKind);
        }
        else
        {
            ErrPrintf("Invalid PTE kind name: %s\n", PteKindName);
            return RC::BAD_PARAMETER;
        }
    }
    PteKindName = tparams->ParamStr("-split_pte_kindZ");
    if (PteKindName)
    {
        UINT32 PteKind;
        if (lwgpu->GetGpuSubdevice(subdev)->GetPteKindFromName(PteKindName, &PteKind))
        {
            surfZ->SetSplitPteKind(PteKind);
        }
        else
        {
            ErrPrintf("Invalid split PTE kind name: %s\n", PteKindName);
            return RC::BAD_PARAMETER;
        }
    }
    for (i = 0; i < MAX_RENDER_TARGETS; i++)
    {
        const char *ArgName = PteKindArgNames[i];
        if (tparams->ParamPresent(ArgName))
            PteKindName = tparams->ParamStr(ArgName);
        else
            PteKindName = tparams->ParamStr("-pte_kindC");
        if (PteKindName)
        {
            UINT32 PteKind;
            if (lwgpu->GetGpuSubdevice(subdev)->GetPteKindFromName(PteKindName, &PteKind))
            {
                surfC[i]->SetPteKind(PteKind);
            }
            else
            {
                ErrPrintf("Invalid PTE kind name: %s\n", PteKindName);
                return RC::BAD_PARAMETER;
            }
        }

        ArgName = SplitPteKindArgNames[i];
        if (tparams->ParamPresent(ArgName))
            PteKindName = tparams->ParamStr(ArgName);
        else
            PteKindName = tparams->ParamStr("-split_pte_kindC");
        if (PteKindName)
        {
            UINT32 PteKind;
            if (lwgpu->GetGpuSubdevice(subdev)->GetPteKindFromName(PteKindName, &PteKind))
            {
                surfC[i]->SetSplitPteKind(PteKind);
            }
            else
            {
                ErrPrintf("Invalid split PTE kind name: %s\n", PteKindName);
                return RC::BAD_PARAMETER;
            }
        }
    }

    for (i = 0; i < MAX_RENDER_TARGETS; i++)
    {
        if (tparams->ParamPresent(PartStrideArgNames[i]))
            surfC[i]->SetPartStride(tparams->ParamUnsigned(PartStrideArgNames[i]));
        else
            surfC[i]->SetPartStride(tparams->ParamUnsigned("-part_strideC", 0));
    }
    surfZ->SetPartStride(tparams->ParamUnsigned("-part_strideZ", 0));

    surfZ->SetLayout(surface_pitch_typeZ);

    m_DoZlwllSanityCheck = (tparams->ParamPresent("-skip_zc_sanity") == 0);
    m_ZLwll.reset(new ZLwll);

    m_ClipBlockHeight =  tparams->ParamUnsigned("-block_height_wid", 16);

    // Set ctagOffset for color & z buffers
    for (i=0; i<MAX_RENDER_TARGETS; i++)
    {
        if (tparams->ParamPresent(ComptagOffsetArgNames[i]) > 0)
            surfC[i]->SetCtagOffset(DRF_NUM(OS32, _ALLOC, _COMPTAG_OFFSET_START, tparams->ParamUnsigned(ComptagOffsetArgNames[i]))
                                  | DRF_DEF(OS32, _ALLOC, _COMPTAG_OFFSET_USAGE, _FIXED));
        else if (tparams->ParamPresent("-comptag_offset_min") > 0)
            surfC[i]->SetCtagOffset(DRF_NUM(OS32, _ALLOC, _COMPTAG_OFFSET_START, tparams->ParamUnsigned("-comptag_offset_min", 0))
                                  | DRF_DEF(OS32, _ALLOC, _COMPTAG_OFFSET_USAGE, _MIN));
        else
            surfC[i]->SetCtagOffset(DRF_NUM(OS32, _ALLOC, _COMPTAG_OFFSET_START, 0)
                                  | DRF_DEF(OS32, _ALLOC, _COMPTAG_OFFSET_USAGE, _OFF));
    }
    if (tparams->ParamPresent("-comptag_offsetZ") > 0)
        surfZ->SetCtagOffset(DRF_NUM(OS32, _ALLOC, _COMPTAG_OFFSET_START, tparams->ParamUnsigned("-comptag_offsetZ"))
                           | DRF_DEF(OS32, _ALLOC, _COMPTAG_OFFSET_USAGE, _FIXED));
    else if (tparams->ParamPresent("-comptag_offset_min") > 0)
        surfZ->SetCtagOffset(DRF_NUM(OS32, _ALLOC, _COMPTAG_OFFSET_START, tparams->ParamUnsigned("-comptag_offset_min", 0))
                           | DRF_DEF(OS32, _ALLOC, _COMPTAG_OFFSET_USAGE, _MIN));
    else
        surfZ->SetCtagOffset(DRF_NUM(OS32, _ALLOC, _COMPTAG_OFFSET_START, 0)
                           | DRF_DEF(OS32, _ALLOC, _COMPTAG_OFFSET_USAGE, _OFF));

    m_IsSliSurfClip = tparams->ParamPresent("-sli_surfaceclip") > 0;

    // Set memory mapping mode.

    for (i = 0; i < MAX_RENDER_TARGETS; ++i)
    {
        if (tparams->ParamPresent(MemoryMappingArgNames[i]) > 0)
        {
            if (tparams->ParamUnsigned(MemoryMappingArgNames[i]) > 0)
            {
                surfC[i]->SetMemoryMappingMode(Surface2D::MapReflected);
            }
            else
            {
                surfC[i]->SetMemoryMappingMode(Surface2D::MapDirect);
            }
        }
        else if (tparams->ParamPresent("-map_mode") > 0)
        {
            if (tparams->ParamUnsigned("-map_mode") > 0)
            {
                surfC[i]->SetMemoryMappingMode(Surface2D::MapReflected);
            }
            else
            {
                surfC[i]->SetMemoryMappingMode(Surface2D::MapDirect);
            }
        }
    }

    if (tparams->ParamPresent("-map_modeZ") > 0)
    {
        if (tparams->ParamUnsigned("-map_modeZ") > 0)
        {
            surfZ->SetMemoryMappingMode(Surface2D::MapReflected);
        }
        else
        {
            surfZ->SetMemoryMappingMode(Surface2D::MapDirect);
        }
    }
    else if (tparams->ParamPresent("-map_mode") > 0)
    {
        if (tparams->ParamUnsigned("-map_mode") > 0)
        {
            surfZ->SetMemoryMappingMode(Surface2D::MapReflected);
        }
        else
        {
            surfZ->SetMemoryMappingMode(Surface2D::MapDirect);
        }
    }

    if (tparams->ParamPresent("-inflate_rendertarget_and_offset_window") > 0 || tparams->ParamPresent("-inflate_rendertarget_and_offset_viewport") > 0)
    {
        // The full width and height represent what the allocated dimensions
        // of the image should be.
        UINT32 fullWidth = 0;
        UINT32 fullHeight = 0;

        // The offsets from the command-line argument represent the distance
        // from the lower-right corner of the drawn image to the lower-right
        // corner of the allocated image.
        UINT32 offsetX = 0;
        UINT32 offsetY = 0;
        if(tparams->ParamPresent("-inflate_rendertarget_and_offset_window") > 0)
        {
            fullWidth = tparams->ParamNUnsigned("-inflate_rendertarget_and_offset_window", 0);
            fullHeight = tparams->ParamNUnsigned("-inflate_rendertarget_and_offset_window", 1);
            offsetX = tparams->ParamNUnsigned("-inflate_rendertarget_and_offset_window", 2);
            offsetY = tparams->ParamNUnsigned("-inflate_rendertarget_and_offset_window", 3);
        }
        else
        {
            fullWidth = tparams->ParamNUnsigned("-inflate_rendertarget_and_offset_viewport", 0);
            fullHeight = tparams->ParamNUnsigned("-inflate_rendertarget_and_offset_viewport", 1);
            offsetX = tparams->ParamNUnsigned("-inflate_rendertarget_and_offset_viewport", 2);
            offsetY = tparams->ParamNUnsigned("-inflate_rendertarget_and_offset_viewport", 3);
        }
        unique_ptr<PartialMapData> mapData;

        for (i = 0; i < MAX_RENDER_TARGETS; i++)
        {
            mapData.reset(new PartialMapData);
            if(tparams->ParamPresent("-inflate_rendertarget_and_offset_viewport"))
            {
                mapData->x1 = fullWidth - surfC[i]->GetWidth() - offsetX;
                mapData->y1 = fullHeight - surfC[i]->GetHeight() - offsetY;
            }
            else
            {
                mapData->x1 = m_UnscaledWOffsetX;
                mapData->y1 = m_UnscaledWOffsetY;
            }
            mapData->x2 = fullWidth - offsetX;
            mapData->y2 = fullHeight - offsetY;
            mapData->arrayIndex = 0;

            surfDataC[i]->partialMaps.push_back(mapData.release());
            surfC[i]->SetSpecialization(Surface2D::VirtualOnly);
            surfC[i]->SetIsSparse(true);
        }

        mapData.reset(new PartialMapData);

        if(tparams->ParamPresent("-inflate_rendertarget_and_offset_viewport"))
        {
            mapData->x1 = fullWidth - surfZ->GetWidth() - offsetX;
            mapData->y1 = fullHeight - surfZ->GetHeight() - offsetY;
        }
        else
        {
            mapData->x1 = m_UnscaledWOffsetX;
            mapData->y1 = m_UnscaledWOffsetY;
        }
        mapData->x2 = fullWidth - offsetX;
        mapData->y2 = fullHeight - offsetY;
        mapData->arrayIndex = 0;

        surfDataZ->partialMaps.push_back(mapData.release());
        surfZ->SetSpecialization(Surface2D::VirtualOnly);
        surfZ->SetIsSparse(true);
    }

    // Check for any -map_region command-line arguments.  These arguments
    // specify that a given color/z surface is to be partially mapped
    // during rendering time (though fully mapped during initialization
    // and also at CRC checking/image dumping.)

    bool partialMap;
    const char *argName = 0;

    for (i = 0; i < MAX_RENDER_TARGETS; i++)
    {
        partialMap = false;

        if (tparams->ParamPresent(MapRegionArgNames[i]) > 0)
        {
            argName = MapRegionArgNames[i];
            partialMap = true;
        }
        else if (tparams->ParamPresent("-map_regionC") > 0)
        {
            argName = "-map_regionC";
            partialMap = true;
        }
        else if (tparams->ParamPresent("-map_region") > 0)
        {
            argName = "-map_region";
            partialMap = true;
        }

        if (partialMap)
        {
            for (UINT32 argIndex = 0;
                 argIndex < tparams->ParamPresent(argName);
                 ++argIndex)
            {
                unique_ptr<PartialMapData> mapData(new PartialMapData);

                // The first four arguments specify a rectangle on the surface.
                // The last argument specifies an array index.
                mapData->x1 = tparams->ParamNUnsigned(argName, argIndex, 0);
                mapData->y1 = tparams->ParamNUnsigned(argName, argIndex, 1);
                mapData->x2 = tparams->ParamNUnsigned(argName, argIndex, 2);
                mapData->y2 = tparams->ParamNUnsigned(argName, argIndex, 3);
                mapData->arrayIndex = tparams->ParamNUnsigned(argName, argIndex, 4);

                if ((mapData->x1 >= mapData->x2) ||
                    (mapData->y1 >= mapData->y2))
                {
                    ErrPrintf("%s coordinates (%u %u) (%u %u) do not form a rectangle.\n",
                        argName, mapData->x1, mapData->y1,
                        mapData->x2, mapData->y2);

                    return RC::BAD_PARAMETER;
                }

                surfDataC[i]->partialMaps.push_back(mapData.release());
            }

            surfC[i]->SetSpecialization(Surface2D::VirtualOnly);
            surfC[i]->SetIsSparse(true);
        }
    }

    partialMap = false;

    if (tparams->ParamPresent("-map_regionZ") > 0)
    {
        argName = "-map_regionZ";
        partialMap = true;
    }
    else if (tparams->ParamPresent("-map_region") > 0)
    {
        argName = "-map_region";
        partialMap = true;
    }

    if (partialMap)
    {
        for (UINT32 argIndex = 0;
             argIndex < tparams->ParamPresent(argName);
             ++argIndex)
        {
            unique_ptr<PartialMapData> mapData(new PartialMapData());

            // The first four arguments specify a rectangle on the surface.
            // The last argument specifies an array index.
            mapData->x1 = tparams->ParamNUnsigned(argName, argIndex, 0);
            mapData->y1 = tparams->ParamNUnsigned(argName, argIndex, 1);
            mapData->x2 = tparams->ParamNUnsigned(argName, argIndex, 2);
            mapData->y2 = tparams->ParamNUnsigned(argName, argIndex, 3);
            mapData->arrayIndex = tparams->ParamNUnsigned(argName, argIndex, 4);

            if ((mapData->x1 >= mapData->x2) ||
                (mapData->y1 >= mapData->y2))
            {
                ErrPrintf("%s coordinates (%u %u) (%u %u) do not form a rectangle.\n",
                    argName, mapData->x1, mapData->y1,
                    mapData->x2, mapData->y2);

                return RC::BAD_PARAMETER;
            }

            surfDataZ->partialMaps.push_back(mapData.release());
        }

        surfZ->SetSpecialization(Surface2D::VirtualOnly);
        surfZ->SetIsSparse(true);
    }

    for (i = 0; i < MAX_RENDER_TARGETS; i++)
    {
        const char *argValue = 0;

        if (tparams->ParamPresent(VprArgNames[i]))
        {
            argValue = tparams->ParamStr(VprArgNames[i]);
        }
        else if (tparams->ParamPresent("-vprC"))
        {
            argValue = tparams->ParamStr("-vprC");
        }

        if (argValue != 0)
        {
            if (strcmp(argValue, "ON") == 0)
            {
                surfC[i]->SetVideoProtected(true);
            }
            else if (strcmp(argValue, "OFF") == 0)
            {
                surfC[i]->SetVideoProtected(false);
            }
            else
            {
                ErrPrintf("Illegal -vprC* value '%s'\n", argValue);
                return RC::BAD_COMMAND_LINE_ARGUMENT;
            }
        }
    }

    if (tparams->ParamPresent("-vprZ"))
    {
        const char *argValue = tparams->ParamStr("-vprZ");

        if (strcmp(argValue, "ON") == 0)
        {
            surfZ->SetVideoProtected(true);
        }
        else if (strcmp(argValue, "OFF") == 0)
        {
            surfZ->SetVideoProtected(false);
        }
        else
        {
            ErrPrintf("Illegal -vprZ value '%s'\n", argValue);
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
    }

    for (i = 0; i < MAX_RENDER_TARGETS; i++)
    {
        const char *argValue = 0;

        if (tparams->ParamPresent(Acr1ArgNames[i]))
        {
            argValue = tparams->ParamStr(Acr1ArgNames[i]);
        }
        else if (tparams->ParamPresent("-acr1C"))
        {
            argValue = tparams->ParamStr("-acr1C");
        }

        if (argValue != 0)
        {
            if (strcmp(argValue, "ON") == 0)
            {
                surfC[i]->SetAcrRegion1(true);
            }
            else if (strcmp(argValue, "OFF") == 0)
            {
                surfC[i]->SetAcrRegion1(false);
            }
            else
            {
                ErrPrintf("Illegal -acr1C* value '%s'\n", argValue);
                return RC::BAD_COMMAND_LINE_ARGUMENT;
            }
        }
    }

    if (tparams->ParamPresent("-acr1Z"))
    {
        const char *argValue = tparams->ParamStr("-acr1Z");

        if (strcmp(argValue, "ON") == 0)
        {
            surfZ->SetAcrRegion1(true);
        }
        else if (strcmp(argValue, "OFF") == 0)
        {
            surfZ->SetAcrRegion1(false);
        }
        else
        {
            ErrPrintf("Illegal -acr1Z value '%s'\n", argValue);
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
    }

    for (i = 0; i < MAX_RENDER_TARGETS; i++)
    {
        const char *argValue = 0;

        if (tparams->ParamPresent(Acr2ArgNames[i]))
        {
            argValue = tparams->ParamStr(Acr2ArgNames[i]);
        }
        else if (tparams->ParamPresent("-acr2C"))
        {
            argValue = tparams->ParamStr("-acr2C");
        }

        if (argValue != 0)
        {
            if (strcmp(argValue, "ON") == 0)
            {
                surfC[i]->SetAcrRegion2(true);
            }
            else if (strcmp(argValue, "OFF") == 0)
            {
                surfC[i]->SetAcrRegion2(false);
            }
            else
            {
                ErrPrintf("Illegal -acr2C* value '%s'\n", argValue);
                return RC::BAD_COMMAND_LINE_ARGUMENT;
            }
        }
    }

    if (tparams->ParamPresent("-acr2Z"))
    {
        const char *argValue = tparams->ParamStr("-acr2Z");

        if (strcmp(argValue, "ON") == 0)
        {
            surfZ->SetAcrRegion2(true);
        }
        else if (strcmp(argValue, "OFF") == 0)
        {
            surfZ->SetAcrRegion2(false);
        }
        else
        {
            ErrPrintf("Illegal -acr2Z value '%s'\n", argValue);
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
    }

    if (tparams->ParamPresent("-use_page_poolC"))
    {
        UINT32 argValue = tparams->ParamUnsigned("-use_page_poolC");
        PagePoolUsage pagePoolUsage = NoPagePool;

        switch (argValue)
        {
            case 0:
                pagePoolUsage = SmallPagePool;
                break;
            case 1:
                pagePoolUsage = BigPagePool;
                break;
            default:
                ErrPrintf("Illegal -use_page_poolC value '%u'.\n", argValue);
                return RC::BAD_COMMAND_LINE_ARGUMENT;
        }

        for (i = 0; i < MAX_RENDER_TARGETS; i++)
        {
            surfC[i]->SetSpecialization(Surface2D::VirtualOnly);
            surfDataC[i]->pagePoolUsage = pagePoolUsage;
        }
    }
    else
    {
        for (i = 0; i < MAX_RENDER_TARGETS; i++)
        {
            surfDataC[i]->pagePoolUsage = NoPagePool;
        }
    }

    if (tparams->ParamPresent("-use_page_poolZ"))
    {
        surfZ->SetSpecialization(Surface2D::VirtualOnly);

        UINT32 argValue = tparams->ParamUnsigned("-use_page_poolZ");
        PagePoolUsage pagePoolUsage = NoPagePool;

        switch (argValue)
        {
            case 0:
                pagePoolUsage = SmallPagePool;
                break;
            case 1:
                pagePoolUsage = BigPagePool;
                break;
            default:
                ErrPrintf("Illegal -use_page_poolZ value '%u'.\n", argValue);
                return RC::BAD_COMMAND_LINE_ARGUMENT;
        }

        surfDataZ->pagePoolUsage = pagePoolUsage;
    }
    else
    {
        surfDataZ->pagePoolUsage = NoPagePool;
    }

    UINT64 size = tparams->ParamUnsigned("-small_page_pool_kb", 1024) * 1024U;

    if (size == 0)
    {
        ErrPrintf("-small_page_pool_kb must be greater than zero.\n");
        return RC::BAD_PARAMETER;
    }

    m_SmallPagePoolSize = ALIGN_UP(size, 4096U);

    size = tparams->ParamUnsigned("-big_page_pool_kb", 1024) * 1024U;

    if (size == 0)
    {
        ErrPrintf("-big_page_pool_kb must be greater than zero.\n");
        return RC::BAD_PARAMETER;
    }

    m_BigPageSize = pGpuDev->GetBigPageSize();
    m_BigPagePoolSize = ALIGN_UP(size, m_BigPageSize);

    // Tesla doesn't support pitch mode for Z; always null out the Z buffer
    if (surfZ->GetLayout() == Surface::Pitch)
        SetForceNull(surfZ, true);

    if (tparams->ParamPresent("-prog_zt_as_ct0") > 0)
        m_ProgZtAsCt0 = true;

    if (tparams->ParamPresent("-send_rt_methods") > 0)
        m_SendRTMethods = true;

    return OK;
}

RC LWGpuSurfaceMgr::CheckAAMode
(
    AAmodetype mode,
    const ArgReader *params
)
{
    RC rc;
    if (!ValidAAMode(mode, params))
    {
        ErrPrintf("Invalid AA mode specifed: GPU=0x%08x mode=%s\n",
            lwgpu->GetArchitecture(), AAstrings[mode]);
        return RC::BAD_PARAMETER;
    }
    return rc;
}

bool LWGpuSurfaceMgr::ValidAAMode(UINT32 mode, const ArgReader * tparams)
{
    switch (mode)
    {
        case AAMODE_1X1:
        case AAMODE_2X1_D3D:
        case AAMODE_4X2_D3D:
        case AAMODE_2X1:
        case AAMODE_2X2:
        case AAMODE_4X2:
        case AAMODE_4X4:
            return true;
        case AAMODE_2X2_VC_4:
        case AAMODE_2X2_VC_12:
        case AAMODE_4X2_VC_8:
        case AAMODE_4X2_VC_24:
            if(tparams && (tparams->ParamPresent("-hybrid_skip_crc") > 0))
            {
                ErrPrintf("option '-hybrid_skip_crc' can't be used with VCAA modes. ");
                return false;
            }
            return true;
        default:
            return false;
    }
}

int LWGpuSurfaceMgr::SetupZCompress(MdiagSurf *surf)
{
    if (GetValid(surf) && surf->GetCompressed())
    {
        if (!m_FastClearDepth)
        {
            InfoPrintf("Enabling -gpu_clear to initialize z compression tag bits\n");
            m_FastClearDepth = true;
        }
    }
    return 1;
}

int LWGpuSurfaceMgr::SetupColorCompress(MdiagSurf *surf)
{
    if (GetValid(surf) && surf->GetCompressed())
    {
        if (!m_FastClearColor)
        {
            InfoPrintf("Enabling -gpu_clear to initialize color compression tag bits\n");
            m_FastClearColor = true;
        }
    }

    return 1;
}

RC LWGpuSurfaceMgr::SetupForceVisible()
{
    ForceVisible = true; // remember for the Zlwll sanity check
    // GPUs since Fermi have not implemented SetupForceVisible
    return OK;
}

RC LWGpuSurfaceMgr::Clear2SubDev(const ArgReader *params, GpuVerif* gpuVerif, const RGBAFloat& color, const ZFloat& z, const Stencil& stencil, UINT32 subdev)
{
    if( m_RenderSurfaces.empty() )    // No need to do clear if no surfaces allocated
    {
        return OK;
    }

    return ClearHelper(params, gpuVerif, m_FastClearColor, m_FastClearDepth, color, z, stencil, m_SrgbWrite, subdev);
}

RC LWGpuSurfaceMgr::Clear(const ArgReader *params, GpuVerif* gpuVerif, const RGBAFloat& color, const ZFloat& z, const Stencil& stencil)
{
    RC rc;
    GpuDevice *pGpuDev = lwgpu->GetGpuDevice();

    for (UINT32 subdev = 0; subdev < pGpuDev->GetNumSubdevices(); ++subdev)
    {
        if (pGpuDev->GetNumSubdevices() > 1)
            CHECK_RC(ch->GetModsChannel()->WriteSetSubdevice(1 << subdev));
        CHECK_RC(Clear2SubDev(params, gpuVerif, color, z, stencil, subdev));
        if (pGpuDev->GetNumSubdevices() > 1)
            CHECK_RC(ch->GetModsChannel()->WriteSetSubdevice(Channel::AllSubdevicesMask));
    }
    return rc;
}

Surface::Layout LWGpuSurfaceMgr::GetGlobalSurfaceLayout()
{
    SurfaceIterator sit;
    for (sit = m_RenderSurfaces.begin(); sit != m_RenderSurfaces.end(); ++sit)
    {
        if (IsColorSurface(sit->second.surfaces[0].GetName()) ||
            (sit->second.surfaces[0].GetType() == LWOS32_TYPE_DEPTH))
        {
            return sit->second.surfaces[0].GetLayout();
        }
    }

    ErrPrintf("Called GetGlobalSurfaceLayout with no surfaces available!\n");

    return Surface::Pitch;
}

void LWGpuSurfaceMgr::SetTargetDisplay(UINT32 w, UINT32 h, UINT32 d, UINT32 ArraySize)
{
    m_TargetDisplayWidth = w;
    m_TargetDisplayHeight = h;
    m_DefaultDepth = d;
    m_DefaultArraySize = ArraySize;
}

void LWGpuSurfaceMgr::SetSurfaceNull(SurfaceType surfType, bool setval,
                                     UINT32 subdev /* = Gpu::UNSPECIFIED_SUBDEV */)
{
    MdiagSurf *surf = GetSurface(surfType, subdev);
    if (surf)
        SetForceNull(surf, setval);
    else
        ErrPrintf("Called SetSurfaceNull with an invalid buffer: %d\n", (UINT32)surfType);
}

void LWGpuSurfaceMgr::SetSurfaceUsed(SurfaceType surfType, bool setval,
                                     UINT32 subdev /* = Gpu::UNSPECIFIED_SUBDEV */)
{
    MdiagSurf *surf = GetSurface(surfType, subdev);
    if (surf)
        SetIsActiveSurface(surf, setval);
    else
        ErrPrintf("Called SetSurfaceUsed with an invalid buffer: %d\n", (UINT32)surfType);
}

bool LWGpuSurfaceMgr::IsSurfaceValid(SurfaceType surfType,
                                     UINT32 subdev /* = Gpu::UNSPECIFIED_SUBDEV */)
{
    MdiagSurf *surf = GetSurface(surfType, subdev);
    if (surf)
        return GetIsActiveSurface(surf) && GetValid(surf);
    else
        return false;
}

MdiagSurf* LWGpuSurfaceMgr::GetSubSurface(UINT32 subdev, SurfaceData& surfData)
{
    MASSERT(surfData.numSurfaces > 0);
    subdev = lwgpu->GetGpuDevice()->FixSubdeviceInst(subdev);
    if ((surfData.surfaces[0].GetLocation() != Memory::Fb)
        && (subdev < surfData.numSurfaces))
    {
        return &surfData.surfaces[subdev];
    }
    else
    {
        MASSERT(surfData.numSurfaces == 1);
        return &surfData.surfaces[0];
    }
}

MdiagSurf *LWGpuSurfaceMgr::GetSurfaceHead(UINT32 subdev)
{
    if (m_RenderSurfaces.empty())
        return NULL;

    m_surfaceLwr = m_RenderSurfaces.begin();

    if (m_surfaceLwr == m_RenderSurfaces.end())
    {
        return NULL;
    }
    else
    {
        return GetSubSurface(subdev, m_surfaceLwr->second);
    }
}

MdiagSurf *LWGpuSurfaceMgr::GetSurfaceNext(UINT32 subdev)
{
    if (m_surfaceLwr == m_RenderSurfaces.end())
    {
        return NULL;
    }
    else
    {
        ++m_surfaceLwr;

        if (m_surfaceLwr == m_RenderSurfaces.end())
        {
            return NULL;
        }
        else
        {
            return GetSubSurface(subdev, m_surfaceLwr->second);
        }
    }
}

MdiagSurf *LWGpuSurfaceMgr::GetSurface(SurfaceType surf, UINT32 subdev)
{
    const string name = GetTypeName(surf);
    SurfaceIterator sit = m_RenderSurfaces.begin();
    while (sit != m_RenderSurfaces.end())
    {
        if (sit->second.surfaces[0].GetName() == name)
        {
            return GetSubSurface(subdev, sit->second);
        }

        ++sit;
    }

    DebugPrintf("Looking for Unknown surface type %d\n", surf);
    return 0;
}

MdiagSurf *LWGpuSurfaceMgr::GetSurface(UINT32 idtag, UINT32 subdev)
{
    SurfaceIterator sit = m_RenderSurfaces.find(idtag);
    if (sit != m_RenderSurfaces.end())
        return GetSubSurface(subdev, sit->second);
    else
        return NULL;
}

LWGpuSurfaceMgr::SurfaceData *LWGpuSurfaceMgr::GetSurfaceDataByType(SurfaceType surfaceType)
{
    const string name = GetTypeName(surfaceType);
    SurfaceIterator surfIter = m_RenderSurfaces.begin();

    while (surfIter != m_RenderSurfaces.end())
    {
        if (surfIter->second.surfaces[0].GetName() == name)
        {
            return &(surfIter->second);
        }

        ++surfIter;
    }

    return 0;
}

LWGpuSurfaceMgr::SurfaceData *LWGpuSurfaceMgr::GetSurfaceDataBySurface(MdiagSurf *surf)
{
    SurfaceIterator sit = m_RenderSurfaces.find(GetIDTag(surf));
    if (sit == m_RenderSurfaces.end())
    {
        return 0;
    }
    return &(sit->second);
}

UINT32 LWGpuSurfaceMgr::GetIDTag(MdiagSurf *surf) const
{
    const IDTagMap::const_iterator findIt = m_IDTagMap.find(surf);
    return (findIt != m_IDTagMap.end()) ? findIt->second : 0;
}

MdiagSurf* LWGpuSurfaceMgr::AddSurface()
{
    const UINT32 IdTag = m_LastIdTag++;
    SurfaceData& data = m_RenderSurfaces[IdTag];
    data.numSurfaces = 1;
    data.active = true;
    data.forceNull = false;
    data.valid = false;
    data.loopback = false;
    data.peerIDs.push_back(USE_DEFAULT_RM_MAPPING);
    m_IDTagMap[&data.surfaces[0]] = IdTag;
    return &data.surfaces[0];
}

void LWGpuSurfaceMgr::SetForceNull(MdiagSurf *surf, bool forceNull)
{
    SurfaceIterator sit = m_RenderSurfaces.find(GetIDTag(surf));
    if (sit != m_RenderSurfaces.end())
    {
        sit->second.forceNull = forceNull;
    }
}

bool LWGpuSurfaceMgr::GetForceNull(MdiagSurf *surf) const
{
    ConstSurfaceIterator sit = m_RenderSurfaces.find(GetIDTag(surf));
    if (sit == m_RenderSurfaces.end())
    {
        return false;
    }
    return sit->second.forceNull;
}

void LWGpuSurfaceMgr::SetIsActiveSurface(MdiagSurf *surf, bool active)
{
    SurfaceIterator sit = m_RenderSurfaces.find(GetIDTag(surf));
    if (sit != m_RenderSurfaces.end())
    {
        sit->second.active = active;
    }
}

bool LWGpuSurfaceMgr::GetIsActiveSurface(MdiagSurf *surf) const
{
    ConstSurfaceIterator sit = m_RenderSurfaces.find(GetIDTag(surf));
    if (sit == m_RenderSurfaces.end())
    {
        // Treat all surfaces not managed by LWGpuSurfaceMgr as active
        return true;
    }
    return sit->second.active;
}

void LWGpuSurfaceMgr::SetValid(MdiagSurf *surf, bool valid)
{
    SurfaceIterator sit = m_RenderSurfaces.find(GetIDTag(surf));
    if (sit != m_RenderSurfaces.end())
    {
        sit->second.valid = valid;
    }
}

bool LWGpuSurfaceMgr::GetValid(MdiagSurf *surf) const
{
    ConstSurfaceIterator sit = m_RenderSurfaces.find(GetIDTag(surf));
    if (sit == m_RenderSurfaces.end())
    {
        // Treat all surfaces not managed by LWGpuSurfaceMgr as valid
        return true;
    }
    return sit->second.valid;
}

void LWGpuSurfaceMgr::SetMapSurfaceLoopback(MdiagSurf *surf, bool loopback, vector<UINT32> peerIDs)
{
    SurfaceIterator sit = m_RenderSurfaces.find(GetIDTag(surf));
    if (sit != m_RenderSurfaces.end())
    {
        sit->second.loopback = loopback;
        sit->second.peerIDs = peerIDs;
    }
}

bool LWGpuSurfaceMgr::GetMapSurfaceLoopback(MdiagSurf *surf, vector<UINT32>* peerIDs) const
{
    ConstSurfaceIterator sit = m_RenderSurfaces.find(GetIDTag(surf));
    if (sit == m_RenderSurfaces.end())
    {
        // Treat all surfaces not managed by LWGpuSurfaceMgr as non-loopback
        peerIDs = 0;
        return false;
    }
    *peerIDs = sit->second.peerIDs;
    return sit->second.loopback;
}

// Return true if the surface is to be partially mapped during rendering time
// (due to -map_region commands).
bool LWGpuSurfaceMgr::IsPartiallyMapped(MdiagSurf *surface) const
{
    ConstSurfaceIterator sit = m_RenderSurfaces.find(GetIDTag(surface));
    if (sit == m_RenderSurfaces.end())
    {
        return false;
    }
    return (sit->second.partialMaps.size() != 0);
}

UINT32 LWGpuSurfaceMgr::DoZLwllSanityCheck(UINT32* zbuffer, UINT32 subdev)
{
    if (!m_DoZlwllSanityCheck)
    {
        WarnPrintf("DoZLwllSanityCheck: -skip_zc_sanity, skipping the sanity check\n");
        return 0;
    }

    // if the user chose this option, we want to read the zlwll rams and dump an image for them,
    // but doing the check does nothing so let's just skip it...
    if (ForceVisible)
    {
        WarnPrintf("DoZLwllSanityCheck: not compatible with -ForceVisible, skipping the sanity check\n");
        return 0;
    }

    MdiagSurf* const pSurface = GetSurface(SURFACE_TYPE_Z, subdev);
    LW50Raster* const pRaster = GetRaster(pSurface->GetColorFormat());
    const SLIScissorSpec scissor = GetSLIScissorSpec(pSurface, pSurface->GetHeight());
    UINT32 startY = 0;
    UINT32 height = pSurface->GetHeight();
    if (!scissor.empty())
    {
        for (UINT32 i = 0; i < subdev; i++)
        {
            startY += scissor[i];
        }
        if (!m_IsSliSurfClip)
        {
            startY = pSurface->GetHeight() - scissor[subdev] - startY;
        }
        height = scissor[subdev];
    }
    return m_ZLwll->SanityCheck(zbuffer, pSurface, pRaster, pSurface->GetWidth(),
                                startY, height, GetAAModeFromSurface(pSurface), !ForceVisible,
                                m_subch, subdev, lwgpu);
}

const UINT32* LWGpuSurfaceMgr::GetZLwllSanityImage(UINT32 rasterFormat)
{
    if (rasterFormat != RASTER_A8R8G8B8)
    {
        ErrPrintf("Only support A8R8G8B8 raster format\n");
        return NULL;
    }
    if( !m_DoZlwllSanityCheck )
    {
        return NULL;
    }
    return m_ZLwll->Image();
}

UINT32 LWGpuSurfaceMgr::ColorFormatFromName(const char *name)
{
    UINT32 value = _ILWALID_COLOR_FORMAT;

    BuffConfigType::const_iterator cit = m_colorBuffConfigs.find(name);
    if (cit != m_colorBuffConfigs.end())
    {
        value = cit->second->GetDeviceFormat();
    }

    return value;
}

UINT32 LWGpuSurfaceMgr::ZetaFormatFromName(const char *name)
{
    UINT32 value = _ILWALID_ZETA_FORMAT;

    BuffConfigType::const_iterator zit = m_zetaBuffConfigs.find(name);
    if (zit != m_zetaBuffConfigs.end())
    {
        value = zit->second->GetDeviceFormat();
    }

    return value;
}

const char* LWGpuSurfaceMgr::ColorNameFromColorFormat(UINT32 cformat)
{
    const char* cret = "COLOR_ILWALID";

    BuffConfigType::const_iterator cit = m_colorBuffConfigs.begin();
    while (cit != m_colorBuffConfigs.end())
    {
        if (cit->second->GetDeviceFormat() == cformat)
        {
            cret = cit->first.c_str();
            break;
        }

        ++cit;
    }

    return cret;
}

const char* LWGpuSurfaceMgr::ZetaNameFromZetaFormat(UINT32 zformat)
{
    const char* zret = "ILWALID_Z";

    BuffConfigType::const_iterator zit = m_zetaBuffConfigs.begin();
    while (zit != m_zetaBuffConfigs.end())
    {
        if (zit->second->GetDeviceFormat() == zformat)
        {
            zret = zit->first.c_str();
            break;
        }

        ++zit;
    }

    return zret;
}

ColorUtils::Format LWGpuSurfaceMgr::FormatFromDeviceFormat(UINT32 format) const
{
    BuffConfigType::const_iterator it;
    for (it=m_colorBuffConfigs.begin();
         it != m_colorBuffConfigs.end();
         ++it)
    {
        if (it->second->GetDeviceFormat() == format)
        {
            return it->second->GetSurfaceFormat();
        }
    }
    for (it=m_zetaBuffConfigs.begin();
         it != m_zetaBuffConfigs.end();
         ++it)
    {
        if (it->second->GetDeviceFormat() == format)
        {
            return it->second->GetSurfaceFormat();
        }
    }
    return ColorUtils::LWFMT_NONE;
}

UINT32 LWGpuSurfaceMgr::DeviceFormatFromFormat(ColorUtils::Format format) const
{
    BuffConfigType::const_iterator it;
    for (it=m_colorBuffConfigs.begin();
         it != m_colorBuffConfigs.end();
         ++it)
    {
        if (it->second->GetSurfaceFormat() == format)
        {
            return it->second->GetDeviceFormat();
        }
    }
    for (it=m_zetaBuffConfigs.begin();
         it != m_zetaBuffConfigs.end();
         ++it)
    {
        if (it->second->GetSurfaceFormat() == format)
        {
            return it->second->GetDeviceFormat();
        }
    }
    return _ILWALID_COLOR_FORMAT;
}

LW50Raster* LWGpuSurfaceMgr::GetRaster(ColorUtils::Format format) const
{
    BuffConfigType::const_iterator cit;

    for (cit=m_colorBuffConfigs.begin();
         cit != m_colorBuffConfigs.end();
         ++cit)
    {
        if (cit->second->GetSurfaceFormat() == format)
        {
            return cit->second;
        }
    }

    for (cit=m_zetaBuffConfigs.begin();
         cit != m_zetaBuffConfigs.end();
         ++cit)
    {
        if (cit->second->GetSurfaceFormat() == format)
        {
            return cit->second;
        }
    }

    return NULL;
}

UINT32 LWGpuSurfaceMgr::EnableRawFBAccess(_RAWMODE mode)
{
    LwRm* pLwRm = m_pTest->GetLwRmPtr();

    SurfaceIterator sit;
    for (sit = m_RenderSurfaces.begin(); sit != m_RenderSurfaces.end(); ++sit)
    {
        const SurfaceData& surfData = sit->second;
        for (UINT32 i=0; i < surfData.numSurfaces; i++)
        {
            const MdiagSurf* const surf = &surfData.surfaces[i];
            if (surf->GetCompressed() && sit->second.valid)
            {
                pLwRm->VidHeapReleaseCompression(surf->GetMemHandle(),
                                                 lwgpu->GetGpuDevice());
                if (surf->GetSplit())
                    pLwRm->VidHeapReleaseCompression(surf->GetSplitMemHandle(),
                                                     lwgpu->GetGpuDevice());
            }
        }
    }

    return 0;
}

UINT32 LWGpuSurfaceMgr::DisableRawFBAccess()
{
    LwRm* pLwRm = m_pTest->GetLwRmPtr();

    SurfaceIterator sit;
    for (sit = m_RenderSurfaces.begin(); sit != m_RenderSurfaces.end(); ++sit)
    {
        const SurfaceData& surfData = sit->second;
        for (UINT32 i=0; i < surfData.numSurfaces; i++)
        {
            const MdiagSurf* const surf = &surfData.surfaces[i];
            if (surf->GetCompressed() && sit->second.valid)
            {
                pLwRm->VidHeapReacquireCompression(surf->GetMemHandle(),
                                                   lwgpu->GetGpuDevice());
                if (surf->GetSplit())
                    pLwRm->VidHeapReacquireCompression(surf->GetSplitMemHandle(),
                                                       lwgpu->GetGpuDevice());
            }
        }
    }

    return 0;
}

UINT32 LWGpuSurfaceMgr::ScaleHeight(float data, float isOffset, float offset) const
{
    return Scale(m_Height, data, isOffset, offset, m_TargetDisplayHeight);
}

UINT32 LWGpuSurfaceMgr::ScaleWidth(float data, float isOffset, float offset) const
{
    return Scale(m_Width, data, isOffset, offset, m_TargetDisplayWidth);
}

UINT32 LWGpuSurfaceMgr::Scale(UINT64 size, float data, float isOffset, float offset, UINT64 targetSize) const
{
    if (size != targetSize) {
        double factor = (double)size / (double)targetSize;
        double value = data;
        InfoPrintf("Input value %g, data 0x%x ", value, *(UINT32*)&data);
        value = value * factor + isOffset * ((2048.0f * factor) - (2048.0f)) +
            offset;
        float ret = (float)value;
        Printf(Tee::PriNormal, "Scaled Width float: %g, hex 0x%x\n", value, *(UINT32*)&ret);
        return *(UINT32*)&ret;
    } else {
        data += offset;
        return *(UINT32*)&data;
    }
    MASSERT(0);
    return 0;
}

//! Sets SLI scissor specification.
//!
//! SLI scissor specification contains relative sizes
//! of horizontal scanline spans rendered by subsequent subdevices.
//! For example:
//!      SLIScissorSpec s1;
//!      s1.push_back(10);
//!      s1.push_back(20);
//!      SLIScissorSpec s2;
//!      s2.push_back(1);
//!      s2.push_back(1);
//! For a FB 1024x768 the following splits would occur:
//!  - s1
//!      subdevice 0 would render scanlines 0..255
//!      subdevice 1 would render scanlines 256..767
//!  - s2
//!      subdevice 0 would render scanlines 0..383
//!      subdevice 1 would render scanlines 384..767
//!
//! This member is for use by GpuVerif::ReadSurface().
bool LWGpuSurfaceMgr::SetSLIScissorSpec(MdiagSurf* surf, const SLIScissorSpec& sliScissorSpec)
{
    SurfaceIterator sit = m_RenderSurfaces.find(GetIDTag(surf));
    if (sit == m_RenderSurfaces.end())
    {
        return false;
    }

    if (sit->second.surfaces[0].GetLocation() != Memory::Fb)
        return false;

    sit->second.sliScissor = sliScissorSpec;
    return true;
}

//! Returns SLI scissor specification, optionally adjusted for height.
IGpuSurfaceMgr::SLIScissorSpec LWGpuSurfaceMgr::GetSLIScissorSpec(MdiagSurf* surf, UINT32 height) const
{
    ConstSurfaceIterator sit = m_RenderSurfaces.find(GetIDTag(surf));
    if (sit == m_RenderSurfaces.end())
    {
        SLIScissorSpec empty;
        return empty;
    }

    if (height == 0)
    {
        return sit->second.sliScissor;
    }
    return AdjustSLIScissorToHeight(sit->second.sliScissor, height);
}

//! Transforms SLI scissor specification, adjusting it for height.
IGpuSurfaceMgr::SLIScissorSpec LWGpuSurfaceMgr::AdjustSLIScissorToHeight(const SLIScissorSpec& sliScissorSpec, UINT32 height) const
{
    if (sliScissorSpec.empty() || (height == 0))
    {
        return sliScissorSpec;
    }

    // Scale by height/inputSum ratio to adjust for the given height
    const UINT32 inputSum = accumulate(sliScissorSpec.begin(), sliScissorSpec.end(), 0);
    SLIScissorSpec newSpec(sliScissorSpec.size());
    UINT32 newSum = 0;
    for (UINT32 j=0; j < sliScissorSpec.size(); j++)
    {
        newSpec[j] = sliScissorSpec[j] * height / inputSum;
        newSum += newSpec[j];
    }

    // Account for rounding error so that all scanlines are covered
    newSpec.back() += (height - newSum);

    return newSpec;
}

RC LWGpuSurfaceMgr::DoPartialMaps()
{
    RC rc = OK;

    for (int i = 0; i < MAX_RENDER_TARGETS; i++)
    {
        CHECK_RC(DoPartialMap(GetSurface(ColorSurfaceTypes[i], 0)));
    }

    CHECK_RC(DoPartialMap(GetSurface(SURFACE_TYPE_Z, 0)));

    return rc;
}

// This function will implement any partial mapping that needs to be
// done if the surface had any -map_region commands.
//
RC LWGpuSurfaceMgr::DoPartialMap(MdiagSurf *surface)
{
    RC rc = OK;

    if (GetValid(surface) && IsPartiallyMapped(surface))
    {
        SurfaceData *data = GetSurfaceDataBySurface(surface);

        // Remove the full mapping of the surface so that partial mapping
        // can be done.
        data->fullMap.Free();

        UINT64 pageSize;
        CHECK_RC(GetPartialMappingPageSize(surface, &pageSize));
        UINT64 pageAlign = pageSize;

        if (m_Params->ParamPresent("-map_region_align_kb"))
        {
            pageAlign = m_Params->ParamUnsigned("-map_region_align_kb");
            pageAlign <<= 10;

            if ((pageAlign > 0) && ((pageAlign % pageSize) != 0))
            {
                ErrPrintf("-map_region_align_kb alignment 0x%llx must be a multiple of page size 0x%llx\n", pageAlign, pageSize);
            }
        }

        vector<PartialMapData *>::iterator iter;
        for (iter = data->partialMaps.begin();
                iter != data->partialMaps.end();
                ++iter)
        {
            UINT64 offset;
            UINT64 length;

            // Determine the length of the mapping and the offset from
            // the start of the virtual allocation.
            CHECK_RC(GetPartialMappingFromCoords(surface, pageAlign, *iter));

            PartialMapData *Mapdata = (*iter);
            UINT32 subRegionIndex = 0;
            for(vector<surfaceMapData *>::iterator siter = Mapdata->surfaceMapDatas.begin();
                    siter != Mapdata->surfaceMapDatas.end();siter++)
            {
                offset = (*siter)->minOffset;
                length = (*siter)->maxOffset - (*siter)->minOffset;
                (*siter)->physMem.SetArrayPitch(length);
                (*siter)->physMem.SetSpecialization(Surface2D::PhysicalOnly);
                (*siter)->physMem.SetColorFormat(surface->GetColorFormat());
                (*siter)->physMem.SetLayout(surface->GetLayout());
                (*siter)->physMem.SetType(surface->GetType());
                (*siter)->physMem.SetPageSize(pageSize >> 10);
                (*siter)->physMem.SetLocation(surface->GetLocation());
                rc = (*siter)->physMem.Alloc(lwgpu->GetGpuDevice(), m_pTest->GetLwRmPtr());

                if (rc != OK)
                {
                    ErrPrintf("Can't allocate physical memory for region %u %u %u %u %u, subRegion %u, minOffset 0x%llx, maxOffset 0x%llx\n",
                            (*iter)->x1, (*iter)->y1, (*iter)->x2, (*iter)->y2,
                            (*iter)->arrayIndex,subRegionIndex, (*siter)->minOffset, (*siter)->maxOffset);

                    return rc;
                }

                (*siter)->map.SetArrayPitch(length);
                (*siter)->map.SetSpecialization(Surface2D::MapOnly);
                (*siter)->map.SetLocation(surface->GetLocation());

                // Map a subset of the virtual allocation to the physical
                // allocation.
                rc = (*siter)->map.MapVirtToPhys(lwgpu->GetGpuDevice(),
                        surface, &(*siter)->physMem, offset, 0, m_pTest->GetLwRmPtr());

                if (rc != OK)
                {
                    ErrPrintf("Can't map memory specified by region %u %u %u %u %u, subRegion %u, minOffset 0x%llx, maxOffset 0x%llx\n",
                            (*iter)->x1, (*iter)->y1, (*iter)->x2, (*iter)->y2,
                            (*iter)->arrayIndex, subRegionIndex, (*siter)->minOffset, (*siter)->maxOffset);

                    return rc;
                }
                UINT64 physAddr = 0;
                CHECK_RC((*siter)->physMem.GetPhysAddress(0, &physAddr));
                DebugPrintf("LWGpuSurfaceMgr::%s Surface Name = %s, Region %u %u %u %u %u,"
                        " subRegionIndex %u, aligned start phys addr = 0x%llx, size = 0x%x,"
                        " minOffset 0x%llx, maxOffset 0x%llx\n", __FUNCTION__, surface->GetName().c_str(),
                        (*iter)->x1,(*iter)->y1,(*iter)->x2,(*iter)->x2,(*iter)->arrayIndex,
                        subRegionIndex,physAddr,length, (*siter)->minOffset, (*siter)->maxOffset);
                subRegionIndex++;
            }
        }
    }

    return rc;
}

RC LWGpuSurfaceMgr::GetPartialMappingPageSize(MdiagSurf *surface,
    UINT64 *pageSize)
{
    RC rc;

    if (Platform::GetSimulationMode() == Platform::Amodel)
    {
        *pageSize = lwgpu->GetGpuDevice()->GetBigPageSize();
        return rc;
    }

    MdiagSurf physMem;

    physMem.SetArrayPitch(1);
    physMem.SetSpecialization(Surface2D::PhysicalOnly);
    physMem.SetColorFormat(surface->GetColorFormat());
    physMem.SetLayout(surface->GetLayout());

    CHECK_RC(physMem.Alloc(lwgpu->GetGpuDevice(), m_pTest->GetLwRmPtr()));

    MdiagSurf map;
    map.SetArrayPitch(1);
    map.SetSpecialization(Surface2D::MapOnly);

    CHECK_RC(map.MapVirtToPhys(lwgpu->GetGpuDevice(),
            surface, &physMem, 0, 0, m_pTest->GetLwRmPtr()));

    *pageSize = map.GetActualPageSizeKB() << 10;

    map.Free();
    physMem.Free();

    return rc;
}

// Based on the given partial mapping data, this function determines the
// dimensions of the smallest mapping that will cover every pixel in the
// specified rectangle (which came from a -map_region command-line argument).
//
RC LWGpuSurfaceMgr::GetPartialMappingFromCoords(MdiagSurf *surface,
    UINT64 pageSize, PartialMapData *mapData)
{
    RC rc = OK;

    UINT32 x1 = mapData->x1;
    UINT32 y1 = mapData->y1;
    UINT32 x2 = mapData->x2;
    UINT32 y2 = mapData->y2;
    UINT32 arrayIndex = mapData->arrayIndex;

    // The coordinates given in the -map_region command are in pixels
    // rather than samples, so scale for anti-aliasing.
    x1 *= surface->GetAAWidthScale();
    x2 *= surface->GetAAWidthScale();
    y1 *= surface->GetAAHeightScale();
    y2 *= surface->GetAAHeightScale();

    set<UINT64> offsetSet;

    // Loop over every pixel in the rectangle and get each pixel's
    // address.
    for (UINT32 x = x1; x < x2; ++x)
    {
        for (UINT32 y = y1; y < y2; ++y)
        {
            UINT64 offset;

            if (surface->GetLayout() == Surface2D::BlockLinear)
            {
                offset = GetBlocklinearOffset(surface, x, y, arrayIndex);
            }
            else
            {
                offset = GetPitchOffset(surface, x, y, arrayIndex);
            }

            offset += surface->GetHiddenAllocSize();

            offsetSet.insert(ALIGN_DOWN(offset, pageSize));
            // Keep track of the minimum and maximum offset found so far.

            // Need the offset at the end of the last pixel to callwlate
            // the maximum offset.
            offset += surface->GetBytesPerPixel();
            offsetSet.insert(ALIGN_UP(offset, pageSize));
        }
    }

    MASSERT(offsetSet.size() != 0);
    set<UINT64>::iterator it = offsetSet.begin();
    UINT64 startOffset = *it;
    UINT64 endOffset = *it;
    it++;
    for(; it != offsetSet.end(); it++)
    {
        if(endOffset + pageSize == *it)
        {
            endOffset += pageSize;
        }
        else
        {
            surfaceMapData *data = new surfaceMapData;
            data->minOffset = startOffset;
            data->maxOffset = min(endOffset + pageSize,surface->GetAllocSize());
            mapData->surfaceMapDatas.push_back(data);
            startOffset = *it;
            endOffset = *it;
        }
    }
    surfaceMapData *data = new surfaceMapData;
    data->minOffset = startOffset;
    data->maxOffset = min(endOffset + pageSize,surface->GetAllocSize());
    mapData->surfaceMapDatas.push_back(data);

    // Align the offsets to the page boundaries and save for later.
    // The CRC checking code will use these offsets when generating
    // a CRC profile key string.  The reason we want to use the page
    // aligned offsets is because RM will align all mappings to full
    // pages, which means that different partial mappings would end up
    // with the same image.  By using page aligned offsets, we make
    // the profile key canonical.

    mapData->minOffset = (*(mapData->surfaceMapDatas.begin()))->minOffset;
    mapData->maxOffset = (*(mapData->surfaceMapDatas.rbegin()))->maxOffset;

    return rc;
}

UINT64 LWGpuSurfaceMgr::GetBlocklinearOffset(MdiagSurf *surface, UINT32 x,
    UINT32 y, UINT32 z)
{
    UINT64 offset;
    BlockLinear bl(surface->GetAllocWidth(), surface->GetAllocHeight(),
        surface->GetDepth(), surface->GetLogBlockWidth(),
        surface->GetLogBlockHeight(), surface->GetLogBlockDepth(),
        surface->GetBytesPerPixel(), lwgpu->GetGpuDevice());

    offset = bl.Address(x, y, z);

    return offset;
}

UINT64 LWGpuSurfaceMgr::GetPitchOffset(MdiagSurf *surface, UINT32 x, UINT32 y,
    UINT32 z)
{
    UINT64 offset = 0;
    UINT32 bpp = surface->GetBytesPerPixel();
    UINT64 pitch = surface->GetPitch();
    UINT32 allocHeight = surface->GetHeight();

    if (pitch == 0)
    {
        pitch = surface->GetSize();
        bpp = 1;
        allocHeight = 1;
    }

    offset = x * bpp + y * pitch + z * allocHeight * pitch;

    return offset;
}

RC LWGpuSurfaceMgr::RestoreFullMaps()
{
    RC rc = OK;

    for (int i = 0; i < MAX_RENDER_TARGETS; i++)
    {
        CHECK_RC(RestoreFullMap(GetSurface(ColorSurfaceTypes[i], 0)));
    }

    CHECK_RC(RestoreFullMap(GetSurface(SURFACE_TYPE_Z, 0)));

    return rc;
}

// If the surface was partially mapping during rendering, this function
// will be called just before CRC checking to remove the partial maps
// and restore the full map as CRC checking and image dumping
// can't handle partially mapped surfaces.
//
RC LWGpuSurfaceMgr::RestoreFullMap(MdiagSurf *surface)
{
    RC rc = OK;

    if (GetValid(surface) && IsPartiallyMapped(surface))
    {
        SurfaceData *data = GetSurfaceDataBySurface(surface);
        vector<PartialMapData *>::iterator iter;

        for (iter = data->partialMaps.begin();
             iter != data->partialMaps.end();
             ++iter)
        {
            PartialMapData *Mapdata = (*iter);
            SurfaceMapDatas surfaceData = Mapdata->surfaceMapDatas;
            for(SurfaceMapDatas::iterator siter = surfaceData.begin();siter != surfaceData.end();++siter)
            {
                (*siter)->map.Free();
                (*siter)->physMem.Free();
            }
        }

        CHECK_RC(DoFullMap(surface));
    }

    return rc;
}

// This function determines extra information that should be added to
// a CRC profile key if the given surface was subject to a -map_region
// command-line argument.
//
string LWGpuSurfaceMgr::GetPartialMapCrcString(MdiagSurf *surface)
{
    string crcString = "map_region_";

    SurfaceData *data = GetSurfaceDataBySurface(surface);

    // Sort the partial mappings by offset so that the order of the
    // command-line arguments doesn't affect the CRC profile key string.
    sort(data->partialMaps.begin(), data->partialMaps.end(),
        PartialMapData::SortCompare);

    vector<PartialMapData *>::iterator iter;

    for (iter = data->partialMaps.begin();
         iter != data->partialMaps.end();
         ++iter)
    {
        crcString += Utility::StrPrintf("0x%llx-0x%llx_",
            (*iter)->minOffset, (*iter)->maxOffset);
    }

    return crcString;
}

// Allocate a pool of physical memory pages to be used for mapping rendertarget
// surfaces when either -use_page_poolC or -use_page_poolZ is specified.
//
// Page pools are used to test aliasing multiple virtual address ranges
// to the same physical memory pages.
//
RC LWGpuSurfaceMgr::AllocPagePool
(
    GpuDevice *pGpuDev,
    MdiagSurf **pagePool,
    UINT64 poolSize,
    UINT64 pageSize,
    Surface2D::Layout layout
)
{
    RC rc;

    unique_ptr<MdiagSurf> phys(new MdiagSurf);
    phys->SetColorFormat(ColorUtils::Y8);
    phys->SetLayout(layout);
    phys->SetSpecialization(Surface2D::PhysicalOnly);
    phys->SetArrayPitch(poolSize);

    // Surface2D::SetPageSize expects a size in kilobytes.
    phys->SetPageSize(pageSize >> 10);

    CHECK_RC(phys->Alloc(pGpuDev, m_pTest->GetLwRmPtr()));

    *pagePool = phys.release();

    return OK;
}

// Map the virtual address range of a surface to a pool of physical memory
// pages, one page at a time.  The page pool may be smaller than the size of
// the surface, so wrapping is necessary.
//
RC LWGpuSurfaceMgr::MapSurfaceToPagePool
(
    GpuDevice *pGpuDev,
    MdiagSurf *virt,
    MdiagSurf *pagePool,
    UINT64 poolSize,
    UINT64 pageSize
)
{
    RC rc;

    UINT64 virtualSize = virt->GetAllocSize();

    for (UINT64 offset = 0; offset < virtualSize; offset += pageSize)
    {
        UINT64 unmappedSize = virt->GetAllocSize() - offset;
        UINT64 mapSize = min(unmappedSize, pageSize);

        CHECK_RC(MapPage(pGpuDev, virt, pagePool, mapSize, offset,
            offset % poolSize));
    }

    return OK;
}

// This function will map a virtual address range to a physical memory page.
//
RC LWGpuSurfaceMgr::MapPage
(
    GpuDevice *pGpuDev,
    MdiagSurf *virt,
    MdiagSurf *phys,
    UINT64 size,
    UINT64 virtOffset,
    UINT64 physOffset
)
{
    RC rc;

    unique_ptr<MdiagSurf> map(new MdiagSurf);
    map->SetColorFormat(virt->GetColorFormat());
    map->SetSpecialization(Surface2D::MapOnly);
    map->SetLayout(virt->GetLayout());
    map->SetLocation(phys->GetLocation());
    map->SetLogBlockWidth(virt->GetLogBlockWidth());
    map->SetLogBlockHeight(virt->GetLogBlockHeight());
    map->SetLogBlockDepth(virt->GetLogBlockDepth());
    map->SetArrayPitch(size);

    CHECK_RC(map->MapVirtToPhys(pGpuDev, virt, phys, virtOffset, physOffset, m_pTest->GetLwRmPtr()));

    // Update virtual surface location to match the newly mapped location.
    virt->SetLocation(map->GetLocation());

    // The page pool my not be allocated with the same PTE kind as the
    // virtual surface, so call RM to change the PTE kind of the newly
    // mapped page.

    LwRm* pLwRm = m_pTest->GetLwRmPtr();
    LW0080_CTRL_DMA_GET_PTE_INFO_PARAMS getParams = {0};
    getParams.gpuAddr = virt->GetCtxDmaOffsetGpu() + virtOffset;

    CHECK_RC(pLwRm->ControlByDevice(
                 pGpuDev,
                 LW0080_CTRL_CMD_DMA_GET_PTE_INFO,
                 &getParams,
                 sizeof(getParams)));

    LW0080_CTRL_DMA_SET_PTE_INFO_PARAMS setParams = {0};
    setParams.gpuAddr = getParams.gpuAddr;

    for (UINT32 i= 0 ; i < LW0080_CTRL_DMA_GET_PTE_INFO_PTE_BLOCKS; ++i)
    {
        setParams.pteBlocks[i] = getParams.pteBlocks[i];
        if (setParams.pteBlocks[i].pageSize != 0 &&
            FLD_TEST_DRF(0080_CTRL_DMA_PTE_INFO_PARAMS, _FLAGS, _VALID,
                _TRUE, setParams.pteBlocks[i].pteFlags))
        {
            setParams.pteBlocks[i].kind = virt->GetPteKind();
        }
    }

    CHECK_RC(pLwRm->ControlByDevice(
                 pGpuDev,
                 LW0080_CTRL_CMD_DMA_SET_PTE_INFO,
                 &setParams,
                 sizeof(setParams)));

    GetSurfaceDataBySurface(virt)->pagePoolMappings.push_back(map.release());

    return OK;
}

PagePoolMappings *LWGpuSurfaceMgr::GetSurfacePagePoolMappings(MdiagSurf *surf)
{
    SurfaceData *data = GetSurfaceDataBySurface(surf);
    if (data != 0)
    {
        return &(data->pagePoolMappings);
    }
    return 0;
}

PartialMappings *LWGpuSurfaceMgr::GetSurfacePartialMappings(MdiagSurf *surface)
{
    SurfaceData *data = GetSurfaceDataBySurface(surface);
    if (data != 0)
    {
        return &(data->partialMaps);
    }
    return 0;
}

RC LWGpuSurfaceMgr::ClearHelper(const ArgReader *params,
                                GpuVerif* gpuVerif,
                                bool fastClearColor,
                                bool fastClearDepth,
                                const RGBAFloat& color,
                                const ZFloat& z,
                                const Stencil& stencil,
                                bool SrgbWrite,
                                UINT32 subdev)
{
    RC rc;
    MdiagSurf *surfC[MAX_RENDER_TARGETS];

    bool color_buffers = false;
    for (int i = 0; i < MAX_RENDER_TARGETS; i++)
    {
        surfC[i] = GetSurface(ColorSurfaceTypes[i], subdev);
        if (surfC[i] && GetValid(surfC[i]))
            color_buffers = true;
    }
    MdiagSurf *surfZ = GetSurface(SURFACE_TYPE_Z, subdev);

    // clearing color
    if (fastClearColor)
    {
        if (color_buffers)
            CHECK_RC(HWClearColor(params, gpuVerif, z, stencil, SrgbWrite, subdev));
    }
    else
    {
        for (int i = 0; i < MAX_RENDER_TARGETS; i++)
        {
            if (surfC[i] && GetValid(surfC[i]))
            {
                char tmp[64];
                sprintf(tmp, "Color Buffer %d", i);
                MDiagUtils::SetMemoryTag tag(gpuVerif->LWGpu()->GetGpuDevice(), tmp);
                PagePoolMappings *mappings =
                    GetSurfacePagePoolMappings(surfC[i]);

                if ((mappings != 0) && (mappings->size() > 0))
                {
                    for (auto& surf : *mappings)
                    {
                        CHECK_RC(BackDoorClear(params, gpuVerif, surf, color, z, stencil,
                            SrgbWrite, subdev));
                    }
                }
                else
                {
                    CHECK_RC(BackDoorClear(params, gpuVerif, surfC[i], color, z, stencil,
                            SrgbWrite, subdev));
                }
            }
        }
    }

    // clearing depth
    if (surfZ && GetValid(surfZ))
    {
        if (fastClearDepth)
        {
            CHECK_RC(HWClearDepth(params, gpuVerif, color, z, stencil, SrgbWrite, subdev));
        }
        else
        {
            MDiagUtils::SetMemoryTag tag(gpuVerif->LWGpu()->GetGpuDevice(), "Z-Buffer");
            PagePoolMappings *mappings =
                GetSurfacePagePoolMappings(surfZ);

            if ((mappings != 0) && (mappings->size() > 0))
            {
                for (auto& surf : *mappings)
                {
                    CHECK_RC(BackDoorClear(params, gpuVerif, surf, color, z, stencil,
                        SrgbWrite, subdev));
                }
            }
            else
            {
                CHECK_RC(BackDoorClear(params, gpuVerif, surfZ, color, z, stencil, SrgbWrite,
                        subdev));
            }
            if (IsZlwllEnabled() || IsSlwllEnabled())
                CHECK_RC(ZlwllOnlyClear(z, stencil, surfZ));
        }
    }

    return OK;
}

RC LWGpuSurfaceMgr::BackDoorClear
(
    const ArgReader *params,
    GpuVerif* gpuVerif,
    MdiagSurf *surf,
    const RGBAFloat &color,
    const ZFloat &z,
    const Stencil &s,
    bool SrgbWrite,
    UINT32 subdev
)
{
    GpuDevice *pGpuDev = lwgpu->GetGpuDevice();

    RC rc;
    LW50Raster *raster = GetRaster(surf->GetColorFormat());
    vector<UINT08> Data(pGpuDev->GobSize());
    bool chiplibDumpDisabled = false;

    if (params->ParamPresent("-backdoor_load") && ChiplibTraceDumper::GetPtr()->IsChiplibTraceDumpEnabled())
    {
        ChiplibTraceDumper::GetPtr()->DisableChiplibTraceDump();
        chiplibDumpDisabled = true;
    }

    raster->FillLinearGob(&Data[0], color, z, s, SrgbWrite, pGpuDev->GobSize());

    PartialMappings *partialMappings = GetSurfacePartialMappings(surf);

    const bool initWithCE = surf->UseCEForMemAccess(params, MdiagSurf::WrSurf);
    const bool hasXBits = surf->FormatHasXBits();

    if (surf->MemAccessNeedsColwersion(params, MdiagSurf::WrSurf))
    {
        PixelFormatTransform::ColwertNaiveToRawSurf(surf, &Data[0], pGpuDev->GobSize());
    }

    if ((partialMappings != 0) && (partialMappings->size() > 0))
    {
        for (auto& mapdata : *partialMappings)
        {
            for (auto& data : mapdata->surfaceMapDatas)
            {
                if (initWithCE && hasXBits)
                {
                    CHECK_RC(ZeroSurface(params, gpuVerif, data->map, subdev, true));
                }
                WriteSurface(params, gpuVerif, data->map, &Data[0], Data.size(), subdev, initWithCE);
            }
        }
    }
    else
    {
        if (initWithCE && hasXBits)
        {
            CHECK_RC(ZeroSurface(params, gpuVerif, *surf, subdev, true));
        }
        WriteSurface(params, gpuVerif, *surf, &Data[0], Data.size(), subdev, initWithCE);
    }

    if (chiplibDumpDisabled)
    {
        ChiplibTraceDumper::GetPtr()->EnableChiplibTraceDump();
    }

    if (params->ParamPresent("-backdoor_load"))
    {
        CHECK_RC(DumpRawMemory(surf, surf->GetSize(), params));
    }

    return OK;
}

RC LWGpuSurfaceMgr::DumpRawMemory(MdiagSurf *surface, UINT32 size, const ArgReader* params)
{
    RC rc;

    CHECK_RC(DumpRawSurfaceMemory(m_pTest->GetLwRmPtr(), m_pTest->GetSmcEngine(), surface, 0, size, surface->GetName(), false, lwgpu, m_subch->channel(), params));

    return rc;
}

RC LWGpuSurfaceMgr::HWClearColor
(
    const ArgReader* params,
    GpuVerif* gpuVerif,
    const ZFloat& z,
    const Stencil& s,
    bool SrgbWrite,
    UINT32 subdev
)
{
    RC rc;

    // For RawImages tests, we must provide an initial clear of all color and
    // Z buffers to zero, without compression enabled.  This will ensures that
    // whatever garbage may have initially been in these buffers won't show up
    // in the leftover portions of each compressed tile.
    if (GetRawImageMode() != RAWOFF)
    {
        GpuDevice *pGpuDev = lwgpu->GetGpuDevice();
        vector<UINT08> Data(pGpuDev->GobSize());

        // bug 373583, initialize clear color for compressed surfaces
        // hardcoded rgba float values for hot pink
        UINT32 CompressedClearRGBA[4];
        CompressedClearRGBA[0] = 0x3F40C0C1;
        CompressedClearRGBA[1] = 0x3E40C0C1;
        CompressedClearRGBA[2] = 0x3F008081;
        CompressedClearRGBA[3] = 0x3F800000;
        RGBAFloat color(CompressedClearRGBA);

        // Clear all the color buffers
        for (UINT32 i = 0; i < MAX_RENDER_TARGETS; i++)
        {
            MdiagSurf *surfC = GetSurface(ColorSurfaceTypes[i], subdev);

            if (surfC && GetValid(surfC))
            {
                LW50Raster *raster = GetRaster(surfC->GetColorFormat());
                raster->FillLinearGob(&(Data[0]), color, z, s, SrgbWrite, pGpuDev->GobSize());
                PagePoolMappings *pagePoolMappings =
                    GetSurfacePagePoolMappings(surfC);
                PartialMappings *partialMappings =
                    GetSurfacePartialMappings(surfC);

                bool initWithCE = surfC->UseCEForMemAccess(params, MdiagSurf::WrSurf);

                if (surfC->MemAccessNeedsColwersion(params, MdiagSurf::WrSurf))
                {
                    PixelFormatTransform::ColwertNaiveToRawSurf(surfC, &Data[0], pGpuDev->GobSize());
                }

                if ((pagePoolMappings != 0) && (pagePoolMappings->size() > 0))
                {
                    for (auto& surf : *pagePoolMappings)
                    {
                        DebugPrintf("%s: initializing raw image color surface via %s.\n", 
                                __FUNCTION__, initWithCE ? "copy engine": "BAR1");
                        WriteSurface(params, gpuVerif, *surf, &Data[0], Data.size(), subdev, initWithCE);
                    }
                }
                else if ((partialMappings != 0) && (partialMappings->size() > 0))
                {
                    for (auto& mapdata : *partialMappings)
                    {
                        for (auto& data : mapdata->surfaceMapDatas)
                        {
                            DebugPrintf("%s: initializing raw image color surface via %s.\n", 
                                    __FUNCTION__, initWithCE ? "copy engine": "BAR1");
                            WriteSurface(params, gpuVerif, data->map, &Data[0], Data.size(), subdev, initWithCE);
                        }
                    }
                }
                else
                {
                    DebugPrintf("%s: initializing raw image color surface via %s.\n", 
                            __FUNCTION__, initWithCE ? "copy engine": "BAR1");
                    WriteSurface(params, gpuVerif, *surfC, &Data[0], Data.size(), subdev, initWithCE);
                }
            }
        }
    }

    if (!GetClearInit())
    {
        UINT32 i;

        // If a render target surface is created from an ALLOC_SURFACE
        // trace command, several render target setup methods that MODS
        // would previously send will not be sent and the trace will send
        // them instead.  If MODS is going to send a GPU clear
        // (because of a command line argument or because a
        // color/z surface is compressed) and the trace is sending
        // the render target setup methods, a CLEAR_INIT trace command
        // is necessary for MODS to know when to clear the trace.
        // (See bug 529602)
        for (i = 0; i < MAX_RENDER_TARGETS; i++)
        {
            MdiagSurf *surfC = GetSurface(ColorSurfaceTypes[i], subdev);

            if (surfC &&
                GetValid(surfC) &&
                surfC->GetCreatedFromAllocSurface() &&
                !m_SendRTMethods)
            {
                // Skip the GPU clear and give a warning,
                // but don't stop the test.
                WarnPrintf("trace_3d: can't use GPU clear because the trace has no CLEAR_INIT command.  The color surfaces will not be cleared.  (GPU clear is used for color surfaces when either the -gpu_clear command-line argument is specified or compression is specified for a color surface.)\n");
                return OK;
            }
        }

        // trace does not have initial clear in test.hdr
        InfoPrintf("Performing hardware color clear\n");

        // Temporarily set stencil write mask to 0xFF, since fakegl state may not be initialized yet
        CHECK_RC(m_subch->MethodWriteRC(LW9097_SET_STENCIL_MASK, 0xFF));

        // Clear all the color buffers
        for (i = 0; i < MAX_RENDER_TARGETS; i++)
        {
            MdiagSurf *surfC = GetSurface(ColorSurfaceTypes[i], subdev);
            if (surfC && GetValid(surfC))
            {
                // Assumes no 3D RT arrays
                UINT32 MaxZ = max(surfC->GetDepth(), surfC->GetArraySize());
                for (UINT32 z = 0; z < MaxZ; z++)
                {
                    // Enable all the color planes
                    UINT32 enables =
                        DRF_NUM(9097, _CLEAR_SURFACE, _MRT_SELECT, i) |
                        DRF_NUM(9097, _CLEAR_SURFACE, _RT_ARRAY_INDEX, z) |
                        DRF_DEF(9097, _CLEAR_SURFACE, _R_ENABLE, _TRUE) |
                        DRF_DEF(9097, _CLEAR_SURFACE, _G_ENABLE, _TRUE) |
                        DRF_DEF(9097, _CLEAR_SURFACE, _B_ENABLE, _TRUE) |
                        DRF_DEF(9097, _CLEAR_SURFACE, _A_ENABLE, _TRUE);

                    // Trigger the clear
                    CHECK_RC(m_subch->MethodWriteRC(LW9097_CLEAR_SURFACE, enables));
                }
            }
        }

        // Undo temporary enabling of stencil write mask
        CHECK_RC(m_subch->MethodWriteRC(LW9097_SET_STENCIL_MASK, 0x00));
    }
    else
    {
        // trace already has initial clear (CLEAR_INIT in test.hdr)
        InfoPrintf("Skipping gpu_clear injection since CLEAR_INIT specified in trace header...\n");
    }

    return OK;
}

RC LWGpuSurfaceMgr::HWClearDepth
(
    const ArgReader* params,
    GpuVerif* gpuVerif,
    const RGBAFloat &color,
    const ZFloat &z,
    const Stencil &s,
    bool SrgbWrite,
    UINT32 subdev
)
{
    RC rc;
    MdiagSurf *surfZ = GetSurface(SURFACE_TYPE_Z, subdev);
    UINT32 StencilClearValue = s.Get();

    MASSERT(surfZ && GetValid(surfZ));

    ColorUtils::Format surfaceFormat = surfZ->GetColorFormat();
    UINT32 zt_format = DeviceFormatFromFormat(surfaceFormat);

    const bool initWithCE = surfZ->UseCEForMemAccess(params, MdiagSurf::WrSurf);
    const bool hasXBits = surfZ->FormatHasXBits();

    // For RawImages tests, we must provide an initial clear of all color and
    // Z buffers to zero, without compression enabled.  This will ensures that
    // whatever garbage may have initially been in these buffers won't show up
    // in the leftover portions of each compressed tile.

    // If the Z surface is cleared by the method in the trace and the Z format
    // contains no stencil (eg Z24X8), MODS needs to clear the buffer for the same
    // reason.
    if ((GetRawImageMode() != RAWOFF) || hasXBits)
    {
        GpuDevice *pGpuDev = lwgpu->GetGpuDevice();
        vector<UINT08> Data(pGpuDev->GobSize());

         //when gpu doesn't has a real bar1, will clear the X bits with 0s
         //so fake bar1 and direct mapping will got a match data, bug 1948381
        bool clearWithZero = false;
        if (hasXBits &&
            (!pGpuDev->GetSubdevice(subdev)->HasFeature(Device::GPUSUB_SUPPORTS_SYSMEM_REFLECTED_MAPPING) ||
             pGpuDev->GetSubdevice(subdev)->IsFakeBar1Enabled()))

        {
            clearWithZero = true;
            InfoPrintf("%s:clear format with X bits with 0s\n", __FUNCTION__);
        }
        else
        {
            FillLinearGobDepth(&(Data[0]), surfZ, color, z, s, SrgbWrite);
        }

        PagePoolMappings *pagePoolMappings = GetSurfacePagePoolMappings(surfZ);
        PartialMappings *partialMappings =
            GetSurfacePartialMappings(surfZ);

        if (surfZ->MemAccessNeedsColwersion(params, MdiagSurf::WrSurf))
        {
            PixelFormatTransform::ColwertNaiveToRawSurf(surfZ, &Data[0], pGpuDev->GobSize());
        }

        if ((pagePoolMappings != 0) && (pagePoolMappings->size() > 0))
        {
            for (auto& surf : *pagePoolMappings)    
            {
                if (clearWithZero)
                {
                    ZeroSurface(params, gpuVerif, *surf, subdev, initWithCE);
                }
                else
                {
                    DebugPrintf("%s: initializing raw image Z surface via %s.\n", 
                            __FUNCTION__, initWithCE ? "copy engine": "BAR1");
                    WriteSurface(params, gpuVerif, *surf, &Data[0], Data.size(), subdev, initWithCE);
                }
            }
        }
        else if ((partialMappings != 0) && (partialMappings->size() > 0))
        {
            for (auto& mapdata : *partialMappings)
            {
                for (auto& data : mapdata->surfaceMapDatas)
                {
                    if (clearWithZero)
                    {
                        ZeroSurface(params, gpuVerif, data->map, subdev, initWithCE);
                    }
                    else
                    {
                        DebugPrintf("%s: initializing raw image Z surface via %s.\n", 
                                __FUNCTION__, initWithCE ? "copy engine": "BAR1");
                        WriteSurface(params, gpuVerif, data->map, &Data[0], Data.size(), subdev, initWithCE);
                    }
                }
            }
        }
        else
        {
            if (clearWithZero)
            {
                ZeroSurface(params, gpuVerif, *surfZ, subdev, initWithCE);
            }
            else
            {
                DebugPrintf("%s: initializing raw image Z surface via %s.\n", 
                        __FUNCTION__, initWithCE ? "copy engine": "BAR1");
                WriteSurface(params, gpuVerif, *surfZ, &Data[0], Data.size(), subdev, initWithCE);
            }
        }
    }

    if (GetClearInit() && (!m_ProgZtAsCt0 || surfZ->GetCreatedFromAllocSurface()))
    {
        // trace already has initial clear (CLEAR_INIT in test.hdr)
        // Also if option -prog_zt_as_ct0 used, MODS has to do the clear unless
        // Z is allocated by ALLOC_SURFACE
        InfoPrintf("Skipping gpu_clear injection for Z since CLEAR_INIT specified in trace header...\n");
        return OK;
    }

    // If a render target surface is created from an ALLOC_SURFACE
    // trace command, several render target setup methods that MODS
    // would previously send will not be sent and the trace will send
    // them instead.  If MODS is going to send a GPU clear
    // (because of a command line argument or because a
    // color/z surface is compressed) and the trace is sending
    // the render target setup methods, a CLEAR_INIT trace command
    // is necessary for MODS to know when to clear the trace.
    // (See bug 529602)
    else if (!GetClearInit() &&
        surfZ->GetCreatedFromAllocSurface() &&
        !m_SendRTMethods)
    {
        // Give an error message but don't stop the test.
        WarnPrintf("trace_3d: can't use GPU clear because the trace has no CLEAR_INIT command.  The Z buffer will not be cleared.  (GPU clear is used on the Z buffer when either the -gpu_clear command-line argument is specified or compression is specified for the Z buffer.)\n");
        return OK;
    }

    InfoPrintf("Performing hardware Z and stencil clear\n");

    // Temporarily set stencil write mask to 0xFF, since fakegl state may not be initialized yet
    CHECK_RC(m_subch->MethodWriteRC(LW9097_SET_STENCIL_MASK, 0xFF));

    // Z formats without stencil need to be overridden to a format that includes
    // stencil, with a stencil clear value of zero.  This ensures that, regardless
    // of the initial contents of the framebuffer, the final image is deterministic.
    bool ResetZFormat = false;
    switch (zt_format)
    {
        case LW9097_SET_ZT_FORMAT_V_X8Z24:
            ResetZFormat = true;
            CHECK_RC(m_subch->MethodWriteRC(
                LW9097_SET_ZT_FORMAT, LW9097_SET_ZT_FORMAT_V_S8Z24));
            break;
        case LW9097_SET_ZT_FORMAT_V_ZF32_X16V8X8:
            ResetZFormat = true;
            CHECK_RC(m_subch->MethodWriteRC(
                LW9097_SET_ZT_FORMAT, LW9097_SET_ZT_FORMAT_V_ZF32_X16V8S8));
            break;
        default:
            // Other formats are fine; nothing to do
            break;
    }
    if (ResetZFormat)
        StencilClearValue = 0;

    CHECK_RC(m_subch->MethodWriteRC(LW9097_SET_STENCIL_CLEAR_VALUE, StencilClearValue));

    // Clear the Z buffer
    if (surfZ && GetValid(surfZ))
    {
        // Assumes no 3D RT arrays
        UINT32 MaxZ = max(surfZ->GetDepth(), surfZ->GetArraySize());
        for (UINT32 z = 0; z < MaxZ; z++)
        {
            // Enable the Z and stencil planes
            UINT32 enables =
                DRF_NUM(9097, _CLEAR_SURFACE, _RT_ARRAY_INDEX, z) |
                DRF_DEF(9097, _CLEAR_SURFACE, _Z_ENABLE, _TRUE) |
                DRF_DEF(9097, _CLEAR_SURFACE, _STENCIL_ENABLE, _TRUE);

            // Trigger the clear
            CHECK_RC(m_subch->MethodWriteRC(LW9097_CLEAR_SURFACE, enables));
        }
    }

    // Undo temporary enabling of stencil write mask
    CHECK_RC(m_subch->MethodWriteRC(LW9097_SET_STENCIL_MASK, 0x00));

    // Undo override of Z format, if necessary
    if (ResetZFormat)
        CHECK_RC(m_subch->MethodWriteRC(LW9097_SET_ZT_FORMAT,
                DeviceFormatFromFormat(surfZ->GetColorFormat())));

    return OK;
}

RC LWGpuSurfaceMgr::ZlwllOnlyClear
(
    const ZFloat &z,
    const Stencil &s,
    const MdiagSurf* surf
)
{
    RC rc;

    InfoPrintf("Performing Fermi zlwll-only clear\n");

    // Temporarily set stencil write mask to 0xFF, since fakegl state may not be initialized yet
    CHECK_RC(m_subch->MethodWriteRC(LW9097_SET_STENCIL_MASK, 0xFF));

    // Clear color and Z are specified as floats -- no colwersions required
    CHECK_RC(m_subch->MethodWriteRC(LW9097_SET_ZT_FORMAT,
            DeviceFormatFromFormat(surf->GetColorFormat())));
    CHECK_RC(m_subch->MethodWriteRC(LW9097_SET_Z_CLEAR_VALUE, z.GetFloat()));
    CHECK_RC(m_subch->MethodWriteRC(LW9097_SET_STENCIL_CLEAR_VALUE, s.Get()));

    // Trigger the clear

    CHECK_RC(m_subch->MethodWriteRC(LW9097_CLEAR_ZLWLL_REGION,
        DRF_DEF(9097, _CLEAR_ZLWLL_REGION, _Z_ENABLE, _TRUE) |
        DRF_DEF(9097, _CLEAR_ZLWLL_REGION, _STENCIL_ENABLE, _TRUE)));

    // Undo temporary enabling of stencil write mask
    CHECK_RC(m_subch->MethodWriteRC(LW9097_SET_STENCIL_MASK, 0x00));

    return OK;
}

void LWGpuSurfaceMgr::FillLinearGobDepth
(
    UINT08* Data,
    MdiagSurf* surf,
    const RGBAFloat& color,
    const ZFloat& z,
    const Stencil& s,
    bool SrgbWrite
)
{
    GpuDevice *pGpuDev = lwgpu->GetGpuDevice();
    LW50Raster *raster = GetRaster(surf->GetColorFormat());
    MASSERT(raster);
    raster->FillLinearGob(Data, color, z, s, SrgbWrite, pGpuDev->GobSize());
}
