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
#include "vkframebuffer.h"
#include "vkdev.h"

//--------------------------------------------------------------------
VulkanFrameBuffer::VulkanFrameBuffer(VulkanDevice* pVulkanDev) :
    m_pVulkanDev(pVulkanDev)
{}

//--------------------------------------------------------------------
VulkanFrameBuffer::~VulkanFrameBuffer()
{
    Cleanup();
}

//--------------------------------------------------------------------
void VulkanFrameBuffer::Cleanup()
{
    if (!m_pVulkanDev)
    {
        MASSERT(m_Fb == VK_NULL_HANDLE);
        return;
    }

    m_pVulkanDev->DestroyFramebuffer(m_Fb);
    m_Fb = VK_NULL_HANDLE;
    m_pVulkanDev = nullptr;
}

//--------------------------------------------------------------------
VulkanFrameBuffer::VulkanFrameBuffer(VulkanFrameBuffer&& vulkFb) noexcept
{
    TakeResources(move(vulkFb));
}

VulkanFrameBuffer& VulkanFrameBuffer::operator=(VulkanFrameBuffer&& vulkFb)
{
    TakeResources(move(vulkFb));
    return *this;
}

//--------------------------------------------------------------------
void VulkanFrameBuffer::TakeResources(VulkanFrameBuffer&& other)
{
    if (this != &other)
    {
        Cleanup();

        m_pVulkanDev = other.m_pVulkanDev;
        m_Fb = other.m_Fb;

        other.m_pVulkanDev = nullptr;
        other.m_Fb = VK_NULL_HANDLE;

        m_Width = other.m_Width;
        m_Height = other.m_Height;
        m_Attachments = move(other.m_Attachments);
    }
}

//--------------------------------------------------------------------
VkResult VulkanFrameBuffer::CreateFrameBuffer
(
    vector<VkImageView> attachments,
    VkRenderPass renderPass
)
{
    MASSERT(m_Fb == VK_NULL_HANDLE);

    VkResult res = VK_SUCCESS;
    m_Attachments = move(attachments);

    VkFramebufferCreateInfo fbCreateInfo = {};
    fbCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbCreateInfo.pNext = nullptr;
    fbCreateInfo.renderPass = renderPass;
    fbCreateInfo.attachmentCount = static_cast<UINT32>(m_Attachments.size());
    fbCreateInfo.pAttachments = m_Attachments.data();
    fbCreateInfo.width = m_Width;
    fbCreateInfo.height = m_Height;
    fbCreateInfo.layers = 1;
    CHECK_VK_RESULT(m_pVulkanDev->CreateFramebuffer(&fbCreateInfo, nullptr, &m_Fb));
    return res;
}

