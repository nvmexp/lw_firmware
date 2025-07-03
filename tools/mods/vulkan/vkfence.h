/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "vkdev.h"

class VulkanFence
{
    public:
        VulkanFence() = default;
        explicit VulkanFence(VulkanDevice* pVulkanDev) : m_pVulkanDev(pVulkanDev) { }
        ~VulkanFence() { Cleanup(); }

        VulkanFence(const VulkanFence&)            = delete;
        VulkanFence& operator=(const VulkanFence&) = delete;

        VulkanFence(VulkanFence&& fence)            { TakeResources(move(fence)); }
        VulkanFence& operator=(VulkanFence&& fence) { TakeResources(move(fence)); return *this; }

        VkResult CreateFence(VkFenceCreateFlags flags = 0);
        void Cleanup();

        GET_PROP(Fence, VkFence);

    private:
        void TakeResources(VulkanFence&& fence);

        VulkanDevice* m_pVulkanDev = nullptr;
        VkFence       m_Fence      = VK_NULL_HANDLE;
};
