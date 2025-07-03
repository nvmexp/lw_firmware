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
#include "vkutil.h"
#include "vkcmdbuffer.h"
#include "vkbuffer.h"
#include "vkimage.h"
#include "vkdev.h"

//-----------------------------------------------------------------------------
ColorUtils::Format VkUtil::ColorUtilsFormat(VkFormat fmt)
{
    switch (fmt)
    {
        case VK_FORMAT_UNDEFINED :                          return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_R4G4_UNORM_PACK8 :                   return ColorUtils::G4R4;
        case VK_FORMAT_R4G4B4A4_UNORM_PACK16 :              return ColorUtils::R4G4B4A4;
        case VK_FORMAT_B4G4R4A4_UNORM_PACK16 :              return ColorUtils::B4G4R4A4;
        case VK_FORMAT_R5G6B5_UNORM_PACK16 :                return ColorUtils::R5G6B5;
        case VK_FORMAT_B5G6R5_UNORM_PACK16 :                return ColorUtils::B5G6R5;
        case VK_FORMAT_R5G5B5A1_UNORM_PACK16 :              return ColorUtils::R5G5B5A1;
        case VK_FORMAT_B5G5R5A1_UNORM_PACK16 :              return ColorUtils::B5G5R5A1;
        case VK_FORMAT_A1R5G5B5_UNORM_PACK16 :              return ColorUtils::A1R5G5B5;
        case VK_FORMAT_R8_UNORM :                           return ColorUtils::R8;
        case VK_FORMAT_R8_SNORM :                           return ColorUtils::RN8;
        case VK_FORMAT_R8_USCALED :                         return ColorUtils::RU8;
        case VK_FORMAT_R8_SSCALED :                         return ColorUtils::RS8;
        case VK_FORMAT_R8_UINT :                            return ColorUtils::RU8;
        case VK_FORMAT_R8_SINT :                            return ColorUtils::RS8;
        case VK_FORMAT_R8_SRGB :                            return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_R8G8_UNORM :                         return ColorUtils::G8R8;
        case VK_FORMAT_R8G8_SNORM :                         return ColorUtils::GN8RN8;
        case VK_FORMAT_R8G8_USCALED :                       return ColorUtils::GU8RU8;
        case VK_FORMAT_R8G8_SSCALED :                       return ColorUtils::GS8RS8;
        case VK_FORMAT_R8G8_UINT :                          return ColorUtils::GU8RU8;
        case VK_FORMAT_R8G8_SINT :                          return ColorUtils::GS8RS8;
        case VK_FORMAT_R8G8_SRGB :                          return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_R8G8B8_UNORM :                       return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_R8G8B8_SNORM :                       return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_R8G8B8_USCALED :                     return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_R8G8B8_SSCALED :                     return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_R8G8B8_UINT :                        return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_R8G8B8_SINT :                        return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_R8G8B8_SRGB :                        return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_B8G8R8_UNORM :                       return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_B8G8R8_SNORM :                       return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_B8G8R8_USCALED :                     return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_B8G8R8_SSCALED :                     return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_B8G8R8_UINT :                        return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_B8G8R8_SINT :                        return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_B8G8R8_SRGB :                        return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_R8G8B8A8_UNORM :                     return ColorUtils::A8B8G8R8;
        case VK_FORMAT_R8G8B8A8_SNORM :                     return ColorUtils::AN8BN8GN8RN8;
        case VK_FORMAT_R8G8B8A8_USCALED :                   return ColorUtils::AU8BU8GU8RU8;
        case VK_FORMAT_R8G8B8A8_SSCALED :                   return ColorUtils::AS8BS8GS8RS8;
        case VK_FORMAT_R8G8B8A8_UINT :                      return ColorUtils::AU8BU8GU8RU8;
        case VK_FORMAT_R8G8B8A8_SINT :                      return ColorUtils::AS8BS8GS8RS8;
        case VK_FORMAT_R8G8B8A8_SRGB :                      return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_B8G8R8A8_UNORM :                     return ColorUtils::A8R8G8B8;
        case VK_FORMAT_B8G8R8A8_SNORM :                     return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_B8G8R8A8_USCALED :                   return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_B8G8R8A8_SSCALED :                   return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_B8G8R8A8_UINT :                      return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_B8G8R8A8_SINT :                      return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_B8G8R8A8_SRGB :                      return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_A8B8G8R8_UNORM_PACK32 :              return ColorUtils::A8R8G8B8;
        case VK_FORMAT_A8B8G8R8_SNORM_PACK32 :              return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_A8B8G8R8_USCALED_PACK32 :            return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_A8B8G8R8_SSCALED_PACK32 :            return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_A8B8G8R8_UINT_PACK32 :               return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_A8B8G8R8_SINT_PACK32 :               return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_A8B8G8R8_SRGB_PACK32 :               return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_A2R10G10B10_UNORM_PACK32 :           return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_A2R10G10B10_SNORM_PACK32 :           return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_A2R10G10B10_USCALED_PACK32 :         return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_A2R10G10B10_SSCALED_PACK32 :         return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_A2R10G10B10_UINT_PACK32 :            return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_A2R10G10B10_SINT_PACK32 :            return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_A2B10G10R10_UNORM_PACK32 :           return ColorUtils::A2B10G10R10;
        case VK_FORMAT_A2B10G10R10_SNORM_PACK32 :           return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_A2B10G10R10_USCALED_PACK32 :         return ColorUtils::AU2BU10GU10RU10;
        case VK_FORMAT_A2B10G10R10_SSCALED_PACK32 :         return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_A2B10G10R10_UINT_PACK32 :            return ColorUtils::AU2BU10GU10RU10;
        case VK_FORMAT_A2B10G10R10_SINT_PACK32 :            return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_R16_UNORM :                          return ColorUtils::R16;
        case VK_FORMAT_R16_SNORM :                          return ColorUtils::RN16;
        case VK_FORMAT_R16_USCALED :                        return ColorUtils::RU16;
        case VK_FORMAT_R16_SSCALED :                        return ColorUtils::RS16;
        case VK_FORMAT_R16_UINT :                           return ColorUtils::RU16;
        case VK_FORMAT_R16_SINT :                           return ColorUtils::RS16;
        case VK_FORMAT_R16_SFLOAT :                         return ColorUtils::RF16;
        case VK_FORMAT_R16G16_UNORM :                       return ColorUtils::R16_G16;
        case VK_FORMAT_R16G16_SNORM :                       return ColorUtils::RN16_GN16;
        case VK_FORMAT_R16G16_USCALED :                     return ColorUtils::RU16_GU16;
        case VK_FORMAT_R16G16_SSCALED :                     return ColorUtils::RS16_GS16;
        case VK_FORMAT_R16G16_UINT :                        return ColorUtils::RU16_GU16;
        case VK_FORMAT_R16G16_SINT :                        return ColorUtils::RS16_GS16;
        case VK_FORMAT_R16G16_SFLOAT :                      return ColorUtils::RF16_GF16;
        case VK_FORMAT_R16G16B16_UNORM :                    return ColorUtils::R32_G32_B32;
        case VK_FORMAT_R16G16B16_SNORM :                    return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_R16G16B16_USCALED :                  return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_R16G16B16_SSCALED :                  return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_R16G16B16_UINT :                     return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_R16G16B16_SINT :                     return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_R16G16B16_SFLOAT :                   return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_R16G16B16A16_UNORM :                 return ColorUtils::R16_G16_B16_A16;
        case VK_FORMAT_R16G16B16A16_SNORM :                 return ColorUtils::RN16_GN16_BN16_AN16;
        case VK_FORMAT_R16G16B16A16_USCALED :               return ColorUtils::RU16_GU16_BU16_AU16;
        case VK_FORMAT_R16G16B16A16_SSCALED :               return ColorUtils::RS16_GS16_BS16_AS16;
        case VK_FORMAT_R16G16B16A16_UINT :                  return ColorUtils::RU16_GU16_BU16_AU16;
        case VK_FORMAT_R16G16B16A16_SINT :                  return ColorUtils::RS16_GS16_BS16_AS16;
        case VK_FORMAT_R16G16B16A16_SFLOAT :                return ColorUtils::RF16_GF16_BF16_AF16;
        case VK_FORMAT_R32_UINT :                           return ColorUtils::RU32;
        case VK_FORMAT_R32_SINT :                           return ColorUtils::RS32;
        case VK_FORMAT_R32_SFLOAT :                         return ColorUtils::RF32;
        case VK_FORMAT_R32G32_UINT :                        return ColorUtils::RU32_GU32;
        case VK_FORMAT_R32G32_SINT :                        return ColorUtils::RS32_GS32;
        case VK_FORMAT_R32G32_SFLOAT :                      return ColorUtils::RF32_GF32;
        case VK_FORMAT_R32G32B32_UINT :                     return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_R32G32B32_SINT :                     return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_R32G32B32_SFLOAT :                   return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_R32G32B32A32_UINT :                  return ColorUtils::RU32_GU32_BU32_AU32;
        case VK_FORMAT_R32G32B32A32_SINT :                  return ColorUtils::RS32_GS32_BS32_AS32;
        case VK_FORMAT_R32G32B32A32_SFLOAT :                return ColorUtils::RF32_GF32_BF32_X32;
        case VK_FORMAT_R64_UINT :                           return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_R64_SINT :                           return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_R64_SFLOAT :                         return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_R64G64_UINT :                        return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_R64G64_SINT :                        return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_R64G64_SFLOAT :                      return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_R64G64B64_UINT :                     return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_R64G64B64_SINT :                     return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_R64G64B64_SFLOAT :                   return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_R64G64B64A64_UINT :                  return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_R64G64B64A64_SINT :                  return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_R64G64B64A64_SFLOAT :                return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_B10G11R11_UFLOAT_PACK32 :            return ColorUtils::BF10GF11RF11;
        case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32 :             return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_D16_UNORM :                          return ColorUtils::Z16;
        case VK_FORMAT_X8_D24_UNORM_PACK32 :                return ColorUtils::X8Z24;
        case VK_FORMAT_D32_SFLOAT :                         return ColorUtils::ZF32;
        case VK_FORMAT_S8_UINT :                            return ColorUtils::S8;
        case VK_FORMAT_D16_UNORM_S8_UINT :                  return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_D24_UNORM_S8_UINT :                  return ColorUtils::S8Z24;
        case VK_FORMAT_D32_SFLOAT_S8_UINT :                 return ColorUtils::ZF32_X24S8;
        case VK_FORMAT_BC1_RGB_UNORM_BLOCK :                return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_BC1_RGB_SRGB_BLOCK :                 return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_BC1_RGBA_UNORM_BLOCK :               return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_BC1_RGBA_SRGB_BLOCK :                return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_BC2_UNORM_BLOCK :                    return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_BC2_SRGB_BLOCK :                     return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_BC3_UNORM_BLOCK :                    return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_BC3_SRGB_BLOCK :                     return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_BC4_UNORM_BLOCK :                    return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_BC4_SNORM_BLOCK :                    return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_BC5_UNORM_BLOCK :                    return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_BC5_SNORM_BLOCK :                    return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_BC6H_UFLOAT_BLOCK :                  return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_BC6H_SFLOAT_BLOCK :                  return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_BC7_UNORM_BLOCK :                    return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_BC7_SRGB_BLOCK :                     return ColorUtils::LWFMT_NONE;
        case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK :            return ColorUtils::ETC2_RGB;
        case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK :             return ColorUtils::ETC2_RGB;
        case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK :          return ColorUtils::ETC2_RGB_PTA;
        case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK :           return ColorUtils::ETC2_RGB_PTA;
        case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK :          return ColorUtils::ETC2_RGBA;
        case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK :           return ColorUtils::ETC2_RGBA;
        case VK_FORMAT_EAC_R11_UNORM_BLOCK :                return ColorUtils::EAC;
        case VK_FORMAT_EAC_R11_SNORM_BLOCK :                return ColorUtils::EAC;
        case VK_FORMAT_EAC_R11G11_UNORM_BLOCK :             return ColorUtils::EACX2;
        case VK_FORMAT_EAC_R11G11_SNORM_BLOCK :             return ColorUtils::EACX2;
        case VK_FORMAT_ASTC_4x4_UNORM_BLOCK :               return ColorUtils::ASTC_2D_4X4;
        case VK_FORMAT_ASTC_4x4_SRGB_BLOCK :                return ColorUtils::ASTC_2D_4X4;
        case VK_FORMAT_ASTC_5x4_UNORM_BLOCK :               return ColorUtils::ASTC_2D_5X4;
        case VK_FORMAT_ASTC_5x4_SRGB_BLOCK :                return ColorUtils::ASTC_2D_5X4;
        case VK_FORMAT_ASTC_5x5_UNORM_BLOCK :               return ColorUtils::ASTC_2D_5X5;
        case VK_FORMAT_ASTC_5x5_SRGB_BLOCK :                return ColorUtils::ASTC_2D_5X5;
        case VK_FORMAT_ASTC_6x5_UNORM_BLOCK :               return ColorUtils::ASTC_2D_6X5;
        case VK_FORMAT_ASTC_6x5_SRGB_BLOCK :                return ColorUtils::ASTC_2D_6X5;
        case VK_FORMAT_ASTC_6x6_UNORM_BLOCK :               return ColorUtils::ASTC_2D_6X6;
        case VK_FORMAT_ASTC_6x6_SRGB_BLOCK :                return ColorUtils::ASTC_2D_6X6;
        case VK_FORMAT_ASTC_8x5_UNORM_BLOCK :               return ColorUtils::ASTC_2D_8X5;
        case VK_FORMAT_ASTC_8x5_SRGB_BLOCK :                return ColorUtils::ASTC_2D_8X5;
        case VK_FORMAT_ASTC_8x6_UNORM_BLOCK :               return ColorUtils::ASTC_2D_8X6;
        case VK_FORMAT_ASTC_8x6_SRGB_BLOCK :                return ColorUtils::ASTC_2D_8X6;
        case VK_FORMAT_ASTC_8x8_UNORM_BLOCK :               return ColorUtils::ASTC_2D_8X8;
        case VK_FORMAT_ASTC_8x8_SRGB_BLOCK :                return ColorUtils::ASTC_2D_8X8;
        case VK_FORMAT_ASTC_10x5_UNORM_BLOCK :              return ColorUtils::ASTC_2D_10X5;
        case VK_FORMAT_ASTC_10x5_SRGB_BLOCK :               return ColorUtils::ASTC_2D_10X5;
        case VK_FORMAT_ASTC_10x6_UNORM_BLOCK :              return ColorUtils::ASTC_2D_10X6;
        case VK_FORMAT_ASTC_10x6_SRGB_BLOCK :               return ColorUtils::ASTC_2D_10X6;
        case VK_FORMAT_ASTC_10x8_UNORM_BLOCK :              return ColorUtils::ASTC_2D_10X8;
        case VK_FORMAT_ASTC_10x8_SRGB_BLOCK :               return ColorUtils::ASTC_2D_10X8;
        case VK_FORMAT_ASTC_10x10_UNORM_BLOCK :             return ColorUtils::ASTC_2D_10X10;
        case VK_FORMAT_ASTC_10x10_SRGB_BLOCK :              return ColorUtils::ASTC_2D_10X10;
        case VK_FORMAT_ASTC_12x10_UNORM_BLOCK :             return ColorUtils::ASTC_2D_12X10;
        case VK_FORMAT_ASTC_12x10_SRGB_BLOCK :              return ColorUtils::ASTC_2D_12X10;
        case VK_FORMAT_ASTC_12x12_UNORM_BLOCK :             return ColorUtils::ASTC_2D_12X12;
        case VK_FORMAT_ASTC_12x12_SRGB_BLOCK :              return ColorUtils::ASTC_2D_12X12;
        default:
            MASSERT(!"Unknown ColorUtils format");
            return ColorUtils::LWFMT_NONE;
    }
}

//--------------------------------------------------------------------
VkResult VkUtil::SetImageLayout
(
    VulkanCmdBuffer *pCmdBuffer,
    VkImage image,
    VkImageAspectFlags aspectMask,
    VkImageLayout oldImgLayout,
    VkImageLayout newImgLayout,
    VkAccessFlags srcAccessMask,
    VkAccessFlags dstAccessMask,
    VkPipelineStageFlags srcStageMask,
    VkPipelineStageFlags dstStageMask
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
    VkImageMemoryBarrier imgMemoryBarrier = {};
    imgMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imgMemoryBarrier.srcAccessMask = srcAccessMask;
    imgMemoryBarrier.dstAccessMask = dstAccessMask;
    imgMemoryBarrier.oldLayout = oldImgLayout;
    imgMemoryBarrier.newLayout = newImgLayout;
    imgMemoryBarrier.image = image;
    imgMemoryBarrier.subresourceRange =
    {
        aspectMask
        ,0 // baseMipLevel
        ,VK_REMAINING_MIP_LEVELS
        ,0 // baseArrayLayer
        ,VK_REMAINING_ARRAY_LAYERS
    };

    //If you are using the barrier to transfer queue family ownership, then these two fields
    //should be the indices of the queue families. They should be set to VK_QUEUE_FAMILY_IGNORED
    //if you don't want to do this (not the default value!).
    imgMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imgMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    VulkanDevice* pVulkanDev = pCmdBuffer->GetVulkanDevice();
    pVulkanDev->CmdPipelineBarrier(pCmdBuffer->GetCmdBuffer(),
        srcStageMask,
        dstStageMask,
        0, 0, nullptr, 0, nullptr, 1, &imgMemoryBarrier);
    if (!isCmdBufferRecordingActive)
    {
        CHECK_VK_RESULT(pCmdBuffer->EndCmdBuffer());
        CHECK_VK_RESULT(pCmdBuffer->ExelwteCmdBuffer(true, true));
    }
    return res;
}

VkResult VkUtil::CopyBuffer
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

)
{
    VkResult res = VK_SUCCESS;
    MASSERT(pCmdBuffer);
    const bool isCmdBufferActive = pCmdBuffer->GetRecordingBegun();
    if (!isCmdBufferActive)
    {
        CHECK_VK_RESULT(pCmdBuffer->BeginCmdBuffer());
    }

    VulkanDevice* pVulkanDev = pCmdBuffer->GetVulkanDevice();
    MASSERT((srcOffset + numBytes <= pSrcBuffer->GetSize()) &&
            (dstOffset + numBytes <= pDstBuffer->GetSize()));
    VkBufferCopy cpyRegion = { srcOffset, dstOffset, numBytes };
    pVulkanDev->CmdCopyBuffer(pCmdBuffer->GetCmdBuffer(),
        pSrcBuffer->GetBuffer(), pDstBuffer->GetBuffer(), 1, &cpyRegion);

    VkBufferMemoryBarrier bufferBarrier = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
    bufferBarrier.buffer = pDstBuffer->GetBuffer();
    bufferBarrier.offset = 0;
    bufferBarrier.size = pDstBuffer->GetSize();
    bufferBarrier.srcAccessMask = srcAccess;
    bufferBarrier.dstAccessMask = dstAccess;
    bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    pVulkanDev->CmdPipelineBarrier(pCmdBuffer->GetCmdBuffer(),
        srcStageMask,
        dstStageMask,
        0, 0, nullptr, 1, &bufferBarrier, 0, nullptr);

    if (!isCmdBufferActive)
    {
        CHECK_VK_RESULT(pCmdBuffer->EndCmdBuffer());
        CHECK_VK_RESULT(pCmdBuffer->ExelwteCmdBuffer(true, true));
    }
    return res;
}

VkResult VkUtil::CopyBuffer
(
    VulkanCmdBuffer *pCmdBuffer
    ,VulkanBuffer *pSrcBuffer
    ,VulkanBuffer *pDstBuffer
    ,VkAccessFlags srcAccess
    ,VkAccessFlags dstAccess
    ,VkPipelineStageFlags srcStageMask
    ,VkPipelineStageFlags dstStageMask

)
{
    MASSERT(pCmdBuffer && pSrcBuffer && pDstBuffer);

    return CopyBuffer(pCmdBuffer
                      ,pSrcBuffer
                      ,pDstBuffer
                      ,0 //srcOffset
                      ,0 //dstOffset
                      ,pDstBuffer->GetSize() // numBytes
                      ,srcAccess
                      ,dstAccess
                      ,srcStageMask
                      ,dstStageMask);
}
//--------------------------------------------------------------------
VkResult VkUtil::CopyBuffer
(
    VulkanCmdBuffer *pCmdBuffer,
    VulkanBuffer *pSrcBuffer,
    VulkanBuffer *pDstBuffer
)
{
    MASSERT(pCmdBuffer && pSrcBuffer && pDstBuffer);

    return CopyBuffer(pCmdBuffer
                      ,pSrcBuffer
                      ,pDstBuffer
                      ,0 //srcOffset
                      ,0 //dstOffset
                      ,pDstBuffer->GetSize() // numBytes
                      ,VK_ACCESS_TRANSFER_WRITE_BIT
                      ,VK_ACCESS_SHADER_READ_BIT
                      ,VK_PIPELINE_STAGE_TRANSFER_BIT
                      ,VK_PIPELINE_STAGE_VERTEX_SHADER_BIT);
}

//-------------------------------------------------------------------------------------------------
// Fill the buffer inline.
VkResult VkUtil::FillBuffer
(
    VulkanCmdBuffer *pCmdBuffer
    ,VulkanBuffer *pDstBuffer
    ,VkDeviceSize offset
    ,VkDeviceSize size
    ,UINT32 data
    ,VkAccessFlags srcAccess
    ,VkAccessFlags dstAccess
    ,VkPipelineStageFlags srcStageMask
    ,VkPipelineStageFlags dstStageMask
)
{
    VkResult res = VK_SUCCESS;
    MASSERT(pCmdBuffer);
    MASSERT(size % 4 == 0 || size == VK_WHOLE_SIZE);
    MASSERT(pDstBuffer->GetUsage() & VK_BUFFER_USAGE_TRANSFER_DST_BIT);

    const bool isCmdBufferActive = pCmdBuffer->GetRecordingBegun();
    if (!isCmdBufferActive)
    {
        CHECK_VK_RESULT(pCmdBuffer->BeginCmdBuffer());
    }
    VulkanDevice* pVulkanDev = pCmdBuffer->GetVulkanDevice();

    pVulkanDev->CmdFillBuffer(pCmdBuffer->GetCmdBuffer(),
                              pDstBuffer->GetBuffer(),
                              offset,
                              size,
                              data);

    VkBufferMemoryBarrier bufferBarrier = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
    bufferBarrier.buffer = pDstBuffer->GetBuffer();
    bufferBarrier.offset = 0;
    bufferBarrier.size = size;
    bufferBarrier.srcAccessMask = srcAccess;
    bufferBarrier.dstAccessMask = dstAccess;
    bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    pVulkanDev->CmdPipelineBarrier(pCmdBuffer->GetCmdBuffer(),
        srcStageMask,
        dstStageMask,
        0, 0, nullptr, 1, &bufferBarrier, 0, nullptr);

    if (!isCmdBufferActive)
    {
        CHECK_VK_RESULT(pCmdBuffer->EndCmdBuffer());
        CHECK_VK_RESULT(pCmdBuffer->ExelwteCmdBuffer(true, true));
    }
    return res;
}

//-------------------------------------------------------------------------------------------------
// Update the buffer inline.
// Note: Buffer updates first copy the data into command buffer memory when the command is recorded
// (which requires additional storage and may inlwr an additional allocation), and then copy the
// data from the command buffer into the dstBuffer when the command is exelwted on the device.
VkResult VkUtil::UpdateBuffer
(
    VulkanCmdBuffer *pCmdBuffer
    ,VulkanBuffer *pDstBuffer
    ,VkDeviceSize offset
    ,VkDeviceSize size
    ,void * pData
    ,VkAccessFlags srcAccess
    ,VkAccessFlags dstAccess
    ,VkPipelineStageFlags srcStageMask
    ,VkPipelineStageFlags dstStageMask
    ,SendBarrierType sendBarrier
)
{
    VkResult res = VK_SUCCESS;
    MASSERT(pCmdBuffer);
    const bool isCmdBufferActive = pCmdBuffer->GetRecordingBegun();
    if (!isCmdBufferActive)
    {
        CHECK_VK_RESULT(pCmdBuffer->BeginCmdBuffer());
    }
    VulkanDevice* pVulkanDev = pCmdBuffer->GetVulkanDevice();

    pVulkanDev->CmdUpdateBuffer(pCmdBuffer->GetCmdBuffer(),
                              pDstBuffer->GetBuffer(),
                              offset,
                              size,
                              pData);

    if (sendBarrier == SEND_BARRIER)
    {
        VkBufferMemoryBarrier bufferBarrier = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
        bufferBarrier.buffer = pDstBuffer->GetBuffer();
        bufferBarrier.offset = 0;
        bufferBarrier.size = size;
        bufferBarrier.srcAccessMask = srcAccess;
        bufferBarrier.dstAccessMask = dstAccess;
        bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        pVulkanDev->CmdPipelineBarrier(pCmdBuffer->GetCmdBuffer(),
            srcStageMask,
            dstStageMask,
            0, 0, nullptr, 1, &bufferBarrier, 0, nullptr);
    }

    if (!isCmdBufferActive)
    {
        CHECK_VK_RESULT(pCmdBuffer->EndCmdBuffer());
        CHECK_VK_RESULT(pCmdBuffer->ExelwteCmdBuffer(true, true));
    }
    return res;
}

//--------------------------------------------------------------------
VkResult VkUtil::CopyImage
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
)
{
    return CopyImage(pCmdBuffer, waitSemaphore,
        srcImage.GetImage(), &srcImageLwrrentLayout,
        &srcImageLwrrentAccessMask, srcImage.GetImageAspectFlags(),
        &srcImageLwrrentStageMask,
        srcImage.GetWidth(), srcImage.GetHeight(),
        dstImage.GetImage(), &dstImageLwrrentLayout,
        &dstImageLwrrentAccessMask, dstImage.GetImageAspectFlags(),
        &dstImageLwrrentStageMask);
}

//--------------------------------------------------------------------
VkResult VkUtil::CopyImage
(
    VulkanCmdBuffer *pCmdBuffer,
    VkSemaphore waitSemaphore,
    VkImage srcImage,
    VkImageLayout * pSrcImageLwrrentLayout,
    VkAccessFlags * pSrcImageLwrrentAccessMask,
    VkImageAspectFlags srcImageAspectFlags,
    VkPipelineStageFlags *pSrcImageLwrrentStageMask,
    UINT32 width,
    UINT32 height,
    VkImage dstImage,
    VkImageLayout *pDstImageLwrrentLayout,
    VkAccessFlags *pDstImageLwrrentAccessMask,
    VkImageAspectFlags dstImageAspectFlags,
    VkPipelineStageFlags *pDstImageLwrrentStageMask
)
{
    VkResult res = VK_SUCCESS;
    MASSERT(pCmdBuffer);
    const bool isCmdBufferRecordingActive = pCmdBuffer->GetRecordingBegun();
    if (!isCmdBufferRecordingActive)
    {
        CHECK_VK_RESULT(pCmdBuffer->BeginCmdBuffer());
    }

    if ((*pSrcImageLwrrentLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) ||
        (*pSrcImageLwrrentAccessMask != VK_ACCESS_TRANSFER_READ_BIT))
    {
        CHECK_VK_RESULT(SetImageLayout(pCmdBuffer,
            srcImage,
            srcImageAspectFlags,
            *pSrcImageLwrrentLayout,                //oldImgLayout
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,   //newImgLayout
            *pSrcImageLwrrentAccessMask,            //oldAccess
            VK_ACCESS_TRANSFER_READ_BIT,            //newAccess
            *pSrcImageLwrrentStageMask,
            VK_PIPELINE_STAGE_TRANSFER_BIT));
        *pSrcImageLwrrentLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        *pSrcImageLwrrentAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        *pSrcImageLwrrentStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }

    if ((*pDstImageLwrrentLayout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) ||
        (*pDstImageLwrrentAccessMask != VK_ACCESS_TRANSFER_WRITE_BIT))
    {
        CHECK_VK_RESULT(SetImageLayout(pCmdBuffer,
            dstImage,
            dstImageAspectFlags,
            *pDstImageLwrrentLayout,                //old layout
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   //new layout
            *pDstImageLwrrentAccessMask,            //old access
            VK_ACCESS_TRANSFER_WRITE_BIT,           //new access
            *pDstImageLwrrentStageMask,             //src stage
            VK_PIPELINE_STAGE_TRANSFER_BIT));       //dst stage
        *pDstImageLwrrentLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        *pDstImageLwrrentAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        *pDstImageLwrrentStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }

    VkImageCopy cpyRegion = {};
    cpyRegion.srcSubresource.aspectMask = srcImageAspectFlags;
    cpyRegion.dstSubresource.aspectMask = dstImageAspectFlags;
    cpyRegion.srcSubresource.layerCount = 1;
    cpyRegion.dstSubresource.layerCount = 1;
    cpyRegion.extent.width = width;
    cpyRegion.extent.height = height;
    cpyRegion.extent.depth = 1;

    VulkanDevice* pVulkanDev = pCmdBuffer->GetVulkanDevice();
    pVulkanDev->CmdCopyImage(pCmdBuffer->GetCmdBuffer(),
        srcImage,  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        dstImage,  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &cpyRegion);

    if (!isCmdBufferRecordingActive)
    {
        CHECK_VK_RESULT(pCmdBuffer->EndCmdBuffer());
        CHECK_VK_RESULT(pCmdBuffer->ExelwteCmdBuffer(waitSemaphore, 0, NULL,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, true, VK_NULL_HANDLE, false, true));
    }
    return VK_SUCCESS;
}

const char* VkUtil::SurfaceTypeToString(VkUtil::SurfaceCheck surfType)
{
    switch (surfType)
    {
        case VkUtil::CHECK_COLOR_SURFACE:
            return "color";
        case VkUtil::CHECK_DEPTH_SURFACE:
            return "depth";
        case VkUtil::CHECK_STENCIL_SURFACE:
            return "stencil";
        case VkUtil::CHECK_COMPUTE_FP16:
            return "compute_FP16";
        case VkUtil::CHECK_COMPUTE_FP32:
            return "compute_FP32";
        case VkUtil::CHECK_COMPUTE_FP64:
            return "compute_FP64";
        case VkUtil::CHECK_RT_SURFACE:
            return "rtColor";
        default:
            MASSERT(!"Unknown SurfaceCheck type");
            return "unknown";
    }
}

VkUtil::DebugMarker::DebugMarker(float c1, float c2, float c3, float c4)
: m_Enabled(false)
, m_VulkanDevice(nullptr)
, m_CommandBuffer(nullptr)
, m_MarkerInfoExt( {
    VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT,
    nullptr,
    m_Name,
    {c1, c2, c3, c4} } )
{
}

VkUtil::DebugMarker::~DebugMarker()
{
    End();
}

void VkUtil::DebugMarker::Begin
(
    bool enable,
    VulkanDevice *pVulkanDevice,
    VkCommandBuffer commandBuffer,
    const char *nameFormat,
    ...
)
{
    m_Enabled = enable;

    if (!m_Enabled)
    {
        return;
    }

    m_VulkanDevice = pVulkanDevice;
    m_CommandBuffer = commandBuffer;

    va_list arguments;
    va_start(arguments, nameFormat);
    vsnprintf(m_Name, sizeof(m_Name), nameFormat, arguments);
    va_end(arguments);

    m_VulkanDevice->CmdDebugMarkerBeginEXT(m_CommandBuffer, &m_MarkerInfoExt);
}

void VkUtil::DebugMarker::End()
{
    if (m_Enabled)
    {
        m_VulkanDevice->CmdDebugMarkerEndEXT(m_CommandBuffer);
        m_Enabled = false;
    }
}
