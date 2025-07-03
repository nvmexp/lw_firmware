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
#ifndef VK_UTIL_H
#define VK_UTIL_H

#include "vkmain.h"
#include "core/include/color.h"

//-------------------------------------------------------------------------------------------------
// This define is used to specify a missing VkMemoryPropertyFlagBits define. It means that the
// memory should be allocated from System memory that is accessible by the GPU but not the CPU.
// We use this define when there is no FB memory and we need to switch all allocations from
// VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT to VK_MEMORY_PROPERTY_SYSMEM_BIT.
#define VK_MEMORY_PROPERTY_SYSMEM_BIT   0

class VulkanCmdBuffer;
class VulkanBuffer;
class VulkanDevice;
class VulkanImage;

//--------------------------------------------------------------------
namespace VkUtil
{
    enum AttachmentType
    {
        COLOR,
        DEPTH,
        INVALID
    };
    enum SendBarrierType
    {
        DONT_SEND_BARRIER = 0,
        SEND_BARRIER = 1
    };

    enum SurfaceCheck
    {
        CHECK_COLOR_SURFACE          = 0x01
        ,CHECK_DEPTH_SURFACE         = 0x02
        ,CHECK_STENCIL_SURFACE       = 0x04
        ,CHECK_DEPTH_STENCIL_SURFACE = 0x06
        ,CHECK_COMPUTE_FP16          = 0x08
        ,CHECK_COMPUTE_FP32          = 0x10
        ,CHECK_COMPUTE_FP64          = 0x20
        ,CHECK_RT_SURFACE            = 0x40
        ,CHECK_ALL_SURFACES          = 0x7F
    };
    const char* SurfaceTypeToString(VkUtil::SurfaceCheck surfType);

    enum DebugFlags
    {
        DBG_IGNORE_FRAME_CHECK   = 1 << 0, // overrides any failure checks when trying to dump
                                           // pngs on frames that don't align with a quadframe.
        DBG_GEOMETRY             = 1 << 1,
        DBG_BUFFER_CHECK         = 1 << 2,  // instrument BufferCheckMs algorithm for debugging.
        DBG_PRINT_ROTATION       = 1 << 3,
        DBG_FMAX                 = 1 << 4   // instrument FMax controller
    };

    ColorUtils::Format ColorUtilsFormat(VkFormat fmt);

    VkResult SetImageLayout
    (
        VulkanCmdBuffer *pCmdBuffer
        ,VkImage image
        ,VkImageAspectFlags aspectMask
        ,VkImageLayout old_image_layout
        ,VkImageLayout new_image_layout
        ,VkAccessFlags srcAccessMask
        ,VkAccessFlags dstAccessMask
        ,VkPipelineStageFlags srcStageMask
        ,VkPipelineStageFlags dstStageMask
    );

    VkResult CopyBuffer
    (
        VulkanCmdBuffer *pCmdBuffer
        ,VulkanBuffer *pSrcBuffer
        ,VulkanBuffer *pDstBuffer
        ,VkAccessFlags srcAccess
        ,VkAccessFlags dstAccess
        ,VkPipelineStageFlags srcStageMask
        ,VkPipelineStageFlags dstStageMask
    );

    VkResult CopyBuffer
    (
        VulkanCmdBuffer *pCmdBuffer,
        VulkanBuffer *pSrcBuffer,
        VulkanBuffer *pDstBuffer
    );

    VkResult CopyBuffer
    (
        VulkanCmdBuffer *pCmdBuffer
        ,VulkanBuffer *pSrcBuffer
        ,VulkanBuffer *pDstBuffer
        ,UINT64 srcOffset
        ,UINT64 dstOffset
        ,UINT64 numBytes
        ,VkAccessFlags srcAccess
        ,VkAccessFlags dstAccess
        ,VkPipelineStageFlags srcStageMask
        ,VkPipelineStageFlags dstStageMask

    );

    VkResult FillBuffer
    (
        VulkanCmdBuffer *pCmdBuffer
        ,VulkanBuffer *pDstBuffer
        ,VkDeviceSize offset
        ,VkDeviceSize size
        ,UINT32 data
        ,VkAccessFlags srcAccess
        ,VkAccessFlags dstAccess
        ,VkPipelineStageFlags srcStageMask
        ,VkPipelineStageFlags dstStateMask
    );

    VkResult UpdateBuffer
    (
        VulkanCmdBuffer *pCmdBuffer
        ,VulkanBuffer *pDstBuffer
        ,VkDeviceSize offset
        ,VkDeviceSize size
        ,void * pData
        ,VkAccessFlags srcAccess
        ,VkAccessFlags dstAccess
        ,VkPipelineStageFlags srcStageMask
        ,VkPipelineStageFlags dstStateMask
        ,SendBarrierType sendBarrier = SEND_BARRIER
    );

    VkResult CopyImage
    (
        VulkanCmdBuffer *pCmdBuffer,
        VkSemaphore waitSemaphore,
        VulkanImage &srcImage,
        VkImageLayout srcImageLwrrentLayout,
        VkAccessFlags srcImageLwrrentAccessMask,
        VkPipelineStageFlags srcImageLwrrentStageMask,
        VulkanImage &dstImage,
        VkImageLayout dstImageLwrrentLayout,
        VkAccessFlags dstImageLwrrentAccessMask,
        VkPipelineStageFlags dstImageLwrrentStageMask
    );

    VkResult CopyImage
    (
        VulkanCmdBuffer *pCmdBuffer,
        VkSemaphore waitSemaphore,
        VkImage srcImage,
        VkImageLayout *pSrcImageLwrrentLayout,
        VkAccessFlags *pSrcImageLwrrentAccessMask,
        VkImageAspectFlags srcImageAspectFlags,
        VkPipelineStageFlags *pSrcImageLwrrentStageMask,
        UINT32 width,
        UINT32 height,
        VkImage dstImage,
        VkImageLayout *pDstImageLwrrentLayout,
        VkAccessFlags *pDstImageLwrrentAccessMask,
        VkImageAspectFlags dstImageAspectFlags,
        VkPipelineStageFlags *pDstImageLwrrentStageMask
    );

    class DebugMarker
    {
    public:
        explicit DebugMarker(float c1 = 0.0f, float c2 = 0.0f, float c3 = 0.0f, float c4 = 0.0f);
        ~DebugMarker();
        void Begin
        (
            bool enable,
            VulkanDevice *pVulkanDevice,
            VkCommandBuffer commandBuffer,
            const char *nameFormat,
            ...
        );
        void End();

    private:
        bool m_Enabled;
        VulkanDevice *m_VulkanDevice;
        VkCommandBuffer m_CommandBuffer;
        char m_Name[32];
        VkDebugMarkerMarkerInfoEXT m_MarkerInfoExt;
    };
};

#ifdef VULKAN_ENABLE_DEBUG_MARKERS
#define VK_DEBUG_MARKER(Name, ...) DebugMarker Name(__VA_ARGS__);
#define VK_DEBUG_MARKER_BEGIN(Marker, Enable, VulkanDevice, CommandBuffer, Format, ...) \
    if (Enable) \
    { \
        Marker.Begin(Enable, VulkanDevice, CommandBuffer, Format, __VA_ARGS__); \
    }
#define VK_DEBUG_MARKER_END(Marker) Marker.End();
#else
#define VK_DEBUG_MARKER(Name, ...)
#define VK_DEBUG_MARKER_BEGIN(Marker, Enable, VulkanDevice, CommandBuffer, Format, ...)
#define VK_DEBUG_MARKER_END(Marker)
#endif


#endif
