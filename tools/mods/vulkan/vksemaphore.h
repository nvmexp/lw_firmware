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

class VulkanSemaphore
{
    public:
        VulkanSemaphore() = default;
        explicit VulkanSemaphore(VulkanDevice* pVulkanDev) : m_pVulkanDev(pVulkanDev) { }
        ~VulkanSemaphore() { Cleanup(); }

        VulkanSemaphore(const VulkanSemaphore&)            = delete;
        VulkanSemaphore& operator=(const VulkanSemaphore&) = delete;

        VulkanSemaphore(VulkanSemaphore&& sema)            { TakeResources(move(sema)); }
        VulkanSemaphore& operator=(VulkanSemaphore&& sema) { TakeResources(move(sema)); return *this; }

        VkResult CreateBinarySemaphore();
        VkResult CreateTimelineSemaphore(UINT64 initialValue = 0);
        void Cleanup();

        GET_PROP(Semaphore, const VkSemaphore&);

    private:
        void TakeResources(VulkanSemaphore&& sema);

        VulkanDevice* m_pVulkanDev = nullptr;
        VkSemaphore   m_Semaphore  = VK_NULL_HANDLE;
};
