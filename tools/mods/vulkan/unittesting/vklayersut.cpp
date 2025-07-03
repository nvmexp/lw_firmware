/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2017,2019 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

// Warning: DO NOT INCLUDE "vkmain.h" here as it renames vk* functions to disabledVk*
#include "vulkan/vk_layer.h"
#include "vk_layer_dispatch_table.h"
#include "vklayers.h"

VkResult VulkanLayers::Init(bool enableLayersInMODS)
{
    return VK_SUCCESS;
}

VkResult VulkanLayers::EnumerateInstanceLayerProperties
(
    uint32_t*                                   pPropertyCount,
    VkLayerProperties*                          pProperties
)
{
    return VK_SUCCESS;
}

VkResult VulkanLayers::EnumerateInstanceExtensionProperties
(
    const char*                                 pLayerName,
    uint32_t*                                   pPropertyCount,
    VkExtensionProperties*                      pProperties
)
{
    return VK_SUCCESS;
}

uint32_t VulkanLayers::EnumerateInstanceVersion()
{
    return VK_API_VERSION_1_0;
}

VkResult VulkanLayers::CreateInstance
(
    const VkInstanceCreateInfo*                 pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkInstance*                                 pInstance,
    VkLayerInstanceDispatchTable*               pvi
)
{
    return VK_SUCCESS;
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
    return VK_SUCCESS;
}
