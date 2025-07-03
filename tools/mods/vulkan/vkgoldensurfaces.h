/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2017-2020 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

#pragma once

#ifndef VULKAN_STANDALONE
#include "core/include/golden.h"
#endif

#include "vkimage.h"
#include "vkbuffer.h"
#include <memory>

class VulkanInstance;

class VulkanGoldenSurfaces : public GoldenSurfaces
{
public:
    explicit VulkanGoldenSurfaces(VulkanInstance* pVkInst);
    virtual ~VulkanGoldenSurfaces();
    void Cleanup();
    void SetCmdBuffers
    (
        UINT32 surfNum,
        VulkanCmdBuffer *pCmdBuffer,
        VulkanCmdBuffer *pTransCmdBuffer
    );
    void SelectWaitSemaphore(VkSemaphore waitSemaphore);

    VkResult AddSurface
    (
        const string &name
        ,VulkanImage * srcImage
        ,VulkanCmdBuffer *pCmdBuffer
        ,VulkanCmdBuffer *pTransCmdBuffer = nullptr
    );
    int NumSurfaces() const override;
    const string & Name(int surfNum) const override;
    RC CheckAndReportDmaErrors(UINT32 subdevNum) override;
    void * GetCachedAddress
    (
        int surfNum,
        Goldelwalues::BufferFetchHint bufFetchHint,
        UINT32 subdevNum,
        vector<UINT08> *surfDumpBuffer
    ) override;
    void Ilwalidate() override;
    INT32 Pitch(int surfNum) const override;
    UINT32 Width(int surfNum) const override;
    UINT32 Height(int surfNum) const override;
    ColorUtils::Format Format(int surfNum) const override;
    UINT32 Display(int surfNum) const override;
    RC GetPitchAlignRequirement(UINT32 *pitch) override;

private:
    struct VulkanGoldenSurface
    {
        string name;
        VulkanImage* srcImage;
        VulkanCmdBuffer *pCmdBuffer = nullptr;
        VulkanCmdBuffer *pTransCmdBuffer = nullptr;

        unique_ptr<VulkanBuffer> transferBuffer;
        bool transferBufferContentValid = false;
        void *pMappedTransferBufferData = nullptr;
        ~VulkanGoldenSurface() {};
        VulkanGoldenSurface() = default;
        VulkanGoldenSurface(const VulkanGoldenSurface&) = delete;
        VulkanGoldenSurface& operator=(const VulkanGoldenSurface&) = delete;
        VulkanGoldenSurface(VulkanGoldenSurface &&) = default;
    };

    vector<VulkanGoldenSurface> m_VulkanGoldenSurfaces;

    VkSemaphore m_WaitSemaphore = VK_NULL_HANDLE;
    VulkanInstance* m_pVulkanInst;

    VkResult    CopyImageToBuffer(int surfNum, VkImageAspectFlags aspectMask);

};
