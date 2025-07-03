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

#include "vkgoldensurfaces.h"
#include "vkcmdbuffer.h"
#include "vkinstance.h"
#include "vkutil.h"
#include "util.h"
#ifndef VULKAN_STANDALONE
#include "core/include/tasker_p.h"
#endif

VulkanGoldenSurfaces::VulkanGoldenSurfaces(VulkanInstance* pVkInst)
: m_pVulkanInst(pVkInst)
{
}

VulkanGoldenSurfaces::~VulkanGoldenSurfaces()
{
    Cleanup();
}

void VulkanGoldenSurfaces::Cleanup()
{
    m_VulkanGoldenSurfaces.clear();
    m_WaitSemaphore = VK_NULL_HANDLE;
}

void VulkanGoldenSurfaces::SelectWaitSemaphore(VkSemaphore waitSemaphore)
{
    m_WaitSemaphore = waitSemaphore;
}
//-------------------------------------------------------------------------------------------------
// Allow user to overide the CmdBuffers that were specified when the surface was added. This is
// mainly for debugging purposes.
void VulkanGoldenSurfaces::SetCmdBuffers
(
    UINT32 surfNum,
    VulkanCmdBuffer *pCmdBuffer,
    VulkanCmdBuffer * pTransCmdBuffer
)
{
    m_VulkanGoldenSurfaces[surfNum].pCmdBuffer = pCmdBuffer;
    m_VulkanGoldenSurfaces[surfNum].pTransCmdBuffer = pTransCmdBuffer;
}

//-------------------------------------------------------------------------------------------------
// There are multiple constraints for Depth/Stencil images.
// 1. We can't allocate HOST_VISIBLE memory for a Depth/Stencil image.
// 2. The memory requirements are DEVICE_LOCAL and we can't allocate & bind memory that is
//    botn DEVICE_LOCAL & HOST_VISIBLE,
// 3. LWPU Vulkan driver doesn't support clearing of linear depth/stencil images. And since
//    the VK_IMAGE_USAGE_TRANSFER_DST_BIT usage can be used for clearing, they report that as
//    unsupported.
// 4. You can't allocate & bind HOST memory with optimal tiling
// So we are going to use a transfer buffer instead of a transfer image to move the data to host
// memory.
VkResult VulkanGoldenSurfaces::AddSurface
(
    const string &name
    ,VulkanImage * srcImage
    ,VulkanCmdBuffer *pCmdBuffer
    ,VulkanCmdBuffer *pTransCmdBuffer
)
{
    VkResult res = VK_SUCCESS;

    m_VulkanGoldenSurfaces.emplace_back(VulkanGoldenSurface());

    auto &vulkanGoldenSurface = *m_VulkanGoldenSurfaces.rbegin();

    vulkanGoldenSurface.name = name;
    vulkanGoldenSurface.srcImage = srcImage;
    vulkanGoldenSurface.transferBuffer =
        make_unique<VulkanBuffer>(m_pVulkanInst->GetVulkanDevice());
    vulkanGoldenSurface.pCmdBuffer = pCmdBuffer;
    vulkanGoldenSurface.pTransCmdBuffer = pTransCmdBuffer ? pTransCmdBuffer : pCmdBuffer;
    auto &tb = *vulkanGoldenSurface.transferBuffer;

    // We can't copy the compressed depth/stencil format from the image to a buffer.
    // So we have to allocate separate sections for this in the transfer buffer.
    // Also even though the depth component is only 24 bits we have to allocate 32 for the
    // transfer.
    UINT32 bpp = (srcImage->GetFormat() == VK_FORMAT_D24_UNORM_S8_UINT) ? 5 : 4;
    const UINT32 sizeBytes = srcImage->GetWidth() * srcImage->GetHeight() * bpp;
    CHECK_VK_RESULT(tb.CreateBuffer(VK_BUFFER_USAGE_TRANSFER_DST_BIT, sizeBytes));
    CHECK_VK_RESULT(tb.AllocateAndBindMemory(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
    return res;
}

//-------------------------------------------------------------------------------------------------
int VulkanGoldenSurfaces::NumSurfaces() const
{
    return static_cast<int>(m_VulkanGoldenSurfaces.size());
}

//-------------------------------------------------------------------------------------------------
const string & VulkanGoldenSurfaces::Name(int surfNum) const
{
    MASSERT(surfNum < NumSurfaces());
    return m_VulkanGoldenSurfaces[surfNum].name;
}

//-------------------------------------------------------------------------------------------------
RC VulkanGoldenSurfaces::CheckAndReportDmaErrors(UINT32 subdevNum)
{
    return RC::SOFTWARE_ERROR;
}

//-------------------------------------------------------------------------------------------------
VkResult VulkanGoldenSurfaces::CopyImageToBuffer(int surfNum, VkImageAspectFlags aspectMask)
{
    VkResult res = VK_SUCCESS;
    auto &vgs = m_VulkanGoldenSurfaces[surfNum];
    VulkanDevice* pVulkanDev = vgs.pCmdBuffer->GetVulkanDevice();

    VkBufferImageCopy bufferImageCopy[2] = { { }, { } };
    bufferImageCopy[0].imageExtent.width = vgs.srcImage->GetWidth();
    bufferImageCopy[0].imageExtent.height = vgs.srcImage->GetHeight();
    bufferImageCopy[0].imageExtent.depth = 1;
    bufferImageCopy[0].imageSubresource.layerCount = 1;
    bufferImageCopy[0].bufferOffset = 0;
    bufferImageCopy[0].imageSubresource.aspectMask =
        (aspectMask & VK_IMAGE_ASPECT_COLOR_BIT) ? aspectMask : VK_IMAGE_ASPECT_DEPTH_BIT;

    bufferImageCopy[1].imageExtent.width = vgs.srcImage->GetWidth();
    bufferImageCopy[1].imageExtent.height = vgs.srcImage->GetHeight();
    bufferImageCopy[1].imageExtent.depth = 1;
    bufferImageCopy[1].imageSubresource.layerCount = 1;
    bufferImageCopy[1].bufferOffset = vgs.srcImage->GetWidth() * vgs.srcImage->GetHeight() * 4;
    bufferImageCopy[1].imageSubresource.aspectMask =
        (aspectMask & VK_IMAGE_ASPECT_COLOR_BIT) ? aspectMask : VK_IMAGE_ASPECT_STENCIL_BIT;

    const bool isCmdBufferRecordingActive = vgs.pTransCmdBuffer->GetRecordingBegun();
    if (!isCmdBufferRecordingActive)
    {
        CHECK_VK_RESULT(vgs.pTransCmdBuffer->BeginCmdBuffer());
    }

    pVulkanDev->CmdCopyImageToBuffer(vgs.pTransCmdBuffer->GetCmdBuffer()
                                     ,vgs.srcImage->GetImage()
                                     ,vgs.srcImage->GetImageLayout()
                                     ,vgs.transferBuffer->GetBuffer()
                                     ,(aspectMask & VK_IMAGE_ASPECT_COLOR_BIT) ? 1 : 2
                                     ,bufferImageCopy);
    if (!isCmdBufferRecordingActive)
    {
        CHECK_VK_RESULT(vgs.pTransCmdBuffer->EndCmdBuffer());
        CHECK_VK_RESULT(vgs.pTransCmdBuffer->ExelwteCmdBuffer(m_WaitSemaphore, 0, nullptr,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, true, VK_NULL_HANDLE, false, true));
    }
    return res;
}

//-------------------------------------------------------------------------------------------------
// Return the address of the cached surface for examination. If the surface has not been cached
// into a local buffer then copy the GPU's surface into the cache.
// Note: This routine must run in sub-millisecond timeslices to prevent the GPU from stalling. So
//       there are several calls to Yield() to insure this is the case.
// Note: This routine is called by VkStress that uses a detached thread to check the buffer.
// Note: calling assign() takes a huge amount of time(100s of ms). Try not to use it for cases
//       when you don't want the GPU to stall.
void * VulkanGoldenSurfaces::GetCachedAddress
(
    int surfNum,
    Goldelwalues::BufferFetchHint bufFetchHint,
    UINT32 subdevNum,
    vector<UINT08> *surfDumpBuffer
)
{
    VkResult res = VK_SUCCESS;
    if (surfNum >= static_cast<int>(m_VulkanGoldenSurfaces.size()))
        return nullptr;
    auto &vgs = m_VulkanGoldenSurfaces[surfNum];
    // Setup srcImage for copying
    VkImageLayout srcImageLayout = vgs.srcImage->GetImageLayout();
    VkAccessFlags srcImageAccess = vgs.srcImage->GetImageAccessMask();
    VkImageAspectFlags srcImageAspectFlags = vgs.srcImage->GetImageAspectFlags();
    VkPipelineStageFlags srcImageStageMask = vgs.srcImage->GetImageStageMask();

    if (!vgs.transferBufferContentValid)
    {
        if ((res = vgs.srcImage->SetImageLayout(
            vgs.pCmdBuffer
            ,srcImageAspectFlags
            ,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL       //new layout
            ,VK_ACCESS_TRANSFER_READ_BIT                //new access
            ,VK_PIPELINE_STAGE_TRANSFER_BIT)) != VK_SUCCESS)
        {
            MASSERT(!"VulkanGoldenSurfaces::GetCachedAddress SetImageLayout error");
            return nullptr;
        }

        if (CopyImageToBuffer(surfNum, srcImageAspectFlags) != VK_SUCCESS)
        {
            MASSERT(!"VulkanGoldenSurfaces::GetCachedAddress CopyImage error");
            return nullptr;
        }
        vgs.transferBufferContentValid = true;
        Tasker::Yield();
        // restore srcImage's layout
        if ((res = vgs.srcImage->SetImageLayout(
            vgs.pCmdBuffer
            ,srcImageAspectFlags                    //new aspect
            ,srcImageLayout                         //new layout
            ,srcImageAccess                         //new access
            ,srcImageStageMask)) != VK_SUCCESS)
        {
            MASSERT(!"VulkanGoldenSurfaces::GetCachedAddress SetImageLayout error");
            return nullptr;
        }
        Tasker::Yield();
    }

    if (!vgs.pMappedTransferBufferData)
    {
        if (VK_SUCCESS != vgs.transferBuffer->Map(VK_WHOLE_SIZE, 0,
            &vgs.pMappedTransferBufferData))
        {
            MASSERT(!"VulkanGoldenSurfaces::GetCachedAddress map error");
            return nullptr;
        }
    }
    // Attention, see warning at top of function!
    if (surfDumpBuffer && vgs.pMappedTransferBufferData)
    {
        UINT08 *pData8 = static_cast<UINT08*>(vgs.pMappedTransferBufferData);

        UINT64 size =  vgs.transferBuffer->GetSize();
        surfDumpBuffer->reserve(static_cast<size_t>(size));
        surfDumpBuffer->assign(pData8, pData8 + size);
    }

    return vgs.pMappedTransferBufferData;
}

//-------------------------------------------------------------------------------------------------
void VulkanGoldenSurfaces::Ilwalidate()
{
    for (auto &vgs : m_VulkanGoldenSurfaces)
    {
        vgs.transferBufferContentValid = false;
    }
}

//-------------------------------------------------------------------------------------------------
INT32 VulkanGoldenSurfaces::Pitch(int surfNum) const
{
    if ((surfNum >= static_cast<int>(m_VulkanGoldenSurfaces.size())) ||
        (m_VulkanGoldenSurfaces[surfNum].srcImage == nullptr))
    {
        return 0;
    }
    return UNSIGNED_CAST(INT32, m_VulkanGoldenSurfaces[surfNum].srcImage->GetPitch());
}

//-------------------------------------------------------------------------------------------------
UINT32 VulkanGoldenSurfaces::Width(int surfNum) const
{
    if (surfNum >= static_cast<int>(m_VulkanGoldenSurfaces.size()) ||
        (m_VulkanGoldenSurfaces[surfNum].srcImage == nullptr))
    {
        return 0;
    }

    return m_VulkanGoldenSurfaces[surfNum].srcImage->GetWidth();
}

//-------------------------------------------------------------------------------------------------
UINT32 VulkanGoldenSurfaces::Height(int surfNum) const
{
    if (surfNum >= static_cast<int>(m_VulkanGoldenSurfaces.size()) ||
        (m_VulkanGoldenSurfaces[surfNum].srcImage == nullptr))
    {
        return 0;
    }

    return m_VulkanGoldenSurfaces[surfNum].srcImage->GetHeight();
}

//-------------------------------------------------------------------------------------------------
ColorUtils::Format VulkanGoldenSurfaces::Format(int surfNum) const
{
    if ((surfNum >= static_cast<int>(m_VulkanGoldenSurfaces.size())) ||
        (m_VulkanGoldenSurfaces[surfNum].srcImage == nullptr))
    {
        return ColorUtils::LWFMT_NONE;
    }

    switch (m_VulkanGoldenSurfaces[surfNum].srcImage->GetFormat())
    {
        case VK_FORMAT_R8G8B8A8_UNORM:
            return ColorUtils::A8B8G8R8;
        case VK_FORMAT_B8G8R8A8_UNORM:
            return ColorUtils::A8R8G8B8;
        case VK_FORMAT_D16_UNORM:
            return ColorUtils::Z16;
        case VK_FORMAT_D24_UNORM_S8_UINT:
            return ColorUtils::Z24S8;
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
            return ColorUtils::ZF32_X24S8;
        case VK_FORMAT_D32_SFLOAT:
            return ColorUtils::ZF32;
        default:
            MASSERT(!"Missing VkFormat to ColorUtils translation");
            return ColorUtils::LWFMT_NONE;
    }
}

//-------------------------------------------------------------------------------------------------
UINT32 VulkanGoldenSurfaces::Display(int surfNum) const
{
    return 0;
}

//-------------------------------------------------------------------------------------------------
RC VulkanGoldenSurfaces::GetPitchAlignRequirement(UINT32 *pitch)
{
    return RC::SOFTWARE_ERROR;
}

#ifdef ENABLE_UNIT_TESTING
#ifdef _WIN32
#   pragma warning(push)
#   pragma warning(disable: 4265) // class has virtual functions, but destructor is not virtual
#endif
#include <boost/test/unit_test.hpp>
#ifdef _WIN32
#   pragma warning(pop)
#endif

#include "vkcmdbuffer.h"

BOOST_AUTO_TEST_CASE(VulkanGoldenSurface)
{
    VulkanInstance utVkInst(1);
    vector<string> extensions; //none needed for this test.
    BOOST_CHECK_EQUAL(utVkInst.InitVulkan(false, 0, extensions, true), VK_SUCCESS);

    VulkanDevice* pVulkanDev = utVkInst.GetVulkanDevice();

    VulkanGoldenSurfaces utGoldenSurface(&utVkInst);

    // Since there is no surface added the response should be "nullptr":
    BOOST_CHECK_EQUAL(utGoldenSurface.GetCachedAddress(0,
        Goldelwalues::opCpu, 0, nullptr), static_cast<void*>(nullptr));
    BOOST_CHECK_EQUAL(utGoldenSurface.GetCachedAddress(1,
        Goldelwalues::opCpu, 0, nullptr), static_cast<void*>(nullptr));
    unique_ptr<VulkanImage> utImage = make_unique<VulkanImage>(pVulkanDev, false);

    utImage->SetFormat(VK_FORMAT_A1R5G5B5_UNORM_PACK16);
    utImage->SetWidth(10);
    utImage->SetHeight(4);
    utImage->SetPitch(10*sizeof(UINT32));
    VulkanCmdPool utCmdPool(pVulkanDev);
    BOOST_CHECK_EQUAL(utCmdPool.InitCmdPool(0, 0), VK_SUCCESS);
    VulkanCmdBuffer utCmdBuffer(pVulkanDev);
    BOOST_CHECK_EQUAL(utCmdBuffer.AllocateCmdBuffer(&utCmdPool), VK_SUCCESS);

    BOOST_CHECK_EQUAL(utGoldenSurface.AddSurface("test", utImage.get(), &utCmdBuffer), VK_SUCCESS);

    // We have now one surface so non "nullptr" pointer should be returned:
    BOOST_CHECK_NE(utGoldenSurface.GetCachedAddress(0,
        Goldelwalues::opCpu, 0, nullptr), static_cast<void*>(nullptr));
    // But there should be still "nullptr" from surface "1":
    BOOST_CHECK_EQUAL(utGoldenSurface.GetCachedAddress(1,
        Goldelwalues::opCpu, 0, nullptr), static_cast<void*>(nullptr));

    // Check that the surfDumpBuffer is really filled:
    vector<UINT08> dumpBuffer;
    BOOST_CHECK_NE(utGoldenSurface.GetCachedAddress(0,
        Goldelwalues::opCpu, 0, &dumpBuffer), static_cast<void*>(nullptr));
    BOOST_CHECK_NE(dumpBuffer.size(), 0);
}

#endif
