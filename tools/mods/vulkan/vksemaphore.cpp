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

#include "vksemaphore.h"

VkResult VulkanSemaphore::CreateBinarySemaphore()
{
    MASSERT(m_Semaphore == VK_NULL_HANDLE);

    VkSemaphoreCreateInfo semaInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

    return m_pVulkanDev->CreateSemaphore(&semaInfo, nullptr, &m_Semaphore);
}

VkResult VulkanSemaphore::CreateTimelineSemaphore(UINT64 initialValue)
{
    MASSERT(m_Semaphore == VK_NULL_HANDLE);

    VkSemaphoreTypeCreateInfo semaType = { VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO };
    semaType.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    semaType.initialValue  = initialValue;

    VkSemaphoreCreateInfo semaInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    semaInfo.pNext = &semaType;

    return m_pVulkanDev->CreateSemaphore(&semaInfo, nullptr, &m_Semaphore);
}

void VulkanSemaphore::Cleanup()
{
    if (m_Semaphore != VK_NULL_HANDLE)
    {
        m_pVulkanDev->DestroySemaphore(m_Semaphore);
        m_Semaphore = VK_NULL_HANDLE;
    }
}

void VulkanSemaphore::TakeResources(VulkanSemaphore&& sema)
{
    if (&sema != this)
    {
        Cleanup();

        m_pVulkanDev     = sema.m_pVulkanDev;
        m_Semaphore      = sema.m_Semaphore;
        sema.m_Semaphore = VK_NULL_HANDLE;
    }
}
