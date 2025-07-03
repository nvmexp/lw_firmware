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

#include "vkdescriptor.h"
#include "vkdev.h"

//--------------------------------------------------------------------
DescriptorInfo::DescriptorInfo(VulkanDevice* pVulkanDev)
    : m_pVulkanDev(pVulkanDev)
{}

//--------------------------------------------------------------------
DescriptorInfo::~DescriptorInfo()
{
    Cleanup();
}

//--------------------------------------------------------------------
VkResult DescriptorInfo::Cleanup()
{
    VkResult res = VK_SUCCESS;

    if (!m_pVulkanDev)
    {
        MASSERT(m_DescriptorSet.empty());
        MASSERT(m_DescriptorSetLayout.empty());
        MASSERT(m_DescriptorPool == VK_NULL_HANDLE);
        return res;
    }

    for (size_t i = 0; i < m_DescriptorSet.size(); i++)
    {
        if (m_DescriptorSet[i])
        {
            res = m_pVulkanDev->FreeDescriptorSets(m_DescriptorPool,
                1, &m_DescriptorSet[i]);
        }
    }
    m_DescriptorSet.clear();

    for (size_t i = 0; i < m_DescriptorSetLayout.size(); i++)
    {
        if (m_DescriptorSetLayout[i])
        {
            m_pVulkanDev->DestroyDescriptorSetLayout(m_DescriptorSetLayout[i]);
        }
    }
    m_DescriptorSetLayout.clear();

    if (m_DescriptorPool)
    {
        m_pVulkanDev->DestroyDescriptorPool(m_DescriptorPool);
        m_DescriptorPool = VK_NULL_HANDLE;
    }

    return res;
}

//--------------------------------------------------------------------
VkResult DescriptorInfo::CreateDescriptorSetLayout
(
    UINT32 descriptorSetLayoutIndex,
    UINT32 numBindings,
    const VkDescriptorSetLayoutBinding *pBindings
)
{
    if (descriptorSetLayoutIndex >= m_DescriptorSetLayout.size())
    {
        m_DescriptorSetLayout.resize(1 + descriptorSetLayoutIndex, VK_NULL_HANDLE);
    }
    MASSERT(m_DescriptorSetLayout[descriptorSetLayoutIndex] == VK_NULL_HANDLE);

    MASSERT(numBindings > 0);
    VkDescriptorSetLayoutCreateInfo descriptorLayout = {};
    descriptorLayout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorLayout.pNext = nullptr;
    descriptorLayout.bindingCount = numBindings;
    descriptorLayout.pBindings = pBindings;

    return m_pVulkanDev->CreateDescriptorSetLayout(&descriptorLayout, nullptr,
        &m_DescriptorSetLayout[descriptorSetLayoutIndex]);
}

//--------------------------------------------------------------------
VkResult DescriptorInfo::CreateDescriptorPool
(
    UINT32 maxSets,
    const vector<VkDescriptorPoolSize> &descPoolSizeInfo
)
{
    MASSERT(m_DescriptorPool == VK_NULL_HANDLE);
    MASSERT(maxSets > 0);
    VkDescriptorPoolCreateInfo descriptorPool = {};
    descriptorPool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPool.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    descriptorPool.maxSets = maxSets;
    descriptorPool.poolSizeCount = static_cast<UINT32>(descPoolSizeInfo.size());
    descriptorPool.pPoolSizes = descPoolSizeInfo.data();
    return m_pVulkanDev->CreateDescriptorPool(&descriptorPool, nullptr, &m_DescriptorPool);
}

//--------------------------------------------------------------------
VkResult DescriptorInfo::AllocateDescriptorSets(UINT32 firstDescriptorSetIndex,
                                                UINT32 count,
                                                UINT32 firstDescriptorLayoutIndex)
{
    const UINT32 endIdx = firstDescriptorSetIndex + count;

    if (endIdx > m_DescriptorSet.size())
    {
        m_DescriptorSet.resize(endIdx, VK_NULL_HANDLE);
    }

    for (UINT32 i = firstDescriptorSetIndex; i < endIdx; i++)
    {
        MASSERT(m_DescriptorSet[i] == VK_NULL_HANDLE);
    }
    MASSERT(firstDescriptorLayoutIndex + count <= m_DescriptorSetLayout.size());

    VkDescriptorSetAllocateInfo descriptorAllocInfo = {};
    descriptorAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorAllocInfo.pNext = nullptr;
    descriptorAllocInfo.descriptorPool = m_DescriptorPool;
    descriptorAllocInfo.descriptorSetCount = count;
    descriptorAllocInfo.pSetLayouts = &m_DescriptorSetLayout[firstDescriptorLayoutIndex];
    return m_pVulkanDev->AllocateDescriptorSets(&descriptorAllocInfo,
                                                &m_DescriptorSet[firstDescriptorSetIndex]);
}

//--------------------------------------------------------------------
void DescriptorInfo::UpdateDescriptorSets
(
    VkWriteDescriptorSet *pWriteData,
    UINT32 numDescriptors
) const
{
    m_pVulkanDev->UpdateDescriptorSets(numDescriptors, pWriteData, 0, nullptr);
}

void DescriptorInfo::TakeResources(DescriptorInfo&& descInfo)
{
    if (this != &descInfo)
    {
        Cleanup();

        m_pVulkanDev          = descInfo.m_pVulkanDev;
        m_DescriptorPool      = descInfo.m_DescriptorPool;
        m_DescriptorSetLayout = move(descInfo.m_DescriptorSetLayout);
        m_DescriptorSet       = move(descInfo.m_DescriptorSet);

        descInfo.m_DescriptorPool = VK_NULL_HANDLE;
    }
}
