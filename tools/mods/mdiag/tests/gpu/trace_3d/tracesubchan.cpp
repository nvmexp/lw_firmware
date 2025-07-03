/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2005-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   tracesubchan.cpp
 * @brief  TraceSubChannel: per-subchannel resources used by Trace3DTest.
 */
#include "mdiag/tests/gpu/lwgpu_ch_helper.h"
#include "tracechan.h"
#include "mdiag/tests/gpu/lwgpu_subchannel.h"
#include "class/cl0005.h" // LW0005_NOTIFY_INDEX_SUBDEVICE
#include "class/cl5080.h" // LW50_DEFERRED_API_CLASS
#include "class/cl86b6.h" // LW86B6_VIDEO_COMPOSITOR
#include "class/cl902d.h" // FERMI_TWOD_A
#include "class/cl90b3.h" // GF100_MSPPP
#include "class/cl90b8.h" // GF106_DMA_DECOMPRESS
#include "class/cl95a1.h" // LW95A1_TSEC
#include "class/cl95b1.h" // LW95B1_VIDEO_MSVLD
#include "class/cl95b2.h" // LW95B2_VIDEO_MSPDEC
#include "class/cla140.h" // KEPLER_INLINE_TO_MEMORY_B
#include "class/cl9097.h" // FERMI_A
#include "class/cl9297.h" // FERMI_C
#include "class/cla097.h" // KEPLER_A
#include "class/clb097.h" // MAXWELL_A
#include "class/clcd97.h" // BLACKWELL_A
#include "mdiag/utils/lwgpu_classes.h"
#include "core/include/massert.h"
#include "core/include/gpu.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/channel.h"
#include "gpu/include/notifier.h"
#include "core/include/lwrm.h"
#include "lwos.h"
#include "mdiag/utils/buffinfo.h"
#include "mdiag/utils/utils.h"
#include "teegroups.h"
#include "core/include/chiplibtracecapture.h"
#include "gpu/utility/semawrap.h"

UINT32 TraceSubChannel::s_ClassNum = LWGpuClasses::GPU_CLASS_UNDEFINED;

#define MSGID() T3D_MSGID(Notifier)

TraceSubChannel::TraceSubChannel( const string& name, BuffInfo* bif, const ArgReader *params )
    :m_pSubCh(0), m_HwClass(0),
     m_SubChNum(-1),
     m_TraceSubChNum(UNSPECIFIED_TRACE_SUBCHNUM),
     m_UseTraceSubchNum(false),
     m_Massager(0),
     m_Name(name),
     m_TrustedContext(false),
     m_WfiMethod(WFI_INTR),
     m_UsingSemaphoreWfi(false),
     m_NotifierLoc(_DMA_TARGET_COHERENT),
     m_NotifierLayout(PAGE_VIRTUAL),
     m_BuffInfo(bif),
     m_CeType(CEObjCreateParam::UNSPECIFIED_CE),
     m_LwEncOffset(0),
     m_LwDecOffset(0),
     m_LwJpgOffset(0),
     m_pLwGpuResource(NULL),
     m_Params(params),
     m_IsDynamic(false),
     m_IsValidInParsing(true),
     m_WasUnspecifiedCE(false)
{
}

TraceSubChannel::~TraceSubChannel()
{
    if (!m_ModsEvents.empty())
    {
        for (auto& modsEvent : m_ModsEvents)
        {
            ModsEvent *pModsEvent;
            LwRm::Handle eventHandle;
            std::tie(pModsEvent, eventHandle) = modsEvent;

            if (pModsEvent)
            {
                Tasker::FreeEvent(pModsEvent);

                // Also need to free RM resource, otherwise RM will report the event to
                // MODS which is incorrect.
                if (eventHandle)
                {
                    GetLwRmPtr()->Free(eventHandle);
                }
            }
        }
        m_ModsEvents.clear();
    }

    if (m_pSubCh)
    {
        delete m_pSubCh;
        m_pSubCh = 0;
    }

    if (m_Notifiers.size())
    {
        for (UINT32 subdev = 0; subdev < m_SubdevCount; ++subdev)
        {
            m_Notifiers[subdev]->Free();
        }
        m_Notifiers.clear();
    }
}

//------------------------------------------------------------------------------
void TraceSubChannel::SetClass(UINT32 hwclass)
{
    m_HwClass = hwclass;

    // s_ClassLevel stores class family (TESLA, FERMI etc) for current channel/test
    if( s_ClassNum == LWGpuClasses::GPU_CLASS_UNDEFINED )
    {
        s_ClassNum = hwclass;
    }
    else if (LW50_DEFERRED_API_CLASS == hwclass)
    {
        // This class is only used by driver to send SW methods, it'll also be used
        // in fermi or later class, so we skip family checking
    }

    if (ComputeSubChannel())
    {
        m_Massager = &GenericTraceModule::MassageFermiComputeMethod;
    }
    else if (m_HwClass == FERMI_TWOD_A)
    {
        m_Massager = &GenericTraceModule::MassageTwodMethod;
    }
    else if (VideoSubChannel() ||
                m_HwClass == LW86B6_VIDEO_COMPOSITOR)
    {
        m_Massager = &GenericTraceModule::MassageMsdecMethod;
    }
    else if (GrSubChannel())
    {
        if (m_HwClass == FERMI_C)
        {
            m_Massager = &GenericTraceModule::MassageFermi_cMethod;
        }
        else if (m_HwClass <= MAXWELL_A)
        {
            m_Massager = &GenericTraceModule::MassageFermiMethod;
        }
        else if (m_HwClass < BLACKWELL_A)
        {
            m_Massager = &GenericTraceModule::MassageMaxwellBMethod;
        }
        else
        {
            m_Massager = &GenericTraceModule::MassageBlackwellMethod;
        }
    }
    else if (CopySubChannel())
    {
        m_Massager = &GenericTraceModule::MassageCeMethod;
    }
    else
    {
        InfoPrintf("No specific massager for class 0x%x, skip massaging.\n",
                    m_HwClass);
    }
}

//------------------------------------------------------------------------------
void TraceSubChannel::SetTrustedContext()
{
    m_TrustedContext = true;
}

//------------------------------------------------------------------------------
RC TraceSubChannel::AllocSubChannel(LWGpuResource * pLwGpu)
{
    m_pLwGpuResource = pLwGpu;
    m_SubdevCount = pLwGpu->GetGpuDevice()->GetNumSubdevices();
   
    for (UINT32 subdev = 0; subdev < m_SubdevCount; subdev++)
    {
        m_Notifiers.push_back(make_unique<MdiagSurf>());

        if (!m_Notifiers.back())
            return RC::CANNOT_ALLOCATE_MEMORY;
    }

    m_ModsEvents.resize(m_SubdevCount, make_pair(nullptr, 0));
    return OK;
}

//------------------------------------------------------------------------------
RC TraceSubChannel::AllocObject(GpuChannelHelper* pChHelper)
{
    RC rc = OK;

    InfoPrintf("TraceSubChannel::AllocObject \"%s\", class %04x.\n",
            m_Name.c_str(), m_HwClass);

    const string scopeName = Utility::StrPrintf("%s Alloc subchannel number %d class 0x%x on channel 0x%x",
            pChHelper->testname(), m_TraceSubChNum, m_HwClass, pChHelper->channel()->GetModsChannel()->GetChannelId());

    ChiplibOpScope newScope(scopeName, NON_IRQ,
                            ChiplibOpScope::SCOPE_COMMON, NULL);

    if (m_UseTraceSubchNum)
    {
        pChHelper->channel()->SetTraceSubchNum(m_HwClass, m_TraceSubChNum);
    }

    void* objCreateParam = 0;
    unique_ptr<CEObjCreateParam> pCeParam;
    LW_MSENC_ALLOCATION_PARAMETERS allocParam = {};
    LW_BSP_ALLOCATION_PARAMETERS lwdecAllocParam = {};
    LW_LWJPG_ALLOCATION_PARAMETERS lwjpgAllocParam = {};

    if (CopySubChannel())
    {
        pCeParam.reset(new CEObjCreateParam(m_CeType, m_HwClass));
        CHECK_RC(pCeParam->GetObjCreateParam(pChHelper->channel(),
            &objCreateParam, &m_CeType));
    }
    else if (EngineClasses::IsClassType("Lwenc", m_HwClass))
    {
        objCreateParam = LWGpuChannel::GetLwEncObjCreateParam(m_LwEncOffset, &allocParam);
    }
    else if (EngineClasses::IsClassType("Lwdec", m_HwClass))
    {
        objCreateParam = LWGpuChannel::GetLwDecObjCreateParam(m_LwDecOffset, &lwdecAllocParam);
    }
    else if (EngineClasses::IsClassType("Lwjpg", m_HwClass))
    {
        objCreateParam = LWGpuChannel::GetLwJpgObjCreateParam(m_LwJpgOffset, &lwjpgAllocParam);
    }
    else
    {
        objCreateParam = 0;
    }
    m_pSubCh = pChHelper->create_object(m_HwClass, objCreateParam);

    if (!m_pSubCh)
    {
        return RC::SOFTWARE_ERROR;
    }

    CHECK_RC(CtrlTrustedContext());
    m_SubChNum = m_pSubCh->subchannel_number();

    return rc;
}

//------------------------------------------------------------------------------
void TraceSubChannel::ForceWfiMethod(WfiMethod m)
{
    m_WfiMethod = m;
}

//------------------------------------------------------------------------------
RC TraceSubChannel::AllocNotifier(const ArgReader * params, const string& channelName)
{
    RC rc;

    for (UINT32 subdev = 0; subdev < m_SubdevCount; ++subdev)
    {
        CHECK_RC(AllocNotifier2SubDev(params, channelName, subdev));
    }
    return rc;
}

//------------------------------------------------------------------------------
RC TraceSubChannel::AllocNotifier2SubDev
(
    const ArgReader* params, 
    const string& channelName, 
    UINT32 subdev
)
{
    RC rc;
    LwRm* pLwRm = GetLwRmPtr();

    m_NotifierLoc = GetLocationFromParams(params, "notifier", _DMA_TARGET_COHERENT);
    m_NotifierLayout = GetPageLayoutFromParams(params, "notifier");

    // Need to create a notifier for either -conlwrrent or for some WFI modes
    if ((params->ParamPresent("-conlwrrent") > 0) || (m_WfiMethod != WFI_POLL && m_WfiMethod != WFI_HOST))
    {
        DebugPrintf(MSGID(), "TraceChannel::AllocNotifier \"%s\".\n",
                m_Name.c_str());

        MdiagSurf* notifier = m_Notifiers[subdev].get();
        UINT32 notifierBytes = 32;
        if (params->ParamPresent("-va_reverse") > 0 ||
            params->ParamPresent("-va_reverse_notifier") > 0)
        {
            notifier->SetVaReverse(true);
        }
        if (params->ParamPresent("-pa_reverse") > 0 ||
            params->ParamPresent("-pa_reverse_notifier") > 0)
        {
            notifier->SetPaReverse(true);
        }
        notifier->SetArrayPitch(notifierBytes);
        notifier->SetColorFormat(ColorUtils::Y8);
        notifier->SetForceSizeAlloc(true);
        notifier->SetLocation(TargetToLocation(m_NotifierLoc));
        notifier->SetProtect(Memory::Writeable);
        notifier->SetGpuVASpace(m_pSubCh->channel()->GetVASpaceHandle());
        SetPageLayout(*notifier, m_NotifierLayout);

        if (notifier->GetLocation() != Memory::Fb)
        {
            if (notifier->GetGpuCacheMode() == Surface2D::GpuCacheOn)
            {
                ErrPrintf("The notifier buffer is in system memory and has GPU"
                    " caching turned on.  Use -vid_notifier to specify vidmem "
                    "or use -gpu_cache_off_notifier to turn off GPU caching.\n");
                return RC::ILWALID_ARGUMENT;
            }
            else if (notifier->GetGpuCacheMode() == Surface2D::GpuCacheDefault)
            {
                notifier->SetGpuCacheMode(Surface2D::GpuCacheOff);
            }
        }

        rc = notifier->Alloc(m_pLwGpuResource->GetGpuDevice(), pLwRm);
        if (rc != OK)
        {
            ErrPrintf("trace_3d: notifier DMA buffer creation for gpu %d failed.\n", subdev);
            return rc;
        }
        PrintDmaBufferParams(*notifier);
        rc = notifier->BindGpuChannel(m_pSubCh->channel()->ChannelHandle());
        if (rc != OK)
        {
            ErrPrintf("trace_3d: Unable to bind context dma to channel.\n");
            return rc;
        }
        rc = notifier->Map(subdev);
        if (rc != OK)
        {
            ErrPrintf("trace_3d: notifier DMA buffer mapping for gpu %d failed.\n", subdev);
            return rc;
        }
        
        string label = Utility::StrPrintf("Notifier(%s %s(%d))", 
            channelName.c_str(), m_Name.c_str(), subdev);
        m_BuffInfo->SetDmaBuff(label, *notifier, 0, subdev);
    }

    if ((m_WfiMethod == WFI_INTR) ||
        (m_WfiMethod == WFI_INTR_NONSTALL))
    {
        // Hook up an RM event to catch "awaken" interrupts sent by our HW object.
        ModsEvent *pModsEvent = Tasker::AllocEvent();
        if (!pModsEvent)
        {
            ErrPrintf("trace_3d: tasker event creation failed\n");
            return RC::SOFTWARE_ERROR;
        }

        m_ModsEvents[subdev].first = pModsEvent;

        LwRm::Handle hEvent;
        UINT32 notifyIndex = 0;
        if (m_SubdevCount > 1)
        {
            // specify the gpu subdev which the event is attacehd to
            notifyIndex |= DRF_NUM(0005, _NOTIFY_INDEX, _SUBDEVICE, subdev) |
                           LW01_EVENT_SUBDEVICE_SPECIFIC;
        }

        LwRm::Handle parentHandle = m_pSubCh->object_handle();
        if (m_WfiMethod == WFI_INTR_NONSTALL)
        {
            GpuSubdevice *pSubdevice = m_pLwGpuResource->GetGpuDevice()->GetSubdevice(subdev);
            parentHandle = pLwRm->GetSubdeviceHandle(pSubdevice);

            UINT32 engineType = 0;
            CHECK_RC(m_pSubCh->GetEngineType(&engineType));

            UINT32 notifier = MDiagUtils::GetRmNotifierByEngineType(engineType, pSubdevice, pLwRm);
            notifyIndex |= ((DRF_NUM(0005, _NOTIFY_INDEX, _INDEX, notifier) |
                            LW01_EVENT_NONSTALL_INTR));
        }

        void* const pOsEvent = Tasker::GetOsEvent(
                pModsEvent,
                pLwRm->GetClientHandle(),
                pLwRm->GetDeviceHandle(m_pLwGpuResource->GetGpuDevice()));

        rc = pLwRm->AllocEvent(
            parentHandle,
            &hEvent,
            LW01_EVENT_OS_EVENT,
            notifyIndex,
            pOsEvent);

        if (rc != OK)
        {
            ErrPrintf("trace_3d: RM event creation failed,\n");
            return rc;
        }

        m_ModsEvents[subdev].second = hEvent;
    }

    return OK;
}

//------------------------------------------------------------------------------
void TraceSubChannel::ClearNotifier()
{
    for (UINT32 subdev = 0; subdev < m_SubdevCount; ++subdev)
    {
        if (m_Notifiers[subdev]->GetCtxDmaHandle())
        {
            vector<UINT08> data(m_Notifiers[subdev]->GetSize(), 0xff);
            Platform::VirtualWr(m_Notifiers[subdev]->GetAddress(), &data[0], data.size());
        }
    }
}

//------------------------------------------------------------------------------
RC TraceSubChannel::SendNotifier()
{
    RC rc;

    for (UINT32 subdev = 0; subdev < m_SubdevCount; ++subdev)
    {
        if (m_SubdevCount > 1)
            CHECK_RC(m_pSubCh->channel()->GetModsChannel()->WriteSetSubdevice(1 << subdev));
        CHECK_RC(SendNotifier2SubDev(subdev));
        if (m_SubdevCount > 1)
            CHECK_RC(m_pSubCh->channel()->GetModsChannel()->WriteSetSubdevice(Channel::AllSubdevicesMask));
    }
    return rc;
}

//------------------------------------------------------------------------------
RC TraceSubChannel::SendNotifier2SubDev(UINT32 subdev)
{
    RC rc;

    if (!m_Notifiers[subdev]->GetCtxDmaHandle())
    {
        // We are not using notifiers (m_WfiMethod is not intr or notify).
        return OK;
    }

    DebugPrintf(MSGID(), "TraceChannel::SendNotifier \"%s\" for gpu %d.\n",
            m_Name.c_str(), subdev);

    if (m_WfiMethod == WFI_INTR_NONSTALL)
    {
        rc = SendNonstallIntr2SubDev(subdev);
    }
    else
    {
        // WFI_INTR or WFI_NOTIFY
        rc = SendIntrNotifier2SubDev(subdev);
    }

    return rc;
}

RC TraceSubChannel::SendIntrNotifier2SubDev(UINT32 subdev)
{
    RC rc;

    // Redirect the notifier (or semaphore, in some classes) dma-ctx
    // to our private notifier surface.
    // This method is universal to all classes, yay architecture!
    // Fermi MSDEC/CE/VC/VE uses semaphore and does not support notify methods
    if (!NonNotifySubChannel() && !VideoSubChannel())
    {
        if (m_Params->ParamPresent("-kepler_preemption_hack") > 0)
        {
            // Special hack for kepler preemption tests
            InfoPrintf("Injecting WFI/NOOP methods to the end of PB ...\n");
            CHECK_RC(m_pSubCh->MethodWriteRC(LWA097_WAIT_FOR_IDLE, 0));
            for (UINT32 count = 0; count < 100; ++count)
            {
                CHECK_RC(m_pSubCh->MethodWriteRC(LWA097_PIPE_NOP, 0) );
            }
        }

        if (!m_Notifiers[subdev]->GetAddress())
            m_Notifiers[subdev]->Map(subdev);
        CHECK_RC(m_pSubCh->MethodWriteRC(LW9097_SET_NOTIFY_A, (UINT32)(m_Notifiers[subdev]->GetCtxDmaOffsetGpu() >> 32)));
        CHECK_RC(m_pSubCh->MethodWriteRC(LW9097_SET_NOTIFY_B, (UINT32)(m_Notifiers[subdev]->GetCtxDmaOffsetGpu() & 0xffffffff)));
    }

    // The notifier method(s) itself is not universal.
    if (VideoSubChannel() ||
        m_HwClass == LW86B6_VIDEO_COMPOSITOR)
    {
        CHECK_RC(SendMsdecSemaphore(m_Notifiers[subdev]->GetCtxDmaOffsetGpu()));
    }
    else if (NonNotifySubChannel())
    {
        CHECK_RC(SendCeSemaphore(m_Notifiers[subdev]->GetCtxDmaOffsetGpu()) );
    }
    else
    {
        CHECK_RC( m_pSubCh->MethodWriteRC(LW9097_NOTIFY,
                m_WfiMethod == WFI_INTR ? LW9097_NOTIFY_TYPE_WRITE_THEN_AWAKEN
                                        : LW9097_NOTIFY_TYPE_WRITE_ONLY) );
        CHECK_RC( m_pSubCh->MethodWriteRC(LW9097_NO_OPERATION, 0) );
    }

    CHECK_RC(m_pSubCh->channel()->MethodFlushRC());

    return OK;
}

RC TraceSubChannel::SendNonstallIntr2SubDev(UINT32 subdev)
{
    RC rc;
    Channel *channel = m_pSubCh->channel()->GetModsChannel();
    SemaphoreChannelWrapper *pSemWrap = channel->GetSemaphoreChannelWrapper();
    MASSERT((pSemWrap != NULL) && pSemWrap->SupportsBackend());

    // Not see wfi with backend semaphore
    // So injecting a wfi method here
    if (GrSubChannel() || ComputeSubChannel())
    {
        CHECK_RC(m_pSubCh->MethodWriteRC(LW9097_WAIT_FOR_IDLE, 0));
    }

    // Just follow existed function SendCeSemaphore() and
    // SendMsdecSemaphore() to use 0 as payload here.
    m_UsingSemaphoreWfi = true;
    return pSemWrap->ReleaseBackendSemaphoreWithTrap(
        m_Notifiers[subdev]->GetCtxDmaOffsetGpu(), 0, false, nullptr);
}

//------------------------------------------------------------------------------
bool TraceSubChannel::PollNotifierDone()
{
    bool NotifierDone = true;

    for (UINT32 subdev = 0; subdev < m_SubdevCount; ++subdev)
    {
        NotifierDone = NotifierDone && PollNotifierDone2SubDev(subdev);
    }
    return NotifierDone;
}

//------------------------------------------------------------------------------
bool TraceSubChannel::PollNotifierDone2SubDev(UINT32 subdev)
{
    // Make sure m_WfiMethod is either WFI_INTR or WFI_NOTIFY
    //
    switch (m_WfiMethod)
    {
        case WFI_POLL:
        case WFI_SLEEP:
            // WFI_POLL does all of its polling in the RM, in
            // LwRmIdleChannels().  So, ironically, WFI_POLL is the
            // one WFI method that we can't poll for in mods.
            //
            MASSERT(!"PollNotifierDone2Subdev does not support WFI_POLL or WFI_SLEEP");
            return false;
        case WFI_INTR:
        case WFI_NOTIFY:
        case WFI_INTR_NONSTALL:
            // Handled below
            break;
        case WFI_HOST:
            MASSERT(!"Should never get here");
            return false;
        default:
            MASSERT(!"invalid wfi method");
            return false;
    }

    // If WFI_INTR, wait for the interrupt before checking the notifier
    //
    bool waitInterrupt = (m_WfiMethod == WFI_INTR) || (m_WfiMethod == WFI_INTR_NONSTALL);
    if (waitInterrupt && !Tasker::IsEventSet(m_ModsEvents[subdev].first))
    {
        return false;
    }

    // Check the notifier
    //
    void *semaphoreAddress = nullptr;
    if (m_UsingSemaphoreWfi)
    {
        // These lack a notifier method, but we can use a
        // semaphore to write an arbitrary 4-byte value.
        // The Notifier class doesn't check alignment, and
        // only reads byte 15, so we'll lie about the
        // starting address of the notifier.
        semaphoreAddress = static_cast<char*>(m_Notifiers[subdev]->GetAddress()) - 12;
    }
    else
    {
        // Notify method. Notifier::IsSetExternal() will shift 15 bytes internally
        semaphoreAddress = m_Notifiers[subdev]->GetAddress();
    }

    return Notifier::IsSetExternal(semaphoreAddress);
}

//------------------------------------------------------------------------------
UINT16 TraceSubChannel::PollNotifierValue()
{
    if (m_SubdevCount > 1)
        MASSERT(!"Not supported yet");

    MASSERT(m_Notifiers[0]->GetCtxDmaHandle());

    uintptr_t address = (uintptr_t)(m_Notifiers[0]->GetAddress());
    return MEM_RD16(address + 14);
    return 0;
}

//------------------------------------------------------------------------------
UINT32 TraceSubChannel::GetClass() const
{
    return m_HwClass;
}

//------------------------------------------------------------------------------
MethodMassager TraceSubChannel::GetMassager() const
{
    return m_Massager;
}

//------------------------------------------------------------------------------
WfiMethod TraceSubChannel::GetWfiMethod() const
{
    return m_WfiMethod;
}

//------------------------------------------------------------------------------
RC TraceSubChannel::CtrlTrustedContext()
{
    if (!m_TrustedContext)
        return OK;

    ErrPrintf("Trusted context control interface not implemented for class %x\n", m_HwClass);
    MASSERT(!"Trusted context control interface not implemented");
    return RC::SOFTWARE_ERROR;
}

//------------------------------------------------------------------------------
bool TraceSubChannel::GrSubChannel() const
{
    return EngineClasses::IsClassType("Gr", m_HwClass);
}

bool TraceSubChannel::I2memSubChannel() const
{
    // Kepler_inline_to_memory_a was only used on Kepler chips.
    // Kepler_inline_to_memory_b is used on all GPUs since Kepler, including on Turing.
    return KEPLER_INLINE_TO_MEMORY_B == m_HwClass;
}

bool TraceSubChannel::ComputeSubChannel() const
{
    return EngineClasses::IsClassType("Compute", m_HwClass);
}

bool TraceSubChannel::VideoSubChannel() const
{
    return m_HwClass == GF100_MSPPP ||
           m_HwClass == LW95A1_TSEC ||
           m_HwClass == LW95B1_VIDEO_MSVLD ||
           m_HwClass == LW95B2_VIDEO_MSPDEC ||
           EngineClasses::IsClassType("Lwdec", m_HwClass) ||
           EngineClasses::IsClassType("Lwenc", m_HwClass) ||
           EngineClasses::IsClassType("Lwjpg", m_HwClass) ||
           EngineClasses::IsClassType("Ofa", m_HwClass);
}

bool TraceSubChannel::NonNotifySubChannel() const
{
    return EngineClasses::IsClassType("Ce", m_HwClass) ||
           m_HwClass == GF106_DMA_DECOMPRESS;
}

bool TraceSubChannel::CopySubChannel() const
{
    return EngineClasses::IsClassType("Ce", m_HwClass); 
}

LwRm* TraceSubChannel::GetLwRmPtr()
{
    LWGpuChannel* pCh = m_pSubCh->channel();
    MASSERT(pCh);

    return (pCh->GetLwRmPtr());
}

