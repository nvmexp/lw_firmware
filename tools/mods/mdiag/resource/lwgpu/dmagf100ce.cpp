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

#include "dmagf100ce.h"
#include "core/include/channel.h"
#include "gpu/include/gpumgr.h"
#include "core/include/mgrmgr.h"
#include "mdiag/tests/gpu/lwgpu_subchannel.h"
#include "mdiag/tests/gpu/trace_3d/tracechan.h"
#include "mdiag/utils/lwgpu_classes.h"
#include "class/cl90b5.h" // GF100_DMA_COPY

#define LW90B5_NUM(r,f,n)  (((n)&DRF_MASK(LW90B5 ## r ## f))<<DRF_SHIFT(LW90B5 ## r ## f))
#define WR90B5_NUM(v,r,f,n) (v=(((v)&~(DRF_MASK(LW90B5##r##f)<<DRF_SHIFT(LW90B5##r##f)))|LW90B5_NUM(r,f,n)))

// --------------------------------------------------------------------------
//  c'tor
// --------------------------------------------------------------------------
DmaReaderGF100CE::DmaReaderGF100CE(WfiType wfiType, LWGpuChannel* ch, UINT32 size)
    : DmaReader(wfiType, ch, size, CE)
{
}

// --------------------------------------------------------------------------
//  d'tor
// --------------------------------------------------------------------------
DmaReaderGF100CE::~DmaReaderGF100CE()
{
}

// --------------------------------------------------------------------------
//  gets the dma reader class id
// --------------------------------------------------------------------------
UINT32 DmaReaderGF100CE::GetClassId() const
{
    UINT32 firstCls = 0;     
    LwRm* pLwRm = m_Channel->GetLwRmPtr();
    GpuDevice *pGpuDev = m_Channel->GetLWGpu()->GetGpuDevice();

    EngineClasses::GetFirstSupportedClass(
        pLwRm,
        pGpuDev,
        "Ce",
        &firstCls);
    return firstCls;
}

// --------------------------------------------------------------------------
//  read line from dma to sysmem
// --------------------------------------------------------------------------
RC DmaReaderGF100CE::ReadLine
(
    UINT32 hSrcCtxDma, UINT64 SrcOffset, UINT32 SrcPitch,
    UINT32 LineCount, UINT32 LineLength,
    UINT32 gpuSubdevice, const Surface2D* srcSurf,
    MdiagSurf* dstSurf, UINT64 DstOffset
)
{
    RC rc = OK;
    UINT32 value = 0x0;

    if (LineCount == 0)
        return OK;

    DebugPrintf("DmaReaderGF100CE::ReadLine start\n");
    DebugPrintf("SrcOffset: %x\n", LwU64_LO32(SrcOffset));
    DebugPrintf("LineLength: %d\n", LineLength);

    UINT64 dstGpuOffset = m_DstGpuOffset;
    if (dstSurf) dstGpuOffset = DstOffset;

    LWGpuChannel* ch = m_Channel;
    ch->CtxSwEnable(false);

    // Get the number of subdevices.
    GpuDevice* pGpuDev = ch->GetLWGpu()->GetGpuDevice();
    MASSERT(pGpuDev);
    UINT32 gpuSubdeviceNum = pGpuDev->GetNumSubdevices();
    if (gpuSubdeviceNum > 1)
    {
        // SLI system: Add the mask.
        MASSERT(gpuSubdevice < gpuSubdeviceNum);
        UINT32 subdevMask = 1 << gpuSubdevice;
        CHECK_RC(ch->GetModsChannel()->WriteSetSubdevice(subdevMask));
    }

    // Enable privileged access for DmaCheck
    Channel* pChannel = ch->GetModsChannel();
    MASSERT(pChannel);
    pChannel->SetPrivEnable(true);

    // reuse subchannel if needed
    UINT32 subChannel = 0;
    CHECK_RC(ch->GetSubChannelNumClass(m_DmaObjHandle, &subChannel, 0));
    CHECK_RC(ch->SetObjectRC(subChannel, m_DmaObjHandle));

    CHECK_RC(ch->BeginInsertedMethodsRC());
    Utility::CleanupOnReturn<Channel> cleanupInsertedMethods(
            pChannel, &Channel::CancelInsertedMethods);

    if (m_ChClass == GF100_DMA_COPY)
    {
        CHECK_RC(ch->MethodWriteRC(subChannel, LW90B5_SET_APPLICATION_ID, LW90B5_SET_APPLICATION_ID_ID_NORMAL));
    }

    CHECK_RC(SendBasicKindMethod(srcSurf, dstSurf));

    // set src addresses and ctx dma index
    CHECK_RC(ch->MethodWriteRC(subChannel, LW90B5_OFFSET_IN_UPPER, LwU64_HI32(SrcOffset)));
    CHECK_RC(ch->MethodWriteRC(subChannel, LW90B5_OFFSET_IN_LOWER, LwU64_LO32(SrcOffset)));

    // set dst addresses and ctx dma index
    CHECK_RC(ch->MethodWriteRC(subChannel, LW90B5_OFFSET_OUT_UPPER, LwU64_HI32(dstGpuOffset)));
    CHECK_RC(ch->MethodWriteRC(subChannel, LW90B5_OFFSET_OUT_LOWER, LwU64_LO32(dstGpuOffset)));

    // set linelength, send launch dma
    CHECK_RC(ch->MethodWriteRC(subChannel, LW90B5_LINE_COUNT, LineCount));
    CHECK_RC(ch->MethodWriteRC(subChannel, LW90B5_LINE_LENGTH_IN, LineLength));
    value = 0x0;
    value = FLD_SET_DRF(90B5, _LAUNCH_DMA, _DATA_TRANSFER_TYPE,
                        _NON_PIPELINED, value);
    value = FLD_SET_DRF(90B5, _LAUNCH_DMA, _FLUSH_ENABLE, _TRUE, value);
    value = FLD_SET_DRF(90B5, _LAUNCH_DMA, _SRC_MEMORY_LAYOUT, _PITCH, value);
    value = FLD_SET_DRF(90B5, _LAUNCH_DMA, _DST_MEMORY_LAYOUT, _PITCH, value);
    value = FLD_SET_DRF(90B5, _LAUNCH_DMA, _MULTI_LINE_ENABLE, _FALSE, value);
    value = FLD_SET_DRF(90B5, _LAUNCH_DMA, _REMAP_ENABLE, _FALSE, value);
    CHECK_RC(ch->MethodWriteRC(subChannel, LW90B5_LAUNCH_DMA, value));

    cleanupInsertedMethods.Cancel();
    CHECK_RC(ch->EndInsertedMethodsRC());

    // SLI system: Renable all the subdevices.
    if (gpuSubdeviceNum > 1)
    {
        CHECK_RC(ch->GetModsChannel()->WriteSetSubdevice(Channel::AllSubdevicesMask));
    }

    ch->CtxSwEnable(true);

    // Cheesy way to do this. We should actually check the notify
    CHECK_RC(ch->WaitForIdleRC());

    DebugPrintf("DmaReaderGF100CE::ReadLine complete\n");

    return OK;
}

// --------------------------------------------------------------------------
//  copy surface from dma to sysmem
// --------------------------------------------------------------------------
RC DmaReaderGF100CE::CopySurface
(
    MdiagSurf* surf,
    _PAGE_LAYOUT Layout,
    UINT32 gpuSubdevice,
    UINT32 Offset
)
{
    InfoPrintf(
        "DmaReaderGF100CE::CopySurface(\n surf=0x%x \n", surf
    );

    RC rc = OK;

    CHECK_RC(AllocateBuffer(Layout, surf->GetSize()));

    UINT64 SrcOffset = surf->GetCtxDmaOffsetGpu();
    SrcOffset += surf->GetExtraAllocSize();
    SrcOffset += Offset;
    UINT64 DstOffset = m_CopyGpuOffset;

    // if the source surface is already naive pitch, copy it all at once.
    // (Current limit on LineLength for pitch surfs is 2^32.)
    UINT32 LineCount = 1;
    UINT32 LineLength = surf->GetSize() - Offset; // byte units
    UINT32 BytesPerPixel = surf->GetBytesPerPixel();
    UINT32 depth = 1;
    UINT32 arraySize = 1;
    bool multilineEnable = false;

    // new 64K limit on LineLength for BL surfs, so use pixels as units
    if ((surf->GetLayout() == Surface2D::BlockLinear) ||
        (surf->GetPitch() != surf->GetWidth()*BytesPerPixel))
    {
        LineCount = surf->GetHeight();
        LineLength = surf->GetWidth(); // pixel units
        depth = surf->GetDepth();
        arraySize = surf->GetArraySize();
        multilineEnable = true;
    }

    UINT32 value = 0x0;

    LWGpuChannel* ch = m_Channel;
    ch->CtxSwEnable(false);

    // Get the number of subdevices.
    GpuDevice* pGpuDev = ch->GetLWGpu()->GetGpuDevice();
    MASSERT(pGpuDev);
    UINT32 gpuSubdeviceNum = pGpuDev->GetNumSubdevices();
    if (gpuSubdeviceNum > 1)
    {
        // SLI system: Add the mask.
        MASSERT(gpuSubdevice < gpuSubdeviceNum);
        UINT32 subdevMask = 1 << gpuSubdevice;
        CHECK_RC(ch->GetModsChannel()->WriteSetSubdevice(subdevMask));
    }

    // Enable privileged access for DmaCheck
    Channel* pChannel = ch->GetModsChannel();
    MASSERT(pChannel);
    pChannel->SetPrivEnable(true);

    // reuse subchannel if needed
    UINT32 subChannel = 0;
    CHECK_RC(ch->GetSubChannelNumClass(m_DmaObjHandle, &subChannel, 0));
    CHECK_RC(ch->SetObjectRC(subChannel, m_DmaObjHandle));

    CHECK_RC(ch->BeginInsertedMethodsRC());
    Utility::CleanupOnReturn<Channel> cleanupInsertedMethods(
            pChannel, &Channel::CancelInsertedMethods);

    if (m_ChClass == GF100_DMA_COPY)
    {
        CHECK_RC(ch->MethodWriteRC(subChannel, LW90B5_SET_APPLICATION_ID, LW90B5_SET_APPLICATION_ID_ID_NORMAL));
    }

    CHECK_RC(SendBasicKindMethod(surf->GetSurf2D(), m_pSurface));

    // set linelength, send launch dma
    CHECK_RC(ch->MethodWriteRC(subChannel, LW90B5_LINE_COUNT, LineCount));
    CHECK_RC(ch->MethodWriteRC(subChannel, LW90B5_LINE_LENGTH_IN, LineLength)); // either pixel or byte units depending on the above logic.

    if (surf->GetLayout() == Surface2D::BlockLinear)
    {
        UINT32 blocks = 0;
        WR90B5_NUM(blocks,_SET_SRC_BLOCK_SIZE,_WIDTH,surf->GetLogBlockWidth());
        WR90B5_NUM(blocks,_SET_SRC_BLOCK_SIZE,_HEIGHT,surf->GetLogBlockHeight());
        WR90B5_NUM(blocks,_SET_SRC_BLOCK_SIZE,_DEPTH,surf->GetLogBlockDepth());
        if (ch->GetLWGpu()->GetGpuSubdevice(gpuSubdevice)->GobHeight() > 4)
        {
            WR90B5_NUM(blocks,_SET_SRC_BLOCK_SIZE,_GOB_HEIGHT,LW90B5_SET_SRC_BLOCK_SIZE_GOB_HEIGHT_GOB_HEIGHT_FERMI_8);
        }
        else
        {
            WR90B5_NUM(blocks,_SET_SRC_BLOCK_SIZE,_GOB_HEIGHT,LW90B5_SET_SRC_BLOCK_SIZE_GOB_HEIGHT_GOB_HEIGHT_TESLA_4);
        }
        CHECK_RC(ch->MethodWriteRC(subChannel, LW90B5_SET_SRC_BLOCK_SIZE, blocks));

        CHECK_RC(ch->MethodWriteRC(subChannel, LW90B5_SET_SRC_ORIGIN, 0));
        CHECK_RC(ch->MethodWriteRC(subChannel, LW90B5_SET_DST_ORIGIN, 0));
    }
    else
    {
        CHECK_RC(ch->MethodWriteRC(subChannel, LW90B5_PITCH_IN, surf->GetPitch()));
    }

    CHECK_RC(ch->MethodWriteRC(subChannel, LW90B5_SET_SRC_WIDTH, LineLength)); // either pixel or byte units depending on the above logic.
    CHECK_RC(ch->MethodWriteRC(subChannel, LW90B5_SET_SRC_HEIGHT, LineCount));
    CHECK_RC(ch->MethodWriteRC(subChannel, LW90B5_SET_SRC_DEPTH, surf->GetDepth()));

    value = 0x0;
    // always a "null" remap - no component swizzling.
    value = FLD_SET_DRF(90B5, _SET_REMAP_COMPONENTS, _DST_X, _SRC_X, value);
    value = FLD_SET_DRF(90B5, _SET_REMAP_COMPONENTS, _DST_Y, _SRC_Y, value);
    value = FLD_SET_DRF(90B5, _SET_REMAP_COMPONENTS, _DST_Z, _SRC_Z, value);
    value = FLD_SET_DRF(90B5, _SET_REMAP_COMPONENTS, _DST_W, _SRC_W, value);

    // hack: since no swizzling, treat 565 formats as 16-bit.
    switch (BytesPerPixel)
    {
    case 1:
        value = FLD_SET_DRF(90B5, _SET_REMAP_COMPONENTS, _NUM_SRC_COMPONENTS,
                            _ONE, value);
        value = FLD_SET_DRF(90B5, _SET_REMAP_COMPONENTS, _NUM_DST_COMPONENTS,
                            _ONE, value);
        value = FLD_SET_DRF(90B5, _SET_REMAP_COMPONENTS, _COMPONENT_SIZE,
                            _ONE, value);
        break;
    case 2:
        value = FLD_SET_DRF(90B5, _SET_REMAP_COMPONENTS, _NUM_SRC_COMPONENTS,
                            _TWO, value);
        value = FLD_SET_DRF(90B5, _SET_REMAP_COMPONENTS, _NUM_DST_COMPONENTS,
                            _TWO, value);
        value = FLD_SET_DRF(90B5, _SET_REMAP_COMPONENTS, _COMPONENT_SIZE,
                            _ONE, value);
        break;
    case 3:
        value = FLD_SET_DRF(90B5, _SET_REMAP_COMPONENTS, _NUM_SRC_COMPONENTS,
                            _THREE, value);
        value = FLD_SET_DRF(90B5, _SET_REMAP_COMPONENTS, _NUM_DST_COMPONENTS,
                            _THREE, value);
        value = FLD_SET_DRF(90B5, _SET_REMAP_COMPONENTS, _COMPONENT_SIZE,
                            _ONE, value);
        break;
    case 4:
        value = FLD_SET_DRF(90B5, _SET_REMAP_COMPONENTS, _NUM_SRC_COMPONENTS,
                            _FOUR, value);
        value = FLD_SET_DRF(90B5, _SET_REMAP_COMPONENTS, _NUM_DST_COMPONENTS,
                            _FOUR, value);
        value = FLD_SET_DRF(90B5, _SET_REMAP_COMPONENTS, _COMPONENT_SIZE,
                            _ONE, value);
        break;
    case 8:
        value = FLD_SET_DRF(90B5, _SET_REMAP_COMPONENTS, _NUM_SRC_COMPONENTS,
                            _FOUR, value);
        value = FLD_SET_DRF(90B5, _SET_REMAP_COMPONENTS, _NUM_DST_COMPONENTS,
                            _FOUR, value);
        value = FLD_SET_DRF(90B5, _SET_REMAP_COMPONENTS, _COMPONENT_SIZE,
                            _TWO, value);
        break;
    case 16:
        value = FLD_SET_DRF(90B5, _SET_REMAP_COMPONENTS, _NUM_SRC_COMPONENTS,
                            _FOUR, value);
        value = FLD_SET_DRF(90B5, _SET_REMAP_COMPONENTS, _NUM_DST_COMPONENTS,
                            _FOUR, value);
        value = FLD_SET_DRF(90B5, _SET_REMAP_COMPONENTS, _COMPONENT_SIZE,
                            _FOUR, value);
        break;
    default:
        return RC::SOFTWARE_ERROR;
        break;
    }
    CHECK_RC(ch->MethodWriteRC(subChannel, LW90B5_SET_REMAP_COMPONENTS, value));

    value = 0x0;
    value = FLD_SET_DRF(90B5, _LAUNCH_DMA, _DATA_TRANSFER_TYPE,
                        _NON_PIPELINED, value);
    value = FLD_SET_DRF(90B5, _LAUNCH_DMA, _FLUSH_ENABLE, _TRUE, value);
    if (surf->GetLayout() == Surface2D::BlockLinear)
    {
        value = FLD_SET_DRF(90B5, _LAUNCH_DMA, _SRC_MEMORY_LAYOUT,
                            _BLOCKLINEAR, value);
    }
    else
    {
        value = FLD_SET_DRF(90B5, _LAUNCH_DMA, _SRC_MEMORY_LAYOUT, _PITCH,
                            value);
    }
    value = FLD_SET_DRF(90B5, _LAUNCH_DMA, _DST_MEMORY_LAYOUT, _PITCH, value);
    value = FLD_SET_DRF(90B5, _LAUNCH_DMA, _MULTI_LINE_ENABLE, _FALSE, value);
    value = FLD_SET_DRF(90B5, _LAUNCH_DMA, _REMAP_ENABLE, _FALSE, value);
    if (multilineEnable)
    {
        // Here LineLength is in pixels, so colwert to bytes.
        // (Pitch is always in bytes.)
        CHECK_RC(ch->MethodWriteRC(subChannel, LW90B5_PITCH_OUT, LineLength*BytesPerPixel));

        value = FLD_SET_DRF(90B5, _LAUNCH_DMA, _MULTI_LINE_ENABLE, _TRUE,
                            value);
        value = FLD_SET_DRF(90B5, _LAUNCH_DMA, _REMAP_ENABLE, _TRUE, value);
    }
    else
    {
        // Here LineLength is in bytes already.
        CHECK_RC(ch->MethodWriteRC(subChannel, LW90B5_PITCH_OUT, LineLength));
    }

    cleanupInsertedMethods.Cancel();
    CHECK_RC(ch->EndInsertedMethodsRC());

    for (UINT32 subsurf = 0; subsurf < arraySize; subsurf++)
    {
        CHECK_RC(ch->BeginInsertedMethodsRC());
        Utility::CleanupOnReturn<Channel> cleanupInsertedMethods(
                pChannel, &Channel::CancelInsertedMethods);

        CHECK_RC(ch->MethodWriteRC(subChannel, LW90B5_OFFSET_IN_UPPER, LwU64_HI32(SrcOffset)));
        CHECK_RC(ch->MethodWriteRC(subChannel, LW90B5_OFFSET_IN_LOWER, LwU64_LO32(SrcOffset)));

        cleanupInsertedMethods.Cancel();
        CHECK_RC(ch->EndInsertedMethodsRC());

        for (UINT32 zIndex = 0; zIndex < depth; zIndex++)
        {
            CHECK_RC(ch->BeginInsertedMethodsRC());
            Utility::CleanupOnReturn<Channel> cleanupInsertedMethods(
                    pChannel, &Channel::CancelInsertedMethods);

            CHECK_RC(ch->MethodWriteRC(subChannel, LW90B5_SET_SRC_LAYER, zIndex));

            CHECK_RC(ch->MethodWriteRC(subChannel, LW90B5_OFFSET_OUT_UPPER, LwU64_HI32(DstOffset)));
            CHECK_RC(ch->MethodWriteRC(subChannel, LW90B5_OFFSET_OUT_LOWER, LwU64_LO32(DstOffset)));

            CHECK_RC(ch->MethodWriteRC(subChannel, LW90B5_LAUNCH_DMA, value));

            cleanupInsertedMethods.Cancel();
            CHECK_RC(ch->EndInsertedMethodsRC());

            if (m_wfiType == SLEEP)
            {
                Platform::SleepUS(1000000);
            }
            else
            {
                // Cheesy way to do this. We should actually check the notify
                CHECK_RC(ch->WaitForIdleRC());
            }

            if (multilineEnable)
            {
                // Here LineLength is in pixels, so colwert to bytes.
                // (DstOffset is in bytes.)
                DstOffset += LineLength*BytesPerPixel*LineCount;
            }
            else
            {
                // Here LineLength is in bytes already.
                DstOffset += LineLength*LineCount;
            }
        }

        SrcOffset += surf->GetArrayPitch();
    }

    // SLI system: Renable all the subdevices.
    if (gpuSubdeviceNum > 1)
    {
        CHECK_RC(ch->GetModsChannel()->WriteSetSubdevice(Channel::AllSubdevicesMask));
    }

    ch->CtxSwEnable(true);

    return OK;
}

