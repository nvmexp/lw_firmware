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

#include "swapchain.h"
#include "vkinstance.h"
#include "vkutil.h"

//--------------------------------------------------------------------
SwapChain::SwapChain(VulkanInstance* pVkInst, VulkanDevice* pVulkanDev)
    :m_pVulkanInst(pVkInst)
    ,m_pVulkanDev(pVulkanDev)
    ,m_SwapChainCmdPool(pVulkanDev)
    ,m_SwapChainCmdBuffer(pVulkanDev)
{}

//--------------------------------------------------------------------
SwapChain::~SwapChain()
{
    Cleanup();
}

//--------------------------------------------------------------------
VkImage SwapChain::GetImage(UINT32 i) const
{
    MASSERT(i < m_SwapChainImage.size());
    return m_SwapChainImage[i]->GetImage();
}

//--------------------------------------------------------------------
VkImageView SwapChain::GetImageView(UINT32 i) const
{
    MASSERT(i < m_SwapChainImage.size());
    return m_SwapChainImage[i]->GetImageView();
}

//--------------------------------------------------------------------
VkResult SwapChain::SetupCmdBuffer
(
    UINT32 familyQueueIdx,
    UINT32 deviceQueueIdx
)
{
    VkResult res = VK_SUCCESS;
    CHECK_VK_RESULT(m_SwapChainCmdPool.InitCmdPool(familyQueueIdx, deviceQueueIdx));
    CHECK_VK_RESULT(m_SwapChainCmdBuffer.AllocateCmdBuffer(&m_SwapChainCmdPool));
    return res;
}

//--------------------------------------------------------------------
void SwapChain::Cleanup()
{
    for (UINT32 i=0; i<m_SwapChainImage.size(); i++)
    {
        m_SwapChainImage[i]->Cleanup();
    }
    m_SwapChainImage.clear();

    m_SwapChainCmdBuffer.FreeCmdBuffer();
    m_SwapChainCmdPool.DestroyCmdPool();
}

//--------------------------------------------------------------------
SwapChainMods::SwapChainMods(VulkanInstance* pVkInst, VulkanDevice* pVulkanDev)
    :SwapChain(pVkInst, pVulkanDev)
{
}

UINT32 SwapChainMods::GetNextSwapChainImage(VkSemaphore presentSemaphore)
{
    m_LwrrentImageIdx = (m_LwrrentImageIdx+1) % m_SwapChainImage.size();
    MASSERT(m_LwrrentImageIdx < m_SwapChainImage.size());
    return m_LwrrentImageIdx;
}

//--------------------------------------------------------------------
VkResult SwapChainMods::CreateSwapChainImageAndViews()
{
    VkResult res = VK_SUCCESS;
    const bool bOwnedByKHR = false;
    const UINT32 numImages = (GetImageMode() == MULTI_IMAGE_MODE) ? 3 : 1;
    // For goldens/pngs
    m_ImageUsage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    m_SwapChainImage.reserve(numImages);
    for (UINT32 i = 0; i < numImages; i++)
    {
        m_SwapChainImage.emplace_back(make_unique<VulkanImage>(m_pVulkanDev, bOwnedByKHR));
    }

    const UINT32 maxImgDim2D = m_pVulkanDev->GetPhysicalDevice()->GetLimits().maxImageDimension2D;
    if (GetImageWidth() > maxImgDim2D || GetImageHeight() > maxImgDim2D)
    {
        Printf(Tee::PriError,
            "Vulkan swapchain image width/height is larger than allowed (%u)\n",
            maxImgDim2D);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    UINT32 imageIdx = 0;
    for (const auto& swapChainImage : m_SwapChainImage)
    {
        if (m_PreferredFormats.empty())
        {
            swapChainImage->PickColorFormat();
        }
        else
        {
            swapChainImage->PickFormat(m_PreferredFormats.data(),
                                       static_cast<UINT32>(m_PreferredFormats.size()),
                                       VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT);
        }
        Printf(Tee::PriLow, "Swapchain image %u: %ux%u %s\n",
               imageIdx++,
               GetImageWidth(),
               GetImageHeight(),
               ColorUtils::FormatToString(VkUtil::ColorUtilsFormat(swapChainImage->GetFormat())).c_str());

        swapChainImage->SetImageProperties(
            GetImageWidth(), GetImageHeight(), 1, VulkanImage::ImageType::COLOR);

        CHECK_VK_RESULT(swapChainImage->CreateImage(
            VkImageCreateFlags(0),
            m_ImageUsage,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_TILING_OPTIMAL,
            VK_SAMPLE_COUNT_1_BIT,
            "ColorImage"));
        CHECK_VK_RESULT(swapChainImage->AllocateAndBindMemory(GetImageMemoryType()));
        CHECK_VK_RESULT(swapChainImage->SetImageLayout(&m_SwapChainCmdBuffer
            ,VK_IMAGE_ASPECT_COLOR_BIT
            ,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL           //new layout
            ,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT               //new access
            ,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT));   //new stage
        CHECK_VK_RESULT(swapChainImage->CreateImageView());
    }
    m_SurfaceFormat = m_SwapChainImage[0]->GetFormat();
    m_SwapChainExtent.width = GetImageWidth();
    m_SwapChainExtent.height = GetImageHeight();

    return res;
}

//--------------------------------------------------------------------
VkResult SwapChainMods::Init
(
    UINT32 width,
    UINT32 height,
    ImageMode imageMode,
    UINT32 queueFamilyIdx,
    UINT32 queueIdx
)
{
    SetImageWidth(width);
    SetImageHeight(height);
    SetImageMode(imageMode);
    MASSERT(m_pVulkanInst);
    VkResult res = VK_SUCCESS;
    CHECK_VK_RESULT(SwapChain::SetupCmdBuffer(queueFamilyIdx, queueIdx));
    CHECK_VK_RESULT(CreateSwapChainImageAndViews());
    return res;
}

//--------------------------------------------------------------------
VkResult SwapChainMods::PresentImage
(
    UINT32 imageIndex,
    VkSemaphore waitOnSemaphore,
    VkSemaphore signalSemaphore,
    bool present
) const
{
#ifndef VULKAN_STANDALONE
    if (present)
    {
        if (waitOnSemaphore)
        {
            glWaitVkSemaphoreLW(UINT64(waitOnSemaphore));
        }
        glDrawVkImageLW(UINT64(GetImage(m_LwrrentImageIdx)), 0, 0, 0,
            static_cast<float>(GetSwapChainExtent().width),
            static_cast<float>(GetSwapChainExtent().height),
            0, 0, 1, 1, 0);
    }
    if (signalSemaphore)
    {
        glSignalVkSemaphoreLW(UINT64(signalSemaphore));
    }
#endif
    return VK_SUCCESS;
}

#ifdef VULKAN_STANDALONE_KHR
//--------------------------------------------------------------------
SwapChainKHR::SwapChainKHR(VulkanInstance* pVkInst, VulkanDevice *pVulkanDev)
    :SwapChain(pVkInst, pVulkanDev)
{
}

//--------------------------------------------------------------------
SwapChainKHR::~SwapChainKHR()
{
    Cleanup();
}
//--------------------------------------------------------------------
void SwapChainKHR::Cleanup()
{
    SwapChain::Cleanup();
    if (m_SwapChainKHR)
    {
        m_pVulkanDev->DestroySwapchainKHR(m_SwapChainKHR);
        m_SwapChainKHR = VK_NULL_HANDLE;
    }

    if (m_Surface)
    {
        m_pVulkanInst->DestroySurfaceKHR(m_pVulkanInst->GetVkInstance(), m_Surface, nullptr);
        m_Surface = VK_NULL_HANDLE;
    }

}
//--------------------------------------------------------------------
UINT32 SwapChainKHR::GetNextSwapChainImage(VkSemaphore presentSemaphore)
{
    UINT32 lwrrentBuffer = 0;
    m_pVulkanDev->AcquireNextImageKHR(m_SwapChainKHR, UINT64_MAX,
        presentSemaphore, VK_NULL_HANDLE, &lwrrentBuffer);
    return lwrrentBuffer;
}

//--------------------------------------------------------------------
VkResult SwapChainKHR::PresentImage
(
    UINT32 imageIndex,
    VkSemaphore waitOnSemaphore,
    VkSemaphore signalSemaphore,
    bool present
) const
{
    VkPresentInfoKHR presentInfo;
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &m_SwapChainKHR;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.pWaitSemaphores = &waitOnSemaphore;
    presentInfo.waitSemaphoreCount = (waitOnSemaphore != VK_NULL_HANDLE) ? 1 : 0;
    presentInfo.pResults = nullptr;

    VkResult res = m_pVulkanDev->QueuePresentKHR(m_pVulkanDev->GetGraphicsQueue(
        m_PresentQueueIdx), &presentInfo);

    return res;
}

//--------------------------------------------------------------------
VkResult SwapChainKHR::CreateSurface(HINSTANCE hinstance, HWND hWindow)
{
    VkResult res = VK_SUCCESS;

#ifdef _WIN32
    VkWin32SurfaceCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
    createInfo.hinstance = hinstance;
    createInfo.hwnd      = hWindow;

    CHECK_VK_RESULT(m_pVulkanInst->CreateWin32SurfaceKHR(m_pVulkanInst->GetVkInstance(),
                                                         &createInfo,
                                                         nullptr,
                                                         &m_Surface));
#elif defined(__linux__)
    VkXcbSurfaceCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR };
    createInfo.connection = hinstance;
    createInfo.window     = hWindow;

    CHECK_VK_RESULT(m_pVulkanInst->CreateXcbSurfaceKHR(m_pVulkanInst->GetVkInstance(),
                                                       &createInfo,
                                                       nullptr,
                                                       &m_Surface));

    Printf(Tee::PriLow, "Created Vulkan surface for window 0x%x\n", hWindow);
#endif

    //find queue that supports presenting on this surface
    UINT32 queueCount;
    m_pVulkanInst->GetPhysicalDeviceQueueFamilyProperties(
        m_pVulkanDev->GetPhysicalDevice()->GetVkPhysicalDevice(),
        &queueCount, nullptr);
    MASSERT(queueCount >= 1);
    bool foundQueueForBothPresentAndGraphics = false;
    for (UINT32 i = 0; i < queueCount; i++)
    {
        VkBool32 supported = false;
        CHECK_VK_RESULT(m_pVulkanInst->GetPhysicalDeviceSurfaceSupportKHR(
            m_pVulkanDev->GetPhysicalDevice()->GetVkPhysicalDevice(),
            i, m_Surface, &supported));
        {
            // use the first queue that supports graphics
            if (supported &&
                i == m_pVulkanDev->GetPhysicalDevice()->GetGraphicsQueueFamilyIdx())
            {
                foundQueueForBothPresentAndGraphics = true;
                break;
            }
        }
    }
    if (!foundQueueForBothPresentAndGraphics)
    {
        return VK_ERROR_FORMAT_NOT_SUPPORTED;
    }

    return res;
}

//--------------------------------------------------------------------
VkResult SwapChainKHR::SetSurfaceFormat()
{
    VkResult res = VK_SUCCESS;
    // Get the list of formats that are supported:
    UINT32 formatCount;
    CHECK_VK_RESULT(m_pVulkanInst->GetPhysicalDeviceSurfaceFormatsKHR(
        m_pVulkanDev->GetPhysicalDevice()->GetVkPhysicalDevice(),
        m_Surface, &formatCount, nullptr));
    vector<VkSurfaceFormatKHR> surfFormats(formatCount);
    CHECK_VK_RESULT(m_pVulkanInst->GetPhysicalDeviceSurfaceFormatsKHR(
        m_pVulkanDev->GetPhysicalDevice()->GetVkPhysicalDevice(), m_Surface, &formatCount,
        surfFormats.data()));

    if (formatCount == 1 && surfFormats[0].format == VK_FORMAT_UNDEFINED)
    {
        m_SurfaceFormat = VK_FORMAT_B8G8R8A8_UNORM;
    }
    else
    {
        MASSERT(formatCount >= 1);
        m_SurfaceFormat = surfFormats[0].format;

        for (VkFormat requestedFormat : m_PreferredFormats)
        {
            if (std::count_if(surfFormats.begin(), surfFormats.end(),
                              [requestedFormat](const VkSurfaceFormatKHR& possibleFormat)
                              {
                                  return requestedFormat == possibleFormat.format;
                              }
                             ) > 0)
            {
                m_SurfaceFormat = requestedFormat;
                break;
            }
        }
    }

    Printf(Tee::PriLow, "Selected swapchain image format %s\n",
           ColorUtils::FormatToString(VkUtil::ColorUtilsFormat(m_SurfaceFormat)).c_str());
    return res;
}

//--------------------------------------------------------------------
VkResult SwapChainKHR::CreateSwapChain()
{
    VkResult res = VK_SUCCESS;
    VkSurfaceCapabilitiesKHR surfCapabilities;
    VkPhysicalDevice vkPhysDev = m_pVulkanDev->GetPhysicalDevice()->GetVkPhysicalDevice();
    CHECK_VK_RESULT(m_pVulkanInst->GetPhysicalDeviceSurfaceCapabilitiesKHR(
        vkPhysDev, m_Surface, &surfCapabilities));
    VkSurfaceTransformFlagBitsKHR preTransform =
        surfCapabilities.supportedTransforms &
            VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR ?
        VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR : surfCapabilities.lwrrentTransform;

    UINT32 presentModeCount;
    CHECK_VK_RESULT(m_pVulkanInst->GetPhysicalDeviceSurfacePresentModesKHR(
        vkPhysDev, m_Surface, &presentModeCount, nullptr));
    vector<VkPresentModeKHR> presentModes(presentModeCount);
    CHECK_VK_RESULT(m_pVulkanInst->GetPhysicalDeviceSurfacePresentModesKHR(
        vkPhysDev, m_Surface, &presentModeCount, presentModes.data()));

    const auto FindMode = [&presentModes](VkPresentModeKHR modeToFind,
                                          VkPresentModeKHR alternativeMode)
                          -> VkPresentModeKHR
    {
        const bool found = find(presentModes.begin(), presentModes.end(), modeToFind)
                           != presentModes.end();
        return found ? modeToFind : alternativeMode;
    };

    // best case select mailbox as it is low latency without tear
    // default to FIFO mode which is always available, per the spec.
    VkPresentModeKHR swapchainPresentMode = FindMode(VK_PRESENT_MODE_MAILBOX_KHR,
                                                     VK_PRESENT_MODE_FIFO_KHR);

    // the fastest way to present, but will have tearing
    swapchainPresentMode = FindMode(VK_PRESENT_MODE_IMMEDIATE_KHR, swapchainPresentMode);

    if (surfCapabilities.lwrrentExtent.width == 0xFFFFFFFF)
    {
        m_SwapChainExtent.width = GetImageWidth();
        m_SwapChainExtent.height = GetImageHeight();
    }
    else
    {
        m_SwapChainExtent = surfCapabilities.lwrrentExtent;
    }

    VkSwapchainCreateInfoKHR swap_chain_params = {};
    swap_chain_params.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swap_chain_params.pNext = nullptr;
    swap_chain_params.surface = m_Surface;
    swap_chain_params.minImageCount = surfCapabilities.minImageCount;
    if (GetImageMode() == MULTI_IMAGE_MODE)
    {
        swap_chain_params.minImageCount += NUM_EXTRA_PRESENTABLE_IMAGES;
    }
    swap_chain_params.imageFormat = m_SurfaceFormat;
    swap_chain_params.imageExtent.width = m_SwapChainExtent.width;
    swap_chain_params.imageExtent.height = m_SwapChainExtent.height;
    swap_chain_params.preTransform = preTransform;
    swap_chain_params.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swap_chain_params.imageArrayLayers = 1;
    swap_chain_params.presentMode = swapchainPresentMode;
    swap_chain_params.oldSwapchain = VK_NULL_HANDLE;
    swap_chain_params.clipped = true;
    swap_chain_params.imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;

    //hmm.. why not include VK_IMAGE_USAGE_TRANSFER_DST_BIT below
    m_ImageUsage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                    VK_IMAGE_USAGE_TRANSFER_SRC_BIT | // For goldens/pngs
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT;  // For copy during present

    swap_chain_params.imageUsage = m_ImageUsage;

    swap_chain_params.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swap_chain_params.queueFamilyIndexCount = 0;
    swap_chain_params.pQueueFamilyIndices = nullptr;
    res = m_pVulkanDev->CreateSwapchainKHR(&swap_chain_params, nullptr, &m_SwapChainKHR);
    return res;
}

//--------------------------------------------------------------------
VkResult SwapChainKHR::GetSwapChainImages()
{
    VkResult res = VK_SUCCESS;

    UINT32 numImages = 0;
    vector<VkImage> swapChainImages;

    // Images are created in the background when vkCreateSwapchainKHR() is called, so...
    // Do not create images if we have created a swap chain, just extract the images from it
    CHECK_VK_RESULT(m_pVulkanDev->GetSwapchainImagesKHR(m_SwapChainKHR, &numImages, nullptr));
    swapChainImages.resize(numImages, VK_NULL_HANDLE);
    CHECK_VK_RESULT(m_pVulkanDev->GetSwapchainImagesKHR(m_SwapChainKHR,
        &numImages, swapChainImages.data()));

    const bool bOwnedByKHR = true;
    char * imageNames[] =
    {
        "KHRImage0", "KHRImage1", "KHRImage2",
        "KHRImage3", "KHRImage4", "KHRImage5"
    };
    for (UINT32 i = 0; i < numImages; i++)
    {
        m_SwapChainImage.emplace_back(make_unique<VulkanImage>(m_pVulkanDev, bOwnedByKHR));
        m_SwapChainImage[i]->SetFormat(m_SurfaceFormat);
        m_SwapChainImage[i]->SetImageProperties(m_SwapChainExtent.width,
                                               m_SwapChainExtent.height,
                                               1,
                                               VulkanImage::COLOR);

        m_SwapChainImage[i]->SetCreateImageProperties(
            swapChainImages[i],
            VK_IMAGE_TILING_OPTIMAL,    // this is a guess
            VK_IMAGE_LAYOUT_UNDEFINED,
            m_SwapChainExtent.width * 4, //pitch = width * 4(bytes per texel)
            GetImageMemoryType(),
            imageNames[i]);
    }
    return res;
}

//--------------------------------------------------------------------
VkResult SwapChainKHR::CreateSwapChainImageViews()
{
    VkResult res = VK_SUCCESS;
    for (UINT32 i = 0; i < m_SwapChainImage.size(); i++)
    {
        CHECK_VK_RESULT(m_SwapChainImage[i]->SetImageLayout(&m_SwapChainCmdBuffer,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_ACCESS_MEMORY_READ_BIT,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT));
        CHECK_VK_RESULT(m_SwapChainImage[i]->CreateImageView());
    }

    return res;
}

//-------------------------------------------------------------------------------------------------
VkResult SwapChainKHR::Init
(
    HINSTANCE hinstance,
    HWND hWindow,
    ImageMode imageMode,
    VkFence fence,
    UINT32 queueFamilyIdx,
    UINT32 queueIdx         //index into the specified queueFamily[]
)
{
    MASSERT(m_pVulkanInst);
    VkResult res = VK_SUCCESS;

    SetImageMode(imageMode);
    CHECK_VK_RESULT(SwapChain::SetupCmdBuffer(queueFamilyIdx, queueIdx));
    CHECK_VK_RESULT(CreateSurface(hinstance, hWindow));
    CHECK_VK_RESULT(SetSurfaceFormat());
    CHECK_VK_RESULT(CreateSwapChain());
    CHECK_VK_RESULT(GetSwapChainImages());
    CHECK_VK_RESULT(CreateSwapChainImageViews());

    m_PresentQueueIdx = queueIdx;

    return res;
}

#endif
