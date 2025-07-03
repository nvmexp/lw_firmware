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

#include "vkrenderpass.h"
#include "vkutil.h"
#include "vkdev.h"

//--------------------------------------------------------------------
VulkanRenderPass::VulkanRenderPass(VulkanDevice* pVulkanDev) :
    m_pVulkanDev(pVulkanDev)
{}

//--------------------------------------------------------------------
VulkanRenderPass::~VulkanRenderPass()
{
    Cleanup();
}

//--------------------------------------------------------------------
VulkanRenderPass::VulkanRenderPass(VulkanRenderPass&& other) noexcept
{
    TakeResources(move(other));
}

VulkanRenderPass& VulkanRenderPass::operator=(VulkanRenderPass&& other)
{
    TakeResources(move(other));
    return *this;
}

void VulkanRenderPass::TakeResources(VulkanRenderPass&& other)
{
    if (this != &other)
    {
        Cleanup();

        m_pVulkanDev = other.m_pVulkanDev;
        m_RenderPass = other.m_RenderPass;

        other.m_pVulkanDev = nullptr;
        other.m_RenderPass = VK_NULL_HANDLE;

        m_Attachments       = other.m_Attachments;
        m_TypeofAttachments = other.m_TypeofAttachments;
        m_Subpasses         = other.m_Subpasses;
        m_SubPassDependencies = other.m_SubPassDependencies;
    }
}

//--------------------------------------------------------------------
void VulkanRenderPass::Cleanup()
{
    if (!m_pVulkanDev)
    {
        MASSERT(m_RenderPass == VK_NULL_HANDLE);
        return;
    }

    if (m_RenderPass)
    {
        m_pVulkanDev->DestroyRenderPass(m_RenderPass);
        m_RenderPass = VK_NULL_HANDLE;
    }
}

//--------------------------------------------------------------------
void VulkanRenderPass::PushAttachmentDescription
(
    VkUtil::AttachmentType attType,
    VkAttachmentDescription *pAttachmentDesc
)
{
    MASSERT(pAttachmentDesc);
    m_Attachments.push_back(*pAttachmentDesc);
    m_TypeofAttachments.push_back(attType);
}

//--------------------------------------------------------------------
void VulkanRenderPass::PushSubpassDescription(VkSubpassDescription *pSubpassDesc)
{
    MASSERT(pSubpassDesc);
    m_Subpasses.push_back(*pSubpassDesc);
}

//--------------------------------------------------------------------
void VulkanRenderPass::PushSubpassDependency(VkSubpassDependency *pSubpassDependency)
{
    MASSERT(pSubpassDependency);
    m_SubPassDependencies.push_back(*pSubpassDependency);
}

//--------------------------------------------------------------------
VkResult VulkanRenderPass::CreateRenderPass()
{
    MASSERT(m_RenderPass == VK_NULL_HANDLE);

    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.pNext = nullptr;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(m_Attachments.size());
    renderPassInfo.pAttachments = m_Attachments.data();
    renderPassInfo.subpassCount = static_cast<uint32_t>(m_Subpasses.size());
    renderPassInfo.pSubpasses = m_Subpasses.data();
    renderPassInfo.dependencyCount = static_cast<uint32_t>(m_SubPassDependencies.size());
    renderPassInfo.pDependencies = m_SubPassDependencies.data();
    return m_pVulkanDev->CreateRenderPass(&renderPassInfo, nullptr, &m_RenderPass);
}
