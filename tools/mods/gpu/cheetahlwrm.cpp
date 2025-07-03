/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// LW resource manager architecture interface.
// Implementation of CheetAh only methods in LwRm class

#include "class/cl0073.h"   // LW04_DISPLAY_COMMON
#include "class/cl5070.h"   // LW50_DISPLAY
#include "class/clc372sw.h" // LWC372_DISPLAY_SW
#include "class/clc670.h"   // LWC670_DISPLAY
#include "class/clc673.h"   // LWC673_DISP_CAPABILITIES
#include "class/clc67a.h"   // LWC67A_LWRSOR_IMM_CHANNEL_PIO
#include "class/clc67b.h"   // LWC67B_WINDOW_IMM_CHANNEL_DMA
#include "class/clc67d.h"   // LWC67D_CORE_CHANNEL_DMA
#include "class/clc77d.h"   // LWC77D_CORE_CHANNEL_DMA
#include "class/clc67e.h"   // LWC67E_WINDOW_CHANNEL_DMA
#include "class/clc870.h"   // LWC870_DISPLAY
#include "class/clc873.h"   // LWC873_DISP_CAPABILITIES
#include "class/clc87a.h"   // LWC87A_LWRSOR_IMM_CHANNEL_PIO
#include "class/clc87b.h"   // LWC87B_WINDOW_IMM_CHANNEL_DMA
#include "class/clc87d.h"   // LWC87D_CORE_CHANNEL_DMA
#include "class/clc87e.h"   // LWC67E_WINDOW_CHANNEL_DMA
#include "class/cl9171.h"   // LW9171_DISP_SF_USER

#include "class/cl0002.h"   // LW01_CONTEXT_DMA
#include "class/cl00f3.h"   // LW01_MEMORY_FLA
#include "class/cl003e.h"   // LW01_MEMORY_SYSTEM
#include "class/cl0070.h"   // LW01_MEMORY_VIRTUAL/LW01_MEMORY_SYSTEM_DYNAMIC

#include "ctrl/ctrl0000.h"
#include "ctrl/ctrl0080.h"
#include "ctrl/ctrl2080.h"
#include "ctrl/ctrl0002.h" // LW01_CONTEXT_DMA_FROM_MEMORY

#include "core/include/gpu.h"
#include "core/include/lwrm.h"
#include "core/include/simclk.h"
#include "dispLwRmApi.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "cheetah/include/tegrasocdevice.h"
#include "cheetah/include/tegrasocdeviceptr.h"
#include "rmapi_tegra.h"

// This probably needs to be removed. Bug 2457569
#define PROFILE(x) SimClk::InstWrapper wrapper(SimClk::InstGroup::x);

//------------------------------------------------------------------------------
RC LwRm::BindDispContextDma
(
    Handle hChannel,
    Handle hCtxDma
)
{
    MASSERT(ValidateClient(hChannel));
    MASSERT(ValidateClient(hCtxDma));

    if (!CheetAh::SocPtr()->HasFeature(Device::SOC_SUPPORTS_DISPLAY_WITH_FE))
        return RC::UNSUPPORTED_FUNCTION;

    // Harmlessly ignore binds of null context DMAs
    if (!hCtxDma)
        return RC::OK;

    UINT32 retval = LW_OK;

    {
        PROFILE(RM_CTRL);
        // Allow ContextDma promoted to DynamicMemory to be silently bound for compatibility
        if (GetClassID(hCtxDma) != LW01_MEMORY_SYSTEM_DYNAMIC)
        {
            LW0002_CTRL_BIND_CONTEXTDMA_PARAMS bindParams = {};
            bindParams.hChannel = hChannel;

            retval = DispLwRmControl(m_DisplayClient,
                                     hCtxDma,
                                     LW0002_CTRL_CMD_BIND_CONTEXTDMA,
                                     &bindParams,
                                     sizeof(bindParams));
        }
    }

    // We don't treat it as an error when we're trying to rebind
    if (retval == LW_ERR_STATE_IN_USE)
    {
        return RC::OK;
    }

    return RmApiStatusToModsRC(retval);
}

//------------------------------------------------------------------------------
void LwRm::UnmapDispMemory
(
    Handle          MemoryHandle,
    volatile void * Address,
    UINT32          Flags,
    GpuSubdevice  * gpusubdev
)
{
    MASSERT(MemoryHandle != 0);
    MASSERT(ValidateClient(MemoryHandle));

    if (!CheetAh::SocPtr()->HasFeature(Device::SOC_SUPPORTS_DISPLAY_WITH_FE))
    {
        MASSERT(!"Function not supported");
        return;
    }

    // Harmlessly ignore unmaps of null pointers
    if (!Address)
        return;

    Handle hSubdev = GetSubdeviceHandle(gpusubdev);

    UINT32 retval = LW_OK;

    {
        PROFILE(RM_FREE);
        retval = DispLwRmUnmapMemory(m_DisplayClient,
                                     hSubdev,
                                     MemoryHandle,
                                     const_cast<void *>(Address),
                                     Flags);
    }

    MASSERT(retval == LW_OK);
    if (retval != LW_OK)
    {
        Printf(Tee::PriDebug, "Failed to unmap memory handle 0x%x\n", MemoryHandle);
    }
}

//------------------------------------------------------------------------------
RC LwRm::MapDispMemory
(
    Handle   MemoryHandle,
    UINT64   Offset,
    UINT64   Length,
    void  ** pAddress,
    UINT32   Flags,
    GpuSubdevice *gpusubdev
)
{
    MASSERT(MemoryHandle != 0);
    MASSERT(Length       != 0);
    MASSERT(pAddress     != 0);
    MASSERT(ValidateClient(MemoryHandle));

    if (!CheetAh::SocPtr()->HasFeature(Device::SOC_SUPPORTS_DISPLAY_WITH_FE))
        return RC::UNSUPPORTED_FUNCTION;

    if (gpusubdev && !gpusubdev->HasFeature(Device::GPUSUB_SUPPORTS_SYSMEM_REFLECTED_MAPPING))
    {
        Flags = FLD_SET_DRF(OS33, _FLAGS, _MAPPING, _DIRECT, Flags);
    }

    Handle hSubdev = GetSubdeviceHandle(gpusubdev);

    UINT32 retval = LW_OK;

    {
        PROFILE(RM_ALLOC);
        retval = DispLwRmMapMemory(m_DisplayClient,
                                   hSubdev,
                                   MemoryHandle,
                                   Offset,
                                   Length,
                                   pAddress,
                                   Flags);
    }

    return RmApiStatusToModsRC(retval);
}

// This function is meant to be called while allocating a display specific surface only
RC LwRm::AllocDisplayMemory
(
    Handle                      *  pRtnMemoryHandle,
    UINT32                         Class,
    LW_MEMORY_ALLOCATION_PARAMS *  pAllocMemParams,
    GpuDevice                      *gpudev
)
{
    MASSERT(Class           != 0);
    MASSERT(pAllocMemParams != 0);
    MASSERT(pRtnMemoryHandle);

    if (!CheetAh::SocPtr()->HasFeature(Device::SOC_SUPPORTS_DISPLAY_WITH_FE))
        return RC::UNSUPPORTED_FUNCTION;

    *pRtnMemoryHandle = GenUniqueHandle(GetDeviceHandle(gpudev), Lib::DISP_RMAPI);
    pAllocMemParams->owner = m_DisplayClient;

    UINT32 result = LW_OK;
    {
        PROFILE(RM_ALLOC);
        result = DispLwRmAlloc(m_DisplayClient,
                               GetDeviceHandle(gpudev),
                               *pRtnMemoryHandle,
                               Class,
                               pAllocMemParams);
    }

    if (result != LW_OK)
    {
        DeleteHandle(*pRtnMemoryHandle);
        *pRtnMemoryHandle = 0;
        return RmApiStatusToModsRC(result);
    }

    return OK;
}

// This function is meant to be called while allocating a display specific surface only
RC LwRm::AllocDisplayContextDma
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

    if (!CheetAh::SocPtr()->HasFeature(Device::SOC_SUPPORTS_DISPLAY_WITH_FE))
        return RC::UNSUPPORTED_FUNCTION;

    // Always set the hash-table disable flag
    Flags = FLD_SET_DRF(OS03, _FLAGS, _HASH_TABLE, _DISABLE, Flags);

    // We use the memory object as the "parent" of the context DMA, since
    // freeing a memory object automatically frees the context DMAs created
    // on the memory.
    *pRtnDmaHandle = GenUniqueHandle(Memory, Lib::DISP_RMAPI);

    // RM's deprecated API translation layer internally allocates the object as
    // a sibling of memory, so copy that behavior here.
    Handle Parent = MapToDispRmHandle(m_Handles.GetParent(Memory));

    UINT32 retval;

    {
        PROFILE(RM_ALLOC);

        if (GetClassID(Memory) == LW01_MEMORY_SYSTEM_DYNAMIC)
        {
            LW_MEMORY_VIRTUAL_ALLOCATION_PARAMS allocVirtualParams = {};

            allocVirtualParams.offset   = Offset;
            allocVirtualParams.limit    = Limit;
            allocVirtualParams.hVASpace = 0;

            retval = DispLwRmAlloc(m_DisplayClient,
                                   Parent,
                                   *pRtnDmaHandle,
                                   LW01_MEMORY_VIRTUAL,
                                   &allocVirtualParams);
        }
        else
        {
            LW_CONTEXT_DMA_ALLOCATION_PARAMS allocParams = {};

            allocParams.flags   = Flags;
            allocParams.hMemory = Memory;
            allocParams.offset  = Offset;
            allocParams.limit   = Limit;

            retval = DispLwRmAlloc(m_DisplayClient,
                                   Parent,
                                   *pRtnDmaHandle,
                                   LW01_CONTEXT_DMA,
                                   &allocParams);
        }
    }

    if (retval != LW_OK)
    {
        DeleteHandle(*pRtnDmaHandle);
        *pRtnDmaHandle = 0;
        return RmApiStatusToModsRC(retval);
    }

    return OK;
}

RC LwRm::AllocDisplayClient()
{
    LwU32 clientHandle;
    UINT32 retval;
    vector<UINT32> clientHandlesToFree;

    if (!CheetAh::SocPtr()->HasFeature(Device::SOC_SUPPORTS_DISPLAY_WITH_FE))
        return RC::UNSUPPORTED_FUNCTION;

    if (IsDisplayClientAllocated())
        return OK;

    Tasker::MutexHolder mh(m_Mutex);

    // Rather crude way to ensure uniqueness of client handle
    do
    {
        {
            PROFILE(RM_ALLOC);
            retval = DispLwRmAllocRoot(&clientHandle);
        }

        if (retval != LW_OK)
            return RmApiStatusToModsRC(retval);

        clientHandlesToFree.push_back(clientHandle);
    } while (m_Handles.GetParent(clientHandle) != m_Handles.Invalid);

    m_DisplayClient = clientHandle;
    // Don't ilwoke free for the unique client handle
    clientHandlesToFree.pop_back();

    for (auto& handle: clientHandlesToFree)
    {
        PROFILE(RM_FREE);
        DispLwRmFree(handle,
                     LW01_NULL_OBJECT,
                     handle);
    }

    m_Handles.RegisterForeignHandle(clientHandle, LW01_NULL_OBJECT);

    return OK;
}

//------------------------------------------------------------------------------
RC LwRm::DispRmInit(GpuDevice *pGpudev)
{
    RC rc;
    UINT32 retval;

    if (!CheetAh::SocPtr()->HasFeature(Device::SOC_SUPPORTS_DISPLAY_WITH_FE))
        return RC::UNSUPPORTED_FUNCTION;

    if (pGpudev == nullptr)
        return RC::SOFTWARE_ERROR;

    CHECK_RC(AllocDisplayClient());

    LW0000_CTRL_GPU_ATTACH_IDS_PARAMS gpuAttachIdsParams = {};
    gpuAttachIdsParams.gpuIds[0] = LW0000_CTRL_GPU_ATTACH_ALL_PROBED_IDS;
    gpuAttachIdsParams.gpuIds[1] = LW0000_CTRL_GPU_ILWALID_ID;
    CHECK_RC(RmApiStatusToModsRC(DispLwRmControl(m_DisplayClient,
                                                 m_DisplayClient,
                                                 LW0000_CTRL_CMD_GPU_ATTACH_IDS,
                                                 &gpuAttachIdsParams,
                                                 sizeof(gpuAttachIdsParams))));

    LW0000_CTRL_GPU_GET_ATTACHED_IDS_PARAMS getGpuAttachedIdsParams = {};
    CHECK_RC(RmApiStatusToModsRC(DispLwRmControl(m_DisplayClient,
                                                 m_DisplayClient,
                                                 LW0000_CTRL_CMD_GPU_GET_ATTACHED_IDS,
                                                 &getGpuAttachedIdsParams,
                                                 sizeof(getGpuAttachedIdsParams))));

    // TODO if display RM detects DGPUs, may have to update this
    UINT32 devIdx = 0;
    for (; (getGpuAttachedIdsParams.gpuIds[devIdx] != LW0000_CTRL_GPU_ILWALID_ID) &&
           (devIdx < LW0000_CTRL_GPU_MAX_ATTACHED_GPUS); devIdx++);

    if (devIdx != 1)
    {
        Printf(Tee::PriError, "Detected %u GPUs. Expected single iGPU in CheetAh\n", devIdx);
        return RC::SOFTWARE_ERROR;
    }

    // Allocate device and sub-device handles in Disp RMAPI. Use same handles as
    // rmapi_tegra so that mods and other LwRm clients can continue to maintain
    // a single handle to represent a given Gpu device/sub device
    LW0080_ALLOC_PARAMETERS lw0080Params = {};
    CHECK_RC(PrepareAllocParams(m_DisplayClient, pGpudev->GetDeviceInst(), &lw0080Params));
    Handle deviceHandle = DevHandle(lw0080Params.deviceId);

    {
        PROFILE(RM_ALLOC);
        retval = DispLwRmAlloc(m_DisplayClient, m_DisplayClient, deviceHandle,
                           LW01_DEVICE_0, &lw0080Params);
    }

    if (retval != LW_OK)
    {
        Printf(Tee::PriError, "Failed to allocate device %u\n", pGpudev->GetDeviceInst());
        return RmApiStatusToModsRC(retval);
    }
    m_ReplicatedHandles.push_back(deviceHandle);

    const UINT32 numOfSubdevs = pGpudev->GetNumSubdevices();
    for (UINT32 subDevReference = 0; subDevReference < numOfSubdevs; subDevReference++)
    {
        Handle hSubdev   = SubdevHandle(pGpudev->GetDeviceInst(), subDevReference + 1);
        LW2080_ALLOC_PARAMETERS lw2080Params = {};
        lw2080Params.subDeviceId = subDevReference;

        {
            PROFILE(RM_ALLOC);
            retval = DispLwRmAlloc(m_DisplayClient, deviceHandle, hSubdev,
                               LW20_SUBDEVICE_0, &lw2080Params);
        }

        if (retval != LW_OK)
        {
            Printf(Tee::PriError, "Failed to allocate subdevice %u\n", subDevReference - 1);
            return RmApiStatusToModsRC(retval);
        }
        m_ReplicatedHandles.push_back(hSubdev);
    }
    return OK;
}

//------------------------------------------------------------------------------
void LwRm::FreeDispRmHandle(Handle objectHandle)
{
    MASSERT(ValidateClient(objectHandle));
    MASSERT(m_Handles.GetParent(objectHandle) != m_Handles.Invalid);

    Tasker::MutexHolder mh(m_Mutex);
    auto IsRoutedToDispRmapi = [&]()->bool
        {
            if (!IsDisplayClientAllocated())
                return false;

            // If rmapi_tegra client handle is being freed, free display client handle as well
            if (m_Client == objectHandle)
                return true;

            // Handles replicated in both libraries need to be freed in both libraries
            if (std::find(m_ReplicatedHandles.begin(), m_ReplicatedHandles.end(), objectHandle)
                != m_ReplicatedHandles.end())
                return true;

            // If handle info indicates it's allocated by Disp Rmapi lib, route it
            if (Lib::DISP_RMAPI == GetLibFromHandle(objectHandle))
                return true;

            return false;
        };

    // Handle not allocated in DispRmapi. No need to free it
    if (!IsRoutedToDispRmapi())
        return;

    // Map rmapi_tegra client handle to disp client handle
    const Handle ParentHandle = m_Handles.GetParent(objectHandle);
    Handle handleToFree = MapToDispRmHandle(objectHandle);
    UINT32 status;

    if (handleToFree == m_DisplayClient)
    {
        LW0000_CTRL_GPU_DETACH_IDS_PARAMS params = {};
        params.gpuIds[0] = LW0000_CTRL_GPU_DETACH_ALL_ATTACHED_IDS;
        params.gpuIds[1] = LW0000_CTRL_GPU_ILWALID_ID;
        DispLwRmControl(m_DisplayClient, m_DisplayClient, LW0000_CTRL_CMD_GPU_DETACH_IDS,
                        &params, sizeof(params));
    }

    {
        PROFILE(RM_FREE);
        status = DispLwRmFree(m_DisplayClient,
                              MapToDispRmHandle(ParentHandle),
                              handleToFree);
    }

    if (status != LW_OK)
    {
        Printf(Tee::PriError, "Failed to delete disp RM handle: 0x%08x!\n", handleToFree);
        MASSERT(!"Failed to delete disp RM handle");
    }

    if (handleToFree == m_DisplayClient)
    {
        if (CountChildren(m_DisplayClient))
        {
            // Display client handle is used only internally and it cannot have any children
            // Resources are assigned as children to m_Client handle and we transform m_Client
            // to m_DisplayClient while ilwoking DispRmapi calls (to minimize impact)
            MASSERT(!"Children detected for display client handle in object tree");
        }
        DeleteHandle(m_DisplayClient);
    }
}

//------------------------------------------------------------------------------
RC LwRm::GetDispRmSupportedClasses(Handle hDev, vector<UINT32> *pClassList)
{
    RC rc;
    UINT32 retval = LW_OK;
    LW0080_CTRL_GPU_GET_CLASSLIST_V2_PARAMS classListParams = {};

    if (pClassList == nullptr)
        return RC::SOFTWARE_ERROR;

    auto& classList = *pClassList;

    if (IsDisplayClientAllocated())
    {
        // Get number of classes
        {
            PROFILE(RM_CTRL);
            retval = DispLwRmControl(m_DisplayClient,
                                     hDev,
                                     LW0080_CTRL_CMD_GPU_GET_CLASSLIST_V2,
                                     &classListParams,
                                     sizeof(classListParams));
        }
        CHECK_RC(RmApiStatusToModsRC(retval));

        // Get supported class list
        UINT32 nonDispClassListSize = classList.size();
        classList.resize(nonDispClassListSize + classListParams.numClasses, 0);

        for (UINT32 i = 0; i < classListParams.numClasses; i++)
        {
            classList[nonDispClassListSize + i] = classListParams.classList[i];
        }
    }
    return rc;
}

bool LwRm::IsDisplayClass(UINT32 ctrlClass) const
{
    switch (ctrlClass)
    {
        case LW04_DISPLAY_COMMON:
        case LW50_DISPLAY:
        case LWC372_DISPLAY_SW:
        case LWC670_DISPLAY:
        case LWC673_DISP_CAPABILITIES:
        case LWC67D_CORE_CHANNEL_DMA:
        case LWC67A_LWRSOR_IMM_CHANNEL_PIO:
        case LWC67B_WINDOW_IMM_CHANNEL_DMA:
        case LWC67E_WINDOW_CHANNEL_DMA:
        case LWC77D_CORE_CHANNEL_DMA:
        case LWC870_DISPLAY:
        case LWC873_DISP_CAPABILITIES:
        case LWC87A_LWRSOR_IMM_CHANNEL_PIO:
        case LWC87B_WINDOW_IMM_CHANNEL_DMA:
        case LWC87D_CORE_CHANNEL_DMA:
        case LWC87E_WINDOW_CHANNEL_DMA:
        case LW9171_DISP_SF_USER:
            return true;

        default:
            return false;
    }
}

bool LwRm::IsRmControlRoutedToDispRmapi(UINT32 cmd) const
{
    if (!IsDisplayClientAllocated())
        return false;

    if (cmd == LW0080_CTRL_CMD_GPU_GET_DISPLAY_OWNER ||
        cmd == LW2080_CTRL_CMD_EVENT_SET_NOTIFICATION ||
        cmd == LW2080_CTRL_CMD_EVENT_SET_MEMORY_NOTIFIES)
        return true;

    const UINT32 ctrlClass = (cmd >> 16) & 0xFFFF;
    return IsDisplayClass(ctrlClass);
}

RC LwRm::AllocDisplaySystemMemory
(
    Handle                      *  pRtnMemoryHandle,
    LW_MEMORY_ALLOCATION_PARAMS *  pAllocMemParams,
    GpuDevice                      *gpudev
)
{
    return AllocDisplayMemory(pRtnMemoryHandle, LW01_MEMORY_SYSTEM,
                              pAllocMemParams, gpudev);
}

RC LwRm::AllocDispEvent
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

    *pRtnEventHandle = GenUniqueHandle(ParentHandle, Lib::DISP_RMAPI);

    UINT32 result;

    {
        PROFILE(RM_ALLOC);
        result = DispLwRmAllocEvent(m_DisplayClient,
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

    return RC::OK;
}

RC LwRm::AllocDisp
(
    Handle   ParentHandle,
    Handle * pRtnObjectHandle,
    UINT32   Class,
    void *   Parameters
)
{
    MASSERT(ValidateClient(ParentHandle));
    UINT32 retval = LW_OK;

    *pRtnObjectHandle = GenUniqueHandle(ParentHandle, Lib::DISP_RMAPI);
    PROFILE(RM_ALLOC);
    retval = DispLwRmAlloc(m_DisplayClient,
                           MapToDispRmHandle(ParentHandle),
                           *pRtnObjectHandle,
                           Class,
                           Parameters);

    if (retval != LW_OK)
    {
        DeleteHandle(*pRtnObjectHandle);
        *pRtnObjectHandle = 0;
        return RmApiStatusToModsRC(retval);
    }

    return RC::OK;
}

UINT32 LwRm::GetClassID(Handle ObjectHandle)
{
    LW0000_CTRL_CLIENT_GET_HANDLE_INFO_PARAMS classIdParams = {};
    UINT32 retval = LW_OK;

    classIdParams.hObject = ObjectHandle;
    classIdParams.index   = LW0000_CTRL_CMD_CLIENT_GET_HANDLE_INFO_INDEX_CLASSID;

    retval = DispLwRmControl(m_DisplayClient,
                             m_DisplayClient,
                             LW0000_CTRL_CMD_CLIENT_GET_HANDLE_INFO,
                             &classIdParams,
                             sizeof(classIdParams));

    if (retval != LW_OK)
        return 0xFFFFFFFF;

    return LwU64_LO32(classIdParams.data.iResult);
}

RC LwRm::GetTegraMemFd(Handle memoryHandle, INT32 *pMemFd)
{
    return LwRmTegraFdFromObject(GetClientHandle(), memoryHandle, pMemFd);
}
