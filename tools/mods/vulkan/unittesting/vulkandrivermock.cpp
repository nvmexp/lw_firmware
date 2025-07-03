/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2017, 2019-2020 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

#include "vkinstance.h"

namespace
{
    VKAPI_ATTR VkResult VKAPI_CALL UnitTestEnumeratePhysicalDevices
    (
        VkInstance                                  instance,
        uint32_t*                                   pPhysicalDeviceCount,
        VkPhysicalDevice*                           pPhysicalDevices
    )
    {
        *pPhysicalDeviceCount = 1;
        if (pPhysicalDevices == nullptr)
            return VK_SUCCESS;

        pPhysicalDevices[0] = reinterpret_cast<VkPhysicalDevice>(1);

        return VK_SUCCESS;
    }

    VKAPI_ATTR void VKAPI_CALL UnitTestGetPhysicalDeviceProperties
    (
        VkPhysicalDevice                            physicalDevice,
        VkPhysicalDeviceProperties*                 pProperties
    )
    {
    }

    VKAPI_ATTR void VKAPI_CALL UnitTestGetPhysicalDeviceProperties2
    (
        VkPhysicalDevice                            physicalDevice,
        VkPhysicalDeviceProperties2*                pProperties
    )
    {
    }

    VKAPI_ATTR void VKAPI_CALL UnitTestGetPhysicalDeviceMemoryProperties
    (
        VkPhysicalDevice                            physicalDevice,
        VkPhysicalDeviceMemoryProperties*           pMemoryProperties
    )
    {
        pMemoryProperties->memoryTypeCount = VK_MAX_MEMORY_TYPES;
        for (UINT32 idx = 0; idx < pMemoryProperties->memoryTypeCount; idx++)
        {
            pMemoryProperties->memoryTypes[idx].propertyFlags = VK_MEMORY_PROPERTY_FLAG_BITS_MAX_ENUM;
        }
    }

    VKAPI_ATTR void VKAPI_CALL UnitTestGetPhysicalDeviceFeatures
    (
        VkPhysicalDevice                            physicalDevice,
        VkPhysicalDeviceFeatures*                   pFeatures
    )
    {
        memset(pFeatures, 0, sizeof(VkPhysicalDeviceFeatures));
    }

    VKAPI_ATTR void VKAPI_CALL UnitTestGetPhysicalDeviceFeatures2
    (
        VkPhysicalDevice                            physicalDevice,
        VkPhysicalDeviceFeatures2*                   pFeatures
    )
    {
        memset(pFeatures, 0, sizeof(VkPhysicalDeviceFeatures2));
    }

    VKAPI_ATTR void VKAPI_CALL UnitTestGetPhysicalDeviceQueueFamilyProperties
    (
        VkPhysicalDevice                            physicalDevice,
        uint32_t*                                   pQueueFamilyPropertyCount,
        VkQueueFamilyProperties*                    pQueueFamilyProperties
    )
    {
        if (!pQueueFamilyProperties)
        {
            *pQueueFamilyPropertyCount = 1;
            return;
        }

        pQueueFamilyProperties->queueCount = 1;
        pQueueFamilyProperties->queueFlags =
            VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT | VK_QUEUE_GRAPHICS_BIT;
    }

    VKAPI_ATTR VkResult VKAPI_CALL UnitTestEnumerateDeviceExtensionProperties
    (
        VkPhysicalDevice                            physicalDevice,
        const char*                                 pLayerName,
        uint32_t*                                   pPropertyCount,
        VkExtensionProperties*                      pProperties
    )
    {
        *pPropertyCount = 0;
        return VK_SUCCESS;
    }

    VKAPI_ATTR VkResult VKAPI_CALL UnitTestCreateImage
    (
        VkDevice                                    device,
        const VkImageCreateInfo*                    pCreateInfo,
        const VkAllocationCallbacks*                pAllocator,
        VkImage*                                    pImage
    )
    {
        return VK_SUCCESS;
    }

    VKAPI_ATTR void VKAPI_CALL UnitTestGetImageSubresourceLayout
    (
        VkDevice                                    device,
        VkImage                                     image,
        const VkImageSubresource*                   pSubresource,
        VkSubresourceLayout*                        pLayout
    )
    {
        pLayout->rowPitch = 14;
    }

    VKAPI_ATTR void VKAPI_CALL UnitTestGetImageMemoryRequirements
    (
        VkDevice                                    device,
        VkImage                                     image,
        VkMemoryRequirements*                       pMemoryRequirements
    )
    {
        pMemoryRequirements->size = 4096;
        pMemoryRequirements->alignment = 4;
        pMemoryRequirements->memoryTypeBits = 0xFFFFFFFF;
    }

    VKAPI_ATTR VkResult VKAPI_CALL UnitTestCreateBuffer
    (
        VkDevice                                    device,
        const VkBufferCreateInfo*                   pCreateInfo,
        const VkAllocationCallbacks*                pAllocator,
        VkBuffer*                                   pImage
    )
    {
        return VK_SUCCESS;
    }

    VKAPI_ATTR void VKAPI_CALL UnitTestGetBufferMemoryRequirements
    (
        VkDevice                                    device,
        VkBuffer                                    buffer,
        VkMemoryRequirements*                       pMemoryRequirements
    )
    {
        pMemoryRequirements->size = 0x100;
        pMemoryRequirements->alignment = 0x100;
        pMemoryRequirements->memoryTypeBits = 0x681;
    }

    VKAPI_ATTR VkResult VKAPI_CALL UnitTestBindBufferMemory
    (
        VkDevice                                    device,
        VkBuffer                                    buffer,
        VkDeviceMemory                              memory,
        VkDeviceSize                                memoryOffset
    )
    {
        return VK_SUCCESS;
    }

    VKAPI_ATTR VkResult VKAPI_CALL UnitTestAllocateMemory
    (
        VkDevice                                    device,
        const VkMemoryAllocateInfo*                 pAllocateInfo,
        const VkAllocationCallbacks*                pAllocator,
        VkDeviceMemory*                             pMemory
    )
    {
        return VK_SUCCESS;
    }

    VKAPI_ATTR VkResult VKAPI_CALL UnitTestBindImageMemory
    (
        VkDevice                                    device,
        VkImage                                     image,
        VkDeviceMemory                              memory,
        VkDeviceSize                                memoryOffset
    )
    {
        return VK_SUCCESS;
    }

    VKAPI_ATTR void VKAPI_CALL UnitTestCmdPipelineBarrier
    (
        VkCommandBuffer                             commandBuffer,
        VkPipelineStageFlags                        srcStageMask,
        VkPipelineStageFlags                        dstStageMask,
        VkDependencyFlags                           dependencyFlags,
        uint32_t                                    memoryBarrierCount,
        const VkMemoryBarrier*                      pMemoryBarriers,
        uint32_t                                    bufferMemoryBarrierCount,
        const VkBufferMemoryBarrier*                pBufferMemoryBarriers,
        uint32_t                                    imageMemoryBarrierCount,
        const VkImageMemoryBarrier*                 pImageMemoryBarriers
    )
    {
    }

    VKAPI_ATTR VkResult VKAPI_CALL UnitTestBeginCommandBuffer
    (
        VkCommandBuffer                             commandBuffer,
        const VkCommandBufferBeginInfo*             pBeginInfo
    )
    {
        return VK_SUCCESS;
    }

    VKAPI_ATTR VkResult VKAPI_CALL UnitTestEndCommandBuffer
    (
        VkCommandBuffer                             commandBuffer
    )
    {
        return VK_SUCCESS;
    }

    VKAPI_ATTR VkResult VKAPI_CALL UnitTestResetCommandBuffer
    (
        VkCommandBuffer                             commandBuffer,
        VkCommandBufferResetFlags                   flags
    )
    {
        return VK_SUCCESS;
    }

    VKAPI_ATTR VkResult VKAPI_CALL UnitTestQueueSubmit
    (
        VkQueue                                     queue,
        uint32_t                                    submitCount,
        const VkSubmitInfo*                         pSubmits,
        VkFence                                     fence
    )
    {
        return VK_SUCCESS;
    }

    VKAPI_ATTR VkResult VKAPI_CALL UnitTestCreateFence
    (
        VkDevice                                    device,
        const VkFenceCreateInfo*                    pCreateInfo,
        const VkAllocationCallbacks*                pAllocator,
        VkFence*                                    pFence
    )
    {
        return VK_SUCCESS;
    }

    VKAPI_ATTR VkResult VKAPI_CALL UnitTestWaitForFences
    (
        VkDevice                                    device,
        uint32_t                                    fenceCount,
        const VkFence*                              pFences,
        VkBool32                                    waitAll,
        uint64_t                                    timeout
    )
    {
        return VK_SUCCESS;
    }

    VKAPI_ATTR VkResult VKAPI_CALL UnitTestResetFences
    (
        VkDevice                                    device,
        uint32_t                                    fenceCount,
        const VkFence*                              pFences
    )
    {
        return VK_SUCCESS;
    }

    VKAPI_ATTR void VKAPI_CALL UnitTestDestroyFence
    (
        VkDevice                                    device,
        VkFence                                     fence,
        const VkAllocationCallbacks*                pAllocator
    )
    {
    }

    VKAPI_ATTR void VKAPI_CALL UnitTestGetDeviceQueue(
        VkDevice                                    device,
        uint32_t                                    queueFamilyIndex,
        uint32_t                                    queueIndex,
        VkQueue*                                    pQueue
    )
    {
    }

    VKAPI_ATTR VkResult VKAPI_CALL UnitTestCreateCommandPool
    (
        VkDevice                                    device,
        const VkCommandPoolCreateInfo*              pCreateInfo,
        const VkAllocationCallbacks*                pAllocator,
        VkCommandPool*                              pCommandPool
    )
    {
        *pCommandPool = reinterpret_cast<VkCommandPool>(2ULL);
        return VK_SUCCESS;
    }

    VKAPI_ATTR void VKAPI_CALL UnitTestDestroyCommandPool
    (
        VkDevice                                    device,
        VkCommandPool                               commandPool,
        const VkAllocationCallbacks*                pAllocator
    )
    {
    }

    VKAPI_ATTR VkResult VKAPI_CALL UnitTestAllocateCommandBuffers
    (
        VkDevice                                    device,
        const VkCommandBufferAllocateInfo*          pAllocateInfo,
        VkCommandBuffer*                            pCommandBuffers
    )
    {
        for (UINT64 ii = 0; ii < static_cast<UINT64>(pAllocateInfo->commandBufferCount); ii++)
        {
            pCommandBuffers[ii] = reinterpret_cast<VkCommandBuffer>(ii+1);
        }
        return VK_SUCCESS;
    }

    VKAPI_ATTR void VKAPI_CALL UnitTestCmdCopyImage
    (
        VkCommandBuffer                             commandBuffer,
        VkImage                                     srcImage,
        VkImageLayout                               srcImageLayout,
        VkImage                                     dstImage,
        VkImageLayout                               dstImageLayout,
        uint32_t                                    regionCount,
        const VkImageCopy*                          pRegions
    )
    {
    }

    VKAPI_ATTR void VKAPI_CALL UnitTestCmdCopyImageToBuffer
    (
        VkCommandBuffer                             commandBuffer,
        VkImage                                     srcImage,
        VkImageLayout                               srcImageLayout,
        VkBuffer                                    dstBuffer,
        uint32_t                                    regionCount,
        const VkBufferImageCopy*                    pRegions
    )
    {
    }

    UINT08 DummyMemory[4096];
    VKAPI_ATTR VkResult VKAPI_CALL UnitTestMapMemory
    (
        VkDevice                                    device,
        VkDeviceMemory                              memory,
        VkDeviceSize                                offset,
        VkDeviceSize                                size,
        VkMemoryMapFlags                            flags,
        void**                                      ppData
    )
    {
        *ppData = DummyMemory;
        return VK_SUCCESS;
    }

    VKAPI_ATTR void VKAPI_CALL UnitTestUnmapMemory
    (
        VkDevice                                    device,
        VkDeviceMemory                              memory
    )
    {
    }

    VKAPI_ATTR void VKAPI_CALL UnitTestDestroyDevice
    (
        VkDevice                                    device,
        const VkAllocationCallbacks*                pAllocator
    )
    {
    }

    VKAPI_ATTR void VKAPI_CALL UnitTestFreeCommandBuffers
    (
        VkDevice device,
        VkCommandPool commandPool,
        uint32_t commandBufferCount,
        const VkCommandBuffer* pCommandBuffers
    )
    {
    }

    VKAPI_ATTR void VKAPI_CALL UnitTestFreeMemory
    (
        VkDevice device,
        VkDeviceMemory memory,
        const VkAllocationCallbacks* pAllocator
    )
    {
    }

    VKAPI_ATTR void VKAPI_CALL UnitTestDestroyImage
    (
        VkDevice device,
        VkImage image,
        const VkAllocationCallbacks* pAllocator
    )
    {
    }
    VKAPI_ATTR void VKAPI_CALL UnitTestDestroyImageView
    (
        VkDevice device,
        VkImageView imageView,
        const VkAllocationCallbacks* pAllocator
    )
    {
    }
    VKAPI_ATTR void VKAPI_CALL UnitTestDestroyBuffer
    (
        VkDevice device,
        VkBuffer buffer,
        const VkAllocationCallbacks* pAllocator
    )
    {
    }
}

VulkanInstance::VulkanInstance(UINT32 unitTestingVariantIndex)
{
    m_UnitTestingVariantIndex = unitTestingVariantIndex;
    switch (unitTestingVariantIndex)
    {
        case 1:
#define ASSIGN_UNIT_TEST_INSTANCE_FUNCTION(name) VkLayerInstanceDispatchTable::name = UnitTest##name;
            ASSIGN_UNIT_TEST_INSTANCE_FUNCTION(EnumeratePhysicalDevices)
            ASSIGN_UNIT_TEST_INSTANCE_FUNCTION(GetPhysicalDeviceProperties)
            ASSIGN_UNIT_TEST_INSTANCE_FUNCTION(GetPhysicalDeviceProperties2)
            ASSIGN_UNIT_TEST_INSTANCE_FUNCTION(GetPhysicalDeviceMemoryProperties)
            ASSIGN_UNIT_TEST_INSTANCE_FUNCTION(GetPhysicalDeviceFeatures)
            ASSIGN_UNIT_TEST_INSTANCE_FUNCTION(GetPhysicalDeviceFeatures2)
            ASSIGN_UNIT_TEST_INSTANCE_FUNCTION(GetPhysicalDeviceQueueFamilyProperties)
            ASSIGN_UNIT_TEST_INSTANCE_FUNCTION(EnumerateDeviceExtensionProperties)
#undef ASSIGN_UNIT_TEST_INSTANCE_FUNCTION

            break;
        default:
            break;
    }
}

VkResult VulkanDevice::Initialize(UINT32 unitTestingVariantIndex)
{
    // Make sure Initialize() is not called multiple times
    if (m_VkDevice != VK_NULL_HANDLE)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    switch (unitTestingVariantIndex)
    {
        case 1:
#define ASSIGN_UNIT_TEST_FUNCTION(name) VkLayerDispatchTable::name = UnitTest##name;
            ASSIGN_UNIT_TEST_FUNCTION(CreateImage)
            ASSIGN_UNIT_TEST_FUNCTION(GetImageSubresourceLayout)
            ASSIGN_UNIT_TEST_FUNCTION(GetImageMemoryRequirements)
            ASSIGN_UNIT_TEST_FUNCTION(AllocateMemory)
            ASSIGN_UNIT_TEST_FUNCTION(BindImageMemory)
            ASSIGN_UNIT_TEST_FUNCTION(CreateBuffer)
            ASSIGN_UNIT_TEST_FUNCTION(GetBufferMemoryRequirements)
            ASSIGN_UNIT_TEST_FUNCTION(BindBufferMemory)
            ASSIGN_UNIT_TEST_FUNCTION(CmdPipelineBarrier)
            ASSIGN_UNIT_TEST_FUNCTION(BeginCommandBuffer)
            ASSIGN_UNIT_TEST_FUNCTION(EndCommandBuffer)
            ASSIGN_UNIT_TEST_FUNCTION(ResetCommandBuffer)
            ASSIGN_UNIT_TEST_FUNCTION(QueueSubmit)
            ASSIGN_UNIT_TEST_FUNCTION(CreateFence)
            ASSIGN_UNIT_TEST_FUNCTION(WaitForFences)
            ASSIGN_UNIT_TEST_FUNCTION(ResetFences)
            ASSIGN_UNIT_TEST_FUNCTION(DestroyFence)
            ASSIGN_UNIT_TEST_FUNCTION(GetDeviceQueue)
            ASSIGN_UNIT_TEST_FUNCTION(CreateCommandPool)
            ASSIGN_UNIT_TEST_FUNCTION(DestroyCommandPool)
            ASSIGN_UNIT_TEST_FUNCTION(AllocateCommandBuffers)
            ASSIGN_UNIT_TEST_FUNCTION(CmdCopyImage)
            ASSIGN_UNIT_TEST_FUNCTION(CmdCopyImageToBuffer)
            ASSIGN_UNIT_TEST_FUNCTION(MapMemory)
            ASSIGN_UNIT_TEST_FUNCTION(UnmapMemory)
            ASSIGN_UNIT_TEST_FUNCTION(DestroyDevice)
            ASSIGN_UNIT_TEST_FUNCTION(FreeCommandBuffers)
            ASSIGN_UNIT_TEST_FUNCTION(FreeMemory)
            ASSIGN_UNIT_TEST_FUNCTION(DestroyImage)
            ASSIGN_UNIT_TEST_FUNCTION(DestroyImageView)
            ASSIGN_UNIT_TEST_FUNCTION(DestroyBuffer)
#undef ASSIGN_UNIT_TEST_FUNCTION
            break;
        default:
            break;
    }

    m_VkDevice = reinterpret_cast<VkDevice>(1);
    m_GraphicsQueues.push_back(reinterpret_cast<VkQueue>(1));
    m_ComputeQueues.push_back(reinterpret_cast<VkQueue>(1));
    m_TransferQueues.push_back(reinterpret_cast<VkQueue>(1));
    return VK_SUCCESS;
}

