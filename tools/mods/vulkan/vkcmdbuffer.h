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
#ifndef VK_CMDBUFFER_H
#define VK_CMDBUFFER_H

#include "vkmain.h"

#ifndef TRACY_ENABLE
typedef struct TracyVkCtxStub* TracyVkCtx;
#define TracyVkContext(physdev, dev, queue, cmdbuf) nullptr
#define TracyVkContextCalibrated(physdev, dev, queue, cmdbuf, gpdctd, gct) nullptr
#endif

class VulkanDevice;

//--------------------------------------------------------------------
class VulkanCmdPool
{
public:
    VulkanCmdPool() = default;
    explicit VulkanCmdPool(VulkanDevice* pVulkanDev);
    ~VulkanCmdPool();
    VulkanCmdPool(VulkanCmdPool&& vulkCmdPool) noexcept;
    VulkanCmdPool& operator=(VulkanCmdPool&& vulkCmdPool);
    VulkanCmdPool(const VulkanCmdPool& vulkCmdPool) = delete;
    VulkanCmdPool& operator=(const VulkanCmdPool& vulkCmdPool) = delete;

    VkResult   InitCmdPool(UINT32 queueFamilyIdx,
                           UINT32 queueIdx,
                           UINT32 flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    void       DestroyCmdPool();
    VkResult   Reset(UINT32 flags = 0);

    SETGET_PROP(QueueFamilyIdx, UINT32);
    SETGET_PROP(QueueIdx, UINT32);
    SETGET_PROP(Queue,    VkQueue);
    GET_PROP(CmdPool, VkCommandPool);

    void InitTracyCtx(VkCommandBuffer cmdBuffer);
    TracyVkCtx GetTracyCtx() const { return m_TracyCtx; }

private:
    VulkanDevice              *m_pVulkanDev = nullptr;
    VkCommandPool             m_CmdPool = VK_NULL_HANDLE;
    VkQueue                   m_Queue = nullptr;
    TracyVkCtx                m_TracyCtx = nullptr;
    UINT32                    m_QueueFamilyIdx = ~0;
    UINT32                    m_QueueIdx = ~0;

    void TakeResources(VulkanCmdPool&& other);
};

constexpr bool WAIT_ON_FENCE = true;
constexpr bool RESET_AFTER_EXEC = true;
//--------------------------------------------------------------------
class VulkanCmdBuffer
{
public:
    VulkanCmdBuffer() = default;
    explicit VulkanCmdBuffer(VulkanDevice* pVulkanDev);
    ~VulkanCmdBuffer();
    VulkanCmdBuffer(VulkanCmdBuffer&& vulkCmdBuf) noexcept;
    VulkanCmdBuffer& operator=(VulkanCmdBuffer&& vulkCmdBuf);
    VulkanCmdBuffer(const VulkanCmdBuffer& vulkCmdBuf) = delete;
    VulkanCmdBuffer& operator=(const VulkanCmdBuffer& vulkCmdBuf) = delete;

    VkResult   AllocateCmdBuffer(VulkanCmdPool*       pCmdPool,
                                 VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    void       FreeCmdBuffer();

    VkResult   BeginCmdBuffer(UINT32                                flags = 0,
                              const VkCommandBufferInheritanceInfo* pInheritanceInfo = nullptr);
    VkResult   ExelwteCmdBuffer(bool waitOnFence, bool resetAfterExec = false);
    VkResult   ExelwteCmdBuffer
    (
        VkSemaphore waitSemaphore,
        UINT32 numSignalSemaphores,
        VkSemaphore *pSignalSemaphores,
        VkPipelineStageFlags waitStageMask,
        bool waitOnFence,
        VkFence fence,
        bool usePolling = false,
        bool resetAfterExec = false
    );

    VkResult   EndCmdBuffer();
    VkResult   ResetCmdBuffer(UINT32 flags = 0);

    GET_PROP(CmdBuffer,         VkCommandBuffer);
    GET_PROP(RecordingBegun,    bool)
    GET_PROP(RecordingEnded,    bool)
    SETGET_PROP(TimeoutNs,      UINT64);
    VulkanDevice* GetVulkanDevice() const { return m_pVulkanDev; }
    VulkanCmdPool* GetCmdPool() const { return m_pVulkanCmdPool; }
    TracyVkCtx GetTracyCtx() const { return m_pVulkanCmdPool->GetTracyCtx(); }

private:
    VulkanDevice              *m_pVulkanDev = nullptr;

    VulkanCmdPool             *m_pVulkanCmdPool = nullptr;
    VkCommandBuffer           m_CmdBuffer = VK_NULL_HANDLE;

    bool                      m_RecordingBegun = false;
    bool                      m_RecordingEnded = false;

    UINT64                    m_TimeoutNs = FENCE_TIMEOUT;

    void TakeResources(VulkanCmdBuffer&& other);
};

#endif
