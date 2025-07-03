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
#ifndef VK_PHYSDEV_H
#define VK_PHYSDEV_H

#include "vkmain.h"
#include <set>

class VulkanInstance;

//! \brief VkPhysicalDevice POD wrapper
class VulkanPhysicalDevice
{
public:
    VulkanPhysicalDevice
    (
        VkPhysicalDevice physicalDevice,
        VulkanInstance* pVkInst,
        bool printExtensions = false
    );

    bool operator==(const VulkanPhysicalDevice& vulkanPhysDevice) const;
    bool operator!=(const VulkanPhysicalDevice& vulkanPhysDevice) const;

    //! \brief Return a list of all supported Vulkan extensions
    VkResult GetExtensions(std::set<string>* pExtensionNames) const;

    //! \brief Print all VkPhysicalDeviceFeatures supported by this device
    void PrintFeatures(Tee::Priority pri);

    //! \brief Return an index into MemProps for an entry that matches the
    //!        requirements and properties.
    UINT32 GetMemoryTypeIndex
    (
        VkMemoryRequirements memRequirements,
        VkMemoryPropertyFlags memPropFlag
    ) const;

    GET_PROP(VkPhysicalDevice, VkPhysicalDevice);
    GET_PROP(Limits, const VkPhysicalDeviceLimits&);
    GET_PROP(MemProps, const VkPhysicalDeviceMemoryProperties&);
    GET_PROP(SMBuiltinsPropertiesLW, const VkPhysicalDeviceShaderSMBuiltinsPropertiesLW&);
    GET_PROP_LWSTOM(Features, const VkPhysicalDeviceFeatures&);
    GET_PROP(Features2, const VkPhysicalDeviceFeatures2&);
    GET_PROP(ExtendedDynamicStateFeatures, const VkPhysicalDeviceExtendedDynamicStateFeaturesEXT&);
    GET_PROP(MeshFeatures, const VkPhysicalDeviceMeshShaderFeaturesLW&);
    GET_PROP(CooperativeMatrixFeatures, const VkPhysicalDeviceCooperativeMatrixFeaturesLW&);
    GET_PROP(RayTracingPipelineFeatures, const VkPhysicalDeviceRayTracingPipelineFeaturesKHR&);
    GET_PROP(RayQueryFeatures, const VkPhysicalDeviceRayQueryFeaturesKHR&);
    GET_PROP(GraphicsQueueFamilyIdx, VulkanQueueFamilyIndex);
    GET_PROP(ComputeQueueFamilyIdx, VulkanQueueFamilyIndex);
    GET_PROP(TransferQueueFamilyIdx, VulkanQueueFamilyIndex);
    GET_PROP(MaxGraphicsQueues, UINT32);
    GET_PROP(MaxComputeQueues, UINT32);
    GET_PROP(MaxTransferQueues, UINT32);
    GET_PROP(VendorID, UINT32);
    GET_PROP(DeviceID, UINT32);
    VkSampleCountFlagBits GetMaxSampleCount() const;
    bool IsSampleCountSupported(UINT32 count) const;

private:
    VulkanInstance* m_pVulkanInst = nullptr;

    VkPhysicalDevice                 m_VkPhysicalDevice = VK_NULL_HANDLE;
    uint32_t                         m_VendorID = 0;
    uint32_t                         m_DeviceID = 0;
    VkPhysicalDeviceLimits           m_Limits = {};
    VkPhysicalDeviceMemoryProperties m_MemProps = {};
    VkPhysicalDeviceShaderSMBuiltinsPropertiesLW m_SMBuiltinsPropertiesLW =
        { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SM_BUILTINS_PROPERTIES_LW, nullptr, 0, 0};
    VkPhysicalDeviceVulkan12Features m_Features12 =
        { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES, nullptr };
    VkPhysicalDeviceExtendedDynamicStateFeaturesEXT m_ExtendedDynamicStateFeatures =
        { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT, &m_Features12, false };
    VkPhysicalDeviceMeshShaderFeaturesLW m_MeshFeatures =
        { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_LW, &m_ExtendedDynamicStateFeatures, false, false };
    VkPhysicalDeviceCooperativeMatrixFeaturesLW m_CooperativeMatrixFeatures =
        { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COOPERATIVE_MATRIX_FEATURES_LW, &m_MeshFeatures, false, false };
    VkPhysicalDeviceAccelerationStructureFeaturesKHR m_AccelerationStructureFeatures =
        { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR, &m_CooperativeMatrixFeatures };
    VkPhysicalDeviceRayQueryFeaturesKHR m_RayQueryFeatures =
        { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR, &m_AccelerationStructureFeatures };
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR m_RayTracingPipelineFeatures =
        { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR, &m_RayQueryFeatures };
    VkPhysicalDeviceFeatures2        m_Features2 =
        { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &m_RayTracingPipelineFeatures };
    VulkanQueueFamilyIndex           m_GraphicsQueueFamilyIdx = VK_QUEUE_FLAG_BITS_MAX_ENUM;
    VulkanQueueFamilyIndex           m_ComputeQueueFamilyIdx = VK_QUEUE_FLAG_BITS_MAX_ENUM;
    VulkanQueueFamilyIndex           m_TransferQueueFamilyIdx = VK_QUEUE_FLAG_BITS_MAX_ENUM;
    UINT32                           m_MaxGraphicsQueues = 0;
    UINT32                           m_MaxComputeQueues = 0;
    UINT32                           m_MaxTransferQueues = 0;

    void GetBestQueueFamilyIndices();
};

#endif

