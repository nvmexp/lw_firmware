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

#include "vkdev.h"
#include "vklayers.h"
#include "vkinstance.h"

set<string> VulkanDevice::s_EnabledExtensionNames;
set<string> VulkanDevice::s_DisabledExtensionNames;

VulkanDevice::VulkanDevice
(
    VkPhysicalDevice physicalDevice,
    VulkanInstance* pVkInst,
    bool printExtensions
) : m_pVulkanInst(pVkInst)
   ,m_pPhysicalDevice(make_unique<VulkanPhysicalDevice>(physicalDevice, pVkInst, printExtensions))
{
    MASSERT(physicalDevice != VK_NULL_HANDLE);
    MASSERT(pVkInst);
}

VulkanDevice::~VulkanDevice()
{
    Shutdown();
}

void VulkanDevice::Shutdown()
{
    if (m_PipelineCache)
    {
        DestroyPipelineCache(m_PipelineCache);
        m_PipelineCache = VK_NULL_HANDLE;
    }
    if (m_VkDevice)
    {
        VkLayerDispatchTable::DestroyDevice(m_VkDevice, nullptr);
    }
    m_VkDevice = VK_NULL_HANDLE;
}

bool VulkanDevice::operator==(const VulkanDevice& vulkanDevice) const
{
    // Compare VkDevice handles
    if (m_VkDevice != vulkanDevice.m_VkDevice)
        return false;

    for (UINT32 i=0; i < m_GraphicsQueues.size(); i++)
    {
        if (m_GraphicsQueues[i] != vulkanDevice.GetGraphicsQueue(i))
        {
            return false;
        }
    }

    for (UINT32 i=0; i < m_ComputeQueues.size(); i++)
    {
        if (m_ComputeQueues[i] != vulkanDevice.GetComputeQueue(i))
        {
            return false;
        }
    }

    for (UINT32 i=0; i < m_TransferQueues.size(); i++)
    {
        if (m_TransferQueues[i] != vulkanDevice.GetTransferQueue(i))
        {
            return false;
        }
    }

    if (*m_pPhysicalDevice.get() != *vulkanDevice.m_pPhysicalDevice.get())
        return false;

    return true;
}

bool VulkanDevice::operator!=(const VulkanDevice& vulkanDevice) const
{
    return !(*this == vulkanDevice);
}

VkResult VulkanDevice::Initialize
(
    const vector <string>& extensionNames,
    bool bPrintExtensions,
    UINT32 numGraphicsQueues,
    UINT32 numTransferQueues,
    UINT32 numComputeQueues
)
{
    PROFILE_ZONE(VULKAN)

    // Make sure Initialize() is not called multiple times
    if (m_VkDevice)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Range check the input
    if (numGraphicsQueues > m_pPhysicalDevice->GetMaxGraphicsQueues() ||
        numComputeQueues > m_pPhysicalDevice->GetMaxComputeQueues() ||
        numTransferQueues > m_pPhysicalDevice->GetMaxTransferQueues())
    {
        Printf(Tee::PriError, "MaxGraphicsQueues=%d MaxComputeQueues:%d MaxTransferQeueues:%d\n",
            m_pPhysicalDevice->GetMaxGraphicsQueues(),
            m_pPhysicalDevice->GetMaxComputeQueues(),
            m_pPhysicalDevice->GetMaxTransferQueues());
        Printf(Tee::PriError, "ReqGraphicsQueues:%d, ReqComputeQueues:%d, ReqTransferQueues:%d\n",
            numGraphicsQueues,
            numComputeQueues,
            numTransferQueues);
        Printf(Tee::PriError, "Available number of queues does not meet requirement\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkResult res = VK_SUCCESS;
    const auto queueInfos = CreateQueueInfos(numGraphicsQueues,
                                             numTransferQueues,
                                             numComputeQueues);

    CHECK_VK_RESULT(GetExtensions(bPrintExtensions));

    AddDefaultExtensions();

    // First add any extensions the test is requiring
    for (const auto& ext : extensionNames)
    {
        m_EnabledExtensionNames.insert(ext);
    }
    // Next add any extensions the user (via JS) is enabling
    for (const auto& ext : s_EnabledExtensionNames)
    {
        m_EnabledExtensionNames.insert(ext);
    }
    // Finally remove any extensions the user (vis Js) is disabling
    for (const auto& ext : s_DisabledExtensionNames)
    {
        m_EnabledExtensionNames.erase(ext);
    }
    // Make sure the enabled extensions are present in the registered list
    for (const auto& ext : m_EnabledExtensionNames)
    {
        if (m_RegisteredExtensionNames.find(ext) == m_RegisteredExtensionNames.end())
        {
            Printf(Tee::PriWarn, "%s is not found in the registered extensions\n", ext.c_str());
        }
    }

    vector<const char*> extensionNamesCstr(m_EnabledExtensionNames.size());
    const Tee::Priority pri = bPrintExtensions ? Tee::PriNormal : Tee::PriLow;
    Printf(pri, VkMods::GetTeeModuleCode(), "Enabling %ld device extensions:\n",
           m_EnabledExtensionNames.size());
    UINT32 idx = 0;
    for (const auto& extension : m_EnabledExtensionNames)
    {
        extensionNamesCstr[idx++] = extension.data();
        Printf(pri, VkMods::GetTeeModuleCode(), "\t%s\n", extension.c_str());
    }

    const VkPhysicalDeviceRayTracingPipelineFeaturesKHR& rtFeatures =
        m_pPhysicalDevice->GetRayTracingPipelineFeatures();

    VkDeviceCreateInfo deviceCreateInfo = {};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pNext = &rtFeatures;
    deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size());
    deviceCreateInfo.pQueueCreateInfos = queueInfos.data();

    VkPhysicalDeviceFeatures features = m_pPhysicalDevice->GetFeatures();
    deviceCreateInfo.pEnabledFeatures = &features;

    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(extensionNamesCstr.size());
    deviceCreateInfo.ppEnabledExtensionNames = extensionNamesCstr.data();

    // Device Layers are deprecated
    deviceCreateInfo.enabledLayerCount = 0;
    deviceCreateInfo.ppEnabledLayerNames = nullptr;

    res = VulkanLayers::CreateDevice(m_pVulkanInst->GetVkInstance(),
        m_pPhysicalDevice->GetVkPhysicalDevice(), &deviceCreateInfo, nullptr,
        &m_VkDevice, this);

    if (res != VK_SUCCESS)
    {
        Printf(Tee::PriError,
               "Cannot create Vulkan device with 1 or more of the requested extensions.\n");
        m_VkDevice = VK_NULL_HANDLE;
        return res;
    }

    m_GraphicsQueues.resize(numGraphicsQueues, VK_NULL_HANDLE);
    for (UINT32 i = 0; i < numGraphicsQueues; i++)
    {
        VkLayerDispatchTable::GetDeviceQueue(m_VkDevice,
            m_pPhysicalDevice->GetGraphicsQueueFamilyIdx(),
            i, &m_GraphicsQueues[i]);
        if (m_GraphicsQueues[i] == VK_NULL_HANDLE)
        {
            Printf(Tee::PriError, "Error getting GraphicsQueue[%d] handle\n", i);
            return VK_ERROR_INITIALIZATION_FAILED;
        }
    }
    m_ComputeQueues.resize(numComputeQueues, VK_NULL_HANDLE);
    for (UINT32 i = 0; i < numComputeQueues; i++)
    {
        VkLayerDispatchTable::GetDeviceQueue(m_VkDevice,
            m_pPhysicalDevice->GetComputeQueueFamilyIdx(),
            i, &m_ComputeQueues[i]);
        if (m_ComputeQueues[i] == VK_NULL_HANDLE)
        {
            Printf(Tee::PriError, "Error getting ComputeQueue[%d] handle\n", i);
            return VK_ERROR_INITIALIZATION_FAILED;
        }
    }
    m_TransferQueues.resize(numTransferQueues, VK_NULL_HANDLE);
    for (UINT32 i = 0; i < numTransferQueues; i++)
    {
        VkLayerDispatchTable::GetDeviceQueue(m_VkDevice,
        m_pPhysicalDevice->GetTransferQueueFamilyIdx(),
        i, &m_TransferQueues[i]);
        if (m_TransferQueues[i] == VK_NULL_HANDLE)
        {
            Printf(Tee::PriError, "Error getting TransferQueue[%d] handle\n", i);
            return VK_ERROR_INITIALIZATION_FAILED;
        }
    }

    VkPipelineCacheCreateInfo pipelineCacheInfo = {};
    pipelineCacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    CHECK_VK_RESULT(CreatePipelineCache(&pipelineCacheInfo, nullptr, &m_PipelineCache));

    return res;
}

VkQueue VulkanDevice::GetGraphicsQueue(UINT32 queueIdx) const
{
    if (queueIdx < m_GraphicsQueues.size())
    {
        return m_GraphicsQueues[queueIdx];
    }
    else
    {
        MASSERT(!"Graphics queueIdx is out of range!");
        return VK_NULL_HANDLE;
    }
}
VkQueue VulkanDevice::GetComputeQueue(UINT32 queueIdx) const
{
    if (queueIdx < m_ComputeQueues.size())
    {
        return m_ComputeQueues[queueIdx];
    }
    else
    {
        MASSERT(!"Compute queueIdx is out of range!");
        return VK_NULL_HANDLE;
    }

}
VkQueue VulkanDevice::GetTransferQueue(UINT32 queueIdx) const
{
    if (queueIdx < m_TransferQueues.size())
    {
        return m_TransferQueues[queueIdx];
    }
    else
    {
        MASSERT(!"Transfer queueIdx is out of range!");
        return VK_NULL_HANDLE;
    }

}


//------------------------------------------------------------------------------------------------
// Get all of the available extensions for this physical device and suppress ones that cause us
// problems.
VkResult VulkanDevice::GetExtensions(bool bPrintExtensions)
{
    VkResult res = VK_SUCCESS;
    if (m_RegisteredExtensionNames.empty())
    {
        CHECK_VK_RESULT(m_pPhysicalDevice->GetExtensions(&m_RegisteredExtensionNames));

        UpdateFeatures();
    }

    if (bPrintExtensions)
    {
        Printf(Tee::PriNormal, "Registered Device Extensions:\n");
        for (const auto& ext : m_RegisteredExtensionNames)
        {
            Printf(Tee::PriNormal, "%s\n", ext.c_str());
        }
    }
    return res;
}

void VulkanDevice::AddDefaultExtensions()
{
    if (HasExt(ExtVK_EXT_pci_bus_info))
    {
        m_EnabledExtensionNames.insert("VK_EXT_pci_bus_info");
    }
}

//---------------------------------------------------------------------------------------------
// Enable a specific extension to be enabled.
// Note: Some extensions have dependancies on other extensions, in addition enabling an extension
//       may change the behavior of the test and/or cause side effects. It's the users
//       responsibility to understand if any side effects are generated and the impact of those
//       side affects.
void VulkanDevice::EnableExt(const string & extension)
{
    // We need to keep 2 separate lists because the test will also present a list of extensions
    // to enable. This list will be combined with the test's list.
    s_DisabledExtensionNames.erase(extension);
    s_EnabledExtensionNames.insert(extension);
}

//---------------------------------------------------------------------------------------------
//Prevent a specific extension from being enabled.
// Note: Some extensions are implicitly included by the test and disabling those extensions
//       may cause the test to fail and/or change the behavior. It's the users responsibility to
//       determine any side effects caused by disabling each extension.
void VulkanDevice::DisableExt(const string & extension)
{
    // We need to keep 2 separate lists because the test will also present a list of extensions
    // to enable. This list will remove extensions specified by the test as well, so be careful.
    s_EnabledExtensionNames.erase(extension);
    s_DisabledExtensionNames.insert(extension);
}

//------------------------------------------------------------------------------------------------
// Return true if the extension is found in the enabled extensions list, false otherwise.
// The GPU must be initiailzed before calling this API.
bool VulkanDevice::HasExt(VulkanDevice::VkExtensionId id)
{
    if (GetExtensions() != VK_SUCCESS)
    {
        return false;
    }

    if (id < VulkanDevice::ExtNUM_EXTENSIONS)
    {
        return m_HasExt[id];
    }
    return false;
}

bool VulkanDevice::HasExt(const char* extName)
{
    if (GetExtensions() != VK_SUCCESS)
    {
        return false;
    }

    const auto found = find(m_RegisteredExtensionNames.begin(),
                            m_RegisteredExtensionNames.end(),
                            extName);

    return found != m_RegisteredExtensionNames.end();
}

void VulkanDevice::UpdateFeatures()
{
    const char * ExtStrings[] =
    {
        "VK_ANDROID_native_buffer"
        ,"VK_ANDROID_external_memory_android_hardware_buffer"
        ,"VK_EXT_acquire_xlib_display"
        ,"VK_EXT_qnxscreen_surface"
        ,"VK_LWX_queue_priority_realtime"
        ,"VK_KHX_multiview"
        ,"VK_KHX_device_group_creation"
        ,"VK_KHX_device_group"
        ,"VK_LW_texture_dirty_tile_map"
        ,"VK_LW_corner_sampled_image"
        ,"VK_LW_internal"
        ,"VK_LW_shading_rate_image"
        ,"VK_KHR_android_surface"
        ,"VK_KHR_surface"
        ,"VK_KHR_swapchain"
        ,"VK_KHR_display"
        ,"VK_KHR_display_swapchain"
        ,"VK_KHR_sampler_mirror_clamp_to_edge"
        ,"VK_KHR_multiview"
        ,"VK_KHR_get_physical_device_properties2"
        ,"VK_KHR_device_group"
        ,"VK_KHR_shader_draw_parameters"
        ,"VK_KHR_maintenance1"
        ,"VK_KHR_device_group_creation"
        ,"VK_KHR_external_memory_capabilities"
        ,"VK_KHR_external_memory"
        ,"VK_KHR_external_memory_fd"
        ,"VK_KHR_external_semaphore_capabilities"
        ,"VK_KHR_external_semaphore"
        ,"VK_KHR_external_semaphore_fd"
        ,"VK_KHR_push_descriptor"
        ,"VK_KHR_16bit_storage"
        ,"VK_KHR_incremental_present"
        ,"VK_KHR_descriptor_update_template"
        ,"VK_KHR_shared_presentable_image"
        ,"VK_KHR_external_fence_capabilities"
        ,"VK_KHR_external_fence"
        ,"VK_KHR_external_fence_fd"
        ,"VK_KHR_maintenance2"
        ,"VK_KHR_get_surface_capabilities2"
        ,"VK_KHR_variable_pointers"
        ,"VK_KHR_get_display_properties2"
        ,"VK_KHR_dedicated_allocation"
        ,"VK_KHR_storage_buffer_storage_class"
        ,"VK_KHR_relaxed_block_layout"
        ,"VK_KHR_get_memory_requirements2"
        ,"VK_KHR_image_format_list"
        ,"VK_KHR_sampler_ycbcr_colwersion"
        ,"VK_KHR_bind_memory2"
        ,"VK_KHR_maintenance3"
        ,"VK_EXT_debug_report"
        ,"VK_LW_glsl_shader"
        ,"VK_EXT_depth_range_unrestricted"
        ,"VK_IMG_filter_lwbic"
        ,"VK_AMD_rasterization_order"
        ,"VK_AMD_shader_trinary_minmax"
        ,"VK_AMD_shader_explicit_vertex_parameter"
        ,"VK_EXT_debug_marker"
        ,"VK_AMD_gcn_shader"
        ,"VK_LW_dedicated_allocation"
        ,"VK_AMD_draw_indirect_count"
        ,"VK_AMD_negative_viewport_height"
        ,"VK_AMD_gpu_shader_half_float"
        ,"VK_AMD_shader_ballot"
        ,"VK_AMD_texture_gather_bias_lod"
        ,"VK_AMD_shader_info"
        ,"VK_AMD_shader_image_load_store_lod"
        ,"VK_IMG_format_pvrtc"
        ,"VK_LW_external_memory_capabilities"
        ,"VK_LW_external_memory"
        ,"VK_EXT_validation_flags"
        ,"VK_EXT_shader_subgroup_ballot"
        ,"VK_EXT_shader_subgroup_vote"
        ,"VK_LWX_device_generated_commands"
        ,"VK_LW_clip_space_w_scaling"
        ,"VK_EXT_direct_mode_display"
        ,"VK_EXT_display_surface_counter"
        ,"VK_EXT_display_control"
        ,"VK_GOOGLE_display_timing"
        ,"VK_LW_sample_mask_override_coverage"
        ,"VK_LW_geometry_shader_passthrough"
        ,"VK_LW_viewport_array2"
        ,"VK_LWX_multiview_per_view_attributes"
        ,"VK_LW_viewport_swizzle"
        ,"VK_EXT_discard_rectangles"
        ,"VK_EXT_swapchain_colorspace"
        ,"VK_EXT_hdr_metadata"
        ,"VK_EXT_sampler_filter_minmax"
        ,"VK_AMD_gpu_shader_int16"
        ,"VK_AMD_mixed_attachment_samples"
        ,"VK_AMD_shader_fragment_mask"
        ,"VK_EXT_shader_stencil_export"
        ,"VK_EXT_sample_locations"
        ,"VK_EXT_blend_operation_advanced"
        ,"VK_LW_fragment_coverage_to_color"
        ,"VK_LW_framebuffer_mixed_samples"
        ,"VK_LW_fill_rectangle"
        ,"VK_EXT_post_depth_coverage"
        ,"VK_EXT_validation_cache"
        ,"VK_EXT_shader_viewport_index_layer"
        ,"VK_EXT_global_priority"
        ,"VK_MVK_ios_surface"
        ,"VK_MVK_macos_surface"
        ,"VK_KHR_mir_surface"
        ,"VK_NN_vi_surface"
        ,"VK_KHR_wayland_surface"
        ,"VK_KHR_win32_surface"
        ,"VK_KHR_external_memory_win32"
        ,"VK_KHR_win32_keyed_mutex"
        ,"VK_KHR_external_semaphore_win32"
        ,"VK_KHR_external_fence_win32"
        ,"VK_LW_external_memory_win32"
        ,"VK_LW_win32_keyed_mutex"
        ,"VK_KHR_xcb_surface"
        ,"VK_KHR_xlib_surface"
        ,"VK_EXT_conditional_rendering"
        ,"VK_EXT_conservative_rasterization"
        ,"VK_EXT_depth_clip_enable"
        ,"VK_EXT_host_query_reset"
        ,"VK_EXT_memory_budget"
        ,"VK_EXT_pci_bus_info"
        ,"VK_EXT_vertex_attribute_divisor"
        ,"VK_EXT_ycbcr_image_arrays"
        ,"VK_KHR_create_renderpass2"
        ,"VK_KHR_depth_stencil_resolve"
        ,"VK_KHR_draw_indirect_count"
        ,"VK_KHR_driver_properties"
        ,"VK_KHR_imageless_framebuffer"
        ,"VK_KHR_shader_atomic_int64"
        ,"VK_LW_compute_shader_derivatives"
        ,"VK_LW_coverage_reduction_mode"
        ,"VK_LW_dedicated_allocation_image_aliasing"
        ,"VK_LW_device_diagnostic_checkpoints"
        ,"VK_LW_fragment_shader_barycentric"
        ,"VK_LW_mesh_shader"
        ,"VK_LW_representative_fragment_test"
        ,"VK_LW_scissor_exclusive"
        ,"VK_LW_shader_image_footprint"
        ,"VK_LW_shader_subgroup_partitioned"
        ,"VK_KHX_external_memory"
        ,"VK_KHX_external_memory_win32"
        ,"VK_KHX_external_semaphore"
        ,"VK_KHX_external_semaphore_win32"
        ,"VK_KHX_win32_keyed_mutex"
        ,"VK_LWX_display_timing"
        ,"VK_KHR_shader_clock"
        ,"VK_LW_shader_sm_builtins"
        ,"VK_LW_gpu_shader5"
        ,"VK_LW_shader_thread_group"
        ,"VK_KHR_ray_tracing"
        ,"VK_KHR_ray_query"
        ,"VK_KHR_acceleration_structure"
        ,"VK_KHR_shader_float16_int8"
        ,"GL_LW_shader_sm_builtins"
        ,"VK_LW_cooperative_matrix"
        ,"VK_EXT_calibrated_timestamps"
        ,"VK_EXT_extended_dynamic_state"
    };
    for (UINT32 i = 0; i < VulkanDevice::ExtNUM_EXTENSIONS; i++)
    {
        m_HasExt[i] = find(m_RegisteredExtensionNames.begin(),
                           m_RegisteredExtensionNames.end(),
                           ExtStrings[i]) != m_RegisteredExtensionNames.end();
    }
}

VulkanPhysicalDevice* VulkanDevice::GetPhysicalDevice() const
{
    return m_pPhysicalDevice.get();
}

vector<VkDeviceQueueCreateInfo> VulkanDevice::CreateQueueInfos
(
    UINT32 numGraphicsQueues,
    UINT32 numTransferQueues,
    UINT32 numComputeQueues
)
{
    // We need to be able to handle these two cases and anything in between:
    // 1. All queues are in different families (common case for our GPUs)
    // 2. All queues are in the same family
    map<VulkanQueueFamilyIndex, vector<VulkanQueueIndex>> numQueuesPerFamily;
    if (numGraphicsQueues)
    {
        numQueuesPerFamily[m_pPhysicalDevice->GetGraphicsQueueFamilyIdx()] =
            vector<VulkanQueueIndex>(numGraphicsQueues);
    }
    if (numComputeQueues)
    {
        numQueuesPerFamily[m_pPhysicalDevice->GetComputeQueueFamilyIdx()] =
            vector<VulkanQueueIndex>(numComputeQueues);
    }
    if (numTransferQueues)
    {
        numQueuesPerFamily[m_pPhysicalDevice->GetTransferQueueFamilyIdx()] =
            vector<VulkanQueueIndex>(numTransferQueues);
    }

    vector<VkDeviceQueueCreateInfo> queueInfos;
    VkDeviceQueueCreateInfo queueCreateInfo = {};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;

    for (const auto& entry : numQueuesPerFamily)
    {
        queueCreateInfo.queueFamilyIndex = entry.first;
        queueCreateInfo.queueCount = static_cast<uint32_t>(entry.second.size());
        m_QueuePriorities[entry.first].resize(entry.second.size()); // ignore priority
        queueCreateInfo.pQueuePriorities = m_QueuePriorities[entry.first].data();
        queueInfos.push_back(queueCreateInfo);
    }

    return queueInfos;
}

// ************************** Vulkan API Wrapper *******************************

#define VKDEV_CALL_NO_ARGS(NAME, RETTYPE) \
    RETTYPE VulkanDevice::NAME() const { return VkLayerDispatchTable::NAME(m_VkDevice); }

#define VKDEV_CALL_ONE_ARG(NAME, RETTYPE, ARG0_T, ARG0) \
    RETTYPE VulkanDevice::NAME(ARG0_T ARG0) const \
    { return VkLayerDispatchTable::NAME(m_VkDevice, ARG0); }

#define VKDEV_CALL_TWO_ARGS(NAME, RETTYPE, ARG0_T, ARG0, ARG1_T, ARG1) \
    RETTYPE VulkanDevice::NAME(ARG0_T ARG0, ARG1_T ARG1) const \
    { return VkLayerDispatchTable::NAME(m_VkDevice, ARG0, ARG1); }

#define VKDEV_CALL_THREE_ARGS(NAME, RETTYPE, ARG0_T, ARG0, ARG1_T, ARG1, ARG2_T, ARG2) \
    RETTYPE VulkanDevice::NAME(ARG0_T ARG0, ARG1_T ARG1, ARG2_T ARG2) const \
    { return VkLayerDispatchTable::NAME(m_VkDevice, ARG0, ARG1, ARG2); }

#define VKDEV_CALL_FOUR_ARGS(NAME, RETTYPE, ARG0_T, ARG0, ARG1_T, ARG1, ARG2_T, ARG2, \
                             ARG3_T, ARG3) \
    RETTYPE VulkanDevice::NAME(ARG0_T ARG0, ARG1_T ARG1, ARG2_T ARG2, ARG3_T ARG3) const \
    { return VkLayerDispatchTable::NAME(m_VkDevice, ARG0, ARG1, ARG2, ARG3); }

#define VKDEV_CALL_FIVE_ARGS(NAME, RETTYPE, ARG0_T, ARG0, ARG1_T, ARG1, ARG2_T, ARG2, \
                             ARG3_T, ARG3, ARG4_T, ARG4) \
    RETTYPE VulkanDevice::NAME(ARG0_T ARG0, ARG1_T ARG1, ARG2_T ARG2, ARG3_T ARG3, \
                               ARG4_T ARG4) const \
    { return VkLayerDispatchTable::NAME(m_VkDevice, ARG0, ARG1, ARG2, ARG3, ARG4); }

#define VKDEV_CALL_SIX_ARGS(NAME, RETTYPE, ARG0_T, ARG0, ARG1_T, ARG1, ARG2_T, ARG2, \
                             ARG3_T, ARG3, ARG4_T, ARG4, ARG5_T, ARG5) \
    RETTYPE VulkanDevice::NAME(ARG0_T ARG0, ARG1_T ARG1, ARG2_T ARG2, ARG3_T ARG3, \
                               ARG4_T ARG4, ARG5_T ARG5) const \
    { return VkLayerDispatchTable::NAME(m_VkDevice, ARG0, ARG1, ARG2, ARG3, ARG4, ARG5); }

#define VKDEV_CALL_SEVEN_ARGS(NAME, RETTYPE, ARG0_T, ARG0, ARG1_T, ARG1, ARG2_T, ARG2, \
                             ARG3_T, ARG3, ARG4_T, ARG4, ARG5_T, ARG5, ARG6_T, ARG6) \
    RETTYPE VulkanDevice::NAME(ARG0_T ARG0, ARG1_T ARG1, ARG2_T ARG2, ARG3_T ARG3, \
                               ARG4_T ARG4, ARG5_T ARG5, ARG6_T ARG6) const \
    { return VkLayerDispatchTable::NAME(m_VkDevice, ARG0, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6); }

VKDEV_CALL_THREE_ARGS(GetDeviceQueue, void,
    uint32_t, queueFamilyIndex,
    uint32_t, queueIndex,
    VkQueue*, pQueue)

VKDEV_CALL_NO_ARGS(DeviceWaitIdle, VkResult)

VKDEV_CALL_THREE_ARGS(AllocateMemory, VkResult,
    const VkMemoryAllocateInfo*, pAllocateInfo,
    const VkAllocationCallbacks*, pAllocator,
    VkDeviceMemory*, pMemory)

VKDEV_CALL_TWO_ARGS(FreeMemory, void,
    VkDeviceMemory, memory,
    const VkAllocationCallbacks*, pAllocator)

VKDEV_CALL_FIVE_ARGS(MapMemory, VkResult,
    VkDeviceMemory, memory,
    VkDeviceSize, offset,
    VkDeviceSize, size,
    VkMemoryMapFlags, flags,
    void**, ppData)

VKDEV_CALL_ONE_ARG(UnmapMemory, void, VkDeviceMemory, memory)

VKDEV_CALL_TWO_ARGS(FlushMappedMemoryRanges, VkResult,
    uint32_t, memoryRangeCount,
    const VkMappedMemoryRange*, pMemoryRanges)

VKDEV_CALL_TWO_ARGS(IlwalidateMappedMemoryRanges, VkResult,
    uint32_t, memoryRangeCount,
    const VkMappedMemoryRange*, pMemoryRanges)

VKDEV_CALL_TWO_ARGS(GetDeviceMemoryCommitment, void,
    VkDeviceMemory, memory,
    VkDeviceSize*, pCommittedMemoryInBytes)

VKDEV_CALL_THREE_ARGS(BindBufferMemory, VkResult,
    VkBuffer, buffer,
    VkDeviceMemory, memory,
    VkDeviceSize, memoryOffset)

VKDEV_CALL_THREE_ARGS(BindImageMemory, VkResult,
    VkImage, image,
    VkDeviceMemory, memory,
    VkDeviceSize, memoryOffset)

VKDEV_CALL_TWO_ARGS(GetBufferMemoryRequirements, void,
    VkBuffer, buffer,
    VkMemoryRequirements*, pMemoryRequirements)

VKDEV_CALL_ONE_ARG(GetBufferDeviceAddress, VkDeviceAddress,
    const VkBufferDeviceAddressInfo*, pInfo);

VKDEV_CALL_TWO_ARGS(GetImageMemoryRequirements, void,
    VkImage, buffer,
    VkMemoryRequirements*, pMemoryRequirements)

VKDEV_CALL_THREE_ARGS(GetImageSparseMemoryRequirements, void,
    VkImage, image,
    uint32_t*, pSparseMemoryRequirementCount,
    VkSparseImageMemoryRequirements*, pSparseMemoryRequirements)

VKDEV_CALL_THREE_ARGS(CreateFence, VkResult,
    const VkFenceCreateInfo*, pCreateInfo,
    const VkAllocationCallbacks*, pAllocator,
    VkFence*, pFence)

VKDEV_CALL_TWO_ARGS(DestroyFence, void,
    VkFence, fence,
    const VkAllocationCallbacks*, pAllocator);

VKDEV_CALL_TWO_ARGS(ResetFences, VkResult,
    uint32_t, fenceCount,
    const VkFence*, pFences)

VKDEV_CALL_ONE_ARG(GetFenceStatus, VkResult, VkFence, fence)

VKDEV_CALL_FOUR_ARGS(WaitForFences, VkResult,
    uint32_t, fenceCount,
    const VkFence*, pFences,
    VkBool32, waitAll,
    uint64_t, timeout);

VKDEV_CALL_THREE_ARGS(CreateSemaphore, VkResult,
    const VkSemaphoreCreateInfo*, pCreateInfo,
    const VkAllocationCallbacks*, pAllocator,
    VkSemaphore*, pSemaphore);

VKDEV_CALL_TWO_ARGS(DestroySemaphore, void,
    VkSemaphore, semaphore,
    const VkAllocationCallbacks*, pAllocator);

VKDEV_CALL_THREE_ARGS(CreateEvent, VkResult,
    const VkEventCreateInfo*, pCreateInfo,
    const VkAllocationCallbacks*, pAllocator,
    VkEvent*, pEvent);

VKDEV_CALL_TWO_ARGS(DestroyEvent, void,
    VkEvent, event,
    const VkAllocationCallbacks*, pAllocator);

VKDEV_CALL_ONE_ARG(GetEventStatus, VkResult, VkEvent, event);

VKDEV_CALL_ONE_ARG(SetEvent, VkResult, VkEvent, event);

VKDEV_CALL_ONE_ARG(ResetEvent, VkResult, VkEvent, event);

VKDEV_CALL_THREE_ARGS(CreateQueryPool, VkResult,
    const VkQueryPoolCreateInfo*, pCreateInfo,
    const VkAllocationCallbacks*, pAllocator,
    VkQueryPool*, pQueryPool);

VKDEV_CALL_TWO_ARGS(DestroyQueryPool, void,
    VkQueryPool, queryPool,
    const VkAllocationCallbacks*, pAllocator);

VKDEV_CALL_SEVEN_ARGS(GetQueryPoolResults, VkResult,
    VkQueryPool, queryPool,
    uint32_t, firstQuery,
    uint32_t, queryCount,
    size_t, dataSize,
    void*, pData,
    VkDeviceSize, stride,
    VkQueryResultFlags, flags);

VKDEV_CALL_THREE_ARGS(CreateBuffer, VkResult,
    const VkBufferCreateInfo*, pCreateInfo,
    const VkAllocationCallbacks*, pAllocator,
    VkBuffer*, pBuffer);

VKDEV_CALL_TWO_ARGS(DestroyBuffer, void,
    VkBuffer, buffer,
    const VkAllocationCallbacks*, pAllocator);

VKDEV_CALL_THREE_ARGS(CreateBufferView, VkResult,
    const VkBufferViewCreateInfo*, pCreateInfo,
    const VkAllocationCallbacks*, pAllocator,
    VkBufferView*, pView);

VKDEV_CALL_TWO_ARGS(DestroyBufferView, void,
    VkBufferView, bufferView,
    const VkAllocationCallbacks*, pAllocator);

VKDEV_CALL_THREE_ARGS(CreateImage, VkResult,
    const VkImageCreateInfo*, pCreateInfo,
    const VkAllocationCallbacks*, pAllocator,
    VkImage*, pImage);

VKDEV_CALL_TWO_ARGS(DestroyImage, void,
    VkImage, image,
    const VkAllocationCallbacks*, pAlloc);

VKDEV_CALL_THREE_ARGS(GetImageSubresourceLayout, void,
    VkImage, image,
    const VkImageSubresource*, pSubresource,
    VkSubresourceLayout*, pLayout);

VKDEV_CALL_THREE_ARGS(CreateImageView, VkResult,
    const VkImageViewCreateInfo*, pCreateInfo,
    const VkAllocationCallbacks*, pAllocator,
    VkImageView*, pView);

VKDEV_CALL_TWO_ARGS(DestroyImageView, void,
    VkImageView, imageView,
    const VkAllocationCallbacks*, pAllocator);

VKDEV_CALL_THREE_ARGS(CreateShaderModule, VkResult,
    const VkShaderModuleCreateInfo*, pCreateInfo,
    const VkAllocationCallbacks*, pAllocator,
    VkShaderModule*, pShaderModule);

VKDEV_CALL_TWO_ARGS(DestroyShaderModule, void,
    VkShaderModule, shaderModule,
    const VkAllocationCallbacks*, pAllocator);

VKDEV_CALL_THREE_ARGS(CreatePipelineCache, VkResult,
    const VkPipelineCacheCreateInfo*, pCreateInfo,
    const VkAllocationCallbacks*, pAllocator,
    VkPipelineCache*, pPipelineCache);

VKDEV_CALL_TWO_ARGS(DestroyPipelineCache, void,
    VkPipelineCache, pipelineCache,
    const VkAllocationCallbacks*, pAllocator);

VKDEV_CALL_THREE_ARGS(GetPipelineCacheData, VkResult,
    VkPipelineCache, pipelineCache,
    size_t*, pDataSize,
    void*, pData);

VKDEV_CALL_THREE_ARGS(MergePipelineCaches, VkResult,
    VkPipelineCache, dstCache,
    uint32_t, srcCacheCount,
    const VkPipelineCache*, pSrcCaches);

VKDEV_CALL_FIVE_ARGS(CreateGraphicsPipelines, VkResult,
    VkPipelineCache, pipelineCache,
    uint32_t, createInfoCount,
    const VkGraphicsPipelineCreateInfo*, pCreateInfos,
    const VkAllocationCallbacks*, pAllocator,
    VkPipeline*, pPipelines);

VKDEV_CALL_FIVE_ARGS(CreateComputePipelines, VkResult,
    VkPipelineCache, pipelineCache,
    uint32_t, createInfoCount,
    const VkComputePipelineCreateInfo*, pCreateInfos,
    const VkAllocationCallbacks*, pAllocator,
    VkPipeline*, pPipelines);

VKDEV_CALL_TWO_ARGS(DestroyPipeline, void,
    VkPipeline, pipeline,
    const VkAllocationCallbacks*, pAllocator);

VKDEV_CALL_THREE_ARGS(CreatePipelineLayout, VkResult,
    const VkPipelineLayoutCreateInfo*, pCreateInfo,
    const VkAllocationCallbacks*, pAllocator,
    VkPipelineLayout*, pPipelineLayout);

VKDEV_CALL_TWO_ARGS(DestroyPipelineLayout, void,
    VkPipelineLayout, pipelineLayout,
    const VkAllocationCallbacks*, pAllocator);

VKDEV_CALL_THREE_ARGS(CreateSampler, VkResult,
    const VkSamplerCreateInfo*, pCreateInfo,
    const VkAllocationCallbacks*, pAllocator,
    VkSampler*, pSampler);

VKDEV_CALL_TWO_ARGS(DestroySampler, void,
    VkSampler, sampler,
    const VkAllocationCallbacks*, pAllocator);

VKDEV_CALL_THREE_ARGS(CreateDescriptorSetLayout, VkResult,
    const VkDescriptorSetLayoutCreateInfo*, pCreateInfo,
    const VkAllocationCallbacks*, pAllocator,
    VkDescriptorSetLayout*, pSetLayout);

VKDEV_CALL_TWO_ARGS(DestroyDescriptorSetLayout, void,
    VkDescriptorSetLayout, descriptorSetLayout,
    const VkAllocationCallbacks*, pAllocator);

VKDEV_CALL_THREE_ARGS(CreateDescriptorPool, VkResult,
    const VkDescriptorPoolCreateInfo*, pCreateInfo,
    const VkAllocationCallbacks*, pAllocator,
    VkDescriptorPool*, pDescriptorPool);

VKDEV_CALL_TWO_ARGS(DestroyDescriptorPool, void,
    VkDescriptorPool, descriptorPool,
    const VkAllocationCallbacks*, pAllocator);

VKDEV_CALL_TWO_ARGS(ResetDescriptorPool, VkResult,
    VkDescriptorPool, descriptorPool,
    VkDescriptorPoolResetFlags, flags);

VKDEV_CALL_TWO_ARGS(AllocateDescriptorSets, VkResult,
    const VkDescriptorSetAllocateInfo*, pAllocateInfo,
    VkDescriptorSet*, pDescriptorSets);

VKDEV_CALL_THREE_ARGS(FreeDescriptorSets, VkResult,
    VkDescriptorPool, descriptorPool,
    uint32_t, descriptorSetCount,
    const VkDescriptorSet*, pDescriptorSets);

VKDEV_CALL_FOUR_ARGS(UpdateDescriptorSets, void,
    uint32_t, descriptorWriteCount,
    const VkWriteDescriptorSet*, pDescriptorWrites,
    uint32_t, descriptorCopyCount,
    const VkCopyDescriptorSet*, pDescriptorCopies);

VKDEV_CALL_THREE_ARGS(CreateFramebuffer, VkResult,
    const VkFramebufferCreateInfo*, pCreateInfo,
    const VkAllocationCallbacks*, pAllocator,
    VkFramebuffer*, pFramebuffer);

VKDEV_CALL_TWO_ARGS(DestroyFramebuffer, void,
    VkFramebuffer, framebuffer,
    const VkAllocationCallbacks*, pAllocator);

VKDEV_CALL_THREE_ARGS(CreateRenderPass, VkResult,
    const VkRenderPassCreateInfo*, pCreateInfo,
    const VkAllocationCallbacks*, pAllocator,
    VkRenderPass*, pRenderPass);

VKDEV_CALL_TWO_ARGS(DestroyRenderPass, void,
    VkRenderPass, renderPass,
    const VkAllocationCallbacks*, pAllocator);

VKDEV_CALL_TWO_ARGS(GetRenderAreaGranularity, void,
    VkRenderPass, renderPass,
    VkExtent2D*, pGranularity);

VKDEV_CALL_THREE_ARGS(CreateCommandPool, VkResult,
    const VkCommandPoolCreateInfo*, pCreateInfo,
    const VkAllocationCallbacks*, pAllocator,
    VkCommandPool*, pCommandPool);

VKDEV_CALL_TWO_ARGS(DestroyCommandPool, void,
    VkCommandPool, commandPool,
    const VkAllocationCallbacks*, pAllocator);

VKDEV_CALL_TWO_ARGS(ResetCommandPool, VkResult,
    VkCommandPool, commandPool,
    VkCommandPoolResetFlags, flags);

VKDEV_CALL_TWO_ARGS(AllocateCommandBuffers, VkResult,
    const VkCommandBufferAllocateInfo*, pAllocateInfo,
    VkCommandBuffer*, pCommandBuffers);

VKDEV_CALL_THREE_ARGS(FreeCommandBuffers, void,
    VkCommandPool, commandPool,
    uint32_t, commandBufferCount,
    const VkCommandBuffer*, pCommandBuffers);

VKDEV_CALL_THREE_ARGS(CreateSwapchainKHR, VkResult,
    const VkSwapchainCreateInfoKHR*, pCreateInfo,
    const VkAllocationCallbacks*, pAllocator,
    VkSwapchainKHR*, pSwapchain);

VKDEV_CALL_TWO_ARGS(DestroySwapchainKHR, void,
    VkSwapchainKHR, swapchain,
    const VkAllocationCallbacks*, pAllocator);

VKDEV_CALL_THREE_ARGS(GetSwapchainImagesKHR, VkResult,
    VkSwapchainKHR, swapchain,
    uint32_t*, pSwapchainImageCount,
    VkImage*, pSwapchainImages);

VKDEV_CALL_FIVE_ARGS(AcquireNextImageKHR, VkResult,
    VkSwapchainKHR, swapchain,
    uint64_t, timeout,
    VkSemaphore, semaphore,
    VkFence, fence,
    uint32_t*, pImageIndex);

VKDEV_CALL_THREE_ARGS(CreateAccelerationStructureKHR, VkResult,
    const VkAccelerationStructureCreateInfoKHR*, pCreateInfo,
    const VkAllocationCallbacks*, pAllocator,
    VkAccelerationStructureKHR*, pAccelerationStructure);

VKDEV_CALL_TWO_ARGS(DestroyAccelerationStructureKHR, void,
    VkAccelerationStructureKHR, accelerationStructure,
    const VkAllocationCallbacks*, pAllocator);

VKDEV_CALL_FOUR_ARGS(GetAccelerationStructureBuildSizesKHR, void,
    VkAccelerationStructureBuildTypeKHR, buildType,
    const VkAccelerationStructureBuildGeometryInfoKHR*, pBuildInfo,
    const uint32_t*, pMaxPrimitiveCounts,
    VkAccelerationStructureBuildSizesInfoKHR*, pSizeInfo);

VKDEV_CALL_ONE_ARG(GetAccelerationStructureDeviceAddressKHR, VkDeviceAddress,
    const VkAccelerationStructureDeviceAddressInfoKHR*, pInfo);

VKDEV_CALL_SIX_ARGS(CreateRayTracingPipelinesKHR, VkResult,
    VkDeferredOperationKHR, deferredOperation,
    VkPipelineCache, pipelineCache,
    uint32_t, createInfoCount,
    const VkRayTracingPipelineCreateInfoKHR*, pCreateInfos,
    const VkAllocationCallbacks*, pAllocator,
    VkPipeline*, pPipelines);

VKDEV_CALL_FIVE_ARGS(GetRayTracingShaderGroupHandlesKHR, VkResult,
    VkPipeline, pipeline,
    uint32_t, firstGroup,
    uint32_t, groupCount,
    size_t, dataSize,
    void*, pData);
// ************************** END Vulkan API Wrapper ***************************

