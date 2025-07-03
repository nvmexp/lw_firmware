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

#include "vkdev.h"
#include "vkimage.h"
#include "vkcmdbuffer.h"
#include "vkutil.h"
#include "vkinstance.h"
#include <algorithm>                 // for std::max
#ifndef VULKAN_STANDALONE
#include "class/cl0040.h" // LW01_MEMORY_LOCAL_USER
#include "core/include/platform.h"
#include "gpu/utility/gpuutils.h"
#endif

//--------------------------------------------------------------------
VulkanImage::VulkanImage(VulkanDevice* pVulkanDev, bool bOwnedByKHR) :
    m_pVulkanInst(pVulkanDev->GetVulkanInstance())
   ,m_pVulkanDev(pVulkanDev)
   ,m_bOwnedByKHR(bOwnedByKHR)
{}

//--------------------------------------------------------------------
VulkanImage::~VulkanImage()
{
    Cleanup();
}

//--------------------------------------------------------------------
void VulkanImage::PickFormat(const VkFormat* formats, UINT32 count, VkFormatFeatureFlags flags)
{
    VkFormatProperties formatProps = {};

    const VkPhysicalDevice vkPhysDev = m_pVulkanDev->GetPhysicalDevice()->GetVkPhysicalDevice();

    //try optimal, then linear
    for (UINT32 i = 0; i < count; i++)
    {
        m_pVulkanInst->GetPhysicalDeviceFormatProperties(vkPhysDev, formats[i], &formatProps);
        if ((formatProps.optimalTilingFeatures & flags) == flags)
        {
            m_Format = formats[i];
            return;
        }
    }
    for (UINT32 i = 0; i < count; i++)
    {
        m_pVulkanInst->GetPhysicalDeviceFormatProperties(vkPhysDev, formats[i], &formatProps);
        if ((formatProps.linearTilingFeatures & flags) == flags)
        {
            m_Format = formats[i];
            return;
        }
    }
    //force the first format
    m_Format = formats[0];
}

//--------------------------------------------------------------------
void VulkanImage::PickColorFormat()
{
    // Populate some formats
    VkFormat formats[] = {
        VK_FORMAT_B8G8R8A8_UNORM,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_R8G8B8_UNORM,
        VK_FORMAT_R32G32B32A32_SFLOAT
    };

    PickFormat(formats, sizeof(formats) / sizeof(formats[0]),
        VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT);
}

//--------------------------------------------------------------------
void VulkanImage::PickDepthFormat()
{
    VkFormat depthFormats[] = {
        VK_FORMAT_D16_UNORM,
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D16_UNORM_S8_UINT
    };

    PickFormat(depthFormats, sizeof(depthFormats) / sizeof(depthFormats[0]),
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

//--------------------------------------------------------------------
#ifndef VULKAN_STANDALONE
RC VulkanImage::DuplicateMemoryHandle(GpuDevice *pGpuDevice)
{
    RC rc;

    if (m_DupMemHandle != 0)
        return rc;

    LwRm::Handle rmClientHandle = 0;
    LwRm::Handle rmObjectHandle = 0;
    CHECK_VK_TO_RC(vkGetMemoryRmInfoLWX2(m_DeviceMemory, &rmClientHandle,
        &rmObjectHandle, &m_DispContextDmaOffset));

    memset(&m_LwLayoutLWX, 0, sizeof(m_LwLayoutLWX));

    // Ugh. this API is not implemented in the validation layers and generates a SEGFAULT when
    // handle wrapping is enabled.
    CHECK_VK_TO_RC(vkGetImageLwLayoutLWX(m_Image, &m_LwLayoutLWX));

    CHECK_RC(LwRmPtr()->DuplicateMemoryHandle(rmObjectHandle, rmClientHandle,
                                              &m_DupMemHandle, pGpuDevice));

    return rc;
}

RC VulkanImage::PopulateDisplayImageDescription
(
    GpuDevice *pGpuDevice,
    GpuUtility::DisplayImageDescription *desc
)
{
    RC rc;

    UINT32 widthMultiplier;
    UINT32 heightMultiplier;
    switch (m_SampleCount)
    {
    case VK_SAMPLE_COUNT_1_BIT:
        widthMultiplier = 1;
        heightMultiplier = 1;
        break;
    case VK_SAMPLE_COUNT_2_BIT:
        widthMultiplier = 2;
        heightMultiplier = 1;
        break;
    case VK_SAMPLE_COUNT_4_BIT:
        widthMultiplier = 2;
        heightMultiplier = 2;
        break;
    case VK_SAMPLE_COUNT_8_BIT:
        widthMultiplier = 4;
        heightMultiplier = 2;
        break;
    case VK_SAMPLE_COUNT_16_BIT:
        widthMultiplier = 4;
        heightMultiplier = 4;
        break;
    case VK_SAMPLE_COUNT_32_BIT:
        widthMultiplier = 8;
        heightMultiplier = 4;
        break;
    case VK_SAMPLE_COUNT_64_BIT:
        widthMultiplier = 8;
        heightMultiplier = 8;
        break;
    default:
        Printf(Tee::PriError, VkMods::GetTeeModuleCode(),
            "PopulateDisplayImageDescription doesn't support sample count = %d\n",
            m_SampleCount);
        return RC::MODE_NOT_SUPPORTED;
    }

    if (desc == nullptr)
        return RC::SOFTWARE_ERROR;

    desc->Reset();

    CHECK_RC(DuplicateMemoryHandle(pGpuDevice));

    if (m_DispContextDma == 0 && !Platform::UsesLwgpuDriver())
    {
        CHECK_RC(LwRmPtr()->AllocContextDma(&m_DispContextDma,
            DRF_DEF(OS03, _FLAGS, _PTE_KIND_BL_OVERRIDE, _TRUE) |
                DRF_DEF(OS03, _FLAGS, _CACHE_SNOOP, _DISABLE),
            m_DupMemHandle, 0, m_DeviceMemorySize + m_DispContextDmaOffset - 1));
    }

    desc->CtxDMAHandle = Platform::UsesLwgpuDriver() ? m_DupMemHandle : m_DispContextDma;
    desc->Offset = m_DispContextDmaOffset;
    desc->Width = widthMultiplier * m_Width;
    desc->Height = heightMultiplier * m_Height;
    desc->AASamples = 1;
    desc->Layout = Surface2D::BlockLinear;
    desc->LogBlockHeight = m_LwLayoutLWX.log2GobsPerBlockY;
    desc->NumBlocksWidth = (desc->Width - 1) / 16 + 1;
    desc->ColorFormat = ColorUtils::A8R8G8B8;

    return rc;
}

RC VulkanImage::GetDupMemHandle(GpuDevice *pGpuDevice, UINT32 *dupMemHandle)
{
    RC rc;

    CHECK_RC(DuplicateMemoryHandle(pGpuDevice));

    *dupMemHandle = m_DupMemHandle;

    return rc;
}
#endif

//--------------------------------------------------------------------
void VulkanImage::SetImageProperties
(
    const UINT32 width,
    const UINT32 height,
    const UINT08 mipLevels,
    const ImageType imageType
)
{
    m_Width = width;
    m_Height = height;
    m_ImageType = imageType;
    m_MipLevels = mipLevels;

    switch (m_ImageType)
    {
    case COLOR:
        m_ImageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
        break;
    case DEPTH:
        m_ImageAspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
        break;
    case STENCIL:
        m_ImageAspectFlags = VK_IMAGE_ASPECT_STENCIL_BIT;
        break;
    case DEPTH_STENCIL:
        m_ImageAspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        break;
    default:
        MASSERT(0);
        m_ImageAspectFlags = 0;
        break;
    }
}

VkResult VulkanImage::VerifyImageFormatProperties(VkImageCreateInfo &info) const
{
    VkResult res = VK_SUCCESS;
    VkImageFormatProperties properties = { };
    CHECK_VK_RESULT(m_pVulkanInst->GetPhysicalDeviceImageFormatProperties(
        m_pVulkanDev->GetPhysicalDevice()->GetVkPhysicalDevice()
            ,info.format
            ,info.imageType
            ,info.tiling
            ,info.usage
            ,info.flags
            ,&properties));

    if (info.extent.width > properties.maxExtent.width ||
        info.extent.height > properties.maxExtent.height ||
        info.extent.depth > properties.maxExtent.depth ||
        info.arrayLayers > properties.maxArrayLayers ||
        info.mipLevels > properties.maxMipLevels ||
        static_cast<UINT32>(info.samples) > properties.sampleCounts)
    {
        MASSERT(!"VerifyImageFormatProperties():info exceeds physical device properties!");
        return VK_ERROR_FORMAT_NOT_SUPPORTED;
    }
    return (res);
}

//------------------------------------------------------------------------------------------------
// Set the create image properties for images that are owned by another process.
VkResult VulkanImage::SetCreateImageProperties
(
    VkImage vkImage
    ,VkImageTiling tiling
    ,VkImageLayout initialLayout
    ,UINT64 pitch
    ,VkMemoryPropertyFlags memPropFlags
    ,const char * imageName
)
{
    // If this image was not constructed with the bOwnedByKHR bit set you can't set these
    // properties using this API. Instead you should be using CreateImage().
    if (m_bOwnedByKHR)
    {
        m_MutexHandle = Tasker::AllocMutex(imageName, Tasker::mtxUnchecked);
        m_Image = vkImage;
        m_ImageLayout = initialLayout;
        m_TilingType = tiling;
        m_Pitch = pitch;
        m_MemPropFlags = memPropFlags;
        m_ImageName = imageName;
        return VK_SUCCESS;
    }
    return VK_ERROR_FEATURE_NOT_PRESENT;
}

//-------------------------------------------------------------------------------------------------
// Creates the Image object but does not provide any data backing store for it.
VkResult VulkanImage::CreateImage
(
    VkImageCreateFlags createFlags,
    VkImageUsageFlags usageFlags,
    VkImageLayout initialLayout,
    VkImageTiling tiling,
    VkSampleCountFlagBits sampleCount,
    const char * imageName
)
{
    MASSERT(m_Image == VK_NULL_HANDLE);

    if (m_bOwnedByKHR)
    {
        // Can't create an image owned by KHR. You should be using SetCreateImageProperties()
        // to setup the internal variables instead.
        return VK_ERROR_FEATURE_NOT_PRESENT;
    }

    m_MutexHandle = Tasker::AllocMutex(imageName, Tasker::mtxUnchecked);
    m_ImageLayout = initialLayout;
    m_TilingType = tiling;
    m_SampleCount = sampleCount;
    m_ImageCreateFlags = createFlags;
    VkImageCreateInfo image_info = {};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    MASSERT(m_Format != VK_FORMAT_UNDEFINED);
    image_info.format = m_Format;
    MASSERT(m_Width > 0 && m_Height > 0);
    image_info.extent.width = m_Width;
    image_info.extent.height = m_Height;
    image_info.extent.depth = 1;
    image_info.mipLevels = m_MipLevels;
    image_info.arrayLayers = 1;
    image_info.samples = m_SampleCount;
    image_info.initialLayout = m_ImageLayout;
    image_info.usage = usageFlags;
    image_info.queueFamilyIndexCount = 0;
    image_info.pQueueFamilyIndices = nullptr;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.flags = m_ImageCreateFlags;
    image_info.tiling = m_TilingType;

    // HTex Image Restrictions:
    // Samplers must use addressing mode VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE.
    // VkImageCreateInfo::imageType must be VK_IMAGE_TYPE_2D or VK_IMAGE_TYPE_3D
    // VkImageCreateInfo::flags must NOT include VK_IMAGE_CREATE_LWBE_COMPATIBLE_BIT
    // VkImageCreateInfo::usage must NOT include VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
    // Yeah... I know the imageType is hardcoded to 2D right now but that will be fixed in the
    // near future.
    // We can remove the #ifdef wrapping of this check once this feature is released to the general
    // public.
#ifndef VULKAN_STANDALONE
    if (m_ImageCreateFlags & VK_IMAGE_CREATE_CORNER_SAMPLED_BIT_LW)
    {
        MASSERT((image_info.imageType == VK_IMAGE_TYPE_2D) ||
                (image_info.imageType == VK_IMAGE_TYPE_3D));
        MASSERT(!(image_info.flags & VK_IMAGE_CREATE_LWBE_COMPATIBLE_BIT));
        MASSERT(!(image_info.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT));
    }
#endif

    VkResult res = VK_SUCCESS;
    CHECK_VK_RESULT(VerifyImageFormatProperties(image_info));
    CHECK_VK_RESULT(m_pVulkanDev->CreateImage(&image_info, nullptr, &m_Image));
    m_ImageName = imageName;
    if (image_info.tiling == VK_IMAGE_TILING_LINEAR)
    {
        VkImageSubresource imageSubresource = {};
        imageSubresource.aspectMask = m_ImageAspectFlags;
        VkSubresourceLayout subresourceLayout = {};
        m_pVulkanDev->GetImageSubresourceLayout(m_Image, &imageSubresource, &subresourceLayout);
        m_Pitch = subresourceLayout.rowPitch;
    }
    else // default to something reasonable
    {
        m_Pitch = m_Width * 4;
    }

    return res;
}

//-------------------------------------------------------------------------------------------------
// Allocate and bind appropriate memory for the data backing store
VkResult VulkanImage::AllocateAndBindMemory(VkMemoryPropertyFlags memoryPropertyFlags)
{
    if (m_bOwnedByKHR)
    {
        // Can't allocate and bind memory for an image owned by KHR. Check your constructor!
        return VK_ERROR_FEATURE_NOT_PRESENT;
    }
    VkResult res = VK_SUCCESS;
    VkMemoryRequirements memoryRequirements;
    m_pVulkanDev->GetImageMemoryRequirements(m_Image, &memoryRequirements);

    // which memory type supports this
    VkMemoryAllocateInfo memAllocInfo = {};

    memAllocInfo.memoryTypeIndex = m_pVulkanDev->GetPhysicalDevice()->GetMemoryTypeIndex(
        memoryRequirements,    // required by the image memory
        memoryPropertyFlags);  // requested memory type
    if (memAllocInfo.memoryTypeIndex == UINT32_MAX)
    {
        return VK_ERROR_VALIDATION_FAILED_EXT;
    }

    memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memAllocInfo.allocationSize = memoryRequirements.size;
    CHECK_VK_RESULT(m_pVulkanDev->AllocateMemory(&memAllocInfo, NULL, &m_DeviceMemory));
    res = m_pVulkanDev->BindImageMemory(m_Image, m_DeviceMemory, 0);
    if (res != VK_SUCCESS)
    {
        m_pVulkanDev->FreeMemory(m_DeviceMemory);
        m_DeviceMemory = VK_NULL_HANDLE;
        return res;
    }
    m_MemPropFlags = memoryPropertyFlags;
    m_DeviceMemorySize = memAllocInfo.allocationSize;
    return res;
}

//-------------------------------------------------------------------------------------------------
VkResult VulkanImage::SetImageBarrier
(
    const VkImageMemoryBarrier& imgBarrier
    ,VkPipelineStageFlags srcStageMask
    ,VkPipelineStageFlags dstStageMask
    ,VulkanCmdBuffer* pCmdBuffer
)
{
    VkResult res = VK_SUCCESS;
    MASSERT(pCmdBuffer);
    const bool isCmdBufferRecordingActive = pCmdBuffer->GetRecordingBegun();
    if (!isCmdBufferRecordingActive)
    {
        CHECK_VK_RESULT(pCmdBuffer->BeginCmdBuffer());
    }
    MASSERT(pCmdBuffer->GetRecordingBegun());

    VulkanDevice* pVulkanDev = pCmdBuffer->GetVulkanDevice();
    pVulkanDev->CmdPipelineBarrier(pCmdBuffer->GetCmdBuffer(),
        srcStageMask,
        dstStageMask,
        0, 0, nullptr, 0, nullptr, 1, &imgBarrier);

    if (!isCmdBufferRecordingActive)
    {
        CHECK_VK_RESULT(pCmdBuffer->EndCmdBuffer());
        CHECK_VK_RESULT(pCmdBuffer->ExelwteCmdBuffer(true, true));
    }

    m_ImageLayout = imgBarrier.newLayout;
    m_ImageAccessMask = imgBarrier.dstAccessMask;
    m_ImageStageMask = dstStageMask;
    m_ImageAspectFlags = imgBarrier.subresourceRange.aspectMask;

    return res;
}

//-------------------------------------------------------------------------------------------------
VkResult VulkanImage::SetImageLayout
(
    VulkanCmdBuffer *pCmdBuffer
    ,VkImageAspectFlags newAspectFlags
    ,VkImageLayout newLayout
    ,VkAccessFlags newAccessMask
    ,VkPipelineStageFlags dstStageMask
)
{
    VkResult res = VK_SUCCESS;
    CHECK_VK_RESULT(VkUtil::SetImageLayout(
        pCmdBuffer
        ,m_Image
        ,newAspectFlags
        ,m_ImageLayout           //oldImgLayout
        ,newLayout               //newImgLayout
        ,m_ImageAccessMask       //srcAccessMask
        ,newAccessMask           //dstAccessMask
        ,m_ImageStageMask
        ,dstStageMask));
    m_ImageLayout = newLayout;
    m_ImageAccessMask = newAccessMask;
    m_ImageStageMask = dstStageMask;
    m_ImageAspectFlags = newAspectFlags;
    return res;
}

//-------------------------------------------------------------------------------------------------
VkResult VulkanImage::CopyFrom
(
    VulkanImage * pSrcImage,
    VulkanCmdBuffer * pCmdBuffer,
    const UINT08 destinationMipLevel
)
{
    VkResult res = VK_SUCCESS;
    MASSERT(pCmdBuffer);
    const bool isCmdBufferRecordingActive = pCmdBuffer->GetRecordingBegun();
    if (!isCmdBufferRecordingActive)
    {
        CHECK_VK_RESULT(pCmdBuffer->BeginCmdBuffer());
    }

    if ((pSrcImage->m_ImageLayout != VK_IMAGE_LAYOUT_GENERAL) ||
        (pSrcImage->m_ImageAccessMask != VK_ACCESS_TRANSFER_READ_BIT))
    {
        CHECK_VK_RESULT(pSrcImage->SetImageLayout(pCmdBuffer
            ,pSrcImage->GetImageAspectFlags()
            ,VK_IMAGE_LAYOUT_GENERAL
            ,VK_ACCESS_TRANSFER_READ_BIT                    //newAccess
            ,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT));  //dstMask
    }

    if ((m_ImageLayout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) ||
        (m_ImageAccessMask != VK_ACCESS_TRANSFER_WRITE_BIT))
    {
        CHECK_VK_RESULT(SetImageLayout(pCmdBuffer
            ,m_ImageAspectFlags
            ,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL           //new layout
            ,VK_ACCESS_TRANSFER_WRITE_BIT                   //new access
            ,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT)); //dstMask
    }

    const UINT32 scaleFactor(1 << destinationMipLevel);
    VkImageCopy cpyRegion = {};
    cpyRegion.srcSubresource.aspectMask = pSrcImage->m_ImageAspectFlags;
    cpyRegion.dstSubresource.aspectMask = m_ImageAspectFlags;
    cpyRegion.srcSubresource.baseArrayLayer = 0;
    cpyRegion.dstSubresource.baseArrayLayer = 0;
    cpyRegion.srcSubresource.layerCount = 1;
    cpyRegion.dstSubresource.layerCount = 1;
    cpyRegion.srcSubresource.mipLevel = 0;
    cpyRegion.dstSubresource.mipLevel = destinationMipLevel;
    cpyRegion.extent.width = std::max(m_Width / scaleFactor, 1U);
    cpyRegion.extent.height = std::max(m_Height / scaleFactor, 1U);
    cpyRegion.extent.depth = 1;

    m_pVulkanDev->CmdCopyImage(pCmdBuffer->GetCmdBuffer(),
        pSrcImage->m_Image,  pSrcImage->m_ImageLayout,
        m_Image,  m_ImageLayout,
        1, &cpyRegion);

    if (!isCmdBufferRecordingActive)
    {
        CHECK_VK_RESULT(pCmdBuffer->EndCmdBuffer());
        CHECK_VK_RESULT(pCmdBuffer->ExelwteCmdBuffer(true, true));
    }
    return res;
}

//-------------------------------------------------------------------------------------------------
// Update this object host visible memory with the contents of data.
// Note: Device local memory can not be mapped and this function will fail if you try
//       to use it on that type of object.
VkResult VulkanImage::SetData(void *data, VkDeviceSize size, VkDeviceSize offset)
{
    VkResult res = VK_SUCCESS;
    void *mappedData = nullptr;
    if (m_MemPropFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
    {
        CHECK_VK_RESULT(Map(size, offset, &mappedData));
        memcpy(mappedData, data, UNSIGNED_CAST(size_t, size));
        CHECK_VK_RESULT(Unmap());
    }
    else
    {
        res = VK_ERROR_FEATURE_NOT_PRESENT;
    }
    return res;
}

//-------------------------------------------------------------------------------------------------
// Get current contents of the host visible memory associated with this object.
// Note: Device local memory can not be mapped and this function will fail if you try
//       to use it on that type of object.
VkResult VulkanImage::GetData(void *data, VkDeviceSize size, VkDeviceSize offset)
{
    VkResult res = VK_SUCCESS;
    void *mappedData = nullptr;
    if (m_MemPropFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
    {
        CHECK_VK_RESULT(Map(size, offset, &mappedData));
        memcpy(data, mappedData, UNSIGNED_CAST(size_t, size));
        CHECK_VK_RESULT(Unmap());
    }
    else
    {
        res = VK_ERROR_FEATURE_NOT_PRESENT;
    }
    return res;
}

VkResult VulkanImage::Map(VkDeviceSize size, VkDeviceSize offset, void **pData)
{
    MASSERT(size <= m_DeviceMemorySize);
    if (!m_bOwnedByKHR)
    {
        if (m_Mapped)
        {
            return VK_ERROR_TOO_MANY_OBJECTS;
        }
        *pData = nullptr;
        const VkResult res = m_pVulkanDev->MapMemory(m_DeviceMemory, offset, size, 0, pData);
        if (!*pData)
        {
            return VK_ERROR_MEMORY_MAP_FAILED;
        }
        m_Mapped = true;
        return res;
    }
    else
    {
        return VK_ERROR_FEATURE_NOT_PRESENT;
    }
}

VkResult VulkanImage::Unmap()
{
    if (!m_bOwnedByKHR)
    {
        if (!m_Mapped)
        {
            return VK_ERROR_LAYER_NOT_PRESENT;
        }

        m_pVulkanDev->UnmapMemory(m_DeviceMemory);
        m_Mapped = false;
        return VK_SUCCESS;
    }
    else
    {
        return VK_ERROR_FEATURE_NOT_PRESENT;
    }
}

//--------------------------------------------------------------------
VkResult VulkanImage::CreateImageView()
{
    MASSERT(m_ImageView == VK_NULL_HANDLE);

    VkImageViewCreateInfo view_info = {};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.format = m_Format;
    view_info.components.r = VK_COMPONENT_SWIZZLE_R;
    view_info.components.g = VK_COMPONENT_SWIZZLE_G;
    view_info.components.b = VK_COMPONENT_SWIZZLE_B;
    view_info.components.a = VK_COMPONENT_SWIZZLE_A;
    view_info.subresourceRange.aspectMask = m_ImageAspectFlags;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = m_MipLevels;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.flags = 0;
    view_info.image = m_Image;
    return m_pVulkanDev->CreateImageView(&view_info, nullptr, &m_ImageView);
}

//--------------------------------------------------------------------
bool VulkanImage::IsSamplingSupported() const
{
    VkFormatProperties formatProps;
    m_pVulkanInst->GetPhysicalDeviceFormatProperties(
        m_pVulkanDev->GetPhysicalDevice()->GetVkPhysicalDevice(), m_Format, &formatProps);
    if (m_TilingType == VK_IMAGE_TILING_OPTIMAL)
    {
        if (!(formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))
        {
            return false;
        }
    }
    else
    {
        if (!(formatProps.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))
        {
            return false;
        }
    }
    return true;
}

void VulkanImage::Reset(VulkanDevice* pVulkanDev, bool bOwnedByKHR)
{
    Cleanup();
    m_pVulkanDev = pVulkanDev;
    m_bOwnedByKHR = bOwnedByKHR;
    m_pVulkanInst = pVulkanDev->GetVulkanInstance();
}

//--------------------------------------------------------------------
void VulkanImage::Cleanup()
{
    if (m_MutexHandle)
    {
        Tasker::FreeMutex(m_MutexHandle);
        m_MutexHandle = nullptr;
    }

#ifndef VULKAN_STANDALONE
    if (m_DispContextDma)
    {
        LwRmPtr()->Free(m_DispContextDma);
        m_DispContextDma = 0;
    }

    if (m_DupMemHandle)
    {
        LwRmPtr()->Free(m_DupMemHandle);
        m_DupMemHandle = 0;
    }
#endif

    if (!m_pVulkanDev)
    {
        MASSERT(m_DeviceMemory == VK_NULL_HANDLE);
        MASSERT(m_Image == VK_NULL_HANDLE);
        MASSERT(m_ImageView == VK_NULL_HANDLE);
        MASSERT(!m_Mapped);
        return;
    }
    if (m_Mapped)
    {
        // Unmap requires a valid m_pVulkanDev
        Unmap();
        m_Mapped = false;
    }
    if (!m_bOwnedByKHR)
    {
        if (m_DeviceMemory)
        {
            m_pVulkanDev->FreeMemory(m_DeviceMemory);
        }
        if (m_Image)
        {
            m_pVulkanDev->DestroyImage(m_Image);
        }
    }
    // Even if the memory is owned by KHR we still need to clear the handles.
    m_DeviceMemory = VK_NULL_HANDLE;
    m_Image = VK_NULL_HANDLE;

    if (m_ImageView)
    {
        m_pVulkanDev->DestroyImageView(m_ImageView);
        m_ImageView = VK_NULL_HANDLE;
    }

    m_Pitch = 0;
    m_DeviceMemorySize = 0;
    m_pVulkanDev = nullptr;
    m_pVulkanInst = nullptr;
}

void VulkanImage::TakeResources(VulkanImage&& image)
{
    if (this == &image)
    {
        return;
    }

    Cleanup();

    m_pVulkanInst          = image.m_pVulkanInst;
    m_pVulkanDev           = image.m_pVulkanDev;
    m_bOwnedByKHR          = image.m_bOwnedByKHR;
    m_Format               = image.m_Format;
    m_DeviceMemory         = image.m_DeviceMemory;
    m_Image                = image.m_Image;
    m_ImageView            = image.m_ImageView;
    m_TilingType           = image.m_TilingType;
    m_Width                = image.m_Width;
    m_Pitch                = image.m_Pitch;
    m_Height               = image.m_Height;
    m_SampleCount          = image.m_SampleCount;
    m_ImageType            = image.m_ImageType;
    m_MipLevels            = image.m_MipLevels;
    m_ImageAspectFlags     = image.m_ImageAspectFlags;
    m_ImageLayout          = image.m_ImageLayout;
    m_ImageAccessMask      = image.m_ImageAccessMask;
    m_ImageStageMask       = image.m_ImageStageMask;
    m_DeviceMemorySize     = image.m_DeviceMemorySize;
    m_Mapped               = image.m_Mapped;
    m_MemPropFlags         = image.m_MemPropFlags;
    m_ImageName            = move(image.m_ImageName);
    m_MutexHandle          = image.m_MutexHandle;
#ifndef VULKAN_STANDALONE
    m_LwLayoutLWX          = image.m_LwLayoutLWX;
    m_DupMemHandle         = image.m_DupMemHandle;
    m_DispContextDma       = image.m_DispContextDma;
    m_DispContextDmaOffset = image.m_DispContextDmaOffset;
    image.m_DispContextDma = 0;
    image.m_DupMemHandle   = 0;
#endif
    m_ImageCreateFlags     = image.m_ImageCreateFlags;

    image.m_pVulkanInst    = nullptr;
    image.m_pVulkanDev     = nullptr;
    image.m_MutexHandle    = nullptr;
    image.m_DeviceMemory   = VK_NULL_HANDLE;
    image.m_Image          = VK_NULL_HANDLE;
    image.m_ImageView      = VK_NULL_HANDLE;
    image.m_Mapped         = false;
    image.m_Pitch          = 0;
    image.m_DeviceMemorySize = 0;
}

void VulkanImage::AcquireMutex()
{
    MASSERT(m_MutexHandle);
    Tasker::AcquireMutex(m_MutexHandle);
}

void VulkanImage::ReleaseMutex()
{
    MASSERT(m_MutexHandle);
    Tasker::ReleaseMutex(m_MutexHandle);
}

void * VulkanImage::GetMutex()
{
    return m_MutexHandle;
}
