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

#include "vkphysdev.h"
#include "vkinstance.h"
#include <algorithm>

VulkanPhysicalDevice::VulkanPhysicalDevice
(
    VkPhysicalDevice physicalDevice
   ,VulkanInstance* pVkInst
   ,bool printExtensions
) :
    m_pVulkanInst(pVkInst)
   ,m_VkPhysicalDevice(physicalDevice)
{
    MASSERT(physicalDevice);

    VkPhysicalDeviceProperties2 properties2 =
        { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &m_SMBuiltinsPropertiesLW };
    if (m_pVulkanInst->GetPhysicalDeviceProperties2)
    {
        m_pVulkanInst->GetPhysicalDeviceProperties2(m_VkPhysicalDevice, &properties2);
    }
    else
    {
        m_pVulkanInst->GetPhysicalDeviceProperties(m_VkPhysicalDevice, &properties2.properties);
    }
    m_VendorID = properties2.properties.vendorID;
    m_DeviceID = properties2.properties.deviceID;
    m_Limits = properties2.properties.limits;
    m_pVulkanInst->GetPhysicalDeviceMemoryProperties(m_VkPhysicalDevice, &m_MemProps);
    if (m_pVulkanInst->GetPhysicalDeviceFeatures2)
    {
        m_pVulkanInst->GetPhysicalDeviceFeatures2(m_VkPhysicalDevice, &m_Features2);
    }

    Tee::Priority pri = (printExtensions) ? Tee::PriNormal : Tee::PriLow;

    PrintFeatures(pri);

    GetBestQueueFamilyIndices();
}

bool VulkanPhysicalDevice::operator==(const VulkanPhysicalDevice& vulkanPhysDevice) const
{
    return m_VkPhysicalDevice == vulkanPhysDevice.m_VkPhysicalDevice;
}

bool VulkanPhysicalDevice::operator!=(const VulkanPhysicalDevice& vulkanPhysDevice) const
{
    return !(*this == vulkanPhysDevice);
}
const VkPhysicalDeviceFeatures& VulkanPhysicalDevice::GetFeatures() const
{
    return m_Features2.features;
}

VkResult VulkanPhysicalDevice::GetExtensions(set<string>* pExtensionNames) const
{
    MASSERT(pExtensionNames);
    pExtensionNames->clear();

    VkResult res = VK_SUCCESS;

    UINT32 deviceExtCount;
    CHECK_VK_RESULT(m_pVulkanInst->EnumerateDeviceExtensionProperties(
        m_VkPhysicalDevice, nullptr, &deviceExtCount, nullptr));

    if (deviceExtCount)
    {
        vector<VkExtensionProperties> deviceExtensions(deviceExtCount);
        CHECK_VK_RESULT(m_pVulkanInst->EnumerateDeviceExtensionProperties(
            m_VkPhysicalDevice, nullptr, &deviceExtCount, deviceExtensions.data()));

        for (const auto& extension : deviceExtensions)
        {
            pExtensionNames->insert(extension.extensionName);
        }
    }

    return res;
}

void VulkanPhysicalDevice::PrintFeatures(Tee::Priority pri)
{
    #define GET_NAME_AND_VALUE(struct, member) #member, tf[struct.member]
    const char *tf[2] = { "disabled", "enabled" };
    typedef struct deviceFeatures
    {
        char *name;
        const char* value;
    } deviceFeatures;

    deviceFeatures features[] =
    {
         { GET_NAME_AND_VALUE(m_Features2.features, robustBufferAccess) }
        ,{ GET_NAME_AND_VALUE(m_Features2.features, fullDrawIndexUint32) }
        ,{ GET_NAME_AND_VALUE(m_Features2.features, imageLwbeArray) }
        ,{ GET_NAME_AND_VALUE(m_Features2.features, independentBlend) }
        ,{ GET_NAME_AND_VALUE(m_Features2.features, geometryShader) }
        ,{ GET_NAME_AND_VALUE(m_Features2.features, tessellationShader) }
        ,{ GET_NAME_AND_VALUE(m_Features2.features, sampleRateShading) }
        ,{ GET_NAME_AND_VALUE(m_Features2.features, dualSrcBlend) }
        ,{ GET_NAME_AND_VALUE(m_Features2.features, logicOp) }
        ,{ GET_NAME_AND_VALUE(m_Features2.features, multiDrawIndirect) }
        ,{ GET_NAME_AND_VALUE(m_Features2.features, drawIndirectFirstInstance) }
        ,{ GET_NAME_AND_VALUE(m_Features2.features, depthClamp) }
        ,{ GET_NAME_AND_VALUE(m_Features2.features, depthBiasClamp) }
        ,{ GET_NAME_AND_VALUE(m_Features2.features, fillModeNonSolid) }
        ,{ GET_NAME_AND_VALUE(m_Features2.features, depthBounds) }
        ,{ GET_NAME_AND_VALUE(m_Features2.features, wideLines) }
        ,{ GET_NAME_AND_VALUE(m_Features2.features, largePoints) }
        ,{ GET_NAME_AND_VALUE(m_Features2.features, alphaToOne) }
        ,{ GET_NAME_AND_VALUE(m_Features2.features, multiViewport) }
        ,{ GET_NAME_AND_VALUE(m_Features2.features, samplerAnisotropy) }
        ,{ GET_NAME_AND_VALUE(m_Features2.features, textureCompressionETC2) }
        ,{ GET_NAME_AND_VALUE(m_Features2.features, textureCompressionASTC_LDR) }
        ,{ GET_NAME_AND_VALUE(m_Features2.features, textureCompressionBC) }
        ,{ GET_NAME_AND_VALUE(m_Features2.features, occlusionQueryPrecise) }
        ,{ GET_NAME_AND_VALUE(m_Features2.features, pipelineStatisticsQuery) }
        ,{ GET_NAME_AND_VALUE(m_Features2.features, vertexPipelineStoresAndAtomics) }
        ,{ GET_NAME_AND_VALUE(m_Features2.features, fragmentStoresAndAtomics) }
        ,{ GET_NAME_AND_VALUE(m_Features2.features, shaderTessellationAndGeometryPointSize) }
        ,{ GET_NAME_AND_VALUE(m_Features2.features, shaderImageGatherExtended) }
        ,{ GET_NAME_AND_VALUE(m_Features2.features, shaderStorageImageExtendedFormats) }
        ,{ GET_NAME_AND_VALUE(m_Features2.features, shaderStorageImageMultisample) }
        ,{ GET_NAME_AND_VALUE(m_Features2.features, shaderStorageImageReadWithoutFormat) }
        ,{ GET_NAME_AND_VALUE(m_Features2.features, shaderStorageImageWriteWithoutFormat) }
        ,{ GET_NAME_AND_VALUE(m_Features2.features, shaderUniformBufferArrayDynamicIndexing) }
        ,{ GET_NAME_AND_VALUE(m_Features2.features, shaderSampledImageArrayDynamicIndexing) }
        ,{ GET_NAME_AND_VALUE(m_Features2.features, shaderStorageBufferArrayDynamicIndexing) }
        ,{ GET_NAME_AND_VALUE(m_Features2.features, shaderStorageImageArrayDynamicIndexing) }
        ,{ GET_NAME_AND_VALUE(m_Features2.features, shaderClipDistance) }
        ,{ GET_NAME_AND_VALUE(m_Features2.features, shaderLwllDistance) }
        ,{ GET_NAME_AND_VALUE(m_Features2.features, shaderFloat64) }
        ,{ GET_NAME_AND_VALUE(m_Features2.features, shaderInt64) }
        ,{ GET_NAME_AND_VALUE(m_Features2.features, shaderInt16) }
        ,{ GET_NAME_AND_VALUE(m_Features2.features, shaderResourceResidency) }
        ,{ GET_NAME_AND_VALUE(m_Features2.features, shaderResourceMinLod) }
        ,{ GET_NAME_AND_VALUE(m_Features2.features, sparseBinding) }
        ,{ GET_NAME_AND_VALUE(m_Features2.features, sparseResidencyBuffer) }
        ,{ GET_NAME_AND_VALUE(m_Features2.features, sparseResidencyImage2D) }
        ,{ GET_NAME_AND_VALUE(m_Features2.features, sparseResidencyImage3D) }
        ,{ GET_NAME_AND_VALUE(m_Features2.features, sparseResidency2Samples) }
        ,{ GET_NAME_AND_VALUE(m_Features2.features, sparseResidency4Samples) }
        ,{ GET_NAME_AND_VALUE(m_Features2.features, sparseResidency8Samples) }
        ,{ GET_NAME_AND_VALUE(m_Features2.features, sparseResidency16Samples) }
        ,{ GET_NAME_AND_VALUE(m_Features2.features, sparseResidencyAliased) }
        ,{ GET_NAME_AND_VALUE(m_Features2.features, variableMultisampleRate) }
        ,{ GET_NAME_AND_VALUE(m_Features2.features, inheritedQueries) }
        ,{ GET_NAME_AND_VALUE(m_ExtendedDynamicStateFeatures, extendedDynamicState) }
        ,{ GET_NAME_AND_VALUE(m_MeshFeatures, taskShader) }
        ,{ GET_NAME_AND_VALUE(m_MeshFeatures, meshShader) }
        ,{ GET_NAME_AND_VALUE(m_CooperativeMatrixFeatures, cooperativeMatrix) }
        ,{ GET_NAME_AND_VALUE(m_CooperativeMatrixFeatures, cooperativeMatrixRobustBufferAccess) }
        ,{ GET_NAME_AND_VALUE(m_AccelerationStructureFeatures, accelerationStructure) }
        ,{ GET_NAME_AND_VALUE(m_RayTracingPipelineFeatures, rayTracingPipeline) }
        ,{ GET_NAME_AND_VALUE(m_RayQueryFeatures, rayQuery) }
        ,{ GET_NAME_AND_VALUE(m_Features12, samplerMirrorClampToEdge) }
        ,{ GET_NAME_AND_VALUE(m_Features12, drawIndirectCount) }
        ,{ GET_NAME_AND_VALUE(m_Features12, storageBuffer8BitAccess) }
        ,{ GET_NAME_AND_VALUE(m_Features12, uniformAndStorageBuffer8BitAccess) }
        ,{ GET_NAME_AND_VALUE(m_Features12, storagePushConstant8) }
        ,{ GET_NAME_AND_VALUE(m_Features12, shaderBufferInt64Atomics) }
        ,{ GET_NAME_AND_VALUE(m_Features12, shaderSharedInt64Atomics) }
        ,{ GET_NAME_AND_VALUE(m_Features12, shaderFloat16) }
        ,{ GET_NAME_AND_VALUE(m_Features12, shaderInt8) }
        ,{ GET_NAME_AND_VALUE(m_Features12, descriptorIndexing) }
        ,{ GET_NAME_AND_VALUE(m_Features12, shaderInputAttachmentArrayDynamicIndexing) }
        ,{ GET_NAME_AND_VALUE(m_Features12, shaderUniformTexelBufferArrayDynamicIndexing) }
        ,{ GET_NAME_AND_VALUE(m_Features12, shaderStorageTexelBufferArrayDynamicIndexing) }
        ,{ GET_NAME_AND_VALUE(m_Features12, shaderUniformBufferArrayNonUniformIndexing) }
        ,{ GET_NAME_AND_VALUE(m_Features12, shaderSampledImageArrayNonUniformIndexing) }
        ,{ GET_NAME_AND_VALUE(m_Features12, shaderStorageBufferArrayNonUniformIndexing) }
        ,{ GET_NAME_AND_VALUE(m_Features12, shaderStorageImageArrayNonUniformIndexing) }
        ,{ GET_NAME_AND_VALUE(m_Features12, shaderInputAttachmentArrayNonUniformIndexing) }
        ,{ GET_NAME_AND_VALUE(m_Features12, shaderUniformTexelBufferArrayNonUniformIndexing) }
        ,{ GET_NAME_AND_VALUE(m_Features12, shaderStorageTexelBufferArrayNonUniformIndexing) }
        ,{ GET_NAME_AND_VALUE(m_Features12, descriptorBindingUniformBufferUpdateAfterBind) }
        ,{ GET_NAME_AND_VALUE(m_Features12, descriptorBindingSampledImageUpdateAfterBind) }
        ,{ GET_NAME_AND_VALUE(m_Features12, descriptorBindingStorageImageUpdateAfterBind) }
        ,{ GET_NAME_AND_VALUE(m_Features12, descriptorBindingStorageBufferUpdateAfterBind) }
        ,{ GET_NAME_AND_VALUE(m_Features12, descriptorBindingUniformTexelBufferUpdateAfterBind) }
        ,{ GET_NAME_AND_VALUE(m_Features12, descriptorBindingStorageTexelBufferUpdateAfterBind) }
        ,{ GET_NAME_AND_VALUE(m_Features12, descriptorBindingUpdateUnusedWhilePending) }
        ,{ GET_NAME_AND_VALUE(m_Features12, descriptorBindingPartiallyBound) }
        ,{ GET_NAME_AND_VALUE(m_Features12, descriptorBindingVariableDescriptorCount) }
        ,{ GET_NAME_AND_VALUE(m_Features12, runtimeDescriptorArray) }
        ,{ GET_NAME_AND_VALUE(m_Features12, samplerFilterMinmax) }
        ,{ GET_NAME_AND_VALUE(m_Features12, scalarBlockLayout) }
        ,{ GET_NAME_AND_VALUE(m_Features12, imagelessFramebuffer) }
        ,{ GET_NAME_AND_VALUE(m_Features12, uniformBufferStandardLayout) }
        ,{ GET_NAME_AND_VALUE(m_Features12, shaderSubgroupExtendedTypes) }
        ,{ GET_NAME_AND_VALUE(m_Features12, separateDepthStencilLayouts) }
        ,{ GET_NAME_AND_VALUE(m_Features12, hostQueryReset) }
        ,{ GET_NAME_AND_VALUE(m_Features12, timelineSemaphore) }
        ,{ GET_NAME_AND_VALUE(m_Features12, bufferDeviceAddress) }
        ,{ GET_NAME_AND_VALUE(m_Features12, bufferDeviceAddressCaptureReplay) }
        ,{ GET_NAME_AND_VALUE(m_Features12, bufferDeviceAddressMultiDevice) }
        ,{ GET_NAME_AND_VALUE(m_Features12, vulkanMemoryModel) }
        ,{ GET_NAME_AND_VALUE(m_Features12, vulkanMemoryModelDeviceScope) }
        ,{ GET_NAME_AND_VALUE(m_Features12, vulkanMemoryModelAvailabilityVisibilityChains) }
        ,{ GET_NAME_AND_VALUE(m_Features12, shaderOutputViewportIndex) }
        ,{ GET_NAME_AND_VALUE(m_Features12, shaderOutputLayer) }
        ,{ GET_NAME_AND_VALUE(m_Features12, subgroupBroadcastDynamicId) }
    };

    // Print the supported features
    Printf(pri, VkMods::GetTeeModuleCode(), "Device Features:\n");
    for (uint i = 0; i < NUMELEMS(features); i++)
    {
        Printf(pri, VkMods::GetTeeModuleCode(), "%-51s:%s\n", features[i].name, features[i].value);
    }
}

//--------------------------------------------------------------------
// Search through the physical device's supported memory types and see
// if there is a memory type available that matches the user
// properties in memPropFlag.
//
UINT32 VulkanPhysicalDevice::GetMemoryTypeIndex
(
    VkMemoryRequirements memRequirements,
    VkMemoryPropertyFlags memPropFlag
) const
{
    for (UINT32 i = 0; i < m_MemProps.memoryTypeCount; i++)
    {
        if (!(memRequirements.memoryTypeBits & (1 << i)))
        {
            continue;
        }
        // memory type is available, does it match the user's properties ?
        if ((m_MemProps.memoryTypes[i].propertyFlags & memPropFlag) == memPropFlag)
        {
            return i;
        }
    }
    return UINT32_MAX;
}

// TODO: Retrieve information about sparse queues
void VulkanPhysicalDevice::GetBestQueueFamilyIndices()
{
    UINT32 queueCount;
    m_pVulkanInst->GetPhysicalDeviceQueueFamilyProperties(
        m_VkPhysicalDevice, &queueCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueProps(queueCount);
    m_pVulkanInst->GetPhysicalDeviceQueueFamilyProperties(
        m_VkPhysicalDevice, &queueCount, queueProps.data());

    // Keep track of whether we have found a queue family that has only the transfer
    // or compute flag set
    bool bestComputeQueueFound = false;
    bool transferHasGfx        = false;
    bool transferHasCompute    = false;

    for (UINT32 queueFamIdx = 0; queueFamIdx < queueCount; queueFamIdx++)
    {
        const UINT32 queueFlags  = queueProps[queueFamIdx].queueFlags;
        const UINT32 queueCount  = queueProps[queueFamIdx].queueCount;
        const bool   hasGfx      = !!(queueFlags & VK_QUEUE_GRAPHICS_BIT);
        const bool   hasCompute  = !!(queueFlags & VK_QUEUE_COMPUTE_BIT);
        const bool   hasTransfer = !!(queueFlags & VK_QUEUE_TRANSFER_BIT);
        const bool   hasSparseB  = !!(queueFlags & VK_QUEUE_SPARSE_BINDING_BIT);

        Printf(Tee::PriDebug, "Queue family %u flags 0x%x:%s%s%s%s\n",
               queueFamIdx,
               queueFlags,
               hasGfx      ? " graphics" : "",
               hasCompute  ? " compute"  : "",
               hasTransfer ? " transfer" : "",
               hasSparseB  ? " sparse binding" : "");

        // Usually all queue families support transfer, so we expect that
        if (!hasTransfer)
        {
            Printf(Tee::PriLow,
                   "Ignoring unexpected queue family %u which does not support transfer\n",
                   queueFamIdx);
            continue;
        }

        if (hasGfx)
        {
            if (hasCompute)
            {
                if (m_GraphicsQueueFamilyIdx == VK_QUEUE_FLAG_BITS_MAX_ENUM)
                {
                    m_GraphicsQueueFamilyIdx = queueFamIdx;
                    m_MaxGraphicsQueues      = queueCount;
                }
            }
            // We expect graphics queue family to have both compute and transfer capabilities
            else
            {
                Printf(Tee::PriLow,
                       "Unexpected queue family %u supports graphics without compute\n",
                       queueFamIdx);
            }
        }

        if (hasCompute && !bestComputeQueueFound)
        {
            // Prefer queue family which does not support graphics
            if (!hasGfx)
            {
                bestComputeQueueFound = true;
            }

            m_ComputeQueueFamilyIdx = queueFamIdx;
            m_MaxComputeQueues      = queueCount;
        }

        if ((m_TransferQueueFamilyIdx == VK_QUEUE_FLAG_BITS_MAX_ENUM) ||
            (transferHasGfx && !hasGfx) ||                    // Prefer transfer queue without graphics
            (transferHasCompute && !hasCompute && !hasGfx) || // Prefer transfer queue without compute
            (m_TransferQueueFamilyIdx == m_GraphicsQueueFamilyIdx) || // Choose a different queue family than graphics
            (m_TransferQueueFamilyIdx == m_ComputeQueueFamilyIdx))    // Choose a different queue family than compute
        {
            m_TransferQueueFamilyIdx = queueFamIdx;
            m_MaxTransferQueues      = queueCount;
            transferHasGfx           = hasGfx;
            transferHasCompute       = hasCompute;
        }
    }

    // Avoid duplication, since queue families are allocated separately
    if (m_ComputeQueueFamilyIdx == m_GraphicsQueueFamilyIdx)
    {
        m_MaxComputeQueues = 0;
    }
    if ((m_TransferQueueFamilyIdx == m_GraphicsQueueFamilyIdx) ||
        (m_TransferQueueFamilyIdx == m_ComputeQueueFamilyIdx))
    {
        m_MaxTransferQueues = 0;
    }

    // TODO this should return RC if any of these conditions are false
    MASSERT(m_GraphicsQueueFamilyIdx != VK_QUEUE_FLAG_BITS_MAX_ENUM);
    MASSERT(m_ComputeQueueFamilyIdx != VK_QUEUE_FLAG_BITS_MAX_ENUM);
    MASSERT(m_TransferQueueFamilyIdx != VK_QUEUE_FLAG_BITS_MAX_ENUM);

    if (!bestComputeQueueFound)
    {
        Printf(Tee::PriLow, VkMods::GetTeeModuleCode(),
               "Could not find optimal compute queue family\n");
    }
    if (transferHasGfx || transferHasCompute)
    {
        Printf(Tee::PriLow, VkMods::GetTeeModuleCode(),
               "Could not find optimal transfer queue family\n");
    }
}

VkSampleCountFlagBits VulkanPhysicalDevice::GetMaxSampleCount() const
{
    VkSampleCountFlags sampleFlags = min(
    {
        m_Limits.framebufferColorSampleCounts,
        m_Limits.framebufferDepthSampleCounts,
        m_Limits.framebufferStencilSampleCounts
    });

    if (sampleFlags & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
    else if (sampleFlags & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
    else if (sampleFlags & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
    else if (sampleFlags & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
    else if (sampleFlags & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
    else if (sampleFlags & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }

    return VK_SAMPLE_COUNT_1_BIT;
}

bool VulkanPhysicalDevice::IsSampleCountSupported(UINT32 count) const
{
    // Stay under the limit and only one bit can be set
    return (count <= static_cast<UINT32>(GetMaxSampleCount())) &&
           count && !(count & (count - 1));
}

