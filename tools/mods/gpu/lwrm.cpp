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

// LW resource manager architecture interface.

#include "core/include/lwrm.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/display.h"
#include "core/include/gpu.h"
#include "core/include/channel.h"
#include "gpu/include/android_channel.h"
#include "core/include/globals.h"
#include "core/include/xp.h"
#include "core/include/massert.h"
#include "lwRmApi.h"
#include "Lwcm.h"
#include "lwmisc.h"
#include "core/include/platform.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/dispchan.h"
#include "core/include/cpu.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/gpumgr.h"
#include "core/include/mgrmgr.h"
#include "core/include/deprecat.h"
#include "gpu/js_gpudv.h"
#include "core/include/evntthrd.h"
#include "gpu/utility/chanwmgr.h"
#include "gpu/utility/tsg.h"
#include "core/include/simclk.h"
#include "rm.h"
#include "core/include/utility.h"
#include <memory>
#include <algorithm>
#include "cheetah/include/tegrasocdevice.h"
#include "cheetah/include/tegrasocdeviceptr.h"

#if defined(LINUX_MFG)
extern "C" void Lw04ControlInternal     (LWOS54_PARAMETERS*);
extern "C" void Lw04MapMemoryInternal   (LWOS33_PARAMETERS*);
extern "C" void Lw04UnmapMemoryInternal (LWOS34_PARAMETERS*);
#endif

#if defined(TEGRA_MODS)
#include "rmapi_tegra.h"
#include "dispLwRmApi.h"
#endif

#include "class/cl0000.h" // LW01_NULL_OBJECT
#include "class/cl0002.h" // LW01_CONTEXT_DMA
#include "class/cl003e.h" // LW01_MEMORY_SYSTEM
#include "class/cl003f.h" // LW01_MEMORY_LOCAL_PRIVILEGED
#include "class/cl0040.h" // LW01_MEMORY_LOCAL_USER
#include "class/cl0070.h" // LW01_MEMORY_SYSTEM_DYNAMIC
#include "class/cl0080.h" // LW01_DEVICE_0
#include "class/cl2080.h" // LW20_SUBDEVICE_0
#include "class/cl506f.h" // LW50_CHANNEL_GPFIFO
#include "class/cl507a.h" // LW50_LWRSOR_CHANNEL_PIO
#include "class/cl507c.h" // LW50_BASE_CHANNEL_DMA
#include "class/cl906f.h" // GF100_CHANNEL_GPFIFO
#include "class/cla06f.h" // KEPLER_CHANNEL_GPFIFO_A
#include "class/cla16f.h" // KEPLER_CHANNEL_GPFIFO_B
#include "class/cla26f.h" // KEPLER_CHANNEL_GPFIFO_C
#include "class/clb06f.h" // MAXWELL_CHANNEL_GPFIFO_A
#include "class/clc06f.h" // PASCAL_CHANNEL_GPFIFO_A
#include "class/clc36f.h" // VOLTA_CHANNEL_GPFIFO_A
#include "class/clc46f.h" // TURING_CHANNEL_GPFIFO_A
#include "class/clc56f.h" // AMPERE_CHANNEL_GPFIFO_A
#include "class/clc86f.h" // HOPPER_CHANNEL_GPFIFO_A
#include "ctrl/ctrl0000.h" // LW0000_CTRL_CMD_SYSTEM_GET_BUILD_VERSION
#include "ctrl/ctrl0000/ctrl0000gpu.h"
#include "ctrl/ctrl0080.h"
#include "ctrl/ctrl2080.h"
#include "class/clc37a.h" // LWC37A_LWRSOR_IMM_CHANNEL_PIO
#include "class/clc37b.h" // LWC37B_WINDOW_IMM_CHANNEL_DMA
#include "class/clc37d.h" // LWC37D_CORE_CHANNEL_DMA
#include "class/clc37e.h" // LWC37E_WINDOW_CHANNEL_DMA
#include "class/clc57b.h" // LWC57B_WINDOW_IMM_CHANNEL_DMA
#include "class/clc57d.h" // LWC57D_CORE_CHANNEL_DMA
#include "class/clc57e.h" // LWC57E_WINDOW_CHANNEL_DMA
#include "class/clc67b.h" // LWC67B_WINDOW_IMM_CHANNEL_DMA
#include "class/clc67d.h" // LWC67D_CORE_CHANNEL_DMA
#include "class/clc77d.h" // LWC77D_CORE_CHANNEL_DMA
#include "class/clc67e.h" // LWC67E_WINDOW_CHANNEL_DMA
#include "class/clc87b.h" // LWC87B_WINDOW_IMM_CHANNEL_DMA
#include "class/clc87d.h" // LWC87D_CORE_CHANNEL_DMA
#include "class/clc87e.h" // LWC87E_WINDOW_CHANNEL_DMA
#include "class/cl00f3.h" // LW01_MEMORY_FLA


#include "gpu/include/userdalloc.h"

// Fixed video heap owner ID used for all allocations
static constexpr UINT32 s_VidHeapOwnerID = (('M'<< 24) | ('O'<<16) | ('D'<<8) | 'S');

// LwRm ClientId counter
static UINT32 s_LwrClientId = 0;

// LwRm clients
LwRm LwRmPtr::s_LwRmClients[LwRmPtr::s_MaxClients];

map<UINT32,int> LwRm::DisableRcRecovery::s_DisableCount;
map<UINT32,int> LwRm::DisableRcWatchdog::s_DisableCount;

// Number of valid clients
UINT32 LwRmPtr::s_ValidClientCount = 1;

UINT32 LwRmPtr::s_FreeClientCount = 0;
#define PROFILE(x) SimClk::InstWrapper wrapper(SimClk::InstGroup::x);

/**
 *------------------------------------------------------------------------------
 * @function(LwRm::LwRm)
 *
 * ctor
 *------------------------------------------------------------------------------
 */

LwRm::LwRm() :
    m_ClientId(0),
    m_Client(0),
    m_Channels(),
    m_DisplayChannels(),
    m_Mutex(NULL),
    m_bEnableControlTrace(false),
    m_UseInternalCall(false)
{
    m_ClientId = s_LwrClientId++;
}

/**
 *------------------------------------------------------------------------------
 * @function(LwRm::LwRm)
 *
 * dtor
 *------------------------------------------------------------------------------
 */

LwRm::~LwRm()
{
    if (Platform::IsForcedTermination())
        return;

    MASSERT(m_Mutex == 0);
}

/**
 *------------------------------------------------------------------------------
 * @function(LwRm::GetClientHandle)
 *
 * Get the client handle.
 *------------------------------------------------------------------------------
 */

LwRm::Handle LwRm::GetClientHandle() const
{
   return m_Client;
}

/**
 *------------------------------------------------------------------------------
 * @function(LwRm::GetDeviceHandle)
 *
 * Get the device handle.
 *------------------------------------------------------------------------------
 */
LwRm::Handle LwRm::GetDeviceHandle(const GpuDevice *gpudev) const
{
    return DevHandle(gpudev->GetDeviceInst());
}

LwRm::Handle LwRm::GetDeviceHandle(UINT32 inst) const
{
    return DevHandle(inst);
}

/**
 *------------------------------------------------------------------------------
 * @function(LwRm::GetSubdeviceHandle)
 *
 * Get the subdevice handle.
 *------------------------------------------------------------------------------
 */
LwRm::Handle LwRm::GetSubdeviceHandle(const GpuSubdevice *gpusubdev) const
{
    MASSERT(gpusubdev);

    if (gpusubdev->GetParentDevice() == NULL)
    {
        // Fake the inst numbers if we skipped that part of the
        // init sequence (e.g. by setting Gpu.SkipRmStateInit).
        //
        return SubdevHandle(0, gpusubdev->GetSubdeviceInst() + 1);
    }
    return SubdevHandle(gpusubdev->GetParentDevice()->GetDeviceInst(),
                        gpusubdev->GetSubdeviceInst() + 1);
}

/**
 *------------------------------------------------------------------------------
 * @function(LwRm::GetHandleForCfgGetSet()
 *
 * Get the device handle or subdevice handle depending on what is appropriate
 * for a config get/set operation.
 *------------------------------------------------------------------------------
 */

LwRm::Handle LwRm::GetHandleForCfgGetSet
(
    UINT32 Index,
    void * pParamStruct,
    bool   isSet,
    const GpuSubdevice *gpusubdev
) const
{
    MASSERT(gpusubdev);
    GpuDevice *gpudev = gpusubdev->GetParentDevice();
    LwRm::Handle hDev = GetDeviceHandle(gpudev);

    // If not in SLI mode, always use the device handle
    if (gpudev->GetNumSubdevices() < 2)
        return hDev;

    // Otherwise, we will usually use the subdevice handle, so as to operate
    // on just one of the 2+ GPUs.
    LwRm::Handle hSub = GetSubdeviceHandle(gpusubdev);

    // Some config calls require the device handle even in SLI mode.
    switch (Index)
    {
        case LW_CFGEX_EVENT_HANDLE:
        {
            LW_CFGEX_EVENT_HANDLE_PARAMS * p = (LW_CFGEX_EVENT_HANDLE_PARAMS *) pParamStruct;
            if (p->EventOrdinal == LW_CFGEX_EVENT_HANDLE_POWER_CONNECTOR_EVENT)
                return hDev;
            else
                return hSub;
        }

        case LW_CFGEX_PERF_MODE:
            if (isSet)
                return hDev;
            else
                return hSub;

        default:
            return hSub;
    }
}

/**
 *------------------------------------------------------------------------------
 * @function(LwRm::GetChannel)
 *
 * Get the channel.
 *------------------------------------------------------------------------------
 */

Channel * LwRm::GetChannel
(
   Handle ChannelHandle
)
{
    // This code is here temporarily due to shortcomings of the DOS Tasker.
    // The problem:
    //  * An IRQ oclwrs due to an exception/channel error
    //  * RC channel recovery calls a MODS callback
    //  * MODS callback calls this function (LwRm::Channel)
    //  * We try to lock the mutex
    //  * But the mutex is locked by another thread
    //  * So Tasker::AcquireMutex() tries to call Tasker::Yield()
    //  * But Tasker::Yield() asserts if called from ISR!
    // Once we migrate sim MODS to the new multithreaded Tasker we
    // can restore MutexHolder here and unconditionally lock the mutex.
    bool mutexAcquired = true;
    if (!Tasker::CanThreadYield() &&
        (Xp::OS_LINUXSIM == Xp::GetOperatingSystem()))
    {
        // On sim MODS use a temporary solution
        mutexAcquired = Tasker::TryAcquireMutex(m_Mutex);
        if (!mutexAcquired)
        {
            Printf(Tee::PriNormal, "Warning: LwRm::GetChannel() called from ISR and unable to acquire LwRm mutex!\n");
        }
    }
    else
    {
        Tasker::AcquireMutex(m_Mutex);
    }

    Channel* pChannel = 0;
    if (m_Channels.find(ChannelHandle) != m_Channels.end())
    {
        pChannel = m_Channels[ChannelHandle];
    }

    if (mutexAcquired)
    {
        Tasker::ReleaseMutex(m_Mutex);
    }

    return pChannel;
}

/**
 *------------------------------------------------------------------------------
 * @function(LwRm::GetDisplayChannel)
 *
 * Get the display channel.
 *------------------------------------------------------------------------------
 */

DisplayChannel * LwRm::GetDisplayChannel
(
   Handle ChannelHandle
)
{
    Tasker::MutexHolder mh(m_Mutex);
    if (m_DisplayChannels.find(ChannelHandle) != m_DisplayChannels.end())
        return m_DisplayChannels[ChannelHandle];

   return 0;
}

//------------------------------------------------------------------------------
RC RmApiStatusToModsRC(UINT32 Status)
{
    switch (Status)
    {
        case LW_OK:
            return OK;
        case LW_ERR_CARD_NOT_PRESENT:
            return RC::LWRM_CARD_NOT_PRESENT;
        case LW_ERR_DUAL_LINK_INUSE:
            return RC::LWRM_DUAL_LINK_INUSE;
        case LW_ERR_NO_SUCH_DOMAIN:
        case LW_ERR_GENERIC:
            return RC::LWRM_ERROR;
        case LW_ERR_GPU_NOT_FULL_POWER:
            return RC::LWRM_GPU_NOT_FULL_POWER;
        case LW_ERR_STATE_IN_USE:
            return RC::LWRM_IN_USE;
        case LW_ERR_NO_FREE_FIFOS:
        case LW_ERR_NO_MEMORY:
        case LW_ERR_INSUFFICIENT_RESOURCES:
            return RC::LWRM_INSUFFICIENT_RESOURCES;
        case LW_ERR_ILWALID_ACCESS_TYPE:
            return RC::LWRM_ILWALID_ACCESS_TYPE;
        case LW_ERR_ILWALID_ARGUMENT:
            return RC::LWRM_ILWALID_ARGUMENT;
        case LW_ERR_ILWALID_BASE:
            return RC::LWRM_ILWALID_BASE;
        case LW_ERR_ILWALID_CHANNEL:
            return RC::LWRM_ILWALID_CHANNEL;
        case LW_ERR_ILWALID_CLASS:
            return RC::LWRM_ILWALID_CLASS;
        case LW_ERR_ILWALID_CLIENT:
        case LW_ERR_LIB_RM_VERSION_MISMATCH:
            return RC::LWRM_ILWALID_CLIENT;
        case LW_ERR_ILWALID_COMMAND:
            return RC::LWRM_ILWALID_COMMAND;
        case LW_WARN_INCORRECT_PERFMON_DATA:
        case LW_ERR_ILWALID_DATA:
            return RC::LWRM_ILWALID_DATA;
        case LW_ERR_ILWALID_DEVICE:
            return RC::LWRM_ILWALID_DEVICE;
        case LW_ERR_ILWALID_DMA_SPECIFIER:
            return RC::LWRM_ILWALID_DMA_SPECIFIER;
        case LW_ERR_ILWALID_EVENT:
            return RC::LWRM_ILWALID_EVENT;
        case LW_ERR_ILWALID_FLAGS:
            return RC::LWRM_ILWALID_FLAGS;
        case LW_ERR_ILWALID_FUNCTION:
            return RC::LWRM_ILWALID_FUNCTION;
        case LW_ERR_ILWALID_HEAP:
            return RC::LWRM_ILWALID_HEAP;
        case LW_ERR_ILWALID_INDEX:
            return RC::LWRM_ILWALID_INDEX;
        case LW_ERR_ILWALID_LIMIT:
            return RC::LWRM_ILWALID_LIMIT;
        case LW_ERR_ILWALID_METHOD:
            return RC::LWRM_ILWALID_METHOD;
        case LW_ERR_ILWALID_OBJECT_BUFFER:
            return RC::LWRM_ILWALID_OBJECT_BUFFER;
        case LW_ERR_OBJECT_TYPE_MISMATCH:
        case LW_ERR_ILWALID_OBJECT:
            return RC::LWRM_ILWALID_OBJECT_ERROR;
        case LW_ERR_ILWALID_OBJECT_HANDLE:
            return RC::LWRM_ILWALID_OBJECT_ERROR;
        case LW_ERR_ILWALID_OBJECT_NEW:
            return RC::LWRM_ILWALID_OBJECT_NEW;
        case LW_ERR_ILWALID_OBJECT_OLD:
            return RC::LWRM_ILWALID_OBJECT_OLD;
        case LW_ERR_ILWALID_OBJECT_PARENT:
            return RC::LWRM_ILWALID_OBJECT_PARENT;
        case LW_ERR_ILWALID_OFFSET:
            return RC::LWRM_ILWALID_OFFSET;
        case LW_ERR_ILWALID_OWNER:
            return RC::LWRM_ILWALID_OWNER;
        case LW_ERR_ILWALID_PARAM_STRUCT:
            return RC::LWRM_ILWALID_PARAM_STRUCT;
        case LW_ERR_ILWALID_PARAMETER:
            return RC::LWRM_ILWALID_PARAMETER;
        case LW_ERR_ILWALID_POINTER:
            return RC::LWRM_ILWALID_POINTER;
        case LW_ERR_ILWALID_REGISTRY_KEY:
            return RC::LWRM_ILWALID_REGISTRY_KEY;
        case LW_ERR_ILWALID_STATE:
        case LW_ERR_ILWALID_LOCK_STATE:
            return RC::LWRM_ILWALID_STATE;
        case LW_ERR_ILWALID_STRING_LENGTH:
            return RC::LWRM_ILWALID_STRING_LENGTH;
        case LW_ERR_IRQ_NOT_FIRING:
            return RC::LWRM_IRQ_NOT_FIRING;
        case LW_ERR_MULTIPLE_MEMORY_TYPES:
            return RC::LWRM_MULTIPLE_MEMORY_TYPES;
        case LW_ERR_NOT_SUPPORTED:
            return RC::LWRM_NOT_SUPPORTED;
        case LW_ERR_ILWALID_READ:
        case LW_ERR_ILWALID_WRITE:
        case LW_ERR_OPERATING_SYSTEM:
            return RC::LWRM_OPERATING_SYSTEM;
        case LW_ERR_PROTECTION_FAULT:
            return RC::LWRM_PROTECTION_FAULT;
        case LW_ERR_TIMEOUT:
        case LW_ERR_TIMEOUT_RETRY:
            return RC::LWRM_TIMEOUT;
        case LW_ERR_TOO_MANY_PRIMARIES:
            return RC::LWRM_TOO_MANY_PRIMARIES;
        case LW_WARN_MORE_PROCESSING_REQUIRED:
            return RC::LWRM_MORE_PROCESSING_REQUIRED;
        case LW_ERR_NOT_READY:
            return RC::LWRM_STATUS_ERROR_NOT_READY;
        case LW_ERR_ILWALID_ADDRESS:
            return RC::LWRM_STATUS_ERROR_ILWALID_ADDRESS;
        case LW_ERR_MEMORY_TRAINING_FAILED:
            return RC::LWRM_FB_TRAINING_FAILED;
        case LW_ERR_OBJECT_NOT_FOUND:
            return RC::LWRM_OBJECT_NOT_FOUND;
        case LW_ERR_BUFFER_TOO_SMALL:
            return RC::LWRM_BUFFER_TOO_SMALL;
        case LW_ERR_RESET_REQUIRED:
            return RC::LWRM_RESET_REQUIRED;
        case LW_ERR_ILLEGAL_ACTION:
        case LW_ERR_I2C_ERROR:
        case LW_ERR_ILWALID_OPERATION:
        case LW_ERR_ILWALID_REQUEST:
            return RC::LWRM_ILWALID_REQUEST;
        case LW_ERR_INSUFFICIENT_PERMISSIONS:
            return RC::LWRM_INSUFFICIENT_PERMISSIONS;
        case LW_ERR_GPU_IS_LOST:
            return RC::LWRM_GPU_IS_LOST;
        case LW_ERR_FLCN_ERROR:
            return RC::LWRM_FLCN_ERROR;
        case LW_ERR_RISCV_ERROR:
            return RC::LWRM_RISCV_ERROR;
        case LW_ERR_FATAL_ERROR:
            return RC::LWRM_FATAL_ERROR;
        case LW_ERR_MODULE_LOAD_FAILED:
            return RC::WAS_NOT_INITIALIZED;
        case LW_ERR_MEMORY_ERROR:
            return RC::LWRM_MEMORY_ERROR;
        case LW_ERR_REJECTED_VBIOS:
            return RC::LWRM_ILWALID_VBIOS;
        case LW_ERR_ILWALID_LICENSE:
            return RC::ILWALID_HULK_LICENSE;
        case LW_ERR_LWLINK_INIT_ERROR:
            return RC::LWLINK_INIT_ERROR;
        case LW_ERR_LWLINK_MINION_ERROR:
            return RC::LWLINK_MINION_ERROR;
        case LW_ERR_LWLINK_CLOCK_ERROR:
            return RC::LWLINK_CLOCK_ERROR;
        case LW_ERR_LWLINK_TRAINING_ERROR:
            return RC::LWLINK_TRAINING_ERROR;
        case LW_ERR_LWLINK_CONFIGURATION_ERROR:
            return RC::LWLINK_CONFIGURATION_ERROR;
        case LW_WARN_NOTHING_TO_DO:
            return RC::LWRM_NOTHING_TO_DO;
        default:
            Printf(Tee::PriError,
                "Could not translate RM error code 0x%X to a MODS error code\n", Status);
            return RC::LWRM_ERROR;
    }
}

/**
 *------------------------------------------------------------------------------
 * @function(LwRm::AllocRoot)
 *
 * Allocate the client.
 *------------------------------------------------------------------------------
 */

RC LwRm::AllocRoot()
{
    m_Mutex = Tasker::AllocMutex("LwRm::m_Mutex", Tasker::mtxUnchecked);

    LwU32 ClientHandle = 0U;
    UINT32 retval;

    {
        PROFILE(RM_ALLOC);
        retval = LwRmAllocRoot(&ClientHandle);
    }

    if (retval != LW_OK)
    {
        return RmApiStatusToModsRC(retval);
    }
    m_Client = ClientHandle;

    m_Handles.RegisterForeignHandle(ClientHandle, LW01_NULL_OBJECT);

    return OK;
}

RC LwRm::PrepareAllocParams
(
    Handle clientHandle,
    UINT32 deviceReference,
    LW0080_ALLOC_PARAMETERS *pAllocParams
) const
{
    if (pAllocParams == nullptr)
        return RC::SOFTWARE_ERROR;

    if (deviceReference >= LW0080_MAX_DEVICES)
    {
        MASSERT(!"Requested device reference is too high");
        return RC::SOFTWARE_ERROR;
    }

    pAllocParams->deviceId = (deviceReference);
    pAllocParams->hClientShare = clientHandle;
    pAllocParams->hTargetDevice = GetDeviceHandle(pAllocParams->deviceId);
    pAllocParams->hTargetClient = pAllocParams->hClientShare;
    pAllocParams->flags = LW_DEVICE_ALLOCATION_FLAGS_VASPACE_IS_TARGET |
        LW_DEVICE_ALLOCATION_FLAGS_MAP_PTE;

    return OK;
}

/**
 *------------------------------------------------------------------------------
 * @function(LwRm::AllocDevice)
 *
 * Allocate a device.
 *------------------------------------------------------------------------------
 */

RC LwRm::AllocDevice
(
   UINT32 DeviceReference
)
{
   RC rc;
   LW0080_ALLOC_PARAMETERS lw0080Params = {0};

   CHECK_RC(PrepareAllocParams(GetClientHandle(), DeviceReference, &lw0080Params));
   return AllocDevice(&lw0080Params);
}

RC LwRm::AllocDevice
(
   void * pvAllocParams
)
{
   LW0080_ALLOC_PARAMETERS *pAllocParams = static_cast<LW0080_ALLOC_PARAMETERS *>(pvAllocParams);
   Handle DeviceHandle = DevHandle(pAllocParams->deviceId);

   if (pAllocParams->deviceId >= LW0080_MAX_DEVICES)
   {
       MASSERT(!"Requested device reference is too high");
       return RC::SOFTWARE_ERROR;
   }

   UINT32 retval;

   {
       PROFILE(RM_ALLOC);
       retval = LwRmAlloc(m_Client, m_Client, DeviceHandle,
                          LW01_DEVICE_0, pvAllocParams);
   }

   if (retval != LW_OK)
   {
      return RmApiStatusToModsRC(retval);
   }

   Tasker::MutexHolder mh(m_Mutex);

   m_Handles.RegisterForeignHandle(DeviceHandle, m_Client);

   return OK;
}

//------------------------------------------------------------------------------
// Count how many objects have this object as their [direct] parent
INT32 LwRm::CountChildren(LwRm::Handle Handle)
{
    Tasker::MutexHolder mh(m_Mutex);

    // The handle had better be in the tree
    MASSERT(ValidateClient(Handle));
    MASSERT(m_Handles.GetParent(Handle) != m_Handles.Invalid);

    int Count = 0;
    for (auto h: m_Handles)
    {
        if (Handle == h.parent)
        {
            Printf(Tee::PriDebug, "Found child 0x%08x for parent 0x%08x\n",
                   h.handle, h.parent);
            Count++;
        }
    }

    return Count;
}
//------------------------------------------------------------------------------
bool LwRm::IsFreed() const
{
    return (m_Client == 0);
}
//------------------------------------------------------------------------------
//! \brief Free the client
//!
//! This function (renamed from FreeClientsAndDevices) no longer frees the
//! devices automatically.  It will assert if you haven't freed them
//! explicitly.
RC LwRm::FreeClient()
{
    // At this point, the only RM objects still allocated should be the
    // Client.
    // If this is not true, someone is leaking resources.
    // This isn't fatal, since the RM should free all those resources when
    // the client is deleted, but it is certainly ill-behaved (you may run
    // out of memory if you loop a test, for example).

    if (m_Client)
    {
        int Children;

        Children = CountChildren(m_Client);
        if (Children)
        {
            Printf(Tee::PriError,
                "Resource leak detected: RM client has %d children outstanding\n",
                Children);
#ifdef SIM_BUILD
            MASSERT(Children == 0);
#else
            // This condition can occur when MODS fails somewhere during GPU
            // init sequence and not all code paths exit cleanly.  Ignore if
            // there is one child left to avoid asserting in such case and
            // let MODS exit with the original error.  Sim MODS remains
            // more stringent.
            MASSERT(Children <= 1);
#endif
        }

        // Freeing the client would automatically free the devices also,
        // but we don't want that because it's bad form.  Hence the above
        // assertion that the devices were already free.
        Free(m_Client);
        m_Client = 0;
    }

    Tasker::FreeMutex(m_Mutex);
    m_Mutex = 0;

    // There should be no entries left in the handle tree at this point.
    MASSERT(m_Handles.Empty());

    return OK;
}

/**
 *------------------------------------------------------------------------------
 * @function(LwRm::AllocSubDevice)
 *
 * Allocate a subdevice.
 *------------------------------------------------------------------------------
 */

//! \brief Allocate a subdevice
//!
//! The subdevice reference passed into this function should be 0-indexed,
//! which matches up with the Dynamic SLI subdevice reference.  For handles
//! this needs to get translated to SubDeviceReference+1 so that backwards
//! compatibility isn't broken, and SubDeviceReference 0 is still "broadcast".
RC LwRm::AllocSubDevice
(
    UINT32 DeviceReference,
    UINT32 SubDeviceReference
)
{
    Handle hDev      = DevHandle(DeviceReference);
    Handle hSubdev   = SubdevHandle(DeviceReference, SubDeviceReference+1);

    LW2080_ALLOC_PARAMETERS lw2080Params = {0};

    lw2080Params.subDeviceId = SubDeviceReference;
    UINT32 retval;

    {
        PROFILE(RM_ALLOC);
        retval = LwRmAlloc(m_Client, hDev, hSubdev,
                           LW20_SUBDEVICE_0, &lw2080Params);
    }

    Tasker::MutexHolder mh(m_Mutex);

    m_Handles.RegisterForeignHandle(hSubdev, hDev);

    return RmApiStatusToModsRC(retval);
}

/**
 *------------------------------------------------------------------------------
 * @function(LwRm::Alloc)
 *
 * Allocate an object.
 *------------------------------------------------------------------------------
 */

RC LwRm::Alloc
(
    Handle   ParentHandle,
    Handle * pRtnObjectHandle,
    UINT32   Class,
    void *   Parameters
)
{
    MASSERT(ValidateClient(ParentHandle));

    UINT32 retval = LW_OK;

#if defined(TEGRA_MODS)
    if (IsDisplayClientAllocated() &&
        IsDisplayClass(Class))
    {
        *pRtnObjectHandle = GenUniqueHandle(ParentHandle, Lib::DISP_RMAPI);
        PROFILE(RM_ALLOC);
        retval = DispLwRmAlloc(m_DisplayClient,
                               MapToDispRmHandle(ParentHandle),
                               *pRtnObjectHandle,
                               Class,
                               Parameters);
    }
    else
#endif
    {
        *pRtnObjectHandle = GenUniqueHandle(ParentHandle);
        PROFILE(RM_ALLOC);
        retval = LwRmAlloc(GetClientHandle(),
                           ParentHandle,
                           *pRtnObjectHandle,
                           Class,
                           Parameters);
    }

    if (retval != LW_OK)
    {
        DeleteHandle(*pRtnObjectHandle);
        *pRtnObjectHandle = 0;
        return RmApiStatusToModsRC(retval);
    }

    return OK;
}

/**
 *------------------------------------------------------------------------------
 * @function(LwRm::AllocContextDma)
 *
 * Allocate a DMA context.
 *------------------------------------------------------------------------------
 */
RC LwRm::AllocContextDma
(
    Handle        * pRtnDmaHandle,
    UINT32          Flags,
    UINT32          Memory,
    UINT64          Offset,
    UINT64          Limit
)
{
    MASSERT(Memory    != 0);
    MASSERT(ValidateClient(Memory));

    // Always set the hash-table disable flag
    Flags = FLD_SET_DRF(OS03, _FLAGS, _HASH_TABLE, _DISABLE, Flags);

    // We use the memory object as the "parent" of the context DMA, since
    // freeing a memory object automatically frees the context DMAs created
    // on the memory.
    *pRtnDmaHandle = GenUniqueHandle(Memory);

    UINT32 retval;

    {
        PROFILE(RM_ALLOC);
        retval = LwRmAllocContextDma2(GetClientHandle(),
                                      *pRtnDmaHandle,
                                      LW01_CONTEXT_DMA,
                                      Flags,
                                      Memory,
                                      Offset,
                                      Limit);
    }

    if (retval != LW_OK)
    {
        DeleteHandle(*pRtnDmaHandle);
        *pRtnDmaHandle = 0;
        return RmApiStatusToModsRC(retval);
    }

    return OK;
}

/**
 *------------------------------------------------------------------------------
 * @function(LwRm::AllocChannelGpFifo)
 *
 * Allocate a GPFIFO channel.
 *------------------------------------------------------------------------------
 */
RC LwRm::AllocChannelGpFifo
(
    Handle * pRtnChannelHandle,
    UINT32   Class,
    Handle   ErrorMemHandle,
    void *   pErrorNotifierMemory,
    Handle   DataMemHandle,
    void *   pPushBufferBase,
    UINT64   PushBufferOffset,
    UINT32   PushBufferSize,
    void *   pGpFifoBase,
    UINT64   GpFifoOffset,
    UINT32   GpFifoEntries,
    Handle   ContextSwitchObjectHandle,
    UINT32   verifFlags,
    UINT32   verifFlags2,
    UINT32   flags,
    GpuDevice *gpudev,
    Tsg     *pTsg,
    UINT32   hVASpace,
    UINT32   engineId,
    bool     useBar1Doorbell,
    UserdAllocPtr pUserdAlloc
)
{
    MASSERT(Class             != 0);
    MASSERT(DataMemHandle != 0);
    MASSERT(pRtnChannelHandle != 0);
    MASSERT(ValidateClient(ErrorMemHandle));
    MASSERT(ValidateClient(DataMemHandle));

    MASSERT(gpudev);

    if (useBar1Doorbell && Class < HOPPER_CHANNEL_GPFIFO_A)
    {
        Printf(Tee::PriError, 
            "BAR1 Doorbell is not supported prior to Hopper Channel class\n");
        return RC::SOFTWARE_ERROR;
    }

    if (gpudev->GetSubdevice(0)->HasFeature(Device::GPUSUB_HOST_REQUIRES_ENGINE_ID))
    {
        // Channels will automatically pick up the engine ID from the TSG when allocated on the
        // TSG if a engine type was specified on the channel and a TSG was also specified
        // ensure that the engine IDs match
        if (pTsg && (engineId != LW2080_ENGINE_TYPE_NULL) && (engineId != pTsg->GetEngineId()))
        {
            Printf(Tee::PriError, "%s : TSG engine ID (%u) and channel engine ID (%u) mismatch!\n",
                   __FUNCTION__, pTsg->GetEngineId(), engineId);
            return RC::SOFTWARE_ERROR;
        }

        if (pTsg && (engineId == LW2080_ENGINE_TYPE_NULL))
        {
            engineId = pTsg->GetEngineId();
        }

        if (engineId >= LW2080_ENGINE_TYPE_LAST)
        {
            Printf(Tee::PriError, "%s : Invalid engine ID %u!\n", __FUNCTION__, engineId);
            return RC::SOFTWARE_ERROR;
        }

        if (engineId == LW2080_ENGINE_TYPE_NULL)
        {
            const bool bRequireEngineId = GpuPtr()->GetRequireChannelEngineId();
            Printf(bRequireEngineId ? Tee::PriError : Tee::PriWarn,
                   "%s : Channel allocations require a valid engine ID!\n",
                   __FUNCTION__);
            if (bRequireEngineId)
                return RC::SOFTWARE_ERROR;
        }
    }
    else
    {
        engineId = LW2080_ENGINE_TYPE_NULL;
    }

    Handle hParent = pTsg ? pTsg->GetHandle() : GetDeviceHandle(gpudev);

    *pRtnChannelHandle = GenUniqueHandle(hParent);

    RC rc;

    // Create the channel
    LW_CHANNELGPFIFO_ALLOCATION_PARAMETERS Params = {0};
    Params.hObjectError  = ErrorMemHandle;
    Params.hObjectBuffer = DataMemHandle;
    Params.gpFifoOffset  = GpFifoOffset;
    Params.gpFifoEntries = GpFifoEntries;
    Params.flags         = flags;
    Params.hContextShare = ContextSwitchObjectHandle;
    Params.hVASpace      = pTsg ? 0 : hVASpace;
    Params.engineType    = engineId;

#ifdef LW_VERIF_FEATURES
    Params.verifFlags    = verifFlags;
    Params.verifFlags2   = verifFlags2;
#endif

    bool cleanupUserdAtExit = pUserdAlloc == nullptr;

    if (!pUserdAlloc)
    {
        CHECK_RC(gpudev->GetUserdManager()->Get(&pUserdAlloc, Class, this));
    }

    DEFER
    {
        if (cleanupUserdAtExit && pUserdAlloc)
        {
            pUserdAlloc->Free();
        }
    };

    for (UINT32 sub = 0; sub < gpudev->GetNumSubdevices(); sub++)
    {
        Params.hUserdMemory[sub] = pUserdAlloc->GetMemHandle();
        Params.userdOffset[sub]  = pUserdAlloc->GetMemOffset(sub);
    }

    UINT32 retval;
    {
        PROFILE(RM_ALLOC);
        retval = LwRmAlloc(GetClientHandle(),
                           hParent,
                           *pRtnChannelHandle,
                           Class,
                           &Params);
    }

    if (retval != LW_OK)
    {
        DeleteHandle(*pRtnChannelHandle);
        *pRtnChannelHandle = 0;
        return RmApiStatusToModsRC(retval);
    }

    CHECK_RC(pUserdAlloc->Setup(*pRtnChannelHandle));

    // Get the ChID
    UINT32 ChID;
    bool cleanupGpFifoChannel = true;
    DEFER
    {
        if (cleanupGpFifoChannel)
        {
            PROFILE(RM_FREE);
            LwRmFree(GetClientHandle(),
                     GetDeviceHandle(gpudev),
                     *pRtnChannelHandle);
            DeleteHandle(*pRtnChannelHandle);
            *pRtnChannelHandle = 0;
        }
    };

    CHECK_RC(ChannelIdFromHandle(gpudev, *pRtnChannelHandle, &ChID));

    // Store the new GPFIFO channel in m_Channels.
    unique_ptr<GpFifoChannel> pCh;
    switch (Class)
    {
#if defined(TEGRA_MODS)
        case KEPLER_CHANNEL_GPFIFO_C:
        case MAXWELL_CHANNEL_GPFIFO_A:
        case PASCAL_CHANNEL_GPFIFO_A:
            pCh.reset(new AndroidGpFifoChannel(pUserdAlloc,
                                               pPushBufferBase,
                                               PushBufferSize,
                                               pErrorNotifierMemory,
                                               *pRtnChannelHandle,
                                               ChID,
                                               Class,
                                               PushBufferOffset,
                                               pGpFifoBase,
                                               GpFifoEntries,
                                               hVASpace,
                                               gpudev,
                                               this,
                                               pTsg));
            break;

        case VOLTA_CHANNEL_GPFIFO_A:
        case AMPERE_CHANNEL_GPFIFO_A:
            pCh.reset(new XavierGpFifoChannel(pUserdAlloc,
                                              pPushBufferBase,
                                              PushBufferSize,
                                              pErrorNotifierMemory,
                                              *pRtnChannelHandle,
                                              ChID,
                                              Class,
                                              PushBufferOffset,
                                              pGpFifoBase,
                                              GpFifoEntries,
                                              hVASpace,
                                              gpudev,
                                              this,
                                              pTsg));
            break;
#else
        case KEPLER_CHANNEL_GPFIFO_A:
        case KEPLER_CHANNEL_GPFIFO_B:
        case KEPLER_CHANNEL_GPFIFO_C:
        case MAXWELL_CHANNEL_GPFIFO_A:
        case PASCAL_CHANNEL_GPFIFO_A:
            if (Platform::GetSimulationMode() == Platform::Amodel)
            {
                pCh.reset(new AmodelGpFifoChannel(pUserdAlloc,
                                                  pPushBufferBase,
                                                  PushBufferSize,
                                                  pErrorNotifierMemory,
                                                  *pRtnChannelHandle,
                                                  ChID,
                                                  Class,
                                                  PushBufferOffset,
                                                  pGpFifoBase,
                                                  GpFifoEntries,
                                                  hVASpace,
                                                  gpudev,
                                                  this,
                                                  pTsg,
                                                  engineId));
            }
            else
            {
                pCh.reset(new KeplerGpFifoChannel(pUserdAlloc,
                                                  pPushBufferBase,
                                                  PushBufferSize,
                                                  pErrorNotifierMemory,
                                                  *pRtnChannelHandle,
                                                  ChID,
                                                  Class,
                                                  PushBufferOffset,
                                                  pGpFifoBase,
                                                  GpFifoEntries,
                                                  hVASpace,
                                                  gpudev,
                                                  this,
                                                  pTsg));
            }
            break;
        case VOLTA_CHANNEL_GPFIFO_A:
        case TURING_CHANNEL_GPFIFO_A:
            pCh.reset(new VoltaGpFifoChannel(pUserdAlloc,
                                             pPushBufferBase,
                                             PushBufferSize,
                                             pErrorNotifierMemory,
                                             *pRtnChannelHandle,
                                             ChID,
                                             Class,
                                             PushBufferOffset,
                                             pGpFifoBase,
                                             GpFifoEntries,
                                             hVASpace,
                                             gpudev,
                                             this,
                                             pTsg,
                                             engineId));
            break;
        case AMPERE_CHANNEL_GPFIFO_A:
            pCh.reset(new AmpereGpFifoChannel(pUserdAlloc,
                                              pPushBufferBase,
                                              PushBufferSize,
                                              pErrorNotifierMemory,
                                              *pRtnChannelHandle,
                                              ChID,
                                              Class,
                                              PushBufferOffset,
                                              pGpFifoBase,
                                              GpFifoEntries,
                                              hVASpace,
                                              gpudev,
                                              this,
                                              pTsg,
                                              engineId));
            break;
        case HOPPER_CHANNEL_GPFIFO_A:
            // TODO/FIXME: move to separate case branch once new channel class is ready
            if (gpudev->GetFamily() >= GpuDevice::Blackwell)
            {
                pCh.reset(new BlackwellGpFifoChannel(pUserdAlloc,
                                                     pPushBufferBase,
                                                     PushBufferSize,
                                                     pErrorNotifierMemory,
                                                     *pRtnChannelHandle,
                                                     ChID,
                                                     Class,
                                                     PushBufferOffset,
                                                     pGpFifoBase,
                                                     GpFifoEntries,
                                                     hVASpace,
                                                     gpudev,
                                                     this,
                                                     pTsg,
                                                     engineId,
                                                     useBar1Doorbell));
            }
            else
            {
                pCh.reset(new HopperGpFifoChannel(pUserdAlloc,
                                                pPushBufferBase,
                                                PushBufferSize,
                                                pErrorNotifierMemory,
                                                *pRtnChannelHandle,
                                                ChID,
                                                Class,
                                                PushBufferOffset,
                                                pGpFifoBase,
                                                GpFifoEntries,
                                                hVASpace,
                                                gpudev,
                                                this,
                                                pTsg,
                                                engineId,
                                                useBar1Doorbell));
            }
            break;
        case GF100_CHANNEL_GPFIFO:
            pCh.reset(new FermiGpFifoChannel(pUserdAlloc,
                                             pPushBufferBase,
                                             PushBufferSize,
                                             pErrorNotifierMemory,
                                             *pRtnChannelHandle,
                                             ChID,
                                             Class,
                                             PushBufferOffset,
                                             pGpFifoBase,
                                             GpFifoEntries,
                                             hVASpace,
                                             gpudev,
                                             this));
            break;
#endif
        default:
            MASSERT(0);
            return RC::UNSUPPORTED_FUNCTION;
    }

    if (pCh.get() == nullptr)
    {
        return RC::CANNOT_ALLOCATE_MEMORY;
    }
    else
    {
        CHECK_RC(pCh->Initialize());
    }

    cleanupGpFifoChannel = false;
    cleanupUserdAtExit   = false;

    SetChannel(*pRtnChannelHandle,
               ChannelWrapperMgr::Instance()->WrapChannel(pCh.release()));

    return OK;
}

//------------------------------------------------------------------------------
RC LwRm::AllocDisplayChannelPio
(
    Handle   ParentHandle,
    Handle * pRtnChannelHandle,
    UINT32   Class,
    UINT32   channelInstance,
    Handle   ErrorContextHandle,
    void *   pErrorNotifierMemory,
    UINT32   Flags,
    GpuDevice *gpudev
)
{
    RC rc;
    MASSERT(Class != 0);
    MASSERT(ValidateClient(ParentHandle));
    MASSERT(ValidateClient(ErrorContextHandle));

    MASSERT(gpudev);

    LW50VAIO_CHANNELPIO_ALLOCATION_PARAMETERS ChannelAllocParams = {0};

    ChannelAllocParams.channelInstance = channelInstance; // Channel Instance
    ChannelAllocParams.hObjectNotify = ErrorContextHandle; // Error reporting CtxDma Handle

    UINT32 retval = LW_OK;

#if defined(TEGRA_MODS)
    if (IsDisplayClientAllocated() &&
        IsDisplayClass(Class))
    {
        *pRtnChannelHandle = GenUniqueHandle(ParentHandle, Lib::DISP_RMAPI);
        PROFILE(RM_ALLOC);
        retval = DispLwRmAlloc(m_DisplayClient,
                               ParentHandle,
                               *pRtnChannelHandle,
                               Class,
                               &ChannelAllocParams);
    }
    else
#endif
    {
        *pRtnChannelHandle = GenUniqueHandle(ParentHandle);
        PROFILE(RM_ALLOC);
        retval = LwRmAlloc(GetClientHandle(),
                           ParentHandle,
                           *pRtnChannelHandle,
                           Class,
                           &ChannelAllocParams);
    }

    if (retval != LW_OK)
    {
        DeleteHandle(*pRtnChannelHandle);
        *pRtnChannelHandle = 0;
        return RmApiStatusToModsRC(retval);
    }

    // Obtain the channel mappings
    void *pChannels[LW2080_MAX_SUBDEVICES] = {0};
    for (UINT32 SubdeviceIdx = 0;
         SubdeviceIdx < gpudev->GetNumSubdevices();
         SubdeviceIdx++)
    {
        rc = MapMemoryInternal(*pRtnChannelHandle,
                              0, sizeof(Lw50DispLwrsorControlPio),
                              &pChannels[SubdeviceIdx], 0,
                              gpudev->GetSubdevice(SubdeviceIdx),
                              Class);
        if (rc != OK)
        {
#if defined(TEGRA_MODS)
            if (IsDisplayClientAllocated() &&
                IsDisplayClass(Class))
            {
                PROFILE(RM_FREE);
                DispLwRmFree(m_DisplayClient,
                             ParentHandle,
                             *pRtnChannelHandle);

            }
            else
#endif
            {
                PROFILE(RM_FREE);
                LwRmFree(GetClientHandle(),
                         ParentHandle,
                         *pRtnChannelHandle);
            }
            DeleteHandle(*pRtnChannelHandle);
            *pRtnChannelHandle = 0;
            return rc;
        }
    }

    // Store the new PIO channel in m_Channels.
    PioDisplayChannel * pCh =
        new PioDisplayChannel(pChannels,
                              pErrorNotifierMemory,
                              *pRtnChannelHandle,
                              gpudev);
    if (0 == pCh)
    {
        SetCriticalEvent(Utility::CE_FATAL_ERROR);
        DeleteHandle(*pRtnChannelHandle);
        *pRtnChannelHandle = 0;
        return RC::CANNOT_ALLOCATE_MEMORY;
    }

    SetDisplayChannel(*pRtnChannelHandle, pCh);

    return OK;
}

//------------------------------------------------------------------------------
RC LwRm::AllocDisplayChannelDma
(
    Handle   ParentHandle,
    Handle * pRtnChannelHandle,
    UINT32   Class,
    UINT32   channelInstance,
    Handle   ErrorContextHandle,
    void *   pErrorNotifierMemory,
    Handle   DataContextHandle,
    void *   pPushBufferBase,
    UINT32   PushBufferSize,
    UINT32   Offset,
    UINT32   Flags,
    GpuDevice *gpudev
)
{
    RC rc;
    MASSERT(Class             != 0);
    MASSERT(DataContextHandle != 0);
    MASSERT(ValidateClient(ParentHandle));
    MASSERT(ValidateClient(ErrorContextHandle));
    MASSERT(ValidateClient(DataContextHandle));

    MASSERT(gpudev);

    LW50VAIO_CHANNELDMA_ALLOCATION_PARAMETERS ChannelAllocParams;
    memset(&ChannelAllocParams, 0, sizeof(ChannelAllocParams));

    //
    // With window/windowImm its channel instance
    // and for cursor its Head
    //
    ChannelAllocParams.channelInstance = channelInstance;
    ChannelAllocParams.hObjectBuffer   = DataContextHandle;  // PB CtxDMA Handle
    ChannelAllocParams.hObjectNotify   = ErrorContextHandle; // Error reporting CtxDma Handle
    ChannelAllocParams.offset          = Offset; // Initial offset within the PB
#ifdef LW_VERIF_FEATURES
    ChannelAllocParams.flags           = Flags;
#endif

    UINT32 retval = LW_OK;

#if defined(TEGRA_MODS)
    if (IsDisplayClientAllocated() &&
        IsDisplayClass(Class))
    {
        *pRtnChannelHandle = GenUniqueHandle(ParentHandle, Lib::DISP_RMAPI);
        PROFILE(RM_ALLOC);
        retval = DispLwRmAlloc(m_DisplayClient,
                               ParentHandle,
                               *pRtnChannelHandle,
                               Class,
                               &ChannelAllocParams);
    }
    else
#endif
    {
        *pRtnChannelHandle = GenUniqueHandle(ParentHandle);
        PROFILE(RM_ALLOC);
        retval = LwRmAlloc(GetClientHandle(),
                           ParentHandle,
                           *pRtnChannelHandle,
                           Class,
                           &ChannelAllocParams);
    }

    if (retval != LW_OK)
    {
        DeleteHandle(*pRtnChannelHandle);
        *pRtnChannelHandle = 0;
        return RmApiStatusToModsRC(retval);
    }

    // Obtain the channel mappings
    void *pChannels[LW2080_MAX_SUBDEVICES] = {0};
    for (UINT32 SubdeviceIdx = 0;
         SubdeviceIdx < gpudev->GetNumSubdevices();
         SubdeviceIdx++)
    {
        switch(Class)
        {
            case LWC37D_CORE_CHANNEL_DMA:
            case LWC57D_CORE_CHANNEL_DMA:
            case LWC67D_CORE_CHANNEL_DMA:
            case LWC77D_CORE_CHANNEL_DMA:
            case LWC87D_CORE_CHANNEL_DMA:
                // Core is of 32K on LWC370_DISPLAY/LWC570_DISPLAY
                rc = MapMemoryInternal(*pRtnChannelHandle,
                                      0, sizeof(LWC37DDispControlDma),
                                      &pChannels[SubdeviceIdx], 0,
                                      gpudev->GetSubdevice(SubdeviceIdx),
                                      Class);
                break;
            default:
                // Rest of the channels are of 4k size
                rc = MapMemoryInternal(*pRtnChannelHandle,
                                      0, sizeof(Lw50DispBaseControlDma),
                                      &pChannels[SubdeviceIdx], 0,
                                      gpudev->GetSubdevice(SubdeviceIdx),
                                      Class);
                break;
        }

        if (rc != OK)
        {
#if defined(TEGRA_MODS)
            if (IsDisplayClientAllocated() &&
                IsDisplayClass(Class))
            {
                PROFILE(RM_FREE);
                DispLwRmFree(m_DisplayClient,
                             ParentHandle,
                             *pRtnChannelHandle);

            }
            else
#endif
            {
                PROFILE(RM_FREE);
                LwRmFree(GetClientHandle(),
                         ParentHandle,
                         *pRtnChannelHandle);
            }
            DeleteHandle(*pRtnChannelHandle);
            *pRtnChannelHandle = 0;
            return rc;
        }
    }

    DmaDisplayChannel * pCh = NULL;
    switch(Class)
    {
        case LWC37D_CORE_CHANNEL_DMA:
        case LWC37E_WINDOW_CHANNEL_DMA:
        case LWC37B_WINDOW_IMM_CHANNEL_DMA:
        case LWC57D_CORE_CHANNEL_DMA:
        case LWC57E_WINDOW_CHANNEL_DMA:
        case LWC57B_WINDOW_IMM_CHANNEL_DMA:
        case LWC67D_CORE_CHANNEL_DMA:
        case LWC67E_WINDOW_CHANNEL_DMA:
        case LWC67B_WINDOW_IMM_CHANNEL_DMA:
        case LWC77D_CORE_CHANNEL_DMA:
        // Window DMA classes (77E, 77B) are not created for ADA, it will use same from C6 class
        case LWC87D_CORE_CHANNEL_DMA:
        case LWC87E_WINDOW_CHANNEL_DMA:
        case LWC87B_WINDOW_IMM_CHANNEL_DMA:
            pCh = new DmaDisplayChannelC3(pChannels,
                                          pPushBufferBase,
                                          PushBufferSize,
                                          pErrorNotifierMemory,
                                          *pRtnChannelHandle,
                                          Offset,
                                          gpudev);
            break;
        default :
            pCh = new DmaDisplayChannel(pChannels,
                                        pPushBufferBase,
                                        PushBufferSize,
                                        pErrorNotifierMemory,
                                        *pRtnChannelHandle,
                                        Offset,
                                        gpudev);
    }

    if (NULL == pCh)
    {
        SetCriticalEvent(Utility::CE_FATAL_ERROR);
        DeleteHandle(*pRtnChannelHandle);
        *pRtnChannelHandle = 0;
        return RC::CANNOT_ALLOCATE_MEMORY;
    }

    SetDisplayChannel(*pRtnChannelHandle, pCh);

    return OK;
}

//------------------------------------------------------------------------------
RC LwRm::SwitchToDisplayChannel
(
    Handle ParentHandle,
    Handle DisplayChannelHandle,
    UINT32 Class,
    UINT32 Head,
    Handle ErrorContextHandle,
    Handle DataContextHandle,
    UINT32 Offset,
    UINT32 Flags
)
{
    MASSERT(Class             != 0);
    MASSERT(DataContextHandle != 0);
    MASSERT(ValidateClient(ParentHandle));
    MASSERT(ValidateClient(DisplayChannelHandle));
    MASSERT(ValidateClient(ErrorContextHandle));
    MASSERT(ValidateClient(DataContextHandle));

    LW50VAIO_CHANNELDMA_ALLOCATION_PARAMETERS ChannelAllocParams = {0};

    ChannelAllocParams.channelInstance = Head;               // Head
    ChannelAllocParams.hObjectBuffer   = DataContextHandle;  // PB CtxDMA Handle
    ChannelAllocParams.hObjectNotify   = ErrorContextHandle; // Error reporting CtxDma Handle
    ChannelAllocParams.offset          = Offset;             // Initial offset within the PB
#ifdef LW_VERIF_FEATURES
    ChannelAllocParams.flags           = Flags;
#endif

    // This performs the channel grab
    UINT32 retval;

    {
        PROFILE(RM_ALLOC);
        retval = LwRmAlloc(GetClientHandle(),
                           ParentHandle,
                           DisplayChannelHandle,
                           Class,
                           &ChannelAllocParams);
    }

    return RmApiStatusToModsRC(retval);
}

/**
 *------------------------------------------------------------------------------
 * @function(LwRm::AllocMemory)
 *
 * Allocate memory.
 *------------------------------------------------------------------------------
 */

RC LwRm::AllocMemory
(
    Handle *  pRtnMemoryHandle,
    UINT32    Class,
    UINT32    Flags,
    void   ** pAddress,
    UINT64 *  pLimit,
    GpuDevice *gpudev
)
{
    void *Address;

    MASSERT(Class        != 0);
    MASSERT(pLimit       != 0);
    MASSERT(pRtnMemoryHandle);
    MASSERT(gpudev);

    // Allow the caller to pass in a null pointer if they don't need the address
    if (!pAddress)
        pAddress = &Address;

    // Initialize Address to 0, because certain code only allocates new
    // memory if Address (*pAddress) is 0.
    if (Class != LW01_MEMORY_FLA)
    {
        *pAddress = 0;
    }

    *pRtnMemoryHandle = GenUniqueHandle(GetDeviceHandle(gpudev));

    UINT32 result;

    {
        PROFILE(RM_ALLOC);
        result = LwRmAllocMemory64(GetClientHandle(),
                                   GetDeviceHandle(gpudev),
                                   *pRtnMemoryHandle,
                                   Class,
                                   Flags,
                                   pAddress,
                                   pLimit);
    }

    if (result != LW_OK)
    {
        DeleteHandle(*pRtnMemoryHandle);
        *pRtnMemoryHandle = 0;
        return RmApiStatusToModsRC(result);
    }

    return OK;
}

RC LwRm::AllocSystemMemory
(
    Handle *  pRtnMemoryHandle,
    UINT32    Flags,
    UINT64    Size,
    GpuDevice *gpudev
)
{
    // Compute the limit from the size, and set the no-map flag automatically.
    UINT64 Limit = Size - 1;
    Flags |= DRF_DEF(OS02, _FLAGS, _MAPPING, _NO_MAP);

    return AllocMemory(pRtnMemoryHandle, LW01_MEMORY_SYSTEM,
                       Flags, NULL, &Limit, gpudev);
}

//------------------------------------------------------------------------------
RC LwRm::AllocEvent
(
    Handle    ParentHandle,
    Handle *  pRtnEventHandle,
    UINT32    Class,
    UINT32    Index,
    void *    pOsEvent
)
{
    MASSERT(pRtnEventHandle);
    MASSERT(pOsEvent);
    MASSERT(ValidateClient(ParentHandle));

    *pRtnEventHandle = GenUniqueHandle(ParentHandle);

    UINT32 result;

    {
        PROFILE(RM_ALLOC);
        result = LwRmAllocEvent(GetClientHandle(),
                                ParentHandle,
                                *pRtnEventHandle,
                                Class,
                                Index,
                                pOsEvent);
    }

    if (result != LW_OK)
    {
        DeleteHandle(*pRtnEventHandle);
        *pRtnEventHandle = 0;
        return RmApiStatusToModsRC(result);
    }

    return OK;
}

//------------------------------------------------------------------------------
RC LwRm::MapMemory
(
    Handle   MemoryHandle,
    UINT64   Offset,
    UINT64   Length,
    void  ** pAddress,
    UINT32   Flags,
    GpuSubdevice *gpusubdev
)
{
    SCOPED_DEV_INST(gpusubdev);

    MASSERT(MemoryHandle != 0);
    MASSERT(Length       != 0);
    MASSERT(pAddress     != 0);
    MASSERT(ValidateClient(MemoryHandle));

    if (!gpusubdev->HasFeature(Device::GPUSUB_SUPPORTS_SYSMEM_REFLECTED_MAPPING))
    {
        Flags = FLD_SET_DRF(OS33, _FLAGS, _MAPPING, _DIRECT, Flags);
    }

    Handle hSubdev = GetSubdeviceHandle(gpusubdev);

    UINT32 retval;

    {
        PROFILE(RM_ALLOC);

#if defined(LINUX_MFG)
        // Internal call is used to ignore RM locks
        // Copied from lwrmdos.c
        if (m_UseInternalCall)
        {
            LWOS33_PARAMETERS Parms;

            memset(&Parms, 0, sizeof(Parms));
            Parms.hClient             = GetClientHandle();
            Parms.hDevice             = hSubdev;
            Parms.hMemory             = MemoryHandle;
            Parms.offset              = Offset;
            Parms.length              = Length;
            Parms.pLinearAddress      = 0;
            Parms.flags               = Flags;

            Lw04MapMemoryInternal(&Parms);

            *pAddress = LwP64_VALUE(Parms.pLinearAddress);

            retval = Parms.status;
        }
        else
#endif
        {
            retval = LwRmMapMemory(GetClientHandle(),
                                   hSubdev,
                                   MemoryHandle,
                                   Offset,
                                   Length,
                                   pAddress,
                                   Flags);
        }
    }

    return RmApiStatusToModsRC(retval);
}

//------------------------------------------------------------------------------
void LwRm::UnmapMemory
(
    Handle          MemoryHandle,
    volatile void * Address,
    UINT32          Flags,
    GpuSubdevice  * gpusubdev
)
{
    SCOPED_DEV_INST(gpusubdev);

    // Harmlessly ignore unmaps of null pointers
    if (!Address)
        return;

    MASSERT(MemoryHandle != 0);
    MASSERT(ValidateClient(MemoryHandle));
    Handle hSubdev = GetSubdeviceHandle(gpusubdev);

    UINT32 retval;

    {
        PROFILE(RM_FREE);

#if defined(LINUX_MFG)
        // Internal call is used to ignore RM locks
        // Copied from lwrmdos.c
        if (m_UseInternalCall)
        {
            LWOS34_PARAMETERS Parms;

            memset(&Parms, 0, sizeof(Parms));
            Parms.hClient             = GetClientHandle();
            Parms.hDevice             = hSubdev;
            Parms.hMemory             = MemoryHandle;
            Parms.pLinearAddress      = LW_PTR_TO_LwP64(const_cast<void *>(Address));
            Parms.flags               = Flags;

            Lw04UnmapMemoryInternal(&Parms);

            retval = Parms.status;
        }
        else
#endif
        {
            retval = LwRmUnmapMemory(GetClientHandle(),
                                     hSubdev,
                                     MemoryHandle,
                                     const_cast<void *>(Address),
                                     Flags);
        }
    }

    MASSERT(retval == LW_OK);
    if (retval != LW_OK)
    {
        Printf(Tee::PriDebug, "Failed to unmap memory handle 0x%x\n", MemoryHandle);
    }
}

//------------------------------------------------------------------------------
RC LwRm::MapFbMemory
(
    UINT64   Offset,
    UINT64   Length,
    void  ** pAddress,
    UINT32   Flags,
    GpuSubdevice *Subdev
)
{
    SCOPED_DEV_INST(Subdev);

    MASSERT(Subdev);

    return MapMemory(Subdev->GetParentDevice()->GetFbMem(this),
                     Offset, Length, pAddress, Flags, Subdev);
}

//------------------------------------------------------------------------------
void LwRm::UnmapFbMemory
(
    volatile void * Address,
    UINT32          Flags,
    GpuSubdevice *Subdev
)
{
    SCOPED_DEV_INST(Subdev);

    MASSERT(Subdev);

    UnmapMemory(Subdev->GetParentDevice()->GetFbMem(this),
                Address, Flags, Subdev);
}

//------------------------------------------------------------------------------
RC LwRm::FlushUserCache
(
    GpuDevice* pGpuDev,
    Handle     hMem,
    UINT64     offset,
    UINT64     size
)
{
#if defined(LW0000_CTRL_CMD_OS_UNIX_FLUSH_USER_CACHE)
    LW0000_CTRL_OS_UNIX_FLUSH_USER_CACHE_PARAMS params = {0};
    params.offset   = offset;
    params.length   = size;
    params.cacheOps = LW0000_CTRL_OS_UNIX_FLAGS_USER_CACHE_FLUSH_ILWALIDATE;
    params.hDevice  = GetDeviceHandle(pGpuDev);
    params.hObject  = hMem;
    return Control(GetClientHandle(),
            LW0000_CTRL_CMD_OS_UNIX_FLUSH_USER_CACHE,
            &params, sizeof(params));
#else
    Printf(Tee::PriError, "Missing CPU cache flush implementation!\n");
    MASSERT(0);
    return RC::SOFTWARE_ERROR;
#endif
}

//------------------------------------------------------------------------------
RC LwRm::AllocObject
(
    Handle   ChannelHandle,
    Handle * pRtnObjectHandle,
    UINT32   Class
)
{
    MASSERT(ValidateClient(ChannelHandle));
    *pRtnObjectHandle = GenUniqueHandle(ChannelHandle);

    UINT32 retval;

    {
        PROFILE(RM_ALLOC);
        retval = LwRmAllocObject(GetClientHandle(),
                                 ChannelHandle,
                                 *pRtnObjectHandle,
                                 Class);
    }

    if (retval != LW_OK)
    {
        DeleteHandle(*pRtnObjectHandle);
        *pRtnObjectHandle = 0;
        return RmApiStatusToModsRC(retval);
    }

    return OK;
}

LwRm::Lib LwRm::GetLibFromHandle(Handle handle)
{
    return ((handle >> 24) & 0x1) ? Lib::DISP_RMAPI : Lib::RMAPI;
}

bool LwRm::ValidateHandleTagAndLib(Handle handle)
{
    const Lib lib = GetLibFromHandle(handle);

    return (Utility::HandleStorage::GeneratedHandle(handle) &&
            ((lib == Lib::RMAPI)
#if defined(TEGRA_MODS)
            || (lib == Lib::DISP_RMAPI)
#endif
           ));
}

/**
 *------------------------------------------------------------------------------
 * @function(LwRm::GenUniqueHandle)
 *
 * Private utility function, generate a globally unique handle ID based on
 * the input class number.
 *------------------------------------------------------------------------------
 */

LwRm::Handle LwRm::GenUniqueHandle(UINT32 Parent, Lib lib)
{
    MASSERT((lib == Lib::RMAPI) || (lib == Lib::DISP_RMAPI));
    Tasker::MutexHolder mh(m_Mutex);

    const UINT32 h = m_Handles.UpperBits
                   | ((static_cast<UINT32>(lib) & 1U) << 24)
                   | (m_ClientId << 16);
    return m_Handles.GenHandle(h, Parent);
}

//------------------------------------------------------------------------------
// Remove a handle and all of its children from the handle tree
void LwRm::DeleteHandleWithChildren(LwRm::Handle Handle)
{
    Tasker::MutexHolder mh(m_Mutex);

    MASSERT(ValidateClient(Handle));
    // The handle had better be in the tree
    MASSERT(m_Handles.GetParent(Handle) != m_Handles.Invalid);

    m_Handles.FreeWithChildren(Handle);
}

//------------------------------------------------------------------------------
// Remove a handle from the handle tree
void LwRm::DeleteHandle(LwRm::Handle handle)
{
    Tasker::MutexHolder mh(m_Mutex);

    MASSERT(ValidateClient(handle));
    m_Handles.Free(handle);
}

UINT32 LwRm::ClientIdFromHandle(LwRm::Handle Handle)
{
    return (static_cast<UINT32>(Handle) >> 16) & 0xFFU;
}

LwRm::Handle LwRm::ClientHandleFromObjectHandle(LwRm::Handle Handle)
{
    LwRmPtr pLwRm(ClientIdFromHandle(Handle));

    return pLwRm->GetClientHandle();
}

bool LwRm::ValidateClient(LwRm::Handle Handle)
{
    if ((Handle == 0) ||
        (!ValidateHandleTagAndLib(Handle) &&
        ((Handle & 0xFF000000) != s_DevBaseHandle)))
    {
        // Either not a MODS created handle or not a handle at all
        // flag as valid.
        return true;
    }

    if (ClientIdFromHandle(Handle) != m_ClientId)
    {
        Printf(Tee::PriError, "Handle 0x%08x does not belong to RM Client %d\n",
               Handle, m_ClientId);
        MASSERT(!"Generic assertion failure<refer to previous message>.");
        return false;
    }

    return true;
}

void LwRm::SetChannel(Handle handle, Channel *pChan)
{
    Tasker::MutexHolder mh(m_Mutex);
    m_Channels[handle] = pChan;
}

void LwRm::SetDisplayChannel(Handle handle, DisplayChannel *pDispChan)
{
    Tasker::MutexHolder mh(m_Mutex);
    m_DisplayChannels[handle] = pDispChan;
}

//! Colwert a channel handle to a ChID.  Used by the AllocChannel methods.
RC LwRm::ChannelIdFromHandle
(
    GpuDevice *pGpuDevice,
    Handle hChannel,
    UINT32 *pChID
)
{
    LwU32 ChannelHandleList = hChannel;
    LwU32 ChannelList = 0;
    LW0080_CTRL_FIFO_GET_CHANNELLIST_PARAMS Params = {0};
    RC rc;

    Params.numChannels = 1;
    Params.pChannelHandleList = LW_PTR_TO_LwP64(&ChannelHandleList);
    Params.pChannelList = LW_PTR_TO_LwP64(&ChannelList);
    CHECK_RC(ControlByDevice(pGpuDevice, LW0080_CTRL_CMD_FIFO_GET_CHANNELLIST,
                             &Params, sizeof(Params)));
    *pChID = ChannelList;
    return rc;
}

//! \brief Free a subdevice by reference
//!
//! SubDeviceReference is 0 indexed, so we add 1 to get the proper reference
//! (0 would be broadcast).
void LwRm::FreeSubDevice(UINT32 DeviceReference, UINT32 SubDeviceReference)
{
    Free(SubdevHandle(DeviceReference, SubDeviceReference+1));
}

//! Free a device by reference
void LwRm::FreeDevice(UINT32 DeviceReference)
{
    int Children = CountChildren(DevHandle(DeviceReference));
    if (Children)
    {
        Printf(Tee::PriError,
               "Resource leak detected: RM device has %d children "
               "outstanding\n", Children);
        // XXX Should be MASSERT(0), but too many arch tests leak
        // stuff right now
        // Well, let's at least *try* to keep them honest
        MASSERT(Children < 5);
    }

    // this will free any outstanding children (the terrible ones remain
    // imprisoned)
    Free(DevHandle(DeviceReference));
}

void LwRm::FreeSyscon(UINT32 SysconRef)
{
    Free(SysconRef);
}

/**
 *------------------------------------------------------------------------------
 * @function(LwRm::Free)
 *
 * Free an object.
 *------------------------------------------------------------------------------
 */

void LwRm::Free
(
    Handle ObjectHandle
)
{
    // Harmlessly ignore frees of null objects
    if (ObjectHandle == 0)
        return;

    Tasker::MutexHolder mh(m_Mutex);

    // The handle had better be in the tree
    MASSERT(ValidateClient(ObjectHandle));
    const Handle ParentHandle = m_Handles.GetParent(ObjectHandle);

    Channel * pChan = GetChannel(ParentHandle);
    if (0 != pChan)
    {
        RC oldError = pChan->CheckError();
        pChan->ClearError();
        RC rc = pChan->UnsetObject(ObjectHandle);
        if (OK != rc)
        {
            Printf(Tee::PriError,
                   "LwRm::Free - error %d trying to clean up"
                    " channel %d before freeing object H=%x\n",
                   UINT32(rc), pChan->GetChannelId(), ObjectHandle);
        }
        pChan->ClearError();
        pChan->SetError(oldError);
    }

    Handle hClient = GetClientHandle();
    UINT32 retval = LW_OK;

    if (Lib::RMAPI == GetLibFromHandle(ObjectHandle))
    {
        PROFILE(RM_FREE);
        retval = LwRmFree(hClient,
                          ParentHandle,
                          ObjectHandle);
    }

    if (retval != LW_OK)
    {
        Printf(Tee::PriLow, "Failed to delete memory with handle: 0x%08x!\n", ObjectHandle);
        MASSERT(!"Failed to delete memory.\n");
        return;
    }

#if defined(TEGRA_MODS)
    FreeDispRmHandle(ObjectHandle);
#endif

    // Don't remove the handle from the handle tree until after LwRmFree has
    // returned, since only now is it safe for another thread to recycle it
    DeleteHandleWithChildren(ObjectHandle);
    Printf(Tee::PriDebug, "LwRm::Free 0x%x, num handles: %u\n",
           ObjectHandle, m_Handles.NumHandles());

    if (hClient == ObjectHandle)
    {
        // Free all channels.
        for
        (
            Channels::iterator iChannel = m_Channels.begin();
            iChannel != m_Channels.end();
            ++iChannel
        )
        {
            delete iChannel->second;
        }
        m_Channels.clear();

        // Free all display channels.
        for
        (
            DisplayChannels::iterator iDisplayChannel = m_DisplayChannels.begin();
            iDisplayChannel != m_DisplayChannels.end();
            ++iDisplayChannel
        )
        {
            delete iDisplayChannel->second;
        }
        m_DisplayChannels.clear();
    }
    else
    {
        if (m_Channels.find(ObjectHandle) != m_Channels.end())
        {
            // Free associated channel object
            Channel *pCh = m_Channels[ObjectHandle];
            delete pCh;
            m_Channels.erase(ObjectHandle);
        }
        if (m_DisplayChannels.find(ObjectHandle) != m_DisplayChannels.end())
        {
            // Free associated display channel object
            DisplayChannel *pDispCh = m_DisplayChannels[ObjectHandle];
            delete pDispCh;
            m_DisplayChannels.erase(ObjectHandle);
        }
    }
}

//------------------------------------------------------------------------------
RC LwRm::VidHeapControl
(
    void *pParams,
    GpuDevice *gpudev
)
{
    LWOS32_PARAMETERS *vidHeapParams = (LWOS32_PARAMETERS *)pParams;

    // This function fills the client and device automatically, but the caller
    // is responsible for all the rest.
    vidHeapParams->hRoot         = GetClientHandle();
    vidHeapParams->hObjectParent = GetDeviceHandle(gpudev);

    UINT32 retval;

    {
        PROFILE(RM_CTRL);
        retval = LwRmVidHeapControl(pParams);
    }

    return RmApiStatusToModsRC(retval);
}

//------------------------------------------------------------------------------
RC LwRm::ReadRegistryDword
(
    const char      *devNode,
    const char      *parmStr,
    UINT32          *data
)
{
#if !defined(CLIENT_SIDE_RESMAN) && !defined(TEGRA_MODS) && !defined(XP_WIN) && !defined(DIRECTAMODEL)
    UINT32 retval;

    retval = LwRmReadRegistryDword(m_Client, m_Client, const_cast<char *>(devNode), parmStr, data);

    if (retval != LW_OK)
    {
        return RmApiStatusToModsRC(retval);
    }

    return OK;
#else
    return RC::LWRM_NOT_SUPPORTED;
#endif
}

//------------------------------------------------------------------------------
RC LwRm::WriteRegistryDword
(
    const char      *devNode,
    const char      *parmStr,
    UINT32           data
)
{
#if !defined(CLIENT_SIDE_RESMAN) && !defined(TEGRA_MODS) && !defined(XP_WIN) && !defined(DIRECTAMODEL)
    UINT32 retval;

    retval = LwRmWriteRegistryDword(m_Client, m_Client, const_cast<char *>(devNode), parmStr, data);

    if (retval != LW_OK)
    {
        return RmApiStatusToModsRC(retval);
    }

    return OK;
#else
    return RC::LWRM_NOT_SUPPORTED;
#endif
}

//------------------------------------------------------------------------------
RC LwRm::VidHeapAllocSize
(
    UINT32   Type,
    UINT32   Attr,
    UINT32   Attr2,
    UINT64   Size,
    UINT32 * pHandle,
    UINT64 * pOffset,
    UINT64 * pLimit,
    UINT64 * pRtnFree,
    UINT64 * pRtnTotal,
    GpuDevice *gpudev,
    UINT32   hVASpace
)
{
    return VidHeapAllocSizeEx(Type, 0, &Attr, &Attr2, Size, 0, NULL, NULL, NULL,
                              pHandle, pOffset, pLimit, pRtnFree, pRtnTotal,
                              0, 0, gpudev, hVASpace);
}

//------------------------------------------------------------------------------
RC LwRm::VidHeapAllocSizeEx
(
    UINT32   Type,
    UINT32   Flags,
    UINT32 * pAttr,
    UINT32 * pAttr2,
    UINT64   Size,
    UINT64   Alignment,
    INT32  * pFormat,
    UINT32 * pCoverage,
    UINT32 * pPartitionStride,
    UINT32 * pHandle,
    UINT64 * pOffset,
    UINT64 * pLimit,
    UINT64 * pRtnFree,
    UINT64 * pRtnTotal,
    UINT32   width,
    UINT32   height,
    GpuDevice *gpudev,
    UINT32   hVASpace
)
{
    return VidHeapAllocSizeEx(Type, Flags, pAttr, pAttr2, Size, Alignment,
        pFormat, pCoverage, pPartitionStride, 0, pHandle, pOffset,
        pLimit, pRtnFree, pRtnTotal, width, height, gpudev, 0, 0, hVASpace);
}

//------------------------------------------------------------------------------
RC LwRm::VidHeapAllocSizeEx
(
    UINT32   Type,
    UINT32   Flags,
    UINT32 * pAttr,
    UINT32 * pAttr2,
    UINT64   Size,
    UINT64   Alignment,
    INT32  * pFormat,
    UINT32 * pCoverage,
    UINT32 * pPartitionStride,
    UINT32   CtagOffset,
    UINT32 * pHandle,
    UINT64 * pOffset,
    UINT64 * pLimit,
    UINT64 * pRtnFree,
    UINT64 * pRtnTotal,
    UINT32   width,
    UINT32   height,
    GpuDevice *gpudev,
    UINT64   rangeBegin,
    UINT64   rangeEnd,
    UINT32   hVASpace
)
{
    RC rc;
    MASSERT(gpudev);
    LWOS32_PARAMETERS params;

#if (defined(LW_MACINTOSH_OSX) && !defined(MACOSX_MFG))
    // Work around a Mac bug: if primary is requested, use type of IMAGE
    // rather than PRIMARY (Apple will not free it's primary).
    // This WAR is NOT applicable to single user Mac MODS
    if (Type == LWOS32_TYPE_PRIMARY)
    {
        Type = LWOS32_TYPE_IMAGE;
    }
#endif

    LwRm::Handle hMemory = GenUniqueHandle(GetDeviceHandle(gpudev));

    memset(&params, 0, sizeof(params));
    //
    // hVASpace handle is required when VIRTUAL flag is provide with per
    // channel vaspace binding option enabled. The option is set under
    // device allocation.
    //
    params.hVASpace = hVASpace;
    params.function = LWOS32_FUNCTION_ALLOC_SIZE;
    params.data.AllocSize.owner     = s_VidHeapOwnerID;
    params.data.AllocSize.type      = Type;
    params.data.AllocSize.flags     = Flags;
    params.data.AllocSize.size      = Size;
    params.data.AllocSize.alignment = Alignment;
    params.data.AllocSize.offset    = pOffset ? *pOffset : 0;
    params.data.AllocSize.rangeBegin = rangeBegin;
    params.data.AllocSize.rangeEnd  = rangeEnd;
    params.data.AllocSize.attr      = pAttr ? *pAttr : 0;
    params.data.AllocSize.attr2     = pAttr2 ? *pAttr2 : 0;
    params.data.AllocSize.format    = pFormat ? *pFormat : 0;
    params.data.AllocSize.comprCovg = pCoverage ? *pCoverage : 0;
    params.data.AllocSize.partitionStride = pPartitionStride ? *pPartitionStride : 0;
    params.data.AllocSize.width     = width ? width : 1; // otherwise RM returns error if buffer
    params.data.AllocSize.height    = height ? height : 1; // is allocated with zlwll, see ZLwllAlloc_LW50()

    params.data.AllocSize.hMemory = hMemory;
    params.data.AllocSize.flags |= LWOS32_ALLOC_FLAGS_MEMORY_HANDLE_PROVIDED;
    params.data.AllocSize.ctagOffset = CtagOffset;
    rc = VidHeapControl(&params, gpudev);
    if (rc != OK)
    {
        DeleteHandle(hMemory);
        params.data.AllocSize.hMemory = hMemory = 0;
    }

    if (pFormat)
        *pFormat = params.data.AllocSize.format;
    if (pCoverage)
        *pCoverage = params.data.AllocSize.comprCovg;
    if (pPartitionStride)
        *pPartitionStride = params.data.AllocSize.partitionStride;
    if (pHandle)
        *pHandle = params.data.AllocSize.hMemory;
    if (pOffset)
        *pOffset = params.data.AllocSize.offset;
    if (pAttr)
        *pAttr = (LwU32)params.data.AllocSize.attr;
    if (pAttr2)
        *pAttr2 = (LwU32)params.data.AllocSize.attr2;
    if (pLimit)
        *pLimit = params.data.AllocSize.limit;
    if (pRtnFree)
        *pRtnFree = params.free;
    if (pRtnTotal)
        *pRtnTotal = params.total;

    return rc;
}

//------------------------------------------------------------------------------
RC LwRm::VidHeapAllocSizeEx
(
    UINT32   Type,
    UINT32   Flags,
    UINT32 * pAttr,
    UINT32 * pAttr2,
    UINT64 * pSize,
    UINT64 * pAlignment,
    UINT32 * pHandle,
    UINT64 * pOffset,
    UINT64 * pLimit,
    GpuDevice *gpudev,
    UINT64   rangeBegin,
    UINT64   rangeEnd,
    UINT32   hVASpace
)
{
    RC rc;
    LWOS32_PARAMETERS params;
    LwRm::Handle hMemory = GenUniqueHandle(GetDeviceHandle(gpudev));

    memset(&params, 0, sizeof(params));

    params.hVASpace = hVASpace;
    params.function = LWOS32_FUNCTION_ALLOC_SIZE;
    params.data.AllocSize.owner     = s_VidHeapOwnerID;
    params.data.AllocSize.type      = Type;
    params.data.AllocSize.flags     = Flags;
    params.data.AllocSize.size      = pSize ? *pSize : 0;
    params.data.AllocSize.alignment = pAlignment ? *pAlignment : 0;
    params.data.AllocSize.offset    = pOffset ? *pOffset : 0;
    params.data.AllocSize.rangeBegin = rangeBegin;
    params.data.AllocSize.rangeEnd  = rangeEnd;
    params.data.AllocSize.attr      = pAttr ? *pAttr : 0;
    params.data.AllocSize.attr2     = pAttr2 ? *pAttr2 : 0;
    params.data.AllocSize.width     = 1;
    params.data.AllocSize.height    = 1;

    params.data.AllocSize.hMemory = hMemory;
    params.data.AllocSize.flags |= LWOS32_ALLOC_FLAGS_MEMORY_HANDLE_PROVIDED;
    rc = VidHeapControl(&params, gpudev);
    if (rc != OK)
    {
        DeleteHandle(hMemory);
        params.data.AllocSize.hMemory = hMemory = 0;
    }

    if (pAttr)
        *pAttr = params.data.AllocSize.attr;
    if (pAttr2)
        *pAttr2 = params.data.AllocSize.attr2;
    if (pSize)
        *pSize = params.data.AllocSize.size;
    if (pAlignment)
        *pAlignment = params.data.AllocSize.alignment;
    if (pHandle)
        *pHandle = params.data.AllocSize.hMemory;
    if (pOffset)
        *pOffset = params.data.AllocSize.offset;
    if (pLimit)
        *pLimit = params.data.AllocSize.limit;

    return rc;
}

//------------------------------------------------------------------------------
RC LwRm::VidHeapAllocPitchHeight
(
    UINT32   Type,
    UINT32   Attr,
    UINT32   Attr2,
    UINT32   Height,
    UINT32 * pPitch,
    UINT32 * pHandle,
    UINT64 * pOffset,
    UINT64 * pLimit,
    UINT64 * pRtnFree,
    UINT64 * pRtnTotal,
    GpuDevice *gpudev,
    LwRm::Handle hVASpace
)
{
    return VidHeapAllocPitchHeightEx(Type, 0, &Attr, &Attr2, Height, pPitch, 0,
                                     NULL, NULL, NULL, 0, pHandle, pOffset,
                                     pLimit, pRtnFree, pRtnTotal, gpudev, 0, 0, hVASpace);
}

//------------------------------------------------------------------------------
RC LwRm::VidHeapAllocPitchHeightEx
(
    UINT32   Type,
    UINT32   Flags,
    UINT32 * pAttr,
    UINT32 * pAttr2,
    UINT32   Height,
    UINT32 * pPitch,
    UINT64   Alignment,
    INT32  * pFormat,
    UINT32 * pCoverage,
    UINT32 * pPartitionStride,
    UINT32 * pHandle,
    UINT64 * pOffset,
    UINT64 * pLimit,
    UINT64 * pRtnFree,
    UINT64 * pRtnTotal,
    GpuDevice *gpudev,
    LwRm::Handle hVASpace
)
{
    return VidHeapAllocPitchHeightEx(Type, Flags, pAttr, pAttr2, Height, pPitch,
        Alignment, pFormat, pCoverage, pPartitionStride, 0, pHandle, pOffset,
        pLimit, pRtnFree, pRtnTotal, gpudev, 0, 0, hVASpace);
}

//------------------------------------------------------------------------------
RC LwRm::VidHeapAllocPitchHeightEx
(
    UINT32   Type,
    UINT32   Flags,
    UINT32 * pAttr,
    UINT32 * pAttr2,
    UINT32   Height,
    UINT32 * pPitch,
    UINT64   Alignment,
    INT32  * pFormat,
    UINT32 * pCoverage,
    UINT32 * pPartitionStride,
    UINT32   CtagOffset,
    UINT32 * pHandle,
    UINT64 * pOffset,
    UINT64 * pLimit,
    UINT64 * pRtnFree,
    UINT64 * pRtnTotal,
    GpuDevice *gpudev,
    UINT64   rangeBegin,
    UINT64   rangeEnd,
    LwRm::Handle hVASpace
)
{
    RC rc;
    MASSERT(gpudev);
    LWOS32_PARAMETERS params;

    // Work around an RM bug: if compression is requested, use type of IMAGE
    // rather than PRIMARY (trying to allocate PRIMARY will fail).
    if ((Type == LWOS32_TYPE_PRIMARY) && DRF_VAL(OS32, _ATTR, _COMPR, *pAttr))
    {
        Type = LWOS32_TYPE_IMAGE;
    }

#if defined(LW_MACINTOSH_OSX)
    // Work around a Mac bug: if primary is requested, use type of IMAGE
    // rather than PRIMARY (Apple will not free it's primary).
    if (Type == LWOS32_TYPE_PRIMARY)
    {
        Type = LWOS32_TYPE_IMAGE;
    }
#endif

    LwRm::Handle hMemory = GenUniqueHandle(GetDeviceHandle(gpudev));

    memset(&params, 0, sizeof(params));
    params.hVASpace = hVASpace;
    params.function = LWOS32_FUNCTION_ALLOC_TILED_PITCH_HEIGHT;
    params.data.AllocTiledPitchHeight.owner     = s_VidHeapOwnerID;
    params.data.AllocTiledPitchHeight.type      = Type;
    params.data.AllocTiledPitchHeight.flags     = Flags;
    params.data.AllocTiledPitchHeight.height    = Height;
    params.data.AllocTiledPitchHeight.pitch     = *pPitch;
    params.data.AllocTiledPitchHeight.alignment = Alignment;
    params.data.AllocTiledPitchHeight.offset    = pOffset ? *pOffset : 0;
    params.data.AllocTiledPitchHeight.rangeBegin = rangeBegin;
    params.data.AllocTiledPitchHeight.rangeEnd  = rangeEnd;
    params.data.AllocTiledPitchHeight.attr      = *pAttr;
    params.data.AllocTiledPitchHeight.attr2     = pAttr2 ? *pAttr2 : 0;
    params.data.AllocTiledPitchHeight.format    = pFormat ? *pFormat : 0;
    params.data.AllocTiledPitchHeight.comprCovg = pCoverage ? *pCoverage : 0;
    params.data.AllocTiledPitchHeight.partitionStride = pPartitionStride ? *pPartitionStride : 0;
    params.data.AllocTiledPitchHeight.ctagOffset = CtagOffset;

    params.data.AllocTiledPitchHeight.hMemory = hMemory;
    params.data.AllocTiledPitchHeight.flags |= LWOS32_ALLOC_FLAGS_MEMORY_HANDLE_PROVIDED;

    rc = VidHeapControl(&params, gpudev);
    if (rc != OK)
    {
        DeleteHandle(hMemory);
        params.data.AllocTiledPitchHeight.hMemory = hMemory = 0;
    }

    *pAttr   = params.data.AllocTiledPitchHeight.attr;
    *pPitch  = params.data.AllocTiledPitchHeight.pitch;
    if (pAttr2)
        *pAttr2 = params.data.AllocTiledPitchHeight.attr2;
    if (pFormat)
        *pFormat = params.data.AllocTiledPitchHeight.format;
    if (pCoverage)
        *pCoverage = params.data.AllocTiledPitchHeight.comprCovg;
    if (pPartitionStride)
        *pPartitionStride = params.data.AllocTiledPitchHeight.partitionStride;
    if (pHandle)
        *pHandle = params.data.AllocTiledPitchHeight.hMemory;
    if (pOffset)
        *pOffset = params.data.AllocTiledPitchHeight.offset;
    if (pLimit)
        *pLimit = params.data.AllocTiledPitchHeight.limit;
    if (pRtnFree)
        *pRtnFree = params.free;
    if (pRtnTotal)
        *pRtnTotal = params.total;

    return rc;
}

//------------------------------------------------------------------------------
RC LwRm::VidHeapReleaseCompression
(
    UINT32     hMemory,
    GpuDevice *gpudev
)
{
    LWOS32_PARAMETERS params;

    memset(&params, 0, sizeof(params));
    params.function = LWOS32_FUNCTION_RELEASE_COMPR;
    params.data.ReleaseCompr.owner   = s_VidHeapOwnerID;
    params.data.ReleaseCompr.hMemory = hMemory;
    params.data.ReleaseCompr.flags   = LWOS32_RELEASE_COMPR_FLAGS_MEMORY_HANDLE_PROVIDED;

    return VidHeapControl(&params, gpudev);
}

//------------------------------------------------------------------------------
RC LwRm::VidHeapReacquireCompression
(
    UINT32     hMemory,
    GpuDevice *gpudev
)
{
    LWOS32_PARAMETERS params;

    memset(&params, 0, sizeof(params));
    params.function = LWOS32_FUNCTION_REACQUIRE_COMPR;
    params.data.ReacquireCompr.owner   = s_VidHeapOwnerID;
    params.data.ReacquireCompr.hMemory = hMemory;
    params.data.ReacquireCompr.flags   = LWOS32_REACQUIRE_COMPR_FLAGS_MEMORY_HANDLE_PROVIDED;

    return VidHeapControl(&params, gpudev);
}

//------------------------------------------------------------------------------
RC LwRm::VidHeapHwAlloc
(
    UINT32 AllocType,
    UINT32 AllocAttr,
    UINT32 AllocAttr2,
    UINT32 AllocWidth,
    UINT32 AllocPitch,
    UINT32 AllocHeight,
    UINT32 AllocSize,
    UINT32 *pAllochMemory,
    GpuDevice *pGpuDev
)
{
    RC rc;
    LWOS32_PARAMETERS params;

    memset(&params, 0, sizeof(params));
    params.function                = LWOS32_FUNCTION_HW_ALLOC;
    params.data.HwAlloc.allocType  = AllocType;
    params.data.HwAlloc.allocAttr  = AllocAttr;
    params.data.HwAlloc.allocAttr2 = AllocAttr2;
    params.data.HwAlloc.allocWidth = AllocWidth;
    params.data.HwAlloc.allocPitch = AllocPitch;
    params.data.HwAlloc.allocHeight = AllocHeight;
    params.data.HwAlloc.allocSize  = AllocSize;
    rc = VidHeapControl(&params, pGpuDev);
    *pAllochMemory = params.data.HwAlloc.allochMemory;
    return rc;
}

//------------------------------------------------------------------------------
RC LwRm::VidHeapHwFree
(
    UINT32 hResourceHandle,
    UINT32 flags,
    GpuDevice *pGpuDev
)
{
    RC rc;
    LWOS32_PARAMETERS params;

    memset(&params, 0, sizeof(params));
    params.function                = LWOS32_FUNCTION_HW_FREE;
    params.data.HwFree.hResourceHandle  = hResourceHandle;
    params.data.HwAlloc.flags  = flags;

    rc = VidHeapControl(&params, pGpuDev);
    return rc;
}
//------------------------------------------------------------------------------
RC LwRm::VidHeapInfo
(
    UINT64 * pFree,
    UINT64 * pTotal,
    UINT64 * pOffsetLargest,
    UINT64 * pSizeLargest,
    GpuDevice *GpuDev
)
{
    RC rc;
    LWOS32_PARAMETERS params;

    memset(&params, 0, sizeof(params));
    params.function = LWOS32_FUNCTION_INFO;
    params.data.Info.attr = DRF_DEF(OS32,_ATTR,_LOCATION,_VIDMEM);

    rc = VidHeapControl(&params, GpuDev);

    if (pFree)
        *pFree = params.free;
    if (pTotal)
        *pTotal = params.total;
    if (pOffsetLargest)
        *pOffsetLargest = params.data.Info.offset;
    if (pSizeLargest)
        *pSizeLargest = params.data.Info.size;

    return rc;
}

//------------------------------------------------------------------------------
RC LwRm::VidHeapOsDescriptor
(
    UINT32 type,
    UINT32 flags,
    UINT32 attr,
    UINT32 attr2,
    UINT64 *physAddr,
    UINT64 size,
    GpuDevice *gpuDevice,
    LwRm::Handle *physMem
)
{
    RC rc;
    LWOS32_PARAMETERS params;

    memset(&params, 0, sizeof(params));
    params.function = LWOS32_FUNCTION_ALLOC_OS_DESCRIPTOR;
    params.data.AllocOsDesc.type = type;
    params.data.AllocOsDesc.flags = flags;
    params.data.AllocOsDesc.attr = attr;
    params.data.AllocOsDesc.attr2 = attr2;
    params.data.AllocOsDesc.descriptor = LW_PTR_TO_LwP64(*physAddr);
    params.data.AllocOsDesc.limit = size;
    params.data.AllocOsDesc.descriptorType =
        LWOS32_DESCRIPTOR_TYPE_OS_PAGE_ARRAY;

    rc = VidHeapControl(&params, gpuDevice);

    if (OK == rc)
    {
        if (physMem)
        {
            *physMem = params.data.AllocOsDesc.hMemory;
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
RC LwRm::FreeOsDescriptor
(
    GpuDevice* pDevice,
    LwRm::Handle physMem
)
{
    RC rc;
    PROFILE(RM_FREE);
    LwRmFree(GetClientHandle(), GetDeviceHandle(pDevice), physMem);

    return rc;
}

static bool ObsoleteIndex(UINT32 Index)
{
    // up to date list at https://engwiki/index.php/Resman/RM_Foundations/Lwrrent_Projects/LwRmControl_API/LwRmConfig-to-LwRmControl_translations
    switch (Index)
    {
        case LW_CFG_PROCESSOR_SPEED:
        case LW_CFG_PCI_ID:
        case LW_CFG_NUMBER_OF_SUBDEVICES:
        case LW_CFG_GRAPHICS_CAPS:
        case LW_CFG_GRAPHICS_CAPS2:
        case LW_CFGEX_GET_GPU_INFO:
        case LW_CFG_BUS_TYPE:
        case LW_CFG_PCI_SUB_ID:
        case LW_CFG_ARCHITECTURE:
        case LW_CFG_IMPLEMENTATION:
        case LW_CFG_DAC_MEMORY_CLOCK:
        case LW_CFG_DAC_GRAPHICS_CLOCK:
        case LW_CFGEX_PERF_MODE:
        case LW_CFGEX_GET_LOGICAL_DEV_EDID:
            // ONLY ENABLED FOR DEBUGGING return true;

        default:
            return false;
    }
}

/**
 *------------------------------------------------------------------------------
 * @function(LwRm::ConfigGet)
 *
 * Get configuration parameter.
 *------------------------------------------------------------------------------
 */

RC LwRm::ConfigGet
(
   UINT32   Index,
   UINT32 * pValue,
   void *   subdev
)
{
    SCOPED_DEV_INST(static_cast<GpuSubdevice*>(subdev));

    MASSERT(pValue != 0);

    if (!GpuPtr()->IsMgrInit())
        return RC::WAS_NOT_INITIALIZED;

    if (ObsoleteIndex(Index))
    {
        Printf(Tee::PriError, "Obsolete index to LwRm::ConfigGet: %d\n",
               Index);
        MASSERT(!"Obsolete index passed to LwRm::ConfigGet");
    }
    Handle h = GetHandleForCfgGetSet(Index, NULL, false, (GpuSubdevice *)subdev);
    UINT32 retval;

    {
        PROFILE(RM_CTRL);
        retval = LwRmConfigGet(GetClientHandle(),
                               h,
                               Index,
                               (LwU32 *)pValue);
    }

    RC rc =  RmApiStatusToModsRC(retval);

    Printf(Tee::PriDebug, "LwRm::ConfigGet idx=%d h=%08x returns %d, data=%08x\n",
            Index, h, UINT32(rc), *pValue);

    return rc;
}

/**
 *------------------------------------------------------------------------------
 * @function(LwRm::ConfigSet)
 *
 * Set configuration parameter.
 *------------------------------------------------------------------------------
 */

RC LwRm::ConfigSet
(
    UINT32   Index,
    UINT32   NewValue,
    UINT32 * pOldValue, // = 0
    GpuSubdevice *subdev
)
{
    SCOPED_DEV_INST(subdev);

    LwU32 OldValue;

    if (ObsoleteIndex(Index))
    {
        Printf(Tee::PriError, "Obsolete index to LwRm::ConfigSet: %d\n",
               Index);
        MASSERT(!"Obsolete index passed to LwRm::ConfigSet");
    }

    UINT32 retval;

    {
        PROFILE(RM_CTRL);
        retval = LwRmConfigSet(GetClientHandle(),
                               GetHandleForCfgGetSet(Index, NULL,
                                                     true, subdev),
                               Index,
                               NewValue,
                               &OldValue);
    }

    if (retval != LW_OK)
    {
        return RmApiStatusToModsRC(retval);
    }

    if (pOldValue != 0)
        *pOldValue = OldValue;
    return OK;
}

/**
 *------------------------------------------------------------------------------
 * @function(LwRm::ConfigSetEx)
 *
 * Extended configuration set.
 *------------------------------------------------------------------------------
 */

RC LwRm::ConfigSetEx
(
   UINT32 Index,
   void * pParamStruct,
   UINT32 ParamSize,
   GpuSubdevice *subdev
)
{
    SCOPED_DEV_INST(subdev);

    MASSERT(pParamStruct != 0);

    if (ObsoleteIndex(Index))
    {
        Printf(Tee::PriError, "Obsolete index to LwRm::ConfigSetEx: %d\n",
               Index);
        MASSERT(!"Obsolete index passed to LwRm::ConfigSetEx");
    }

    UINT32 retval;

    {
        PROFILE(RM_CTRL);
#if defined(TEGRA_MODS)
        if (CheetAh::SocPtr()->HasFeature(Device::SOC_SUPPORTS_DISPLAY_WITH_FE))
        {
            retval = DispLwRmConfigSetEx(m_DisplayClient,
                                     GetHandleForCfgGetSet(Index, pParamStruct,
                                                           true, subdev),
                                     Index,
                                     pParamStruct,
                                     ParamSize);
        }
        else
#endif
        {
            retval = LwRmConfigSetEx(GetClientHandle(),
                                     GetHandleForCfgGetSet(Index, pParamStruct,
                                                           true, subdev),
                                     Index,
                                     pParamStruct,
                                     ParamSize);
        }
    }

    return RmApiStatusToModsRC(retval);
}

/**
 *------------------------------------------------------------------------------
 * @function(LwRm::ConfigGetEx)
 *
 * Extended configuration get.
 *------------------------------------------------------------------------------
 */

RC LwRm::ConfigGetEx
(
   UINT32 Index,
   void * pParamStruct,
   UINT32 ParamSize,
   const GpuSubdevice *subdev
)
{
    SCOPED_DEV_INST(subdev);

    if (!GpuPtr()->IsMgrInit())
        return RC::WAS_NOT_INITIALIZED;

    MASSERT(pParamStruct != 0);

    if (ObsoleteIndex(Index))
    {
        Printf(Tee::PriError, "Obsolete index to LwRm::ConfigGetEx: %d\n",
               Index);
        MASSERT(!"Obsolete index passed to LwRm::ConfigGetEx");
    }

    Handle h = GetHandleForCfgGetSet(Index, pParamStruct, false, subdev);

    UINT32 retval;

    {
        PROFILE(RM_CTRL);
        retval = LwRmConfigGetEx(GetClientHandle(),
                                 h,
                                 Index,
                                 pParamStruct,
                                 ParamSize);
    }

    RC rc =  RmApiStatusToModsRC(retval);

    Printf(Tee::PriDebug, "LwRm::ConfigGetEx idx=%d h=%08x returns %d\n",
            Index, h, UINT32(rc));

    const UINT08 * pData = (const UINT08 *)pParamStruct;
    UINT32 i;
    for(i = 0; i < ParamSize; i++)
    {
        if (0 == i % 16)
            Printf(Tee::PriDebug, "\n\tData  =");
        if (0 == i % 4)
            Printf(Tee::PriDebug, " ");

        Printf(Tee::PriDebug, "%02x ", pData[i]);
    }
    Printf(Tee::PriDebug, "\n");

    return rc;
}

// Mac Only Config Calls.
// Also #ifdef'd in RM
#if defined(LW_MACINTOSH_OSX) && !defined(MACOSX_MFG)

/**
 *------------------------------------------------------------------------------
 * @function(LwRm::OsConfigGet)
 *
 * Get configuration parameter.
 *------------------------------------------------------------------------------
 */

RC LwRm::OsConfigGet
(
    UINT32   Index,
    UINT32 * pValue
)
{
    MASSERT(pValue != 0);

    UINT32 retval = LwRmOsConfigGet(GetClientHandle(),
                                    GetDeviceHandle(),
                                    Index,
                                    (LwU32 *)pValue);
    return RmApiStatusToModsRC(retval);
}

/**
 *------------------------------------------------------------------------------
 * @function(LwRm::OsConfigSet)
 *
 * Set configuration parameter.
 *------------------------------------------------------------------------------
 */

RC LwRm::OsConfigSet
(
    UINT32   Index,
    UINT32   NewValue,
    UINT32 * pOldValue // = 0
)
{
    LwU32 OldValue;

    UINT32 retval = LwRmOsConfigSet(GetClientHandle(),
                                    GetDeviceHandle(),
                                    Index,
                                    NewValue,
                                    &OldValue);
    if (retval != LW_OK)
    {
        return RmApiStatusToModsRC(retval);
    }

    if (pOldValue != 0)
        *pOldValue = OldValue;
    return OK;
}

#endif

/**
 *------------------------------------------------------------------------------
 * @function(LwRm::I2CAccess)
 *
 * I2C access.
 *------------------------------------------------------------------------------
 */

RC LwRm::I2CAccess
(
   void * pCtrlStruct,
   void * subdev
)
{
    SCOPED_DEV_INST(static_cast<GpuSubdevice*>(subdev));

    MASSERT(pCtrlStruct != 0);

#if !defined(TEGRA_MODS)
    UINT32 retval;

    {
        PROFILE(RM_ALLOC);
        retval = LwRmI2CAccess(GetClientHandle(),
                               GetSubdeviceHandle((GpuSubdevice *)subdev),
                               pCtrlStruct);
    }

    return RmApiStatusToModsRC(retval);
#else
    MASSERT(!"RM I2C calls not supported on Android!");
    return RC::SOFTWARE_ERROR;
#endif
}

/**
 *------------------------------------------------------------------------------
 * @function(LwRm::IdleChannels)
 *
 * Wait for the specified channel or channels to finish.
 *------------------------------------------------------------------------------
 */
RC LwRm::IdleChannels
(
   Handle hChannel,
   UINT32 numChannels,
   UINT32 *phClients,
   UINT32 *phDevices,
   UINT32 *phChannels,
   UINT32 flags,
   UINT32 timeout,
   UINT32 deviceInst
)
{
    UINT32 retval;

    {
        PROFILE(RM_FREE);
        retval = LwRmIdleChannels(GetClientHandle(),
                                  DevHandle(deviceInst),
                                  hChannel,
                                  numChannels,
                                  (LwU32 *)phClients,
                                  (LwU32 *)phDevices,
                                  (LwU32 *)phChannels,
                                  flags,
                                  timeout);
    }

    return RmApiStatusToModsRC(retval);
}

//------------------------------------------------------------------------------
RC LwRm::BindContextDma
(
    Handle hChannel,
    Handle hCtxDma
)
{
    MASSERT(ValidateClient(hChannel));
    MASSERT(ValidateClient(hCtxDma));

    // Harmlessly ignore binds of null context DMAs
    if (!hCtxDma)
        return OK;

    UINT32 retval;

    {
        PROFILE(RM_ALLOC);
        retval = LwRmBindContextDma(GetClientHandle(),
                                    hChannel,
                                    hCtxDma);
    }

    // We don't treat it as an error when we're trying to rebind
    if (retval == LW_ERR_STATE_IN_USE)
    {
        return OK;
    }

    return RmApiStatusToModsRC(retval);
}

//------------------------------------------------------------------------------
RC LwRm::MapMemoryDma
(
    Handle hDma,
    Handle hMemory,
    UINT64 offset,
    UINT64 length,
    UINT32 flags,
    UINT64 *pDmaOffset,
    GpuDevice *gpudev
)
{
    MASSERT(ValidateClient(hDma));
    MASSERT(ValidateClient(hMemory));

    UINT32 retval;

    {
        PROFILE(RM_ALLOC);
        retval = LwRmMapMemoryDma(GetClientHandle(),
                                  GetDeviceHandle(gpudev),
                                  hDma,
                                  hMemory,
                                  offset,
                                  length,
                                  flags,
                                  pDmaOffset);
    }

    return RmApiStatusToModsRC(retval);
}

//------------------------------------------------------------------------------
void LwRm::UnmapMemoryDma
(
    Handle hDma,
    Handle hMemory,
    UINT32 flags,
    UINT64 dmaOffset,
    GpuDevice *gpudev
)
{
    MASSERT(ValidateClient(hDma));
    MASSERT(ValidateClient(hMemory));

    UINT32 retval;

    {
        PROFILE(RM_FREE);
        retval = LwRmUnmapMemoryDma(GetClientHandle(),
                                    GetDeviceHandle(gpudev),
                                    hDma,
                                    hMemory,
                                    flags,
                                    dmaOffset);
    }

    MASSERT(retval == LW_OK);
    if (retval != LW_OK)
    {
        Printf(Tee::PriError, "Failed to unmap memory handle 0x%x\n", hMemory);
    }
}

//------------------------------------------------------------------------------
RC LwRm::GetKernelHandle(Handle hMem, void** pKernelMemHandle)
{
#if defined(TEGRA_MODS)
    return RmApiStatusToModsRC(
            LwRmTegraMemHandleFromObject(GetClientHandle(), hMem,
                                         reinterpret_cast<LwRmTegraMemHandle *>(pKernelMemHandle)));
#else
    return RC::UNSUPPORTED_FUNCTION;
#endif
}

//------------------------------------------------------------------------------
TegraRm* LwRm::CheetAh()
{
    return GpuPtr()->GetTegraRm();
}

//! \brief Check if multiple a SLI configuration is valid
//!
//! This function will check to see if the desired SLI configuration
//! is valid.
RC LwRm::ValidateConfiguration
(
    UINT32 NumGpus,
    UINT32 *GpuIds,
    UINT32 *DisplayGpuIndex,
    UINT32 *SliStatus
)
{
    LW0000_CTRL_GPU_GET_SLI_CONFIG_INFO_PARAMS params;
    memset(&params, 0, sizeof(params));

    params.sliStatus = 0;
    params.sliConfig.sliInfo = 0;
    params.sliConfig.displayGpuIndex = 0;
    params.sliConfig.masterGpuIndex = 0;
    params.sliConfig.gpuCount = NumGpus;
    for (UINT32 gpu = 0; gpu < NumGpus; gpu++)
      params.sliConfig.gpuIds[gpu] = GpuIds[gpu];

    RC status = Control(GetClientHandle(),
                        LW0000_CTRL_CMD_GPU_GET_SLI_CONFIG_INFO,
                        &params, sizeof(params));

    if (status == OK)
    {
        if (DisplayGpuIndex)
            *DisplayGpuIndex = params.sliConfig.displayGpuIndex;
        if (SliStatus)
            *SliStatus = params.sliStatus;
    }
    return status;
}

//! Duplicate a handle from another client
RC LwRm::DuplicateMemoryHandle
(
    Handle hSrc,
    Handle hSrcClient,
    Handle *hDst,
    GpuDevice *pGpuDevice
)
{
    *hDst = GenUniqueHandle(GetDeviceHandle(pGpuDevice));

    UINT32 retval;

    {
        PROFILE(RM_CTRL);
        retval = LwRmDupObject(GetClientHandle(),
            GetDeviceHandle(pGpuDevice),
            *hDst,
            hSrcClient,
            hSrc,
            0);
    }

    if (retval != LW_OK)
    {
        DeleteHandle(*hDst);
        *hDst = 0;
    }
    return RmApiStatusToModsRC(retval);
}

RC LwRm::DuplicateMemoryHandle(Handle hSrc, Handle *hDst, GpuDevice *pGpuDevice)
{
    if (ValidateHandleTagAndLib(hSrc))
    {
        MASSERT(m_ClientId != ClientIdFromHandle(hSrc));
    }

    return DuplicateMemoryHandle(hSrc, ClientHandleFromObjectHandle(hSrc),
        hDst, pGpuDevice);
}

//! \brief Return the info. for the each valid SLI configuration
//! SliConfigList must point to a vector of LW0000_CTRL_GPU_SLI_CONFIG
//  structs
RC LwRm::GetValidConfigurations
(
    vector <LW0000_CTRL_GPU_SLI_CONFIG> *SliConfigList
)
{
    RC rc;
    LW0000_CTRL_GPU_GET_VALID_SLI_CONFIGS_PARAMS params = {0};

    params.sliConfigCount = 0;
    params.sliConfigList = LwP64_NULL;
    CHECK_RC(Control(GetClientHandle(),
                     LW0000_CTRL_CMD_GPU_GET_VALID_SLI_CONFIGS,
                     &params, sizeof(params)));

    if (!params.sliConfigCount)
        return OK;

    SliConfigList->resize(params.sliConfigCount);

    // Allocate enough space to store the configuration data
    params.sliStatus = 0;
    params.sliConfigList = LW_PTR_TO_LwP64(&((*SliConfigList)[0]));
    CHECK_RC(Control(GetClientHandle(),
                     LW0000_CTRL_CMD_GPU_GET_VALID_SLI_CONFIGS,
                     &params, sizeof(params)));

    return rc;
}

//! \brief Return the info. for the each invalid SLI configuration
//! SliConfigList must point to a vector of LW0000_CTRL_GPU_SLI_CONFIG
//! structs, and SliIlwalidStatus should point to a paralell vector to
//! indicate the reason why each configuration was rejected
RC LwRm::GetIlwalidConfigurations(vector <LW0000_CTRL_GPU_SLI_CONFIG> *SliConfigList,
                                  vector <UINT32> *SliIlwalidStatus)
{
    LW0000_CTRL_GPU_GET_ILWALID_SLI_CONFIGS_PARAMS params = {0};
    LW0000_CTRL_GPU_GET_SLI_CONFIG_INFO_PARAMS configInfo;
    RC rc;

    params.sliConfigCount = 0;
    params.sliConfigList = LwP64_NULL;
    CHECK_RC(Control(GetClientHandle(),
                     LW0000_CTRL_CMD_GPU_GET_ILWALID_SLI_CONFIGS,
                     &params, sizeof(params)));

    if (!params.sliConfigCount)
        return OK;

    SliConfigList->resize(params.sliConfigCount);
    SliIlwalidStatus->resize(params.sliConfigCount);
    params.sliConfigList = LW_PTR_TO_LwP64(&((*SliConfigList)[0]));

    CHECK_RC(Control(GetClientHandle(),
                     LW0000_CTRL_CMD_GPU_GET_ILWALID_SLI_CONFIGS,
                     &params, sizeof(params)));

    for (UINT32 i = 0; i < SliConfigList->size(); i++)
    {
        memset(&configInfo, 0, sizeof(configInfo));
        // The sliConfig structure is not correctly populated for invalid
        // configurations unless it is requested to be
        configInfo.sliConfig = (*SliConfigList)[i];
        CHECK_RC(LwRmPtr()->Control(LwRmPtr()->GetClientHandle(),
                         LW0000_CTRL_CMD_GPU_GET_SLI_CONFIG_INFO,
                         &configInfo, sizeof(configInfo)));
        (*SliConfigList)[i] = configInfo.sliConfig;
        (*SliIlwalidStatus)[i] = configInfo.sliStatus;
    }

    return rc;
}

//! \brief Link multiple GpuIds into an SLI device (if legal)
//!
//! This function will attempt to link two or more GpuIds into an SLI
//! configuration.  All subdevices being linked should first be freed
//! (as should their parent devices), and after linking all freed
//! subdevices (and their parent devices) should be reallocated.
RC LwRm::LinkGpus
(
    UINT32 NumGpus,
    UINT32 *GpuIds,
    UINT32 DisplayGpuIndex,
    UINT32 *LinkedDeviceInst
)
{
    LW0000_CTRL_GPU_LINK_SLI_DEVICE_PARAMS params;
    memset(&params, 0, sizeof(params));

    params.sliConfig.displayGpuIndex = DisplayGpuIndex;
    params.sliConfig.masterGpuIndex = 0;
    params.sliConfig.gpuCount = NumGpus;
    for (UINT32 gpu = 0; gpu < NumGpus; gpu++)
      params.sliConfig.gpuIds[gpu] = GpuIds[gpu];

    RC status = Control(GetClientHandle(),
                        LW0000_CTRL_CMD_GPU_LINK_SLI_DEVICE,
                        &params, sizeof(params));

    if (status == OK ) *LinkedDeviceInst = params.deviceInstance;
    else *LinkedDeviceInst = 0xffffffff;

    return status;
}

//! \brief Unlink a device (no-op if not in SLI)
//!
//! This function will attempt to unlink a device.  All subdevices
//! being unlinked should first be freed (as should the device itself),
//! and after unlinking all freed subdevices (and their parent devices)
//! should be reallocated.  Unlinking a device with a single subdevice
//! will be a no-op in the RM.
RC LwRm::UnlinkDevice
(
    UINT32 DeviceInst
)
{
    LW0000_CTRL_GPU_UNLINK_SLI_DEVICE_PARAMS params = {0};

    params.deviceInstance = DeviceInst;

    RC status = Control(GetClientHandle(),
                        LW0000_CTRL_CMD_GPU_UNLINK_SLI_DEVICE,
                        &params, sizeof(params));

    return status;
}

//------------------------------------------------------------------------------
RC LwRm::ControlByDevice
(
    const GpuDevice *pGpuDevice,
    UINT32 cmd,
    void   *pParams,
    UINT32 paramsSize
)
{
    MASSERT(pGpuDevice != NULL);
    return Control(GetDeviceHandle(pGpuDevice), cmd, pParams, paramsSize);
}

//------------------------------------------------------------------------------
RC LwRm::ControlBySubdevice
(
    const GpuSubdevice *pGpuSubdevice,
    UINT32 cmd,
    void   *pParams,
    UINT32 paramsSize
)
{
    SCOPED_DEV_INST(pGpuSubdevice);

    MASSERT(pGpuSubdevice != NULL);
    return Control(GetSubdeviceHandle(pGpuSubdevice),
                   cmd, pParams, paramsSize);
}

//------------------------------------------------------------------------------
RC LwRm::ControlSubdeviceChild
(
    const GpuSubdevice *pGpuSubdevice,
    UINT32 ClassId,
    UINT32 Cmd,
    void   *pParams,
    UINT32 ParamsSize
)
{
    SCOPED_DEV_INST(pGpuSubdevice);

    MASSERT(pGpuSubdevice != NULL);
    MASSERT(pParams != NULL);

    LwRm::Handle hObject;
    RC rc;

    // Make sure we don't allocate two instances of ClassId at the same time
    //
    Tasker::MutexHolder mh(m_Mutex);

    // Allocate temporary instance of ClassId and call Control on it
    //
    if (!IsClassSupported(ClassId, pGpuSubdevice->GetParentDevice()))
    {
        return RC::LWRM_ILWALID_CLASS;
    }
    CHECK_RC(Alloc(GetSubdeviceHandle(pGpuSubdevice),
                   &hObject, ClassId, NULL));
    rc = Control(hObject, Cmd, pParams, ParamsSize);
    Free(hObject);
    return rc;
}

//-----------------------------------------------------------------------------
RC LwRm::CreateFbSegment
(
    GpuDevice *pGpudev,
    void *pParams
)
{
    LW0080_CTRL_FB_CREATE_FB_SEGMENT_PARAMS *pCreateFBSegParams =
        (LW0080_CTRL_FB_CREATE_FB_SEGMENT_PARAMS *) pParams;
    pCreateFBSegParams->hMemory =
        GenUniqueHandle(GetDeviceHandle(pGpudev));
    return Control(GetDeviceHandle(pGpudev),
            LW0080_CTRL_CMD_FB_CREATE_FB_SEGMENT,
            pCreateFBSegParams,
            sizeof(*pCreateFBSegParams));
}

//-----------------------------------------------------------------------------
RC LwRm::DestroyFbSegment
(
    GpuDevice *pGpudev,
    void *pParams
)
{
    RC rc;
    LW0080_CTRL_FB_DESTROY_FB_SEGMENT_PARAMS *pDestroyFBSegParams =
        (LW0080_CTRL_FB_DESTROY_FB_SEGMENT_PARAMS *) pParams;
    CHECK_RC(Control(GetDeviceHandle(pGpudev),
            LW0080_CTRL_CMD_FB_DESTROY_FB_SEGMENT,
            pDestroyFBSegParams,
            sizeof(*pDestroyFBSegParams)));
    DeleteHandleWithChildren(pDestroyFBSegParams->hMemory);
    return rc;
}

//------------------------------------------------------------------------------
RC LwRm::Control
(
    Handle hObject,
    UINT32 cmd,
    void   *pParams,
    UINT32 paramsSize
)
{
    MASSERT(hObject);
    UINT32 retval;

    {
        PROFILE(RM_CTRL);

#if defined(LINUX_MFG)
        // Internal call is used to ignore RM locks
        // Copied from lwrmdos.c
        if (m_UseInternalCall)
        {
            LWOS54_PARAMETERS Parms;

            Parms.hClient    = GetClientHandle();
            Parms.hObject    = hObject;
            Parms.cmd        = cmd;
            Parms.params     = LW_PTR_TO_LwP64(pParams);
            Parms.paramsSize = paramsSize;
            Parms.flags      = 0;

            Lw04ControlInternal(&Parms);

            retval = Parms.status;
        }
        else
#endif
        {
#if defined(TEGRA_MODS)
            if (IsRmControlRoutedToDispRmapi(cmd))
            {
                retval = DispLwRmControl(m_DisplayClient,
                                         hObject,
                                         cmd,
                                         pParams,
                                         paramsSize);
            }
            else
#endif
            {
                retval = LwRmControl(GetClientHandle(),
                                     hObject,
                                     cmd,
                                     pParams,
                                     paramsSize);
            }
        }
    }

    RC rc =  RmApiStatusToModsRC(retval);

    Printf(Tee::PriDebug, "LwRm::Control cmd=0x%x obj=%08x returns %d\n",
        cmd, hObject, UINT32(rc));

    if (m_bEnableControlTrace)
    {
        string printString;
        if (pParams != NULL)
        {
            const UINT08 * pData = (const UINT08 *)pParams;
            UINT32 i;
            for(i = 0; i < paramsSize; i++)
            {
                if (0 == i % 16)
                {
                    if (paramsSize > 64)
                        printString += Utility::StrPrintf("\n\tData[%02x]  =", i);
                    else
                        printString += "\n\tData  =";
                }
                if (0 == i % 4)
                    printString += " ";

                printString += Utility::StrPrintf("%02x ", pData[i]);
            }
            printString += "\n";
        }
        Printf(Tee::PriDebug, "%s", printString.c_str());
    }

    return rc;
}

// This function is called from the resman ISR on DOS and sim platforms.
// It records that the event oclwrred and wakes the event thread.
// It is unsafe to do callbacks directly from inside the ISR.
// Note that resman Events can be used to catch an Awaken interrupt to
// avoid polling for a notifier or semaphore.
//
void LwRm::HandleResmanEvent
(
   void * pOsEvent,
   UINT32 GpuInstance
)
{
    // Get the ModsEvent.  Since this function should only be called
    // on DOS/sim platforms in which the ModsEvent and OsEvent are the
    // same, we should be able to simply cast the pOsEvent pointer.
    //
    ModsEvent *pEvent = (ModsEvent*)pOsEvent;
    MASSERT(Tasker::GetOsEvent(pEvent, GetClientHandle(), 0) == pOsEvent);

    // Set the ModsEvent.  If the event is owned by an EventThread,
    // then use EventThread::SetCountingEvent() to set the event so
    // that we keep track of how many times the event was called.
    //
    EventThread *pEventThread = EventThread::FindEventThread(pEvent);
    if (pEventThread)
        pEventThread->SetCountingEvent();
    else
        Tasker::SetEvent(pEvent);
}

//------------------------------------------------------------------------------
void LwRm::PrintHandleTree(UINT32 Parent, UINT32 Indent)
{
    Tasker::MutexHolder mh(m_Mutex);

    for (auto h: m_Handles)
    {
        if (h.parent == Parent)
        {
            for (UINT32 i = 0; i < Indent; i++)
                Printf(Tee::PriNormal, "  ");
            Printf(Tee::PriNormal, "0x%08X\n", h.handle);
            PrintHandleTree(h.handle, Indent+1);
        }
    }
}

//------------------------------------------------------------------------------
bool LwRm::IsClassSupported(UINT32 ClassID, GpuDevice *pGpudev)
{
    UINT32 dummy;
    return OK == GetFirstSupportedClass(1, &ClassID, &dummy, pGpudev);
}

//------------------------------------------------------------------------------
RC LwRm::GetFirstSupportedClass
(
    UINT32 nClasses,
    const UINT32 *pClassIDs,
    UINT32 *pReturnID,
    GpuDevice *pGpudev
)
{
    MASSERT(nClasses > 0);
    MASSERT(pClassIDs);
    MASSERT(pReturnID);

    vector<LwU32> classList;
    RC rc;

    CHECK_RC(GetSupportedClasses(&classList, pGpudev));

    // Scan for the first supported class
    //
    for (UINT32 ii = 0; ii < nClasses; ++ii)
    {
        if (classList.end() != find(classList.begin(), classList.end(),
                                    static_cast<LwU32>(pClassIDs[ii])))
        {
            // This one is supported, return it
            //
            *pReturnID = pClassIDs[ii];
            return RC::OK;
        }
    }

    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
RC LwRm::GetSupportedClasses
(
    vector<UINT32>* classList,
    GpuDevice* pGpuDevice
)
{
    vector<UINT32> tempClassList;
    RC rc;

    // RM doesn't differentiate chip families for AModel. It will always return
    // all the known classes. Hence use EscapeRead to query the supported class
    // list.
    // Some SW used-only class, like LW01_TIMER and etc. are not included in
    // the classList returned by EscapeRead. For such classes call RM and get
    // the list of supported classes. Append it towards the end of the list so
    // that when checking for the first supported class, the ones returned by
    // EscapeRead are given preference
    if (Platform::GetSimulationMode() == Platform::Amodel)
    {
        CHECK_RC(GetSupportedClassesFromAmodel(&tempClassList, pGpuDevice));
        classList->insert(classList->end(), tempClassList.begin(), tempClassList.end());
        tempClassList.clear();
    }

    // Get supported classList for all platforms via RM ctrl call
    CHECK_RC(GetSupportedClassesFromRm(&tempClassList, pGpuDevice));
    classList->insert(classList->end(), tempClassList.begin(), tempClassList.end());

    return rc;
}

//------------------------------------------------------------------------------
RC LwRm::GetSupportedClassesFromAmodel
(
    vector<UINT32>* classList,
    GpuDevice* pGpuDevice
)
{
    MASSERT(classList);
    MASSERT(Platform::GetSimulationMode() == Platform::Amodel);

    // RM doesn't differentiate chip families for AModel and will always return
    // all the known classes hence MODS uses EscapeReads
    // GetSupportedClassCount/GetSupportedClassList to get the list of
    // supported classes in AModel
    bool isAmodelSupportedGetClassList = false;
    UINT32 isGetSupportedClassCountSupported = 0;

    if ((pGpuDevice->EscapeReadBuffer("supported/GetSupportedClassCount", 0,
        sizeof(LwU32), &isGetSupportedClassCountSupported) == 0) &&
        (isGetSupportedClassCountSupported != 0))
    {
        UINT32 isGetSupportedClassListSupported = 0;
        if ((pGpuDevice->EscapeReadBuffer("supported/GetSupportedClassList", 0,
            sizeof(LwU32), &isGetSupportedClassListSupported) == 0) &&
            (isGetSupportedClassListSupported != 0))
        {
            isAmodelSupportedGetClassList = true;
        }
    }

    if (isAmodelSupportedGetClassList)
    {
        UINT32 classCount = 0;
        pGpuDevice->EscapeReadBuffer("GetSupportedClassCount", 0, sizeof(LwU32),
            &classCount);
        if (classCount == 0)
        {
            Printf(Tee::PriError,
                "No supported classes in this version of Amodel\n");
            return RC::UNSUPPORTED_FUNCTION;
        }

        classList->resize(classCount, 0);
        pGpuDevice->EscapeReadBuffer("GetSupportedClassList", 0,
            sizeof(LwU32)*classCount, &(classList->front()));
    }
    return RC::OK;
}

//------------------------------------------------------------------------------
RC LwRm::GetSupportedClassesFromRm
(
    vector<UINT32>* classList,
    GpuDevice* pGpuDevice
)
{
    MASSERT(pGpuDevice);
    Handle hClient = GetClientHandle();
    Handle hDev = GetDeviceHandle(pGpuDevice);
    LW0080_CTRL_GPU_GET_CLASSLIST_V2_PARAMS classListParams = {0};

    {
        PROFILE(RM_CTRL);
        UINT32 status = LwRmControl(hClient,
                                    hDev,
                                    LW0080_CTRL_CMD_GPU_GET_CLASSLIST_V2,
                                    &classListParams,
                                    sizeof (classListParams));

       // Check for RM error
       //
       MASSERT(status == LW_OK);
       if (status != LW_OK)
       {
            Printf(Tee::PriNormal,
                  "GetSupportedClassesFromRm: GET_CLASSLIST failed: 0x%x\n", status);
            return RC::UNSUPPORTED_FUNCTION;
       }

        classList->resize(classListParams.numClasses, 0);

        for (UINT32 i = 0; i < classListParams.numClasses; i++)
        {
            classList->data()[i] = classListParams.classList[i];
        }
    }

    // Get more classes on CheetAh
    //
    if (pGpuDevice->GetSubdevice(0)->IsSOC())
    {
        const vector<UINT32> tegraClasses = CheetAh::SocPtr()->GetClassList();
        classList->insert(classList->end(), tegraClasses.begin(), tegraClasses.end());

#if defined(TEGRA_MODS)
        RC rc;
        CHECK_RC(GetDispRmSupportedClasses(hDev, classList));
#endif
    }

    return RC::OK;
}

//------------------------------------------------------------------------------
// LwRm object, properties, and methods.
//------------------------------------------------------------------------------

JS_CLASS(LwRm);

static SObject LwRm_Object
(
   "LwRm",
   LwRmClass,
   0,
   0,
   "Resource manager architecture interface."
);

P_(Lwrm_Get_ClientHandle);
static SProperty LwRm_ClientHandle
(
   LwRm_Object,
   "ClientHandle",
   0,
   0,
   Lwrm_Get_ClientHandle,
   0,
   0,
   "Client handle."
);

P_(Lwrm_Get_ClientCount);
P_(Lwrm_Set_ClientCount);
static SProperty Lwrm_ClientCount
(
  LwRm_Object,
  "ClientCount",
  0,
  0,
  Lwrm_Get_ClientCount,
  Lwrm_Set_ClientCount,
  0,
  "The number of valid RM clients."
);

P_(Lwrm_Get_FreeClients);
P_(Lwrm_Set_FreeClients);
static SProperty Lwrm_FreeClients
(
  LwRm_Object,
  "FreeClients",
  0,
  0,
  Lwrm_Get_FreeClients,
  Lwrm_Set_FreeClients,
  0,
  "The number of free (uninitialized) RM clients."
);

P_(Lwrm_Set_ControlTrace);
static SProperty LwRm_ControlTrace
(
  LwRm_Object,
  "DumpControlTrace",
  0,
  0,
  0,
  Lwrm_Set_ControlTrace,
  0,
  "Level of detail in dumping RM Control Trace."
);

P_(Lwrm_Get_DeviceHandle);
static SProperty LwRm_DeviceHandle
(
   LwRm_Object,
   "DeviceHandle",
   0,
   0,
   Lwrm_Get_DeviceHandle,
   0,
   0,
   "Device handle."
);

C_(LwRm_ConfigGet);
static STest LwRm_ConfigGet
(
   LwRm_Object,
   "ConfigGet",
   C_LwRm_ConfigGet,
   2,
   "Get a configuration parameter."
);

C_(LwRm_ConfigSet);
static STest LwRm_ConfigSet
(
   LwRm_Object,
   "ConfigSet",
   C_LwRm_ConfigSet,
   3,
   "Set a configuration parameter."
);

C_(LwRm_RunStressTest);
static STest LwRm_RunStressTest
(
   LwRm_Object,
   "RunStressTest",
   C_LwRm_RunStressTest,
   2,
   "Run the resman's internal stress test, return pass/fail."
);

C_(LwRm_I2CAccess);
static STest LwRm_I2CAccess
(
   LwRm_Object,
   "I2CAccess",
   C_LwRm_I2CAccess,
   5,
   "Access an I2C device."
);

C_(LwRm_GetBuildVersion);
static STest LwRm_GetBuildVersion
(
    LwRm_Object,
    "GetBuildVersion",
    C_LwRm_GetBuildVersion,
    1,
    "Get an array of [version_string, title_string, build_cl_num, official_cl_num]."
);

//
// Implementation.
//

// SProperty
P_(Lwrm_Get_ClientHandle)
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   if (OK != JavaScriptPtr()->ToJsval(LwRmPtr()->GetClientHandle(), pValue))
   {
      JS_ReportError(pContext, "Failed to get LwRm.ClientHandle.");
      *pValue = JSVAL_NULL;
      return JS_FALSE;
   }

   return JS_TRUE;
}

// SProperty
P_(Lwrm_Get_DeviceHandle)
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   static Deprecation depr("LwRm.DeviceHandle", "4/20/2019",
                           "Use Lwrm.GetDeviceHandle(GpuDevice) instead\n");
   if (!depr.IsAllowed(pContext))
       return JS_FALSE;

   GpuDevMgr* pGpuDevMgr = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr);
   if (OK != JavaScriptPtr()->ToJsval(LwRmPtr()->GetDeviceHandle(pGpuDevMgr->GetFirstGpuDevice()), pValue))
   {
      JS_ReportError(pContext, "Failed to get LwRm.DeviceHandle.");
      *pValue = JSVAL_NULL;
      return JS_FALSE;
   }

   return JS_TRUE;
}

JS_SMETHOD_LWSTOM(LwRm,
                  GetDeviceHandle,
                  1,
                  "Get the device handle for a particular GpuDevice")
{
    STEST_HEADER(1, 1, "Usage: LwRm.GetDeviceHandle(GpuDevice);\n");
    STEST_ARG(0, JSObject*, pJsObj);

    JsGpuDevice *pJsGpuDevice = nullptr;
    if ((pJsGpuDevice = JS_GET_PRIVATE(JsGpuDevice,
                                       pContext,
                                       pJsObj,
                                       "GpuDevice")) == nullptr)
    {
        JS_ReportError(pContext, "Invalid JsGpuDevice\n");
        return JS_FALSE;
    }

    if (OK != JavaScriptPtr()->ToJsval(LwRmPtr()->GetDeviceHandle(pJsGpuDevice->GetGpuDevice()), pReturlwalue))
    {
        JS_ReportError(pContext, "Error in LwRm.GetDeviceHandle().\n");
        return JS_FALSE;
    }

    return JS_TRUE;
}

// SProperty
P_(Lwrm_Set_ControlTrace)
{
    bool dumpControlTrace;
    if (OK != JavaScriptPtr()->FromJsval(*pValue, &dumpControlTrace))
    {
       JS_ReportError(pContext, "Failed to set LwRm.DumpControlTrace");
       return JS_FALSE;
    }
    LwRmPtr()->EnableControlTrace(dumpControlTrace);
    return JS_TRUE;
}

// SProperty
P_(Lwrm_Get_ClientCount)
{
  if (OK != JavaScriptPtr()->ToJsval(LwRmPtr::GetValidClientCount(), pValue))
  {
     JS_ReportError(pContext, "Failed to get LwRm.MaxClients" );
     *pValue = JSVAL_NULL;
     return JS_FALSE;
  }
  return JS_TRUE;
}
P_(Lwrm_Set_ClientCount)
{
    UINT32        ClientCount;

    if (OK != JavaScriptPtr()->FromJsval(*pValue, &ClientCount))
    {
       JS_ReportError(pContext, "Failed to set LwRm.ClientCount");
       return JS_FALSE;
    }
    if (OK != LwRmPtr::SetValidClientCount(ClientCount))
    {
       JS_ReportError(pContext, "Error Setting ClientCount");
       return JS_FALSE;
    }
    return JS_TRUE;
}

// SProperty
P_(Lwrm_Get_FreeClients)
{
  if (OK != JavaScriptPtr()->ToJsval(LwRmPtr::GetFreeClientCount(), pValue))
  {
     JS_ReportError(pContext, "Failed to get LwRm.FreeClients" );
     *pValue = JSVAL_NULL;
     return JS_FALSE;
  }
  return JS_TRUE;
}
P_(Lwrm_Set_FreeClients)
{
    UINT32        ClientCount;

    if (OK != JavaScriptPtr()->FromJsval(*pValue, &ClientCount))
    {
       JS_ReportError(pContext, "Failed to set LwRm.FreeClients");
       return JS_FALSE;
    }
    if (OK != LwRmPtr::SetFreeClientCount(ClientCount))
    {
       JS_ReportError(pContext, "Error Setting FreeClients");
       return JS_FALSE;
    }
    return JS_TRUE;
}

// STest
C_(LwRm_ConfigGet)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   JavaScriptPtr pJavaScript;

   // Check the arguments.
   UINT32     Index;
   JSObject * pReturlwals;

   if
   (
         (NumArguments != 2)
      || (OK != pJavaScript->FromJsval(pArguments[0], &Index))
      || (OK != pJavaScript->FromJsval(pArguments[1], &pReturlwals))
   )
   {
      JS_ReportError(pContext, "Usage: LwRm.ConfigGet(index, [value])");

      return JS_FALSE;
   }

   RC     rc = OK;
   UINT32 Value;

   GpuSubdevice* pSubdev = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr)->GetFirstGpu();
   if (OK != (rc = LwRmPtr()->ConfigGet(Index, &Value, pSubdev)))
      RETURN_RC(rc);

   if (OK != (rc = pJavaScript->SetElement(pReturlwals, 0, Value)))
      RETURN_RC(rc);

   RETURN_RC(OK);
}

// STest
C_(LwRm_RunStressTest)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   JavaScriptPtr pJavaScript;

   // Check the arguments.
   UINT32     LwClkHz;
   UINT32     MClkHz;

   if
   (
         (NumArguments != 2)
      || (OK != pJavaScript->FromJsval(pArguments[0], &LwClkHz))
      || (OK != pJavaScript->FromJsval(pArguments[1], &MClkHz))
   )
   {
      JS_ReportError(pContext, "Usage: LwRm.RunStressTest(LwClkHz, MClkHz)");

      return JS_FALSE;
   }

   RETURN_RC(RC::GPU_STRESS_TEST_FAILED);
}

// STest
C_(LwRm_ConfigSet)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   JavaScriptPtr pJavaScript;

   // Check the arguments.
   UINT32     Index;
   UINT32     NewValue;
   JSObject * pReturlwals;

   if
   (
         (NumArguments != 3)
      || (OK != pJavaScript->FromJsval(pArguments[0], &Index))
      || (OK != pJavaScript->FromJsval(pArguments[1], &NewValue))
      || (OK != pJavaScript->FromJsval(pArguments[2], &pReturlwals))
   )
   {
      JS_ReportError(pContext, "Usage: LwRm.ConfigSet(index, new value, "
         "[old value])");

      return JS_FALSE;
   }

   RC     rc = OK;
   UINT32 OldValue;

   GpuSubdevice* pSubdev = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr)->GetFirstGpu();
   if (OK != (rc = LwRmPtr()->ConfigSet(Index, NewValue, &OldValue, pSubdev)))
      RETURN_RC(rc);

   if (OK != (rc = pJavaScript->SetElement(pReturlwals, 0, OldValue)))
      RETURN_RC(rc);

   RETURN_RC(OK);
}

// STest
C_(LwRm_I2CAccess)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   JavaScriptPtr pJavaScript;

   // Check the arguments.
   UINT32     Command;
   UINT32     Port;
   UINT32     Flags;
   UINT32     DataIn;
   JSObject * pReturlwals;

   if
   (
         (NumArguments !=5)
      || (OK != pJavaScript->FromJsval(pArguments[0], &Command))
      || (OK != pJavaScript->FromJsval(pArguments[1], &Port))
      || (OK != pJavaScript->FromJsval(pArguments[2], &Flags))
      || (OK != pJavaScript->FromJsval(pArguments[3], &DataIn))
      || (OK != pJavaScript->FromJsval(pArguments[4], &pReturlwals))
   )
   {
      JS_ReportError(pContext, "Usage: LwRm.I2CAccess("
         "command, port, flags, data in, [data out])");

      return JS_FALSE;
   }

   // Store the token state (semaphore).
   static UINT32 s_Token = 0;
   LwU8 data[4];

   LW2080_CTRL_I2C_ACCESS_PARAMS I2CControl =
      {
          s_Token
         ,Command
         ,Port
         ,Flags
         ,0
         ,0
         ,0
         ,0
      };

   data[0] = LwU8(DataIn & 0xFF);
   data[1] = LwU8((DataIn >> 8)  & 0xFF);
   data[2] = LwU8((DataIn >> 16) & 0xFF);
   data[3] = LwU8((DataIn >> 24) & 0xFF);

   I2CControl.data = LW_PTR_TO_LwP64(data); // XXX64, assumes little endian.

   RC rc = OK;

   GpuSubdevice* pSubdev = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr)->GetFirstGpu();
   if (OK != (rc = LwRmPtr()->I2CAccess(&I2CControl, pSubdev)))
   {
      RETURN_RC(rc);
   }

   s_Token = I2CControl.token;
   UINT32 RetVal = 0;

   RetVal |= data[0];
   RetVal |= data[1] << 8;
   RetVal |= data[2] << 16;
   RetVal |= data[3] << 24;

   if (OK != (rc = pJavaScript->SetElement(pReturlwals, 0, RetVal)))
   {
      RETURN_RC(rc);
   }

   RETURN_RC(OK);

}

C_(LwRm_GetBuildVersion)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   JavaScriptPtr pJs;

   // Check the arguments.
   JSObject * pReturlwals;

   if
   (
         (NumArguments !=1)
      || (OK != pJs->FromJsval(pArguments[0], &pReturlwals))
   )
   {
      JS_ReportError(pContext, "Usage: rc = LwRm.GetBuildVersion([ver_str,title_str,build_cl_num,official_cl_num])");
      return JS_FALSE;
   }

    RC rc;
    LwRmPtr pLwRm;

    LW0000_CTRL_SYSTEM_GET_BUILD_VERSION_PARAMS params;
    memset(&params, 0, sizeof(params));

    // First learn how much memory to allocate for the strings.
    C_CHECK_RC(pLwRm->Control(
            pLwRm->GetClientHandle(),
            LW0000_CTRL_CMD_SYSTEM_GET_BUILD_VERSION,
            &params,
            sizeof(params)));

    vector<char> VersionBuffer(params.sizeOfStrings, 0);
    vector<char> TitleBuffer(params.sizeOfStrings, 0);
    vector<char> DriverVersionBuffer(params.sizeOfStrings, 0);

    // Alloc memory for the strings and colwert to LwP64 to make the char *
    // compatible with 64 bit systems.
    params.pVersionBuffer = LW_PTR_TO_LwP64(&VersionBuffer[0]);
    params.pTitleBuffer   = LW_PTR_TO_LwP64(&TitleBuffer[0]);
    params.pDriverVersionBuffer = LW_PTR_TO_LwP64(&DriverVersionBuffer[0]);

    // Call again to get the strings.
    C_CHECK_RC(pLwRm->Control(
            pLwRm->GetClientHandle(),
            LW0000_CTRL_CMD_SYSTEM_GET_BUILD_VERSION,
            &params,
            sizeof(params)));

    // Build the return structure.
    string stmp = (const char *) LwP64_VALUE(params.pVersionBuffer);
    MASSERT(stmp.size());
    C_CHECK_RC(pJs->SetElement(pReturlwals, 0, stmp));
    stmp = (const char *) LwP64_VALUE(params.pTitleBuffer);
    MASSERT(stmp.size());
    C_CHECK_RC(pJs->SetElement(pReturlwals, 1, stmp));
    C_CHECK_RC(pJs->SetElement(pReturlwals, 2, (UINT32) params.changelistNumber));
    C_CHECK_RC(pJs->SetElement(pReturlwals, 3, (UINT32) params.officialChangelistNumber));

    RETURN_RC(rc);
}

LwRm::DisableRcWatchdog::DisableRcWatchdog(GpuSubdevice* pGpuSubdev)
    : m_pSubdev(pGpuSubdev), m_Disabled(false)
{
    SCOPED_DEV_INST(pGpuSubdev);

    MASSERT(m_pSubdev != NULL);
    LwRmPtr pLwRm;
    Tasker::MutexHolder mutexHolder(pLwRm->GetMutex());

    if (s_DisableCount.count(m_pSubdev->GetGpuId()) == 0)
        s_DisableCount[m_pSubdev->GetGpuId()] = 0;

    int& DisableCount = s_DisableCount[m_pSubdev->GetGpuId()];

    if (DisableCount == 0)
    {
        LW2080_CTRL_RC_GET_WATCHDOG_INFO_PARAMS params = {0};
        if (OK != pLwRm->ControlBySubdevice(
                m_pSubdev, LW2080_CTRL_CMD_RC_GET_WATCHDOG_INFO,
                &params, sizeof(params)))
        {
            Printf(Tee::PriNormal,
                   "Warning: Unable to get RC Watchdog info\n");
            MASSERT(!"Generic assertion failure<refer to previous message>.");
            return;
        }
        if ((params.watchdogStatusFlags &
             LW2080_CTRL_RC_GET_WATCHDOG_INFO_FLAGS_DISABLED)
            || !(params.watchdogStatusFlags &
                 LW2080_CTRL_RC_GET_WATCHDOG_INFO_FLAGS_RUNNING))
        {
            Printf(Tee::PriLow,
                   "RC Watchdog is already disabled - nothing to do\n");
            return;
        }
        if (OK != pLwRm->ControlBySubdevice(
                m_pSubdev, LW2080_CTRL_CMD_RC_DISABLE_WATCHDOG, NULL, 0))
        {
            Printf(Tee::PriNormal, "Warning: Unable to disable RC Watchdog\n");
            MASSERT(!"Generic assertion failure<refer to previous message>.");
            return;
        }
        Printf(Tee::PriLow, "RC Watchdog disabled.\n");
    }
    ++DisableCount;
    m_Disabled = true;
}

LwRmPtr::LwRmPtr()
{
    m_pLwRmClient = &s_LwRmClients[0];
}

LwRmPtr::LwRmPtr(UINT32 Client)
{
    if (s_ValidClientCount + s_FreeClientCount > Client)
    {
        m_pLwRmClient = &s_LwRmClients[Client];
    }
    else
    {
        m_pLwRmClient = NULL;
        MASSERT(!"Invalid RM Client!!");
    }
}

LwRm::DisableRcWatchdog::~DisableRcWatchdog()
{
    LwRmPtr pLwRm;
    Tasker::MutexHolder mutexHolder(pLwRm->GetMutex());

    if (m_Disabled)
    {
        SCOPED_DEV_INST(m_pSubdev);

        int& DisableCount = s_DisableCount[m_pSubdev->GetGpuId()];
        MASSERT(DisableCount > 0);
        if (DisableCount == 1)
        {
            LW2080_CTRL_RC_GET_WATCHDOG_INFO_PARAMS params = {0};
            if (OK != pLwRm->ControlBySubdevice(
                    m_pSubdev, LW2080_CTRL_CMD_RC_GET_WATCHDOG_INFO,
                    &params, sizeof(params)))
            {
                Printf(Tee::PriError,
                       "Error: Unable to get RC Watchdog info!\n");
                MASSERT(!"Generic assertion failure<refer to previous message>.");
                return;
            }
            MASSERT(params.watchdogStatusFlags &
                    LW2080_CTRL_RC_GET_WATCHDOG_INFO_FLAGS_DISABLED);
            if (OK != pLwRm->ControlBySubdevice(
                    m_pSubdev, LW2080_CTRL_CMD_RC_RELEASE_WATCHDOG_REQUESTS, NULL, 0))
            {
                Printf(Tee::PriError,
                       "Error: Unable to re-enable RC Watchdog!\n");
                MASSERT(!"Generic assertion failure<refer to previous message>.");
                return;
            }
            Printf(Tee::PriLow, "RC Watchdog enabled.\n");
        }
        DisableCount--;
    }
}

// On CheetAh we don't use RM and there is no way to lock the GPU driver
#ifdef TEGRA_MODS
#define RmAcquireSema() TRUE
#define RmCondAcquireSema() TRUE
#define RmReleaseSema()
#endif

LwRm::ExclusiveLockRm::~ExclusiveLockRm()
{
    if (m_LockCount)
    {
        m_pLwRm->m_UseInternalCall = false;
        RmReleaseSema();
        m_LockCount = 0;
    }
}

RC LwRm::ExclusiveLockRm::AcquireLock()
{
    const INT32 prevValue = Cpu::AtomicAdd(&m_LockCount, 1);

    if (prevValue == 0)
    {
        if (RmAcquireSema() != TRUE)
        {
            Cpu::AtomicAdd(&m_LockCount, -1);
            return RC::SOFTWARE_ERROR;
        }
        m_pLwRm->m_UseInternalCall = true;
    }
    return OK;
}

RC LwRm::ExclusiveLockRm::TryAcquireLock()
{
    const INT32 prevValue = Cpu::AtomicAdd(&m_LockCount, 1);

    if (prevValue == 0)
    {
        if (RmCondAcquireSema() != TRUE)
        {
            Cpu::AtomicAdd(&m_LockCount, -1);
            return RC::SOFTWARE_ERROR;
        }
        m_pLwRm->m_UseInternalCall = true;
    }
    return OK;
}

void LwRm::ExclusiveLockRm::ReleaseLock()
{
    const INT32 prevValue = Cpu::AtomicAdd(&m_LockCount, -1);
    if (prevValue == 1)
    {
        m_pLwRm->m_UseInternalCall = false;
        RmReleaseSema();
    }
}

RC LwRmPtr::SetValidClientCount(UINT32 ClientCount)
{
    if ((ClientCount + s_FreeClientCount) > s_MaxClients)
    {
        Printf(Tee::PriError, "%d RM Clients exceeds the maximum of %d\n",
               ClientCount, s_MaxClients);
        return RC::UNSUPPORTED_FUNCTION;
    }
    if (s_LwRmClients[0].GetClientHandle())
    {
        Printf(Tee::PriError,
               "Cannot change RM client count after allocation!!!\n");
        return RC::SOFTWARE_ERROR;
    }
    s_ValidClientCount = ClientCount;
    return OK;
}

RC LwRmPtr::SetFreeClientCount(UINT32 ClientCount)
{
    if ((ClientCount + s_ValidClientCount) > s_MaxClients)
    {
        Printf(Tee::PriError, "%d RM Clients exceeds the maximum of %d\n",
               ClientCount, s_MaxClients);
        return RC::UNSUPPORTED_FUNCTION;
    }
    if (s_LwRmClients[0].GetClientHandle())
    {
        Printf(Tee::PriError,
               "Cannot change RM client count after allocation!!!\n");
        return RC::SOFTWARE_ERROR;
    }
    s_FreeClientCount = ClientCount;
    return OK;
}

LwRm * LwRmPtr::GetFreeClient()
{
    const UINT32 firstFreeClient = s_ValidClientCount;
    for (UINT32 i = 0; i < (firstFreeClient + s_FreeClientCount); i++)
    {
        if (s_LwRmClients[i].IsFreed())
            return &s_LwRmClients[i];
    }
    return nullptr;
}

LwRm::DisableRcRecovery::DisableRcRecovery(GpuSubdevice* pGpuSubdev)
    : m_pSubdev(pGpuSubdev), m_Disabled(false)
{
    MASSERT(m_pSubdev != NULL);

    SCOPED_DEV_INST(pGpuSubdev);

    // Lwrrently this is not supported on CheetAh
    if (pGpuSubdev->IsSOC())
    {
        return;
    }

    LwRmPtr pLwRm;
    Tasker::MutexHolder mutexHolder(pLwRm->GetMutex());

    if (s_DisableCount.count(m_pSubdev->GetGpuId()) == 0)
        s_DisableCount[m_pSubdev->GetGpuId()] = 0;
    int& DisableCount = s_DisableCount[m_pSubdev->GetGpuId()];

    if (DisableCount == 0)
    {
        LW2080_CTRL_CMD_RC_RECOVERY_PARAMS params = {0};
        if (OK != pLwRm->ControlBySubdevice(
                m_pSubdev, LW2080_CTRL_CMD_GET_RC_RECOVERY,
                &params, sizeof(params)))
        {
            Printf(Tee::PriNormal,
                   "Warning: Unable to get RC Recovery info\n");
            MASSERT(!"Generic assertion failure<refer to previous message>.");
            return;
        }

        if (params.rcEnable == LW2080_CTRL_CMD_RC_RECOVERY_DISABLED)
        {
            Printf(Tee::PriLow,
                   "RC Recovery is already disabled - nothing to do\n");
            return;
        }

        params.rcEnable = LW2080_CTRL_CMD_RC_RECOVERY_DISABLED;
        if (OK != pLwRm->ControlBySubdevice(
                m_pSubdev, LW2080_CTRL_CMD_SET_RC_RECOVERY,
                &params, sizeof(params)))
        {
            Printf(Tee::PriNormal, "Warning: Unable to disable RC Recovery\n");
            MASSERT(!"Generic assertion failure<refer to previous message>.");
            return;
        }
        Printf(Tee::PriLow, "RC Recovery disabled.\n");
    }
    ++DisableCount;
    m_Disabled = true;
}

LwRm::DisableRcRecovery::~DisableRcRecovery()
{
    LwRmPtr pLwRm;
    Tasker::MutexHolder mutexHolder(pLwRm->GetMutex());

    if (m_Disabled)
    {
        SCOPED_DEV_INST(m_pSubdev);

        int& DisableCount = s_DisableCount[m_pSubdev->GetGpuId()];
        MASSERT(DisableCount > 0);
        if (DisableCount == 1)
        {
            LW2080_CTRL_CMD_RC_RECOVERY_PARAMS params = {0};
            params.rcEnable = LW2080_CTRL_CMD_RC_RECOVERY_ENABLED;

            if (OK != pLwRm->ControlBySubdevice(
                    m_pSubdev, LW2080_CTRL_CMD_SET_RC_RECOVERY,
                    &params, sizeof(params)))
            {
                Printf(Tee::PriNormal, "Warning: Unable to enable RC Recovery\n");
                MASSERT(!"Generic assertion failure<refer to previous message>.");
                return;
            }
            Printf(Tee::PriLow, "RC Recovery enabled.\n");
        }
        DisableCount--;
    }
}

//------------------------------------------------------------------------------
// Class for temporarily disabling tracing of Control calls
//------------------------------------------------------------------------------
volatile INT32 LwRm::DisableControlTrace::s_DisableCount = 0;
volatile bool  LwRm::DisableControlTrace::s_bEnableState = false;

LwRm::DisableControlTrace::DisableControlTrace()
{
    const INT32 prevRefCount = Cpu::AtomicAdd(&s_DisableCount, 1);
    if (prevRefCount == 0)
    {
        s_bEnableState = LwRmPtr()->GetEnableControlTrace();
        LwRmPtr()->EnableControlTrace(false);
    }
}
LwRm::DisableControlTrace::~DisableControlTrace()
{
    const INT32 prevRefCount = Cpu::AtomicAdd(&s_DisableCount, -1);
    MASSERT(prevRefCount > 0);
    if (prevRefCount == 1)
    {
        LwRmPtr()->EnableControlTrace(s_bEnableState);
    }
}

//------------------------------------------------------------------------------
RC LwRm::MapMemoryInternal
(
    Handle   MemoryHandle,
    UINT64   Offset,
    UINT64   Length,
    void  ** ppAddress,
    UINT32   Flags,
    GpuSubdevice *gpusubdev,
    UINT32   Class
)
{
    SCOPED_DEV_INST(gpusubdev);

    RC rc;

#if defined(TEGRA_MODS)
    if (IsDisplayClientAllocated() &&
        IsDisplayClass(Class))
    {
        rc = MapDispMemory(MemoryHandle,
                           Offset,
                           Length,
                           ppAddress,
                           Flags,
                           gpusubdev);
    }
    else
#endif
    {
        rc = MapMemory(MemoryHandle,
                       Offset,
                       Length,
                       ppAddress,
                       Flags,
                       gpusubdev);
    }

    return rc;
}
