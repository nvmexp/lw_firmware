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
#ifndef VK_RENDERPASS_H
#define VK_RENDERPASS_H

#include "vkmain.h"
#include "vkutil.h"

class VulkanDevice;

//--------------------------------------------------------------------
class VulkanRenderPass
{
public:
    VulkanRenderPass() = default;
    explicit VulkanRenderPass(VulkanDevice* pVulkanDev);
    ~VulkanRenderPass();
    void Cleanup();
    VulkanRenderPass(VulkanRenderPass&& rp) noexcept;
    VulkanRenderPass& operator=(VulkanRenderPass&& rp);
    VulkanRenderPass(const VulkanRenderPass& rp) = delete;
    VulkanRenderPass& operator=(const VulkanRenderPass& rp) = delete;

    void PushAttachmentDescription
    (
        VkUtil::AttachmentType atttype,
        VkAttachmentDescription *pAttachmentDesc
    );
    void PushSubpassDescription(VkSubpassDescription *pSubpassDesc);
    void PushSubpassDependency(VkSubpassDependency *pSubpassDependency);

    VkResult CreateRenderPass();

    VkUtil::AttachmentType GetAttachmentType(UINT32 i) const
    {
        MASSERT(i < m_TypeofAttachments.size());
        return m_TypeofAttachments[i];
    }

    UINT32 GetNumAttachments()
    {
        return static_cast<UINT32>(m_Attachments.size());
    }
    UINT32 GetNumSubpasses()
    {
        return static_cast<UINT32>(m_Subpasses.size());
    }
    UINT32 GetNumSubpassDependencies()
    {
        return static_cast<UINT32>(m_SubPassDependencies.size());
    }
    GET_PROP(RenderPass, VkRenderPass);

private:
    VulkanDevice                    *m_pVulkanDev = nullptr;

    VkRenderPass                     m_RenderPass = VK_NULL_HANDLE;

    vector<VkAttachmentDescription>  m_Attachments;
    vector<VkUtil::AttachmentType>   m_TypeofAttachments;

    vector<VkSubpassDescription>     m_Subpasses;

    vector<VkSubpassDependency>      m_SubPassDependencies;

    void TakeResources(VulkanRenderPass&& other);
};

#endif
