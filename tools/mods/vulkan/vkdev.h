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
#ifndef VK_DEVICE_H
#define VK_DEVICE_H

#include "vkmain.h"
#include "vkphysdev.h"
#include "vk_layer_dispatch_table.h"
#include <map>
#include <set>

//! \brief VkDevice wrapper that encapsulates one or more physical devices
class VulkanDevice : private VkLayerDispatchTable
{
public:
    enum VkExtensionId
    {
        ExtVK_ANDROID_native_buffer
        ,ExtVK_ANDROID_external_memory_android_hardware_buffer
        ,ExtVK_EXT_acquire_xlib_display
        ,ExtVK_EXT_qnxscreen_surface
        ,ExtVK_LWX_queue_priority_realtime
        ,ExtVK_KHX_multiview
        ,ExtVK_KHX_device_group_creation
        ,ExtVK_KHX_device_group
        ,ExtVK_LW_texture_dirty_tile_map
        ,ExtVK_LW_corner_sampled_image
        ,ExtVK_LW_internal
        ,ExtVK_LW_shading_rate_image
        ,ExtVK_KHR_android_surface
        ,ExtVK_KHR_surface
        ,ExtVK_KHR_swapchain
        ,ExtVK_KHR_display
        ,ExtVK_KHR_display_swapchain
        ,ExtVK_KHR_sampler_mirror_clamp_to_edge
        ,ExtVK_KHR_multiview
        ,ExtVK_KHR_get_physical_device_properties2
        ,ExtVK_KHR_device_group
        ,ExtVK_KHR_shader_draw_parameters
        ,ExtVK_KHR_maintenance1
        ,ExtVK_KHR_device_group_creation
        ,ExtVK_KHR_external_memory_capabilities
        ,ExtVK_KHR_external_memory
        ,ExtVK_KHR_external_memory_fd
        ,ExtVK_KHR_external_semaphore_capabilities
        ,ExtVK_KHR_external_semaphore
        ,ExtVK_KHR_external_semaphore_fd
        ,ExtVK_KHR_push_descriptor
        ,ExtVK_KHR_16bit_storage
        ,ExtVK_KHR_incremental_present
        ,ExtVK_KHR_descriptor_update_template
        ,ExtVK_KHR_shared_presentable_image
        ,ExtVK_KHR_external_fence_capabilities
        ,ExtVK_KHR_external_fence
        ,ExtVK_KHR_external_fence_fd
        ,ExtVK_KHR_maintenance2
        ,ExtVK_KHR_get_surface_capabilities2
        ,ExtVK_KHR_variable_pointers
        ,ExtVK_KHR_get_display_properties2
        ,ExtVK_KHR_dedicated_allocation
        ,ExtVK_KHR_storage_buffer_storage_class
        ,ExtVK_KHR_relaxed_block_layout
        ,ExtVK_KHR_get_memory_requirements2
        ,ExtVK_KHR_image_format_list
        ,ExtVK_KHR_sampler_ycbcr_colwersion
        ,ExtVK_KHR_bind_memory2
        ,ExtVK_KHR_maintenance3
        ,ExtVK_EXT_debug_report
        ,ExtVK_LW_glsl_shader
        ,ExtVK_EXT_depth_range_unrestricted
        ,ExtVK_IMG_filter_lwbic
        ,ExtVK_AMD_rasterization_order
        ,ExtVK_AMD_shader_trinary_minmax
        ,ExtVK_AMD_shader_explicit_vertex_parameter
        ,ExtVK_EXT_debug_marker
        ,ExtVK_AMD_gcn_shader
        ,ExtVK_LW_dedicated_allocation
        ,ExtVK_AMD_draw_indirect_count
        ,ExtVK_AMD_negative_viewport_height
        ,ExtVK_AMD_gpu_shader_half_float
        ,ExtVK_AMD_shader_ballot
        ,ExtVK_AMD_texture_gather_bias_lod
        ,ExtVK_AMD_shader_info
        ,ExtVK_AMD_shader_image_load_store_lod
        ,ExtVK_IMG_format_pvrtc
        ,ExtVK_LW_external_memory_capabilities
        ,ExtVK_LW_external_memory
        ,ExtVK_EXT_validation_flags
        ,ExtVK_EXT_shader_subgroup_ballot
        ,ExtVK_EXT_shader_subgroup_vote
        ,ExtVK_LWX_device_generated_commands
        ,ExtVK_LW_clip_space_w_scaling
        ,ExtVK_EXT_direct_mode_display
        ,ExtVK_EXT_display_surface_counter
        ,ExtVK_EXT_display_control
        ,ExtVK_GOOGLE_display_timing
        ,ExtVK_LW_sample_mask_override_coverage
        ,ExtVK_LW_geometry_shader_passthrough
        ,ExtVK_LW_viewport_array2
        ,ExtVK_LWX_multiview_per_view_attributes
        ,ExtVK_LW_viewport_swizzle
        ,ExtVK_EXT_discard_rectangles
        ,ExtVK_EXT_swapchain_colorspace
        ,ExtVK_EXT_hdr_metadata
        ,ExtVK_EXT_sampler_filter_minmax
        ,ExtVK_AMD_gpu_shader_int16
        ,ExtVK_AMD_mixed_attachment_samples
        ,ExtVK_AMD_shader_fragment_mask
        ,ExtVK_EXT_shader_stencil_export
        ,ExtVK_EXT_sample_locations
        ,ExtVK_EXT_blend_operation_advanced
        ,ExtVK_LW_fragment_coverage_to_color
        ,ExtVK_LW_framebuffer_mixed_samples
        ,ExtVK_LW_fill_rectangle
        ,ExtVK_EXT_post_depth_coverage
        ,ExtVK_EXT_validation_cache
        ,ExtVK_EXT_shader_viewport_index_layer
        ,ExtVK_EXT_global_priority
        ,ExtVK_MVK_ios_surface
        ,ExtVK_MVK_macos_surface
        ,ExtVK_KHR_mir_surface
        ,ExtVK_NN_vi_surface
        ,ExtVK_KHR_wayland_surface
        ,ExtVK_KHR_win32_surface
        ,ExtVK_KHR_external_memory_win32
        ,ExtVK_KHR_win32_keyed_mutex
        ,ExtVK_KHR_external_semaphore_win32
        ,ExtVK_KHR_external_fence_win32
        ,ExtVK_LW_external_memory_win32
        ,ExtVK_LW_win32_keyed_mutex
        ,ExtVK_KHR_xcb_surface
        ,ExtVK_KHR_xlib_surface
        ,ExtVK_EXT_conditional_rendering
        ,ExtVK_EXT_conservative_rasterization
        ,ExtVK_EXT_depth_clip_enable
        ,ExtVK_EXT_host_query_reset
        ,ExtVK_EXT_memory_budget
        ,ExtVK_EXT_pci_bus_info
        ,ExtVK_EXT_vertex_attribute_divisor
        ,ExtVK_EXT_ycbcr_image_arrays
        ,ExtVK_KHR_create_renderpass2
        ,ExtVK_KHR_depth_stencil_resolve
        ,ExtVK_KHR_draw_indirect_count
        ,ExtVK_KHR_driver_properties
        ,ExtVK_KHR_imageless_framebuffer
        ,ExtVK_KHR_shader_atomic_int64
        ,ExtVK_LW_compute_shader_derivatives
        ,ExtVK_LW_coverage_reduction_mode
        ,ExtVK_LW_dedicated_allocation_image_aliasing
        ,ExtVK_LW_device_diagnostic_checkpoints
        ,ExtVK_LW_fragment_shader_barycentric
        ,ExtVK_LW_mesh_shader
        ,ExtVK_LW_representative_fragment_test
        ,ExtVK_LW_scissor_exclusive
        ,ExtVK_LW_shader_image_footprint
        ,ExtVK_LW_shader_subgroup_partitioned
        ,ExtVK_KHX_external_memory
        ,ExtVK_KHX_external_memory_win32
        ,ExtVK_KHX_external_semaphore
        ,ExtVK_KHX_external_semaphore_win32
        ,ExtVK_KHX_win32_keyed_mutex
        ,ExtVK_LWX_display_timing
        ,ExtVK_KHR_shader_clock
        ,ExtVK_LW_shader_sm_builtins
        ,ExtVK_LW_gpu_shader5
        ,ExtVK_LW_shader_thread_group
        ,ExtVK_KHR_ray_tracing
        ,ExtVK_KHR_ray_query
        ,ExtVK_KHR_acceleration_structure
        ,ExtVK_KHR_shader_float16_int8
        ,ExtGL_LW_shader_sm_builtins
        ,ExtVK_LW_cooperative_matrix
        ,ExtVK_EXT_calibrated_timestamps
        ,ExtVK_EXT_extended_dynamic_state
        ,ExtNUM_EXTENSIONS
        ,ExtNO_SUCH_EXTENSION = ExtNUM_EXTENSIONS
    };

    struct PexAddr { UINT32 domain, bus, device, function; };

    VulkanDevice
    (
        VkPhysicalDevice physicalDevice,
        VulkanInstance* pVkInst,
        bool printExtensions = false
    );
    ~VulkanDevice();

    bool operator==(const VulkanDevice& vulkanDevice) const;
    bool operator!=(const VulkanDevice& vulkanDevice) const;

    //! \brief prevent a specific extension from being enbabled. This may cause the test to fail!
    static void DisableExt(const string& extension);
    //! \brief add a specific extension to be enabled.
    static void EnableExt(const string& extension);

    //! \brief return true is the specific extension is supported and enabled
    bool HasExt(VkExtensionId extId);

    //! \brief return true is the specific extension is supported and enabled
    bool HasExt(const char* extName);

    bool IsInitialized() const { return m_VkDevice != VK_NULL_HANDLE; } //$
    //! \brief retrieve the current list of supported extensions and disable those that cause
    //!        problems for us.
    VkResult GetExtensions(bool bPrintExtensions = false);

    //! \brief Creates the VkDevice and VkQueues
#ifdef ENABLE_UNIT_TESTING
    VkResult Initialize(UINT32 unitTestingVariantIndex);
#endif
    VkResult Initialize
    (
        const vector<string>& extensionNames,
        bool bPrintExtensions = false,
        UINT32 numGraphicsQueues = 1,
        UINT32 numTransferQueues = 0,
        UINT32 numComputeQueues = 0
    );
    void Shutdown();

    VkQueue GetGraphicsQueue(UINT32 queueIdx) const;
    VkQueue GetComputeQueue(UINT32 queueIdx) const;
    VkQueue GetTransferQueue(UINT32 queueIdx) const;

    SETGET_PROP(TimeoutNs, UINT64);
    GET_PROP(PipelineCache, VkPipelineCache);
    VulkanPhysicalDevice* GetPhysicalDevice() const;
    VkDevice GetVkDevice() const { return m_VkDevice; }

    VulkanInstance* GetVulkanInstance() const { return m_pVulkanInst; }

private:
    VulkanInstance* m_pVulkanInst = nullptr;

    VkDevice m_VkDevice = VK_NULL_HANDLE;

    // TODO: Support multiple physical devices (SLI)
    unique_ptr<VulkanPhysicalDevice> m_pPhysicalDevice;

    // Support for multiple queues per family
    vector<VkQueue> m_GraphicsQueues;
    vector<VkQueue> m_ComputeQueues;
    vector<VkQueue> m_TransferQueues;

    vector<VkDeviceQueueCreateInfo> CreateQueueInfos
    (
        UINT32 numGraphicsQueues,
        UINT32 numTransferQueues,
        UINT32 numComputeQueues
    );

    map<VulkanQueueFamilyIndex, vector<float>> m_QueuePriorities;

    UINT64 m_TimeoutNs = FENCE_TIMEOUT;

    // Vulkan does not have an API to enumerate the enabled extensions so we
    // need to maintain this list ourselves.
    std::set<string> m_EnabledExtensionNames;
    std::set<string> m_RegisteredExtensionNames;

    // Maintain a list of extensions the user wants enabled/disabled via JS.
    static std::set<string> s_EnabledExtensionNames;
    static std::set<string> s_DisabledExtensionNames;

    // Maintain a quick lookup list of enabled extensions
    bool m_HasExt[ExtNUM_EXTENSIONS];

    void                     UpdateFeatures();

    VkPipelineCache m_PipelineCache = VK_NULL_HANDLE;

    //! \brief Add commonly used extensions
    void AddDefaultExtensions();

public:
    // **************************** Vulkan APIs ********************************

    // Expose all VkLayerDispatchTable Vulkan APIs that do not take a VkDevice
    // parameter here.

    // Core APIs
    using VkLayerDispatchTable::QueueSubmit;
    using VkLayerDispatchTable::QueueWaitIdle;
    using VkLayerDispatchTable::QueueBindSparse;
    using VkLayerDispatchTable::BeginCommandBuffer;
    using VkLayerDispatchTable::EndCommandBuffer;
    using VkLayerDispatchTable::ResetCommandBuffer;
    using VkLayerDispatchTable::CmdBindPipeline;
    using VkLayerDispatchTable::CmdSetViewport;
    using VkLayerDispatchTable::CmdSetScissor;
    using VkLayerDispatchTable::CmdSetLineWidth;
    using VkLayerDispatchTable::CmdSetDepthBias;
    using VkLayerDispatchTable::CmdSetBlendConstants;
    using VkLayerDispatchTable::CmdSetDepthBounds;
    using VkLayerDispatchTable::CmdSetDepthCompareOpEXT;
    using VkLayerDispatchTable::CmdSetStencilCompareMask;
    using VkLayerDispatchTable::CmdSetStencilOpEXT;
    using VkLayerDispatchTable::CmdSetStencilWriteMask;
    using VkLayerDispatchTable::CmdSetStencilReference;
    using VkLayerDispatchTable::CmdBindDescriptorSets;
    using VkLayerDispatchTable::CmdBindIndexBuffer;
    using VkLayerDispatchTable::CmdBindVertexBuffers;
    using VkLayerDispatchTable::CmdDraw;
    using VkLayerDispatchTable::CmdDrawIndexed;
    using VkLayerDispatchTable::CmdDrawIndirect;
    using VkLayerDispatchTable::CmdDrawIndexedIndirect;
    using VkLayerDispatchTable::CmdDispatch;
    using VkLayerDispatchTable::CmdDispatchIndirect;
    using VkLayerDispatchTable::CmdCopyBuffer;
    using VkLayerDispatchTable::CmdCopyImage;
    using VkLayerDispatchTable::CmdBlitImage;
    using VkLayerDispatchTable::CmdCopyBufferToImage;
    using VkLayerDispatchTable::CmdCopyImageToBuffer;
    using VkLayerDispatchTable::CmdUpdateBuffer;
    using VkLayerDispatchTable::CmdFillBuffer;
    using VkLayerDispatchTable::CmdClearColorImage;
    using VkLayerDispatchTable::CmdClearDepthStencilImage;
    using VkLayerDispatchTable::CmdClearAttachments;
    using VkLayerDispatchTable::CmdResolveImage;
    using VkLayerDispatchTable::CmdSetEvent;
    using VkLayerDispatchTable::CmdResetEvent;
    using VkLayerDispatchTable::CmdWaitEvents;
    using VkLayerDispatchTable::CmdPipelineBarrier;
    using VkLayerDispatchTable::CmdBeginQuery;
    using VkLayerDispatchTable::CmdEndQuery;
    using VkLayerDispatchTable::CmdResetQueryPool;
    using VkLayerDispatchTable::CmdWriteTimestamp;
    using VkLayerDispatchTable::CmdCopyQueryPoolResults;
    using VkLayerDispatchTable::CmdPushConstants;
    using VkLayerDispatchTable::CmdBeginRenderPass;
    using VkLayerDispatchTable::CmdNextSubpass;
    using VkLayerDispatchTable::CmdEndRenderPass;
    using VkLayerDispatchTable::CmdExelwteCommands;

    // Extensions
    using VkLayerDispatchTable::QueuePresentKHR;
    using VkLayerDispatchTable::CmdPushDescriptorSetKHR;
    using VkLayerDispatchTable::CmdPushDescriptorSetWithTemplateKHR;
    using VkLayerDispatchTable::DebugMarkerSetObjectTagEXT;
    using VkLayerDispatchTable::DebugMarkerSetObjectNameEXT;
    using VkLayerDispatchTable::CmdDebugMarkerBeginEXT;
    using VkLayerDispatchTable::CmdDebugMarkerEndEXT;
    using VkLayerDispatchTable::CmdDebugMarkerInsertEXT;
    //using VkLayerDispatchTable::CmdDrawIndirectCountAMD; temporarily disabled until driver is updated with new KHR function
    //using VkLayerDispatchTable::CmdDrawIndexedIndirectCountAMD; temporarily disabled until driver is updated with new KHR function
    using VkLayerDispatchTable::CmdSetDiscardRectangleEXT;

    // VK_KHR_ray_tracing_pipeline extension commands
    using VkLayerDispatchTable::CmdBuildAccelerationStructuresKHR;
    using VkLayerDispatchTable::CmdCopyAccelerationStructureKHR;
    using VkLayerDispatchTable::CmdTraceRaysKHR;
    using VkLayerDispatchTable::CmdWriteAccelerationStructuresPropertiesKHR;
    // Vulkan APIs that take a VkDevice parameter

    // Core APIs
    void GetDeviceQueue
    (
        uint32_t queueFamilyIndex,
        uint32_t queueIndex,
        VkQueue* pQueue
    ) const;

    VkResult DeviceWaitIdle() const;

    VkResult AllocateMemory
    (
        const VkMemoryAllocateInfo* pAllocateInfo,
        const VkAllocationCallbacks* pAllocator,
        VkDeviceMemory* pMemory
    ) const;

    void FreeMemory
    (
        VkDeviceMemory memory,
        const VkAllocationCallbacks* pAllocator = nullptr
    ) const;

    VkResult MapMemory
    (
        VkDeviceMemory memory,
        VkDeviceSize offset,
        VkDeviceSize size,
        VkMemoryMapFlags flags,
        void** ppData
    ) const;

    void UnmapMemory(VkDeviceMemory memory) const;

    VkResult FlushMappedMemoryRanges
    (
        uint32_t memoryRangeCount,
        const VkMappedMemoryRange* pMemoryRanges
    ) const;

    VkResult IlwalidateMappedMemoryRanges
    (
        uint32_t memoryRangeCount,
        const VkMappedMemoryRange* pMemoryRanges
    ) const;

    void GetDeviceMemoryCommitment
    (
        VkDeviceMemory memory,
        VkDeviceSize* pCommittedMemoryInBytes
    ) const;

    VkResult BindBufferMemory
    (
        VkBuffer buffer,
        VkDeviceMemory memory,
        VkDeviceSize memoryOffset
    ) const;

    VkResult BindImageMemory
    (
        VkImage image,
        VkDeviceMemory memory,
        VkDeviceSize memoryOffset
    ) const;

    void GetBufferMemoryRequirements
    (
        VkBuffer buffer,
        VkMemoryRequirements* pMemoryRequirements
    ) const;

    VkDeviceAddress GetBufferDeviceAddress
    (
        const VkBufferDeviceAddressInfo* pInfo
    ) const;

    void GetImageMemoryRequirements
    (
        VkImage image,
        VkMemoryRequirements* pMemoryRequirements
    ) const;

    void GetImageSparseMemoryRequirements
    (
        VkImage image,
        uint32_t* pSparseMemoryRequirementCount,
        VkSparseImageMemoryRequirements* pSparseMemoryRequirements
    ) const;

    VkResult CreateFence
    (
        const VkFenceCreateInfo* pCreateInfo,
        const VkAllocationCallbacks* pAllocator,
        VkFence* pFence
    ) const;

    void DestroyFence
    (
        VkFence fence,
        const VkAllocationCallbacks* pAllocator = nullptr
    ) const;

    VkResult ResetFences
    (
        uint32_t fenceCount,
        const VkFence* pFences
    ) const;

    VkResult GetFenceStatus(VkFence fence) const;

    VkResult WaitForFences
    (
        uint32_t fenceCount,
        const VkFence* pFences,
        VkBool32 waitAll,
        uint64_t timeout
    ) const;

    VkResult CreateSemaphore
    (
        const VkSemaphoreCreateInfo* pCreateInfo,
        const VkAllocationCallbacks* pAllocator,
        VkSemaphore* pSemaphore
    ) const;

    void DestroySemaphore
    (
        VkSemaphore semaphore,
        const VkAllocationCallbacks* pAllocator = nullptr
    ) const;

    VkResult CreateEvent
    (
        const VkEventCreateInfo* pCreateInfo,
        const VkAllocationCallbacks* pAllocator,
        VkEvent* pEvent
    ) const;

    void DestroyEvent
    (
        VkEvent event,
        const VkAllocationCallbacks* pAllocator = nullptr
    ) const;

    VkResult GetEventStatus(VkEvent event) const;

    VkResult SetEvent(VkEvent event) const;

    VkResult ResetEvent(VkEvent event) const;

    VkResult CreateQueryPool
    (
        const VkQueryPoolCreateInfo* pCreateInfo,
        const VkAllocationCallbacks* pAllocator,
        VkQueryPool* pQueryPool
    ) const;

    void DestroyQueryPool
    (
        VkQueryPool queryPool,
        const VkAllocationCallbacks* pAllocator = nullptr
    ) const;

    VkResult GetQueryPoolResults
    (
        VkQueryPool queryPool,
        uint32_t firstQuery,
        uint32_t queryCount,
        size_t dataSize,
        void* pData,
        VkDeviceSize stride,
        VkQueryResultFlags flags
    ) const;

    VkResult CreateBuffer
    (
        const VkBufferCreateInfo* pCreateInfo,
        const VkAllocationCallbacks* pAllocator,
        VkBuffer* pBuffer
    ) const;

    void DestroyBuffer
    (
        VkBuffer buffer,
        const VkAllocationCallbacks* pAllocator = nullptr
    ) const;

    VkResult CreateBufferView
    (
        const VkBufferViewCreateInfo* pCreateInfo,
        const VkAllocationCallbacks* pAllocator,
        VkBufferView* pView
    ) const;

    void DestroyBufferView
    (
        VkBufferView bufferView,
        const VkAllocationCallbacks* pAllocator = nullptr
    ) const;

    VkResult CreateImage
    (
        const VkImageCreateInfo* pCreateInfo,
        const VkAllocationCallbacks* pAllocator,
        VkImage* pImage
    ) const;

    void DestroyImage
    (
        VkImage image,
        const VkAllocationCallbacks* pAllocator = nullptr
    ) const;

    void GetImageSubresourceLayout
    (
        VkImage image,
        const VkImageSubresource* pSubresource,
        VkSubresourceLayout* pLayout
    ) const;

    VkResult CreateImageView
    (
        const VkImageViewCreateInfo* pCreateInfo,
        const VkAllocationCallbacks* pAllocator,
        VkImageView* pView
    ) const;

    void DestroyImageView
    (
        VkImageView imageView,
        const VkAllocationCallbacks* pAllocator = nullptr
    ) const;

    VkResult CreateShaderModule
    (
        const VkShaderModuleCreateInfo* pCreateInfo,
        const VkAllocationCallbacks* pAllocator,
        VkShaderModule* pShaderModule
    ) const;

    void DestroyShaderModule
    (
        VkShaderModule shaderModule,
        const VkAllocationCallbacks* pAllocator = nullptr
    ) const;

    VkResult CreatePipelineCache
    (
        const VkPipelineCacheCreateInfo* pCreateInfo,
        const VkAllocationCallbacks* pAllocator,
        VkPipelineCache* pPipelineCache
    ) const;

    void DestroyPipelineCache
    (
        VkPipelineCache pipelineCache,
        const VkAllocationCallbacks* pAllocator = nullptr
    ) const;

    VkResult GetPipelineCacheData
    (
        VkPipelineCache pipelineCache,
        size_t* pDataSize,
        void* pData
    ) const;

    VkResult MergePipelineCaches
    (
        VkPipelineCache dstCache,
        uint32_t srcCacheCount,
        const VkPipelineCache* pSrcCaches
    ) const;

    VkResult CreateGraphicsPipelines
    (
        VkPipelineCache pipelineCache,
        uint32_t createInfoCount,
        const VkGraphicsPipelineCreateInfo* pCreateInfos,
        const VkAllocationCallbacks* pAllocator,
        VkPipeline* pPipelines
    ) const;

    VkResult CreateComputePipelines
    (
        VkPipelineCache pipelineCache,
        uint32_t createInfoCount,
        const VkComputePipelineCreateInfo* pCreateInfos,
        const VkAllocationCallbacks* pAllocator,
        VkPipeline* pPipelines
    ) const;

    void DestroyPipeline
    (
        VkPipeline pipeline,
        const VkAllocationCallbacks* pAllocator = nullptr
    ) const;

    VkResult CreatePipelineLayout
    (
        const VkPipelineLayoutCreateInfo* pCreateInfo,
        const VkAllocationCallbacks* pAllocator,
        VkPipelineLayout* pPipelineLayout
    ) const;

    void DestroyPipelineLayout
    (
        VkPipelineLayout pipelineLayout,
        const VkAllocationCallbacks* pAllocator = nullptr
    ) const;

    VkResult CreateSampler
    (
        const VkSamplerCreateInfo* pCreateInfo,
        const VkAllocationCallbacks* pAllocator,
        VkSampler* pSampler
    ) const;

    void DestroySampler
    (
        VkSampler sampler,
        const VkAllocationCallbacks* pAllocator = nullptr
    ) const;

    VkResult CreateDescriptorSetLayout
    (
        const VkDescriptorSetLayoutCreateInfo* pCreateInfo,
        const VkAllocationCallbacks* pAllocator,
        VkDescriptorSetLayout* pSetLayout
    ) const;

    void DestroyDescriptorSetLayout
    (
        VkDescriptorSetLayout descriptorSetLayout,
        const VkAllocationCallbacks* pAllocator = nullptr
    ) const;

    VkResult CreateDescriptorPool
    (
        const VkDescriptorPoolCreateInfo* pCreateInfo,
        const VkAllocationCallbacks* pAllocator,
        VkDescriptorPool* pDescriptorPool
    ) const;

    void DestroyDescriptorPool
    (
        VkDescriptorPool descriptorPool,
        const VkAllocationCallbacks* pAllocator = nullptr
    ) const;

    VkResult ResetDescriptorPool
    (
        VkDescriptorPool descriptorPool,
        VkDescriptorPoolResetFlags flags
    ) const;

    VkResult AllocateDescriptorSets
    (
        const VkDescriptorSetAllocateInfo* pAllocateInfo,
        VkDescriptorSet* pDescriptorSets
    ) const;

    VkResult FreeDescriptorSets
    (
        VkDescriptorPool descriptorPool,
        uint32_t descriptorSetCount,
        const VkDescriptorSet* pDescriptorSets
    ) const;

    void UpdateDescriptorSets
    (
        uint32_t descriptorWriteCount,
        const VkWriteDescriptorSet* pDescriptorWrites,
        uint32_t descriptorCopyCount,
        const VkCopyDescriptorSet* pDescriptorCopies
    ) const;

    VkResult CreateFramebuffer
    (
        const VkFramebufferCreateInfo* pCreateInfo,
        const VkAllocationCallbacks* pAllocator,
        VkFramebuffer* pFramebuffer
    ) const;

    void DestroyFramebuffer
    (
        VkFramebuffer framebuffer,
        const VkAllocationCallbacks* pAllocator = nullptr
    ) const;

    VkResult CreateRenderPass
    (
        const VkRenderPassCreateInfo* pCreateInfo,
        const VkAllocationCallbacks* pAllocator,
        VkRenderPass* pRenderPass
    ) const;

    void DestroyRenderPass
    (
        VkRenderPass renderPass,
        const VkAllocationCallbacks* pAllocator = nullptr
    ) const;

    void GetRenderAreaGranularity
    (
        VkRenderPass renderPass,
        VkExtent2D* pGranularity
    ) const;

    VkResult CreateCommandPool
    (
        const VkCommandPoolCreateInfo* pCreateInfo,
        const VkAllocationCallbacks* pAllocator,
        VkCommandPool* pCommandPool
    ) const;

    void DestroyCommandPool
    (
        VkCommandPool commandPool,
        const VkAllocationCallbacks* pAllocator = nullptr
    ) const;

    VkResult ResetCommandPool
    (
        VkCommandPool commandPool,
        VkCommandPoolResetFlags flags
    ) const;

    VkResult AllocateCommandBuffers
    (
        const VkCommandBufferAllocateInfo* pAllocateInfo,
        VkCommandBuffer* pCommandBuffers
    ) const;

    void FreeCommandBuffers
    (
        VkCommandPool commandPool,
        uint32_t commandBufferCount,
        const VkCommandBuffer* pCommandBuffers
    ) const;

    // Extensions

    VkResult CreateSwapchainKHR
    (
        const VkSwapchainCreateInfoKHR* pCreateInfo,
        const VkAllocationCallbacks* pAllocator,
        VkSwapchainKHR* pSwapchain
    ) const;

    void DestroySwapchainKHR
    (
        VkSwapchainKHR swapchain,
        const VkAllocationCallbacks* pAllocator = nullptr
    ) const;

    VkResult GetSwapchainImagesKHR
    (
        VkSwapchainKHR swapchain,
        uint32_t* pSwapchainImageCount,
        VkImage* pSwapchainImages
    ) const;

    VkResult AcquireNextImageKHR
    (
        VkSwapchainKHR swapchain,
        uint64_t timeout,
        VkSemaphore semaphore,
        VkFence fence,
        uint32_t* pImageIndex
    ) const;

    // VK_KHR_acceleration_structure and VK_KHR_ray_tracing_pipeline extension commands
    VkResult CreateAccelerationStructureKHR
    (
        const VkAccelerationStructureCreateInfoKHR* pCreateInfo,
        const VkAllocationCallbacks* pAllocator,
        VkAccelerationStructureKHR* pAccelerationStructure
    ) const;

    void DestroyAccelerationStructureKHR
    (
        VkAccelerationStructureKHR accelerationStructure,
        const VkAllocationCallbacks* pAllocator
    ) const;

    void GetAccelerationStructureBuildSizesKHR
    (
        VkAccelerationStructureBuildTypeKHR buildType,
        const VkAccelerationStructureBuildGeometryInfoKHR* pBuildInfo,
        const uint32_t* pMaxPrimitiveCounts,
        VkAccelerationStructureBuildSizesInfoKHR* pSizeInfo
    ) const;

    VkDeviceAddress GetAccelerationStructureDeviceAddressKHR
    (
        const VkAccelerationStructureDeviceAddressInfoKHR* pInfo
    ) const;

    VkResult CreateRayTracingPipelinesKHR
    (
        VkDeferredOperationKHR deferredOperation,
        VkPipelineCache pipelineCache,
        uint32_t createInfoCount,
        const VkRayTracingPipelineCreateInfoKHR* pCreateInfos,
        const VkAllocationCallbacks* pAllocator,
        VkPipeline* pPipelines
    ) const;

    VkResult GetRayTracingShaderGroupHandlesKHR
    (
        VkPipeline pipeline,
        uint32_t firstGroup,
        uint32_t groupCount,
        size_t dataSize,
        void* pData
    ) const;

    VkResult GetPhysicalDeviceCooperativeMatrixPropertiesLW
    (
        VkPhysicalDevice physicalDevice,
        uint32_t* pPropertyCount,
        VkCooperativeMatrixPropertiesLW* pProperties
    );

    // TODO: Add support for extensions like device_group (SLI)

    // ************************** END Vulkan APIs *****************************
};

#endif // VK_DEVICE_H

