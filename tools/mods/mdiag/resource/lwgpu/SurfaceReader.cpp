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

#include "SurfaceReader.h"
#include "mdiag/utils/IGpuSurfaceMgr.h"
#include "core/include/argread.h"
#include "mdiag/tests/gpu/trace_3d/tracechan.h"
#include "dmagf100ce.h"
#include "mdiag/lwgpu.h"
#include "core/include/platform.h"

#include "surfasm.h"
#include "dmasurfr.h"
#include "gpu/utility/surfrdwr.h"
#include "sclnrdim.h"
#include "dmard.h"
#include "mdiag/utils/surfutil.h"

#include "core/include/utility.h"

#include "crcchain.h"
#include "core/include/chiplibtracecapture.h"

#include "ampere/ga100/dev_fb.h"

class SurfaceCompressionUtil
{
public:
    SurfaceCompressionUtil(MdiagSurf *pSurface, GpuDevice *pGpuDevice, LwRm* pLwRm)
    : m_pLwRm(pLwRm), m_pSurface(pSurface), m_pGpuDevice(pGpuDevice)
    {
        m_OriPteKind = m_pSurface->GetPteKind();
        GpuSubdevice *pSubdev = m_pGpuDevice->GetSubdevice(0);
        MASSERT(pSubdev);
        if (pSubdev->HasFeature(Device::GPUSUB_SUPPORTS_GENERIC_MEMORY))
        {
            pSubdev->GetPteKindFromName("GENERIC_MEMORY", &m_TargetPteKind);
        }
        else
        {
            pSubdev->GetPteKindFromName("GENERIC_16BX2", &m_TargetPteKind);
        }

        UINT32 kindS8;
        if ((pSubdev->GetPteKindFromName("S8_2S", &kindS8) && m_OriPteKind == kindS8) ||
            (pSubdev->GetPteKindFromName("S8_COMPRESSIBLE_DISABLE_PLC", &kindS8) && m_OriPteKind == kindS8))
        {
            m_KindS8 = true;
        }
        else
        {
            m_KindS8 = false;
        }
    }

    void Release()
    {
        m_pLwRm->VidHeapReleaseCompression(m_pSurface->GetMemHandle(), m_pGpuDevice);

        if (m_pSurface->GetSplit())
        {
            m_pLwRm->VidHeapReleaseCompression(m_pSurface->GetSplitMemHandle(), m_pGpuDevice);
        }
        //Mods need not swizzle for s8 kind. So need not change the cached pte kind.
        if (!m_KindS8)
        {
            m_pSurface->SetPteKind(m_TargetPteKind);
        }
    }

    void Reacquire()
    {
        m_pLwRm->VidHeapReacquireCompression(m_pSurface->GetMemHandle(), m_pGpuDevice);

        if (m_pSurface->GetSplit())
        {
            m_pLwRm->VidHeapReacquireCompression(m_pSurface->GetSplitMemHandle(), m_pGpuDevice);
        }
        //Mods need not swizzle for s8 kind. So need not change the cached pte kind.
        if (!m_KindS8)
        {
            m_pSurface->SetPteKind(m_OriPteKind);
        }
    }

private:
    LwRm* m_pLwRm;
    MdiagSurf *m_pSurface;
    GpuDevice *m_pGpuDevice;
    UINT32 m_OriPteKind;
    UINT32 m_TargetPteKind;
    bool m_KindS8;
};

class UncompressedRawSurfaceReadHelper
{
public:
    UncompressedRawSurfaceReadHelper(MdiagSurf *pSurface, GpuSubdevice *pGpuSubDevice)
        : m_pSurface(pSurface), m_OriginalPteKind(pSurface->GetPteKind()),
          m_TargetPteKind(0), m_RawImageSupport(false)
    {
        UINT32 kindS8 = 0;
        // for bug 1376694, mods suport for uncompressed raw image
        // should only be enabled for gm20x and later
        if (pGpuSubDevice->HasFeature(Device::GPUSUB_SUPPORTS_UNCOMPRESSED_RAWIMAGE))
        {
            if (pGpuSubDevice->GetPteKindFromName("S8", &kindS8) && (UINT32)m_OriginalPteKind == kindS8)
            {
                // for bug 1417838, some z-formats, e.g. Z24S8, have Z and stencil
                // data, and this causes raw images to be different from the non-raw
                // images, so raw image support is needed, but it is not an issue for
                // S8 , please refer to comment #106 in that bug for more information.
                m_RawImageSupport = false;
            }
            else
            {
                m_RawImageSupport = true;
                if (!pGpuSubDevice->GetPteKindFromName("GENERIC_MEMORY", &m_TargetPteKind) &&
                    !pGpuSubDevice->GetPteKindFromName("GENERIC_16BX2", &m_TargetPteKind))
                {
                    MASSERT(!"Neither GENERIC_MEMORY nor GENERIC_16BX2 "
                        "PTE kinds exist for this device.");
                }
            }
        }
    }

    UINT32 GetOriginalPteKind() const
    {
        return m_OriginalPteKind;
    }

    UINT32 GetTargetPteKind() const
    {
        return m_TargetPteKind;
    }

    bool NeedReadRawImage()
    {
        return m_RawImageSupport && (m_pSurface->GetType() == LWOS32_TYPE_DEPTH)
            && (m_OriginalPteKind != m_TargetPteKind);
    }

private:
    MdiagSurf *m_pSurface;
    UINT32 m_OriginalPteKind;
    UINT32 m_TargetPteKind;
    bool m_RawImageSupport;
};

CheckDmaBuffer::CheckDmaBuffer():
    pDmaBuf(0),
    pTilingParams(0),
    pBlockLinear(0),
    CrcChainClass(CrcChainManager::UNSPECIFIED_CLASSNUM)
{
}

SurfaceReader::SurfaceReader(LwRm* pLwRm, SmcEngine* pSmcEngine, LWGpuResource *inLwgpu, 
        IGpuSurfaceMgr* surfMgr, const ArgReader* params,
        UINT32 trcArch, TraceChannel* channel, bool dmaCheck) : 
        m_pLwRm(pLwRm), m_pSmcEngine(pSmcEngine),
        m_DmaReader(0), m_lwgpu(inLwgpu), m_surfMgr(surfMgr),
        m_Params(params), m_Channel(channel), m_SurfaceCopier(0),
        m_DmaCheck(dmaCheck), m_ReadRawImage(false)
{
    m_class = trcArch;

    if (params->ParamPresent("-RawImageMode") > 0)
    {
        if ((_RAWMODE)params->ParamUnsigned("-RawImageMode") == RAWON)
        {
            m_ReadRawImage = true;
        }
    }
}

SurfaceReader::~SurfaceReader()
{
    // Trusted CE state should have been restored after every surface read/copy

    for (const auto& item : m_MarkedCeVprTrusted)
    {
        if (item.second)
        {
            MASSERT(!"VPR register is not restored after marking CE as trusted");
        }
    }

    for (const auto& item : m_MarkedCeCcTrusted)
    {
        if (item.second)
        {
            MASSERT(!"CC register is not restored after marking CE as trusted");
        }
    }

    if (m_SurfaceCopier)
    {
        if (m_DmaCheck &&
            (m_Params->ParamUnsigned("-dma_check") > 4) )
        {
            // reused ce
            m_SurfaceCopier = 0;
        }
        else
        {
            // didn't reuse ce
            delete m_SurfaceCopier;
            m_SurfaceCopier = 0;
        }
    }

    if (m_DmaReader)
    {
        delete m_DmaReader;
        m_DmaReader = 0;
    }
}

bool SurfaceReader::SetupDmaBuffer( UINT32 sz, UINT32 gpuSubdevice )
{
    if (m_DmaReader)            // MdiagSurf already setup
    {
        return true;
    }

    UINT32 max = sz;
    // Cap -dmaCheck memory allocations to 1MB for Tesla chips;
    // anything more must be done in 1MB chunks
    if (max > 1024*1024)
        max = 1024*1024;

    // Some non-graphics traces might have tiny C or Z surfaces, but huge
    // "dmabuffer" surfaces.   Unfortunately, AddDmaBuffer is called after
    // Setup is called, so we don't know how big these surfaces are yet.
    // Make sure we have a reasonable minimum size.
    if (max < 64*1024)
        max = 64*1024;

    // Create dma reader object.
    m_DmaReader = DmaReader::CreateDmaReader(
        DmaReader::GetEngineType(m_Params, m_lwgpu),
        DmaReader::GetWfiType(m_Params),
        m_lwgpu, m_Channel? m_Channel->GetCh():0,
        max, m_Params, m_pLwRm, m_pSmcEngine,
        gpuSubdevice);
    if (m_DmaReader == NULL)
    {
        ErrPrintf("Error oclwred while allocating DmaReader!\n");
        return false;
    }

    // allocate notifier
    if (!m_DmaReader->AllocateNotifier(GetPageLayoutFromParams(m_Params, NULL)))
    {
        ErrPrintf("Error oclwred while allocating DmaReader notifier!\n");
        return false;
    }

    // allocate dst dma buffer
    _DMA_TARGET target = (_DMA_TARGET)m_Params->ParamUnsigned("-loc_dmard", _DMA_TARGET_COHERENT);
    if (m_DmaReader->AllocateDstBuffer(GetPageLayoutFromParams(m_Params, NULL), max, target) == false)
    {
        ErrPrintf("Error oclwred while allocating DmaReader dst buffer!\n");
        return false;
    }

    // allocate dma reader handles
    if (m_DmaReader->AllocateObject() == false)
    {
        ErrPrintf("Error oclwred while allocating DmaReader handles!\n");
        return false;
    }

    SetupSurfaceCopier(gpuSubdevice);

    return true;
}

bool SurfaceReader::SetupSurfaceCopier( UINT32 gpuSubdevice )
{
    if (!m_SurfaceCopier)
    {
        bool crcOnGpu = m_DmaCheck && m_Params->ParamUnsigned("-dma_check") == 6;

        if (crcOnGpu || (m_DmaCheck && m_Params->ParamUnsigned("-dma_check") == 5))
        {
            // reuse existing ce reader
            m_SurfaceCopier = m_DmaReader;
        }
        else
        {
            // create colwerter
            m_SurfaceCopier = DmaReader::CreateDmaReader(
                                DmaReader::CE, DmaReader::GetWfiType(m_Params),
                                m_lwgpu, m_Channel?m_Channel->GetCh():0,
                                1024*1024, m_Params, m_pLwRm, m_pSmcEngine,
                                gpuSubdevice);

            // allocate notifier
            if (!m_SurfaceCopier->AllocateNotifier(GetPageLayoutFromParams(m_Params, NULL)))
            {
                ErrPrintf("Error oclwred while allocating DmaReader handles!\n");
                return false;
            }

            // allocate dma reader handles
            if (m_SurfaceCopier->AllocateObject() == false)
            {
                ErrPrintf("Error oclwred while allocating DmaReader handles!\n");
                return false;
            }
        }
        if (!crcOnGpu && m_Params->ParamPresent("-copy_surface") && m_Params->ParamUnsigned("-copy_surface") == 1)
        {
            m_SurfaceCopier->SetCopyLocation(Memory::Coherent);
        }
    }
    return true;
}

RC SurfaceReader::ReadSurface(vector<UINT08>* pOutSurface,
                         MdiagSurf *pSurface,
                         UINT32 gpuSubdevice,
                         bool rawCrc,
                         const SurfaceAssembler::Rectangle &rect)
{
    RC rc;

    UncompressedRawSurfaceReadHelper uncompRawSurfaceReaderHelper(
        pSurface, pSurface->GetGpuDev()->GetSubdevice(gpuSubdevice));
    SurfaceCompressionUtil surfaceCompressUtil(pSurface, m_lwgpu->GetGpuDevice(), m_pLwRm);
    Utility::CleanupOnReturn<SurfaceCompressionUtil>
        reacquireSurfaceCompression(&surfaceCompressUtil, &SurfaceCompressionUtil::Reacquire);
    SurfacePteModifier surfacePteModifier(pSurface, uncompRawSurfaceReaderHelper.GetTargetPteKind());
    Utility::CleanupOnReturn<SurfacePteModifier, RC>
        restoreSurfacePte(&surfacePteModifier, &SurfacePteModifier::Restore);

    bool bUseDmaReader = pSurface->UseCEForMemAccess(m_Params, MdiagSurf::RdSurf);

    // override bUseDmaReader if there are no enforcement to use CE and LWGpuSurfaceMgr has no optimizations
    if (pSurface->GetIsSparse() && !bUseDmaReader)
    {
        bUseDmaReader = !m_surfMgr->IsPartiallyMapped(pSurface);
    }

    if (bUseDmaReader && m_DmaReader == nullptr)
    {
        SetupDmaBuffer((UINT32)(pSurface->GetAllocSize()&0xffffffff), gpuSubdevice);
    }

    bool rawImage = false;
    if (m_ReadRawImage && pSurface->GetCompressed())
    {
        surfaceCompressUtil.Release();
    }
    else
    {
        reacquireSurfaceCompression.Cancel();
    }

    if (m_ReadRawImage && !pSurface->GetCompressed() &&
        uncompRawSurfaceReaderHelper.NeedReadRawImage())
    {
        if ((bUseDmaReader && (m_DmaReader != 0)) ||
            (m_Params->ParamPresent("-copy_surface") &&
             m_Params->ParamUnsigned("-copy_surface") != 2) )
        {
            CHECK_RC(surfacePteModifier.ChangeTo());
        }
        else
        {
            rawImage = true;
            pSurface->SetPteKind(uncompRawSurfaceReaderHelper.GetTargetPteKind());
            restoreSurfacePte.Cancel();
        }
    }
    else
    {
        restoreSurfacePte.Cancel();
    }

    MdiagSurf lwrrentCopy = *pSurface;
    if ((m_Params->ParamPresent("-copy_surface") &&
        m_Params->ParamUnsigned("-copy_surface") != 2) ||
        (m_Params->ParamPresent("-backdoor_crc") && pSurface->GetLayout() != Surface2D::Pitch))
    {
        if (!m_SurfaceCopier)
        {
            SetupSurfaceCopier(gpuSubdevice);
        }

        RC rc = m_SurfaceCopier->CopySurface(pSurface, GetPageLayoutFromParams(m_Params, NULL), gpuSubdevice);

        if(rc != OK)
        {
            ErrPrintf("Could not read back %s buffer: %s\n",
                pSurface->GetName().c_str(), rc.Message());
            return rc;
        }

        lwrrentCopy = *m_SurfaceCopier->GetCopySurface();
        lwrrentCopy.SetName(pSurface->GetName());
    }
    lwrrentCopy.SetDmaBufferAlloc(true);

    // Create surface and scanline readers for surface assembler
    auto pMappingReader = SurfaceUtils::CreateMappingSurfaceReader(lwrrentCopy);
    pMappingReader->SetRawImage(rawImage);
    if (m_Params->ParamPresent("-max_map_size") > 0)
    {
        const UINT32 granularity = 128; // See surfrdwr.cpp
        UINT32 maxSize = m_Params->ParamUnsigned("-max_map_size", 0);
        if (maxSize % granularity != 0)
        {
            maxSize = (maxSize + granularity - 1) / granularity * granularity;
            WarnPrintf("-max_map_size specified a size which is not 128k "
                "aligned, now it's set to %dk\n", maxSize);
        }
        pMappingReader->SetMaxMappedChunkSize(maxSize << 10);
    }

    auto pDmaSurfReader = bUseDmaReader
        ? SurfaceUtils::CreateDmaSurfaceReader(lwrrentCopy, m_DmaReader)
        : nullptr;
    SurfaceUtils::ISurfaceReader* pSurfReader = bUseDmaReader
        ? static_cast<SurfaceUtils::ISurfaceReader*>(pDmaSurfReader.get())
        : static_cast<SurfaceUtils::ISurfaceReader*>(pMappingReader.get());

    if (m_Params->ParamPresent("-dump_raw"))
    {
        UINT32 size = lwrrentCopy.GetSize();

        vector<UINT08> tmp(size,0);

        RC rc = pSurfReader->ReadBytes(0, &tmp[0], size);

        if(rc != OK)
        {
            ErrPrintf("Could not read back %s buffer: %s\n",
                lwrrentCopy.GetName().c_str(), rc.Message());
            return rc;
        }

        tmp.swap(*pOutSurface);
        return OK;
    }

    // Check to see if the surface is a rendertarget with partial mappings.
    // If so, the scanline reader can read just the mapped portions.
    PartialMappings *partialMappings = m_surfMgr->GetSurfacePartialMappings(pSurface);

    // The DMA readers can't handle partial mappings, so force them to
    // read the entire surface.
    if (bUseDmaReader)
    {
        partialMappings = 0;
    }

    const bool forcePitch = rawCrc;

    // For CE path, pitch to pitch copy should be used for bl surface with 0 width/height.
    bool pitchToPitchCopy = false;
    if ((pSurface->GetLayout() == Surface2D::BlockLinear) &&
        ((pSurface->GetWidth() == 0) || (pSurface->GetHeight() == 0)))
    {
        pitchToPitchCopy = true;
    }

    // Use a blocklinear-to-pitch copy if the DMA reader supports it and CE access
    // is choosed for current surfacer,
    // except if using a command-line argument that is likely to result
    // in an offset too large for the blocklinear-to-pitch copy.
    // (In the future, the blocklinear-to-pitch copy could be reworked to
    // prevent address offset overlow, but this was deemed the least disruptive
    // option for solving bug 2059391.)
    const bool blocklinearToPitch = (pDmaSurfReader != nullptr) &&
        !pitchToPitchCopy &&
        m_DmaReader->NoPitchCopyOfBlocklinear() &&
        !m_Params->ParamPresent("-inflate_rendertarget_and_offset_window") &&
        !m_Params->ParamPresent("-inflate_rendertarget_and_offset_viewport");

    unique_ptr<ScanlineReader> scanlineReader;
    CHECK_RC(CreateScanlineReader(
        &scanlineReader,
        *pSurfReader,
        *m_surfMgr,
        partialMappings,
        forcePitch,
        blocklinearToPitch));
    (*scanlineReader).SetOrigSurface(pSurface);

    // Support reverse scanline ordering
    const SurfaceAssembler::LineOrder lineOrder
        = (m_Params->ParamDefined("-yilw") && m_Params->ParamPresent("-yilw"))
        ? SurfaceAssembler::reverseLineOrder
        : SurfaceAssembler::normalLineOrder;

    // Obtain SLI scissor specification
    // NOTE: Use the original pSurface pointer, to find the spec in the map.
    const IGpuSurfaceMgr::SLIScissorSpec scissorSpec =
        m_surfMgr->GetSLIScissorSpec(pSurface, lwrrentCopy.GetHeight());

    // Setup surface assembler
    SurfaceAssembler surfAsm(lineOrder);
    if (scissorSpec.size() < 2) // No SLI
    {
        RC rc;
        // fix bug 486821: CRC checks fail for buffers allocated with
        // the ALLOC_SURFACE directive.
        if ((*scanlineReader).GetSurfaceWidth() == 0)
        {
            CHECK_RC(surfAsm.AddRect(
                gpuSubdevice,
                !rect.IsNull() ? rect : SurfaceAssembler::Rectangle(0,0,pSurface->GetSize(),1),
                0, 0));
        }
        else
        {
            CHECK_RC(surfAsm.AddRect(
                gpuSubdevice,
                !rect.IsNull() ? rect : SurfaceAssembler::Rectangle(*scanlineReader),
                0, 0));
        }
    }
    else // SLI
    {
        UINT32 fake_subdev = 0;
        bool   fake_subdev_mode = false;
        if (m_Params->ParamPresent("-sli_sfr_subdev") > 0)
        {
            fake_subdev = m_Params->ParamUnsigned("-sli_sfr_subdev", 0);
            fake_subdev_mode = true;
        }
        else
        {
            MASSERT(scissorSpec.size() == m_lwgpu->GetGpuDevice()->GetNumSubdevices());
        }
        const SurfaceAssembler::Rectangle whole(*scanlineReader);
        UINT32 y = 0;
        for (size_t subdev=0; subdev < scissorSpec.size(); subdev++)
        {
            const UINT32 height = scissorSpec[subdev];
            RC rc;
            // Fermi scissor works completely differntly from tesla due to y-ilwersion.
            // Consequently we need to composite the image in reverse order
            // If -sli_sfr_subdev is used, we apply the clip/scissor for specified subdev
            // to the single GPU(subdev=0), see detail at bug536829
            if (m_Params->ParamPresent("-sli_surfaceclip"))
            {
                if (!fake_subdev_mode || (fake_subdev_mode && fake_subdev == subdev))
                {
                    CHECK_RC(surfAsm.AddRect(
                        fake_subdev_mode ? 0 : subdev,
                        SurfaceAssembler::Rectangle(0, y, whole.width, height),
                        0, y));
                }
            }
            else
            {
                if (!fake_subdev_mode || (fake_subdev_mode && fake_subdev == subdev))
                {
                    CHECK_RC(surfAsm.AddRect(
                        fake_subdev_mode ? 0 : subdev,
                        SurfaceAssembler::Rectangle(0, (whole.height-height-y), whole.width, height),
                        0, whole.height-height-y));
                }
            }
            y += height;
        }
        MASSERT(y == whole.height);
    }

    // Print out the configuration
    InfoPrintf("Assembling %s %s %ux%ux%ux%u %ubpp%s%s%s %s\n",
        (lwrrentCopy.GetLayout()==Surface2D::BlockLinear) ? "BlockLinear" : "Pitch",
        lwrrentCopy.GetName().c_str(),
        unsigned(lwrrentCopy.GetWidth()),
        unsigned(lwrrentCopy.GetHeight()),
        unsigned(lwrrentCopy.GetDepth()),
        unsigned(lwrrentCopy.GetArraySize()),
        unsigned(lwrrentCopy.GetBytesPerPixel()*8),
        (lineOrder==SurfaceAssembler::reverseLineOrder) ? " bottom-up" : "",
        lwrrentCopy.GetSplit() ? " split" : "",
        (scissorSpec.size() > 1) ? " SLI" : "",
        (bUseDmaReader && (m_DmaReader != 0)) ? "DMA" : "mem-mapped");

    if (!rect.IsNull())
    {
        InfoPrintf("Partial CRC checking enabled, reading rect(%d, %d, %d, %d)\n",
            rect.x, rect.y, rect.x + rect.width - 1, rect.y + rect.height - 1);
    }

    bool bNeedRestoreBar1Dump = false;
    bool bNeedRestoreSysmemDump = false;
    // disable BAR1/sysmem access dump during crc surface read since for -backdoor_crc 
    // we will check the dumped raw file in replay
    if (m_Params->ParamPresent("-backdoor_crc"))
    {
        if ((lwrrentCopy.GetLocation() == Memory::Fb) && 
            ChiplibTraceDumper::GetPtr()->IsBar1DumpEnabled())
        {
            ChiplibTraceDumper::GetPtr()->SetBar1DumpFlag(false);
            bNeedRestoreBar1Dump = true;
        }
        else if ((lwrrentCopy.GetLocation() == Memory::Coherent || 
            lwrrentCopy.GetLocation() == Memory::NonCoherent) && 
            (ChiplibTraceDumper::GetPtr()->IsSysmemDumpEnabled()))
        {
            ChiplibTraceDumper::GetPtr()->SetSysmemDumpFlag(false);
            bNeedRestoreSysmemDump = true;
        }
    }

    {
        // Mark CE as trusted

        Utility::CleanupOnReturnArg<SurfaceReader, void, UINT32>
            restoreVpr(this, &SurfaceReader::RestoreCeVprTrusted, gpuSubdevice);
        if (NeedCeVprTrusted(&lwrrentCopy, gpuSubdevice))
        {
            SetCeVprTrusted(gpuSubdevice);
        }
        else
        {
            restoreVpr.Cancel();
        }

        Utility::CleanupOnReturnArg<SurfaceReader, void, UINT32>
            restoreCc(this, &SurfaceReader::RestoreCeCcTrusted, gpuSubdevice);
        if (NeedCeCcTrusted(&lwrrentCopy, gpuSubdevice))
        {
            SetCeCcTrusted(gpuSubdevice);
        }
        else
        {
            restoreCc.Cancel();
        }

        // Surface information to dump chiplib trace
        ColorUtils::Format fmt = pSurface->GetColorFormat();
        ChiplibOpScope::Crc_Surf_Read_Info surfInfo =
        {
            pSurface->GetName(), ColorUtils::FormatToString(fmt),
            pSurface->GetPitch(), pSurface->GetWidth(), pSurface->GetHeight(),
            pSurface->GetBytesPerPixel(), pSurface->GetDepth(),
            pSurface->GetArraySize(), /*srcX, srcY, srcWidth, srcHeight, destX, destY*/
            0, 0, 0, 0, 0, 0, // TODO
        };
        ChiplibOpScope newScope("crc_surf_read", NON_IRQ,
            ChiplibOpScope::SCOPE_CRC_SURF_READ, &surfInfo);
        // check if we're just in the backdoor archive read case.
        if (!(m_Params->ParamPresent("-backdoor_archive") &&
            m_Params->ParamUnsigned("-backdoor_archive") == 1))
        {

            // Assemble surface
            RC rc;
            if (!rect.IsNull())
            {
                UINT32 size = rect.width *
                              rect.height *
                              pSurface->GetDepth() *
                              pSurface->GetArraySize() *
                              pSurface->GetBytesPerPixel();
                CHECK_RC(surfAsm.Assemble(*scanlineReader, size));
            }
            else
            {
                CHECK_RC(surfAsm.Assemble(*scanlineReader));
            }
            surfAsm.ExtractOutputBuf(pOutSurface);

            if (m_Params->ParamPresent("-backdoor_crc"))
            {
                // write pitch data to tga file
                string fileName;
                if (m_Params->ParamPresent("-o") &&
                    (ChiplibTraceDumper::GetPtr()->GetTestNum() > 1))
                {
                    fileName = m_Params->ParamStr("-o", "");
                    fileName = Utility::StringReplace(fileName,"/", "_") + "_";
                }
                fileName += pSurface->GetName() + ".pitch.chiplib_dump";
                WriteDataToBmp(&((*pOutSurface)[0]), pSurface->GetWidth(), pSurface->GetHeight(),
                                      pSurface->GetColorFormat(), fileName);
            }
        }

        if (m_Params->ParamPresent("-backdoor_crc"))
        {
            RC rc;
            UINT32 width = pSurface->GetWidth();
            UINT32 size = width*pSurface->GetBytesPerPixel()*pSurface->GetHeight();
            size = size*pSurface->GetDepth()*pSurface->GetArraySize();
            if (width == 0) size = pSurface->GetAllocSize();

            if ((m_Params->ParamPresent("-raw_memory_dump_mode") > 0) &&
                (m_Params->ParamUnsigned("-raw_memory_dump_mode") == DUMP_RAW_CRC))
            {
                CHECK_RC(DumpRawSurfaceMemory(m_pLwRm, m_pSmcEngine, &lwrrentCopy, 0, size,
                        lwrrentCopy.GetName() + ".pitch", true, m_lwgpu,
                        m_Channel->GetCh(), m_Params, pOutSurface));
            }
            else
            {
                CHECK_RC(DumpRawMemory(&lwrrentCopy, 0, size));
            }
        }
    }

    if (bNeedRestoreBar1Dump)
    {
        ChiplibTraceDumper::GetPtr()->SetBar1DumpFlag(true);
    }
    if (bNeedRestoreSysmemDump)
    {
        ChiplibTraceDumper::GetPtr()->SetSysmemDumpFlag(true);
    }

    return OK;
}

RC SurfaceReader::DumpRawMemory(MdiagSurf *surface, UINT64 offset, UINT32 size)
{
    RC rc;

    CHECK_RC(DumpRawSurfaceMemory(m_pLwRm, m_pSmcEngine, surface, offset, size,
                surface->GetName() + ".pitch", true, m_lwgpu,
                m_Channel->GetCh(), m_Params));

    return rc;
}

RC SurfaceReader::ReadDmaBuffer(UINT08 **ppSurface,
        CheckDmaBuffer *pCheck,
        UINT32 gpuSubdevice,
        UINT32 chunkSize)
{
    RC rc;
    MdiagSurf *pDmaBuf = pCheck->pDmaBuf;
    // if chunkSize > 0 means we need to read the buffer chunk by chunk
    UINT32 readSize = chunkSize > 0 ? chunkSize : pCheck->GetBufferSize();
    vector<UINT08> pDst(readSize);

    GpuSubdevice *pGpuSubDev = m_lwgpu->GetGpuDevice()->GetSubdevice(gpuSubdevice);

    if (pDmaBuf->UseCEForMemAccess(m_Params, MdiagSurf::RdSurf))
    {
        if (m_DmaReader == nullptr)
        {
            SetupDmaBuffer(readSize, gpuSubdevice);
        }

        InfoPrintf("%s: Read surface %s with CE.\n",
                    __FUNCTION__, pDmaBuf->GetName().c_str());
        // Mark CE as trusted

        Utility::CleanupOnReturnArg<SurfaceReader, void, UINT32>
            restoreVpr(this, &SurfaceReader::RestoreCeVprTrusted, gpuSubdevice);
        if (NeedCeVprTrusted(pDmaBuf, gpuSubdevice))
        {
            SetCeVprTrusted(gpuSubdevice);
        }
        else
        {
            restoreVpr.Cancel();
        }

        Utility::CleanupOnReturnArg<SurfaceReader, void, UINT32>
            restoreCc(this, &SurfaceReader::RestoreCeCcTrusted, gpuSubdevice);
        if (NeedCeCcTrusted(pDmaBuf, gpuSubdevice))
        {
            SetCeCcTrusted(gpuSubdevice);
        }
        else
        {
            restoreCc.Cancel();
        }

        // DMA the framebuffer to system memory before working on it (fast on
        // HW/emulation and slow on cmodel/RTL)

        // Offset from base of context DMA
        UINT64 SrcOffset = pDmaBuf->GetCtxDmaOffsetGpu() + pDmaBuf->GetExtraAllocSize() + pCheck->Offset;

        //MASSERT(m_DmaCheckDstBufferSize != 0);
        MASSERT(m_DmaReader);
        MASSERT(m_DmaReader->GetBufferSize() != 0);

        // Do we need to split this up into multiple copies?
        if (readSize > m_DmaReader->GetBufferSize())
        {
            UINT32 LwrSize, Size;
            UINT32 Offset = 0;
            Size = readSize;
            while (Size > 0)
            {
                // Copy up to the size of the buffer per copy
                LwrSize = min(m_DmaReader->GetBufferSize(), Size);

                // DMA the chunk into the dma check buffer
                InfoPrintf("Doing MdiagSurf dmaCheck from offset 0x%llx\n",
                    SrcOffset);

                CHECK_RC(m_DmaReader->ReadLine(
                    pDmaBuf->GetCtxDmaHandle(), SrcOffset, 0, // Source buffer handle, offset, pitch
                    1, LwrSize, gpuSubdevice, pDmaBuf->GetSurf2D()));

                // Copy the chunk from from the dma check buffer to the final
                // buffer
                Platform::VirtualRd((void *)m_DmaReader->GetBuffer(), // Source
                                    &pDst[Offset], // Destination
                                    LwrSize);

                // Advance to the next chunk
                SrcOffset += LwrSize;
                Offset += LwrSize;
                Size -= LwrSize;
            }
        }
        else
        // Dma check buffer large enough to hold entire surface, do
        // monolithic dma
        {
            InfoPrintf("Doing MdiagSurf dmaCheck from offset 0x%llx\n",
                SrcOffset);

            CHECK_RC(m_DmaReader->ReadLine(
                pDmaBuf->GetCtxDmaHandle(), SrcOffset, 0, // Source buffer handle, offset, pitch
                1, readSize, gpuSubdevice, pDmaBuf->GetSurf2D()));

            // Read back the DMA check buffer into our buffer
            Platform::VirtualRd((void *)m_DmaReader->GetBuffer(),
                    &pDst[0], readSize);
        }
    }
    else
    {
        if (!pGpuSubDev->HasFeature(Device::GPUSUB_SUPPORTS_SYSMEM_REFLECTED_MAPPING))
        {
            InfoPrintf("%s: Read surface %s with direct mapping.\n",
                        __FUNCTION__, pDmaBuf->GetName().c_str());
        }

        // Read this region of the surface back
        CHECK_RC(SurfaceUtils::ReadSurface(*(pDmaBuf->GetSurf2D()), pCheck->Offset,
            &pDst[0], readSize, gpuSubdevice));
    }

    *ppSurface = new UINT08[pDst.size()];
    memcpy(*ppSurface, &pDst[0], pDst.size());
    if (!m_Params->ParamPresent("-dump_raw"))
    {
        // a colwersion from raw blocklinear to naive blocklinear needs to be done if the
        // surface is blocklinear and is using a generic memory PTE kind.
        if (pDmaBuf->MemAccessNeedsColwersion(m_Params, MdiagSurf::RdSurf)) 
        {
            PixelFormatTransform::ColwertRawToNaiveSurf(pDmaBuf, *ppSurface, pDst.size());
        }

        if (pCheck->pBlockLinear)
        {
            UINT08 *Temp = new UINT08[pCheck->Width * pCheck->Height];
            if (!Temp)
            {
                delete [] *ppSurface;
                return RC::CANNOT_ALLOCATE_MEMORY;
            }
            for (UINT32 i = 0; i <  pCheck->Height; ++i)
            {
                for (UINT32 j = 0; j < pCheck->Width; ++j)
                {
                    Temp[pCheck->Width*i + j] =
                        (*ppSurface)[pCheck->pBlockLinear->Address(pCheck->Width*i + j)];
                }
            }
            delete[] *ppSurface;
            *ppSurface = Temp;
        }
        else
        {
            rc = DoVP2UntilingLayout(ppSurface, pCheck);
        }
    }
    else
    {
        // if dump_raw is used, the untiled size is not set which will cause ASSERT error
        // in CheckDmaBuffer::GetUntiledSize. Set it to the buffer size for raw dump.
        pCheck->SetUntiledSize(pCheck->GetBufferSize());
    }
    if (OK != rc)
    {
        delete [] *ppSurface;
        return rc;
    }

    return OK;
}

RC SurfaceReader::CopySurface(MdiagSurf* pSurface, UINT32 gpuSubdevice, UINT32 Offset)
{
    if (!m_SurfaceCopier)
    {
        SetupSurfaceCopier(gpuSubdevice);
    }
    if (m_SurfaceCopier)
    {
        RC rc;

        UncompressedRawSurfaceReadHelper uncompRawSurfaceReaderHelper(
            pSurface, pSurface->GetGpuDev()->GetSubdevice(gpuSubdevice));
        SurfaceCompressionUtil surfaceCompressUtil(pSurface, m_lwgpu->GetGpuDevice(), m_pLwRm);
        Utility::CleanupOnReturn<SurfaceCompressionUtil>
            reacquireSurfaceCompression(&surfaceCompressUtil, &SurfaceCompressionUtil::Reacquire);
        SurfacePteModifier surfacePteModifier(pSurface, uncompRawSurfaceReaderHelper.GetTargetPteKind());
        Utility::CleanupOnReturn<SurfacePteModifier, RC>
            restoreSurfacePte(&surfacePteModifier, &SurfacePteModifier::Restore);

        if (m_ReadRawImage && pSurface->GetCompressed())
        {
            surfaceCompressUtil.Release();
        }
        else
        {
            reacquireSurfaceCompression.Cancel();
        }

        if (m_ReadRawImage && !pSurface->GetCompressed() &&
            uncompRawSurfaceReaderHelper.NeedReadRawImage())
        {
            CHECK_RC(surfacePteModifier.ChangeTo());
        }
        else
        {
            restoreSurfacePte.Cancel();
        }

        {
            // Mark CE as trusted

            Utility::CleanupOnReturnArg<SurfaceReader, void, UINT32>
                restoreVpr(this, &SurfaceReader::RestoreCeVprTrusted, gpuSubdevice);
            if (NeedCeVprTrusted(pSurface, gpuSubdevice))
            {
                SetCeVprTrusted(gpuSubdevice);
            }
            else
            {
                restoreVpr.Cancel();
            }

            Utility::CleanupOnReturnArg<SurfaceReader, void, UINT32>
                restoreCc(this, &SurfaceReader::RestoreCeCcTrusted, gpuSubdevice);
            if (NeedCeCcTrusted(pSurface, gpuSubdevice))
            {
                SetCeCcTrusted(gpuSubdevice);
            }
            else
            {
                restoreCc.Cancel();
            }

            rc = m_SurfaceCopier->CopySurface(pSurface, GetPageLayoutFromParams(m_Params, NULL), gpuSubdevice, Offset);
        }

        return rc;
    }
    return RC::SOFTWARE_ERROR;
}

UINT32 SurfaceReader::GetCopyHandle() const
{
    if (m_SurfaceCopier)
    {
        return m_SurfaceCopier->GetCopyHandle();
    }
    return 0;
}

UINT64 SurfaceReader::GetCopyBuffer() const
{
    if (m_SurfaceCopier)
    {
        return m_SurfaceCopier->GetCopyBuffer();
    }
    return 0;
}

bool SurfaceReader::NeedCeVprTrusted(const MdiagSurf *surf, UINT32 gpuSubdevice)
{
    if (surf->GetVideoProtected() &&
        surf->UseCEForMemAccess(m_Params, MdiagSurf::RdSurf))
    {
        InfoPrintf("VPR Trusted CE is needed by surface %s on GpuSubdevice %d.\n",
            surf->GetName().c_str(), gpuSubdevice);
        return true;
    }

    return false;
}

bool SurfaceReader::NeedCeCcTrusted(const MdiagSurf *surf, UINT32 gpuSubdevice)
{
    if (m_Params->ParamPresent("-set_cc_trusted_for_crc") &&
        surf->UseCEForMemAccess(m_Params, MdiagSurf::RdSurf))
    {
        InfoPrintf("CC Trusted CE is needed by surface %s on GpuSubdevice %d.\n",
            surf->GetName().c_str(), gpuSubdevice);
        return true;
    }

    return false;
}

void SurfaceReader::SetCeVprTrusted(UINT32 gpuSubdevice)
{
    if (m_MarkedCeVprTrusted[gpuSubdevice])
    {
        return; // it has not been marked; no need to do it again
    }

    GpuSubdevice *pGpuSubdevice = m_lwgpu->GetGpuDevice()->GetSubdevice(gpuSubdevice);

    // Only for chips after GP100
    if (pGpuSubdevice->DeviceId() < Gpu::GP100)
    {
        return;
    }

    // Mark CE as trusted client
    // Steps copied from bug 1684488

    // Chips before GA100 cannot use the newer LW_PFB_PRI_MMU_VPR_MODE and
    // LW_PFB_PRI_MMU_VPR_CYA_LO registers.
    if (pGpuSubdevice->DeviceId() < Gpu::GA100)
    {
        UINT32 cmd = 0;

        // 1.Write LW_PFB_PRI_MMU_VPR_INFO_INDEX_CYA_LO to LW_PFB_PRI_MMU_VPR_INFO.
        cmd = FLD_SET_DRF(_PFB, _PRI_MMU_VPR_INFO, _INDEX, _CYA_LO, cmd);
        m_lwgpu->RegWr32(LW_PFB_PRI_MMU_VPR_INFO, cmd);

        // 2.Set old_cya_lo to the value read from LW_PFB_PRI_MMU_VPR_INFO.
        m_SavedVprRegInfo[gpuSubdevice] = m_lwgpu->RegRd32(LW_PFB_PRI_MMU_VPR_INFO);

        // 3.Set the LW_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX bits of old_cya_lo to LW_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX_VPR_CYA_LO.
        // 4.Set new_cya_lo(cmd) equal to old_cya_lo(m_SavedVprRegInfo[gpuSubdevice])
        cmd = FLD_SET_DRF(_PFB, _PRI_MMU_VPR_WPR_WRITE, _INDEX, _VPR_CYA_LO, m_SavedVprRegInfo[gpuSubdevice]);

        // 5.Set the LW_PFB_PRI_MMU_VPR_WPR_WRITE_VPR_CYA_LO_TRUST_DEFAULT bit in new_cya_lo to 0.
        cmd = FLD_SET_DRF_NUM(_PFB, _PRI_MMU_VPR_WPR_WRITE, _VPR_CYA_LO_TRUST_DEFAULT, 0, cmd);

        // 6.Set the LW_PFB_PRI_MMU_VPR_WPR_WRITE_VPR_CYA_LO_TRUST_CE2 bits in new_cya_lo to LW_PFB_PRI_MMU_VPR_WPR_WRITE_VPR_CYA_TRUSTED.
        cmd = DRF_NUM(_PFB, _PRI_MMU_VPR_WPR_WRITE, _VPR_CYA_LO_TRUST_CE2, LW_PFB_PRI_MMU_VPR_WPR_WRITE_VPR_CYA_TRUSTED) |
            (cmd & ~DRF_SHIFTMASK(LW_PFB_PRI_MMU_VPR_WPR_WRITE_VPR_CYA_LO_TRUST_CE2));

        // 7.Write new_cya_lo to LW_PFB_PRI_MMU_VPR_WPR_WRITE.
        m_lwgpu->RegWr32(LW_PFB_PRI_MMU_VPR_WPR_WRITE, cmd);

        UINT32 regValue;
        do
        {
            regValue = pGpuSubdevice->Regs().Read32(MODS_PFB_PRI_MMU_VPR_INFO);

        } while (pGpuSubdevice->Regs().Test32(MODS_PFB_PRI_MMU_VPR_INFO_FETCH_TRUE));

        regValue = pGpuSubdevice->Regs().SetField(MODS_PFB_PRI_MMU_VPR_INFO_FETCH_TRUE);
        pGpuSubdevice->Regs().Write32(MODS_PFB_PRI_MMU_VPR_INFO, regValue);

        do
        {
            regValue = pGpuSubdevice->Regs().Read32(MODS_PFB_PRI_MMU_VPR_INFO);

        } while (pGpuSubdevice->Regs().Test32(MODS_PFB_PRI_MMU_VPR_INFO_FETCH_TRUE));

        InfoPrintf("%s: Saved LW_PFB_PRI_MMU_VPR_INFO data = 0x%x\n", __FUNCTION__, m_SavedVprRegInfo[gpuSubdevice]);
        InfoPrintf("%s: Write LW_PFB_PRI_MMU_VPR_WPR_WRITE with data = 0x%x\n", __FUNCTION__, cmd);
    }
    else
    {
        // Save the original CYA_LO value
        m_SavedVprRegInfo[gpuSubdevice] = m_lwgpu->RegRd32(LW_PFB_PRI_MMU_VPR_CYA_LO);

        UINT32 newCyaLo = m_SavedVprRegInfo[gpuSubdevice];

        // Set the CYA_LO_TRUST_DEFAULT bit in newCyaLo to 0.
        newCyaLo = FLD_SET_DRF(_PFB, _PRI_MMU_VPR_CYA_LO, _TRUST_DEFAULT, _USE_CYA, newCyaLo);

        // Set the LW_PFB_PRI_MMU_VPR_CYA_LO_TRUST_CE2 bits in newCyaLo to LW_PFB_PRI_MMU_VPR_CYA_TRUSTED.
        newCyaLo = DRF_NUM(_PFB, _PRI_MMU_VPR_CYA_LO, _TRUST_CE2, LW_PFB_PRI_MMU_VPR_CYA_TRUSTED) |
            (newCyaLo & ~DRF_SHIFTMASK(LW_PFB_PRI_MMU_VPR_CYA_LO_TRUST_CE2));

        m_lwgpu->RegWr32(LW_PFB_PRI_MMU_VPR_CYA_LO, newCyaLo);

        UINT32 regValue;
        do
        {
            regValue = pGpuSubdevice->Regs().Read32(MODS_PFB_PRI_MMU_VPR_MODE);

        } while (pGpuSubdevice->Regs().Test32(MODS_PFB_PRI_MMU_VPR_MODE_FETCH_TRUE));

        regValue = pGpuSubdevice->Regs().SetField(MODS_PFB_PRI_MMU_VPR_MODE_FETCH_TRUE);
        pGpuSubdevice->Regs().Write32(MODS_PFB_PRI_MMU_VPR_MODE, regValue);

        do
        {
            regValue = pGpuSubdevice->Regs().Read32(MODS_PFB_PRI_MMU_VPR_MODE);

        } while (pGpuSubdevice->Regs().Test32(MODS_PFB_PRI_MMU_VPR_MODE_FETCH_TRUE));

        InfoPrintf("%s: Saved LW_PFB_PRI_MMU_VPR_CYA_LO data = 0x%x\n", __FUNCTION__, m_SavedVprRegInfo[gpuSubdevice]);
        InfoPrintf("%s: Write LW_PFB_PRI_MMU_VPR_CYA_LO with data = 0x%x\n", __FUNCTION__, newCyaLo);
    }

    m_MarkedCeVprTrusted[gpuSubdevice] = true;
}

void SurfaceReader::SetCeCcTrusted(UINT32 gpuSubdevice)
{
    if (m_MarkedCeCcTrusted[gpuSubdevice])
    {
        return;
    }

    GpuSubdevice *pGpuSubdevice =
        m_lwgpu->GetGpuDevice()->GetSubdevice(gpuSubdevice);
    RegHal &regs = pGpuSubdevice->Regs();

    m_SavedCcRegInfo[gpuSubdevice] =
        regs.Read32(MODS_PFB_PRI_MMU_CC_TRUST_LEVEL_B_CE);

    regs.Write32(MODS_PFB_PRI_MMU_CC_TRUST_LEVEL_B_CE,
        regs.LookupAddress(MODS_PFB_PRI_MMU_CC_TRUST_LEVEL_TRUSTED));

    m_MarkedCeCcTrusted[gpuSubdevice] = true;
}

void SurfaceReader::RestoreCeVprTrusted(UINT32 gpuSubdevice)
{
    if (m_MarkedCeVprTrusted[gpuSubdevice] == false)
    {
        return; // it has been restored; No need to do it again
    }

    GpuSubdevice *pGpuSubdevice = m_lwgpu->GetGpuDevice()->GetSubdevice(gpuSubdevice);

    // Only for chips after GP100
    if (pGpuSubdevice->DeviceId() < Gpu::GP100)
    {
        return;
    }
    else if (pGpuSubdevice->DeviceId() < Gpu::GA100)
    {
        m_lwgpu->RegWr32(LW_PFB_PRI_MMU_VPR_WPR_WRITE, m_SavedVprRegInfo[gpuSubdevice]);

        InfoPrintf("%s: Restore LW_PFB_PRI_MMU_VPR_WPR_WRITE with data = 0x%x\n",
            __FUNCTION__, m_SavedVprRegInfo[gpuSubdevice]);

        UINT32 regValue;
        do
        {
            regValue = pGpuSubdevice->Regs().Read32(MODS_PFB_PRI_MMU_VPR_INFO);

        } while (pGpuSubdevice->Regs().Test32(MODS_PFB_PRI_MMU_VPR_INFO_FETCH_TRUE));

        regValue = pGpuSubdevice->Regs().SetField(MODS_PFB_PRI_MMU_VPR_INFO_FETCH_TRUE);
        pGpuSubdevice->Regs().Write32(MODS_PFB_PRI_MMU_VPR_INFO, regValue);

        do
        {
            regValue = pGpuSubdevice->Regs().Read32(MODS_PFB_PRI_MMU_VPR_INFO);

        } while (pGpuSubdevice->Regs().Test32(MODS_PFB_PRI_MMU_VPR_INFO_FETCH_TRUE));
    }
    else
    {
        m_lwgpu->RegWr32(LW_PFB_PRI_MMU_VPR_CYA_LO, m_SavedVprRegInfo[gpuSubdevice]);

        InfoPrintf("%s: Restore LW_PFB_PRI_MMU_VPR_CYA_LO with data = 0x%x\n",
            __FUNCTION__, m_SavedVprRegInfo[gpuSubdevice]);

        UINT32 regValue;
        do
        {
            regValue = pGpuSubdevice->Regs().Read32(MODS_PFB_PRI_MMU_VPR_MODE);

        } while (pGpuSubdevice->Regs().Test32(MODS_PFB_PRI_MMU_VPR_MODE_FETCH_TRUE));

        regValue = pGpuSubdevice->Regs().SetField(MODS_PFB_PRI_MMU_VPR_MODE_FETCH_TRUE);
        pGpuSubdevice->Regs().Write32(MODS_PFB_PRI_MMU_VPR_MODE, regValue);

        do
        {
            regValue = pGpuSubdevice->Regs().Read32(MODS_PFB_PRI_MMU_VPR_MODE);

        } while (pGpuSubdevice->Regs().Test32(MODS_PFB_PRI_MMU_VPR_MODE_FETCH_TRUE));
    }

    m_MarkedCeVprTrusted[gpuSubdevice] = false;
}

void SurfaceReader::RestoreCeCcTrusted(UINT32 gpuSubdevice)
{
    if (!m_MarkedCeCcTrusted[gpuSubdevice])
    {
        return;
    }

    GpuSubdevice *pGpuSubdevice =
        m_lwgpu->GetGpuDevice()->GetSubdevice(gpuSubdevice);
    RegHal &regs = pGpuSubdevice->Regs();

    regs.Write32(MODS_PFB_PRI_MMU_CC_TRUST_LEVEL_B_CE,
        m_SavedCcRegInfo[gpuSubdevice]);

    m_MarkedCeCcTrusted[gpuSubdevice] = false;
}
