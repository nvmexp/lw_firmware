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

#include "vkmain.h"

class VulkanDevice;

//! \brief Wraps a VkSampler
class VulkanSampler
{
public:
    VulkanSampler();
    explicit VulkanSampler(VulkanDevice* pVulkanDev);
    ~VulkanSampler();
    VulkanSampler(VulkanSampler&& vulkSampler) noexcept;
    VulkanSampler& operator=(VulkanSampler&& vulkSampler);
    VulkanSampler(const VulkanSampler& vulkSampler) = delete;
    VulkanSampler& operator=(const VulkanSampler& vulkSampler) = delete;
    operator VkSampler() const;

    void Cleanup();
    VkResult CreateSampler();
    VkResult CreateSampler(const VkSamplerCreateInfo& samplerInfo);

    VkDescriptorImageInfo GetDescriptorImageInfo() const;

    // NOTE: must call Set*() before CreateSampler()
    void SetMagFilter(VkFilter filter);
    void SetMinFilter(VkFilter filter);
    void SetAddressMode(VkSamplerAddressMode addressMode);
    void SetAnisotropy(FLOAT32 anisotropy);

    GET_PROP(VkSampler, VkSampler);
    VkFilter GetMagFilter() const;
    VkFilter GetMinFilter() const;
    VkSamplerAddressMode GetAddressMode() const;
    FLOAT32 GetAnisotropy() const;

    // TODO: add functions to support mip-mapping

private:
    VkSamplerCreateInfo m_SamplerInfo =
    {
        VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO  // sType;
        ,nullptr                               // pNext;
        ,0                                     // flags;
        ,VK_FILTER_NEAREST                     // magFilter;
        ,VK_FILTER_NEAREST                     // minFilter;
        ,VK_SAMPLER_MIPMAP_MODE_NEAREST        // mipmapMode;
        ,VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE // addressModeU;
        ,VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE // addressModeV;
        ,VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE // addressModeW;
        ,0.0f                                  // mipLodBias;
        ,VK_FALSE                              // anisotropyEnable;
        ,0.0f                                  // maxAnisotropy;
        ,VK_FALSE                              // compareEnable;
        ,VK_COMPARE_OP_NEVER                   // compareOp;
        ,0.0f                                  // minLod;
        ,0.0f                                  // maxLod;
        ,VK_BORDER_COLOR_INT_OPAQUE_WHITE      // borderColor;
        ,VK_FALSE                              // unnormalizedCoordinates
    };

    VkSampler m_VkSampler = VK_NULL_HANDLE;
    VulkanDevice* m_pVulkanDev = nullptr;

    void TakeResources(VulkanSampler&& other);
};

