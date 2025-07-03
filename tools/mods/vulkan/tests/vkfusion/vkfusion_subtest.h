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

class VulkanCmdBuffer;
class VulkanImage;

namespace VkFusion
{
    struct Queue
    {
        UINT32 family;
        UINT32 idx;
        UINT32 caps;

        Queue(UINT32 family, UINT32 idx, UINT32 caps)
        : family(family), idx(idx), caps(caps) { }
    };

    struct QueueCounts
    {
        UINT32 graphics = 0u;
        UINT32 compute  = 0u;
        UINT32 transfer = 0u;
    };

    struct ExtensionList
    {
        const char* const* extensions;
        UINT32             numExtensions;
    };

    //! Subtest base class
    //!
    //! Subtests exercise concrete functionality, e.g. RT cores, tensor cores, etc.
    class Subtest
    {
        public:
            virtual ~Subtest() = default;

            //! Returns how many of each type of queue is required by the subtest
            //!
            //! That many primary command buffers of each type will be created and
            //! passed in Execute().
            virtual QueueCounts GetRequiredQueues() const = 0;

            //! Returns list of extensions required by the subtest
            virtual ExtensionList GetRequiredExtensions() const = 0;

            //! Allocates resources and bakes in secondary command buffers
            //!
            //! The implementation must take into account m_NumJobs member, because subsequent
            //! ilwocations of Execute() will reference jobId.
            //!
            //! @param numJobs Number of jobs scheduled simultaneously for which to prepare resources
            virtual RC Setup() = 0;

            //! Releases all resources
            virtual RC Cleanup() = 0;

            //! Called once at the beginning of Run()
            virtual void PreRun() { }

            //! Runs single workload on specific primary command buffers
            //!
            //! The implementation should add necessary commands to the primary command
            //! buffer(s) to dump images if m_DumpPng is set.
            //!
            //! @param jobId        Zero-based job index, up to m_NumJobs-1.
            //! @param pCmdBufs     Primary command buffers for each queue.
            //! @param numCmdBufs   Number of primary command buffers in pCmdBufs. This is equal
            //!                     to the total number of queues requested by GetRequiredQueues().
            //! @param pJobWaitSema Execute() may return optional semaphore on which vkQueueSubmit()
            //!                     will wait before being exelwted.  This is typically to
            //!                     wait on image from vkAcquireNextImageKHR().
            //! @param pPresentSema Execute() may return optional Semaphore on which vkQueuePresent()
            //!                     will wait.  This semaphore will be signaled by vkQueueSubmit()
            //!                     and will then be passed to Present().
            virtual RC Execute
            (
                UINT32            jobId,
                VulkanCmdBuffer** pCmdBufs,
                UINT32            numCmdBufs,
                VkSemaphore*      pJobWaitSema,
                VkSemaphore*      pPresentSema
            ) = 0;

            //! Performs Present()
            virtual RC Present(VkSemaphore presentSema) { return RC::OK; }

            //! Checks test output after a job has been completed
            //!
            //! @param jobId      Zero-based job index for which to check the results
            //! @param frameId    Zero-based frame index for which to check the results
            //! @param finalFrame True is this is the frame in this job bucket
            virtual RC CheckFrame(UINT32 jobId, UINT64 frameId, bool finalFrame) = 0;

            //! Dumps output surfaces
            //!
            //! This function is ilwoked after a job created by Execute() has already been
            //! completed.
            virtual RC Dump(UINT32 jobId, UINT64 frameId) = 0;

            //! Updates statistics tracked internally in the subtest and prints
            //! the statistics using VerbosePrintf
            //!
            //! @param elapsedMs How long it took to run the subtest
            virtual void UpdateAndPrintStats(UINT64 elapsedMs) = 0;

            void SetInstance(VulkanInstance* pInst)
            {
                m_pVulkanInst = pInst;
                m_pVulkanDev  = pInst->GetVulkanDevice();
            }
            void SetQueues(vector<Queue> queues) { m_Queues = move(queues); }
            const vector<Queue>& GetQueues() const { return m_Queues; }

            //! Forces MODS swap chain to be used instead of KHR swap chain (only standalone)
            void ForceModsSwapChain() { m_bModsSwapChain = true; }

            //! Sets up data specific to Windows swap chain
#ifdef VULKAN_STANDALONE_KHR
            void SetWindowsSwapChain(HINSTANCE hInstance, HWND hWindow)
            {
                m_HInstance = hInstance;
                m_HWindow   = hWindow;
            }
#else
            void SetWindowsSwapChain(UINT32, UINT32) { }
#endif

            SETGET_PROP(VerbosePri, INT32);
            SETGET_PROP(TimeoutMs,  INT32);
            SETGET_PROP(RandomSeed, UINT32);
            SETGET_PROP(RunMask,    UINT32);
            SETGET_PROP(NumJobs,    UINT32);
            SETGET_PROP(DumpPng,    bool);
            SETGET_PROP(ShaderReplacement, bool);
            GET_PROP   (Name,       const char*);

        protected:
            Subtest(const char* name, UINT32 numJobs)
                : m_Name(name), m_NumJobs(numJobs) { }
#ifndef VerbosePrintf
            void VerbosePrintf(const char* format, ...) const;
#endif
            VkResult AllocateImage(VulkanImage*          pImage,
                                   VkFormat              format,
                                   UINT32                width,
                                   UINT32                height,
                                   UINT32                mipmapLevels,
                                   VkImageUsageFlags     usage,
                                   VkImageTiling         tiling,
                                   VkMemoryPropertyFlags location) const;

            const char*     m_Name           = nullptr;
            VulkanInstance* m_pVulkanInst    = nullptr;
            VulkanDevice*   m_pVulkanDev     = nullptr;
            vector<Queue>   m_Queues;
            INT32           m_VerbosePri     = Tee::PriLow;
            UINT32          m_TimeoutMs      = 1000;
            UINT32          m_RandomSeed     = 1;
            UINT32          m_RunMask        = 0;
            UINT32          m_NumJobs        = 0;
            bool            m_DumpPng        = false;
            bool            m_ShaderReplacement = false;
            bool            m_bModsSwapChain = false; // For standalone, ignored in MODS
#ifdef VULKAN_STANDALONE_KHR
            HINSTANCE       m_HInstance      = 0;
            HWND            m_HWindow        = 0;
#endif
    };
}
