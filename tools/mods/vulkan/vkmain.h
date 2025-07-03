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

#pragma once

#ifdef VULKAN_STANDALONE

#include "vulkan/vulkan.h"

#include "vkmods.h"
#include "vkmodssub.h"
#include "vktest_platform.h"
#include "core/include/utility.h"

// Needed for vktest.exe until the new masks appear in the sdk:
#ifndef VK_SHADER_STAGE_TASK_BIT_LW
#define VK_SHADER_STAGE_TASK_BIT_LW         VkShaderStageFlagBits(0x00000040)
#endif
#ifndef VK_SHADER_STAGE_MESH_BIT_LW
#define VK_SHADER_STAGE_MESH_BIT_LW         VkShaderStageFlagBits(0x00000080)
#endif

#define SETGET_PROP(instance, type) type Get##instance() const \
    {return m_##instance;}  void Set##instance(type temp){ m_##instance = temp;}
#define GET_PROP(instance, type) type Get##instance() const {return m_##instance;}

// Macro to allow custom property getter functions
#define GET_PROP_LWSTOM(instance, type)   \
    type Get##instance() const;

#define SETGET_JSARRAY_PROP_LWSTOM(propName)

// Macro to allow setter/getter functions from custom functions
#define SETGET_PROP_FUNC(propName, resultType, setFunc, getFunc) \
    resultType Get##propName() const { return getFunc(); }       \
    void Set##propName(resultType val) { setFunc(val); }

#ifdef NDEBUG
#define Printf
#else
INT32 Printf
(
    INT32        Priority,
    const char * Format,
    ... //       Arguments
);
INT32 Printf
(
    INT32        Priority,
    UINT32       ModuleCode,
    const char * Format,
    ... //       Arguments
);
#endif

#define CHECK_VK_RESULT(func)           \
    do {                                \
        if (res != VK_SUCCESS) PlatformPrintf("Possible lost VkResult %d.\n", res); \
        if ((res = func) != VK_SUCCESS) \
        return res;                     \
    } while (0)

template<typename Func>
class DeferredCleanup
{
    public:
        explicit DeferredCleanup(Func f) : m_Func(f), m_Active(true) { }

        ~DeferredCleanup() noexcept(false)
        {
            if (m_Active)
            {
                m_Func();
            }
        }

        DeferredCleanup(DeferredCleanup&& f)
        : m_Func(move(f.m_Func)), m_Active(true)
        {
            f.m_Active = false;
        }

        void Cancel()
        {
            m_Active = false;
        }

        DeferredCleanup(const DeferredCleanup&)            = delete;
        DeferredCleanup& operator=(const DeferredCleanup&) = delete;

    private:
        Func m_Func;
        bool m_Active;
};

class GenDeferObject
{
    public:
        template<typename Func>
        DeferredCleanup<Func> operator+(Func f) const
        {
            return DeferredCleanup<Func>(f);
        }
};

#define __MODS_UTILITY_CATNAME_INTERNAL(a, b) a##b
#define __MODS_UTILITY_CATNAME(a, b) __MODS_UTILITY_CATNAME_INTERNAL(a, b)
#define DEFER DEFERNAME(__MODS_UTILITY_CATNAME(_defer_, __LINE__))

#define HS_(s) \
    []() -> Utility::HiddenStringStorage<sizeof(s)>& { \
        constexpr size_t strSize = sizeof(s); \
        constexpr auto colwerter = Utility::MakeHiddenStringColwerter<strSize>(s); \
        static auto storage = Utility::HiddenStringStorage<strSize>(colwerter); \
        return storage; \
    }()

#else

#define HS_

#include "vulkan/vulkanlw.h"
#include "opengl/modsgl.h"
#include "core/include/setget.h"
#include "core/include/xp.h"
#include "vkmods.h"

#define CHECK_VK_RESULT(func)           \
    do {                                \
        if (res != VK_SUCCESS) Printf(Tee::PriWarn, "Possible lost VkResult %d.\n", res); \
        if ((res = func) != VK_SUCCESS) \
        return res;                     \
    } while (0)

#endif

#include "vk_layer_dispatch_table.h"
#include "vkerror.h"

#include <functional>
#include <memory>

#ifdef TRACY_ENABLE
#   include "TracyVulkan.hpp"
#   define PROFILE_COLOR_VULKAN 0x6311B3
#   define PROFILE_COLOR_GPU    0x63B113
#   define PROFILE_VK_ZONE(name, cmdbuf) TracyVkZoneC((cmdbuf).GetTracyCtx(), (cmdbuf).GetCmdBuffer(), name, PROFILE_COLOR_GPU)
#   define PROFILE_VK_ZONE_COLLECT(cmdbuf) TracyVkCollect((cmdbuf).GetTracyCtx(), (cmdbuf).GetCmdBuffer())
#else
#   define PROFILE_VK_ZONE(name, cmdbuf)
#   define PROFILE_VK_ZONE_COLLECT(cmdbuf)
#endif

using VulkanQueueFamilyIndex = uint32_t;
using VulkanQueueIndex = uint32_t;

//Increasing the num images in swap chain reduces the time it takes to present however dont exceed:
//(VkSurfaceCapabilitiesKHR.maxImageCount - VkSurfaceCapabilitiesKHR.minImageCount)
#define NUM_EXTRA_PRESENTABLE_IMAGES      1

// Amount of time, in nanoseconds, to wait for a command buffer to complete
static constexpr UINT64 FENCE_TIMEOUT = 1'000'000'000ULL; // 1 second

#define vkCreateInstance disabledVkCreateInstance
#define vkDestroyInstance disabledVkDestroyInstance
#define vkEnumeratePhysicalDevices disabledVkEnumeratePhysicalDevices
#define vkGetPhysicalDeviceFeatures disabledVkGetPhysicalDeviceFeatures
#define vkGetPhysicalDeviceFormatProperties disabledVkGetPhysicalDeviceFormatProperties
#define vkGetPhysicalDeviceImageFormatProperties disabledVkGetPhysicalDeviceImageFormatProperties
#define vkGetPhysicalDeviceProperties disabledVkGetPhysicalDeviceProperties
#define vkGetPhysicalDeviceQueueFamilyProperties disabledVkGetPhysicalDeviceQueueFamilyProperties
#define vkGetPhysicalDeviceMemoryProperties disabledVkGetPhysicalDeviceMemoryProperties
#define vkGetInstanceProcAddr disabledVkGetInstanceProcAddr
#define vkGetDeviceProcAddr disabledVkGetDeviceProcAddr
#define vkCreateDevice disabledVkCreateDevice
#define vkDestroyDevice disabledVkDestroyDevice
#define vkEnumerateInstanceExtensionProperties disabledVkEnumerateInstanceExtensionProperties
#define vkEnumerateDeviceExtensionProperties disableVkEnumerateDeviceExtensionProperties
#define vkEnumerateInstanceLayerProperties disabledVkEnumerateInstanceLayerProperties
#define vkEnumerateDeviceLayerProperties disabledVkEnumerateDeviceLayerProperties
#define vkGetDeviceQueue disabledVkGetDeviceQueue
#define vkQueueSubmit disabledVkQueueSubmit
#define vkQueueWaitIdle disabledVkQueueWaitIdle
#define vkDeviceWaitIdle disabledVkDeviceWaitIdle
#define vkAllocateMemory disabledVkAllocateMemory
#define vkFreeMemory disabledVkFreeMemory
#define vkMapMemory disabledVkMapMemory
#define vkUnmapMemory disabledVkUnmapMemory
#define vkFlushMappedMemoryRanges disabledVkFlushMappedMemoryRanges
#define vkIlwalidateMappedMemoryRanges disabledVkIlwalidateMappedMemoryRanges
#define vkGetDeviceMemoryCommitment disabledVkGetDeviceMemoryCommitment
#define vkBindBufferMemory disabledVkBindBufferMemory
#define vkBindImageMemory disabledVkBindImageMemory
#define vkGetBufferMemoryRequirements disabledVkGetBufferMemoryRequirements
#define vkGetImageMemoryRequirements disabledVkGetImageMemoryRequirements
#define vkGetImageSparseMemoryRequirements disabledVkGetImageSparseMemoryRequirements
#define vkGetPhysicalDeviceSparseImageFormatProperties disabledVkGetPhysicalDeviceSparseImageFormatProperties
#define vkQueueBindSparse disabledVkQueueBindSparse
#define vkCreateFence disabledVkCreateFence
#define vkDestroyFence disabledVkDestroyFence
#define vkResetFences disabledVkResetFences
#define vkGetFenceStatus disabledVkGetFenceStatus
#define vkWaitForFences disabledVkWaitForFences
#define vkCreateSemaphore disabledVkCreateSemaphore
#define vkDestroySemaphore disabledVkDestroySemaphore
#define vkCreateEvent disabledVkCreateEvent
#define vkDestroyEvent disabledVkDestroyEvent
#define vkGetEventStatus disabledVkGetEventStatus
#define vkSetEvent disabledVkSetEvent
#define vkResetEvent disabledVkResetEvent
#define vkCreateQueryPool disabledVkCreateQueryPool
#define vkDestroyQueryPool disabledVkDestroyQueryPool
#define vkGetQueryPoolResults disabledVkGetQueryPoolResults
#define vkCreateBuffer disabledVkCreateBuffer
#define vkDestroyBuffer disabledVkDestroyBuffer
#define vkCreateBufferView disabledVkCreateBufferView
#define vkDestroyBufferView disabledVkDestroyBufferView
#define vkCreateImage disabledVkCreateImage
#define vkDestroyImage disabledVkDestroyImage
#define vkGetImageSubresourceLayout disabledVkGetImageSubresourceLayout
#define vkCreateImageView disabledVkCreateImageView
#define vkDestroyImageView disabledVkDestroyImageView
#define vkCreateShaderModule disabledVkCreateShaderModule
#define vkDestroyShaderModule disabledVkDestroyShaderModule
#define vkCreatePipelineCache disabledVkCreatePipelineCache
#define vkDestroyPipelineCache disabledVkDestroyPipelineCache
#define vkGetPipelineCacheData disabledVkGetPipelineCacheData
#define vkMergePipelineCaches disabledVkMergePipelineCaches
#define vkCreateGraphicsPipelines disabledVkCreateGraphicsPipelines
#define vkCreateComputePipelines disabledVkCreateComputePipelines
#define vkDestroyPipeline disabledVkDestroyPipeline
#define vkCreatePipelineLayout disabledVkCreatePipelineLayout
#define vkDestroyPipelineLayout disabledVkDestroyPipelineLayout
#define vkCreateSampler disabledVkCreateSampler
#define vkDestroySampler disabledVkDestroySampler
#define vkCreateDescriptorSetLayout disabledVkCreateDescriptorSetLayout
#define vkDestroyDescriptorSetLayout disabledVkDestroyDescriptorSetLayout
#define vkCreateDescriptorPool disabledVkCreateDescriptorPool
#define vkDestroyDescriptorPool disabledVkDestroyDescriptorPool
#define vkResetDescriptorPool disabledVkResetDescriptorPool
#define vkAllocateDescriptorSets disabledVkAllocateDescriptorSets
#define vkFreeDescriptorSets disabledVkFreeDescriptorSets
#define vkUpdateDescriptorSets disabledVkUpdateDescriptorSets
#define vkCreateFramebuffer disabledVkCreateFramebuffer
#define vkDestroyFramebuffer disabledVkDestroyFramebuffer
#define vkCreateRenderPass disabledVkCreateRenderPass
#define vkDestroyRenderPass disabledVkDestroyRenderPass
#define vkGetRenderAreaGranularity disabledVkGetRenderAreaGranularity
#define vkCreateCommandPool disabledVkCreateCommandPool
#define vkDestroyCommandPool disabledVkDestroyCommandPool
#define vkResetCommandPool disabledVkResetCommandPool
#define vkAllocateCommandBuffers disabledVkAllocateCommandBuffers
#define vkFreeCommandBuffers disabledVkFreeCommandBuffers
#define vkBeginCommandBuffer disabledVkBeginCommandBuffer
#define vkEndCommandBuffer disabledVkEndCommandBuffer
#define vkResetCommandBuffer disabledVkResetCommandBuffer
#define vkCmdBindPipeline disabledVkCmdBindPipeline
#define vkCmdSetViewport disabledVkCmdSetViewport
#define vkCmdSetScissor disabledVkCmdSetScissor
#define vkCmdSetLineWidth disabledVkCmdSetLineWidth
#define vkCmdSetDepthBias disabledVkCmdSetDepthBias
#define vkCmdSetBlendConstants disabledVkCmdSetBlendConstants
#define vkCmdSetDepthBounds disabledVkCmdSetDepthBounds
#define vkCmdSetDepthCompareOpEXT disabledVkCmdSetDepthCompareOpEXT
#define vkCmdSetStencilCompareMask disabledVkCmdSetStencilCompareMask
#define vkCmdSetStencilOpEXT disabledVkCmdSetStencilOpEXT
#define vkCmdSetStencilWriteMask disabledVkCmdSetStencilWriteMask
#define vkCmdSetStencilReference disabledVkCmdSetStencilReference
#define vkCmdBindDescriptorSets disabledVkCmdBindDescriptorSets
#define vkCmdBindIndexBuffer disabledVkCmdBindIndexBuffer
#define vkCmdBindVertexBuffers disabledVkCmdBindVertexBuffers
#define vkCmdDraw disabledVkCmdDraw
#define vkCmdDrawIndexed disabledVkCmdDrawIndexed
#define vkCmdDrawIndirect disabledVkCmdDrawIndirect
#define vkCmdDrawIndexedIndirect disabledVkCmdDrawIndexedIndirect
#define vkCmdDispatch disabledVkCmdDispatch
#define vkCmdDispatchIndirect disabledVkCmdDispatchIndirect
#define vkCmdCopyBuffer disabledVkCmdCopyBuffer
#define vkCmdCopyImage disabledVkCmdCopyImage
#define vkCmdBlitImage disabledVkCmdBlitImage
#define vkCmdCopyBufferToImage disabledVkCmdCopyBufferToImage
#define vkCmdCopyImageToBuffer disabledVkCmdCopyImageToBuffer
#define vkCmdUpdateBuffer disabledVkCmdUpdateBuffer
#define vkCmdFillBuffer disabledVkCmdFillBuffer
#define vkCmdClearColorImage disabledVkCmdClearColorImage
#define vkCmdClearDepthStencilImage disabledVkCmdClearDepthStencilImage
#define vkCmdClearAttachments disabledVkCmdClearAttachments
#define vkCmdResolveImage disabledVkCmdResolveImage
#define vkCmdSetEvent disabledVkCmdSetEvent
#define vkCmdResetEvent disabledVkCmdResetEvent
#define vkCmdWaitEvents disabledVkCmdWaitEvents
#define vkCmdPipelineBarrier disabledVkCmdPipelineBarrier
#define vkCmdBeginQuery disabledVkCmdBeginQuery
#define vkCmdEndQuery disabledVkCmdEndQuery
#define vkCmdResetQueryPool disabledVkCmdResetQueryPool
#define vkCmdWriteTimestamp disabledVkCmdWriteTimestamp
#define vkCmdCopyQueryPoolResults disabledVkCmdCopyQueryPoolResults
#define vkCmdPushConstants disabledVkCmdPushConstants
#define vkCmdBeginRenderPass disabledVkCmdBeginRenderPass
#define vkCmdNextSubpass disabledVkCmdNextSubpass
#define vkCmdEndRenderPass disabledVkCmdEndRenderPass
#define vkCmdExelwteCommands disabledVkCmdExelwteCommands
#define vkDestroySurfaceKHR disabledDestroySurfaceKHR;
#define vkGetPhysicalDeviceSurfaceSupportKHR disabledGetPhysicalDeviceSurfaceSupportKHR
#define vkGetPhysicalDeviceSurfaceCapabilitiesKHR disabledGetPhysicalDeviceSurfaceCapabilitiesKHR
#define vkGetPhysicalDeviceSurfaceFormatsKHR disabledGetPhysicalDeviceSurfaceFormatsKHR
#define vkGetPhysicalDeviceSurfacePresentModesKHR disabledGetPhysicalDeviceSurfacePresentModesKHR
#define vkGetPhysicalDeviceDisplayPropertiesKHR disabledGetPhysicalDeviceDisplayPropertiesKHR
#define vkGetPhysicalDeviceDisplayPlanePropertiesKHR disabledGetPhysicalDeviceDisplayPlanePropertiesKHR
#define vkGetDisplayPlaneSupportedDisplaysKHR disabledGetDisplayPlaneSupportedDisplaysKHR
#define vkGetDisplayModePropertiesKHR disabledGetDisplayModePropertiesKHR
#define vkCreateDisplayModeKHR disabledCreateDisplayModeKHR
#define vkGetDisplayPlaneCapabilitiesKHR disabledGetDisplayPlaneCapabilitiesKHR
#define vkCreateDisplayPlaneSurfaceKHR disabledCreateDisplayPlaneSurfaceKHR
#define vkCreateWin32SurfaceKHR disabledCreateWin32SurfaceKHR
#define vkGetPhysicalDeviceWin32PresentationSupportKHR disabledGetPhysicalDeviceWin32PresentationSupportKHR
#define vkCreateSwapchainKHR disabledCreateSwapchainKHR
#define vkDestroySwapchainKHR disabledDestroySwapchainKHR
#define vkGetSwapchainImagesKHR disabledGetSwapchainImagesKHR
#define vkAcquireNextImageKHR disabledAcquireNextImageKHR
#define vkQueuePresentKHR disabledQueuePresentKHR
#define vkEnumerateInstanceVersion disabledVkEnumerateInstanceVersion
#define vkGetPhysicalDeviceFeatures2 disabledVkGetPhysicalDeviceFeatures2
#define vkGetPhysicalDeviceProperties2 disabledVkGetPhysicalDeviceProperties2
#define vkGetPhysicalDeviceFormatProperties2 disabledVkGetPhysicalDeviceFormatProperties2
#define vkGetPhysicalDeviceImageFormatProperties2 disabledVkGetPhysicalDeviceImageFormatProperties2
#define vkGetPhysicalDeviceQueueFamilyProperties2 disabledVkGetPhysicalDeviceQueueFamilyProperties2
#define vkGetPhysicalDeviceMemoryProperties2 disabledVkGetPhysicalDeviceMemoryProperties2
#define vkGetPhysicalDeviceSparseImageFormatProperties2 disabledVkGetPhysicalDeviceSparseImageFormatProperties2
#define vkGetPhysicalDeviceFeatures2KHR disabledVkGetPhysicalDeviceFeatures2KHR
#define vkGetPhysicalDeviceProperties2KHR disabledVkGetPhysicalDeviceProperties2KHR
#define vkGetPhysicalDeviceFormatProperties2KHR disabledVkGetPhysicalDeviceFormatProperties2KHR
#define vkGetPhysicalDeviceImageFormatProperties2KHR disabledVkGetPhysicalDeviceImageFormatProperties2KHR
#define vkGetPhysicalDeviceQueueFamilyProperties2KHR disabledVkGetPhysicalDeviceQueueFamilyProperties2KHR
#define vkGetPhysicalDeviceMemoryProperties2KHR disabledVkGetPhysicalDeviceMemoryProperties2KHR
#define vkGetPhysicalDeviceSparseImageFormatProperties2KHR disabledVkGetPhysicalDeviceSparseImageFormatProperties2KHR
#define vkGetPhysicalDeviceExternalBufferPropertiesKHR disabledVkGetPhysicalDeviceExternalBufferPropertiesKHR
#define vkGetPhysicalDeviceExternalSemaphorePropertiesKHR disabledVkGetPhysicalDeviceExternalSemaphorePropertiesKHR
#define vkGetPhysicalDeviceExternalFencePropertiesKHR disabledVkGetPhysicalDeviceExternalFencePropertiesKHR
#define vkGetBufferDeviceAddress disabledVkGetBufferDeviceAddress
#define vkCreateDebugReportCallbackEXT disabledVkCreateDebugReportCallbackEXT
#define vkDestroyDebugReportCallbackEXT disabledVkDestroyDebugReportCallbackEXT
#define vkDebugReportMessageEXT disabledVkDebugReportMessageEXT
#define vkDebugMarkerSetObjectTagEXT disabledVkDebugMarkerSetObjectTagEXT
#define vkDebugMarkerSetObjectNameEXT disabledVkDebugMarkerSetObjectNameEXT
#define vkCmdDebugMarkerBeginEXT disabledVkCmdDebugMarkerBeginEXT
#define vkCmdDebugMarkerEndEXT disabledVkCmdDebugMarkerEndEXT
#define vkCmdDebugMarkerInsertEXT disabledVkCmdDebugMarkerInsertEXT
#define vkCreateAccelerationStructureKHR disabledVkCreateAccelerationStructureKHR
#define vkDestroyAccelerationStructureKHR disabledVkDestroyAccelerationStructureKHR
#define vkGetAccelerationStructureBuildSizesKHR disabledVkGetAccelerationStructureBuildSizesKHR
#define vkGetAccelerationStructureDeviceAddressKHR disabledVkGetAccelerationStructureDeviceAddressKHR
#define vkCmdBuildAccelerationStructuresKHR disabledVkCmdBuildAccelerationStructuresKHR
#define vkCmdCopyAccelerationStructureKHR disabledVkCmdCopyAccelerationStructureKHR
#define vkCmdTraceRaysKHR disabledVkCmdTraceRaysKHR
#define vkCreateRayTracingPipelinesKHR disabledVkCreateRayTracingPipelinesKHR
#define vkGetRayTracingShaderGroupHandlesKHR disabledVkGetRayTracingShaderGroupHandlesKHR
#define vkCmdWriteAccelerationStructuresPropertiesKHR disabledVkCmdWriteAccelerationStructuresPropertiesKHR
#define vkGetPhysicalDeviceCooperativeMatrixPropertiesLW disabledVkGetPhysicalDeviceCooperativeMatrixPropertiesLW
