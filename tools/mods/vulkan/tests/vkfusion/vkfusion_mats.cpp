/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "vkfusion_mats.h"
#include "vulkan/vkbuffer.h"
#include "vulkan/vkimage.h"
#include "vulkan/vkfence.h"
#ifndef VULKAN_STANDALONE
#include "core/include/memerror.h"
#endif

namespace
{
    constexpr UINT32 s_Width = 1024u;
    constexpr UINT32 s_Pitch = s_Width * 4u; // 4 bytes per pixel
}

VkFusion::Mats::Mats()
: Subtest("Mats", 2)
{
    m_PatternSet = PatternClass::PATTERNSET_ALL;
    m_PatternClass.SelectPatternSet(m_PatternSet);
}

VkFusion::QueueCounts VkFusion::Mats::GetRequiredQueues() const
{
    QueueCounts counts = { m_NumGraphicsQueues, m_NumComputeQueues, m_NumTransferQueues };
    return counts;
}

VkFusion::ExtensionList VkFusion::Mats::GetRequiredExtensions() const
{
    ExtensionList list = { nullptr, 0 };
    return list;
}

RC VkFusion::Mats::Setup()
{
    // We do vkCmdCopyImage() from an image to itself, so we're using VK_IMAGE_LAYOUT_GENERAL,
    // but validation layers issue a perf warning that we should use either
    // VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL or VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL.
    m_pVulkanInst->AddDebugMessageToIgnore(
    {
        VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_BUFFER_EXT,
        0,
        "Validation",
        "vkCmdCopyImage(): For optimal performance VkImage %c",
        '0'
    });

    if (m_BoxSizeKb < s_Pitch / 1024u)
    {
        Printf(Tee::PriError, "BoxSizeKb %u too small, needs to be at least %u\n",
               m_BoxSizeKb, s_Pitch / 1024u);
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }
    MASSERT((s_Pitch % 1024u) == 0u);
    if (m_BoxSizeKb % (s_Pitch / 1024u))
    {
        Printf(Tee::PriError, "BoxSizeKb %u is not a multiple of %u\n",
               m_BoxSizeKb, s_Pitch / 1024u);
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    VkPhysicalDeviceProperties props = { };
    const VkPhysicalDevice physDev = m_pVulkanDev->GetPhysicalDevice()->GetVkPhysicalDevice();
    m_pVulkanInst->GetPhysicalDeviceProperties(physDev, &props);

    m_Random.SeedRandom(m_RandomSeed ^ 0xB0509C08u); // Unique seed for this subtest

    ////////////////////////////////////////////////////////////////////////////
    // Allocate memory for testing on the GPU

    const UINT32 maxHeight   = props.limits.maxImageDimension2D;
    const UINT32 boxHeight   = m_BoxSizeKb * 1024u / s_Pitch;
    const UINT64 totalHeight = static_cast<UINT64>(m_MaxFbMb * 1_MB / s_Pitch);
    const UINT32 numBoxes    = static_cast<UINT32>(totalHeight / boxHeight);

    VerbosePrintf("Mats: Allocating %u boxes, %llu MB of memory, box height %u, box size %u KB\n",
                  numBoxes, (totalHeight * s_Pitch / 1_MB), boxHeight, boxHeight * s_Pitch / 1024u);

    if (numBoxes < 2u)
    {
        Printf(Tee::PriError, "Not enough boxes, need at least 2 boxes to run Mats subtest\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    m_BoxesPerImage = static_cast<UINT32>(min<UINT64>(totalHeight, maxHeight) / boxHeight);

    RC rc;
    const VkMemoryPropertyFlags location = m_UseSysmem ? 0 : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    for (UINT64 h = 0; h < totalHeight; )
    {
        UINT32 allocHeight = static_cast<UINT32>(min<UINT64>(totalHeight - h, maxHeight));
        allocHeight -= allocHeight % boxHeight; // Align on box size

        Printf(Tee::PriDebug, "Allocate image %u: %ux%u\n",
               static_cast<unsigned>(m_Memory.size()), s_Width, allocHeight);

        m_Memory.emplace_back();
        CHECK_VK_TO_RC(AllocateImage(&m_Memory.back(),
                                     VK_FORMAT_B8G8R8A8_UNORM,
                                     s_Width,
                                     allocHeight,
                                     1, // mipmapLevels
                                     VK_IMAGE_USAGE_STORAGE_BIT | // needed by layers, we don't use this bit
                                         VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                         VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                     VK_IMAGE_TILING_OPTIMAL,
                                     location));

        h += allocHeight;
    }

    VerbosePrintf("Mats: Allocated %u images\n", static_cast<unsigned>(m_Memory.size()));

    ////////////////////////////////////////////////////////////////////////////
    // Prepare patterns on the host

    m_PatternClass.FillRandomPattern(m_Random.GetRandom());
    m_PatternClass.ResetFillIndex();
    m_PatternClass.ResetCheckIndex();
    vector<UINT32> patternIds;
    CHECK_RC(m_PatternClass.GetSelectedPatterns(&patternIds));

    VulkanBuffer patternBuf(m_pVulkanDev);
    CHECK_VK_TO_RC(patternBuf.CreateBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                           boxHeight * s_Pitch * static_cast<UINT32>(patternIds.size()),
                                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));

    {
        VulkanBufferView<UINT32> view(patternBuf);
        CHECK_VK_TO_RC(view.Map());

        for (UINT32 i = 0; i < patternIds.size(); i++)
        {
            CHECK_RC(m_PatternClass.FillMemoryWithPattern(view.begin() + i * boxHeight * s_Width,
                                                          s_Pitch,
                                                          boxHeight,
                                                          s_Pitch,
                                                          32,
                                                          patternIds[i],
                                                          nullptr));
        }
    }

    m_BoxToPattern.resize(numBoxes);
    for (UINT32 i = 0; i < numBoxes; i++)
    {
        m_BoxToPattern[i] = i % static_cast<UINT32>(patternIds.size());
    }
    m_Random.Shuffle(numBoxes, &m_BoxToPattern[0]);

    ////////////////////////////////////////////////////////////////////////////
    // Allocate command buffers

    MASSERT(m_Queues.size() == m_NumGraphicsQueues + m_NumComputeQueues + m_NumTransferQueues);

    m_CmdPools.resize(m_Queues.size());

    for (UINT32 i = 0; i < m_Queues.size(); i++)
    {
        VulkanCmdPool& cmdPool = m_CmdPools[i];

        cmdPool = VulkanCmdPool(m_pVulkanDev);
        CHECK_VK_TO_RC(cmdPool.InitCmdPool(m_Queues[i].family, m_Queues[i].idx));
    }

    m_Jobs.resize(m_NumJobs);

    for (auto& job : m_Jobs)
    {
        job.cmdBuf.resize(m_Queues.size());

        for (UINT32 i = 0; i < m_Queues.size(); i++)
        {
            VulkanCmdBuffer& cmdBuf = job.cmdBuf[i];

            cmdBuf = VulkanCmdBuffer(m_pVulkanDev);
            CHECK_VK_TO_RC(cmdBuf.AllocateCmdBuffer(&m_CmdPools[i], VK_COMMAND_BUFFER_LEVEL_SECONDARY));
        }
    }

    ////////////////////////////////////////////////////////////////////////////
    // Send boxes to GPU

    const UINT32 sendQueue = m_NumTransferQueues ? (m_NumGraphicsQueues + m_NumComputeQueues) : 0;
    VulkanCmdBuffer sendCmdBuf(m_pVulkanDev);
    CHECK_VK_TO_RC(sendCmdBuf.AllocateCmdBuffer(&m_CmdPools[sendQueue]));
    CHECK_VK_TO_RC(sendCmdBuf.BeginCmdBuffer());
    for (UINT32 iImage = 0; iImage < m_Memory.size(); iImage++)
    {
        VulkanImage& dst = m_Memory[iImage];

        MASSERT(dst.GetWidth() == s_Width);
        CHECK_VK_TO_RC(dst.SetImageLayout(&sendCmdBuf,
                                          VK_IMAGE_ASPECT_COLOR_BIT,
                                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                          VK_ACCESS_TRANSFER_WRITE_BIT,
                                          VK_PIPELINE_STAGE_TRANSFER_BIT));

        const UINT32 boxesInImage = dst.GetHeight() / boxHeight;
        for (UINT32 imgBoxIdx = 0; imgBoxIdx < boxesInImage; imgBoxIdx++)
        {
            const UINT32 globalBoxIdx = (iImage * m_BoxesPerImage) + imgBoxIdx;
            MASSERT(globalBoxIdx < m_BoxToPattern.size());
            const UINT32 patIdx = m_BoxToPattern[globalBoxIdx];

            Printf(Tee::PriDebug, "Fill box %u in image %u lines %u..%u with pattern %u\n",
                   globalBoxIdx,
                   iImage,
                   imgBoxIdx * boxHeight,
                   imgBoxIdx * boxHeight + boxHeight - 1,
                   patternIds[patIdx]);

            // Copy pattern number patIdx to box number globalBoxIdx.
            // - m_BoxToPattern vector tells us which pattern should be used by which box.
            // - patternBuf is a host VkBuffer which contains all patterns.  It contains
            //   one box per pattern, i.e. i'th box in patternBuf is filled with i'th pattern.
            // - To fill box number globalBoxIdx in GPU memory, we simply copy the corresponding
            //   box from patternBuf at index patIdx, this way we will it with the correct
            //   pattern.

            VkBufferImageCopy copyRegion = { };
            copyRegion.bufferOffset                = patIdx * boxHeight * s_Pitch;
            copyRegion.bufferRowLength             = s_Width;
            copyRegion.bufferImageHeight           = boxHeight;
            copyRegion.imageSubresource.aspectMask = dst.GetImageAspectFlags();
            copyRegion.imageSubresource.layerCount = 1;
            copyRegion.imageOffset.y               = imgBoxIdx * boxHeight;
            copyRegion.imageExtent.width           = s_Width;
            copyRegion.imageExtent.height          = boxHeight;
            copyRegion.imageExtent.depth           = 1;

            m_pVulkanDev->CmdCopyBufferToImage(sendCmdBuf.GetCmdBuffer(),
                                               patternBuf.GetBuffer(),
                                               dst.GetImage(),
                                               dst.GetImageLayout(),
                                               1,
                                               &copyRegion);
        }

        // Set layout used by secondary command buffers
        CHECK_VK_TO_RC(dst.SetImageLayout(&sendCmdBuf,
                                          VK_IMAGE_ASPECT_COLOR_BIT,
                                          VK_IMAGE_LAYOUT_GENERAL,
                                          VK_ACCESS_TRANSFER_WRITE_BIT,
                                          VK_PIPELINE_STAGE_TRANSFER_BIT));
    }
    CHECK_VK_TO_RC(sendCmdBuf.EndCmdBuffer());
    CHECK_VK_TO_RC(sendCmdBuf.ExelwteCmdBuffer(true));
    sendCmdBuf.FreeCmdBuffer();
    m_CmdPools[sendQueue].Reset();

    ////////////////////////////////////////////////////////////////////////////
    // Create secondary command buffers for each job

    const UINT32 numQueues = static_cast<UINT32>(m_Queues.size());
    m_BoxesPerQueue        = (numBoxes + numQueues - 1u) / numQueues; // Last queue may have less boxes

    // All the jobs created are identical.  Each job has one secondary command buffer per queue.
    // All jobs can be in flight at the same time, but each job slot has to finish before
    // it can be submitted again.
    for (UINT32 jobIdx = 0; jobIdx < m_Jobs.size(); jobIdx++)
    {
        Job& job = m_Jobs[jobIdx];

        job.bytesTransferred = 0;

        UINT32 iBox = 0;

        // Create one secondary command buffer per queue.  Each queue uses a separate area of
        // memory, so they do not overlap.
        for (VulkanCmdBuffer& cmdBuf : job.cmdBuf)
        {
            const UINT32 iLastBox = min((iBox + m_BoxesPerQueue), numBoxes) - 1u;
            MASSERT(iLastBox < m_BoxToPattern.size());

            CHECK_VK_TO_RC(cmdBuf.BeginCmdBuffer());

            // Record a number of identical blit loops into the command buffer.
            // This is to reduce the workload on the CPU, so that the CPU has to
            // resubmit jobs less frequently.
            for (UINT32 loop = 0; loop < m_NumBlitLoops; loop++)
            {
                for (UINT32 i = iBox; i <= iLastBox; i++)
                {
                    const UINT32 srcBox    = (i < iLastBox) ? (i + 1u) : iBox;
                    const UINT32 srcImgIdx = srcBox / m_BoxesPerImage;
                    const UINT32 dstImgIdx = i / m_BoxesPerImage;
                    VulkanImage& srcImg    = m_Memory[srcImgIdx];
                    VulkanImage& dstImg    = m_Memory[dstImgIdx];
                    const UINT32 srcBoxIdx = srcBox % m_BoxesPerImage;
                    const UINT32 dstBoxIdx = i % m_BoxesPerImage;

                    CHECK_VK_TO_RC(dstImg.SetImageLayout(&cmdBuf,
                                                         VK_IMAGE_ASPECT_COLOR_BIT,
                                                         VK_IMAGE_LAYOUT_GENERAL,
                                                         VK_ACCESS_TRANSFER_READ_BIT,
                                                         VK_PIPELINE_STAGE_TRANSFER_BIT));

                    VkImageCopy region = { };
                    region.srcSubresource.aspectMask = srcImg.GetImageAspectFlags();
                    region.srcSubresource.layerCount = 1u;
                    region.dstSubresource.aspectMask = dstImg.GetImageAspectFlags();
                    region.dstSubresource.layerCount = 1u;
                    region.srcOffset.y               = srcBoxIdx * boxHeight;
                    region.dstOffset.y               = dstBoxIdx * boxHeight;
                    region.extent.width              = s_Width;
                    region.extent.height             = boxHeight;
                    region.extent.depth              = 1u;

                    Printf(Tee::PriDebug, "Job %u, loop %u: copy img %u y %u -> img %u y %u\n",
                           jobIdx, loop,
                           srcImgIdx, region.srcOffset.y, dstImgIdx, region.dstOffset.y);

                    m_pVulkanDev->CmdCopyImage(cmdBuf.GetCmdBuffer(),
                                               srcImg.GetImage(),
                                               srcImg.GetImageLayout(),
                                               dstImg.GetImage(),
                                               dstImg.GetImageLayout(),
                                               1u,
                                               &region);

                    CHECK_VK_TO_RC(dstImg.SetImageLayout(&cmdBuf,
                                                         VK_IMAGE_ASPECT_COLOR_BIT,
                                                         VK_IMAGE_LAYOUT_GENERAL,
                                                         VK_ACCESS_TRANSFER_WRITE_BIT,
                                                         VK_PIPELINE_STAGE_TRANSFER_BIT));

                    job.bytesTransferred += s_Pitch * boxHeight;
                }
            }

            CHECK_VK_TO_RC(cmdBuf.EndCmdBuffer());

            iBox = iLastBox + 1u;
        }
    }

    return RC::OK;
}

RC VkFusion::Mats::Cleanup()
{
    m_Jobs.clear();
    m_CmdPools.clear();
    m_Memory.clear();
    m_BoxToPattern = vector<UINT32>(); // Free memory held by the vector
    return RC::OK;
}

void VkFusion::Mats::PreRun()
{
    m_TotalGB     = 0;
    m_GBPerSecond = 0;

    for (Job& job : m_Jobs)
    {
        job.numRuns = 0;
    }
}

RC VkFusion::Mats::Execute
(
    UINT32            jobId,
    VulkanCmdBuffer** pCmdBufs,
    UINT32            numCmdBufs,
    VkSemaphore*      pJobWaitSema,
    VkSemaphore*      pPresentSema
)
{
    MASSERT(numCmdBufs == m_Queues.size());

    Job& job = m_Jobs[jobId];

    for (UINT32 i = 0; i < numCmdBufs; i++)
    {
        const VkCommandBuffer secondaryCmdBuf = job.cmdBuf[i].GetCmdBuffer();
        m_pVulkanDev->CmdExelwteCommands(pCmdBufs[i]->GetCmdBuffer(), 1, &secondaryCmdBuf);
    }

    ++job.numRuns;

    m_LastJobId = jobId;

    return RC::OK;
}

RC VkFusion::Mats::CheckFrame(UINT32 jobId, UINT64 frameId, bool finalFrame)
{
    if (!finalFrame)
    {
        return RC::OK;
    }

    // If this is the final frame for this job slot, only do the checking in the last
    // job slot that was submitted
    if (jobId != m_LastJobId)
    {
        return RC::OK;
    }

    RC rc;
    vector<UINT32> patternIds;
    m_PatternClass.ResetCheckIndex();
    CHECK_RC(m_PatternClass.GetSelectedPatterns(&patternIds));

    constexpr UINT64 checkBufSize    = 8_MB;
    constexpr UINT32 numPingPongBufs = 2;

    const UINT32 boxesToCheckInOneGo = static_cast<UINT32>(checkBufSize / (m_BoxSizeKb * 1_KB));

    // Callwlate total number of rotations, to know where each box pattern is
    UINT64 numRotations = 0;
    for (Job& job : m_Jobs)
    {
        numRotations += job.numRuns;
    }

    struct CheckJob
    {
        VulkanBuffer    checkBuf;
        VulkanCmdBuffer cmdBuf;
        VulkanFence     fence;
        UINT32          boxIdx   = ~0U;
        UINT32          numBoxes = 0;
    };
    CheckJob jobs[numPingPongBufs];

    const UINT32 sendQueue = m_NumTransferQueues ? (m_NumGraphicsQueues + m_NumComputeQueues) : 0;

    for (CheckJob& job : jobs)
    {
        job.checkBuf = VulkanBuffer(m_pVulkanDev);
        CHECK_VK_TO_RC(job.checkBuf.CreateBuffer(VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                 boxesToCheckInOneGo * m_BoxSizeKb * 1_KB,
                                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                     VK_MEMORY_PROPERTY_HOST_CACHED_BIT));

        job.cmdBuf = VulkanCmdBuffer(m_pVulkanDev);
        CHECK_VK_TO_RC(job.cmdBuf.AllocateCmdBuffer(&m_CmdPools[sendQueue]));

        job.fence = VulkanFence(m_pVulkanDev);
        CHECK_VK_TO_RC(job.fence.CreateFence());
    }

    StickyRC     checkRc;
    UINT32       numPendingJobs = 0;
    UINT32       iBox           = 0;
    UINT32       boxesChecked   = 0;
    const UINT32 boxHeight      = m_BoxSizeKb * 1024u / s_Pitch;
    const UINT32 totalBoxes     = static_cast<UINT32>(m_BoxToPattern.size());

    for (UINT32 loop = 0; (loop == 0) || numPendingJobs; loop++)
    {
        CheckJob& job = jobs[loop % numPingPongBufs];

        Printf(Tee::PriDebug, "Check loop %u, job %u, box %u, send box %u, pending %u jobs\n",
               loop, loop % numPingPongBufs, job.boxIdx, iBox, numPendingJobs);

        // Check downloaded boxes
        if (job.boxIdx != ~0U)
        {
            // Wait for copy to complete
            VkFence fence = job.fence.GetFence();
            CHECK_VK_TO_RC(m_pVulkanDev->WaitForFences(1,
                                                       &fence,
                                                       VK_TRUE,
                                                       m_TimeoutMs * 1'000'000ULL));
            CHECK_VK_TO_RC(m_pVulkanDev->ResetFences(1, &fence));
            --numPendingJobs;

            // Check the contents
            VulkanBufferView<UINT32> view(job.checkBuf);
            CHECK_VK_TO_RC(view.Map());

            for (UINT32 i = 0; i < job.numBoxes; i++)
            {
                const UINT32 origBoxIdx = job.boxIdx + i;
                UINT32       boxIdx     = origBoxIdx;

                // Detect which pattern it is after all box rotations were performed
                if (numRotations)
                {
                    UINT32       boxInGroup   = boxIdx % m_BoxesPerQueue;
                    const UINT32 groupBegin   = boxIdx - boxInGroup;
                    const UINT32 boxesInGroup = min(m_BoxesPerQueue, totalBoxes - groupBegin);

                    // Handle spare box
                    if (boxInGroup == 0)
                    {
                        boxInGroup += boxesInGroup - 1;
                    }

                    boxInGroup = (boxInGroup - 1 + numRotations) % (boxesInGroup - 1);

                    boxIdx = groupBegin + 1 + boxInGroup;
                }

                const UINT32 patIdx = m_BoxToPattern[boxIdx];

                Printf(Tee::PriDebug, "Check box %u with pattern %u\n", origBoxIdx, patternIds[patIdx]);

                ++boxesChecked;

                UINT32* const data = view.begin() + i * s_Width * boxHeight;

                checkRc = m_PatternClass.CheckMemory(data,
                                                     s_Pitch,
                                                     boxHeight,
                                                     s_Pitch,
                                                     32,
                                                     patternIds[patIdx],
                                                     HandleError,
                                                     this);
            }
        }

        // Download subsequent boxes to host memory
        if (iBox < totalBoxes)
        {
            const UINT32 srcImgIdx        = iBox / m_BoxesPerImage;
            const UINT32 imgBoxIdx        = iBox % m_BoxesPerImage;
            const UINT32 boxesLeft        = totalBoxes - iBox;
            const UINT32 boxesLeftInImage = m_BoxesPerImage - imgBoxIdx;
            const UINT32 boxesToCopy = min(min(boxesLeft, boxesLeftInImage), boxesToCheckInOneGo);
            MASSERT(boxesToCopy > 0);
            MASSERT(srcImgIdx < m_Memory.size());

            job.boxIdx   = iBox;
            job.numBoxes = boxesToCopy;
            iBox        += boxesToCopy;

            CHECK_VK_TO_RC(job.cmdBuf.ResetCmdBuffer());
            CHECK_VK_TO_RC(job.cmdBuf.BeginCmdBuffer());

            VulkanImage& srcImg = m_Memory[srcImgIdx];

            CHECK_VK_TO_RC(srcImg.SetImageLayout(&job.cmdBuf,
                                                 VK_IMAGE_ASPECT_COLOR_BIT,
                                                 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                                 VK_ACCESS_TRANSFER_READ_BIT,
                                                 VK_PIPELINE_STAGE_TRANSFER_BIT));

            VkBufferImageCopy copyRegion = { };
            copyRegion.bufferOffset                = 0;
            copyRegion.bufferRowLength             = s_Width;
            copyRegion.bufferImageHeight           = boxHeight * boxesToCopy;
            copyRegion.imageSubresource.aspectMask = srcImg.GetImageAspectFlags();
            copyRegion.imageSubresource.layerCount = 1;
            copyRegion.imageOffset.y               = imgBoxIdx * boxHeight;
            copyRegion.imageExtent.width           = s_Width;
            copyRegion.imageExtent.height          = boxHeight * boxesToCopy;
            copyRegion.imageExtent.depth           = 1;

            m_pVulkanDev->CmdCopyImageToBuffer(job.cmdBuf.GetCmdBuffer(),
                                               srcImg.GetImage(),
                                               srcImg.GetImageLayout(),
                                               job.checkBuf.GetBuffer(),
                                               1,
                                               &copyRegion);

            if (boxesToCopy == boxesLeftInImage)
            {
                // Leave image in sane state in case the test is exelwted again without Cleanup()
                CHECK_VK_TO_RC(srcImg.SetImageLayout(&job.cmdBuf,
                                                     VK_IMAGE_ASPECT_COLOR_BIT,
                                                     VK_IMAGE_LAYOUT_GENERAL,
                                                     VK_ACCESS_TRANSFER_WRITE_BIT,
                                                     VK_PIPELINE_STAGE_TRANSFER_BIT));
            }

            CHECK_VK_TO_RC(job.cmdBuf.EndCmdBuffer());
            CHECK_VK_TO_RC(job.cmdBuf.ExelwteCmdBuffer(VK_NULL_HANDLE,
                                                       0,
                                                       nullptr,
                                                       VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                                       false,
                                                       job.fence.GetFence()));

            ++numPendingJobs;
        }
    }

    MASSERT(boxesChecked == totalBoxes);

    CHECK_RC(checkRc);

    return RC::OK;
}

RC VkFusion::Mats::HandleError
(
    void*         pCookie,
    UINT32        offset,
    UINT32        expected,
    UINT32        actual,
    UINT32        patternOffset,
    const string& patternName
)
{
    return static_cast<Mats*>(pCookie)->HandleError(offset, expected, actual, patternOffset, patternName);
}

RC VkFusion::Mats::HandleError
(
    UINT32        offset,
    UINT32        expected,
    UINT32        actual,
    UINT32        patternOffset,
    const string& patternName
)
{
    if (m_pMemError)
    {
        m_pMemError->LogOffsetError(32,
                                    offset,
                                    actual,
                                    expected,
                                    0,
                                    4,
                                    patternName,
                                    patternOffset);
    }

    return RC::OK;
}

RC VkFusion::Mats::Dump(UINT32 jobId, UINT64 frameId)
{
    return RC::OK;
}

void VkFusion::Mats::UpdateAndPrintStats(UINT64 elapsedMs)
{
    UINT64 totalTransferredKB = 0;

    for (Job& job : m_Jobs)
    {
        totalTransferredKB += job.numRuns * (job.bytesTransferred / 1024ull);
    }

    m_TotalGB     = totalTransferredKB / 1_MB;
    m_GBPerSecond = (totalTransferredKB * 1000.0) / (elapsedMs * 1024.0 * 1024.0);

    VerbosePrintf("Mats: Transferred (read+written) %llu GB in %llu ms, %.1f GBps\n",
                  m_TotalGB, elapsedMs, m_GBPerSecond);
}
