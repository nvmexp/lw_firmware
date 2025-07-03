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

#include "vktexfill.h"

#include "vkdev.h"
#include "vktexture.h"
#include "vkutil.h"

#include <algorithm>

/* TODO:
 *
 * 1. Implement ComputeTexFiller
 * 2. Change PatternTexFiller to use multiple host textures and semaphores to
 *    speed it up.
 * 3. Run one of the Fillers in a detached thread so that we can execute
 *    PatternTexFiller and ComputeTexFiller conlwrrently.
 * 3. Do queue family ownership transfers when sharing VulkanImages across
 *    different queues (e.g. compute and transfer). We are not doing it right
 *    now because the LWPU driver lets us get away with it and the validation
 *    layers are somehow missing this. Need to change VulkanImage slightly...
 * 4. Support mip-maps via vkCmdBlitImage()
 * 5. Support block texture compression (e.g. VK_FORMAT_BC1_RGBA_SRGB_BLOCK)
 * 6. Support all of the formats in color.cpp:ColorFormatTable[] that we can
 * 7. Support 3D textures
 * 8. Support Array & LwbeMap textures
 * 9. Support Multisample textures
 * 10. Specify a starting index into the VulkanTexture vector with the number of
 *     textures to fill.
 *
 */

//-----------------------------------------------------------------------------
VulkanTextureFillerStrategy::VulkanTextureFillerStrategy(VulkanDevice* pVulkanDev) :
    m_pVulkanDev(pVulkanDev)
{
}

//-----------------------------------------------------------------------------
VulkanTextureFillerStrategy::VulkanTextureFillerStrategy
(
    VulkanDevice* pVulkanDev
   ,UINT32 seed
) :
    m_pVulkanDev(pVulkanDev)
   ,m_Seed(seed)
{
}

//-----------------------------------------------------------------------------
PatternTexFiller::PatternTexFiller(VulkanDevice* pVulkanDev) :
    VulkanTextureFillerStrategy(pVulkanDev)
{
}

//-----------------------------------------------------------------------------
PatternTexFiller::PatternTexFiller(VulkanDevice* pVulkanDev, UINT32 seed) :
    VulkanTextureFillerStrategy(pVulkanDev, seed)
{
}

//-----------------------------------------------------------------------------
RC PatternTexFiller::Setup
(
    UINT32 transferQueueIdx,
    UINT32 graphicsQueueIdx,
    UINT32 computeQueueIdx  //not used
)
{
    RC rc;

    m_PatternClass.FillRandomPattern(m_Seed);

    m_CopyCmdPool = make_unique<VulkanCmdPool>(m_pVulkanDev);
    m_CopyCmdBuf = make_unique<VulkanCmdBuffer>(m_pVulkanDev);
    CHECK_VK_TO_RC(m_CopyCmdPool->InitCmdPool(
        m_pVulkanDev->GetPhysicalDevice()->GetTransferQueueFamilyIdx(),
        transferQueueIdx));
    CHECK_VK_TO_RC(m_CopyCmdBuf->AllocateCmdBuffer(m_CopyCmdPool.get()));

    m_GraphicsCmdPool = make_unique<VulkanCmdPool>(m_pVulkanDev);
    m_GraphicsCmdBuf = make_unique<VulkanCmdBuffer>(m_pVulkanDev);
    CHECK_VK_TO_RC(m_GraphicsCmdPool->InitCmdPool(
        m_pVulkanDev->GetPhysicalDevice()->GetGraphicsQueueFamilyIdx(),
        graphicsQueueIdx));
    CHECK_VK_TO_RC(m_GraphicsCmdBuf->AllocateCmdBuffer(m_GraphicsCmdPool.get()));

    return rc;
}

//-----------------------------------------------------------------------------
void PatternTexFiller::Cleanup()
{
    m_CopyCmdBuf->FreeCmdBuffer();
    m_CopyCmdPool->DestroyCmdPool();
    m_GraphicsCmdBuf->FreeCmdBuffer();
    m_GraphicsCmdPool->DestroyCmdPool();
}

//-----------------------------------------------------------------------------
// Creates a host texture that is filled by the PatternClass. We will then
// copy one texture at a time over to the GPU.
RC PatternTexFiller::Fill(const vector<VulkanTexture*>& textures)
{
    MASSERT(!textures.empty());
    RC rc;

    CHECK_VK_TO_RC(AcquireTextures(textures));
    DEFER
    {
        ReleaseTextures(textures);
    };

    // Create a texture in sysmem that we will fill with PatternClass
    auto hostTex = make_unique<VulkanTexture>(m_pVulkanDev);
    hostTex->SetFormatPixelSizeBytes(4);

    UINT32 texIdx = 0;
    for (const auto& pTex : textures)
    {
        // If new texture is different size from old one, resize the host texture
        if ((pTex->GetFormat() != hostTex->GetFormat()) ||
            (pTex->GetWidth() != hostTex->GetWidth()) ||
            (pTex->GetHeight() != hostTex->GetHeight()))
        {
            hostTex->Reset(m_pVulkanDev);
            hostTex->SetTextureDimensions(pTex->GetWidth(), pTex->GetHeight(), 1);
            hostTex->SetFormat(pTex->GetFormat());

            CHECK_VK_TO_RC(hostTex->AllocateImage(
                VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                ,pTex->GetTexImage().GetImageCreateFlags()
                ,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                ,VK_IMAGE_LAYOUT_PREINITIALIZED
                ,VK_IMAGE_TILING_LINEAR
                ,"HostTexture"));
            if (!hostTex->IsSamplingSupported())
            {
                return RC::MODS_VK_ERROR_FORMAT_NOT_SUPPORTED;
            }
            CHECK_VK_TO_RC(hostTex->GetTexImage().SetImageLayout(
                m_CopyCmdBuf.get()
                ,hostTex->GetTexImage().GetImageAspectFlags()
                ,VK_IMAGE_LAYOUT_GENERAL
                ,VK_ACCESS_HOST_WRITE_BIT
                ,VK_PIPELINE_STAGE_HOST_BIT));
        }

        if (m_UseRandomData)
        {
            CHECK_VK_TO_RC(hostTex->FillRandom(m_Seed + 757 * texIdx++));
        }
        else
        {
            CHECK_VK_TO_RC(hostTex->FillTexture(m_PatternClass, -1)); // increments PatternClass
        }
        CHECK_VK_TO_RC(pTex->CopyTexture(hostTex.get(), m_CopyCmdBuf.get()));
    }

    return rc;
}

//-----------------------------------------------------------------------------
VkResult PatternTexFiller::AcquireTextures(const vector<VulkanTexture*>& textures)
{
    VkResult res = VK_SUCCESS;

    const VulkanDevice* pVulkanDev = m_CopyCmdBuf->GetVulkanDevice();
    const VulkanPhysicalDevice* pPhysDev = pVulkanDev->GetPhysicalDevice();

    // Switch ownership from the graphics queue family to the transfer queue family
    // See the "Queue Family Ownership Transfer" section of the Vulkan spec.
    CHECK_VK_RESULT(m_GraphicsCmdBuf->BeginCmdBuffer());
    CHECK_VK_RESULT(m_CopyCmdBuf->BeginCmdBuffer());
    DEFER
    {
        // It is safe to reset a command buffer that has already been reset
        m_GraphicsCmdBuf->ResetCmdBuffer(
            VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
        m_CopyCmdBuf->ResetCmdBuffer(
            VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
    };

    VkImageMemoryBarrier imgMemoryBarrier = {};
    imgMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imgMemoryBarrier.srcAccessMask = 0;
    imgMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    imgMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imgMemoryBarrier.subresourceRange =
    {
        VK_IMAGE_ASPECT_COLOR_BIT
        ,0 // baseMipLevel
        ,VK_REMAINING_MIP_LEVELS
        ,0 // baseArrayLayer
        ,VK_REMAINING_ARRAY_LAYERS
    };
    imgMemoryBarrier.srcQueueFamilyIndex = pPhysDev->GetGraphicsQueueFamilyIdx();
    imgMemoryBarrier.dstQueueFamilyIndex = pPhysDev->GetTransferQueueFamilyIdx();

    for (const auto& pTex : textures)
    {
        auto& texImage = pTex->GetTexImage();
        imgMemoryBarrier.oldLayout = texImage.GetImageLayout();
        imgMemoryBarrier.image = texImage.GetImage();

        CHECK_VK_RESULT(texImage.SetImageBarrier(imgMemoryBarrier,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            m_GraphicsCmdBuf.get()));

        CHECK_VK_RESULT(texImage.SetImageBarrier(imgMemoryBarrier,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            m_CopyCmdBuf.get()));
    }

    // Release from graphics queue
    CHECK_VK_RESULT(m_GraphicsCmdBuf->EndCmdBuffer());
    CHECK_VK_RESULT(m_GraphicsCmdBuf->ExelwteCmdBuffer(
        VK_NULL_HANDLE, 0, nullptr, 0, true, VK_NULL_HANDLE));
    m_GraphicsCmdBuf->ResetCmdBuffer(
        VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);

    // Acquire with transfer queue
    CHECK_VK_RESULT(m_CopyCmdBuf->EndCmdBuffer());
    CHECK_VK_RESULT(m_CopyCmdBuf->ExelwteCmdBuffer(
        VK_NULL_HANDLE, 0, nullptr, 0, true, VK_NULL_HANDLE));
    m_CopyCmdBuf->ResetCmdBuffer(
        VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);

    return res;
}

//-----------------------------------------------------------------------------
VkResult PatternTexFiller::ReleaseTextures(const vector<VulkanTexture*>& textures)
{
    VkResult res = VK_SUCCESS;

    const VulkanDevice* pVulkanDev = m_CopyCmdBuf->GetVulkanDevice();
    const VulkanPhysicalDevice* pPhysDev = pVulkanDev->GetPhysicalDevice();

    // Switch ownership from the transfer queue family to the graphics queue family
    // See the "Queue Family Ownership Transfer" section of the Vulkan spec.
    CHECK_VK_RESULT(m_GraphicsCmdBuf->BeginCmdBuffer());
    CHECK_VK_RESULT(m_CopyCmdBuf->BeginCmdBuffer());
    DEFER
    {
        // It is safe to reset a command buffer that has already been reset
        m_GraphicsCmdBuf->ResetCmdBuffer(
            VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
        m_CopyCmdBuf->ResetCmdBuffer(
            VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
    };

    VkImageMemoryBarrier imgMemoryBarrier = {};
    imgMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imgMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    imgMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    imgMemoryBarrier.subresourceRange =
    {
        VK_IMAGE_ASPECT_COLOR_BIT
        ,0 // baseMipLevel
        ,VK_REMAINING_MIP_LEVELS
        ,0 // baseArrayLayer
        ,VK_REMAINING_ARRAY_LAYERS
    };
    imgMemoryBarrier.srcQueueFamilyIndex = pPhysDev->GetTransferQueueFamilyIdx();
    imgMemoryBarrier.dstQueueFamilyIndex = pPhysDev->GetGraphicsQueueFamilyIdx();

    for (const auto& pTex : textures)
    {
        auto& texImage = pTex->GetTexImage();
        imgMemoryBarrier.image = texImage.GetImage();
        imgMemoryBarrier.oldLayout = texImage.GetImageLayout();
        imgMemoryBarrier.newLayout = texImage.GetImageLayout();

        CHECK_VK_RESULT(texImage.SetImageBarrier(imgMemoryBarrier,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            m_CopyCmdBuf.get()));

        CHECK_VK_RESULT(texImage.SetImageBarrier(imgMemoryBarrier,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            m_GraphicsCmdBuf.get()));
    }

    // Release from transfer queue
    CHECK_VK_RESULT(m_CopyCmdBuf->EndCmdBuffer());
    CHECK_VK_RESULT(m_CopyCmdBuf->ExelwteCmdBuffer(
        VK_NULL_HANDLE, 0, nullptr, 0, true, VK_NULL_HANDLE));
    m_CopyCmdBuf->ResetCmdBuffer(
        VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);

    // Acquire with graphics queue
    CHECK_VK_RESULT(m_GraphicsCmdBuf->EndCmdBuffer());
    CHECK_VK_RESULT(m_GraphicsCmdBuf->ExelwteCmdBuffer(
        VK_NULL_HANDLE, 0, nullptr, 0, true, VK_NULL_HANDLE));
    m_GraphicsCmdBuf->ResetCmdBuffer(
        VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);

    // Switch all the textures to VK_IMAGE_LAYOUT_GENERAL
    CHECK_VK_RESULT(m_GraphicsCmdBuf->BeginCmdBuffer());
    imgMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imgMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imgMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    for (const auto& pTex : textures)
    {
        auto& texImage = pTex->GetTexImage();
        imgMemoryBarrier.image = texImage.GetImage();
        imgMemoryBarrier.oldLayout = texImage.GetImageLayout();
        imgMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        imgMemoryBarrier.srcAccessMask = texImage.GetImageAccessMask();

        CHECK_VK_RESULT(texImage.SetImageBarrier(imgMemoryBarrier,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
            m_GraphicsCmdBuf.get()));
    }
    CHECK_VK_RESULT(m_GraphicsCmdBuf->EndCmdBuffer());
    CHECK_VK_RESULT(m_GraphicsCmdBuf->ExelwteCmdBuffer(
        VK_NULL_HANDLE, 0, nullptr, 0, true, VK_NULL_HANDLE));
    m_GraphicsCmdBuf->ResetCmdBuffer(
        VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);

    return res;
}

//-----------------------------------------------------------------------------
ComputeTexFiller::ComputeTexFiller(VulkanDevice* pVulkanDev) :
    VulkanTextureFillerStrategy(pVulkanDev)
{
}

//-----------------------------------------------------------------------------
ComputeTexFiller::ComputeTexFiller(VulkanDevice* pVulkanDev, UINT32 seed) :
    VulkanTextureFillerStrategy(pVulkanDev, seed)
{
}

//-----------------------------------------------------------------------------
RC ComputeTexFiller::Setup
(
    UINT32 transferQueueIdx,    //not used
    UINT32 graphicsQueueIdx,    //not used
    UINT32 computeQueueIdx
)
{
    RC rc;
    //TODO: This class is not implemented and not lwrrently usable. VkStress is
    //      responsible for allocating the required number of compute queues during
    //      device creation (see VerifyQueueRequirements()). You will need to account
    //      for the additional compute queue in there before you try to create a pool
    //      in here.
    //m_ComputeCmdPool = make_unique<VulkanCmdPool>(m_pVulkanDev);
    //
    //CHECK_VK_TO_RC(m_ComputeCmdPool->InitCmdPool(
    //    m_pVulkanDev->GetPhysicalDevice()->GetComputeQueueFamilyIdx(),
    //    computeQueueIdx));

    return rc;
}

//-----------------------------------------------------------------------------
void ComputeTexFiller::Cleanup()
{
    // See comments in Setup()
    //m_ComputeCmdPool->DestroyCmdPool();
}

//-----------------------------------------------------------------------------
RC ComputeTexFiller::Fill(const vector<VulkanTexture*>& textures)
{
    MASSERT(!"VulkanTexture fill via compute is not implemented yet\n");
    return RC::SOFTWARE_ERROR;
}

//-----------------------------------------------------------------------------
VulkanTextureFiller::~VulkanTextureFiller()
{
    Cleanup();
}

//-----------------------------------------------------------------------------
RC VulkanTextureFiller::Setup
(
    VulkanDevice* pVulkanDev,
    UINT32 transferQueueIdx,
    UINT32 graphicsQueueIdx,
    UINT32 computeQueueIdx,
    UINT32 seed,
    bool bUseRandomData
)
{
    RC rc;

    m_PatternFiller = make_unique<PatternTexFiller>(pVulkanDev, seed);
    CHECK_RC(m_PatternFiller->Setup(transferQueueIdx, graphicsQueueIdx, computeQueueIdx));
    m_PatternFiller->SetUseRandomData(bUseRandomData);

    m_ComputeFiller = make_unique<ComputeTexFiller>(pVulkanDev, seed);
    CHECK_RC(m_ComputeFiller->Setup(transferQueueIdx, graphicsQueueIdx, computeQueueIdx));

    return rc;
}

//-----------------------------------------------------------------------------
void VulkanTextureFiller::Cleanup()
{
    if (m_PatternFiller)
    {
        m_PatternFiller->Cleanup();
        m_PatternFiller = nullptr;
    }
    if (m_ComputeFiller)
    {
        m_ComputeFiller->Cleanup();
        m_ComputeFiller = nullptr;
    }
    m_pVulkanDev = nullptr;
}

//-----------------------------------------------------------------------------
RC VulkanTextureFiller::Fill(const vector<unique_ptr<VulkanTexture>>& textures)
{
    RC rc;

    vector<VulkanTexture*> bigTextures;
    bigTextures.reserve(textures.size());
    vector<VulkanTexture*> smallTextures;
    smallTextures.reserve(textures.size());

    for (const auto& pTex : textures)
    {
        if (pTex->GetSizeBytes() >= m_MinComputeTxSize)
        {
            bigTextures.push_back(pTex.get());
        }
        else
        {
            smallTextures.push_back(pTex.get());
        }
    }

    // Sort textures by size to minimize host texture reallocations
    auto SortTexturesBySize = [](vector<VulkanTexture*>& textures) -> void
    {
        std::sort(textures.begin(), textures.end(),
            [](VulkanTexture*& tex1, VulkanTexture*& tex2)
            {
                return tex1->GetSizeBytes() < tex2->GetSizeBytes();
            });
    };

    if (!bigTextures.empty())
    {
        SortTexturesBySize(bigTextures);
        CHECK_RC(m_ComputeFiller->Fill(bigTextures));
    }
    if (!smallTextures.empty())
    {
        SortTexturesBySize(smallTextures);
        CHECK_RC(m_PatternFiller->Fill(smallTextures));
    }

    return rc;
}

