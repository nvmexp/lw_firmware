/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "vkfusion_job.h"

void VkFusion::Job::Stats::AddSample(UINT64 begin, UINT64 end)
{
    MASSERT(end > begin);
    const UINT64 duration = end - begin;
    first   = min(first, begin);
    last    = max(last,  end);
    minimum = min(minimum, duration);
    maximum = max(maximum, duration);
    total  += duration;
    ++numSamples;
}

VkFusion::Job::Stats& VkFusion::Job::Stats::operator+=(const Stats& other)
{
    first       = min(first,   other.first);
    last        = max(last,    other.last);
    minimum     = min(minimum, other.minimum);
    maximum     = max(maximum, other.maximum);
    total      += other.total;
    numSamples += other.numSamples;
    return *this;
}

VkResult VkFusion::Job::Initialize
(
    VulkanDevice*  pVulkanDev,
    VulkanCmdPool* pCmdPool,
    UINT32         measureSubmissions
)
{
    VkResult res = VK_SUCCESS;

    m_CmdBuf = VulkanCmdBuffer(pVulkanDev);
    CHECK_VK_RESULT(m_CmdBuf.AllocateCmdBuffer(pCmdPool));

    m_Fence = VulkanFence(pVulkanDev);
    CHECK_VK_RESULT(m_Fence.CreateFence());

    m_Semaphore = VulkanSemaphore(pVulkanDev);
    CHECK_VK_RESULT(m_Semaphore.CreateBinarySemaphore());

    if (pCmdPool->GetQueueFamilyIdx() == pVulkanDev->GetPhysicalDevice()->GetTransferQueueFamilyIdx())
    {
        Printf(Tee::PriDebug, "Disabling timestamp query on transfer queue %u\n",
               pCmdPool->GetQueueIdx());
        measureSubmissions = 0;
    }

    m_QueryCount = 2 * measureSubmissions;
    if (measureSubmissions)
    {
        m_TimestampQuery = VulkanQuery(pVulkanDev);
        CHECK_VK_RESULT(m_TimestampQuery.Init(VK_QUERY_TYPE_TIMESTAMP, m_QueryCount, 0));
    }

    m_Pending = false;

    return res;
}

VkResult VkFusion::Job::Begin(VulkanCmdBuffer** ppCmdBuf)
{
    MASSERT(!m_Pending);

    VkResult res = VK_SUCCESS;

    CHECK_VK_RESULT(m_CmdBuf.ResetCmdBuffer());
    CHECK_VK_RESULT(m_CmdBuf.BeginCmdBuffer());

    if (m_QueryCount)
    {
        if (m_QueryIdx == 0)
        {
            CHECK_VK_RESULT(m_TimestampQuery.CmdReset(&m_CmdBuf, 0, m_QueryCount));
        }
        CHECK_VK_RESULT(m_TimestampQuery.CmdWriteTimestamp(&m_CmdBuf,
                                                           VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                                           m_QueryIdx++));
    }

    *ppCmdBuf = &m_CmdBuf;

    return res;
}

VkResult VkFusion::Job::Execute
(
    bool               signalSemaphore,
    VkSemaphore        presentSemaphore,
    const VkSemaphore* pWaitSemaphores,
    UINT32             numWaitSemaphores
)
{
    MASSERT(!m_Pending);

    VkResult res = VK_SUCCESS;

    if (m_QueryCount)
    {
        CHECK_VK_RESULT(m_TimestampQuery.CmdWriteTimestamp(&m_CmdBuf,
                                                           VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                                           m_QueryIdx++));
        if (m_QueryIdx == m_QueryCount)
        {
            CHECK_VK_RESULT(m_TimestampQuery.CmdCopyResults(&m_CmdBuf, 0, m_QueryCount,
                                                            VK_QUERY_RESULT_64_BIT |
                                                                VK_QUERY_RESULT_WAIT_BIT));
            m_QueryIdx = 0;
        }
    }

    CHECK_VK_RESULT(m_CmdBuf.EndCmdBuffer());

    VkCommandBuffer cmdBuf = m_CmdBuf.GetCmdBuffer();

    vector<VkPipelineStageFlags> pipelineStages;
    if (numWaitSemaphores)
    {
        pipelineStages.resize(numWaitSemaphores, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
    }

    VkSemaphore signalSemaphores[2];
    UINT32      numSignalSemaphores = 0;
    if (signalSemaphore)
    {
        // This is the internal semaphore used by job to track exelwtion times
        signalSemaphores[numSignalSemaphores++] = m_Semaphore.GetSemaphore();
    }
    if (presentSemaphore)
    {
        // This is the semaphore on which Present has to wait before showing
        // image on the screen
        signalSemaphores[numSignalSemaphores++] = presentSemaphore;
    }

    VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submitInfo.commandBufferCount   = 1;
    submitInfo.pCommandBuffers      = &cmdBuf;
    submitInfo.pWaitSemaphores      = pWaitSemaphores;
    submitInfo.waitSemaphoreCount   = numWaitSemaphores;
    submitInfo.pWaitDstStageMask    = numWaitSemaphores ? &pipelineStages[0] : nullptr;
    submitInfo.pSignalSemaphores    = &signalSemaphores[0];
    submitInfo.signalSemaphoreCount = numSignalSemaphores;

    CHECK_VK_RESULT(m_CmdBuf.GetVulkanDevice()->QueueSubmit(m_CmdBuf.GetCmdPool()->GetQueue(),
                                                            1,
                                                            &submitInfo,
                                                            m_Fence.GetFence()));

    m_Pending = true;

    return res;
}

VkResult VkFusion::Job::Wait(double timeoutMs)
{
    if (!m_Pending)
    {
        return VK_SUCCESS;
    }

    m_Pending = false;

    VkResult res = VK_SUCCESS;

    VkFence fence = m_Fence.GetFence();

    VulkanDevice* const pVulkanDev = m_CmdBuf.GetVulkanDevice();

    CHECK_VK_RESULT(pVulkanDev->WaitForFences(1,
                                              &fence,
                                              VK_TRUE,
                                              static_cast<UINT64>(timeoutMs * 1'000'000.0)));

    CHECK_VK_RESULT(pVulkanDev->ResetFences(1, &fence));

    if (m_QueryCount && (m_QueryIdx == 0))
    {
        const double  tsPeriod = pVulkanDev->GetPhysicalDevice()->GetLimits().timestampPeriod;
        const UINT64* pResults = static_cast<const UINT64*>(m_TimestampQuery.GetResultsPtr());
        const UINT64* const pEnd = pResults + m_QueryCount;
        for ( ; pResults < pEnd; pResults += 2)
        {
            m_DurationStats.AddSample(static_cast<UINT64>(pResults[0] * tsPeriod),
                                      static_cast<UINT64>(pResults[1] * tsPeriod));
        }
    }

    return res;
}

void VkFusion::Job::Cleanup()
{
    m_CmdBuf.FreeCmdBuffer();
    m_Fence.Cleanup();
    m_Semaphore.Cleanup();
    m_TimestampQuery.Cleanup();
    m_Pending = false;
    ResetStats();
}

void VkFusion::Job::ResetStats()
{
    m_DurationStats = Stats();
}
