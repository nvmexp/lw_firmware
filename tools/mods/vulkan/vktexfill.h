/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "vkmain.h"
#include "vkcmdbuffer.h"
#include "vktexture.h"
#include "core/utility/ptrnclss.h"

class VulkanDevice;

namespace
{
    constexpr UINT32 DEFAULT_SEED = 0x12345678;

    // TODO: Set DEFAULT_COMPUTE_TX_SIZE to a reasonable value
    // It is set to a large number to avoid using compute to fill textures
    constexpr UINT64 DEFAULT_COMPUTE_TX_SIZE = 0xFFFFFFFFFFFFFFFF;
}

//! \brief Interface for filling a VulkanTexture
class VulkanTextureFillerStrategy
{
public:
    explicit VulkanTextureFillerStrategy(VulkanDevice* pVulkanDev);
    VulkanTextureFillerStrategy(VulkanDevice* pVulkanDev, UINT32 seed);
    virtual ~VulkanTextureFillerStrategy() = default;
    virtual RC Setup(UINT32 transferQueueIdx, UINT32 graphicsQueueIdx, UINT32 computeQueueIdx) = 0;
    virtual void Cleanup() = 0;

    virtual RC Fill(const vector<VulkanTexture*>& textures) = 0;

protected:
    VulkanDevice* m_pVulkanDev;
    const UINT32 m_Seed = DEFAULT_SEED;
};

//! \brief Fills VulkanTextures via PatternClass
class PatternTexFiller final : public VulkanTextureFillerStrategy
{
public:
    explicit PatternTexFiller(VulkanDevice* pVulkanDev);
    PatternTexFiller(VulkanDevice* pVulkanDev, UINT32 seed);
    RC Setup(UINT32 transferQueueIdx, UINT32 graphicsQueueIdx, UINT32 computeQueueIdx) override;
    void Cleanup() override;

    RC Fill(const vector<VulkanTexture*>& textures) override;

    SETGET_PROP(UseRandomData, bool);

private:
    PatternClass m_PatternClass;
    unique_ptr<VulkanCmdPool> m_CopyCmdPool = nullptr;
    unique_ptr<VulkanCmdBuffer> m_CopyCmdBuf = nullptr;

    // We need access to the graphics queue to transition image layouts
    // to SHADER_READ_ONLY_OPTIMAL and also for vkCmdBlitImage().
    unique_ptr<VulkanCmdPool> m_GraphicsCmdPool = nullptr;
    unique_ptr<VulkanCmdBuffer> m_GraphicsCmdBuf = nullptr;
    bool m_UseRandomData = false;

    // Move the textures from graphics queue to transfer queue
    VkResult AcquireTextures(const vector<VulkanTexture*>& textures);

    // Move the textures from transfer queue back to graphics queue
    // and also make the textures VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    VkResult ReleaseTextures(const vector<VulkanTexture*>& textures);
};

//! \brief Fills large VulkanTextures via GLSL compute shader
class ComputeTexFiller final : public VulkanTextureFillerStrategy
{
public:
    explicit ComputeTexFiller(VulkanDevice* pVulkanDev);
    ComputeTexFiller(VulkanDevice* pVulkanDev, UINT32 seed);
    RC Setup(UINT32 transferQueueIdx, UINT32 graphicsQueueIdx, UINT32 computeQueueIdx) override;
    void Cleanup() override;

    RC Fill(const vector<VulkanTexture*>& textures) override;

private:
    unique_ptr<VulkanCmdPool> m_ComputeCmdPool = nullptr;
};

// \brief Fills VulkanTexture objects with some data
// Uses PatternClass to fill small textures and a compute shader to fill larger ones
class VulkanTextureFiller
{
public:
    VulkanTextureFiller() = default;
    ~VulkanTextureFiller();
    RC Setup
    (
        VulkanDevice* pVulkanDev,
        UINT32 transferQueueIdx,    // used by m_PatternFiller
        UINT32 graphicsQueueIdx,    // used by m_PatternFiller
        UINT32 computeQueueIdx,     // used by m_ComputeFiller
        UINT32 seed = DEFAULT_SEED,
        bool bUseRandomData = false
    );
    void Cleanup();

    RC Fill(const vector<unique_ptr<VulkanTexture>>& textures);

    SETGET_PROP(MinComputeTxSize, UINT64);

private:
    VulkanDevice* m_pVulkanDev = nullptr;

    unique_ptr<PatternTexFiller> m_PatternFiller = nullptr;
    unique_ptr<ComputeTexFiller> m_ComputeFiller = nullptr;

    UINT64 m_MinComputeTxSize = DEFAULT_COMPUTE_TX_SIZE;
};

