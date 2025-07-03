/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2017 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

#pragma once

namespace VulkanLayers
{
    VkResult Init(bool enableLayersInMODS);

    VkResult EnumerateInstanceExtensionProperties
    (
        const char*                                 pLayerName,
        uint32_t*                                   pPropertyCount,
        VkExtensionProperties*                      pProperties
    );

    VkResult EnumerateInstanceLayerProperties
    (
        uint32_t*                                   pPropertyCount,
        VkLayerProperties*                          pProperties
    );

    uint32_t EnumerateInstanceVersion();

    VkResult CreateInstance
    (
        const VkInstanceCreateInfo*                 pCreateInfo,
        const VkAllocationCallbacks*                pAllocator,
        VkInstance*                                 pInstance,
        VkLayerInstanceDispatchTable*               pvi
    );

    VkResult CreateDevice
    (
        VkInstance                                  instance,
        VkPhysicalDevice                            physicalDevice,
        const VkDeviceCreateInfo*                   pCreateInfo,
        const VkAllocationCallbacks*                pAllocator,
        VkDevice*                                   pDevice,
        VkLayerDispatchTable*                       pvd
    );
}

