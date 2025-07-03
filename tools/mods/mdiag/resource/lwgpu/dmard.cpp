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

#include "dmard.h"
#include "dmagf100ce.h"
#include "dmavoltace.h"
#include "dmaturingce.h"
#include "mdiag/tests/gpu/lwgpu_subchannel.h"
#include "mdiag/utils/lwgpu_classes.h"
#include "lwos.h"
#include "ctrl/ctrl0080.h"
#include "mdiag/tests/gpu/trace_3d/tracechan.h"
#include "class/clc3b5.h" // VOLTA_DMA_COPY_A
#include "class/clc5b5.h" // TURING_DMA_COPY_A

#define MAX_SUBCHANNELS 8

namespace
{
    // Get CE type from cmdline offset arg. Only CE0-LAST_CE (0-last_CE#) valid.
    // Get UNSPECIFIED_CE by default if no arg specified.
    // @return false for errors
    static bool GetCEType(const ArgReader* pParams, CEObjCreateParam::CEType* pType)
    {
        MASSERT(pType);
        UINT32 engOff = pParams->ParamUnsigned("-dmacopy_ce_offset", UINT32(CEObjCreateParam::UNSPECIFIED_CE));
        CEObjCreateParam::CEType ceType = CEObjCreateParam::CEType(engOff);
        if (ceType != CEObjCreateParam::UNSPECIFIED_CE && ceType > CEObjCreateParam::LAST_CE)
        {
            ErrPrintf("Invalid argument value %d from -dmacopy_ce_offset, only %d-%d supported.\n",
                UINT32(ceType),
                UINT32(CEObjCreateParam::CE0),
                UINT32(CEObjCreateParam::LAST_CE)
                );
            return false;
        }
        *pType = ceType;
        return true;
    }
}

DmaReader::EngineType DmaReader::GetEngineType(const ArgReader* params, LWGpuResource* lwgpu)
{
    if (params->ParamPresent("-dma_check") > 0)
    {
        UINT32 dmaArgVal = params->ParamUnsigned("-dma_check");
        switch(dmaArgVal)
        {
            case 5:
                return CE;
                break;
            case 6:
                return GPUCRC_FECS;
                break;
            default:
                MASSERT(!"Invalid DMA reader type!\n");
        }
    }
    return CE;
}

DmaReader::WfiType DmaReader::GetWfiType(const ArgReader* params)
{
    if (params->ParamPresent("-wfi_method") > 0 &&
        params->ParamUnsigned("-wfi_method") == WFI_SLEEP)
    {
        return SLEEP;
    }
    else
    {
        return NOTIFY;
    }
}

// --------------------------------------------------------------------------
//  c'tor
// --------------------------------------------------------------------------
DmaReader::DmaReader(WfiType wfiType, LWGpuChannel* ch,
                     UINT32 size, EngineType engineType)
    : m_Channel(ch), m_DmaObjHandle(0),
      m_DstBufSize(size), m_DstDmaHandle(0), m_DstDmaBase(0), m_DstGpuOffset(0),
      m_NotifierHandle(0), m_NotifierBase(0), m_NotifierGpuOffset(0), m_ChClass(0), m_Params(0),
      m_wfiType(wfiType), m_EngineType(engineType), m_pSurface(0), m_CopyDmaHandle(0),
      m_CopyGpuOffset(0), m_CopyLocation(Memory::Fb),
      m_IsNewChannelCreated(false), m_IsNewObjCreated(false)
{
}

// --------------------------------------------------------------------------
//  d'tor
// --------------------------------------------------------------------------
DmaReader::~DmaReader()
{
    LWGpuResource* lwgpu = m_Channel->GetLWGpu();
    lwgpu->DeleteSurface(m_DstDmaBase);
    lwgpu->DeleteSurface(m_NotifierBase);

    if (m_pSurface)
    {
        delete m_pSurface;
    }

    if (m_IsNewObjCreated)
    {
        if (m_DmaObjHandle)
        {
            m_Channel->DestroyObject(m_DmaObjHandle);
            m_DmaObjHandle = 0;
        }
        m_IsNewObjCreated = false;
    }
}

// --------------------------------------------------------------------------
//  factory function to create appropriate dma reader type
// --------------------------------------------------------------------------
DmaReader* DmaReader::CreateDmaReader
(
    EngineType engineType,
    WfiType wfiType,
    LWGpuResource* lwgpu,
    LWGpuChannel* channel,
    UINT32 size,
    const ArgReader* params,
    LwRm *pLwRm,
    SmcEngine *pSmcEngine,
    UINT32 subdev
)
{
    DmaReader* reader = NULL;
    UINT32 classId = 0;
    MASSERT(lwgpu);
    GpuDevice *pGpuDev = lwgpu->GetGpuDevice();

    EngineClasses::GetFirstSupportedClass(
        pLwRm,
        pGpuDev,
        "Ce",
        &classId);

    UINT32 engineId = LW2080_ENGINE_TYPE_NULL;

    // EngineId for channels is relevant only starting from Ampere
    if (pGpuDev->GetSubdevice(0)->HasFeature(Device:: GPUSUB_HOST_REQUIRES_ENGINE_ID))
    {
        // Get the first available CE
        GpuSubdevice *pSubDev = pGpuDev->GetSubdevice(0);
        vector<UINT32> supportedCEs;
        if (lwgpu->GetSupportedCeNum(pSubDev, &supportedCEs, pLwRm) != OK)
            MASSERT(!"Error in GetSupportedCeNum\n");
        MASSERT(supportedCEs.size() != 0);
        CEObjCreateParam::CEType ceType = CEObjCreateParam::UNSPECIFIED_CE;
        if (!GetCEType(params, &ceType))
        {
            return 0;
        }
        UINT32 ceOff = LW2080_ENGINE_TYPE_COPY0;
        if (ceType != CEObjCreateParam::UNSPECIFIED_CE)
        {
            ceOff += UINT32(ceType);
        }
        engineId = supportedCEs[0] + ceOff;
    }

    switch(engineType)
    {
        case CE:
        case GPUCRC_FECS:
            // dump images using ce
            if (classId >= TURING_DMA_COPY_A)
                reader = new DmaReaderTuringCE(wfiType, channel, size);
            else if (classId >= VOLTA_DMA_COPY_A)
                reader = new DmaReaderVoltaCE(wfiType, channel, size);
            else
                reader = new DmaReaderGF100CE(wfiType, channel, size);
            break;
        default:
            MASSERT(!"Invalid DMA reader type!\n");
    }
    reader->m_LWGpuResource = lwgpu;
    reader->m_Subdev = subdev;
    reader->m_Params = params;
    // When creating object within this channel, either null params (GetDmaObjHandle) 
    // or UNSPECIFIED_CE (AllocateObject) is passed.
    // From Ampere-10, RM will deduce the engineId for an object from the channel handle. 
    // For the channel creation we pass the first available CE and hence 
    // null params/UNSPECIFIED_CE would also mean to use the first available CE
    reader->SetCreateDmaChannel(pLwRm, pSmcEngine, engineId);

    return reader;
}

// --------------------------------------------------------------------------
//  read the specified number of bytes from dma
// --------------------------------------------------------------------------
RC DmaReader::Read(UINT32 dmaCtxHandle, UINT64 pos, UINT32 size, UINT32 subdev, const Surface2D* srcSurf)
{
    // The DMA channel might be shared between threads, so lock the channel
    // until the read is done to prevent timing issues.  (See bug 1685132.)
    Tasker::MutexHolder mh(m_Channel->GetMutex());

    return ReadLine(dmaCtxHandle,       // source buffer handle
        pos,                // source buffer position
        0,                  // source buffer pitch
        1,                  // line count
        size,               // line length
        subdev,
        srcSurf);
}

// --------------------------------------------------------------------------
//  create a new channel or reuse current channel
// --------------------------------------------------------------------------
bool DmaReader::IsNewChannelNeeded()
{
    if (!m_Channel)
    {
        return true;
    }

    // Alloc a new channel for the reader if we're passed
    // option -crc_new_channel or using buffer-based runlists.
    bool createNewChannel = m_Params &&
             (m_Params->ParamPresent("-crc_new_channel") ||
              m_Channel->GetModsChannel()->GetRunlistChannelWrapper());
    if (createNewChannel)
    {
        return true;
    }

    if (EngineClasses::IsGpuFamilyClassOrLater(
        m_Channel->GetChannelClass(), LWGpuClasses::GPU_CLASS_KEPLER))
    {
        if (m_EngineType == CE) 
        {
            // Copy engines can't share channel with other engines except gr
            // In SMC mode there is no GrCE hence new channel has to be 
            // created for CE
            if ((m_Channel->HasGrEngineObjectCreated(true) &&
                !m_LWGpuResource->IsSMCEnabled()) 
                ||
                m_Channel->HasCopyEngineObjectCreated(true))
                return false;
            else
                return true;
        }
        // Not sure about other cases, go default path
    }

    return false;
}

// --------------------------------------------------------------------------
//  allocates the dma transfer obj
// --------------------------------------------------------------------------
bool DmaReader::AllocateObject()
{
    if (m_DmaObjHandle) return true;
    UINT32 dma_class = GetClassId();

    if (dma_class == 0)
    {
        ErrPrintf("Current class is not supported for dma check\n");
        return false;
    }

    CEObjCreateParam::CEType ceType = CEObjCreateParam::UNSPECIFIED_CE;
    if (!GetCEType(m_Params, &ceType))
    {
        return false;
    }
    if (CEObjCreateParam::UNSPECIFIED_CE == ceType)
    {
        if (m_IsNewChannelCreated && m_EngineType != GPUCRC_FECS)
        {
            m_DmaObjHandle = m_Channel->GetLWGpu()->GetDmaObjHandle(
                m_Channel->GetVASpace(), dma_class, m_Channel->GetLwRmPtr(), &m_ChClass);
            return true;
        }
    }
    // check if dma obj already created in current channel's subchannels
    const vector<UINT32> ceClasses = EngineClasses::GetClassVec("Ce");
    auto it = ceClasses.begin();
    for (; it != ceClasses.end(); it++)
    {
        if (m_Channel->GetSubChannelHandle(*it, &m_DmaObjHandle) == OK)
        {
            InfoPrintf("Reusing subchannel(handle 0x%x class 0x%x) for dma check.\n",
                m_DmaObjHandle, *it);
            m_ChClass = *it;
            break;
        }
    }
    if (it == ceClasses.end())
    {
        // create dma obj if necessary
        LWGpuChannel* ch = m_Channel;

        if (m_EngineType == CE)
        {
            void* objCreateParam = 0;
            CEObjCreateParam::CEType ceType = CEObjCreateParam::UNSPECIFIED_CE;
            if (!GetCEType(m_Params, &ceType))
            {
                return false;
            }
            CEObjCreateParam* pCeParam = new CEObjCreateParam
                (ceType, dma_class);
            if (pCeParam->GetObjCreateParam(ch, &objCreateParam) != OK)
            {
                ErrPrintf("Couldn't get CE parameter buffer!  Try using option -crc_new_channel.\n");
                return false;
            }

            m_DmaObjHandle = ch->CreateObject(dma_class, objCreateParam, &m_ChClass);
            delete pCeParam;

            // tag it as newly created subchannel, free it in destructor
            MASSERT(!m_IsNewObjCreated);
            m_IsNewObjCreated = true;
        }
        else if (m_EngineType == GPUCRC_FECS)
        {
            // using PRI: class object not needed
            m_DmaObjHandle = 0;
        }
        else
        {
            m_DmaObjHandle = ch->CreateObject(dma_class, 0, &m_ChClass);

            // tag it as newly created subchannel, free it in destructor
            MASSERT(!m_IsNewObjCreated);
            m_IsNewObjCreated = true;
        }

    }

    if ((!m_DmaObjHandle) && (m_EngineType != GPUCRC_FECS))
    {
        WarnPrintf("Couldn't create dma reader object!\n");
        return false;
    }

    return true;
}

// --------------------------------------------------------------------------
//  allocates dma notifier and handles
// --------------------------------------------------------------------------
bool DmaReader::AllocateNotifier(_PAGE_LAYOUT Layout)
{
    RC rc;
    LWGpuChannel* ch = m_Channel;

    rc = ch->AllocSurfaceRC(
        Memory::ReadWrite, 32,
        _DMA_TARGET_COHERENT,
        Layout,
        &m_NotifierHandle,
        &m_NotifierBase,
        0,
        &m_NotifierGpuOffset,
        m_Subdev);
    if (rc != OK)
    {
        ErrPrintf("Couldn't alloc dmacheck notify dma buffer: %s\n", rc.Message());
        return false;
    }

    return true;
}

// --------------------------------------------------------------------------
//  allocates destination dma buffer and handles
// --------------------------------------------------------------------------
bool DmaReader::AllocateDstBuffer(_PAGE_LAYOUT Layout, UINT32 size, _DMA_TARGET target)
{
    RC rc;
    LWGpuChannel* ch = m_Channel;

    rc = ch->AllocSurfaceRC(
        Memory::ReadWrite, size,
        target,
        Layout,
        &m_DstDmaHandle,
        &m_DstDmaBase,
        0,
        &m_DstGpuOffset,
        m_Subdev);
    if (rc != OK)
    {
        ErrPrintf("Couldn't alloc dmacheck dst dma buffer: %s\n", rc.Message());
        return false;
    }

    switch (target)
    {
        case _DMA_TARGET_VIDEO:
            m_DstLocation = Memory::Fb;
            break;

        case _DMA_TARGET_COHERENT:
            m_DstLocation = Memory::Coherent;
            break;

        case _DMA_TARGET_NONCOHERENT:
            m_DstLocation = Memory::NonCoherent;
            break;

        default:
            ErrPrintf("Invalid Target");
            return false;
    }

    return true;
}

// --------------------------------------------------------------------------
//  poll dma transfer completion
// --------------------------------------------------------------------------
bool DmaReader::PollDmaDone(void *pVoidPollArgs)
{
    PollDmaDoneArgs *pPollArgs = (PollDmaDoneArgs *)pVoidPollArgs;
    UINT32 payload = MEM_RD32(pPollArgs->pSemaphorePayload);
    return ((payload & 0xFFFF) == (PollDmaDoneArgs::ExpectedPayload & 0xFFFF));
}

void DmaReader::SetCreateDmaChannel(LwRm* pLwRm, SmcEngine *pSmcEngine, UINT32 engineId)
{
    if (IsNewChannelNeeded())
    {
        // create a new channel

        shared_ptr<VaSpace> pVaSpace;
        if (m_Channel)
        {
            pVaSpace = m_Channel->GetVASpace();
        }
        else
        {
            WarnPrintf("DmaReader doesn't bind to a channel!\n");

            if (m_Params->ParamPresent("-address_space_name"))
            {
                string name = m_Params->ParamStr("-address_space_name");
                pVaSpace =  m_LWGpuResource->GetVaSpaceManager(pLwRm)->GetResourceObject(LWGpuResource::TEST_ID_GLOBAL, name);
            }
            else
            {
                // use the default VAS with handle 0
                // test Id doesn't matter here
                UINT32 testId = 0;
                string name = "default";
                pVaSpace = m_LWGpuResource->GetVaSpaceManager(pLwRm)->GetResourceObject(testId, name);

                // not create yet
                if (pVaSpace == nullptr)
                {
                    pVaSpace = m_LWGpuResource->GetVaSpaceManager(pLwRm)->CreateResourceObject(testId, name);
                }
            }
        }
        m_Channel = m_LWGpuResource->GetDmaChannel(pVaSpace, m_Params, pLwRm, pSmcEngine, engineId);

        // tag it as newly created channel, free it in destructor
        MASSERT(!m_IsNewChannelCreated);
        m_IsNewChannelCreated = true;
    }
}

int DmaReader::BindSurface ( UINT32 handle )
{
    // If a new channel is created, the surfaces to be checked must
    // be bound to it.
    if (m_IsNewChannelCreated)
    {
        m_Channel->BindContextDma( handle );
    }
    return 0;
}

RC DmaReader::AllocateBuffer(_PAGE_LAYOUT Layout, UINT64 size)
{
    RC rc = OK;

    if (!m_pSurface || size > m_pSurface->GetAllocSize())
    {
        if (m_pSurface)
        {
            delete m_pSurface;
        }

        m_pSurface = new MdiagSurf();

        switch (Layout)
        {
            case PAGE_PHYSICAL:
            case PAGE_VIRTUAL:
            case PAGE_VIRTUAL_4KB:
            case PAGE_VIRTUAL_64KB:
            case PAGE_VIRTUAL_2MB:
            case PAGE_VIRTUAL_512MB:
                break;
            default:
                MASSERT(!"Invalid Layout");
                return RC::BAD_PARAMETER;
        }

        m_pSurface->SetColorFormat(ColorUtils::Y8);
        m_pSurface->SetArrayPitch(size);
        m_pSurface->SetLayout(Surface2D::Pitch);
        m_pSurface->SetLocation(m_CopyLocation);
        m_pSurface->SetProtect(Memory::ReadWrite);
        m_pSurface->SetVirtAlignment(256);
        m_pSurface->SetAlignment(256);
        m_pSurface->SetGpuVASpace(m_Channel->GetVASpaceHandle());
        m_pSurface->SetGpuCacheMode(Surface2D::GpuCacheOff);

        LWGpuResource* lwgpu = m_Channel->GetLWGpu();

        CHECK_RC(m_pSurface->Alloc(lwgpu->GetGpuDevice(), m_Channel->GetLwRmPtr()));

        m_CopyDmaHandle = m_pSurface->GetCtxDmaHandle();
        m_CopyGpuOffset = m_pSurface->GetCtxDmaOffsetGpu();

        m_pSurface->BindGpuChannel(m_Channel->ChannelHandle());
    }

    return rc;
}

RC DmaReader::ReadBlocklinearToPitch
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
    MASSERT(!"Invalid DmaReader::ReadBlocklinear call");
    return RC::SOFTWARE_ERROR;
}
