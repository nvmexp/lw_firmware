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

#include "vksampler.h"
#include "vkdev.h"

using namespace VkMods;

VulkanSampler::VulkanSampler() : VulkanSampler(nullptr) {}

VulkanSampler::VulkanSampler(VulkanDevice* pVulkanDev) :
    m_pVulkanDev(pVulkanDev)
{
}

VulkanSampler::~VulkanSampler()
{
    Cleanup();
}

void VulkanSampler::Cleanup()
{
    if (!m_pVulkanDev)
    {
        MASSERT(m_VkSampler == VK_NULL_HANDLE);
        return;
    }

    if (m_VkSampler)
    {
        m_pVulkanDev->DestroySampler(m_VkSampler);
        m_VkSampler = VK_NULL_HANDLE;
    }
}

VulkanSampler::VulkanSampler(VulkanSampler&& other) noexcept
{
    TakeResources(move(other));
}

VulkanSampler& VulkanSampler::operator=(VulkanSampler&& other)
{
    TakeResources(move(other));
    return *this;
}

void VulkanSampler::TakeResources(VulkanSampler&& other)
{
    if (this != &other)
    {
        Cleanup();

        m_SamplerInfo = other.m_SamplerInfo;
        m_VkSampler = other.m_VkSampler;
        m_pVulkanDev = other.m_pVulkanDev;

        other.m_pVulkanDev = nullptr;
        other.m_VkSampler = VK_NULL_HANDLE;
    }
}

VulkanSampler::operator VkSampler() const
{
    return m_VkSampler;
}

VkResult VulkanSampler::CreateSampler()
{
    return CreateSampler(m_SamplerInfo);
}

VkResult VulkanSampler::CreateSampler(const VkSamplerCreateInfo& samplerInfo)
{
    MASSERT(m_pVulkanDev);
    MASSERT(m_VkSampler == VK_NULL_HANDLE);

    m_SamplerInfo = samplerInfo;
    return m_pVulkanDev->CreateSampler(&m_SamplerInfo, nullptr, &m_VkSampler);
}

VkDescriptorImageInfo VulkanSampler::GetDescriptorImageInfo() const
{
    return
    {
        m_VkSampler               // sampler
       ,VK_NULL_HANDLE            // imageView
       ,VK_IMAGE_LAYOUT_UNDEFINED // imageLayout
    };
}

void VulkanSampler::SetMagFilter(VkFilter filter)
{
    MASSERT(m_VkSampler == VK_NULL_HANDLE && !"Property must be set before sampler is created");
    m_SamplerInfo.magFilter = filter;
}

void VulkanSampler::SetMinFilter(VkFilter filter)
{
    MASSERT(m_VkSampler == VK_NULL_HANDLE && !"Property must be set before sampler is created");
    m_SamplerInfo.minFilter = filter;
}

void VulkanSampler::SetAddressMode(VkSamplerAddressMode addressMode)
{
    MASSERT(m_VkSampler == VK_NULL_HANDLE && !"Property must be set before sampler is created");
    m_SamplerInfo.addressModeU = addressMode;
    m_SamplerInfo.addressModeV = addressMode;
    m_SamplerInfo.addressModeW = addressMode;
}

void VulkanSampler::SetAnisotropy(FLOAT32 anisotropy)
{
    MASSERT(m_VkSampler == VK_NULL_HANDLE && !"Property must be set before sampler is created");
    m_SamplerInfo.anisotropyEnable = VK_TRUE;
    m_SamplerInfo.maxAnisotropy = anisotropy;
}

VkFilter VulkanSampler::GetMagFilter() const
{
    return m_SamplerInfo.magFilter;
}

VkFilter VulkanSampler::GetMinFilter() const
{
    return m_SamplerInfo.minFilter;
}

VkSamplerAddressMode VulkanSampler::GetAddressMode() const
{
    return m_SamplerInfo.addressModeU;
}

FLOAT32 VulkanSampler::GetAnisotropy() const
{
    return m_SamplerInfo.maxAnisotropy;
}

