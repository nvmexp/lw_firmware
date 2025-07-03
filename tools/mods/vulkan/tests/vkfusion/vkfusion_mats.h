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

#pragma once

#include "vkfusion_subtest.h"
#include "vulkan/vkcmdbuffer.h"
#include "core/utility/ptrnclss.h"

class MemError;

namespace VkFusion
{
    class Mats final: public Subtest
    {
        public:
            Mats();

            // Subtest overrides
            QueueCounts GetRequiredQueues() const override;
            ExtensionList GetRequiredExtensions() const override;
            RC Setup() override;
            RC Cleanup() override;
            void PreRun() override;
            RC Execute
            (
                UINT32            jobId,
                VulkanCmdBuffer** pCmdBufs,
                UINT32            numCmdBufs,
                VkSemaphore*      pJobWaitSema,
                VkSemaphore*      pPresentSema
            ) override;
            RC CheckFrame(UINT32 jobId, UINT64 frameId, bool finalFrame) override;
            RC Dump(UINT32 jobId, UINT64 frameId) override;
            void UpdateAndPrintStats(UINT64 elapsedMs) override;

            // Accessor functions for test configuration
            SETGET_PROP(MaxFbMb,           double);
            SETGET_PROP(BoxSizeKb,         UINT32);
            SETGET_PROP(UseSysmem,         bool);
            SETGET_PROP(NumBlitLoops,      UINT32);
            SETGET_PROP(NumGraphicsQueues, UINT32);
            SETGET_PROP(NumComputeQueues,  UINT32);
            SETGET_PROP(NumTransferQueues, UINT32);
            GET_PROP(   TotalGB,           UINT64);
            GET_PROP(   GBPerSecond,       double);
            void SetMemError(MemError& memError) { m_pMemError = &memError; }

        private:
            static RC HandleError(void*         pCookie,
                                  UINT32        offset,
                                  UINT32        expected,
                                  UINT32        actual,
                                  UINT32        patternOffset,
                                  const string& patternName);
            RC HandleError(UINT32        offset,
                           UINT32        expected,
                           UINT32        actual,
                           UINT32        patternOffset,
                           const string& patternName);

            struct Job
            {
                // Secondary command buffers with box blits for each queue
                vector<VulkanCmdBuffer> cmdBuf;
                // Number of bytes read & written in one submission of this job
                UINT32                  bytesTransferred = 0;
                // How many times this job has been submitted to the GPU
                UINT32                  numRuns = 0;
            };

            // Test args
            double m_MaxFbMb           = 64.0;
            UINT32 m_BoxSizeKb         = 8192u;
            bool   m_UseSysmem         = false;
            UINT32 m_NumBlitLoops      = 1u;
            UINT32 m_NumGraphicsQueues = 0u;
            UINT32 m_NumComputeQueues  = 0u;
            UINT32 m_NumTransferQueues = 1u;
            UINT64 m_TotalGB           = 0u;
            double m_GBPerSecond       = 0.0;

            Random                    m_Random;
            vector<Job>               m_Jobs;
            vector<VulkanImage>       m_Memory;
            vector<VulkanCmdPool>     m_CmdPools;

            MemError*                 m_pMemError = nullptr;
            PatternClass              m_PatternClass;
            PatternClass::PatternSets m_PatternSet;
            vector<UINT32>            m_BoxToPattern;      // For each box, index of the pattern the box was filled with
            UINT32                    m_BoxesPerImage = 0; // Number of boxes in each VulkanImage
            UINT32                    m_BoxesPerQueue = 0; // Number of boxes processed by each queue
            UINT32                    m_LastJobId     = 0; // Index of last scheduled job
    };
}
