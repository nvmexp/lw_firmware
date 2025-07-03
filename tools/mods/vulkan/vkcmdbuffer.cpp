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

#include "vkcmdbuffer.h"
#include "vkdev.h"
#include "vkinstance.h"
#ifndef VULKAN_STANDALONE
#include "core/include/tasker.h"
#include "core/include/xp.h"
#endif
//--------------------------------------------------------------------
VulkanCmdPool::VulkanCmdPool(VulkanDevice* pVulkanDev) :
    m_pVulkanDev(pVulkanDev)
{
    MASSERT(m_pVulkanDev);
}

//--------------------------------------------------------------------
VulkanCmdPool::~VulkanCmdPool()
{
    DestroyCmdPool();
}

VulkanCmdPool::VulkanCmdPool(VulkanCmdPool&& other) noexcept
{
    TakeResources(move(other));
}

VulkanCmdPool& VulkanCmdPool::operator=(VulkanCmdPool&& other)
{
    TakeResources(move(other));
    return *this;
}

//--------------------------------------------------------------------
void VulkanCmdPool::TakeResources(VulkanCmdPool&& other)
{
    if (this != &other)
    {
        DestroyCmdPool();

        m_pVulkanDev = other.m_pVulkanDev;
        m_CmdPool = other.m_CmdPool;
        m_Queue = other.m_Queue;
        m_QueueFamilyIdx = other.m_QueueFamilyIdx;
        m_QueueIdx = other.m_QueueIdx;

        other.m_pVulkanDev = nullptr;
        other.m_CmdPool = VK_NULL_HANDLE;
        other.m_QueueFamilyIdx = ~0;
        other.m_QueueIdx = ~0;
    }
}

//-------------------------------------------------------------------------------------------------
VkResult VulkanCmdPool::InitCmdPool
(
    UINT32 queueFamilyIdx
    ,UINT32 queueIdx
    ,UINT32 flags /* = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT */
)
{
    MASSERT(m_CmdPool == VK_NULL_HANDLE);

    SetQueueFamilyIdx(queueFamilyIdx);
    SetQueueIdx(queueIdx);
    m_pVulkanDev->GetDeviceQueue(m_QueueFamilyIdx, m_QueueIdx, &m_Queue);

    VkCommandPoolCreateInfo cmdPoolInfo = {};
    cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    // Note: default is to allow command buffers to be rerecorded individually.
    // Without this flag they all have to be reset together! yuck.
    cmdPoolInfo.flags = flags;
    cmdPoolInfo.queueFamilyIndex = m_QueueFamilyIdx;
    return m_pVulkanDev->CreateCommandPool(&cmdPoolInfo, nullptr, &m_CmdPool);
}

//--------------------------------------------------------------------
void VulkanCmdPool::InitTracyCtx(VkCommandBuffer cmdBuffer)
{
#ifdef TRACY_ENABLE
    if (m_TracyCtx)
    {
        return;
    }

    VulkanInstance* const pVulkanInst = m_pVulkanDev->GetVulkanInstance();
    VkPhysicalDevice physDev = m_pVulkanDev->GetPhysicalDevice()->GetVkPhysicalDevice();

    UINT32 queueCount;
    pVulkanInst->GetPhysicalDeviceQueueFamilyProperties(physDev, &queueCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueProps(queueCount);
    pVulkanInst->GetPhysicalDeviceQueueFamilyProperties(physDev, &queueCount, queueProps.data());

    MASSERT(m_QueueFamilyIdx < queueCount);
    const VkQueueFlags flags = queueProps[m_QueueFamilyIdx].queueFlags;
    if (flags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT))
    {
        m_TracyCtx = TracyVkContextCalibrated(physDev,
                                              m_pVulkanDev->GetVkDevice(),
                                              m_Queue,
                                              cmdBuffer,
                                              vkGetPhysicalDeviceCalibrateableTimeDomainsEXT,
                                              vkGetCalibratedTimestampsEXT);
    }
#endif
}

//--------------------------------------------------------------------
void VulkanCmdPool::DestroyCmdPool()
{
#ifdef TRACY_ENABLE
    if (m_TracyCtx)
    {
        TracyVkDestroy(m_TracyCtx);
        m_TracyCtx = nullptr;
    }
#endif

    if (!m_pVulkanDev)
    {
        MASSERT(m_CmdPool == VK_NULL_HANDLE);
        return;
    }

    if (m_CmdPool)
    {
        m_pVulkanDev->DestroyCommandPool(m_CmdPool);
        m_CmdPool = VK_NULL_HANDLE;
    }
}

//--------------------------------------------------------------------
VkResult VulkanCmdPool::Reset(UINT32 flags)
{
    return m_pVulkanDev->ResetCommandPool(m_CmdPool, flags);
}

//--------------------------------------------------------------------
VulkanCmdBuffer::VulkanCmdBuffer(VulkanDevice* pVulkanDev) :
    m_pVulkanDev(pVulkanDev)
{
    MASSERT(m_pVulkanDev);
    m_TimeoutNs = m_pVulkanDev->GetTimeoutNs();
}

//--------------------------------------------------------------------
VulkanCmdBuffer::~VulkanCmdBuffer()
{
    FreeCmdBuffer();
}

//--------------------------------------------------------------------
VulkanCmdBuffer::VulkanCmdBuffer(VulkanCmdBuffer&& other) noexcept
{
    TakeResources(move(other));
}

//--------------------------------------------------------------------
VulkanCmdBuffer& VulkanCmdBuffer::operator=(VulkanCmdBuffer&& other)
{
    TakeResources(move(other));
    return *this;
}

//--------------------------------------------------------------------
void VulkanCmdBuffer::TakeResources(VulkanCmdBuffer&& other)
{
    if (this != &other)
    {
        FreeCmdBuffer();

        m_pVulkanDev = other.GetVulkanDevice();
        m_pVulkanCmdPool = other.m_pVulkanCmdPool;
        m_CmdBuffer = other.GetCmdBuffer();

        other.m_pVulkanDev = nullptr;
        other.m_pVulkanCmdPool = nullptr;
        other.m_CmdBuffer = VK_NULL_HANDLE;

        m_RecordingBegun = other.GetRecordingBegun();
        m_RecordingEnded = other.GetRecordingEnded();
    }
}

//--------------------------------------------------------------------
VkResult VulkanCmdBuffer::AllocateCmdBuffer(VulkanCmdPool*       pCmdPool,
                                            VkCommandBufferLevel level)
{
    MASSERT(pCmdPool);
    m_pVulkanCmdPool = pCmdPool;
    MASSERT(m_pVulkanCmdPool->GetCmdPool() != VK_NULL_HANDLE);
    MASSERT(m_CmdBuffer == VK_NULL_HANDLE);

    VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.commandPool = m_pVulkanCmdPool->GetCmdPool();
    commandBufferAllocateInfo.level = level;
    commandBufferAllocateInfo.commandBufferCount = 1;
    const VkResult result = m_pVulkanDev->AllocateCommandBuffers(&commandBufferAllocateInfo, &m_CmdBuffer);
    if (result == VK_SUCCESS)
    {
        pCmdPool->InitTracyCtx(m_CmdBuffer);
    }
    return result;
}

//--------------------------------------------------------------------
void VulkanCmdBuffer::FreeCmdBuffer()
{
    if (!m_pVulkanDev || !m_pVulkanCmdPool || m_CmdBuffer == VK_NULL_HANDLE)
    {
        MASSERT(m_CmdBuffer == VK_NULL_HANDLE);
        return;
    }

    m_pVulkanDev->FreeCommandBuffers(m_pVulkanCmdPool->GetCmdPool(), 1, &m_CmdBuffer);
    m_CmdBuffer = VK_NULL_HANDLE;
}

//--------------------------------------------------------------------
VkResult VulkanCmdBuffer::BeginCmdBuffer(UINT32                                flags,
                                         const VkCommandBufferInheritanceInfo* pInheritanceInfo)
{
    VkResult res = VK_SUCCESS;
    MASSERT(!m_RecordingBegun);

    // If no inheritance was provided by the caller, use default/empty values.
    // Inheritance info is required for secondary command buffers.
    VkCommandBufferInheritanceInfo inherit = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO };
    inherit.framebuffer = VK_NULL_HANDLE;
    if (!pInheritanceInfo)
    {
        pInheritanceInfo = &inherit;
    }

    VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    beginInfo.flags = flags;
    beginInfo.pInheritanceInfo = pInheritanceInfo;

    CHECK_VK_RESULT(m_pVulkanDev->BeginCommandBuffer(m_CmdBuffer, &beginInfo));
    m_RecordingBegun = true;
    return VK_SUCCESS;
}

//------------------------------------------------------------------------------------------------
// Submit a single command buffer object to the command queue.
// Parameters:
//  waitSemaphore   : If supplied the command buffer will wait for this semaphore to become
//                    signaled before exelwting the commands. The semaphore will be set to
//                    unsignaled state prior to exelwting the commands.
//  signalSemaphore : If supplied this semaphore will be signaled after the command buffer has
//                    been exelwted.
//  waitStageMask   : The pipeline stage at which each corresponding semaphore wait will occur.
//  waitOnFence     : If true the routine will wait on the fence to be signaled before returning.
//                    If a handle to a fence is not provided a local fence will be created.
//  fence           : A handle to an optional fence to be signaled
//
// Truth table used to determine the appropriate actions for handling the fence:
//
// waitOnFence  fence   create  wait  submit  reset  destroy
// -----------  -----   ------  ----  ------  -----  -------
// false        null    no      no    no      no     no
// false        !null   no      no    yes     no     no
// true         null    yes     yes   yes     yes    yes
// true         !null   no      yes   yes     yes    no
VkResult VulkanCmdBuffer::ExelwteCmdBuffer
(
    VkSemaphore waitSemaphore,
    UINT32 numSignalSemaphores,
    VkSemaphore *pSignalSemaphores,
    VkPipelineStageFlags waitStageMask,
    bool waitOnFence,
    VkFence fence,
    bool usePolling,
    bool resetAfterExec
)
{
    VkResult res = VK_SUCCESS;
    MASSERT(m_RecordingBegun && m_RecordingEnded);
    VkSubmitInfo submit_info = {};
    submit_info.pNext = nullptr;
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = waitSemaphore ? 1 : 0;
    submit_info.pWaitSemaphores = &waitSemaphore;
    if (waitSemaphore)
    {
        submit_info.pWaitDstStageMask = &waitStageMask;
    }
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &m_CmdBuffer;
    submit_info.signalSemaphoreCount = numSignalSemaphores;
    submit_info.pSignalSemaphores = pSignalSemaphores;
    bool fenceCreated = false;
    VkFenceCreateInfo fenceInfo;

    //use defer so that we free up the fence on errors and don't cause memory leaks
    DEFER
    {
        if (fenceCreated)
        {
            m_pVulkanDev->DestroyFence(fence);
        }
    };

    if (waitOnFence && !fence)
    {
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.pNext = nullptr;
        fenceInfo.flags = 0; // unsignaled state
        CHECK_VK_RESULT(m_pVulkanDev->CreateFence(&fenceInfo, nullptr, &fence));
        fenceCreated = true;
    }

    CHECK_VK_RESULT(m_pVulkanDev->QueueSubmit(m_pVulkanCmdPool->GetQueue(),
        1, &submit_info, fence));
    if (waitOnFence)
    {
#ifndef VULKAN_STANDALONE
        if (usePolling)
        {
            UINT64 endTime = Xp::GetWallTimeNS() + m_TimeoutNs;
            while ((res = m_pVulkanDev->GetFenceStatus(fence)) == VK_NOT_READY)
            {
                if (Xp::GetWallTimeNS() > endTime)
                {
                    Printf(Tee::PriWarn, "Fence status still NOT_READY after %lldns."
                                         "Fence(s) will not be reset!\n", m_TimeoutNs);
                    // return early because we can't reset a fence that is still active in a queue.
                    return VK_TIMEOUT; // colwert error code to VK_TIMEOUT
                }
                Tasker::Yield();
            }
        }
        else
#endif
        {
            CHECK_VK_RESULT(m_pVulkanDev->WaitForFences(1, &fence, VK_TRUE, m_TimeoutNs));
        }
        CHECK_VK_RESULT(m_pVulkanDev->ResetFences(1, &fence));
    }
    if (resetAfterExec)
    {
        CHECK_VK_RESULT(ResetCmdBuffer());
    }
    return (res);
}
VkResult VulkanCmdBuffer::ExelwteCmdBuffer (const bool waitOnFence, bool resetAfterExec)
{
    return ExelwteCmdBuffer(
        VK_NULL_HANDLE,
        0,
        nullptr,
        VkPipelineStageFlags(0),
        waitOnFence,
        VK_NULL_HANDLE,
        false,
        resetAfterExec);
}

//--------------------------------------------------------------------
VkResult VulkanCmdBuffer::EndCmdBuffer()
{
    MASSERT(m_CmdBuffer != VK_NULL_HANDLE);
    MASSERT(!m_RecordingEnded);
    VkResult res = VK_SUCCESS;
    CHECK_VK_RESULT(m_pVulkanDev->EndCommandBuffer(m_CmdBuffer));
    m_RecordingEnded = true;
    return VK_SUCCESS;
}

//--------------------------------------------------------------------
VkResult VulkanCmdBuffer::ResetCmdBuffer(UINT32 flags)
{
    MASSERT(m_CmdBuffer != VK_NULL_HANDLE);
    m_RecordingBegun = false;
    m_RecordingEnded = false;
    return m_pVulkanDev->ResetCommandBuffer(m_CmdBuffer, flags);
}

