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

#pragma once

#include "vulkan/vkinstance.h"
#include "vulkan/vkcmdbuffer.h"
#include "vulkan/vkfence.h"
#include "vulkan/vksemaphore.h"
#include "vulkan/vkquery.h"

namespace VkFusion
{
    class Job
    {
        public:
            Job()  = default;
            ~Job() = default;
            Job(const Job&)            = delete;
            Job& operator=(const Job&) = delete;
            Job(Job&&)                 = default;
            Job& operator=(Job&&)      = default;

            struct Stats
            {
                UINT64 first      = ~0ull;
                UINT64 last       = 0;
                UINT64 minimum    = ~0ull;
                UINT64 maximum    = 0;
                UINT64 total      = 0;
                UINT32 numSamples = 0;

                void AddSample(UINT64 begin, UINT64 end);
                Stats& operator+=(const Stats& other);
                UINT64 GetAverage() const { return numSamples ? (total / numSamples) : 0; }
            };

            VkResult Initialize(VulkanDevice*  pVulkanDev,
                                VulkanCmdPool* pCmdPool,
                                UINT32         measureSubmissions);
            VkResult Begin(VulkanCmdBuffer** ppCmdBuffer);
            VkResult Execute(bool               signalSemaphore,
                             VkSemaphore        presentSemaphore,
                             const VkSemaphore* pWaitSemaphores   = nullptr,
                             UINT32             numWaitSemaphores = 0);
            VkResult Wait(double timeoutMs);
            void Cleanup();
            VkSemaphore GetSemaphore() const { return m_Semaphore.GetSemaphore(); }

            void ResetStats();
            GET_PROP(DurationStats,   const Stats&);

        private:
            VulkanCmdBuffer m_CmdBuf;
            VulkanFence     m_Fence;
            VulkanSemaphore m_Semaphore;
            VulkanQuery     m_TimestampQuery;
            Stats           m_DurationStats;
            UINT32          m_QueryIdx   = 0;
            UINT32          m_QueryCount = 0;
            bool            m_Pending    = false;
    };
}
