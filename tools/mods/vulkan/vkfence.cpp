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

#include "vkfence.h"

VkResult VulkanFence::CreateFence(VkFenceCreateFlags flags)
{
    MASSERT(m_Fence == VK_NULL_HANDLE);

    VkFenceCreateInfo fenceInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    fenceInfo.flags = flags;

    return m_pVulkanDev->CreateFence(&fenceInfo, nullptr, &m_Fence);
}

void VulkanFence::Cleanup()
{
    if (m_Fence != VK_NULL_HANDLE)
    {
        m_pVulkanDev->DestroyFence(m_Fence);
        m_Fence = VK_NULL_HANDLE;
    }
}

void VulkanFence::TakeResources(VulkanFence&& fence)
{
    if (&fence != this)
    {
        Cleanup();

        m_pVulkanDev  = fence.m_pVulkanDev;
        m_Fence       = fence.m_Fence;
        fence.m_Fence = VK_NULL_HANDLE;
    }
}
