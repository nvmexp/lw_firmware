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
#ifndef VK_SWAPCHAIN_H
#define VK_SWAPCHAIN_H

#include "vkmain.h"
#include "vkimage.h"
#include "vkcmdbuffer.h"

class VulkanInstance;
class VulkanDevice;

//--------------------------------------------------------------------
//TODO: Add support for double-buffering
class SwapChain
{
public:
    enum ImageMode
    {
        MULTI_IMAGE_MODE,
        SINGLE_IMAGE_MODE
    };

    SwapChain
    (
        VulkanInstance* pVkInst,
        VulkanDevice* pVulkanDev
    );
    SwapChain(const SwapChain& swapChain) = delete;
    SwapChain& operator=(const SwapChain& swapChain) = delete;
    virtual ~SwapChain();

    virtual UINT32 GetNextSwapChainImage(VkSemaphore presentSemaphore) = 0;

    virtual VkResult PresentImage
    (
        UINT32      imageIndex,
        VkSemaphore waitOnSemaphore,
        VkSemaphore signalSemaphore,
        bool        present
    ) const = 0;

    void         Cleanup();
    VkImage      GetImage(UINT32 i) const;
    VkImageView  GetImageView(UINT32 i) const;
    size_t       GetNumImages() const { return m_SwapChainImage.size(); }

    VkResult     SetupCmdBuffer
    (
        UINT32 queueFamilyIdx,
        UINT32 queueIdx
    );

    //! Before initializing the swapchain, sets the preferred color formats.
    //! For SwapChainMods, this forces the swapchain to use one of these formats
    //! and Init will fail if none of these are possible.
    //! For SwapChainKHR, these are preferred formats if the swapchain supports them,
    //! otherwise it will fall back to whatever is supported.
    void SetPreferredFormats(vector<VkFormat> formats) { m_PreferredFormats = move(formats); }

    GET_PROP(SurfaceFormat,     VkFormat);
    GET_PROP(SwapChainExtent,   VkExtent2D);
    SETGET_PROP(ImageHeight,    UINT32);
    SETGET_PROP(ImageWidth,     UINT32);
    SETGET_PROP(ImageMode,      UINT32);
    SETGET_PROP(ImageMemoryType, UINT32);
    SETGET_PROP(ImageUsage,     UINT32);
    VulkanImage * GetSwapChainImage(UINT32 i)
    {
        MASSERT(i < m_SwapChainImage.size());
        return m_SwapChainImage[i].get();
    }

    VulkanInstance             *m_pVulkanInst;
    VulkanDevice               *m_pVulkanDev;

    VkFormat                   m_SurfaceFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D                 m_SwapChainExtent = {};

    VulkanCmdPool              m_SwapChainCmdPool;
    VulkanCmdBuffer            m_SwapChainCmdBuffer;

protected:
    vector<VkFormat>           m_PreferredFormats;
    vector<unique_ptr<VulkanImage>> m_SwapChainImage;
    UINT32                     m_ImageUsage = 0;

private:
    UINT32                     m_ImageWidth = 800;
    UINT32                     m_ImageHeight = 600;
    UINT32                     m_ImageMode = ImageMode::MULTI_IMAGE_MODE;
    // The default is to store the swapchain images in FB, however in some cases like when
    // there is no framebuffer (aka -fb_broken) we need to change that location at runtime.
    UINT32                     m_ImageMemoryType = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
};

//--------------------------------------------------------------------
class SwapChainMods : public SwapChain
{
public:

    SwapChainMods
    (
        VulkanInstance* pVkInst,
        VulkanDevice* pVulkanDev
    );
    ~SwapChainMods(){}

    VkResult    CreateSwapChainImageAndViews();
    VkResult    Init
    (
        UINT32 width,
        UINT32 height,
        ImageMode imageMode,
        UINT32 queueFamilyIdx,
        UINT32 queueIdx
    );

    UINT32      GetNextSwapChainImage(VkSemaphore presentSemaphore) override;

    VkResult    PresentImage
    (
        UINT32 imageIndex,
        VkSemaphore  waitOnSemaphore,
        VkSemaphore signalSemaphore,
        bool present
    ) const override;

private:
    UINT32 m_LwrrentImageIdx = 0;
};

#ifdef VULKAN_STANDALONE_KHR
//------------------------------------------------------------------------------------------------
// The difference between a SwapChainMods and the SwapChainKHR is that the KHR verion
// automatically manages the creation and memory allocation of the VkImage objects.
// However other parts of MODS ie. VulkanGoldenSurfaces need access to the VkImage object
// information. So we create VulkanImage object's but specify that the internal VkImage is owned
// by some other KHR process.
class SwapChainKHR : public SwapChain
{
public:
    SwapChainKHR
    (
        VulkanInstance* pVulkanInst,
        VulkanDevice* pVulkanDev
    );

    ~SwapChainKHR();

    void         Cleanup();
    UINT32       GetNextSwapChainImage(VkSemaphore presentSemaphore) override;
    VkResult     PresentImage
    (
        UINT32 imageIndex,
        VkSemaphore  waitOnSemaphore,
        VkSemaphore signalSemaphore,
        bool present
    ) const override;

    VkResult     Init
    (
        HINSTANCE hinstance,
        HWND hWindow,
        ImageMode imageMode,
        VkFence fence,
        UINT32 queueFamilyIdx,
        UINT32 queueIdx
    );

    GET_PROP(SwapChainKHR,     VkSwapchainKHR);

private:
    VkSurfaceKHR               m_Surface = VK_NULL_HANDLE;
    VkSwapchainKHR             m_SwapChainKHR = VK_NULL_HANDLE;
    UINT32                     m_PresentQueueIdx = _UINT32_MAX;

    VkResult     CreateSurface(HINSTANCE hinstance, HWND hWindow);
    VkResult     SetSurfaceFormat();
    VkResult     CreateSwapChain();
    VkResult     GetSwapChainImages();
    VkResult     CreateSwapChainImageViews();
};
#endif

#endif
