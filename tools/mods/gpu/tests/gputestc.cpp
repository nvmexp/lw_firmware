/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Implementation of GpuTestConfiguration class.
#include "gputestc.h"
#include "core/include/jscript.h"
#include "core/include/channel.h"
#include "lwos.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/utility/surf2d.h"
#include "gpu/utility/subcontext.h"
#include "gpu/include/gpudev.h"
#include "core/include/platform.h"
#include "core/include/circsink.h"
#include "core/include/mgrmgr.h"
#include "gpu/include/testdevice.h"
#include "gpu/include/testdevicemgr.h"
#include "gpu/utility/tsg.h"
#include "gpu/include/gralloc.h"

#include "class/cl2080.h"
#include "ctrl/ctrl2080.h"

#include "class/cl906f.h" // GF100_CHANNEL_GPFIFO

#if defined(TEGRA_MODS)
#include "gpu/include/android_channel.h"
#endif
#include "gpu/include/tegra_thi_channel.h"

//------------------------------------------------------------------------------
struct GpuTestConfiguration::ChannelStuff
{
    ChannelStuff * Next;
    UINT32         Number;
    bool           IsShared;

    UINT32 PBMemBytes;
    Surface2D PBMem;
    Surface2D ErrMem;

    LwRm::Handle hChan;
    Channel *      pChan;
    void *         PBMemAddress;  // for the GetPushBufferAddr hack
};

//------------------------------------------------------------------------------
// JS stuff
//
static SProperty TestConfiguration_DmaProtocol
(
    TestConfiguration_Object,
    "DmaProtocol",
    0,
    Memory::Default,
    0,
    0,
    0,
    "Flag allocated context DMAs as default, safe, or fast\n"
);

static SProperty TestConfiguration_SystemMemoryModel
(
    TestConfiguration_Object,
    "SystemMemoryModel",
    0,
    Memory::OptimalModel,
    0,
    0,
    0,
    "SystemMemoryModel: Memory.PciExpressModel, Memory.PciModel, or Memory.OptimalModel"
);

static SProperty TestConfiguration_NotifierLocation
(
    TestConfiguration_Object,
    "NotifierLocation",
    0,
    Memory::Coherent,
    0,
    0,
    0,
    "Notifer memory location: Memory.Coherent, .NonCoherent, or .Fb."
);

static SProperty TestConfiguration_GpFifoEntries
(
    TestConfiguration_Object,
    "GpFifoEntries",
    0,
    512,
    0,
    0,
    0,
    "GPFIFO size in entries."
);

static SProperty TestConfiguration_AutoFlush
(
    TestConfiguration_Object,
    "AutoFlush",
    0,
    true,
    0,
    0,
    0,
    "Channels should flush automatically whenever 256 or more dwords accumulate."
);

static SProperty TestConfiguration_AutoFlushThresh
(
    TestConfiguration_Object,
    "AutoFlushThresh",
    0,
    256,
    0,
    0,
    0,
    "Number of dwords which should accumulate before a channel auto-flushes."
);

static SProperty TestConfiguration_UseBar1Doorbell
(
    TestConfiguration_Object,
    "UseBar1Doorbell",
    0,
    false,
    0,
    0,
    0,
    "Channels should use BAR1 doorbell ring for work submission."
);

static SProperty TestConfiguration_ChannelLogging
(
    TestConfiguration_Object,
    "ChannelLogging",
    0,
    false,
    0,
    0,
    0,
    "Channels should print all method data to logfile when flushing."
);

static SProperty TestConfiguration_AllowVIC
(
    TestConfiguration_Object,
    "AllowVIC",
    0,
    true,
    0,
    0,
    0,
    "Allow the use of VIC on CheetAh"
);

static SProperty TestConfiguration_SemaphorePayloadSize
(
    TestConfiguration_Object,
    "SemaphorePayloadSize",
    0,
    Channel::SEM_PAYLOAD_SIZE_DEFAULT,
    0,
    0,
    0,
    "Semaphore payload size."
);

static SProperty TestConfiguration_PointMaxValue
(
   TestConfiguration_Object,
   "PointMaxValue",
   0,
   GpuTestConfiguration::PointMaxValue,
   0,
   0,
   JSPROP_READONLY,
   "CONSTANT: the max coordinate value."
);
static SProperty TestConfiguration_PointMilwalue
(
   TestConfiguration_Object,
   "PointMilwalue",
   0,
   GpuTestConfiguration::PointMilwalue,
   0,
   0,
   JSPROP_READONLY,
   "CONSTANT: the min coordinate value."
);

static SProperty TestConfiguration_SemPayloadSize32
(
   TestConfiguration_Object,
   "SEM_PAYLOAD_SIZE_32BIT",
   0,
   Channel::SEM_PAYLOAD_SIZE_32BIT,
   0,
   0,
   JSPROP_READONLY,
   "CONSTANT: 32bit semaphore payload size."
);

static SProperty TestConfiguration_SemPayloadSize64
(
   TestConfiguration_Object,
   "SEM_PAYLOAD_SIZE_64BIT",
   0,
   Channel::SEM_PAYLOAD_SIZE_64BIT,
   0,
   0,
   JSPROP_READONLY,
   "CONSTANT: 64bit semaphore payload size."
);

static SProperty TestConfiguration_SemPayloadSizeAuto
(
   TestConfiguration_Object,
   "SEM_PAYLOAD_SIZE_AUTO",
   0,
   Channel::SEM_PAYLOAD_SIZE_AUTO,
   0,
   0,
   JSPROP_READONLY,
   "CONSTANT: Automatic semaphore payload size."
);

static SProperty TestConfiguration_SemPayloadSizeDefault
(
   TestConfiguration_Object,
   "SEM_PAYLOAD_SIZE_DEFAULT",
   0,
   Channel::SEM_PAYLOAD_SIZE_DEFAULT,
   0,
   0,
   JSPROP_READONLY,
   "CONSTANT: Default semaphore payload size."
);

// TODO delete this once the old RNG is removed
static SProperty TestConfiguration_UseOldRNG
(
    TestConfiguration_Object,
    "UseOldRNG",
    0,
    false,
    0,
    0,
    0,
    "Force legacy RNG implementation."
);


static SProperty TestConfiguration_UphyLogMask
(
    TestConfiguration_Object,
    "UphyLogMask",
    0,
    0,
    0,
    0,
    0,
    "Mask for logging uphy data"
);

//------------------------------------------------------------------------------
// Constructor
//------------------------------------------------------------------------------

GpuTestConfiguration::GpuTestConfiguration()
  : TestConfiguration(), m_pLwRm(0), m_pChannelStuff(0)
{
    // Don't call base class version of Reset, TestConfiguration ctor already
    // does that
    GpuTestConfiguration::Reset(false);
}

GpuTestConfiguration::~GpuTestConfiguration()
{
    while (m_pChannelStuff)
        FreeChannel(m_pChannelStuff->pChan);
}

//------------------------------------------------------------------------------
void GpuTestConfiguration::SetDmaProtocol
(
    Memory::DmaProtocol protocol
)
{
    m_DmaProtocol = protocol;
}

//-----------------------------------------------------------------------------
RC GpuTestConfiguration::SetSystemMemModel
(
   Memory::SystemModel model
)
{
   if (!m_pTestDevice.get())
       return RC::OK;

   RC rc;

   GpuSubdevice *const pSubdev = m_pTestDevice->GetInterface<GpuSubdevice>();
   if (!pSubdev)
       return RC::OK;
   CHECK_RC(pSubdev->LoadBusInfo());

   switch(model)
   {
      case Memory::PciExpressModel:
      {
         if (!(FLD_TEST_DRF(2080, _CTRL_BUS_INFO, _COHERENT_DMA_FLAGS_GPUGART,
                           _TRUE, pSubdev->CFlags())
              || FLD_TEST_DRF(2080, _CTRL_BUS_INFO,
                              _NONCOHERENT_DMA_FLAGS_GPUGART,
                              _TRUE, pSubdev->NCFlags())))
         {
            Printf(Tee::PriNormal,
               "WARNING:  Setting Memory Model to PciExpressModel, though the "
               "system does not appear to support it\n");
         }
         m_SystemMemModel = Memory::PciExpressModel;
         break;
      }

      case Memory::PciModel:
         m_SystemMemModel = Memory::PciModel;
         break;

      case Memory::OptimalModel:
         if (pSubdev->NCFlags() == 0)
         {
             // Print at PriLow since this is the common case with PCI cards
             Printf(Tee::PriLow, "WARNING: System does not support NonCoherent memory.\n");
             Printf(Tee::PriLow, "WARNING: Defaulting to PCI memory model\n");
             m_SystemMemModel = Memory::PciModel;
         }
         else
         {
             m_SystemMemModel = Memory::OptimalModel;
         }
         break;
   }
   return OK;
}

void GpuTestConfiguration::SetChannelType
(
    TestConfiguration::ChannelType ct
)
{
    if (!m_pTestDevice.get())
        return;

    LwRm *pLwRm = GetRmClient();
    UINT32 ignored;
    bool UserHasSpecifiedASupportedChannelType = false;

    switch (ct)
    {
        case GpFifoChannel:
            if (OK == pLwRm->GetFirstSupportedClass(GetNumFifoClasses(),
                        GetFifoClasses(), &ignored, GetGpuDevice()))
            {
                UserHasSpecifiedASupportedChannelType = true;
            }
            break;

        case TegraTHIChannel:
#if defined(TEGRA_MODS)
            UserHasSpecifiedASupportedChannelType = true;
#else
            UserHasSpecifiedASupportedChannelType = false;
#endif
            break;

        case UseNewestAvailable:
            UserHasSpecifiedASupportedChannelType = true;
            break;

        default:
            // Garbage value from JS, fix it.
            ct = UseNewestAvailable;
            UserHasSpecifiedASupportedChannelType = true;
            break;
    }

    if (UserHasSpecifiedASupportedChannelType)
    {
        TestConfiguration::SetChannelType(ct);
    }
    else
    {
        Printf(Tee::PriNormal,
                "Warning, user-specified channel type %s is not supported.\n"
                "Using newest supported channel type instead.\n",
                ChannelTypeString(ct));

        TestConfiguration::SetChannelType(UseNewestAvailable);
    }
}

TestConfiguration::ChannelType GpuTestConfiguration::GetChannelType() const
{
    if (!m_pTestDevice.get())
        return GpFifoChannel;

    LwRm *pLwRm = GetRmClient();
    UINT32 ignored;
    ChannelType ct = TestConfiguration::GetChannelType();

    switch (ct)
    {
        case GpFifoChannel:
        case TegraTHIChannel:
            // User has specified a particular channel type.
            // We're obliged to use that one.
            return ct;

        case UseNewestAvailable:
            // User has not overridden our default, which is to use the newest
            // kind of channel the hw supports.

            if (OK == pLwRm->GetFirstSupportedClass(GetNumFifoClasses(),
                        GetFifoClasses(), &ignored, GetGpuDevice()))
            {
                return GpFifoChannel;
            }

            // When running CheetAh MODS with lwgpu driver in a scenario where
            // the GPU is purposefully turned off, rmapi_tegra does not return
            // any usable GPU classes, but we can still use THI channel,
            if (m_pTestDevice->GetInterface<GpuSubdevice>()->IsFakeTegraGpu())
            {
                return TegraTHIChannel;
            }
            break;

        default:
            break;
    }

    Printf(Tee::PriHigh,
            "ERROR: Subdevice doesn't support DMA, GpFifo, or CheetAh "
            "Channels!  Using gpfifo channel, but it won't work.");
    MASSERT(!"ERROR: No Channels Supported on this device!");

    return GpFifoChannel;
}

//------------------------------------------------------------------------------
RC GpuTestConfiguration::AllocateChannelGr
(
    Channel        ** ppChan,
    LwRm::Handle    * phChan,
    Tsg              *pTsg,
    LwRm::Handle      hVASpace,
    GrClassAllocator *pGrAlloc,
    LwU32             flags,
    UINT32            engineId
)
{
    MASSERT(pGrAlloc);
    RC rc;
    UINT32 hostEngineId;

    if (engineId == LW2080_ENGINE_TYPE_ALLENGINES)
    {
        CHECK_RC(pGrAlloc->GetDefaultEngineId(GetGpuDevice(), GetRmClient(), &engineId));
    }

    CHECK_RC(Channel::GetHostEngineId(GetGpuDevice(), GetRmClient(), engineId, &hostEngineId));

    bool bAllocatedTsg = false;
    if (pTsg)
    {
        if (hVASpace)
        {
            Printf(Tee::PriError,
                   "Can't have channel group parent along with a vaspace handle\n");
            return RC::SOFTWARE_ERROR;
        }
        if (pTsg->GetHandle() != 0)
        {
            if (pTsg->GetEngineId() != hostEngineId)
            {
                Printf(Tee::PriError,
                       "TSG allocated on engine %u but %s requires engine %u\n",
                       pTsg->GetEngineId(),
                       pGrAlloc->GetName(),
                       hostEngineId);
                return RC::SOFTWARE_ERROR;
            }
        }
        else
        {
            pTsg->SetEngineId(hostEngineId);
            CHECK_RC(pTsg->Alloc());
            bAllocatedTsg = true;
        }
    }
    DEFERNAME(freeTsg)
    {
        if (bAllocatedTsg)
            pTsg->Free();
    };

    CHECK_RC(AllocateChannel(ppChan,
                             phChan,
                             pTsg,
                             nullptr,
                             hVASpace,
                             flags,
                             hostEngineId));
    DEFERNAME(freeChannel)
    {
        FreeChannel(*ppChan);
    };
    CHECK_RC(pGrAlloc->AllocOnEngine(*phChan, engineId, GetGpuDevice(), GetRmClient()));
    freeTsg.Cancel();
    freeChannel.Cancel();
    return rc;

}
//------------------------------------------------------------------------------
RC GpuTestConfiguration::AllocateChannel
(
    Channel     **ppChan,
    LwRm::Handle *phChan,
    Tsg          *pTsg,
    Subcontext   *pSubctx,
    LwRm::Handle  hVASpace,
    LwU32         flags,
    UINT32        engineId
)
{
   RC      rc;

   MASSERT(ppChan);
   MASSERT(phChan);

   if (!m_pTestDevice.get())
       return RC::OK;

   LwRm *pLwRm = GetRmClient(); // previous bound or client 0

   if (m_pTestDevice->GetInterface<GpuSubdevice>()->HasFeature(Device::GPUSUB_HOST_REQUIRES_ENGINE_ID) &&
       (LW2080_ENGINE_TYPE_NULL == engineId))
   {
       const bool bRequireEngineId = GpuPtr()->GetRequireChannelEngineId();
       Printf(bRequireEngineId ? Tee::PriError : Tee::PriWarn,
              "---------------------------------------------------------------\n"
              "- The GPU under test requires an engine to be specified!      -\n"
              "- Channel allocation or test exelwtion may fail.              -\n"
              "---------------------------------------------------------------\n");
       if (bRequireEngineId)
           return RC::SOFTWARE_ERROR;
   }

   if (pTsg)
   {
        // Subcontext is part of a TSG. So no need to explicity specify a TSG
        if (pSubctx)
        {
            MASSERT(!"TSG and subcontext can't be specified together.");
            return RC::SOFTWARE_ERROR;
        }
        else
        {
            // Context shares must be specified per channel when the channel is part
            // of a channel group on a GPU that supports subcontexts.
            if (GetGpuDevice()->GetSubdevice(0)->HasFeature(Device::GPUSUB_HAS_SUBCONTEXTS))
            {
                // For now print a PriLow message, eventually RM will fail if a context
                // share is not specified and then this should become an error case
                Printf(Tee::PriLow,
                       "Channels on GPUs with subcontexts that are part of a channel"
                       "group should specify a subcontext");
            }
        }

        if (hVASpace)
        {
            MASSERT(!"Can't have channel group parent along with a vaspace handle");
            return RC::SOFTWARE_ERROR;
        }
    }
    else if (pSubctx)
    {
        if (hVASpace)
        {
            MASSERT(!"Can't have subcontext along with a vaspace handle");
            return RC::SOFTWARE_ERROR;
        }

        pTsg = pSubctx->GetTsg();
    }

   if (m_pChannelStuff && !AllowMultipleChannels())
   {
      MASSERT(!"Possible channel leak - multiple channels not allowed.");
      return RC::SOFTWARE_ERROR;
   }

   GpuDevice* const gpudev = GetGpuDevice();

   if (PushBufferLocation() == Memory::Fb &&
       gpudev->GetNumSubdevices() > 1)
   {
      Printf(Tee::PriDebug,
             "SLI doesn't allow FB pushbuffers: using coherent.\n");
      SetPushBufferLocation(Memory::Coherent);
   }

   // Allocate and initialize our ChannelStuff structure.

   ChannelStuff * tmpCS  = m_pChannelStuff;
   m_pChannelStuff = new ChannelStuff;

   m_pChannelStuff->Next   = tmpCS;
   m_pChannelStuff->Number = tmpCS ? (tmpCS->Number+1) : 0;
   m_pChannelStuff->IsShared = false;

   UINT32 tmpHandle = HandleBase() + 0x10 * m_pChannelStuff->Number;

   m_pChannelStuff->PBMemBytes      = ChannelSize();
   m_pChannelStuff->pChan           = NULL;
   m_pChannelStuff->PBMemAddress    = NULL;

   const char * MemLocStrings[] = { "Fb", "Coherent", "Non-Coherent" };

   Printf(Tee::PriDebug, "Alloc Channel: H=0x%x %s %s 0x%x bytes\n",
          tmpHandle,
          GetChannelType() == GpFifoChannel ? "GpFifo" : "TegraTHI",
          MemLocStrings[ PushBufferLocation() ],
          ChannelSize());

   if (GetChannelType() == TegraTHIChannel)
#if defined(TEGRA_MODS)
   {
       unique_ptr<Channel> pChannel(new ::AndroidHost1xChannel(pLwRm, gpudev));
       if (GetChannelType() == TegraTHIChannel)
       {
           unique_ptr<Channel> pInner(move(pChannel));
           // TegraTHIChannel constructor consumes our unique_ptr
           // and takes ownership of the channel object.
           pChannel.reset(new ::TegraTHIChannel(move(pInner)));
       }
       m_pChannelStuff->pChan = pChannel.release();
       m_pChannelStuff->pChan->SetDefaultTimeoutMs(TimeoutMs());
       m_pChannelStuff->pChan->EnableLogging(m_ChannelLogging);
       m_pChannelStuff->hChan = 0;
       *ppChan = m_pChannelStuff->pChan;
       *phChan = 0;
       return OK;
   }
#else
   {
       Printf(Tee::PriError, "Unsupported channel class\n");
       return RC::UNSUPPORTED_FUNCTION;
   }
#endif

    // Allocate the error notifier context dma:
    if (pTsg == NULL)
    {
        CHECK_RC(AllocateErrorNotifier(&m_pChannelStuff->ErrMem,
            pSubctx ? pSubctx->GetVASpaceHandle() : hVASpace));
    }

    // Allocate memory for a DMA channel.
    UINT32 PBSize = m_pChannelStuff->PBMemBytes;
    Memory::Location pbLocation = PushBufferLocation();

    if (GetChannelType() == TestConfiguration::GpFifoChannel)
    {
        // WARNING: This is technically wrong since we might choose
        // a different GP FIFO depending on what card we are using.
        // As of this moment, all of the GP_ENTRY__SIZEs are set to 8
        // so it's not a problem.
        PBSize = m_pChannelStuff->PBMemBytes +
                 m_GpFifoEntries * LW906F_GP_ENTRY__SIZE;
        m_pChannelStuff->PBMem.SetAddressModel(Memory::Paging);
    }
    else
    {
        m_pChannelStuff->PBMem.SetAddressModel(Memory::Segmentation);
    }

    m_pChannelStuff->PBMem.SetWidth(PBSize);
    m_pChannelStuff->PBMem.SetPitch(PBSize);
    m_pChannelStuff->PBMem.SetHeight(1);
    m_pChannelStuff->PBMem.SetColorFormat(ColorUtils::Y8);

    CHECK_RC(ApplySystemModelFlags(pbLocation, &m_pChannelStuff->PBMem));
    m_pChannelStuff->PBMem.SetLocation(pbLocation);
    m_pChannelStuff->PBMem.SetProtect(Memory::Readable);
    m_pChannelStuff->PBMem.SetGpuVASpace(pSubctx ? pSubctx->GetVASpaceHandle() : hVASpace);
    if (GetChannelType() == TestConfiguration::GpFifoChannel)
    {
        m_pChannelStuff->PBMem.SetVASpace(Surface2D::GPUVASpace);
    }
    CHECK_RC(m_pChannelStuff->PBMem.Alloc(GetGpuDevice(), pLwRm));
    CHECK_RC(m_pChannelStuff->PBMem.Map());
    m_pChannelStuff->PBMemAddress = m_pChannelStuff->PBMem.GetAddress();

    if (GetChannelType() == TestConfiguration::GpFifoChannel)
    {
        UINT32 FifoClass;
        CHECK_RC(pLwRm->GetFirstSupportedClass(GetNumFifoClasses(),
            GetFifoClasses(), &FifoClass, GetGpuDevice()));

        CHECK_RC(pLwRm->AllocChannelGpFifo(
            &m_pChannelStuff->hChan,
            FifoClass,
            m_pChannelStuff->ErrMem.GetMemHandle(pLwRm),
            m_pChannelStuff->ErrMem.GetAddress(),
            m_pChannelStuff->PBMem.GetMemHandle(pLwRm), 
            m_pChannelStuff->PBMem.GetAddress(),
            m_pChannelStuff->PBMem.GetCtxDmaOffsetGpu(),
            m_pChannelStuff->PBMemBytes,
            ((UINT08*)m_pChannelStuff->PBMem.GetAddress()) +
            m_pChannelStuff->PBMemBytes,
            m_pChannelStuff->PBMem.GetCtxDmaOffsetGpu() + m_pChannelStuff->PBMemBytes,
            m_GpFifoEntries,
            pSubctx ? pSubctx->GetHandle() : 0,  // context object
            0,  // verifFlags
            0,  // verifFlags2
            flags,  // flags
            GetGpuDevice(),
            pTsg,
            pSubctx ? 0 : hVASpace,
            engineId,
            m_UseBar1Doorbell));
    }
    else
    {
        return RC::TEST_CONFIGURATION_ILWALID_CHANNEL_TYPE;
    }

    // Bind standard context DMAs to the channel
    if (!hVASpace)
    {
        UINT32 hCtxDma = gpudev->GetGartCtxDma(pLwRm);
        CHECK_RC(pLwRm->BindContextDma(m_pChannelStuff->hChan, hCtxDma));
        if (!gpudev->GetSubdevice(0)->
                HasFeature(Device::GPUSUB_HAS_UNIFIED_GART_FLAGS))
        {
            hCtxDma = gpudev->GetFbCtxDma(pLwRm);
            CHECK_RC(pLwRm->BindContextDma(m_pChannelStuff->hChan, hCtxDma));
        }
    }

    // Get a pointer to the Channel object that was created by LwRm.
    m_pChannelStuff->pChan = pLwRm->GetChannel(m_pChannelStuff->hChan);
    *ppChan = m_pChannelStuff->pChan;
    if (phChan)
        *phChan = m_pChannelStuff->hChan;

    CHECK_RC(m_pChannelStuff->pChan->SetAutoFlush(m_AutoFlush, m_AutoFlushThresh));
    m_pChannelStuff->pChan->EnableLogging(m_ChannelLogging);
    m_pChannelStuff->pChan->SetDefaultTimeoutMs(TimeoutMs());
    CHECK_RC(m_pChannelStuff->pChan->SetSemaphorePayloadSize(m_SemaphorePayloadSize));

    return OK;
}

//------------------------------------------------------------------------------
RC GpuTestConfiguration::AllocateChannel
(
    Channel **ppChan,
    LwRm::Handle * phChan
)
{
    return AllocateChannel(ppChan, phChan, NULL, NULL, 0, 0, LW2080_ENGINE_TYPE_NULL);
}

//------------------------------------------------------------------------------
RC GpuTestConfiguration::AllocateChannel
(
    Channel **ppChan,
    LwRm::Handle * phChan,
    UINT32 engineId
)
{
    return AllocateChannel(ppChan, phChan, NULL, NULL, 0, 0, engineId);
}

//------------------------------------------------------------------------------
RC GpuTestConfiguration::AllocateChannelWithEngine
(
    Channel         ** ppChan,
    LwRm::Handle     * phChan,
    GrClassAllocator * pGrAlloc,
    UINT32             engineId
)
{
    return AllocateChannelGr(ppChan,
                             phChan,
                             nullptr,
                             0,
                             pGrAlloc,
                             0,
                             engineId);
}

//------------------------------------------------------------------------------
RC GpuTestConfiguration::AllocateErrorNotifier(Surface2D *pErrorNotifier, UINT32 hVASpace)
{
    RC rc;
    Memory::Location loc = Platform::IsCCMode() ? Memory::Fb : Memory::Coherent;
    CHECK_RC(ApplySystemModelFlags(loc, pErrorNotifier));
    pErrorNotifier->SetWidth(4096);
    pErrorNotifier->SetPitch(4096);
    pErrorNotifier->SetHeight(1);
    pErrorNotifier->SetPageSize(4);
    pErrorNotifier->SetColorFormat(ColorUtils::Y8);
    pErrorNotifier->SetLocation(loc);
    pErrorNotifier->SetProtect(Memory::ReadWrite);
    pErrorNotifier->SetAddressModel(hVASpace ? Memory::Paging : Memory::Segmentation);
    pErrorNotifier->SetKernelMapping(true);
    if (hVASpace)
    {
        pErrorNotifier->SetGpuVASpace(hVASpace);
    }
    if (GetChannelType() == GpFifoChannel)
    {
        pErrorNotifier->SetVASpace(Surface2D::GPUVASpace);
    }
    CHECK_RC(pErrorNotifier->Alloc(GetGpuDevice(), GetRmClient()));
    CHECK_RC(pErrorNotifier->Map());

    return rc;
}

void GpuTestConfiguration::BindGpuDevice(GpuDevice* gpudev)
{
    TestDeviceMgr* pTestDeviceMgr = static_cast<TestDeviceMgr*>(DevMgrMgr::d_TestDeviceMgr);
    TestDevicePtr pTestDevice;
    pTestDeviceMgr->GetDevice(gpudev->GetSubdevice(0)->DevInst(), &pTestDevice);
    MASSERT(pTestDevice.get() != nullptr);
    m_pTestDevice = pTestDevice;
}

//------------------------------------------------------------------------------
RC GpuTestConfiguration::FreeChannel (Channel * pChan)
{
    // Find the appropriate ChannelStuff pointer (probably the only one).
    ChannelStuff ** ppCS = &m_pChannelStuff;
    while (*ppCS && ((*ppCS)->pChan != pChan))
    {
        ppCS = &(*ppCS)->Next;
    }
    if (! *ppCS)
    {
        return OK;
    }

    // We have a ChannelStuff struct.
    // Unlink the ChannelStuff struct from the singly-linked list.

    ChannelStuff * pCS = *ppCS;
    *ppCS = (*ppCS)->Next;

    if (pChan && !pCS->hChan)
    {
        delete pChan;
    }
    else if (pChan)
    {
        // We have an allocated channel.
        LwRm * pLwRm = GetRmClient();

        IdleChannel(pChan);

        if (! pCS->IsShared)
        {
            Printf(Tee::PriDebug, "Free Channel: H=0x%x\n", pCS->hChan);

            // Free the channel and its objects.
            pLwRm->Free(pCS->hChan);
        }
    }
    // Free the ChannelStuff struct.
    delete pCS;

    return OK;
}

//-----------------------------------------------------------------------------
RC GpuTestConfiguration::IdleChannel(Channel* pChan)
{
    if (pChan != 0)
    {
        // Note: Polling inside WaitIdle will return USER_ABORTED_SCRIPT only
        // if there was no other error. Therefore we consider USER_ABORTED_SCRIPT
        // as OK.
        const RC rc = pChan->WaitIdle(TimeoutMs());
        if ((OK != rc) && (RC::USER_ABORTED_SCRIPT != rc))
        {
            Printf(Tee::PriNormal, "Failed idling a channel, will retry in 10s...\n");
            Tasker::Sleep(10000);
            const FLOAT64 NewTimeoutMs = 10 * TimeoutMs();
            Printf(Tee::PriNormal, "Waiting %gs for the channel to go idle...\n",
                    NewTimeoutMs/1000.0);
            const RC secondRc = pChan->WaitIdle(NewTimeoutMs);
            if ((OK != secondRc) && (RC::USER_ABORTED_SCRIPT != secondRc))
            {
                Printf(Tee::PriHigh, "Unable to idle the channel!\n");

                // Flush the log file and hope for the best since we will probably hang
                // the system next time we access the GPU.
                Tee::FlushToDisk();
                Tee::GetCirlwlarSink()->Dump(Tee::FileOnly);
            }
        }
    }
    return OK;
}

//-----------------------------------------------------------------------------
RC GpuTestConfiguration::IdleAllChannels()
{
    ChannelStuff* pChanStuff = m_pChannelStuff;
    for ( ; pChanStuff != 0; pChanStuff = pChanStuff->Next)
    {
        IdleChannel(pChanStuff->pChan);
    }
    return OK;
}

//-----------------------------------------------------------------------------
// Free function to avoid duplication in ApplySystemModelFlags
namespace
{
    RC CoherenceError()
    {
        Printf(Tee::PriError, "Desired coherency not supported by this system\n");
        return RC::BAD_MEMLOC;
    }
}

//-----------------------------------------------------------------------------
RC GpuTestConfiguration::ApplySystemModelFlags
(
    Memory::Location loc,
    Surface2D* pSurf
) const
{
    MASSERT(pSurf);

    if (!m_pTestDevice.get())
        return RC::OK;

    // Update location to reflect real surface location
    GpuSubdevice* const pSubdev = GetGpuDevice()->GetSubdevice(0);
    loc = Surface2D::GetActualLocation(loc, pSubdev);

    // Return if the surface is in vidmem
    if (Memory::Fb == loc)
    {
        return OK;
    }
    MASSERT((Memory::Coherent == loc) || (Memory::NonCoherent == loc));

    RC rc;
    constexpr auto priority = Tee::PriDebug;
    // Init to invalid values
    UINT32 memFlags = 0xFFFFFFFF;
    UINT32 dmaFlags = 0xFFFFFFFF;

    const UINT32 SNOOP_DISABLE_FLAGS =
        DRF_DEF(OS03, _FLAGS, _CACHE_SNOOP, _DISABLE);

    const UINT32 SNOOP_ENABLE_FLAGS =
        DRF_DEF(OS03, _FLAGS, _CACHE_SNOOP, _ENABLE);

    const UINT32 PCI_WC =
        DRF_DEF(OS02, _FLAGS, _LOCATION,    _PCI)
    | DRF_DEF(OS02, _FLAGS, _COHERENCY,   _WRITE_COMBINE);

    const UINT32 PCI_CACHED =
        DRF_DEF(OS02, _FLAGS, _LOCATION,    _PCI)
    | DRF_DEF(OS02, _FLAGS, _COHERENCY,   _CACHED);

    CHECK_RC(pSubdev->LoadBusInfo());

    Printf(priority, "DMA CAPS: NonCoherentFlags: 0x%08x\n",
            (UINT32)pSubdev->NCFlags());
    Printf(priority, "DMA CAPS: CoherentFlags: 0x%08x\n",
            (UINT32)pSubdev->CFlags());

    // Some duplicate code, but it is easier to understand we break this
    // down by System Memory Model.
    switch (m_SystemMemModel)
    {
        case Memory::OptimalModel:
        {
            switch (loc)
            {
            // Try GART first, then context dma
            case Memory::NonCoherent:
            {
                if (FLD_TEST_DRF(2080, _CTRL_BUS_INFO, _NONCOHERENT_DMA_FLAGS_GPUGART,
                                _TRUE, pSubdev->NCFlags()))
                {
                    Printf(priority
                        , "DmaFlags: 'OptimalModel' Using NonCoherent\n");
                    memFlags = PCI_WC;
                    dmaFlags = SNOOP_DISABLE_FLAGS;
                    break;
                }
                Printf(priority
                    , "DmaFlags: 'OptimalModel' System does not support NonCoherent Memory\n");
                return CoherenceError();
            }

            case Memory::Coherent:
            {
                if (FLD_TEST_DRF(2080, _CTRL_BUS_INFO, _COHERENT_DMA_FLAGS_GPUGART,
                                _TRUE, pSubdev->CFlags()))
                {
                    Printf(priority
                        , "DmaFlags: 'OptimalModel' Using Coherent\n");
                    memFlags = PCI_CACHED;
                    dmaFlags = SNOOP_ENABLE_FLAGS;
                    break;
                }
                Printf(priority
                    , "DmaFlags: 'OptimalModel' System does not support Coherent Memory\n");
                return CoherenceError();
            }

            default:
                return CoherenceError();
            } // switch (loc)
        } // Case Memory::OptimalMode
        break;

        case Memory::PciExpressModel:
        {
            switch (loc)
            {
            // In the PCI-Express Model, the GART doesn't exist
            case Memory::NonCoherent:
            {
                if (FLD_TEST_DRF(2080, _CTRL_BUS_INFO, _NONCOHERENT_DMA_FLAGS_GPUGART,
                                _TRUE, pSubdev->NCFlags()))
                {
                    Printf(priority
                        , "DmaFlags: 'PciExpressModel' Using NonCoherent\n");
                    memFlags = PCI_WC;
                    dmaFlags = SNOOP_DISABLE_FLAGS;
                    break;
                }
                Printf(priority
                    , "DmaFlags: 'PciExpressModel' System does not support NonCoherent Memory\n");
                return CoherenceError();
            }

            case Memory::Coherent:
            {
                if (FLD_TEST_DRF(2080, _CTRL_BUS_INFO, _COHERENT_DMA_FLAGS_GPUGART,
                                _TRUE, pSubdev->CFlags()))
                {
                    Printf(priority
                        , "DmaFlags: 'PciExpressModel' Using Coherent\n");
                    memFlags = PCI_CACHED;
                    dmaFlags = SNOOP_ENABLE_FLAGS;
                    break;
                }
                Printf(priority
                    , "DmaFlags: 'PciExpressModel' System does not support Coherent Memory\n");
                return CoherenceError();
            }

            default:
                return CoherenceError();
            } // switch (loc)
        } // Case Memory::PciExpressModel
        break;

        case Memory::PciModel:
        {
            switch (loc)
            {
            case Memory::NonCoherent:
            {
                // Fall back to WC (or UC if -no_wc specified) memory.
                Printf(priority
                    , "DmaFlags: NonCoherent requested on 'PciModel':\n");
                Printf(priority
                    , "          FALLING BACK to PCI_WC (or UC) with snoop enabled\n");
                memFlags = PCI_WC;
                dmaFlags = SNOOP_ENABLE_FLAGS;
                break;
            }

            case Memory::Coherent:
            {
                if (FLD_TEST_DRF(2080, _CTRL_BUS_INFO, _COHERENT_DMA_FLAGS_GPUGART,
                                _TRUE, pSubdev->CFlags()))
                {
                    Printf(priority
                        , "DmaFlags: 'PciModel' Using Coherent\n");
                    memFlags = PCI_CACHED;
                    dmaFlags = SNOOP_ENABLE_FLAGS;
                    break;
                }
                Printf(priority
                    , "DmaFlags: 'PciModel' System does not support Coherent Memory\n");
                return CoherenceError();
            }

            default:
                return CoherenceError();
            } // switch (loc)
        } // Case Memory::PciModel
        break;
    }

    pSurf->SetExtSysMemFlags(memFlags, dmaFlags);

    return OK;
}

//------------------------------------------------------------------------------
// ACCESSORS
//

Memory::DmaProtocol GpuTestConfiguration::DmaProtocol() const
{
   return m_DmaProtocol;
}

UINT32 GpuTestConfiguration::GpFifoEntries() const
{
   return m_GpFifoEntries;
}

Memory::SystemModel GpuTestConfiguration::MemoryModel() const
{
   return m_SystemMemModel;
}

UINT32 GpuTestConfiguration::TiledHeapAttr() const
{
    return UseTiledSurface() ? DRF_DEF(OS32, _ATTR, _TILED, _ANY) : 0;
}

void *GpuTestConfiguration::GetPushBufferAddr(LwRm::Handle channel) const
{
   ChannelStuff * tmpCS  = m_pChannelStuff;

   while(tmpCS != NULL && tmpCS->hChan != channel)
      tmpCS = tmpCS->Next;

   if (tmpCS != NULL)
      return tmpCS->PBMemAddress;

   return NULL;
}

Memory::Location GpuTestConfiguration::NotifierLocation() const
{
   return m_NotifierLocation;
}

TestConfiguration::TestConfigurationType GpuTestConfiguration::GetTestConfigurationType()
{
    return TestConfiguration::TC_GPU;
}

GpuDevice *GpuTestConfiguration::GetGpuDevice() const
{
    if (!m_pTestDevice.get())
        return nullptr;

    auto pSubdev = m_pTestDevice->GetInterface<GpuSubdevice>();
    if (pSubdev)
        return pSubdev->GetParentDevice();
    return nullptr;
}
LwRm *GpuTestConfiguration::GetRmClient() const
{
    if (!m_pLwRm) return LwRmPtr(0).Get();
        return m_pLwRm;
}

bool GpuTestConfiguration::ChannelLogging() const
{
    return m_ChannelLogging;
}

bool GpuTestConfiguration::AllowVIC() const
{
    return m_AllowVIC;
}

UINT32 GpuTestConfiguration::ChannelSize() const
{
    return TestConfiguration::ChannelSize();
}

Channel::SemaphorePayloadSize GpuTestConfiguration::SemaphorePayloadSize() const
{
    return m_SemaphorePayloadSize;
}

//------------------------------------------------------------------------------
// Protected helpers
void GpuTestConfiguration::Reset(bool callBaseClass)
{
    if (callBaseClass)
        TestConfiguration::Reset();

    m_DmaProtocol            = Memory::Default;
    m_SystemMemModel         = Memory::OptimalModel;
    m_GpFifoEntries          = 512;
    m_NotifierLocation       = Memory::Coherent;
    m_AutoFlush              = true;
    m_AutoFlushThresh        = 256;
    m_UseBar1Doorbell        = false;
    m_ChannelLogging         = false;
    m_AllowVIC               = true;
    m_SemaphorePayloadSize   = Channel::SEM_PAYLOAD_SIZE_DEFAULT;
    m_DisplayMgrRequirements = DisplayMgr::PreferRealDisplay;
    m_UseOldRNG              = false;
    m_UphyLogMask            = 0;
}

void GpuTestConfiguration::Reset()
{
    Reset(true);
}

RC GpuTestConfiguration::GetProperties()
{
    RC rc;

    CHECK_RC(TestConfiguration::GetProperties());

    JavaScriptPtr pJs;
    UINT32        tmpI;

    CHECK_RC(pJs->GetProperty(TestConfiguration_Object, TestConfiguration_DmaProtocol, &tmpI));
    if (tmpI != Memory::Safe && tmpI != Memory::Fast)
        m_DmaProtocol = Memory::Default;
    else m_DmaProtocol = (Memory::DmaProtocol) tmpI;

    CHECK_RC(pJs->GetProperty(TestConfiguration_Object, TestConfiguration_SystemMemoryModel, &tmpI));
    SetSystemMemModel((Memory::SystemModel)tmpI);

    CHECK_RC(pJs->GetProperty(TestConfiguration_Object, TestConfiguration_GpFifoEntries, &m_GpFifoEntries));

    CHECK_RC(pJs->GetProperty(TestConfiguration_Object, TestConfiguration_AutoFlush, &m_AutoFlush));
    CHECK_RC(pJs->GetProperty(TestConfiguration_Object, TestConfiguration_AutoFlushThresh, &m_AutoFlushThresh));
    CHECK_RC(pJs->GetProperty(TestConfiguration_Object, TestConfiguration_UseBar1Doorbell, &m_UseBar1Doorbell));
    CHECK_RC(pJs->GetProperty(TestConfiguration_Object, TestConfiguration_ChannelLogging, &m_ChannelLogging));
    CHECK_RC(pJs->GetProperty(TestConfiguration_Object, TestConfiguration_AllowVIC, &m_AllowVIC));

    Memory::GetLocationProperty(TestConfiguration_Object,
            TestConfiguration_NotifierLocation,
            &m_NotifierLocation);

    CHECK_RC(pJs->GetProperty(TestConfiguration_Object, TestConfiguration_SemaphorePayloadSize, &tmpI));
    m_SemaphorePayloadSize = static_cast<Channel::SemaphorePayloadSize>(tmpI);

    CHECK_RC(pJs->GetProperty(TestConfiguration_Object, TestConfiguration_UseOldRNG, &m_UseOldRNG));
    CHECK_RC(pJs->GetProperty(TestConfiguration_Object, TestConfiguration_UphyLogMask, &m_UphyLogMask));

    // Make sure we don't have channels left over from the last test run.
    MASSERT(0 == m_pChannelStuff);

    return rc;
}

RC GpuTestConfiguration::ValidateProperties()
{
    if
    (
        (m_DmaProtocol != Memory::Default)
     && (m_DmaProtocol != Memory::Safe)
     && (m_DmaProtocol != Memory::Fast)
    )
    {
        Printf(Tee::PriHigh, "Dma Protocol must be default, safe, or fast (%08x).  Forcing to default\n", m_DmaProtocol);
        m_DmaProtocol = Memory::Default;
    }

    switch (m_NotifierLocation)
    {
        case Memory::Fb:
        case Memory::Coherent:
        case Memory::NonCoherent:
           break;

        default:
           Printf(Tee::PriHigh,
                "Bad NotifierLocation (%d), forcing Memory.Coherent.\n",
                m_NotifierLocation);
           m_NotifierLocation = Memory::Coherent;
           break;
    }

    switch (m_DisplayMgrRequirements)
    {
        case DisplayMgr::RequireNullDisplay:
        case DisplayMgr::PreferRealDisplay:
        case DisplayMgr::RequireRealDisplay:
            break;

        default:
           Printf(Tee::PriHigh,
                "Bad DisplayMgrRequirements (%d), forcing Display.PreferRealDisplay.\n",
                m_DisplayMgrRequirements);
           m_DisplayMgrRequirements = DisplayMgr::PreferRealDisplay;
           break;
    }

    switch (m_SemaphorePayloadSize)
    {
        case Channel::SEM_PAYLOAD_SIZE_32BIT:
        case Channel::SEM_PAYLOAD_SIZE_64BIT:
        case Channel::SEM_PAYLOAD_SIZE_AUTO:
        case Channel::SEM_PAYLOAD_SIZE_DEFAULT:
            break;

        default:
            Printf(Tee::PriHigh,
                   "Bad SemaphorePayloadSize (%d), forcing Channel.SEM_PAYLOAD_SIZE_DEFAULT.\n",
                   m_SemaphorePayloadSize);
            m_SemaphorePayloadSize = Channel::SEM_PAYLOAD_SIZE_DEFAULT;
            break;
    }

    return TestConfiguration::ValidateProperties();
}

void GpuTestConfiguration::PrintJsProperties(Tee::Priority pri)
{
    TestConfiguration::PrintJsProperties(pri);

    // TODOWJM: Add SystemModel

    const char *protocol = "Default";

    static const char * reqStrings [] =
    {
        "RequireNullDisplay",
        "PreferRealDisplay",
        "RequireRealDisplay"
    };

    static const char * ftStrings[] = { "false", "true" };
    Printf(pri, "\t%-32s %s\n",         "Dma Protocol:",       protocol);
    Printf(pri, "\t%-32s %s\n",         "NotifierLocation:",   Memory::GetMemoryLocationName(m_NotifierLocation)); //$
    Printf(pri, "\t%-32s %#010x(%i)\n", "GpFifoEntries:",      m_GpFifoEntries, m_GpFifoEntries);
    Printf(pri, "\t%-32s %s\n",         "AutoFlush:",          ftStrings[m_AutoFlush]);
    Printf(pri, "\t%-32s %d\n",         "AutoFlushThresh:",    m_AutoFlushThresh);
    Printf(pri, "\t%-32s %s\n",         "UseBar1Doorbell:",    ftStrings[m_UseBar1Doorbell]);
    Printf(pri, "\t%-32s %s\n",         "ChannelLogging:",     ftStrings[m_ChannelLogging]);
    Printf(pri, "\t%-32s %s\n",         "AllowVIC:",           ftStrings[m_AllowVIC]);

    static const char * semPayloadStrings [] = { "32b", "64b", "Auto", "Default" };
    Printf(pri, "\t%-32s %s\n",         "SemaphorePayloadSize:",    semPayloadStrings[m_SemaphorePayloadSize]); //$
    Printf(pri, "\t%-32s %s\n",         "DisplayMgrRequirements:",  reqStrings[DisplayMgrRequirements()]); //$
    Printf(pri, "\t%-32s %s\n",         "UseOldRNG:",               ftStrings[m_UseOldRNG]);
    Printf(pri, "\t%-32s 0x%x\n",       "UphyLogMask:",             m_UphyLogMask);
}

RC GpuTestConfiguration::GetPerTestOverrides(JSObject *valuesObj)
{
    RC rc;

    CHECK_RC(TestConfiguration::GetPerTestOverrides(valuesObj));

    JavaScriptPtr pJs;
    UINT32 tmpI;
    bool   tmpB;

    if (OK == pJs->GetProperty(valuesObj, UseProperty("SystemMemoryModel"), &tmpI))
        SetSystemMemModel((Memory::SystemModel)tmpI);

    if (OK == pJs->GetProperty(valuesObj, UseProperty("GpFifoEntries"), &tmpI))
        m_GpFifoEntries = tmpI;

    if (OK == pJs->GetProperty(valuesObj, UseProperty("AutoFlush"), &tmpB))
        m_AutoFlush = tmpB;

    if (OK == pJs->GetProperty(valuesObj, UseProperty("AutoFlushThresh"), &tmpI))
        m_AutoFlushThresh = tmpI;

    if (OK == pJs->GetProperty(valuesObj, UseProperty("UseBar1Doorbell"), &tmpB))
        m_UseBar1Doorbell = tmpB;

    if (OK == pJs->GetProperty(valuesObj, UseProperty("ChannelLogging"), &tmpB))
        m_ChannelLogging = tmpB;

    if (OK == pJs->GetProperty(valuesObj, UseProperty("AllowVIC"), &tmpB))
        m_AllowVIC = tmpB;

    if (OK == pJs->GetProperty(valuesObj, UseProperty("DmaProtocol"), &tmpI))
        m_DmaProtocol = (Memory::DmaProtocol) tmpI; // validated above

    if (OK == pJs->GetProperty(valuesObj, UseProperty("NotifierLocation"), &tmpI))
        m_NotifierLocation = (Memory::Location) tmpI; // validated above

    if (OK == pJs->GetProperty(valuesObj, UseProperty("SemaphorePayloadSize"), &tmpI))
        m_SemaphorePayloadSize = static_cast<Channel::SemaphorePayloadSize>(tmpI); // validated above

    if (RC::OK == pJs->GetProperty(valuesObj, UseProperty("UseOldRNG"), &tmpB))
        m_UseOldRNG = tmpB;

    if (RC::OK == pJs->GetProperty(valuesObj, UseProperty("UphyLogMask"), &tmpI))
        m_UphyLogMask = tmpI;

    if (OK == pJs->GetProperty(valuesObj, UseProperty("DisplayMgrRequirements"), &tmpI))
        m_DisplayMgrRequirements = (DisplayMgr::Requirements) tmpI; // validated above

    return rc;
}

const UINT32 *GpuTestConfiguration::GetFifoClasses() const
{
    return &Channel::FifoClasses[0];
}

DisplayMgr::Requirements GpuTestConfiguration::DisplayMgrRequirements() const
{
    return m_DisplayMgrRequirements;
}
