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
#ifndef VK_IMAGE_H
#define VK_IMAGE_H

#include "vkmain.h"
#include "core/include/types.h"      // for UINT08
class VulkanCmdBuffer;
class VulkanDevice;
class VulkanInstance;

//--------------------------------------------------------------------
class VulkanImage
{
public:
    enum ImageType
    {
        COLOR,
        DEPTH,
        STENCIL,
        DEPTH_STENCIL,
        INVALID
    };

    VulkanImage() = default;
    explicit VulkanImage(VulkanDevice* pVulkanDev, bool bOwnedByKHR = false);
    VulkanImage(VulkanImage&& image) { TakeResources(move(image)); }
    VulkanImage& operator=(VulkanImage&& image) { TakeResources(move(image)); return *this; }
    VulkanImage(const VulkanImage&) = delete;
    VulkanImage& operator=(const VulkanImage&) = delete;
    ~VulkanImage();

    void *      GetMutex();
    void        AcquireMutex();
    void        ReleaseMutex();
    void        Reset(VulkanDevice* pVulkanDev, bool bOwnedByKHR = false);
    VkResult    AllocateAndBindMemory(VkMemoryPropertyFlags memoryPropertyFlags);
    void        Cleanup();
    VkResult    CreateImage
                (
                    VkImageCreateFlags createFlags,
                    VkImageUsageFlags usageFlag,
                    VkImageLayout initialLayout,
                    VkImageTiling tiling,
                    VkSampleCountFlagBits sampleCount,
                    const char * imageName = "unknown"
                );
    // Create an Image View so the shaders can get access to the image data.
    VkResult    CreateImageView();

    //! \brief Copies data from a source VulkanImage
    //! \param pSrcImage - the source VulkanImage
    //! \param pCmdBuffer - the command buffer that will perform the copy
    //
    // Note: CopyFrom will copy the image data, but you will probably need
    // an additional memory barrier to switch to the final desired image layout
    // such as VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL.
    VkResult    CopyFrom
                (
                    VulkanImage * pSrcImage,
                    VulkanCmdBuffer *pCmdBuffer,
                    UINT08 destinationMipLevel
                );

    VkResult    GetData(void *data, VkDeviceSize size, VkDeviceSize offset);
    bool        IsSamplingSupported() const;

    VkResult    SetData(void *data, VkDeviceSize size, VkDeviceSize offset);
    VkResult    SetImageBarrier
                (
                    const VkImageMemoryBarrier& imgBarrier,
                    VkPipelineStageFlags srcStageMask,
                    VkPipelineStageFlags dstStageMask,
                    VulkanCmdBuffer* pCmdBuffer
                );
    VkResult    SetImageLayout
                (
                    VulkanCmdBuffer *pCmdBuffer,
                    VkImageAspectFlags newAspectFlags,
                    VkImageLayout newLayout,
                    VkAccessFlags newAccessMask,
                    VkPipelineStageFlags dstStageMask
                );
    void        SetImageProperties
                (
                    const UINT32 width,
                    const UINT32 height,
                    const UINT08 mipLevels,
                    const ImageType imageType
                );
    VkResult    SetCreateImageProperties
                (
                    VkImage vkImage,
                    VkImageTiling tiling,
                    VkImageLayout  initialLayout,
                    UINT64 pitch,
                    VkMemoryPropertyFlags memPropFlags,
                    const char * imageName = "unknown"
                 );
    VkResult    Map(VkDeviceSize size, VkDeviceSize offset, void **pData);
    void        PickFormat(const VkFormat* formats, UINT32 count, VkFormatFeatureFlags flags);
    void        PickColorFormat();
    void        PickDepthFormat();

#ifndef VULKAN_STANDALONE
    RC          PopulateDisplayImageDescription
                (
                    GpuDevice *pGpuDevice,
                    GpuUtility::DisplayImageDescription *desc
                );

    RC          GetDupMemHandle(GpuDevice *pGpuDevice, UINT32 *dupMemHandle);
#endif

    VkResult    Unmap();

    SETGET_PROP (Format, VkFormat);
    SETGET_PROP (Width, UINT32);
    SETGET_PROP (Pitch, UINT64);
    SETGET_PROP (Height, UINT32);
    GET_PROP    (MipLevels, UINT08);
    GET_PROP    (DeviceMemorySize, UINT64);
    GET_PROP    (Image, VkImage);
    SETGET_PROP (ImageType, UINT32);
    GET_PROP    (ImageView, VkImageView);
    SETGET_PROP (ImageAspectFlags, VkImageAspectFlags);
    SETGET_PROP (ImageAccessMask, VkAccessFlags);
    SETGET_PROP (ImageStageMask, VkPipelineStageFlags);
    SETGET_PROP (ImageLayout, VkImageLayout);
    SETGET_PROP (ImageCreateFlags, VkImageCreateFlags);
    SETGET_PROP (ImageName, string);

private:
    VkResult VerifyImageFormatProperties(VkImageCreateInfo &info) const;
    void     TakeResources(VulkanImage&& image);

    VulkanInstance             *m_pVulkanInst = nullptr;
    VulkanDevice               *m_pVulkanDev = nullptr;

    // If true then we don't take responsibility for creating the actual VkImage object and
    // it will be provided
    bool                        m_bOwnedByKHR = false;

    VkFormat                    m_Format = VK_FORMAT_UNDEFINED;
    VkDeviceMemory              m_DeviceMemory = VK_NULL_HANDLE;
    VkImage                     m_Image = VK_NULL_HANDLE;
    VkImageView                 m_ImageView = VK_NULL_HANDLE;

    VkImageTiling               m_TilingType = VK_IMAGE_TILING_OPTIMAL;
    UINT32                      m_Width = 0;
    UINT64                      m_Pitch = 0;
    UINT32                      m_Height = 0;
    VkSampleCountFlagBits       m_SampleCount = VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM;
    UINT32                      m_ImageType = ImageType::INVALID;
    UINT08                      m_MipLevels = 1;
    VkImageAspectFlags          m_ImageAspectFlags = 0;
    VkImageLayout               m_ImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkAccessFlags               m_ImageAccessMask = 0;
    VkPipelineStageFlags        m_ImageStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    UINT64                      m_DeviceMemorySize = 0;
    bool                        m_Mapped = false;
    VkMemoryPropertyFlags       m_MemPropFlags = 0;
    string                      m_ImageName;

    void *                      m_MutexHandle = nullptr;
#ifndef VULKAN_STANDALONE
    VkLwLayoutLWX               m_LwLayoutLWX = {};
    UINT32                      m_DupMemHandle = 0;
    UINT32                      m_DispContextDma = 0;
    uint64_t                    m_DispContextDmaOffset = 0;
    RC                          DuplicateMemoryHandle(GpuDevice *pGpuDevice);
#endif

    VkImageCreateFlags          m_ImageCreateFlags = 0;
};

#endif
