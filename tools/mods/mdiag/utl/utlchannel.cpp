/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/rc.h"
#include "gpu/utility/semawrap.h"
#include "gpu/include/gpudev.h"
#include "gpu/tests/gputestc.h"
#include "mdiag/sysspec.h"
#include "mdiag/lwgpu.h"
#include "mdiag/utils/utils.h"
#include "mdiag/utils/lwgpu_classes.h"
#include "mdiag/smc/smcengine.h"
#include "class/cl906f.h"       // GF100_CHANNEL_GPFIFO
#include "class/cla06f.h"       // KEPLER_CHANNEL_GPFIFO_A
#include "ctrl/ctrlc36f.h"

#include "class/cl85b5sw.h"     // LW85B5_ALLOCATION_PARAMETERS
#include "class/cla0b5sw.h"     // LWA0B5_ALLOCATION_PARAMETERS
#include "class/cla06fsubch.h"  // LWA06F_SUBCHANNEL_COPY_ENGINE

#include "utlchannel.h"
#include "utlchanwrap.h"
#include "utlgpu.h"
#include "utlusertest.h"
#include "utlutil.h"
#include "utlengine.h"
#include "utlgpupartition.h"
#include "utlsmcengine.h"
#include "utlrawmemory.h"
#include "utltest.h"

map<LWGpuChannel*, unique_ptr<UtlChannel>> UtlChannel::s_Channels;
map<UtlTest*, map<string, UtlChannel*>>  UtlChannel::s_ScriptChannels;

void UtlChannel::Register(py::module module)
{
    UtlKwargsMgr* kwargs = UtlKwargsMgr::Instance();
    kwargs->RegisterKwarg("Channel.WriteMethods", "method", true, "method[s] to be written- can be a single value or a list of values");
    kwargs->RegisterKwarg("Channel.WriteMethods", "data", true, "data[s] to be written- can be a single value or a list of values");
    kwargs->RegisterKwarg("Channel.WriteMethods", "subchannel", false, "subchannel number; if not given, the most recent number seen by the semaphore channel wrapper will be used, or 0 if no such wrapper exists");
    kwargs->RegisterKwarg("Channel.WriteMethods", "userTest", false, "user test responsible for the inserted methods");
    kwargs->RegisterKwarg("Channel.WriteMethods", "mode", false, "Channel::MethodType enum; defaults to INCREMENTING");
    kwargs->RegisterKwarg("Channel.Create", "name", true, "a unique name for this channel");
    kwargs->RegisterKwarg("Channel.Create", "gpu", false, "GPU to which this channel belongs");
    kwargs->RegisterKwarg("Channel.Create", "tsg", false, "TSG to which this channel belongs");
    kwargs->RegisterKwarg("Channel.Create", "engine", false, "engine associated with this channel; required for Ampere and later if tsg is not given");
    kwargs->RegisterKwarg("Channel.Create", "userTest", false, "user test associated with this channel");
    kwargs->RegisterKwarg("Channel.Create", "pushBufferAperture", false, "channel's PushBuffer aperture enum. Possible values are Utl.Mmu.APERTURE.FB, Utl.Mmu.APERTURE.COHERENT and Utl.Mmu.APERTURE.NONCOHERENT. Defaults to FB for Amodel and NONCOHERENT for others.");
    kwargs->RegisterKwarg("Channel.Create", "gpFifoAperture", false, "channel's GpFifo aperture enum. Possible values are Utl.Mmu.APERTURE.FB, Utl.Mmu.APERTURE.COHERENT and Utl.Mmu.APERTURE.NONCOHERENT. Defaults to FB for Amodel and NONCOHERENT for others.");
    kwargs->RegisterKwarg("Channel.Create", "errorNotifierAperture", false, "channel's ErrorNotifier aperture enum. Possible values are Utl.Mmu.APERTURE.FB, Utl.Mmu.APERTURE.COHERENT and Utl.Mmu.APERTURE.NONCOHERENT. Defaults to FB for Amodel and COHERENT for others.");
    kwargs->RegisterKwarg("Channel.CreateSubchannel", "hwClass", true, "subchannel class");
    kwargs->RegisterKwarg("Channel.CreateSubchannel", "inst", true, "subchannel number");    
    kwargs->RegisterKwarg("Channel.GetVaSpace", "test", false, "Test object");
    kwargs->RegisterKwarg("Channel.WriteSetSubdevice", "subdevMask", true, "Subdevice mask");

    py::class_<UtlChannel> channel(module, "Channel");
    channel.def_property("methodCount",
        &UtlChannel::GetMethodCount,
        &UtlChannel::SetMethodCount,
        "A count of the number of test methods that will be sent to this channel.  This property is read-only unless the channel is created with the utl.Channel.Create function.");
    channel.def_property_readonly("name", &UtlChannel::GetName,
        "A read-only string represents the name of the channel.");
    channel.def("WriteMethods", &UtlChannel::WriteMethods,
        UTL_DOCSTRING("Channel.WriteMethods", 
        "This function writes one or more methods to the channel. If more than one method is specified then the method count should match data count. MODS would then send each of the method:data pair with count=1 (INCREMENTING_IMM does not have a count). For a single method, multiple datas are allowed (except for INCREMENTING_IMM which expects only one data per method). For other modes with single method, MODS would send one method with the mode and count set followed by all the data[s]."));
    channel.def("Flush", &UtlChannel::Flush,
        "This function flushes all previously written methods on this channel.");
    channel.def("WaitForIdle", &UtlChannel::WaitForIdle,
        "This function will wait for the channel to execute all methods that have been written to it.");
    channel.def_static("Create", &UtlChannel::CreatePy,
        UTL_DOCSTRING("Channel.Create", "This static function creates a global channel."),
        py::return_value_policy::reference);
    channel.def("CreateSubchannel", &UtlChannel::CreateSubchannel,
        UTL_DOCSTRING("Channel.CreateSubchannel", "This function creates a sub channel on target channel."));
    channel.def("Disable", &UtlChannel::Disable,
        "This function will disable the channel");
    channel.def("Reset", &UtlChannel::Reset,
        "This function will reset channel and engine without unmap context");
    channel.def("GetEngine", &UtlChannel::GetEngine,
        "This function returns the Engine object associated with the channel",
        py::return_value_policy::reference);
    channel.def("GetClass", &UtlChannel::GetClass,
        UTL_DOCSTRING("Channel.GetClass", "This function returns the class type of channel (eg: HOPPER_CHANNEL_GPFIFO_A)."));
    channel.def("GetType", &UtlChannel::GetChannelType,
        UTL_DOCSTRING("Channel.GetType", "This function returns the channel type (PIO/FIFO)."));

    channel.def("GetInstanceBlock", &UtlChannel::GetInstanceBlock,
        UTL_DOCSTRING("Channel.GetInstanceBlock", "This function returns the instance block object associated with the channel."),
        py::return_value_policy::reference);
    channel.def("GetSubchannelCEInstances", &UtlChannel::GetSubchannelCEInstances,
        "This function returns a list of CE subchannel instances within this channel.");
    channel.def("GetChannelId", &UtlChannel::GetChannelId,
        UTL_DOCSTRING("Channel.GetChannelId", "This function returns the channel Id."));
    channel.def("GetPhysicalChannelId", &UtlChannel::GetPhysicalChannelId,
        UTL_DOCSTRING("Channel.GetPhysicalChannelId", "This function returns the physical channel Id for VF channels. In PF/default mode, physical and virtual channel Ids are the same, hence it will return the same Id as GetChannelId()"));
    channel.def("GetLogicalCopyEngineId", &UtlChannel::GetLogicalCopyEngineId,
        UTL_DOCSTRING("Channel.GetLogicalCopyEngineId", "This function returns the logical copy engine Id."));
    channel.def("GetSmcEngine", &UtlChannel::GetSmcEngine,
        UTL_DOCSTRING("Channel.GetSmcEngine", "This function returns the SmcEngine object associated with the channel."),
        py::return_value_policy::reference);
    channel.def("GetGpu", &UtlChannel::GetGpu,
        UTL_DOCSTRING("Channel.GetGpu", "This function returns the gpu device object associated with the channel."),
        py::return_value_policy::reference);
    channel.def("GetDoorbellToken", &UtlChannel::GetDoorbellToken,
        UTL_DOCSTRING("Channel.GetDoorbellToken", "This function returns the doorbell token for the channel which can be used to ring doorbell after GpPut update."));
    channel.def_property("doorbellRingEnable",
        &UtlChannel::GetDoorbellRingEnable,
        &UtlChannel::SetDoorbellRingEnable,
        "A boolean property representing whether MODS doorbell ring (aftr GpPut update) is enabled/disabled. It is enabled by default in MODS. Disabling it is not supported when autoflush is enabled since it might interfere with MODS work submission. Please be very careful when disabling doorbell ring as it may cause additional issues.");
    channel.def("CheckForErrors", &UtlChannel::CheckForErrors,
        UTL_DOCSTRING("Channel.CheckForErrors", "This function checks the channel for any errors that may have oclwred while processing methods.  If an error is found (e.g., a page fault when exelwting the channel methods), an exception will be raised."));
    channel.def("ClearError", &UtlChannel::ClearError,
        UTL_DOCSTRING("Channel.ClearError", "This functions clears the channel error."));
    channel.def("GetSubCtx", &UtlChannel::GetSubCtx,
        UTL_DOCSTRING("Channel.GetSubCtx", "This function returns the subctx corresponding to the channel."), py::return_value_policy::reference);
    channel.def("GetTsg", &UtlChannel::GetTsg,
        UTL_DOCSTRING("Channel.GetTsg", "This function returns the tsg corresponding to the channel."), py::return_value_policy::reference);
    channel.def("GetVaSpace", &UtlChannel::GetVaSpace,
        UTL_DOCSTRING("Channel.GetVaSpace", "The VaSpace belonging to this channel."), py::return_value_policy::reference);
    channel.def("WriteSetSubdevice", &UtlChannel::WriteSetSubdevice,
        UTL_DOCSTRING("Channel.WriteSetSubdevice", "This function sets the subdevice mask"));

    py::enum_<MethodType>(channel, "MethodType",
        "An enum specifying how methods should be written in WriteMethods().")
        .value("NON_INCREMENTING", NON_INCREMENTING)
        .value("INCREMENTING", INCREMENTING)
        .value("INCREMENTING_ONCE", INCREMENTING_ONCE)
        .value("INCREMENTING_IMM", INCREMENTING_IMM);

    py::enum_<ChannelType>(channel, "ChannelType",
        "An enum indicating the channel type")
        .value("PIO", PIO)
        .value("GPFIFO", GPFIFO);

    UtlChannel::PythonChannelConstants ChannelConstants(channel, "Constants");
    UTL_BIND_CONSTANT(ChannelConstants, LW2080_ENGINE_TYPE_ALLENGINES, "NO_CE");
}

// This function is the external API for creating UtlChannel objects.
//
UtlChannel* UtlChannel::Create
(
    LWGpuChannel* channel,
    const string& name,
    vector<UINT32>* pSubChCEInsts
)
{
    MASSERT(s_Channels.count(channel) == 0);
    unique_ptr<UtlChannel> utlChannel(new UtlChannel(channel, name, pSubChCEInsts));
    UtlChannel* returnPtr = utlChannel.get();
    s_Channels[channel] = move(utlChannel);

    return returnPtr;
}

// This constructor should only be called from UtlChannel::Create.
//
UtlChannel::UtlChannel
(
    LWGpuChannel* channel,
    const string& name,
    vector<UINT32>* pSubChCEInsts
) :
    m_Channel(channel),
    m_Name(name),
    m_UtlTsg(nullptr),
    m_pUtlEngine(nullptr)
{
    MASSERT(channel != nullptr);

    if (pSubChCEInsts)
    {
        m_SubChCEInsts = *pSubChCEInsts;
    }
    m_UtlChannelWrapper = channel->GetModsChannel()->GetUtlChannelWrapper();
    m_UtlChannelWrapper->SetUtlChannel(this);

    UtlGpuPartition* pUtlGpuPartition = channel->GetSmcEngine() ?
            UtlGpuPartition::GetFromGpuPartitionPtr(channel->GetSmcEngine()->GetGpuPartition()) :
            nullptr;

    GpuDevice* pGpuDev = GetGpuDevice();

    if (pGpuDev->GetSubdevice(0)->HasFeature(Device:: GPUSUB_HOST_REQUIRES_ENGINE_ID))
    {
        UINT32 engineId = channel->GetEngineId();

        // In SMC mode, UtlEngine objects are created per MemoryPartition but 
        // the GR channels are being created with engineId=GR0 (with SmcEngine 
        // RM client) hence MemoryPartition-local SmcEngineId needs to be used 
        // for UtlEngine creation.
        if (pUtlGpuPartition && MDiagUtils::IsGraphicsEngine(engineId))
        {
            engineId = MDiagUtils::GetGrEngineId(channel->GetSmcEngine()->GetSmcEngineId());
        }

        m_pUtlEngine = UtlEngine::Create(
            engineId,
            GetGpu(),
            pUtlGpuPartition
        );
    }
}

UtlChannel::~UtlChannel()
{
    if (m_OwnedLWGpuChannel.get())
    {
        m_OwnedLWGpuChannel.reset(nullptr);

        if (m_UtlTsg != nullptr)
        {
            m_UtlTsg->GetLWGpuTsg()->RelRef();
            if (0 == m_UtlTsg->GetLWGpuTsg()->GetRefCount())
            {
                delete m_UtlTsg->GetLWGpuTsg();
            }
        }
    }
}

// Free the UtlChannel object for the passed LWGpuChannel object
//
void UtlChannel::FreeChannel(LWGpuChannel* channel)
{
    if (s_Channels.find(channel) != s_Channels.end())
    {
        UtlChannel* utlChannel = GetFromChannelPtr(channel);
        for (auto keyValue : s_ScriptChannels)
        {
            auto iter = keyValue.second.find(utlChannel->m_Name);
            if (iter != keyValue.second.end())
            {
                keyValue.second.erase(iter);
            }
        }
        s_Channels.erase(channel);
    }
}

// Free all allocated UtlChannel objects.
//
void UtlChannel::FreeChannels()
{
    s_ScriptChannels.clear();
    s_Channels.clear();
}

// This function can be called by UTL script to
// create a Global channel.
//
UtlChannel* UtlChannel::CreatePy(py::kwargs kwargs)
{
    UtlUtility::RequirePhase(Utl::InitEventPhase | Utl::RunPhase,
        "Channel.Create");

    // Parse arguments from Python script
    string name = UtlUtility::GetRequiredKwarg<string>(kwargs, "name",
            "Channel.Create");
    UtlTsg* pTsg = nullptr;
    UtlUtility::GetOptionalKwarg<UtlTsg*>(&pTsg, kwargs, "tsg", "Channel.Create");

    UtlGpu* gpu = nullptr;
    UtlUtility::GetOptionalKwarg<UtlGpu*>(&gpu, kwargs, "gpu", "Channel.Create");

    // If a userTest parameter is passed in the kwargs, the channel is
    // associated with the given user test.  The UtlChannel::s_Channels member
    // will still own the UtlChannel object on the C++ side, but the user test
    // will own the Channel object on the Python side.
    UtlUserTest* userTest = nullptr;
    py::object userTestObj;
    UtlTest* utlTest = Utl::Instance()->GetTestFromScriptId();

    if (UtlUtility::GetOptionalKwarg<py::object>(&userTestObj, kwargs, "userTest", "Channel.Create"))
    {
        userTest = UtlUserTest::GetTestFromPythonObject(userTestObj);

        if (userTest == nullptr)
        {
            UtlUtility::PyError("invalid userTest argument.");
        }
        else if (userTest->GetChannel(name) != nullptr)
        {
            UtlUtility::PyError("userTest object already has Channel with name %s.",
                name.c_str());
        }
    }
    else
    {
        auto iter = s_ScriptChannels.find(utlTest);
        if ((iter != s_ScriptChannels.end()) &&
            (*iter).second.find(name) != (*iter).second.end())
        {
            UtlUtility::PyError("channel with name '%s' already exists.",
                name.c_str());
        }
    }

    if ((pTsg == nullptr) && (gpu == nullptr))
    {
        UtlUtility::PyError("Channel.Create requires either tsg or gpu parameter.");
    }

    GpuDevice* pGpuDevice = gpu ? gpu->GetGpudevice() : pTsg->GetGpuDevice();
    UtlEngine* pEngine = nullptr;
    UINT32 engineId = LW2080_ENGINE_TYPE_NULL;
    if (pGpuDevice->GetSubdevice(0)->HasFeature(Device:: GPUSUB_HOST_REQUIRES_ENGINE_ID))
    {
        UtlUtility::GetOptionalKwarg<UtlEngine*>(&pEngine, kwargs, "engine", "Channel.Create");
        if (!pTsg && !pEngine)
        {
            UtlUtility::PyError("For Ampere and later, engine or tsg is required when creating a channel");
        }

        if (pEngine)
        {
            if (pEngine->IsGrCE())
            {
                engineId = LW2080_ENGINE_TYPE_GR0;
            }
            else
            {
                engineId = pEngine->GetEngineId();
            }
        }
        else
        {
            engineId = pTsg->GetEngineId();
        }

        // sanity check engineId if both Tsg and engine are given
        if (pTsg && pEngine && pTsg->GetEngineId() != engineId)
        {
            UtlUtility::PyError("mismatch between engine and tsg on channel creation");
        }
    }

    UtlMmu::APERTURE pushBufferAperture = UtlMmu::APERTURE::APERTURE_ILWALID;
    UtlMmu::APERTURE gpFifoAperture = UtlMmu::APERTURE::APERTURE_ILWALID;
    UtlMmu::APERTURE errorNotifierAperture = UtlMmu::APERTURE::APERTURE_ILWALID;
    UtlUtility::GetOptionalKwarg<UtlMmu::APERTURE>(
        &pushBufferAperture, kwargs, "pushBufferAperture", "Channel.Create");
    UtlUtility::GetOptionalKwarg<UtlMmu::APERTURE>(
        &gpFifoAperture, kwargs, "gpFifoAperture", "Channel.Create");
    UtlUtility::GetOptionalKwarg<UtlMmu::APERTURE>(
        &errorNotifierAperture, kwargs, "errorNotifierAperture", "Channel.Create");

    UtlGil::Release gil;

    LWGpuResource* lwgpu = nullptr;
    if (gpu != nullptr)
    {
        lwgpu = gpu->GetGpuResource();
    }
    else
    {
        lwgpu = pTsg->GetLwGpuRes();
    }

    LwRm* pLwRm = LwRmPtr().Get();
    if (pTsg)
    {
        pLwRm = pTsg->GetLwRmPtr();
    }
    else if (pEngine)
    {
        pLwRm = pEngine->GetLwRmPtr();
    }

    // In SMC mode, UtlEngine objects are created per MemoryPartition but 
    // the GR channels need to be created using SmcEngine RM client and 
    // engineId = GR0 and non-GR channels will be created using SmcEngine0's
    // RM client and MemoryPartition local engineId since they are shared
    // among SmcEngines
    if (pEngine && pEngine->GetGpuPartition())
    {
        SmcEngine* pSmcEngine = nullptr;
        GpuPartition* pGpuPartition = pEngine->GetGpuPartition()->GetGpuPartitionRawPtr();
        if (MDiagUtils::IsGraphicsEngine(engineId))
        {
            pSmcEngine = pGpuPartition->GetSmcEngineByType(pEngine->GetEngineId());
            engineId = LW2080_ENGINE_TYPE_GR0;
        }
        else
        {
            pSmcEngine = pGpuPartition->GetSmcEngine(0);
        }
        pLwRm = lwgpu->GetLwRmPtr(pSmcEngine);
    }

    // Create related LwGpuChannel object.
    unique_ptr<LWGpuChannel> pLWGpuChannel = 
        make_unique<LWGpuChannel>(lwgpu, pLwRm, pTsg ? pTsg->GetSmcEngine() : nullptr);
    if (pLWGpuChannel.get() == nullptr)
    {
        UtlUtility::PyError("unable to create LWGpuChannel.");
    }

    pLWGpuChannel->SetName(name);
    pLWGpuChannel->ParseChannelArgs(lwgpu->GetLwgpuArgs());
    
    unique_ptr<MdiagSurf> pErrorNotifier = make_unique<MdiagSurf>();
    unique_ptr<MdiagSurf> pPushBuffer = make_unique<MdiagSurf>();
    unique_ptr<MdiagSurf> pGpFifo = make_unique<MdiagSurf>();
    LwRm::Handle channelHandle;
    LwRm::Handle hVaSpace = pTsg ? pTsg->GetVASpaceHandle() : 0;

    pPushBuffer->SetLocation(
        UtlMmu::ColwertMmuApertureToMemoryLocation(pushBufferAperture));
    pGpFifo->SetLocation(
        UtlMmu::ColwertMmuApertureToMemoryLocation(gpFifoAperture));
    pErrorNotifier->SetLocation(
        UtlMmu::ColwertMmuApertureToMemoryLocation(errorNotifierAperture));

    RC rc = MDiagUtils::AllocateChannelBuffers(pGpuDevice,
                pErrorNotifier.get(), pPushBuffer.get(), pGpFifo.get(),
                name.c_str(), hVaSpace, pLwRm);
    UtlUtility::PyErrorRC(rc, "MDiagUtils::AllocateChannelBuffers fails");

    GpuTestConfiguration testConfig;
    UINT32 channelClass;
    if (Platform::GetSimulationMode() == Platform::Amodel)
    {
        channelClass = KEPLER_CHANNEL_GPFIFO_A;
    }
    else
    {
        rc = pLwRm->GetFirstSupportedClass(
                testConfig.GetNumFifoClasses(),
                testConfig.GetFifoClasses(),
                &channelClass,
                pGpuDevice);

        UtlUtility::PyErrorRC(rc, "RM GetFirstSupportedClass fails");
    }

    rc = pLwRm->AllocChannelGpFifo(
            &channelHandle,
            channelClass,
            pTsg ? 0 : pErrorNotifier->GetMemHandle(),
            pTsg ? nullptr : pErrorNotifier->GetAddress(),
            pPushBuffer->GetMemHandle(),
            pPushBuffer->GetAddress(),
            pPushBuffer->GetCtxDmaOffsetGpu(),
            (UINT32) pPushBuffer->GetSize(),
            pGpFifo->GetAddress(),
            pGpFifo->GetCtxDmaOffsetGpu(),
            512,
            0, // Handle ContextSwitchObjectHandle
            0, // verifFlags
            pLWGpuChannel->GetVerifFlags2(engineId, channelClass),
            0, // UINT32 flags
            pGpuDevice,
            pTsg ? pTsg->GetLWGpuTsg() : nullptr,
            hVaSpace,
            engineId,
            pLWGpuChannel->GetUseBar1Doorbell());

    UtlUtility::PyErrorRC(rc, "Unable to allocate UTL created GpFifo channel %s.",
        name.c_str());

    UINT32 hCtxDma = pGpuDevice->GetGartCtxDma(pLwRm);
    rc = pLwRm->BindContextDma(channelHandle, hCtxDma);
    UtlUtility::PyErrorRC(rc, "Couldn't bind GPU device Gart Ctx Dma: %s",
        rc.Message());

    LwRm::Handle errCtxDmaHandle = 0;
    if (pTsg != nullptr)
    {
        // Channels in a TSG do not have individual error notifier buffers.
        // Instead the TSG will have one error notifier that will be used
        // by all the channels.
        errCtxDmaHandle = pTsg->GetLWGpuTsg()->GetErrCtxDmaHandle();
    }
    else
    {
        errCtxDmaHandle = pErrorNotifier->GetCtxDmaHandle();
    }

    if (errCtxDmaHandle != 0)
    {
        rc = pLwRm->BindContextDma(channelHandle,
            errCtxDmaHandle);
        UtlUtility::PyErrorRC(rc, "Couldn't bind error notifier to channel: %s",
            rc.Message());
    }

    Channel *pChannel = pLwRm->GetChannel(channelHandle);
    pChannel->EnableLogging(lwgpu->GetChannelLogging());
    pLWGpuChannel->SetModsChannel(pChannel);
    pLWGpuChannel->SetChannelHandle(channelHandle);
    pLWGpuChannel->SetUpdateType(pLWGpuChannel->GetUpdateType());

    // Create related UtlChannel object
    UtlChannel* utlChannel = UtlChannel::Create(pLWGpuChannel.get(), name, nullptr);

    auto iter = s_ScriptChannels.find(utlTest);
    if (iter == s_ScriptChannels.end())
    {
        iter = s_ScriptChannels.emplace(
            make_pair(utlTest, map<string, UtlChannel*>())).first;
    }
    (*iter).second.emplace(make_pair(name, utlChannel));

    utlChannel->m_OwnedLWGpuChannel = move(pLWGpuChannel);
    utlChannel->m_ErrorNotifier = move(pErrorNotifier);
    utlChannel->m_PushBuffer = move(pPushBuffer);
    utlChannel->m_GpFifo = move(pGpFifo);

    if (pTsg != nullptr)
    {
        utlChannel->SetTsg(pTsg);
        pTsg->GetLWGpuTsg()->AddRef();
        utlChannel->GetLwGpuChannel()->SetLWGpuTsg(pTsg->GetLWGpuTsg());
    }

    if (userTest != nullptr)
    {
        userTest->AddChannel(name, utlChannel);
    }

    utlChannel->PrintChannelInfo();

    return utlChannel;
}

// This function can be called from a UTL script
// to create subchannel.
//
void UtlChannel::CreateSubchannel(py::kwargs kwargs)
{
    UINT32 hwClass = UtlUtility::GetRequiredKwarg<UINT32>(kwargs, "hwClass",
            "Channel.CreateSubchannel");

    UINT32 inst = UtlUtility::GetRequiredKwarg<UINT32>(kwargs, "inst",
            "Channel.CreateSubchannel");

    UtlGil::Release gil;

    LwRm* pLwRm = GetLwRmPtr();
    GpuDevice* pGpuDev = GetGpuDevice();
    void *params = nullptr;
    LW_MSENC_ALLOCATION_PARAMETERS lwencAllocParam = {};
    LW_BSP_ALLOCATION_PARAMETERS lwdecAllocParam = {};
    LW_LWJPG_ALLOCATION_PARAMETERS lwjpgAllocParam = {};

    // Check whether hwClass is valid or not.
    static const UINT32 hwClasses[] =
    {
        hwClass
    };

    UINT32 subClass;
    RC rc = pLwRm->GetFirstSupportedClass(
            sizeof(hwClasses) / sizeof(UINT32),
            hwClasses,
            &subClass,
            pGpuDev);
    UtlUtility::PyErrorRC(rc, "hwClass is invalid.");
    
    // Check whether hwClass is ce class.

    UINT32 subChClass;
    rc = EngineClasses::GetFirstSupportedClass(
            pLwRm,
            pGpuDev,
            "Ce",
            &subChClass);
   
    // If rc != OK or
    // rc == OK, subChClass != hwClass
    // that means hwClass is not a ce class.
    vector<UINT08> objParams;
    if (rc == OK && subChClass == hwClass)
    {
        // Reference wiki: https://confluence.lwpu.com/display/ArchMods/CreateChannel+and+CreateSubChannel
        string engineName = pGpuDev->GetEngineName(m_Channel->GetEngineId());
        if (engineName.compare(0, 8, "GRAPHICS") == 0)
        {
            objParams.resize(sizeof(LWA0B5_ALLOCATION_PARAMETERS));
            LWA0B5_ALLOCATION_PARAMETERS* pClassParam =
                reinterpret_cast<LWA0B5_ALLOCATION_PARAMETERS*>(&objParams[0]);

            pClassParam->version = LWA0B5_ALLOCATION_PARAMETERS_VERSION_1;
            rc = m_Channel->GetGpuResource()->GetGrCopyEngineType(&pClassParam->engineType, m_Channel->GetPbdma(), m_Channel->GetLwRmPtr());
            UtlUtility::PyErrorRC(rc, "Unable to allocate subchannel class 0x%x: failed to get engineType",hwClass);
            m_SubChCEInsts.push_back(MDiagUtils::GetCopyEngineOffset(pClassParam->engineType));
            inst = LWA06F_SUBCHANNEL_COPY_ENGINE; // = 4
        }
        else if (engineName.compare(0, 4, "COPY") == 0)
        {
            objParams.resize(sizeof(LW85B5_ALLOCATION_PARAMETERS));
            LW85B5_ALLOCATION_PARAMETERS* pClassParam =
                reinterpret_cast<LW85B5_ALLOCATION_PARAMETERS*>(&objParams[0]);
            pClassParam->engineInstance = MDiagUtils::GetCopyEngineOffset(m_Channel->GetEngineId());
            m_SubChCEInsts.push_back(pClassParam->engineInstance);

            inst = 0; // any number between 0 ~ 4
        }
        else
        {
            // This case can be entered only pre-Ampere since Ampere onwards
            // each channel has an associated engine and CE subchannel can 
            // only be created with GR and CE channels
            inst = 4;
        }
        params = objParams.size() > 0 ? (void*)(&objParams[0]) : nullptr;
    }
    else if (EngineClasses::IsClassType("Lwenc", hwClass))
    {
        params = LWGpuChannel::GetLwEncObjCreateParam(
            MDiagUtils::GetLwEncEngineOffset(m_Channel->GetEngineId()),
            &lwencAllocParam);
    }
    else if (EngineClasses::IsClassType("Lwdec", hwClass))
    {
        params = LWGpuChannel::GetLwDecObjCreateParam(
            MDiagUtils::GetLwDecEngineOffset(m_Channel->GetEngineId()), 
            &lwdecAllocParam);
    }
    else if (EngineClasses::IsClassType("Lwjpg", hwClass))
    {
        params = LWGpuChannel::GetLwJpgObjCreateParam(
            MDiagUtils::GetLwJpgEngineOffset(m_Channel->GetEngineId()),
            &lwjpgAllocParam);
    }
    rc.Clear();

    if (inst >= Channel::NumSubchannels)
    {
        UtlUtility::PyError("inst is invalid.");
    }

    // Check this inst in this channel whether has been set.
    if (m_SubchannelNames[inst].classNum == hwClass)
    {
        UtlUtility::PyError("This channel %s which subchannel %d has been registered with class 0x%08x.",
            GetName().c_str(), inst, subChClass);
    }

    LwRm::Handle subchannelHandle;

    rc = pLwRm->Alloc(GetHandle(), &subchannelHandle, hwClass, params);
    UtlUtility::PyErrorRC(rc, "Unable to allocate subchannel class 0x%x for channel %u: %s",
        hwClass, m_Channel->ChannelNum(), rc.Message());

    rc = m_Channel->ScheduleChannel(true);
    UtlUtility::PyErrorRC(rc, "Failed to schedule channel %u.",
        m_Channel->ChannelNum());

    if (m_Channel->SetObject(inst, subchannelHandle) == 0)
    {
        UtlUtility::PyError("Can't set subchannel number %u on subchannel with class 0x%x.",
            inst, hwClass);
    }

    m_SubchannelNames[inst].classNum = hwClass;
}

// This function can be called from a UTL script
// to get the name of the channel.
//
const string& UtlChannel::GetName() const
{
    return m_Name;
}

// This function can be called from a UTL script to get the number of test
// methods that will be written to the channel.
//
int UtlChannel::GetMethodCount() const
{
    // Don't include the channel init method count because it doesn't represent
    // test methods (and it's not an accurate value anyway!)
    return m_Channel->GetMethodCount() - m_Channel->GetChannelInitMethodCount();
}

void UtlChannel::SetMethodCount(UINT32 methodCount)
{
    if (m_OwnedLWGpuChannel.get() == nullptr)
    {
        UtlUtility::PyError("Channel.methodCount can only be set for channels created with Channel.Create().");
    }

    return m_Channel->SetMethodCount(methodCount);
}

// This function can be called from a UTL script to write methods
// to this channel.
//
void UtlChannel::WriteMethods(py::kwargs kwargs)
{
    RC rc;

    int subchannel = 0;

    if (!UtlUtility::GetOptionalKwarg<int>(&subchannel, kwargs, "subchannel", "Channel.WriteMethods"))
    {
        // Subchannel is an optional parameter.  If the user doesn't specify
        // a subchannel number, use the most recent subchannel number seen
        // by the semaphore channel wrapper.  If that wrapper doesn't exist,
        // use zero as the default.
        SemaphoreChannelWrapper* chanWrap =
            m_Channel->GetModsChannel()->GetSemaphoreChannelWrapper();
        subchannel = chanWrap ? chanWrap->GetLwrrentSubchannel() : 0;
    }

    // method can be a single integer or a list
    py::object pyMethod;
    vector<UINT32> method;
    pyMethod = UtlUtility::GetRequiredKwarg<py::object>(kwargs, "method", 
        "Channel.WriteMethods");
    UtlUtility::CastPyObjectToListOrSingleElement(pyMethod, &method); 
    
    if (method.empty())
    {
        UtlUtility::PyError("No method to write in Channel.WriteMethods");
    }

    // data can be a single integer or a list
    py::object pyData;
    vector<UINT32> data;
    pyData = UtlUtility::GetRequiredKwarg<py::object>(kwargs, "data", 
        "Channel.WriteMethods");
    UtlUtility::CastPyObjectToListOrSingleElement(pyData, &data);
    
    if (data.empty())
    {
        UtlUtility::PyError("No data to write in Channel.WriteMethods");
    }

    UtlUserTest* userTest = nullptr;
    py::object userTestObj;
    if (UtlUtility::GetOptionalKwarg<py::object>(&userTestObj, kwargs, "userTest", "Channel.WriteMethods"))
    {
        userTest = UtlUserTest::GetTestFromPythonObject(userTestObj);

        if (userTest == nullptr)
        {
            UtlUtility::PyError("invalid userTest argument.");
        }
    }

    MethodType mode = INCREMENTING;
    UtlUtility::GetOptionalKwarg<MethodType>(&mode, kwargs, "mode", "Channel.WriteMethods");
    if (mode == INCREMENTING_IMM && 
        data.size() != method.size())
    {
        UtlUtility::PyError("WriteMethods called with INCREMENTING_IMM but number of methods != number of datas");
    }

    UtlGil::Release gil;

    // Don't treat these methods as inserted if they come from a user test.
    if (userTest == nullptr)
    {
        rc = m_Channel->GetModsChannel()->BeginInsertedMethods();
        UtlUtility::PyErrorRC(rc, "BeginInsertedMethods call failed");
    }

    UINT32 failedMethod = 0;
    if (method.size() > 1)
    {
        if (method.size() != data.size())
        {
            UtlUtility::PyError("Multiple methods passed, but number of methods != number of datas");
        }

        for (UINT32 idx = 0; idx < method.size(); idx++)
        {
            DebugPrintf("UTL: Writing method 0x%04x with data 0x%08x to channel %s (subchannel %d)\n", 
                method[idx], data[idx], m_Name.c_str(), subchannel);
            rc = m_Channel->MethodWriteN_RC(subchannel, method[idx], 1, &data[idx], mode);

            if (OK != rc)
            {
                failedMethod = method[idx];
                break;
            }
        }
    }
    else
    {
        UINT32 classNum = ~0U;
        if (OK != m_Channel->GetSubChannelClass(subchannel, &classNum))
        {
            UtlUtility::PyErrorRC(rc, "Could not get class for subchannel %d", subchannel);
        }

        if (EngineClasses::IsSemaphoreMethod(method[0], classNum, m_Channel->GetChannelClass()) && data.size() == 1)
        {
            // Error out if the user has specified a single semaphore method/data
            // This could lead to interleaving with other Trace or MODS inserted semaphore methods
            ErrPrintf("Single semaphore method/data is not allowed for UtlChannel.WriteMethods.\n");
            UtlUtility::PyError("Either specify all semaphore methods together in a list or use INCREMENTING mode with one method and multiple datas.\n"); 
        }

        DebugPrintf("UTL: Writing method 0x%04x with data beginning with 0x%08x to channel %s (subchannel %d)\n", 
            method[0], data[0], m_Name.c_str(), subchannel);
        rc = m_Channel->MethodWriteN_RC(subchannel, method[0], data.size(), &data[0], mode);
        failedMethod = method[0];
    }

    if (OK != rc)
    {
        if (userTest == nullptr)
        {
            m_Channel->GetModsChannel()->CancelInsertedMethods();
        }
        UtlUtility::PyErrorRC(rc, "could not write method 0x%04x with data beginning with 0x%08x to channel %s",
            failedMethod, data[0], m_Name.c_str());
    }

    if (userTest == nullptr)
    {
        rc = m_Channel->GetModsChannel()->EndInsertedMethods();
        UtlUtility::PyErrorRC(rc, "EndInsertedMethods call failed");
    }
}

// This function can be called from a UTL script to flush all methods that
// have been written to the channel so far.
//
void UtlChannel::Flush()
{
    RC rc;

    UtlGil::Release gil;

    rc = m_Channel->MethodFlushRC();
    UtlUtility::PyErrorRC(rc, "Flush call failed");
}

// This function can be called from a UTL script to wait for the channel
// to execute all methods that have been written to it.
//
void UtlChannel::WaitForIdle()
{
    RC rc;

    UtlGil::Release gil;

    rc = m_Channel->WaitForIdleRC();
    UtlUtility::PyErrorRC(rc, "WaitForIdle call failed");
}

// Find the UtlChannel object that wraps the specfied LWGpuChannel pointer.
//
UtlChannel* UtlChannel::GetFromChannelPtr(LWGpuChannel* channel)
{
    if (s_Channels.count(channel) > 0)
    {
        return s_Channels[channel].get();
    }

    return nullptr;
}

void UtlChannel::Reset()
{
    RC rc;
    
    MASSERT(m_Channel);

    UtlGil::Release gil;

    Channel * pCh = m_Channel->GetModsChannel();
    MASSERT(pCh);
    rc = pCh->RmcResetChannel(GetEngineId());

    UtlUtility::PyErrorRC(rc, "Can't reset channel/engine successfully,");
}

UtlEngine* UtlChannel::GetEngine()
{
    GpuDevice* pGpuDev = GetGpuDevice();
    if (!(pGpuDev->GetSubdevice(0)->HasFeature(Device::GPUSUB_HOST_REQUIRES_ENGINE_ID)))
    {
        UtlUtility::PyError("UtlChannel.GetEngine is only supported Ampere onwards.");
    }

    MASSERT(m_pUtlEngine);
    return m_pUtlEngine;
}

UtlMmu::APERTURE UtlChannel::GetGmmuAperture(UINT32 aperture)
{
    switch (aperture)
    {
        case LW2080_CTRL_CMD_FIFO_GET_CHANNEL_MEM_APERTURE_ILWALID:
            return UtlMmu::APERTURE::APERTURE_ILWALID;
        case LW2080_CTRL_CMD_FIFO_GET_CHANNEL_MEM_APERTURE_VIDMEM:
            return UtlMmu::APERTURE::APERTURE_FB;
        case LW2080_CTRL_CMD_FIFO_GET_CHANNEL_MEM_APERTURE_SYSMEM_COH:
            return UtlMmu::APERTURE::APERTURE_COHERENT;
        case LW2080_CTRL_CMD_FIFO_GET_CHANNEL_MEM_APERTURE_SYSMEM_NCOH:
            return UtlMmu::APERTURE::APERTURE_NONCOHERENT;
        default:
            MASSERT(!"UtlChannel.GetGmmuAperture: Aperture is unsupported/ not recognized");
            return UtlMmu::APERTURE::APERTURE_ILWALID;
    }
}

UtlRawMemory* UtlChannel::GetInstanceBlock()
{
    UtlGil::Release gil;

    if (m_pInstanceBlock.get() ==  nullptr)
    {
        unique_ptr<UtlRawMemory> instBlock = make_unique<UtlRawMemory>(GetLwRmPtr(), GetGpuDevice());
        
        LW2080_CTRL_CMD_FIFO_GET_CHANNEL_MEM_INFO_PARAMS params = {};
        params.hChannel = GetHandle();

        RC rc = GetLwRmPtr()->ControlBySubdevice(
            GetGpuDevice()->GetSubdevice(0), LW2080_CTRL_CMD_FIFO_GET_CHANNEL_MEM_INFO,
            &params, sizeof(params));
        UtlUtility::PyErrorRC(rc, "Fail to get instance block.");
        UtlMmu::APERTURE instBlockAperture = GetGmmuAperture(params.chMemInfo.inst.aperture);
        
        instBlock->SetPhysAddress(params.chMemInfo.inst.base);
        instBlock->SetAperture(instBlockAperture);

        rc = instBlock->Map();
        UtlUtility::PyErrorRC(rc, "Fail to get instance block.");

        m_pInstanceBlock = move(instBlock);
    }

    return m_pInstanceBlock.get();
}

vector<UINT32> UtlChannel::GetSubchannelCEInstances()
{
    GpuDevice* pGpuDev = GetGpuDevice();
    if (!(pGpuDev->GetSubdevice(0)->HasFeature(Device::GPUSUB_HOST_REQUIRES_ENGINE_ID)))
    {
        UtlUtility::PyError("UtlChannel.GetSubchannelCEInstances is only supported Ampere onwards.");
    }
    return m_SubChCEInsts;
}

void UtlChannel::PrintChannelInfo()
{
    m_ChannelAllocInfo.AddChannel(m_Channel);
    m_ChannelAllocInfo.Print();
}

GpuDevice* UtlChannel::GetGpuDevice()
{
    GpuDevice* pGpuDev = m_Channel->GetGpuResource()->GetGpuDevice();
    MASSERT(pGpuDev);
    return pGpuDev;
}

UINT32 UtlChannel::GetChannelId()
{
    return m_Channel->ChannelNum();
}

UINT32 UtlChannel::GetPhysicalChannelId()
{
    UtlGil::Release gil;

    return MDiagUtils::GetPhysChannelId(
        m_Channel->ChannelNum(), GetLwRmPtr(), GetEngineId());
}

UINT32 UtlChannel::GetLogicalCopyEngineId()
{
    UtlGil::Release gil;

    UINT32 engineId = m_Channel->GetEngineId();

    if (MDiagUtils::IsCopyEngine(engineId))
    {
        return MDiagUtils::GetCopyEngineOffset(engineId);
    }

    return LW2080_ENGINE_TYPE_ALLENGINES;
}

UtlSmcEngine* UtlChannel::GetSmcEngine()
{
    return UtlSmcEngine::GetFromSmcEnginePtr(m_Channel->GetSmcEngine());
}

UtlGpu* UtlChannel::GetGpu()
{
    return UtlGpu::GetFromGpuSubdevicePtr(GetGpuDevice()->GetSubdevice(0));
}

UINT32 UtlChannel::GetClass()
{
    UtlGil::NoYield gil;
    return m_Channel->GetModsChannel()->GetClass();
}

ChannelType UtlChannel::GetChannelType()
{
    UtlGil::NoYield gil;
    UINT32 pGpPutPtr;
    if (m_Channel->GetGpPut(&pGpPutPtr) == OK)
        return GPFIFO;
    else
        return PIO;
}

void UtlChannel::WriteSetSubdevice(py::kwargs kwargs)
{
    UINT32 subdevMask = UtlUtility::GetRequiredKwarg<UINT32>(kwargs, "subdevMask", "Channel.WriteSetSubdevice");

    UtlGil::Release gil;
    RC rc = m_Channel->WriteSetSubdevice(subdevMask);
    UtlUtility::PyErrorRC(rc, "WriteSetSubdevice failed.");
}

UINT32 UtlChannel::GetDoorbellToken()
{
    UtlGil::Release gil;

    LWC36F_CTRL_CMD_GPFIFO_GET_WORK_SUBMIT_TOKEN_PARAMS params = {};
    RC rc = GetLwRmPtr()->Control(GetHandle(), 
                LWC36F_CTRL_CMD_GPFIFO_GET_WORK_SUBMIT_TOKEN,
                &params,
                sizeof(params));

    UtlUtility::PyErrorRC(rc, "RM ctrl call to get Doorbell token failed.");
    
    return params.workSubmitToken;
}

bool UtlChannel::GetDoorbellRingEnable()
{
    return m_Channel->GetModsChannel()->GetDoorbellRingEnable();
}

void UtlChannel::SetDoorbellRingEnable(bool enable)
{
    if (m_Channel->GetModsChannel()->GetAutoFlushEnable() && !enable)
    {
        UtlUtility::PyError(
            "Disabling doorbell ring with autoflush enabled will interfere with MODS work submission process. Please use -single_kick if the UTL script disables doorbell ring.");
    }

    RC rc = m_Channel->GetModsChannel()->SetDoorbellRingEnable(enable);
    UtlUtility::PyErrorRC(rc, "%s doorbell ring failed\n", 
        enable ? "Enabling" : "Disabling");
}

UtlChannel* UtlChannel::GetChannelFromHandle(LwRm::Handle hObject)
{
    for (const auto& keyValue : s_Channels)
    {
        if (keyValue.second->GetHandle() == hObject)
        {
            return keyValue.second.get();
        }
    }
    return nullptr;
}

// Find the first UTL channel that matches the specified engined ID.
//
UtlChannel* UtlChannel::FindFirstEngineChannel(UINT32 engineId)
{
    for (const auto& keyValue : s_Channels)
    {
        if (keyValue.second->GetEngineId() == engineId)
        {
            return keyValue.second.get();
        }
    }
    return nullptr;
}

void UtlChannel::CheckForErrors()
{
    UtlGil::Release gil;
    RC rc = m_Channel->CheckForErrorsRC();
    UtlUtility::PyErrorRC(rc, "Channel.CheckForErrors found an error.");
}

void UtlChannel::ClearError()
{
    UtlGil::Release gil;
    m_Channel->GetModsChannel()->ClearError();
}

UtlVaSpace* UtlChannel::GetVaSpace(py::kwargs kwargs)
{
    UtlTest* pTest = nullptr;
    UtlUtility::GetOptionalKwarg<UtlTest*>(&pTest, kwargs, "test", "Channel.GetVaSpace");

    shared_ptr<VaSpace> pVaSpace = m_Channel->GetVASpace();
    return UtlVaSpace::GetVaSpace(pVaSpace->GetHandle(), pTest);
}
