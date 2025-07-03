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
#ifndef VK_TEXTURE_H
#define VK_TEXTURE_H

#include "vkmain.h"
#include "vkimage.h"
#include "util.h"
#include "core/utility/ptrnclss.h"

class VulkanDevice;

//--------------------------------------------------------------------
class VulkanTexture
{
public:
    enum TextureType
    {
        TEX_1D,
        TEX_2D,
        TEX_3D
    };

    explicit VulkanTexture(VulkanDevice* pVulkanDev);
    ~VulkanTexture();

    VkResult AllocateImage
    (
        VkImageUsageFlags usageFlags,
        VkImageCreateFlags createFlags,
        VkMemoryPropertyFlags memoryFlag,
        VkImageLayout layout,
        VkImageTiling tiling,
        const char * imageName
    );

    bool        IsSamplingSupported() const;
    VkResult    SetDataRaw(void *data, VkDeviceSize size, VkDeviceSize offset);
    VkResult    GetData(void *data, VkDeviceSize size, VkDeviceSize offset);
    VkResult    FillRandom(UINT32 seed);
    VkResult    FillTexture();
    VkResult    FillTexture(PatternClass & rPattern, INT32 patIdx);
    VkResult    PrintTexture();
    VkDescriptorImageInfo GetDescriptorImageInfo() const;
    void        Reset(VulkanDevice* pVulkanDev);
    void        Cleanup();
    UINT64      GetSizeBytes() const;
    UINT64      GetPitch() const;
    void        SetUseMipMap(const bool useMipMap);

    // Note: CopyTexture will copy the image data, but you will probably need
    // an additional memory barrier to switch to the final desired image layout
    // such as VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL.
    VkResult    CopyTexture(VulkanTexture *pSrcTexture,
                            VulkanCmdBuffer *pCmdBuffer);

    VkResult    CopyTexture_TestMipMap(VulkanTexture *pSrcTexture,
                                        VulkanCmdBuffer *pCmdBuffer);

    void        SetTextureDimensions
    (
        const UINT32 width,
        const UINT32 height,
        const UINT32 depth
    );
    VkImage     GetVkImage() const;
    VulkanImage & GetTexImage();

    static void SetTestMipMapMode(bool overrideTextureMapping);

    SETGET_PROP(Width,         UINT32);
    SETGET_PROP(Height,        UINT32);
    SETGET_PROP(Depth,         UINT32);
    SETGET_PROP(Format,        VkFormat);
    SETGET_PROP(FormatPixelSizeBytes, UINT32);
    SETGET_PROP(VkSampler,     VkSampler);

private:
    VulkanDevice                *m_pVulkanDev = nullptr;
    VkSampler                    m_VkSampler = VK_NULL_HANDLE;
    VulkanImage                  m_TexImage;

    UINT32                       m_Width = 0;
    UINT32                       m_Height = 0;
    UINT32                       m_Depth = 0;
    VkFormat                     m_Format = VK_FORMAT_UNDEFINED;
    UINT32                       m_FormatPixelSizeBytes = 0;
    vector<UINT08>               m_Data;
    bool                         m_UseMipMap = false;
    static bool                  m_OverrideTextureMapping/* = false*/;

    VkGeometry                   m_Geometry;

    VkResult SetData(VkFormat dataFormat);
};

#endif
