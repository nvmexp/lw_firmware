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
#ifndef VK_FRAMEBUFFER_H
#define VK_FRAMEBUFFER_H

#include "vkmain.h"

class VulkanDevice;

//--------------------------------------------------------------------
class VulkanFrameBuffer
{
public:
    VulkanFrameBuffer() = default;
    explicit VulkanFrameBuffer(VulkanDevice* pVulkanDev);
    ~VulkanFrameBuffer();
    VulkanFrameBuffer(VulkanFrameBuffer&& vulkFb) noexcept;
    VulkanFrameBuffer& operator=(VulkanFrameBuffer&& vulkFb);
    VulkanFrameBuffer(const VulkanFrameBuffer& vulkFb) = delete;
    VulkanFrameBuffer& operator=(const VulkanFrameBuffer& vulkFb) = delete;

    VkResult CreateFrameBuffer(vector<VkImageView> attachments, VkRenderPass renderPass);
    void Cleanup();

    SETGET_PROP(Width,      UINT32);
    SETGET_PROP(Height,     UINT32);
    VkFramebuffer GetVkFrameBuffer() const { return m_Fb; }

private:
    VulkanDevice       *m_pVulkanDev = nullptr;
    UINT32              m_Width      = 0;
    UINT32              m_Height     = 0;
    vector<VkImageView> m_Attachments;
    VkFramebuffer       m_Fb         = VK_NULL_HANDLE;

    void TakeResources(VulkanFrameBuffer&& other);
};

#endif
