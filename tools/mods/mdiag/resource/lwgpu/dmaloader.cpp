/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "dmaloader.h"
#include "core/include/argread.h"
#include "mdiag/tests/gpu/trace_3d/tracechan.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "mdiag/tests/gpu/lwgpu_subchannel.h"
#include "core/include/channel.h"
#include "mdiag/lwgpu.h"
#include "mdiag/utils/lwgpu_classes.h"
#include "mdiag/utils/utils.h"
#include "lwos.h"
#include "core/include/platform.h"
#include "core/include/utility.h"
#include "ctrl/ctrl0080.h"
#include "class/cl90b5.h" // GF100_DMA_COPY
#include "class/clc3b5.h" // VOLTA_DMA_COPY_A

// -----------------------------------------------------------------------------
// Factory function to create DMA loader using appropriate engine
// -----------------------------------------------------------------------------
DmaLoader *DmaLoader::CreateDmaLoader
(
    const ArgReader *params,
    TraceChannel *channel
)
{
    DmaLoader *loader = nullptr;
    UINT32 classId = 0;
    LwRm* pLwRm = channel->GetLwRmPtr();
    SmcEngine *pSmcEngine = channel->GetSmcEngine();
    GpuDevice *pGpuDev = channel->GetCh()->GetLWGpu()->GetGpuDevice();

    EngineClasses::GetFirstSupportedClass(
            pLwRm,
            pGpuDev,
            "Ce",
            &classId);
    
    if (classId >= VOLTA_DMA_COPY_A)
        loader = new DmaLoaderVolta();
    else
        loader = new DmaLoaderGF100();

    UINT32 engineId = LW2080_ENGINE_TYPE_NULL;
    // EngineId for channels is relevant only starting from Ampere
    if (pGpuDev->GetSubdevice(0)->HasFeature(Device:: GPUSUB_HOST_REQUIRES_ENGINE_ID))
    {
        // Get the first available CE
        GpuSubdevice *pSubDev = pGpuDev->GetSubdevice(0);
        vector<UINT32> supportedCEs;
        if (channel->GetCh()->GetLWGpu()->GetSupportedCeNum(pSubDev, &supportedCEs, pLwRm) != OK)
            MASSERT(!"Error in GetSupportedCeNum\n");
        MASSERT(supportedCEs.size() != 0);
        engineId = MDiagUtils::GetCopyEngineId(supportedCEs[0]);
    }

    loader->m_TraceChannel = channel;
    // When creating object within this channel, either null params (GetDmaObjHandle) 
    // or UNSPECIFIED_CE (AllocateObject) is passed.
    // From Ampere-10, RM will deduce the engineId for an object from the channel handle. 
    // For the channel creation we pass the first available CE and hence 
    // null params/UNSPECIFIED_CE would also mean to use the first available CE
    loader->m_GpuChannel = params->ParamPresent("-load_new_channel") > 0 ?
                           channel->GetGpuResource()->GetDmaChannel(channel->GetVASpace(), params, pLwRm, pSmcEngine, engineId) :
                           channel->GetCh();
    loader->params = params;

    if (!loader->AllocateObject())
    {
        delete loader;
        loader = nullptr;
    }

    return loader;
}

// -----------------------------------------------------------------------------
// Public constructor
// -----------------------------------------------------------------------------
DmaLoader::DmaLoader()
    : m_TraceChannel(0), m_GpuChannel(0), m_DmaObjHandle(0), m_IsNewObjCreated(false), m_NotifierHandle(0),
    m_NotifierBase(0), m_NotifierGpuOffset(0), m_SrcSurf(0), m_Class(0),
    m_ChunkSize(1<<20), params(0)
{
}

// -----------------------------------------------------------------------------
// Public destructor
// -----------------------------------------------------------------------------
DmaLoader::~DmaLoader()
{
    LWGpuResource* lwgpu = m_TraceChannel->GetGpuResource();
    lwgpu->DeleteSurface(m_NotifierBase);

    // delete dma handle if it's created in DmaLoader object
    if (m_IsNewObjCreated && m_DmaObjHandle)
    {
        m_GpuChannel->DestroyObject(m_DmaObjHandle);
        m_DmaObjHandle = 0;
    }
    if (m_SrcSurf)
    {
        m_SrcSurf->Free();
        delete m_SrcSurf;
        m_SrcSurf = 0;
    }
}

// -----------------------------------------------------------------------------
// Allocate dma object
// -----------------------------------------------------------------------------
bool DmaLoader::AllocateObject()
{
    UINT32 dmaClass = GetClassId();

    if (dmaClass == 0)
    {
        ErrPrintf("Current class is not supported for dma loading\n");
        return false;
    }

    if (params->ParamPresent("-load_new_channel") > 0)
    {
        // In "createDmaChannel" mode, the GpuResource creates
        // the separate DMA object, so get it from there.
        m_DmaObjHandle = m_TraceChannel->GetGpuResource()->GetDmaObjHandle(
            m_TraceChannel->GetVASpace(), dmaClass, m_TraceChannel->GetLwRmPtr(), &m_Class);
    }
    else
    {
        // check if dma obj already created in current channel's subchannels
        bool foundObj = false;
        const vector<UINT32> ceClasses = EngineClasses::GetClassVec("Ce");
        auto it = ceClasses.begin();
        for (; it != ceClasses.end(); it++)
        {
            if (m_GpuChannel->GetSubChannelHandle(*it, &m_DmaObjHandle) == OK)
            {
                InfoPrintf("Reusing subchannel(handle 0x%x class 0x%x) for dma loading.\n",
                    m_DmaObjHandle, *it);
                m_Class = *it;
                foundObj = true;
                break;
            }
        }

        // create dma obj if necessary
        if (!foundObj)
        {
            if (EngineClasses::IsClassType("Ce", dmaClass))
            {
                //need to create CEObjCreateParam if in CE modeobj if necessary
                void* objCreateParam = 0;
                CEObjCreateParam* pCeParam = new CEObjCreateParam
                    (CEObjCreateParam::UNSPECIFIED_CE,dmaClass);
                if(pCeParam->GetObjCreateParam(m_GpuChannel, &objCreateParam) != OK)
                {
                    ErrPrintf("Could not get CE parameter buffer! Try -load_new_channel\n");
                    return false;
                }

                m_DmaObjHandle = m_GpuChannel->CreateObject(dmaClass,objCreateParam, &m_Class);
                delete pCeParam;

                if(!(m_DmaObjHandle))
                {
                    ErrPrintf("Couldn't create dma ce object!\n");
                    return false;
                }

            }
            else
            {
                m_DmaObjHandle = m_GpuChannel->CreateObject(dmaClass, 0, &m_Class);
            }
            m_IsNewObjCreated = true;
        }
    }

    if (!m_DmaObjHandle)
    {
        ErrPrintf("Couldn't create dma loader object!\n");
        return false;
    }

    return true;
}

// -----------------------------------------------------------------------------
// Bind a buffer to be uploaded, we need to load the data to a temp surface
// in step one, then we use HW engine to performe the copy in step two
// -----------------------------------------------------------------------------
RC DmaLoader::BindSrcBuf(void *srcBuf, size_t size, UINT32 subdev)
{
    RC rc = OK;
    MASSERT(srcBuf != 0);
    if (m_SrcSurf != 0)
    {
        //m_SrcSurf->Unmap();
        m_SrcSurf->Free();
        delete m_SrcSurf;
        m_SrcSurf = 0;
    }
    m_SrcSurf = new MdiagSurf;
    m_SrcSurf->SetColorFormat(ColorUtils::Y8);
    m_SrcSurf->SetForceSizeAlloc(true);
    m_SrcSurf->SetName(GetTypeName(SURFACE_TYPE_COLOR));
    m_SrcSurf->SetWidth(size);
    m_SrcSurf->SetPitch(size);
    m_SrcSurf->SetHeight(1);
    m_SrcSurf->SetAlignment(512);
    m_SrcSurf->SetLayout(Surface2D::Pitch);
    m_SrcSurf->SetLocation(Memory::Coherent);
    m_SrcSurf->SetGpuVASpace(m_GpuChannel->GetVASpaceHandle());
    CHECK_RC(m_SrcSurf->Alloc(m_GpuChannel->GetLWGpu()->GetGpuDevice(), m_TraceChannel->GetLwRmPtr()));
    m_SrcSurf->BindGpuChannel(m_GpuChannel->ChannelHandle());

    // Copy buffer to the temp surface using CPU
    CHECK_RC(m_SrcSurf->MapRegion(0, size, subdev));
    MDiagUtils::SetMemoryTag tag(m_GpuChannel->GetLWGpu()->GetGpuDevice(), "DMA uploading");
    Platform::VirtualWr(m_SrcSurf->GetAddress(), srcBuf, size);
    m_SrcSurf->Unmap();

    return rc;
}

DmaLoaderGF100::DmaLoaderGF100()
{
}

DmaLoaderGF100::~DmaLoaderGF100()
{
}

UINT32 DmaLoaderGF100::GetClassId() const
{
    UINT32 firstCl = 0;
    LwRm* pLwRm = m_TraceChannel->GetLwRmPtr();
    GpuDevice *pGpuDev = m_GpuChannel->GetLWGpu()->GetGpuDevice();

    EngineClasses::GetFirstSupportedClass(
        pLwRm,
        pGpuDev,
        "Ce",
        &firstCl);

    return firstCl;
}

bool DmaLoaderGF100::ClassMatched(UINT32 classId)
{
    return EngineClasses::IsClassType("Ce", classId);
}

// -----------------------------------------------------------------------------
// Allocate public notifier, need to call every time before binding buffer
// -----------------------------------------------------------------------------
RC DmaLoaderGF100::AllocateNotifier(UINT32 subdev)
{
    RC rc;
    _PAGE_LAYOUT Layout = GetPageLayoutFromParams(params, 0);

    rc = m_GpuChannel->AllocSurfaceRC(
            Memory::ReadWrite, 32,
            _DMA_TARGET_COHERENT,
            Layout,
            &m_NotifierHandle,
            &m_NotifierBase,
            0,
            &m_NotifierGpuOffset,
            subdev);
    if (rc != OK)
    {
        ErrPrintf("Couldn't alloc dma notifier buffer: %s\n", rc.Message());
        return rc;
    }

    return OK;
}

RC DmaLoaderGF100::FillSurface
(
    MdiagSurf *dstSurf,
    UINT64 offset,
    UINT32 filledValue,
    UINT64 size,
    UINT32 subdev
)
{
    MASSERT(EngineClasses::IsClassType("Ce", m_Class));

    LWGpuChannel* ch = m_GpuChannel;

    // The DMA channel might be shared between threads, so lock the channel
    // until the read is done to prevent timing issues.  (See bug 1685132.)
    Tasker::MutexHolder mh(ch->GetMutex());

    ch->CtxSwEnable(false);
    UINT64 dstOffset = dstSurf->GetCtxDmaOffsetGpu() + offset;

    UINT32 LineCount = 1;
    UINT32 LineLength = size;

    RC rc = OK;
    UINT32 value = 0x0;

    GpuDevice* pGpuDev = ch->GetLWGpu()->GetGpuDevice();
    MASSERT(pGpuDev);
    UINT32 gpuSubdeviceNum = pGpuDev->GetNumSubdevices();
    if(gpuSubdeviceNum > 1)
    {
        MASSERT(subdev < gpuSubdeviceNum);
        UINT32 subdevMask = 1 << subdev;
        CHECK_RC(ch->GetModsChannel()->WriteSetSubdevice(subdevMask));
    }

    Channel* pChannel = ch->GetModsChannel();
    MASSERT(pChannel);
    pChannel->SetPrivEnable(true);

    UINT32 subChannel = 0;
    CHECK_RC(ch->GetSubChannelNumClass(m_DmaObjHandle, &subChannel, 0));
    CHECK_RC(ch->SetObjectRC(subChannel,m_DmaObjHandle));
    if(m_Class == GF100_DMA_COPY)
    {
        CHECK_RC(ch->MethodWriteRC(subChannel, LW90B5_SET_APPLICATION_ID, LW90B5_SET_APPLICATION_ID_ID_NORMAL));
    }

    CHECK_RC(SendDstBasicKindMethod(dstSurf));

    // set linelength, send launch dma
    CHECK_RC(ch->MethodWriteRC(subChannel, LW90B5_LINE_COUNT, LineCount));
    CHECK_RC(ch->MethodWriteRC(subChannel, LW90B5_LINE_LENGTH_IN, LineLength));

    CHECK_RC(ch->MethodWriteRC(subChannel, LW90B5_OFFSET_OUT_UPPER, LwU64_HI32(dstOffset)));
    CHECK_RC(ch->MethodWriteRC(subChannel, LW90B5_OFFSET_OUT_LOWER, LwU64_LO32(dstOffset)));

    // set remapping
    CHECK_RC(ch->MethodWriteRC(subChannel, LW90B5_SET_REMAP_CONST_A, filledValue));
    CHECK_RC(ch->MethodWriteRC(subChannel, LW90B5_SET_REMAP_COMPONENTS, 0x4));

    value = 0x0;
    value = FLD_SET_DRF(90B5, _LAUNCH_DMA, _DATA_TRANSFER_TYPE,
                        _NON_PIPELINED, value);
    value = FLD_SET_DRF(90B5, _LAUNCH_DMA, _FLUSH_ENABLE, _TRUE, value);

    value = FLD_SET_DRF(90B5, _LAUNCH_DMA, _SRC_MEMORY_LAYOUT, _PITCH, value);
    value = FLD_SET_DRF(90B5, _LAUNCH_DMA, _DST_MEMORY_LAYOUT, _PITCH, value);
    value = FLD_SET_DRF(90B5, _LAUNCH_DMA, _MULTI_LINE_ENABLE, _FALSE, value);
    value = FLD_SET_DRF(90B5, _LAUNCH_DMA, _REMAP_ENABLE, _TRUE, value);

    CHECK_RC(ch->MethodWriteRC(subChannel, LW90B5_LAUNCH_DMA, value));

    // SLI system: Renable all the subdevices.
    if (gpuSubdeviceNum > 1)
    {
        CHECK_RC(ch->GetModsChannel()->WriteSetSubdevice(Channel::AllSubdevicesMask));
    }

    ch->CtxSwEnable(true);

    // Cheesy way to do this. We should actually check the notify
    CHECK_RC(ch->WaitForIdleRC());
    return OK;
}

RC DmaLoaderGF100::LoadSurface
(
    MdiagSurf *dstSurf,
    UINT64 offset,
    UINT32 subdev
)
{
    MASSERT(!"DmaLoaderGF100::LoadSurface is not supported yet!\n");
    return OK;
}

RC DmaLoaderGF100::SendDstBasicKindMethod(MdiagSurf *dstSurf)
{
    return OK;
}

DmaLoaderVolta::DmaLoaderVolta()
{
}

DmaLoaderVolta::~DmaLoaderVolta()
{
}

RC DmaLoaderVolta::SendDstBasicKindMethod(MdiagSurf *dstSurf)
{
    MASSERT(dstSurf);

    RC rc = OK;

    DebugPrintf("DmaLoaderVolta::SendDstBasicKindMethod start\n");

    UINT32 dstPhysMode = 0;
    switch( dstSurf->GetLocation() )
    {
        case Memory::Fb:
            dstPhysMode = DRF_DEF(C3B5, _SET_DST_PHYS_MODE, _TARGET, _LOCAL_FB);
            break;

        case Memory::Coherent:
            dstPhysMode = DRF_DEF(C3B5, _SET_DST_PHYS_MODE, _TARGET, _COHERENT_SYSMEM);
            break;

        case Memory::NonCoherent:
            dstPhysMode = DRF_DEF(C3B5, _SET_DST_PHYS_MODE, _TARGET, _NONCOHERENT_SYSMEM);
            break;

        default:
            MASSERT(!"Unsupported location!\n");
    }

    LWGpuChannel* ch = m_GpuChannel;
    ch->CtxSwEnable(false);

    UINT32 subChannel = 0;
    CHECK_RC(ch->GetSubChannelNumClass(m_DmaObjHandle, &subChannel, 0));
    CHECK_RC(ch->SetObjectRC(subChannel, m_DmaObjHandle));

    CHECK_RC(ch->MethodWriteRC(subChannel, LWC3B5_SET_DST_PHYS_MODE, dstPhysMode));

    DebugPrintf("DmaLoaderVolta::SendDstBasicKindMethod complete\n");

    return OK;
}
