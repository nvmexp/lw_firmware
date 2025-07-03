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

#include "vkmain.h"

class VulkanDevice;

//--------------------------------------------------------------------
class DescriptorInfo
{
public:
    DescriptorInfo() = default;
    explicit DescriptorInfo(VulkanDevice* pVulkanDev);
    DescriptorInfo(const DescriptorInfo& descInfo) = delete;
    DescriptorInfo& operator=(const DescriptorInfo& descInfo) = delete;
    DescriptorInfo(DescriptorInfo&& descInfo) { TakeResources(move(descInfo)); }
    DescriptorInfo& operator=(DescriptorInfo&& descInfo) { TakeResources(move(descInfo)); return *this; }
    ~DescriptorInfo();
    VkResult CreateDescriptorSetLayout
    (
        UINT32 descriptorSetLayoutIndex,
        UINT32 numBindings,
        const VkDescriptorSetLayoutBinding *pBindings
    );

    VkResult CreateDescriptorPool
    (
        UINT32 maxSets
       ,const vector<VkDescriptorPoolSize> &descPoolSizeInfo
    );
    VkResult AllocateDescriptorSets
    (
        UINT32 firstDescriptorSetIndex,
        UINT32 count,
        UINT32 firstDescriptorLayoutIndex
    );
    void UpdateDescriptorSets(VkWriteDescriptorSet *pWriteData, UINT32 numDescriptors) const;

    VkResult Cleanup();

    VkDescriptorSet GetDescriptorSet(UINT32 i)
    {
        MASSERT(i < m_DescriptorSet.size());
        return m_DescriptorSet[i];
    }

    const vector<VkDescriptorSet>& GetAllDescriptorSets()
    {
        return m_DescriptorSet;
    }

    VkDescriptorSetLayout GetDescriptorSetLayout(UINT32 i)
    {
        MASSERT(i < m_DescriptorSetLayout.size());
        return m_DescriptorSetLayout[i];
    }

    const vector<VkDescriptorSetLayout>& GetAllDescriptorSetLayouts()
    {
        return m_DescriptorSetLayout;
    }

private:
    void TakeResources(DescriptorInfo&& descInfo);

    VulkanDevice                   *m_pVulkanDev = nullptr;

    VkDescriptorPool                m_DescriptorPool = VK_NULL_HANDLE;

    vector<VkDescriptorSetLayout>   m_DescriptorSetLayout;
    vector<VkDescriptorSet>         m_DescriptorSet;
};
