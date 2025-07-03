/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "l2prefetch.h"
#include "class/cla06fsubch.h" // LWA06F_SUBCHANNEL_COPY_ENGINE
#include "class/clc7b5.h" // AMPERE_DMA_COPY_B copy methods
#include "core/include/channel.h"
#include "core/include/platform.h"
#include "gpu/include/gpudev.h"
#include "gpu/utility/surf2d.h"

//------------------------------------------------------------------------------
// SurfaceUtils::L2Prefetch: Class to prefetch surfaces from FB to L2 cache
//------------------------------------------------------------------------------

RC SurfaceUtils::L2Prefetch::Setup(GpuDevice *pGpuDevice, LwRm *pLwRm, FLOAT64 timeoutMs)
{
    MASSERT(pGpuDevice);
    MASSERT(pLwRm);

    RC rc;
    CHECK_RC(GenerateTestConfig(pGpuDevice, pLwRm, timeoutMs));
    CHECK_RC(Setup(m_pTestConfigLocal.get()));
    return rc;
}

RC SurfaceUtils::L2Prefetch::Setup(GpuTestConfiguration *pTestConfig)
{
    RC rc;
    MASSERT(pTestConfig);
    m_pTestConfig = pTestConfig;

    TestConfiguration::ChannelType originalChannelType = m_pTestConfig->GetChannelType();
    m_pTestConfig->SetChannelType(TestConfiguration::GpFifoChannel);
    DEFER
    {
        m_pTestConfig->SetChannelType(originalChannelType);
    };
    m_pTestConfig->SetAllowMultipleChannels(true);

    UINT32 engineId = 0;
    CHECK_RC(FindNonGrCE(&engineId));

    // Allocate channel for with PreFetch flag set
    constexpr UINT32 flags = DRF_DEF(OS04, _FLAGS_SET_EVICT_LAST_CE, _PREFETCH_CHANNEL, _TRUE);
    LwRm::Handle hCh;
    CHECK_RC(m_pTestConfig->AllocateChannelGr(&m_pCh, &hCh, nullptr, 0,
                &m_DmaCopyAlloc, flags, engineId));

    // Flush after all Surfaces are ready
    m_pCh->SetAutoFlush(false, 0);
    m_pCh->SetDefaultTimeoutMs(m_pTestConfig->TimeoutMs());
    CHECK_RC(m_pCh->SetObject(LWA06F_SUBCHANNEL_COPY_ENGINE, m_DmaCopyAlloc.GetHandle()));

    return rc;
}

RC SurfaceUtils::L2Prefetch::InsertPersistentSurface(Surface2D *pSurface)
{
    m_PersistentSurfaces.push_back(pSurface);
    return RC::OK;
}

RC SurfaceUtils::L2Prefetch::InsertNonPersistentSurface(Surface2D *pSurface)
{
    m_NonPersistentSurfaces.push_back(pSurface);
    return RC::OK;
}

RC SurfaceUtils::L2Prefetch::RemovePersistentSurfaces()
{
    m_PersistentSurfaces.clear();
    return RC::OK;
}

RC SurfaceUtils::L2Prefetch::RemoveNonPersistentSurfaces()
{
    m_NonPersistentSurfaces.clear();
    return RC::OK;
}

RC SurfaceUtils::L2Prefetch::RemoveAllSurfaces()
{
    RC rc;
    CHECK_RC(RemovePersistentSurfaces());
    CHECK_RC(RemoveNonPersistentSurfaces());
    return rc;
}

RC SurfaceUtils::L2Prefetch::RunPrefetchSequence()
{
    RC rc;

    {
        DEFER
        {
            LW2080_CTRL_CMD_LPWR_DIFR_PREFETCH_RESPONSE_PARAMS difrResponseParams = { };
            difrResponseParams.responseVal = (rc == RC::OK
                    ? LW2080_CTRL_LPWR_DIFR_PREFETCH_SUCCESS
                    : LW2080_CTRL_LPWR_DIFR_PREFETCH_FAIL_CE_HW_ERROR);

            rc = m_pTestConfig->GetRmClient()->ControlBySubdevice(
                       m_pTestConfig->GetGpuDevice()->GetSubdevice(0),
                       LW2080_CTRL_CMD_LPWR_DIFR_PREFETCH_RESPONSE,
                       &difrResponseParams, sizeof(difrResponseParams));
        };

        CHECK_RC(PrefetchSurfaces(m_PersistentSurfaces));
        CHECK_RC(PrefetchSurfaces(m_NonPersistentSurfaces));

        CHECK_RC(m_pCh->Flush());
        CHECK_RC(m_pCh->WaitIdle(m_pTestConfig->TimeoutMs()));

        Printf(Tee::PriDebug, "L2Prefetch::Number of surfaces Prefetched = %u\n",
               static_cast<UINT32>(m_PersistentSurfaces.size() + m_NonPersistentSurfaces.size()));
    }

    return rc;
}

RC SurfaceUtils::L2Prefetch::Cleanup()
{
    StickyRC rc;
    rc = RemoveAllSurfaces();
    if (m_pCh)
    {
        rc = m_pCh->WaitIdle(m_pTestConfig->TimeoutMs());
        rc = m_pTestConfig->FreeChannel(m_pCh);
        m_pCh = nullptr;
    }
    return rc;
}

RC SurfaceUtils::L2Prefetch::GenerateTestConfig
(
    GpuDevice *pGpuDevice,
    LwRm *pLwRm,
    FLOAT64 timeoutMs
)
{
#ifdef INCLUDE_MDIAG
    RC rc;

    if (!pGpuDevice)
    {
        Printf(Tee::PriError, "GPU Device cannot be null\n");
        return RC::BAD_PARAMETER;
    }

    if (!pLwRm)
    {
        Printf(Tee::PriError, "RM client cannot be null\n");
        return RC::BAD_PARAMETER;
    }

    TestDeviceMgr* pTestDeviceMgr = static_cast<TestDeviceMgr*>(DevMgrMgr::d_TestDeviceMgr);
    TestDevicePtr pTestDevice;
    CHECK_RC(pTestDeviceMgr->GetDevice(pGpuDevice->GetDeviceInst(), &pTestDevice));

    m_pTestConfigLocal = make_unique<GpuTestConfiguration>();
    m_pTestConfigLocal->BindTestDevice(pTestDevice);
    m_pTestConfigLocal->BindGpuDevice(pGpuDevice);
    m_pTestConfigLocal->BindRmClient(pLwRm);
    m_pTestConfigLocal->SetTimeoutMs(timeoutMs);

    return rc;
#else
    Printf(Tee::PriError, "GpuTestConfiguration object should already be available\n");
    MASSERT("Local GpuTestConfiguration object should only be created for MDIAG flow");
    return RC::UNSUPPORTED_FUNCTION;
#endif
}

RC SurfaceUtils::L2Prefetch::FindNonGrCE(UINT32 *pEngineId)
{
    RC rc;

    vector<UINT32> classList(1, m_DmaCopyAlloc.GetSupportedClass(m_pTestConfig->GetGpuDevice()));
    vector<UINT32> engineList;
    CHECK_RC(m_pTestConfig->GetGpuDevice()->GetPhysicalEnginesFilteredByClass(classList,
                &engineList));
    sort(engineList.begin(), engineList.end());

    *pEngineId = 0;
    GpuSubdevice *pGpuSubdevice = m_pTestConfig->GetGpuDevice()->GetSubdevice(0);
    for (const auto& engine : engineList)
    {
        bool isGrCe = true;
        CHECK_RC(m_DmaCopyAlloc.IsGrceEngine(engine, pGpuSubdevice, m_pTestConfig->GetRmClient(),
                    &isGrCe));
        if (!isGrCe)
        {
            *pEngineId = engine;
            break;
        }
    }
    if (!*pEngineId)
    {
        Printf(Tee::PriError, "No CE found to perform Prefetch\n");
        return RC::UNSUPPORTED_SYSTEM_CONFIG;
    }

    return rc;
}

RC SurfaceUtils::L2Prefetch::PrefetchSurfaces(const Surfaces& surfaces)
{
    // References
    // Doc: //hw/doc/gpu/ada/ada/power/DIFR/Ada_43_DIFR_FD.docx, Section: 3.2
    // CopyEngineStrategy::Write90b5Methods - dmawrap.cpp

    // _SRC_ methods have no functional implications on Prefetch operation.
    // But these methods needs to be programmed to avoid CE legacy launch errors.
    // Class file: //hw/lwgpu/class/mfs/class/2d/ampere_dma_copy.mfs

    RC rc;

    m_DmaCopyAlloc.SetOldestSupportedClass(AMPERE_DMA_COPY_B);
    m_DmaCopyAlloc.SetNewestSupportedClass(AMPERE_DMA_COPY_B);
    if (!m_DmaCopyAlloc.IsSupported(m_pTestConfig->GetGpuDevice()))
    {
        // Some methods like LWC7B5_OFFSET_OUT_UPPER seems to be changing between chips.
        // Make sure all correct methods are being used before enabling this function
        // for the chip in question.
        Printf(Tee::PriError, "This function is not supported for current chip\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    constexpr UINT32 subChan = LWA06F_SUBCHANNEL_COPY_ENGINE;
    for (const auto& surf : surfaces)
    {
        const UINT32 bytesPerPixel = surf->GetBytesPerPixel();

        // Callwlate memory offset based on Memory layout and coordinates.
        const UINT64 dstOffset64 = surf->GetCtxDmaOffsetGpu();
        CHECK_RC(m_pCh->Write(subChan, LWC7B5_OFFSET_OUT_UPPER, 2,
            static_cast<UINT32>(dstOffset64 >> 32), // LWC7B5_OFFSET_OUT_UPPER
            static_cast<UINT32>(dstOffset64))); // LWC7B5_OFFSET_OUT_LOWER

        UINT32 launchDMAFlags = DRF_DEF(C7B5, _LAUNCH_DMA, _DST_TYPE, _VIRTUAL);

        // Layout
        if (Surface2D::Pitch == surf->GetLayout())
        {
            launchDMAFlags |= DRF_DEF(C7B5, _LAUNCH_DMA, _SRC_MEMORY_LAYOUT, _PITCH);
            CHECK_RC(m_pCh->Write(subChan, LWC7B5_PITCH_IN,  surf->GetPitch()));

            launchDMAFlags |= DRF_DEF(C7B5, _LAUNCH_DMA, _DST_MEMORY_LAYOUT, _PITCH);
            CHECK_RC(m_pCh->Write(subChan, LWC7B5_PITCH_OUT, surf->GetPitch()));
        }
        else
        {
            // SRC
            launchDMAFlags |= DRF_DEF(C7B5, _LAUNCH_DMA, _SRC_MEMORY_LAYOUT, _BLOCKLINEAR);
            CHECK_RC(m_pCh->Write(subChan, LWC7B5_SET_DST_BLOCK_SIZE,
                DRF_NUM(C7B5, _SET_SRC_BLOCK_SIZE, _WIDTH,
                    surf->GetLogBlockWidth()) |
                DRF_NUM(C7B5, _SET_SRC_BLOCK_SIZE, _HEIGHT,
                    surf->GetLogBlockHeight()) |
                DRF_NUM(C7B5, _SET_SRC_BLOCK_SIZE, _DEPTH,
                    surf->GetLogBlockDepth()) |
                DRF_DEF(C7B5, _SET_SRC_BLOCK_SIZE, _GOB_HEIGHT, _GOB_HEIGHT_FERMI_8)));

            CHECK_RC(m_pCh->Write(subChan, LWC7B5_SET_SRC_WIDTH,
                surf->GetAllocWidth()* bytesPerPixel));
            CHECK_RC(m_pCh->Write(subChan, LWC7B5_SET_SRC_HEIGHT,
                surf->GetHeight()));
            CHECK_RC(m_pCh->Write(subChan, LWC7B5_SET_SRC_DEPTH,
                surf->GetDepth()));

            CHECK_RC(m_pCh->Write(subChan, LWC7B5_SRC_ORIGIN_X, 2,
                                  DRF_NUM(C7B5, _SRC_ORIGIN_X, _VALUE, 0),
                                  DRF_NUM(C7B5, _SRC_ORIGIN_Y, _VALUE, 0)));

            // DST
            launchDMAFlags |= DRF_DEF(C7B5, _LAUNCH_DMA, _DST_MEMORY_LAYOUT, _BLOCKLINEAR);
            CHECK_RC(m_pCh->Write(subChan, LWC7B5_SET_DST_BLOCK_SIZE,
                DRF_NUM(C7B5, _SET_DST_BLOCK_SIZE, _WIDTH,
                    surf->GetLogBlockWidth()) |
                DRF_NUM(C7B5, _SET_DST_BLOCK_SIZE, _HEIGHT,
                    surf->GetLogBlockHeight()) |
                DRF_NUM(C7B5, _SET_DST_BLOCK_SIZE, _DEPTH,
                    surf->GetLogBlockDepth()) |
                DRF_DEF(C7B5, _SET_DST_BLOCK_SIZE, _GOB_HEIGHT, _GOB_HEIGHT_FERMI_8)));

            CHECK_RC(m_pCh->Write(subChan, LWC7B5_SET_DST_WIDTH,
                surf->GetAllocWidth()* bytesPerPixel));
            CHECK_RC(m_pCh->Write(subChan, LWC7B5_SET_DST_HEIGHT,
                surf->GetHeight()));
            CHECK_RC(m_pCh->Write(subChan, LWC7B5_SET_DST_DEPTH,
                surf->GetDepth()));

            CHECK_RC(m_pCh->Write(subChan, LWC7B5_DST_ORIGIN_X, 2,
                                  DRF_NUM(C7B5, _DST_ORIGIN_X, _VALUE, 0),
                                  DRF_NUM(C7B5, _DST_ORIGIN_Y, _VALUE, 0)));
        }

        // Size
        const UINT32 lineLength = static_cast<UINT32>(ceil
                (surf->GetPitch() / static_cast<FLOAT32>(bytesPerPixel)));
        CHECK_RC(m_pCh->Write(subChan, LWC7B5_LINE_LENGTH_IN, lineLength));
        CHECK_RC(m_pCh->Write(subChan, LWC7B5_LINE_COUNT, surf->GetHeight()));

        // Remap Component
        MASSERT(bytesPerPixel > 0 && bytesPerPixel <= 4);
        const UINT32 remapComponent = DRF_DEF(C7B5, _SET_REMAP_COMPONENTS, _COMPONENT_SIZE, _ONE)
                            | DRF_DEF(C7B5, _SET_REMAP_COMPONENTS, _DST_X, _CONST_A)
                            | DRF_DEF(C7B5, _SET_REMAP_COMPONENTS, _DST_Y, _CONST_A)
                            | DRF_DEF(C7B5, _SET_REMAP_COMPONENTS, _DST_Z, _CONST_A)
                            | DRF_DEF(C7B5, _SET_REMAP_COMPONENTS, _DST_W, _CONST_A)
                            | DRF_NUM(C7B5, _SET_REMAP_COMPONENTS, _NUM_DST_COMPONENTS,
                                        bytesPerPixel - 1); 
        launchDMAFlags |= DRF_DEF(C7B5, _LAUNCH_DMA, _REMAP_ENABLE, _TRUE);
        CHECK_RC(m_pCh->Write(subChan, LWC7B5_SET_REMAP_COMPONENTS, remapComponent));

        launchDMAFlags |= DRF_DEF(C7B5, _LAUNCH_DMA, _FORCE_RMWDISABLE, _FALSE)
                            | DRF_DEF(C7B5, _LAUNCH_DMA, _VPRMODE, _VPR_NONE)
                            | DRF_DEF(C7B5, _LAUNCH_DMA, _MULTI_LINE_ENABLE, _TRUE)
                            | DRF_DEF(C7B5, _LAUNCH_DMA, _FLUSH_ENABLE, _TRUE)
                            | DRF_DEF(C7B5, _LAUNCH_DMA, _FLUSH_TYPE, _SYS)
                            | DRF_DEF(C7B5, _LAUNCH_DMA, _DATA_TRANSFER_TYPE, _NON_PIPELINED)
                            | DRF_DEF(C7B5, _LAUNCH_DMA, _INTERRUPT_TYPE, _NONE)
                            | DRF_DEF(C7B5, _LAUNCH_DMA, _SRC_TYPE, _VIRTUAL)
                            | DRF_DEF(C7B5, _LAUNCH_DMA, _SEMAPHORE_TYPE, _NONE);

        CHECK_RC(m_pCh->Write(subChan, LWC7B5_LAUNCH_DMA, launchDMAFlags));
    }

    return rc;
}
