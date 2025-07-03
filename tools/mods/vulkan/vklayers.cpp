/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Warning: DO NOT INCLUDE "vkmain.h" here as it renames vk* functions to disabledVk*
#include "vulkan/vk_layer.h"
#include "vulkan/vk_icd.h"
#include "vk_layer_dispatch_table.h"
#include "vklayers.h"
#include "core/include/dllhelper.h"
#include "core/include/xp.h"
#include <unordered_map>

namespace
{
    struct LayerFile
    {
        const char *vulkanLayerName;
        const char *filename;
    };

    struct LayerLibrary
    {
        string layerName;
        void *handle = nullptr;
        PFN_vkGetInstanceProcAddr getInstanceProcAddr = nullptr;
        PFN_vkGetDeviceProcAddr getDeviceProcAddr = nullptr;
    };

    vector<LayerLibrary> s_LayerLibraries;
    unordered_map<VkInstance, vector<const LayerLibrary*>> s_EnabledLayersInInstance;

    VkResult LoadLayerLibraries(UINT32 num, const LayerFile *layerFile)
    {
        RC rc;

        for (UINT32 fileIdx = 0; fileIdx < num; fileIdx++)
        {
            LayerLibrary library;

            library.layerName = layerFile[fileIdx].vulkanLayerName;

            const char* const filename = layerFile[fileIdx].filename;

            if (RC::OK != Utility::LoadDLLFromPackage(
                        filename,
                        &library.handle,
                        Xp::UnloadDLLOnExit | Xp::DeepBindDLL))
            {
                return VK_ERROR_LAYER_NOT_PRESENT;
            }
            library.getInstanceProcAddr =
                reinterpret_cast<PFN_vkGetInstanceProcAddr>(
                    Xp::GetDLLProc(library.handle, "vkGetInstanceProcAddr"));
            if (library.getInstanceProcAddr == nullptr)
            {
                Printf(Tee::PriError,
                    "vkGetInstanceProcAddr function not found in %s\n",
                    filename);
                return VK_ERROR_LAYER_NOT_PRESENT;
            }

            library.getDeviceProcAddr =
                reinterpret_cast<PFN_vkGetDeviceProcAddr>(
                    Xp::GetDLLProc(library.handle, "vkGetDeviceProcAddr"));
            if (library.getDeviceProcAddr == nullptr)
            {
                Printf(Tee::PriError,
                    "vkGetDeviceProcAddr function not found in %s\n",
                    filename);
                return VK_ERROR_LAYER_NOT_PRESENT;
            }

            s_LayerLibraries.push_back(library);
        }

        return VK_SUCCESS;
    }

    void FillInstanceDispatchTableFromGetInstanceProcAddr
    (
        VkLayerInstanceDispatchTable *pIDT,
        VkInstance instance,
        PFN_vkGetInstanceProcAddr pGetInstanceProcAddr
    )
    {
    #define GET_INSTANCE_FUNCTION(name) pIDT->name = PFN_vk##name(pGetInstanceProcAddr(instance, "vk" #name))
        GET_INSTANCE_FUNCTION(DestroyInstance);
        GET_INSTANCE_FUNCTION(EnumeratePhysicalDevices);
        GET_INSTANCE_FUNCTION(GetPhysicalDeviceFeatures);
        GET_INSTANCE_FUNCTION(GetPhysicalDeviceFormatProperties);
        GET_INSTANCE_FUNCTION(GetPhysicalDeviceImageFormatProperties);
        GET_INSTANCE_FUNCTION(GetPhysicalDeviceProperties);
        GET_INSTANCE_FUNCTION(GetPhysicalDeviceQueueFamilyProperties);
        GET_INSTANCE_FUNCTION(GetPhysicalDeviceMemoryProperties);
        GET_INSTANCE_FUNCTION(GetInstanceProcAddr);
        GET_INSTANCE_FUNCTION(CreateDevice);
        GET_INSTANCE_FUNCTION(EnumerateDeviceExtensionProperties);
        GET_INSTANCE_FUNCTION(EnumerateDeviceLayerProperties);
        GET_INSTANCE_FUNCTION(GetPhysicalDeviceSparseImageFormatProperties);

        GET_INSTANCE_FUNCTION(DestroySurfaceKHR);
        GET_INSTANCE_FUNCTION(GetPhysicalDeviceSurfaceSupportKHR);
        GET_INSTANCE_FUNCTION(GetPhysicalDeviceSurfaceCapabilitiesKHR);
        GET_INSTANCE_FUNCTION(GetPhysicalDeviceSurfaceFormatsKHR);
        GET_INSTANCE_FUNCTION(GetPhysicalDeviceSurfacePresentModesKHR);

        GET_INSTANCE_FUNCTION(GetPhysicalDeviceDisplayPropertiesKHR);
        GET_INSTANCE_FUNCTION(GetPhysicalDeviceDisplayPlanePropertiesKHR);
        GET_INSTANCE_FUNCTION(GetDisplayPlaneSupportedDisplaysKHR);
        GET_INSTANCE_FUNCTION(GetDisplayModePropertiesKHR);
        GET_INSTANCE_FUNCTION(CreateDisplayModeKHR);
        GET_INSTANCE_FUNCTION(GetDisplayPlaneCapabilitiesKHR);
        GET_INSTANCE_FUNCTION(CreateDisplayPlaneSurfaceKHR);

    #ifdef VK_USE_PLATFORM_XLIB_KHR
        GET_INSTANCE_FUNCTION(CreateXlibSurfaceKHR);
        GET_INSTANCE_FUNCTION(GetPhysicalDeviceXlibPresentationSupportKHR);
    #endif

    #ifdef VK_USE_PLATFORM_XCB_KHR
        GET_INSTANCE_FUNCTION(CreateXcbSurfaceKHR);
        GET_INSTANCE_FUNCTION(GetPhysicalDeviceXcbPresentationSupportKHR);
    #endif

    #ifdef VK_USE_PLATFORM_WAYLAND_KHR
        GET_INSTANCE_FUNCTION(CreateWaylandSurfaceKHR);
        GET_INSTANCE_FUNCTION(GetPhysicalDeviceWaylandPresentationSupportKHR);
    #endif

    #ifdef VK_USE_PLATFORM_MIR_KHR
        GET_INSTANCE_FUNCTION(CreateMirSurfaceKHR);
        GET_INSTANCE_FUNCTION(GetPhysicalDeviceMirPresentationSupportKHR);
    #endif

    #ifdef VK_USE_PLATFORM_ANDROID_KHR
        GET_INSTANCE_FUNCTION(CreateAndroidSurfaceKHR);
    #endif

    #ifdef VK_USE_PLATFORM_WIN32_KHR
        GET_INSTANCE_FUNCTION(CreateWin32SurfaceKHR);
        GET_INSTANCE_FUNCTION(GetPhysicalDeviceWin32PresentationSupportKHR);
    #endif

        GET_INSTANCE_FUNCTION(GetPhysicalDeviceFeatures2);
        GET_INSTANCE_FUNCTION(GetPhysicalDeviceProperties2);
        GET_INSTANCE_FUNCTION(GetPhysicalDeviceFormatProperties2);
        GET_INSTANCE_FUNCTION(GetPhysicalDeviceImageFormatProperties2);
        GET_INSTANCE_FUNCTION(GetPhysicalDeviceQueueFamilyProperties2);
        GET_INSTANCE_FUNCTION(GetPhysicalDeviceMemoryProperties2);
        GET_INSTANCE_FUNCTION(GetPhysicalDeviceSparseImageFormatProperties2);

        GET_INSTANCE_FUNCTION(GetPhysicalDeviceFeatures2KHR);
        GET_INSTANCE_FUNCTION(GetPhysicalDeviceProperties2KHR);
        GET_INSTANCE_FUNCTION(GetPhysicalDeviceFormatProperties2KHR);
        GET_INSTANCE_FUNCTION(GetPhysicalDeviceImageFormatProperties2KHR);
        GET_INSTANCE_FUNCTION(GetPhysicalDeviceQueueFamilyProperties2KHR);
        GET_INSTANCE_FUNCTION(GetPhysicalDeviceMemoryProperties2KHR);
        GET_INSTANCE_FUNCTION(GetPhysicalDeviceSparseImageFormatProperties2KHR);

        GET_INSTANCE_FUNCTION(GetPhysicalDeviceExternalBufferPropertiesKHR);
        GET_INSTANCE_FUNCTION(GetPhysicalDeviceExternalSemaphorePropertiesKHR);
        GET_INSTANCE_FUNCTION(GetPhysicalDeviceExternalFencePropertiesKHR);

    #ifdef vkGetPhysicalDeviceSurfaceCapabilities2KHR
        GET_INSTANCE_FUNCTION(GetPhysicalDeviceSurfaceCapabilities2KHR);
        GET_INSTANCE_FUNCTION(GetPhysicalDeviceSurfaceFormats2KHR);
    #endif

        GET_INSTANCE_FUNCTION(CreateDebugReportCallbackEXT);
        GET_INSTANCE_FUNCTION(DestroyDebugReportCallbackEXT);
        GET_INSTANCE_FUNCTION(DebugReportMessageEXT);
        GET_INSTANCE_FUNCTION(GetPhysicalDeviceExternalImageFormatPropertiesLW);
        GET_INSTANCE_FUNCTION(GetPhysicalDevicePresentRectanglesKHR);

    #ifdef VK_USE_PLATFORM_VI_NN
        GET_INSTANCE_FUNCTION(CreateViSurfaceNN);
    #endif

        GET_INSTANCE_FUNCTION(EnumeratePhysicalDeviceGroupsKHR);
        GET_INSTANCE_FUNCTION(ReleaseDisplayEXT);

    #ifdef VK_USE_PLATFORM_XLIB_XRANDR_EXT
        GET_INSTANCE_FUNCTION(AcquireXlibDisplayEXT);
        GET_INSTANCE_FUNCTION(GetRandROutputDisplayEXT);
    #endif

        GET_INSTANCE_FUNCTION(GetPhysicalDeviceSurfaceCapabilities2EXT);

    #ifdef VK_USE_PLATFORM_IOS_MVK
        GET_INSTANCE_FUNCTION(CreateIOSSurfaceMVK);
    #endif

    #ifdef VK_USE_PLATFORM_MACOS_MVK
        GET_INSTANCE_FUNCTION(CreateMacOSSurfaceMVK);
    #endif

        GET_INSTANCE_FUNCTION(GetPhysicalDeviceCooperativeMatrixPropertiesLW);

    #undef GET_INSTANCE_FUNCTION
    }

    void FillDeviceDispatchTableFromGetDeviceProcAddr
    (
        VkLayerDispatchTable *pDDT,
        VkDevice device,
        PFN_vkGetDeviceProcAddr pGetDeviceProcAddr
    )
    {
    #define GET_DEVICE_FUNCTION(name) pDDT->name = PFN_vk##name(pGetDeviceProcAddr(device, "vk" #name))

        GET_DEVICE_FUNCTION(DestroyDevice);
        GET_DEVICE_FUNCTION(GetDeviceQueue);
        GET_DEVICE_FUNCTION(QueueSubmit);
        GET_DEVICE_FUNCTION(QueueWaitIdle);
        GET_DEVICE_FUNCTION(DeviceWaitIdle);
        GET_DEVICE_FUNCTION(AllocateMemory);
        GET_DEVICE_FUNCTION(FreeMemory);
        GET_DEVICE_FUNCTION(MapMemory);
        GET_DEVICE_FUNCTION(UnmapMemory);
        GET_DEVICE_FUNCTION(FlushMappedMemoryRanges);
        GET_DEVICE_FUNCTION(IlwalidateMappedMemoryRanges);
        GET_DEVICE_FUNCTION(GetDeviceMemoryCommitment);
        GET_DEVICE_FUNCTION(BindBufferMemory);
        GET_DEVICE_FUNCTION(BindImageMemory);
        GET_DEVICE_FUNCTION(GetBufferMemoryRequirements);
        GET_DEVICE_FUNCTION(GetImageMemoryRequirements);
        GET_DEVICE_FUNCTION(GetImageSparseMemoryRequirements);
        GET_DEVICE_FUNCTION(QueueBindSparse);
        GET_DEVICE_FUNCTION(CreateFence);
        GET_DEVICE_FUNCTION(DestroyFence);
        GET_DEVICE_FUNCTION(ResetFences);
        GET_DEVICE_FUNCTION(GetFenceStatus);
        GET_DEVICE_FUNCTION(WaitForFences);
        GET_DEVICE_FUNCTION(CreateSemaphore);
        GET_DEVICE_FUNCTION(DestroySemaphore);
        GET_DEVICE_FUNCTION(CreateEvent);
        GET_DEVICE_FUNCTION(DestroyEvent);
        GET_DEVICE_FUNCTION(GetEventStatus);
        GET_DEVICE_FUNCTION(SetEvent);
        GET_DEVICE_FUNCTION(ResetEvent);
        GET_DEVICE_FUNCTION(CreateQueryPool);
        GET_DEVICE_FUNCTION(DestroyQueryPool);
        GET_DEVICE_FUNCTION(GetQueryPoolResults);
        GET_DEVICE_FUNCTION(CreateBuffer);
        GET_DEVICE_FUNCTION(DestroyBuffer);
        GET_DEVICE_FUNCTION(CreateBufferView);
        GET_DEVICE_FUNCTION(DestroyBufferView);
        GET_DEVICE_FUNCTION(CreateImage);
        GET_DEVICE_FUNCTION(DestroyImage);
        GET_DEVICE_FUNCTION(GetImageSubresourceLayout);
        GET_DEVICE_FUNCTION(CreateImageView);
        GET_DEVICE_FUNCTION(DestroyImageView);
        GET_DEVICE_FUNCTION(CreateShaderModule);
        GET_DEVICE_FUNCTION(DestroyShaderModule);
        GET_DEVICE_FUNCTION(CreatePipelineCache);
        GET_DEVICE_FUNCTION(DestroyPipelineCache);
        GET_DEVICE_FUNCTION(GetPipelineCacheData);
        GET_DEVICE_FUNCTION(MergePipelineCaches);
        GET_DEVICE_FUNCTION(CreateGraphicsPipelines);
        GET_DEVICE_FUNCTION(CreateComputePipelines);
        GET_DEVICE_FUNCTION(DestroyPipeline);
        GET_DEVICE_FUNCTION(CreatePipelineLayout);
        GET_DEVICE_FUNCTION(DestroyPipelineLayout);
        GET_DEVICE_FUNCTION(CreateSampler);
        GET_DEVICE_FUNCTION(DestroySampler);
        GET_DEVICE_FUNCTION(CreateDescriptorSetLayout);
        GET_DEVICE_FUNCTION(DestroyDescriptorSetLayout);
        GET_DEVICE_FUNCTION(CreateDescriptorPool);
        GET_DEVICE_FUNCTION(DestroyDescriptorPool);
        GET_DEVICE_FUNCTION(ResetDescriptorPool);
        GET_DEVICE_FUNCTION(AllocateDescriptorSets);
        GET_DEVICE_FUNCTION(FreeDescriptorSets);
        GET_DEVICE_FUNCTION(UpdateDescriptorSets);
        GET_DEVICE_FUNCTION(CreateFramebuffer);
        GET_DEVICE_FUNCTION(DestroyFramebuffer);
        GET_DEVICE_FUNCTION(CreateRenderPass);
        GET_DEVICE_FUNCTION(DestroyRenderPass);
        GET_DEVICE_FUNCTION(GetRenderAreaGranularity);
        GET_DEVICE_FUNCTION(CreateCommandPool);
        GET_DEVICE_FUNCTION(DestroyCommandPool);
        GET_DEVICE_FUNCTION(ResetCommandPool);
        GET_DEVICE_FUNCTION(AllocateCommandBuffers);
        GET_DEVICE_FUNCTION(FreeCommandBuffers);
        GET_DEVICE_FUNCTION(BeginCommandBuffer);
        GET_DEVICE_FUNCTION(EndCommandBuffer);
        GET_DEVICE_FUNCTION(ResetCommandBuffer);
        GET_DEVICE_FUNCTION(CmdBindPipeline);
        GET_DEVICE_FUNCTION(CmdSetViewport);
        GET_DEVICE_FUNCTION(CmdSetScissor);
        GET_DEVICE_FUNCTION(CmdSetLineWidth);
        GET_DEVICE_FUNCTION(CmdSetDepthBias);
        GET_DEVICE_FUNCTION(CmdSetBlendConstants);
        GET_DEVICE_FUNCTION(CmdSetDepthBounds);
        GET_DEVICE_FUNCTION(CmdSetDepthCompareOpEXT);
        GET_DEVICE_FUNCTION(CmdSetStencilCompareMask);
        GET_DEVICE_FUNCTION(CmdSetStencilOpEXT);
        GET_DEVICE_FUNCTION(CmdSetStencilWriteMask);
        GET_DEVICE_FUNCTION(CmdSetStencilReference);
        GET_DEVICE_FUNCTION(CmdBindDescriptorSets);
        GET_DEVICE_FUNCTION(CmdBindIndexBuffer);
        GET_DEVICE_FUNCTION(CmdBindVertexBuffers);
        GET_DEVICE_FUNCTION(CmdDraw);
        GET_DEVICE_FUNCTION(CmdDrawIndexed);
        GET_DEVICE_FUNCTION(CmdDrawIndirect);
        GET_DEVICE_FUNCTION(CmdDrawIndexedIndirect);
        GET_DEVICE_FUNCTION(CmdDispatch);
        GET_DEVICE_FUNCTION(CmdDispatchIndirect);
        GET_DEVICE_FUNCTION(CmdCopyBuffer);
        GET_DEVICE_FUNCTION(CmdCopyImage);
        GET_DEVICE_FUNCTION(CmdBlitImage);
        GET_DEVICE_FUNCTION(CmdCopyBufferToImage);
        GET_DEVICE_FUNCTION(CmdCopyImageToBuffer);
        GET_DEVICE_FUNCTION(CmdUpdateBuffer);
        GET_DEVICE_FUNCTION(CmdFillBuffer);
        GET_DEVICE_FUNCTION(CmdClearColorImage);
        GET_DEVICE_FUNCTION(CmdClearDepthStencilImage);
        GET_DEVICE_FUNCTION(CmdClearAttachments);
        GET_DEVICE_FUNCTION(CmdResolveImage);
        GET_DEVICE_FUNCTION(CmdSetEvent);
        GET_DEVICE_FUNCTION(CmdResetEvent);
        GET_DEVICE_FUNCTION(CmdWaitEvents);
        GET_DEVICE_FUNCTION(CmdPipelineBarrier);
        GET_DEVICE_FUNCTION(CmdBeginQuery);
        GET_DEVICE_FUNCTION(CmdEndQuery);
        GET_DEVICE_FUNCTION(CmdResetQueryPool);
        GET_DEVICE_FUNCTION(CmdWriteTimestamp);
        GET_DEVICE_FUNCTION(CmdCopyQueryPoolResults);
        GET_DEVICE_FUNCTION(CmdPushConstants);
        GET_DEVICE_FUNCTION(CmdBeginRenderPass);
        GET_DEVICE_FUNCTION(CmdNextSubpass);
        GET_DEVICE_FUNCTION(CmdEndRenderPass);
        GET_DEVICE_FUNCTION(CmdExelwteCommands);
        GET_DEVICE_FUNCTION(CreateSwapchainKHR);
        GET_DEVICE_FUNCTION(DestroySwapchainKHR);
        GET_DEVICE_FUNCTION(GetSwapchainImagesKHR);
        GET_DEVICE_FUNCTION(AcquireNextImageKHR);
        GET_DEVICE_FUNCTION(QueuePresentKHR);
        GET_DEVICE_FUNCTION(CreateSharedSwapchainsKHR);
        GET_DEVICE_FUNCTION(TrimCommandPoolKHR);

    #ifdef VK_USE_PLATFORM_WIN32_KHR
        GET_DEVICE_FUNCTION(GetMemoryWin32HandleKHR);
        GET_DEVICE_FUNCTION(GetMemoryWin32HandlePropertiesKHR);
    #endif

        GET_DEVICE_FUNCTION(GetMemoryFdKHR);
        GET_DEVICE_FUNCTION(GetMemoryFdPropertiesKHR);

    #ifdef VK_USE_PLATFORM_WIN32_KHR
        GET_DEVICE_FUNCTION(ImportSemaphoreWin32HandleKHR);
        GET_DEVICE_FUNCTION(vkGetSemaphoreWin32HandleKHR);
    #endif

        GET_DEVICE_FUNCTION(ImportSemaphoreFdKHR);
        GET_DEVICE_FUNCTION(GetSemaphoreFdKHR);

        GET_DEVICE_FUNCTION(CmdPushDescriptorSetKHR);
        GET_DEVICE_FUNCTION(CreateDescriptorUpdateTemplateKHR);
        GET_DEVICE_FUNCTION(DestroyDescriptorUpdateTemplateKHR);
        GET_DEVICE_FUNCTION(UpdateDescriptorSetWithTemplateKHR);
        GET_DEVICE_FUNCTION(CmdPushDescriptorSetWithTemplateKHR);
        GET_DEVICE_FUNCTION(GetSwapchainStatusKHR);

    #ifdef VK_USE_PLATFORM_WIN32_KHR
        GET_DEVICE_FUNCTION(ImportFenceWin32HandleKHR);
        GET_DEVICE_FUNCTION(GetFenceWin32HandleKHR);
    #endif

        GET_DEVICE_FUNCTION(ImportFenceFdKHR);
        GET_DEVICE_FUNCTION(GetFenceFdKHR);

        GET_DEVICE_FUNCTION(GetImageMemoryRequirements2KHR);
        GET_DEVICE_FUNCTION(GetBufferMemoryRequirements2KHR);
        GET_DEVICE_FUNCTION(GetImageSparseMemoryRequirements2KHR);

        GET_DEVICE_FUNCTION(DebugMarkerSetObjectTagEXT);
        GET_DEVICE_FUNCTION(DebugMarkerSetObjectNameEXT);
        GET_DEVICE_FUNCTION(CmdDebugMarkerBeginEXT);
        GET_DEVICE_FUNCTION(CmdDebugMarkerEndEXT);
        GET_DEVICE_FUNCTION(CmdDebugMarkerInsertEXT);
        // GET_DEVICE_FUNCTION(CmdDrawIndirectCountAMD); temporarily disabled until driver is updated with new KHR function
        // GET_DEVICE_FUNCTION(CmdDrawIndexedIndirectCountAMD); temporarily disabled until driver is updated with new KHR function
    #ifdef VK_USE_PLATFORM_WIN32_KHR
        GET_DEVICE_FUNCTION(GetMemoryWin32HandleLW);
    #endif
        GET_DEVICE_FUNCTION(GetDeviceGroupPeerMemoryFeaturesKHR);
        GET_DEVICE_FUNCTION(BindBufferMemory2KHR);
        GET_DEVICE_FUNCTION(BindImageMemory2KHR);
        GET_DEVICE_FUNCTION(CmdSetDeviceMaskKHR);
        GET_DEVICE_FUNCTION(GetDeviceGroupPresentCapabilitiesKHR);
        GET_DEVICE_FUNCTION(GetDeviceGroupSurfacePresentModesKHR);
        GET_DEVICE_FUNCTION(AcquireNextImage2KHR);
        GET_DEVICE_FUNCTION(CmdDispatchBaseKHR);

        GET_DEVICE_FUNCTION(GetBufferDeviceAddress);

        GET_DEVICE_FUNCTION(DisplayPowerControlEXT);
        GET_DEVICE_FUNCTION(RegisterDeviceEventEXT);
        GET_DEVICE_FUNCTION(RegisterDisplayEventEXT);
        GET_DEVICE_FUNCTION(GetSwapchainCounterEXT);
        GET_DEVICE_FUNCTION(GetRefreshCycleDurationGOOGLE);
        GET_DEVICE_FUNCTION(GetPastPresentationTimingGOOGLE);
        GET_DEVICE_FUNCTION(CmdSetDiscardRectangleEXT);
        GET_DEVICE_FUNCTION(SetHdrMetadataEXT);

        GET_DEVICE_FUNCTION(CmdBuildAccelerationStructuresKHR);
        GET_DEVICE_FUNCTION(CmdCopyAccelerationStructureKHR);
        GET_DEVICE_FUNCTION(CmdTraceRaysKHR);
        GET_DEVICE_FUNCTION(CmdWriteAccelerationStructuresPropertiesKHR);
        GET_DEVICE_FUNCTION(CreateAccelerationStructureKHR);
        GET_DEVICE_FUNCTION(DestroyAccelerationStructureKHR);
        GET_DEVICE_FUNCTION(GetAccelerationStructureBuildSizesKHR);
        GET_DEVICE_FUNCTION(GetAccelerationStructureDeviceAddressKHR);
        GET_DEVICE_FUNCTION(CreateRayTracingPipelinesKHR);
        GET_DEVICE_FUNCTION(GetRayTracingShaderGroupHandlesKHR);
    }

    const LayerLibrary *FindLayer(const char *vulkanLayerName)
    {
        for (const auto &layerLibrary : s_LayerLibraries)
        {
            if (layerLibrary.layerName.compare(vulkanLayerName) == 0)
            {
                return &layerLibrary;
            }
        }

        return nullptr;
    }

    VkResult CreateInstanceToDriver
    (
        const VkInstanceCreateInfo*                 pCreateInfo,
        const VkAllocationCallbacks*                pAllocator,
        VkInstance*                                 pInstance
    )
    {
        VkResult res = vkCreateInstance(pCreateInfo, pAllocator, pInstance);

        if (res == VK_SUCCESS)
        {
            VkLayerInstanceDispatchTable *pInstanceDispatchTable = new VkLayerInstanceDispatchTable;
            memset(pInstanceDispatchTable, 0, sizeof(VkLayerInstanceDispatchTable));
            // The validation layers are using the dispatch table pointer as a key
            // to internal data structures, thus it has to be set to something
            // unique (originally from the driver it always comes the same
            // as "__VK_LOADER_MAGIC_VALUE 0x01CDC0DE".
            *reinterpret_cast<void**>(*pInstance) = pInstanceDispatchTable;
        }

        return res;
    }

    void DestroyInstanceToDriver
    (
        VkInstance                                  instance,
        const VkAllocationCallbacks*                pAllocator
    )
    {
        VkLayerInstanceDispatchTable *pInstanceDispatchTable =
            *reinterpret_cast<VkLayerInstanceDispatchTable**>(instance);

        vkDestroyInstance(instance, pAllocator);

        memset(pInstanceDispatchTable, 0, sizeof(VkLayerInstanceDispatchTable));
        delete pInstanceDispatchTable;

        s_EnabledLayersInInstance.erase(instance);
    }

    VkResult EnumeratePhysicalDevicesToDriver
    (
        VkInstance                                  instance,
        uint32_t*                                   pPhysicalDeviceCount,
        VkPhysicalDevice*                           pPhysicalDevices
    )
    {
        VkResult res = vkEnumeratePhysicalDevices(instance, pPhysicalDeviceCount, pPhysicalDevices);

        if ((pPhysicalDevices != nullptr) && (res == VK_SUCCESS))
        {
            for (UINT32 deviceIdx = 0; deviceIdx < *pPhysicalDeviceCount; deviceIdx++)
            {
                if (pPhysicalDevices[deviceIdx] != nullptr)
                {
                    *reinterpret_cast<void**>(pPhysicalDevices[deviceIdx]) =
                        *reinterpret_cast<void**>(instance);
                }
            }
        }

        return res;
    }

    VkResult CreateDeviceToDriver
    (
        VkPhysicalDevice                            physicalDevice,
        const VkDeviceCreateInfo*                   pCreateInfo,
        const VkAllocationCallbacks*                pAllocator,
        VkDevice*                                   pDevice
    )
    {
        VkResult res = vkCreateDevice(physicalDevice, pCreateInfo, pAllocator, pDevice);

        if (res == VK_SUCCESS)
        {
            VkLayerDispatchTable *pDeviceDispatchTable = new VkLayerDispatchTable;
            memset(pDeviceDispatchTable, 0, sizeof(VkLayerDispatchTable));
            // The validation layers are using the dispatch table pointer as a key
            // to internal data structures, thus it has to be set to something
            // unique (originally from the driver it always comes the same
            // as "__VK_LOADER_MAGIC_VALUE 0x01CDC0DE".
            *reinterpret_cast<void**>(*pDevice) = pDeviceDispatchTable;
        }

        return res;
    }

    VKAPI_ATTR void VKAPI_CALL DestroyDeviceToDriver
    (
        VkDevice                                    device,
        const VkAllocationCallbacks*                pAllocator
    )
    {
        VkLayerDispatchTable *pDeviceDispatchTable =
            *reinterpret_cast<VkLayerDispatchTable**>(device);

        vkDestroyDevice(device, pAllocator);

        memset(pDeviceDispatchTable, 0, sizeof(VkLayerDispatchTable));
        delete pDeviceDispatchTable;
    }

    void GetDeviceQueueToDriver
    (
        VkDevice                                    device,
        uint32_t                                    queueNodeIndex,
        uint32_t                                    queueIndex,
        VkQueue*                                    pQueue
    )
    {
        vkGetDeviceQueue(device, queueNodeIndex, queueIndex, pQueue);

        if (*pQueue != nullptr)
        {
            *reinterpret_cast<void**>(*pQueue) =
                *reinterpret_cast<void**>(device);
        }
    }

    VkResult AllocateCommandBuffersToDriver
    (
        VkDevice                                    device,
        const VkCommandBufferAllocateInfo*          pAllocateInfo,
        VkCommandBuffer*                            pCommandBuffers
    )
    {
        VkResult res = vkAllocateCommandBuffers(device, pAllocateInfo, pCommandBuffers);

        if (res == VK_SUCCESS)
        {
            for (UINT32 bufferIdx = 0; bufferIdx < pAllocateInfo->commandBufferCount; bufferIdx++)
            {
                if (pCommandBuffers[bufferIdx] != nullptr)
                {
                    *reinterpret_cast<void**>(pCommandBuffers[bufferIdx]) =
                        *reinterpret_cast<void**>(device);
                }
            }
        }

        return res;
    }

    VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
    ModsGetInstanceProcAddr(VkInstance instance, const char* pName)
    {
        #define RETURN_DRV_FUNCTION(name) \
            if (!strcmp(pName, #name)) return PFN_vkVoidFunction(name);

        if (!strcmp(pName, "vkGetInstanceProcAddr"))
            return PFN_vkVoidFunction(ModsGetInstanceProcAddr);
        if (!strcmp(pName, "vkCreateInstance"))
            return PFN_vkVoidFunction(CreateInstanceToDriver);
        if (!strcmp(pName, "vkDestroyInstance"))
            return PFN_vkVoidFunction(DestroyInstanceToDriver);
        if (!strcmp(pName, "vkEnumeratePhysicalDevices"))
            return PFN_vkVoidFunction(EnumeratePhysicalDevicesToDriver);
        RETURN_DRV_FUNCTION(vkGetPhysicalDeviceFeatures)
        RETURN_DRV_FUNCTION(vkGetPhysicalDeviceFormatProperties)
        RETURN_DRV_FUNCTION(vkGetPhysicalDeviceImageFormatProperties)
        RETURN_DRV_FUNCTION(vkGetPhysicalDeviceProperties)
        RETURN_DRV_FUNCTION(vkGetPhysicalDeviceQueueFamilyProperties)
        RETURN_DRV_FUNCTION(vkGetPhysicalDeviceMemoryProperties)
        RETURN_DRV_FUNCTION(vkEnumerateDeviceExtensionProperties)
        RETURN_DRV_FUNCTION(vkEnumerateDeviceLayerProperties)
        RETURN_DRV_FUNCTION(vkGetPhysicalDeviceSparseImageFormatProperties)

        RETURN_DRV_FUNCTION(vkGetPhysicalDeviceFeatures2);
        RETURN_DRV_FUNCTION(vkGetPhysicalDeviceProperties2);
        RETURN_DRV_FUNCTION(vkGetPhysicalDeviceFormatProperties2);
        RETURN_DRV_FUNCTION(vkGetPhysicalDeviceImageFormatProperties2);
        RETURN_DRV_FUNCTION(vkGetPhysicalDeviceQueueFamilyProperties2);
        RETURN_DRV_FUNCTION(vkGetPhysicalDeviceMemoryProperties2);
        RETURN_DRV_FUNCTION(vkGetPhysicalDeviceSparseImageFormatProperties2);

        RETURN_DRV_FUNCTION(vkGetPhysicalDeviceFeatures2KHR);
        RETURN_DRV_FUNCTION(vkGetPhysicalDeviceProperties2KHR);
        RETURN_DRV_FUNCTION(vkGetPhysicalDeviceFormatProperties2KHR);
        RETURN_DRV_FUNCTION(vkGetPhysicalDeviceImageFormatProperties2KHR);
        RETURN_DRV_FUNCTION(vkGetPhysicalDeviceQueueFamilyProperties2KHR);
        RETURN_DRV_FUNCTION(vkGetPhysicalDeviceMemoryProperties2KHR);
        RETURN_DRV_FUNCTION(vkGetPhysicalDeviceSparseImageFormatProperties2KHR);

        RETURN_DRV_FUNCTION(vkGetPhysicalDeviceExternalBufferPropertiesKHR);
        RETURN_DRV_FUNCTION(vkGetPhysicalDeviceExternalSemaphorePropertiesKHR);
        RETURN_DRV_FUNCTION(vkGetPhysicalDeviceExternalFencePropertiesKHR);
        RETURN_DRV_FUNCTION(vkCreateDebugReportCallbackEXT)
        RETURN_DRV_FUNCTION(vkDestroyDebugReportCallbackEXT)
        RETURN_DRV_FUNCTION(vkDebugReportMessageEXT)

        RETURN_DRV_FUNCTION(vkGetPhysicalDeviceCooperativeMatrixPropertiesLW)

        if (!strcmp(pName, "vkCreateDevice"))
            return PFN_vkVoidFunction(CreateDeviceToDriver);

        #undef RETURN_DRV_FUNCTION

        return nullptr;
    }

    VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
    ModsGetDeviceProcAddr
    (
        VkDevice device,
        const char *pName
    )
    {
        #define RETURN_DRV_FUNCTION(name) \
            if (!strcmp(pName, #name)) return PFN_vkVoidFunction(name);

        if (!strcmp(pName, "vkDestroyDevice"))
            return PFN_vkVoidFunction(DestroyDeviceToDriver);
        if (!strcmp(pName, "vkGetDeviceQueue"))
            return PFN_vkVoidFunction(GetDeviceQueueToDriver);
        RETURN_DRV_FUNCTION(vkQueueSubmit)
        RETURN_DRV_FUNCTION(vkQueueWaitIdle)
        RETURN_DRV_FUNCTION(vkDeviceWaitIdle)
        RETURN_DRV_FUNCTION(vkAllocateMemory)
        RETURN_DRV_FUNCTION(vkFreeMemory)
        RETURN_DRV_FUNCTION(vkMapMemory)
        RETURN_DRV_FUNCTION(vkUnmapMemory)
        RETURN_DRV_FUNCTION(vkFlushMappedMemoryRanges)
        RETURN_DRV_FUNCTION(vkIlwalidateMappedMemoryRanges)
        RETURN_DRV_FUNCTION(vkGetDeviceMemoryCommitment)
        RETURN_DRV_FUNCTION(vkBindBufferMemory)
        RETURN_DRV_FUNCTION(vkBindImageMemory)
        RETURN_DRV_FUNCTION(vkGetBufferMemoryRequirements)
        RETURN_DRV_FUNCTION(vkGetImageMemoryRequirements)
        RETURN_DRV_FUNCTION(vkGetImageSparseMemoryRequirements)
        RETURN_DRV_FUNCTION(vkQueueBindSparse)
        RETURN_DRV_FUNCTION(vkCreateFence)
        RETURN_DRV_FUNCTION(vkDestroyFence)
        RETURN_DRV_FUNCTION(vkResetFences)
        RETURN_DRV_FUNCTION(vkGetFenceStatus)
        RETURN_DRV_FUNCTION(vkWaitForFences)
        RETURN_DRV_FUNCTION(vkCreateSemaphore)
        RETURN_DRV_FUNCTION(vkDestroySemaphore)
        RETURN_DRV_FUNCTION(vkCreateEvent)
        RETURN_DRV_FUNCTION(vkDestroyEvent)
        RETURN_DRV_FUNCTION(vkGetEventStatus)
        RETURN_DRV_FUNCTION(vkSetEvent)
        RETURN_DRV_FUNCTION(vkResetEvent)
        RETURN_DRV_FUNCTION(vkCreateQueryPool)
        RETURN_DRV_FUNCTION(vkDestroyQueryPool)
        RETURN_DRV_FUNCTION(vkGetQueryPoolResults)
        RETURN_DRV_FUNCTION(vkCreateBuffer)
        RETURN_DRV_FUNCTION(vkDestroyBuffer)
        RETURN_DRV_FUNCTION(vkCreateBufferView)
        RETURN_DRV_FUNCTION(vkDestroyBufferView)
        RETURN_DRV_FUNCTION(vkCreateImage)
        RETURN_DRV_FUNCTION(vkDestroyImage)
        RETURN_DRV_FUNCTION(vkGetImageSubresourceLayout)
        RETURN_DRV_FUNCTION(vkCreateImageView)
        RETURN_DRV_FUNCTION(vkDestroyImageView)
        RETURN_DRV_FUNCTION(vkCreateShaderModule)
        RETURN_DRV_FUNCTION(vkDestroyShaderModule)
        RETURN_DRV_FUNCTION(vkCreatePipelineCache)
        RETURN_DRV_FUNCTION(vkDestroyPipelineCache)
        RETURN_DRV_FUNCTION(vkGetPipelineCacheData)
        RETURN_DRV_FUNCTION(vkMergePipelineCaches)
        RETURN_DRV_FUNCTION(vkCreateGraphicsPipelines)
        RETURN_DRV_FUNCTION(vkCreateComputePipelines)
        RETURN_DRV_FUNCTION(vkDestroyPipeline)
        RETURN_DRV_FUNCTION(vkCreatePipelineLayout)
        RETURN_DRV_FUNCTION(vkDestroyPipelineLayout)
        RETURN_DRV_FUNCTION(vkCreateSampler)
        RETURN_DRV_FUNCTION(vkDestroySampler)
        RETURN_DRV_FUNCTION(vkCreateDescriptorSetLayout)
        RETURN_DRV_FUNCTION(vkDestroyDescriptorSetLayout)
        RETURN_DRV_FUNCTION(vkCreateDescriptorPool)
        RETURN_DRV_FUNCTION(vkDestroyDescriptorPool)
        RETURN_DRV_FUNCTION(vkResetDescriptorPool)
        RETURN_DRV_FUNCTION(vkAllocateDescriptorSets)
        RETURN_DRV_FUNCTION(vkFreeDescriptorSets)
        RETURN_DRV_FUNCTION(vkUpdateDescriptorSets)
        RETURN_DRV_FUNCTION(vkCreateFramebuffer)
        RETURN_DRV_FUNCTION(vkDestroyFramebuffer)
        RETURN_DRV_FUNCTION(vkCreateRenderPass)
        RETURN_DRV_FUNCTION(vkDestroyRenderPass)
        RETURN_DRV_FUNCTION(vkGetRenderAreaGranularity)
        RETURN_DRV_FUNCTION(vkCreateCommandPool)
        RETURN_DRV_FUNCTION(vkDestroyCommandPool)
        RETURN_DRV_FUNCTION(vkResetCommandPool)
        if (!strcmp(pName, "vkAllocateCommandBuffers"))
            return PFN_vkVoidFunction(AllocateCommandBuffersToDriver);
        RETURN_DRV_FUNCTION(vkFreeCommandBuffers)
        RETURN_DRV_FUNCTION(vkBeginCommandBuffer)
        RETURN_DRV_FUNCTION(vkEndCommandBuffer)
        RETURN_DRV_FUNCTION(vkResetCommandBuffer)
        RETURN_DRV_FUNCTION(vkCmdBindPipeline)
        RETURN_DRV_FUNCTION(vkCmdSetViewport)
        RETURN_DRV_FUNCTION(vkCmdSetScissor)
        RETURN_DRV_FUNCTION(vkCmdSetLineWidth)
        RETURN_DRV_FUNCTION(vkCmdSetDepthBias)
        RETURN_DRV_FUNCTION(vkCmdSetBlendConstants)
        RETURN_DRV_FUNCTION(vkCmdSetDepthBounds)
        RETURN_DRV_FUNCTION(vkCmdSetDepthCompareOpEXT)
        RETURN_DRV_FUNCTION(vkCmdSetStencilCompareMask)
        RETURN_DRV_FUNCTION(vkCmdSetStencilOpEXT)
        RETURN_DRV_FUNCTION(vkCmdSetStencilWriteMask)
        RETURN_DRV_FUNCTION(vkCmdSetStencilReference)
        RETURN_DRV_FUNCTION(vkCmdBindDescriptorSets)
        RETURN_DRV_FUNCTION(vkCmdBindIndexBuffer)
        RETURN_DRV_FUNCTION(vkCmdBindVertexBuffers)
        RETURN_DRV_FUNCTION(vkCmdDraw)
        RETURN_DRV_FUNCTION(vkCmdDrawIndexed)
        RETURN_DRV_FUNCTION(vkCmdDrawIndirect)
        RETURN_DRV_FUNCTION(vkCmdDrawIndexedIndirect)
        RETURN_DRV_FUNCTION(vkCmdDispatch)
        RETURN_DRV_FUNCTION(vkCmdDispatchIndirect)
        RETURN_DRV_FUNCTION(vkCmdCopyBuffer)
        RETURN_DRV_FUNCTION(vkCmdCopyImage)
        RETURN_DRV_FUNCTION(vkCmdBlitImage)
        RETURN_DRV_FUNCTION(vkCmdCopyBufferToImage)
        RETURN_DRV_FUNCTION(vkCmdCopyImageToBuffer)
        RETURN_DRV_FUNCTION(vkCmdUpdateBuffer)
        RETURN_DRV_FUNCTION(vkCmdFillBuffer)
        RETURN_DRV_FUNCTION(vkCmdClearColorImage)
        RETURN_DRV_FUNCTION(vkCmdClearDepthStencilImage)
        RETURN_DRV_FUNCTION(vkCmdClearAttachments)
        RETURN_DRV_FUNCTION(vkCmdResolveImage)
        RETURN_DRV_FUNCTION(vkCmdSetEvent)
        RETURN_DRV_FUNCTION(vkCmdResetEvent)
        RETURN_DRV_FUNCTION(vkCmdWaitEvents)
        RETURN_DRV_FUNCTION(vkCmdPipelineBarrier)
        RETURN_DRV_FUNCTION(vkCmdBeginQuery)
        RETURN_DRV_FUNCTION(vkCmdEndQuery)
        RETURN_DRV_FUNCTION(vkCmdResetQueryPool)
        RETURN_DRV_FUNCTION(vkCmdWriteTimestamp)
        RETURN_DRV_FUNCTION(vkCmdCopyQueryPoolResults)
        RETURN_DRV_FUNCTION(vkCmdPushConstants)
        RETURN_DRV_FUNCTION(vkCmdBeginRenderPass)
        RETURN_DRV_FUNCTION(vkCmdNextSubpass)
        RETURN_DRV_FUNCTION(vkCmdEndRenderPass)
        RETURN_DRV_FUNCTION(vkCmdExelwteCommands)
        RETURN_DRV_FUNCTION(vkGetBufferDeviceAddress)
        RETURN_DRV_FUNCTION(vkDebugMarkerSetObjectTagEXT)
        RETURN_DRV_FUNCTION(vkDebugMarkerSetObjectNameEXT)
        RETURN_DRV_FUNCTION(vkCmdDebugMarkerBeginEXT)
        RETURN_DRV_FUNCTION(vkCmdDebugMarkerEndEXT)
        RETURN_DRV_FUNCTION(vkCmdDebugMarkerInsertEXT)
        RETURN_DRV_FUNCTION(vkCmdBuildAccelerationStructuresKHR)
        RETURN_DRV_FUNCTION(vkCmdCopyAccelerationStructureKHR)
        RETURN_DRV_FUNCTION(vkCmdTraceRaysKHR)
        RETURN_DRV_FUNCTION(vkCmdWriteAccelerationStructuresPropertiesKHR)
        RETURN_DRV_FUNCTION(vkCreateAccelerationStructureKHR)
        RETURN_DRV_FUNCTION(vkDestroyAccelerationStructureKHR)
        RETURN_DRV_FUNCTION(vkGetAccelerationStructureBuildSizesKHR)
        RETURN_DRV_FUNCTION(vkGetAccelerationStructureDeviceAddressKHR)
        RETURN_DRV_FUNCTION(vkCreateRayTracingPipelinesKHR)
        RETURN_DRV_FUNCTION(vkGetRayTracingShaderGroupHandlesKHR)

        #undef RETURN_DRV_FUNCTION

        return nullptr;
    }
}

VkResult VulkanLayers::Init(bool enableLayersInMODS)
{

    if (!enableLayersInMODS || s_LayerLibraries.size() > 0)
    {
        return VK_SUCCESS;
    }

#ifndef DIRECTAMODEL
    UINT32 version = 3;
    const VkResult res = vk_icdNegotiateLoaderICDInterfaceVersion(&version);
    if (res != VK_SUCCESS)
    {
        return res;
    }
#endif

#ifdef _WIN32
#   define LIB_PREFIX
#else
#   define LIB_PREFIX "lib"
#endif

    const LayerFile layerFiles[] =
    {
        // This layer is implemented in version 1.1.106.0 and newer
        {
            "VK_LAYER_LUNARG_khronos_validation",
            LIB_PREFIX "VkLayer_khronos_validation"
        }
    };
    return LoadLayerLibraries(sizeof(layerFiles)/sizeof(layerFiles[0]), layerFiles);
}

VkResult VulkanLayers::EnumerateInstanceExtensionProperties
(
    const char*                                 pLayerName,
    uint32_t*                                   pPropertyCount,
    VkExtensionProperties*                      pProperties
)
{
    if (pLayerName != nullptr)
    {
        *pPropertyCount = 0;
        return VK_SUCCESS;
    }

    return vkEnumerateInstanceExtensionProperties(pLayerName, pPropertyCount, pProperties);
}

VkResult VulkanLayers::EnumerateInstanceLayerProperties
(
    uint32_t*                                   pPropertyCount,
    VkLayerProperties*                          pProperties
)
{
    *pPropertyCount = static_cast<uint32_t>(s_LayerLibraries.size());

    if (pProperties == nullptr)
    {
        return VK_SUCCESS;
    }

    UINT32 propertyIndex = 0;
    for (const auto &library : s_LayerLibraries)
    {
        auto &layerProperty = pProperties[propertyIndex];

        strncpy(layerProperty.layerName, library.layerName.c_str(), sizeof(layerProperty.layerName));
        layerProperty.specVersion = 1;
        layerProperty.implementatiolwersion = 1;
        layerProperty.description[0] = 0;

        propertyIndex++;
    }

    return VK_SUCCESS;
}

uint32_t VulkanLayers::EnumerateInstanceVersion()
{
    uint32_t ver = VK_API_VERSION_1_0;

    vkEnumerateInstanceVersion(&ver);

    return ver;
}

VkResult VulkanLayers::CreateInstance
(
    const VkInstanceCreateInfo*                 pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkInstance*                                 pInstance,
    VkLayerInstanceDispatchTable*               pvi
)
{
    VkLayerInstanceCreateInfo layerInstanceCreateInfo;
    memset(&layerInstanceCreateInfo, 0, sizeof(layerInstanceCreateInfo));
    layerInstanceCreateInfo.sType = VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO;
    layerInstanceCreateInfo.pNext = pCreateInfo->pNext;
    layerInstanceCreateInfo.function = VK_LAYER_LINK_INFO;

    // The selected size "enabledLayerCount" is the worst case scenario - not
    // all entries may end up being used:
    vector<VkLayerInstanceLink> layerInstanceLinks(pCreateInfo->enabledLayerCount);
    UINT32 nextLayerInstanceLinkIdx = 0;

    PFN_vkGetInstanceProcAddr lwrrentGetInstanceProcAddr = ModsGetInstanceProcAddr;

    vector<const LayerLibrary*> enabledLayers;

    for (UINT32 layerIdx = 0; layerIdx < pCreateInfo->enabledLayerCount; layerIdx++)
    {
        const LayerLibrary *pLayerLibrary =
            FindLayer(pCreateInfo->ppEnabledLayerNames[layerIdx]);
        if (pLayerLibrary == nullptr)
            continue;

        auto &layerInstanceLink = layerInstanceLinks[nextLayerInstanceLinkIdx];
        layerInstanceLink.pNext = layerInstanceCreateInfo.u.pLayerInfo;
        layerInstanceLink.pfnNextGetInstanceProcAddr = lwrrentGetInstanceProcAddr;
        lwrrentGetInstanceProcAddr = pLayerLibrary->getInstanceProcAddr;

        layerInstanceCreateInfo.u.pLayerInfo = &layerInstanceLink;

        enabledLayers.emplace_back(pLayerLibrary);

        nextLayerInstanceLinkIdx++;
    }

    PFN_vkCreateInstance lwrrentVkCreateInstance = PFN_vkCreateInstance(
        lwrrentGetInstanceProcAddr(*pInstance, "vkCreateInstance"));

    if (lwrrentVkCreateInstance == nullptr)
    {
        Printf(Tee::PriError, "vkCreateInstance function pointer is NULL\n");
        return VK_ERROR_LAYER_NOT_PRESENT;
    }

    VkInstanceCreateInfo createInfoWithLayers;
    memcpy(&createInfoWithLayers, pCreateInfo, sizeof(VkInstanceCreateInfo));
    createInfoWithLayers.pNext = &layerInstanceCreateInfo;

    VkResult res = lwrrentVkCreateInstance(&createInfoWithLayers, pAllocator, pInstance);
    if (res != VK_SUCCESS)
        return res;

    s_EnabledLayersInInstance[*pInstance] = enabledLayers;
    FillInstanceDispatchTableFromGetInstanceProcAddr(pvi, *pInstance, lwrrentGetInstanceProcAddr);

    return res;
}

VkResult VulkanLayers::CreateDevice
(
    VkInstance                                  instance,
    VkPhysicalDevice                            physicalDevice,
    const VkDeviceCreateInfo*                   pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkDevice*                                   pDevice,
    VkLayerDispatchTable*                       pvd
)
{
    if (pCreateInfo->enabledLayerCount != 0)
    {
        MASSERT(!"Device layers are not supported");
        return VK_ERROR_FEATURE_NOT_PRESENT;
    }

    VkLayerDeviceCreateInfo layerDeviceCreateInfo;
    vector<VkLayerDeviceLink> layerDeviceLinks;

    VkDeviceCreateInfo deviceCreateInfoWithLayers;
    memcpy(&deviceCreateInfoWithLayers, pCreateInfo, sizeof(VkDeviceCreateInfo));

    PFN_vkGetInstanceProcAddr lwrrentGetInstanceProcAddr = ModsGetInstanceProcAddr;
    PFN_vkGetDeviceProcAddr lwrrentGetDeviceProcAddr = ModsGetDeviceProcAddr;

    const auto& enabledLayersItr = s_EnabledLayersInInstance.find(instance);
    if (enabledLayersItr != s_EnabledLayersInInstance.end() && !enabledLayersItr->second.empty())
    {
        const auto &enabledLayers = enabledLayersItr->second;

        memset(&layerDeviceCreateInfo, 0, sizeof(layerDeviceCreateInfo));
        layerDeviceCreateInfo.sType = VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO;
        layerDeviceCreateInfo.pNext = pCreateInfo->pNext;
        layerDeviceCreateInfo.function = VK_LAYER_LINK_INFO;

        layerDeviceLinks.resize(enabledLayers.size());
        UINT32 nextLayerDeviceLinkIdx = 0;

        for (const auto &pLayerLibrary : enabledLayers)
        {
            auto &layerDeviceLink = layerDeviceLinks[nextLayerDeviceLinkIdx];
            layerDeviceLink.pNext = layerDeviceCreateInfo.u.pLayerInfo;
            layerDeviceLink.pfnNextGetInstanceProcAddr = lwrrentGetInstanceProcAddr;
            layerDeviceLink.pfnNextGetDeviceProcAddr = lwrrentGetDeviceProcAddr;
            lwrrentGetInstanceProcAddr = pLayerLibrary->getInstanceProcAddr;
            lwrrentGetDeviceProcAddr = pLayerLibrary->getDeviceProcAddr;

            layerDeviceCreateInfo.u.pLayerInfo = &layerDeviceLink;

            nextLayerDeviceLinkIdx++;
        }

        deviceCreateInfoWithLayers.pNext = &layerDeviceCreateInfo;
    }

    PFN_vkCreateDevice lwrrentVkCreateDevice = PFN_vkCreateDevice(
        lwrrentGetInstanceProcAddr(instance, "vkCreateDevice"));

    if (lwrrentVkCreateDevice == nullptr)
    {
        Printf(Tee::PriError, "vkCreateDevice function pointer is NULL\n");
        return VK_ERROR_LAYER_NOT_PRESENT;
    }

    VkResult res = lwrrentVkCreateDevice(physicalDevice, &deviceCreateInfoWithLayers,
        pAllocator, pDevice);

    if (res != VK_SUCCESS)
        return res;

    FillDeviceDispatchTableFromGetDeviceProcAddr(pvd, *pDevice, lwrrentGetDeviceProcAddr);

    return res;
}
