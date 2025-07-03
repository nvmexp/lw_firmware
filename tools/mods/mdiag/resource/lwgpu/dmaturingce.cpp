/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "dmaturingce.h"
#include "mdiag/tests/gpu/lwgpu_subchannel.h"
#include "class/clc5b5.h" // TURING_DMA_COPY_A

// --------------------------------------------------------------------------
//  c'tor
// --------------------------------------------------------------------------
DmaReaderTuringCE::DmaReaderTuringCE
(
    WfiType wfiType,
    LWGpuChannel* ch,
    UINT32 size
)
: DmaReaderGF100CE(wfiType, ch, size)
{
}

// --------------------------------------------------------------------------
//  d'tor
// --------------------------------------------------------------------------
DmaReaderTuringCE::~DmaReaderTuringCE()
{
}

// --------------------------------------------------------------------------
//  read line from dma to sysmem
// --------------------------------------------------------------------------
RC DmaReaderTuringCE::ReadBlocklinearToPitch
(
    UINT64 sourceVirtAddr,
    UINT32 x,
    UINT32 y,
    UINT32 z,
    UINT32 lineLength,
    UINT32 lineCount,
    UINT32 gpuSubdevice,
    const Surface2D* srcSurf
)
{
    RC rc;
    MASSERT(srcSurf->GetLayout() == Surface2D::BlockLinear);

    DebugPrintf("DmaReaderTuringCE::ReadBlocklinear() start\n");

    LWGpuChannel* ch = m_Channel;
    ch->CtxSwEnable(false);

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

    UINT32 subChannel = 0;
    CHECK_RC(ch->GetSubChannelNumClass(m_DmaObjHandle, &subChannel, 0));
    CHECK_RC(ch->SetObjectRC(subChannel, m_DmaObjHandle));

    CHECK_RC(ch->BeginInsertedMethodsRC());
    Utility::CleanupOnReturn<Channel> cleanupInsertedMethods(
            pChannel, &Channel::CancelInsertedMethods);

    CHECK_RC(ch->MethodWriteRC(subChannel, LWC5B5_LINE_COUNT, lineCount));
    CHECK_RC(ch->MethodWriteRC(subChannel, LWC5B5_LINE_LENGTH_IN, lineLength));

    UINT32 blocks =
          DRF_NUM(C5B5, _SET_SRC_BLOCK_SIZE, _WIDTH, srcSurf->GetLogBlockWidth())
        | DRF_NUM(C5B5, _SET_SRC_BLOCK_SIZE, _HEIGHT, srcSurf->GetLogBlockHeight())
        | DRF_NUM(C5B5, _SET_SRC_BLOCK_SIZE, _DEPTH, srcSurf->GetLogBlockDepth())
        | DRF_DEF(C5B5, _SET_SRC_BLOCK_SIZE, _GOB_HEIGHT, _GOB_HEIGHT_FERMI_8);

    CHECK_RC(ch->MethodWriteRC(subChannel, LWC5B5_SET_SRC_BLOCK_SIZE, blocks));
    CHECK_RC(ch->MethodWriteRC(subChannel, LWC5B5_SRC_ORIGIN_X, x));
    CHECK_RC(ch->MethodWriteRC(subChannel, LWC5B5_SRC_ORIGIN_Y, y));
    CHECK_RC(ch->MethodWriteRC(subChannel, LWC5B5_SET_SRC_LAYER, z));
    CHECK_RC(ch->MethodWriteRC(subChannel, LWC5B5_SET_SRC_WIDTH, srcSurf->GetAllocWidth()));
    CHECK_RC(ch->MethodWriteRC(subChannel, LWC5B5_SET_SRC_HEIGHT, srcSurf->GetAllocHeight()));
    CHECK_RC(ch->MethodWriteRC(subChannel, LWC5B5_SET_SRC_DEPTH, srcSurf->GetDepth()));

    UINT32 value =
          DRF_DEF(C5B5, _SET_REMAP_COMPONENTS, _DST_X, _SRC_X)
        | DRF_DEF(C5B5, _SET_REMAP_COMPONENTS, _DST_Y, _SRC_Y)
        | DRF_DEF(C5B5, _SET_REMAP_COMPONENTS, _DST_Z, _SRC_Z)
        | DRF_DEF(C5B5, _SET_REMAP_COMPONENTS, _DST_W, _SRC_W);

    switch (srcSurf->GetBytesPerPixel())
    {
    case 1:
        value |= DRF_DEF(C5B5, _SET_REMAP_COMPONENTS, _NUM_SRC_COMPONENTS, _ONE);
        value |= DRF_DEF(C5B5, _SET_REMAP_COMPONENTS, _NUM_DST_COMPONENTS, _ONE);
        value |= DRF_DEF(C5B5, _SET_REMAP_COMPONENTS, _COMPONENT_SIZE, _ONE);
        break;
    case 2:
        value |= DRF_DEF(C5B5, _SET_REMAP_COMPONENTS, _NUM_SRC_COMPONENTS, _TWO);
        value |= DRF_DEF(C5B5, _SET_REMAP_COMPONENTS, _NUM_DST_COMPONENTS, _TWO);
        value |= DRF_DEF(C5B5, _SET_REMAP_COMPONENTS, _COMPONENT_SIZE, _ONE);
        break;
    case 3:
        value |= DRF_DEF(C5B5, _SET_REMAP_COMPONENTS, _NUM_SRC_COMPONENTS, _THREE);
        value |= DRF_DEF(C5B5, _SET_REMAP_COMPONENTS, _NUM_DST_COMPONENTS, _THREE);
        value |= DRF_DEF(C5B5, _SET_REMAP_COMPONENTS, _COMPONENT_SIZE, _ONE);
        break;
    case 4:
        value |= DRF_DEF(C5B5, _SET_REMAP_COMPONENTS, _NUM_SRC_COMPONENTS, _FOUR);
        value |= DRF_DEF(C5B5, _SET_REMAP_COMPONENTS, _NUM_DST_COMPONENTS, _FOUR);
        value |= DRF_DEF(C5B5, _SET_REMAP_COMPONENTS, _COMPONENT_SIZE, _ONE);
        break;
    case 8:
        value |= DRF_DEF(C5B5, _SET_REMAP_COMPONENTS, _NUM_SRC_COMPONENTS, _FOUR);
        value |= DRF_DEF(C5B5, _SET_REMAP_COMPONENTS, _NUM_DST_COMPONENTS, _FOUR);
        value |= DRF_DEF(C5B5, _SET_REMAP_COMPONENTS, _COMPONENT_SIZE, _TWO);
        break;
    case 16:
        value |= DRF_DEF(C5B5, _SET_REMAP_COMPONENTS, _NUM_SRC_COMPONENTS, _FOUR);
        value |= DRF_DEF(C5B5, _SET_REMAP_COMPONENTS, _NUM_DST_COMPONENTS, _FOUR);
        value |= DRF_DEF(C5B5, _SET_REMAP_COMPONENTS, _COMPONENT_SIZE, _FOUR);
        break;
    default:
        return RC::SOFTWARE_ERROR;
        break;
    }
    CHECK_RC(ch->MethodWriteRC(subChannel, LWC5B5_SET_REMAP_COMPONENTS, value));
    CHECK_RC(ch->MethodWriteRC(subChannel, LWC5B5_PITCH_OUT,
        lineLength * srcSurf->GetBytesPerPixel()));

    CHECK_RC(ch->MethodWriteRC(subChannel, LWC5B5_OFFSET_IN_UPPER, LwU64_HI32(sourceVirtAddr)));
    CHECK_RC(ch->MethodWriteRC(subChannel, LWC5B5_OFFSET_IN_LOWER, LwU64_LO32(sourceVirtAddr)));

    CHECK_RC(ch->MethodWriteRC(subChannel, LWC5B5_OFFSET_OUT_UPPER, LwU64_HI32(m_DstGpuOffset)));
    CHECK_RC(ch->MethodWriteRC(subChannel, LWC5B5_OFFSET_OUT_LOWER, LwU64_LO32(m_DstGpuOffset)));

    value =
          DRF_DEF(C5B5, _LAUNCH_DMA, _DATA_TRANSFER_TYPE, _NON_PIPELINED)
        | DRF_DEF(C5B5, _LAUNCH_DMA, _FLUSH_ENABLE, _TRUE)
        | DRF_DEF(C5B5, _LAUNCH_DMA, _SRC_MEMORY_LAYOUT, _BLOCKLINEAR)
        | DRF_DEF(C5B5, _LAUNCH_DMA, _DST_MEMORY_LAYOUT, _PITCH)
        | DRF_DEF(C5B5, _LAUNCH_DMA, _MULTI_LINE_ENABLE, _TRUE)
        | DRF_DEF(C5B5, _LAUNCH_DMA, _REMAP_ENABLE, _TRUE);
    CHECK_RC(ch->MethodWriteRC(subChannel, LWC5B5_LAUNCH_DMA, value));

    cleanupInsertedMethods.Cancel();
    CHECK_RC(ch->EndInsertedMethodsRC());

    // SLI system: Renable all the subdevices.
    if (gpuSubdeviceNum > 1)
    {
        CHECK_RC(ch->GetModsChannel()->WriteSetSubdevice(Channel::AllSubdevicesMask));
    }

    ch->CtxSwEnable(true);
    CHECK_RC(ch->WaitForIdleRC());

    DebugPrintf("DmaReaderTuringCE::ReadBlocklinear complete\n");

    return OK;
}
