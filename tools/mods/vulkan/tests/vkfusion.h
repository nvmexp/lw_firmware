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

#ifndef VULKAN_STANDALONE
#include "gpu/tests/gputest.h"
#include "core/include/tasker.h"
#include "opengl/modsgl.h"
#endif

#include "vulkan/vkinstance.h"
#include "vkfusion/vkfusion_job.h"
#include "vkfusion/vkfusion_graphics.h"
#include "vkfusion/vkfusion_raytracing.h"
#include "vkfusion/vkfusion_mats.h"

#include <atomic>

namespace VkFusion
{
    class Test final : public GpuTest
    {
        public:
            Test();
            virtual ~Test();

            RC AddExtraObjects(JSContext* cx, JSObject* obj) override;

            // JS accessors for testargs
            SETGET_PROP(RuntimeMs,         UINT32);
            SETGET_PROP(KeepRunning,       bool);
            SETGET_PROP(EnableValidation,  bool);
            SETGET_PROP(PrintExtensions,   bool);
            SETGET_PROP(Group0Us,          UINT32);
            SETGET_PROP(Group1Us,          UINT32);
            SETGET_PROP(Group2Us,          UINT32);
            SETGET_PROP(Group3Us,          UINT32);
            SETGET_PROP(DetachedPresentMs, UINT32);

        private:
            // Overrides of functions from GpuTest class
            void PrintJsProperties(Tee::Priority pri) override;
            bool IsSupported() override;
            RC   Setup()       override;
            RC   Cleanup()     override;
            RC   Run()         override;

            struct JobQueue
            {
                VulkanCmdPool cmdPool;
                vector<Job>   jobs;
            };

            struct SubtestData
            {
                Subtest*         pSubtest         = nullptr;
                vector<JobQueue> queues;
                atomic<UINT64>   nextFrame;
                UINT64           lastFrameChecked = ~0ULL;
                bool             continuous       = false;
                atomic<INT32>    rc{0};

                SubtestData() = default;
                SubtestData(SubtestData&& other);
            };

            struct SubtestGroup
            {
                vector<SubtestData*> subtests;         // Subtests in this group
                UINT32               targetTimeUs = 0; // How long to run this group (ideal time)
            };

            struct SubtestThread
            {
                SubtestData*     pSubtestData = nullptr;
                Tasker::ThreadID threadId     = Tasker::NULL_THREAD;
                atomic<bool>     exitFlag{false};

                explicit SubtestThread(SubtestData* pData)
                    : pSubtestData(pData) { }
                SubtestThread(SubtestThread&& other);
            };

            // Internal functions of the test
            RC AddExtraObject(JSContext*  cx,
                              JSObject*   containerObj,
                              JSObject*   protoObj,
                              const char* memberName,
                              JSClass*    classObj,
                              Subtest*    pSubtest);
            RC SelectQueues(QueueCounts* pOutQueues);
            RC SetupSubtestGroups();
            RC SetupJobQueues(SubtestData& subtest);
            RC CheckSubtestErrors() const;
            UINT64 GetNumScheduledFrames() const;
            RC SpawnContinuousSubtests();
            RC StopContinuousSubtests();
            void RunContinuousSubtest(SubtestThread* pThread);
            void GetGroupSemaphores(const SubtestGroup&  group,
                                    vector<VkSemaphore> *pSemaphores);
            RC ScheduleGroups();
            UINT32 AdjustRunTime(SubtestData& subtest);
            RC RenderNextFrame(SubtestData&       subtest,
                               bool               signalSemaphore   = false,
                               const VkSemaphore* pWaitSemaphores   = nullptr,
                               UINT32             numWaitSemaphores = 0);
            RC WaitForAllJobs(SubtestData& subtest);
            RC WaitForAllJobs();
            RC CheckFinalFrames(SubtestData& subtest);
            RC StartPresentThread();
            void StopPresentThread();

            // Testargs
            UINT32 m_RuntimeMs         = 5000;
            bool   m_KeepRunning       = false;
            bool   m_EnableValidation  = false;
            bool   m_PrintExtensions   = false;
            UINT32 m_Group0Us          = 1000;
            UINT32 m_Group1Us          = 1000;
            UINT32 m_Group2Us          = 1000;
            UINT32 m_Group3Us          = 1000;
            UINT32 m_DetachedPresentMs = 100;

#ifdef VULKAN_STANDALONE_KHR
            using FakeSwapChain = SwapChainKHR;
#else
            static constexpr UINT32 m_Hinstance = 0;
            static constexpr UINT32 m_HWindow   = 0;
            class FakeSwapChain
            {
                public:
                    FakeSwapChain(VulkanInstance*, VulkanDevice*) { }
                    VkResult Init(UINT32, UINT32, SwapChain::ImageMode, VkFence, UINT32, UINT32) const { return VK_SUCCESS; }
                    UINT32 GetNextSwapChainImage(VkSemaphore) const { return 0; }
                    VkResult PresentImage(UINT32, VkSemaphore, VkSemaphore, bool) const { return VK_SUCCESS; }
            };
#endif

            // Internal test objects
            mglTestContext            m_MglTestCtx;
            VulkanInstance            m_VulkanInst;
            VulkanDevice*             m_pVulkanDev  = nullptr;

            unique_ptr<FakeSwapChain> m_pSwapChain              = nullptr;
            Tasker::ThreadID          m_PresentThread           = Tasker::NULL_THREAD;
            Tasker::EventID           m_PresentThreadQuitEvent  = nullptr;
            VulkanSemaphore           m_PresentCompleteSem;
            VulkanFence               m_PresentFence;
            UINT32                    m_DetachedPresentQueueIdx = ~0U;

            vector<SubtestData>       m_Subtests;        // All active subtests selected by user
            vector<SubtestGroup>      m_Groups;          // Groups of subtests running together
            vector<SubtestThread>     m_ContinuousTests; // Subtests running continuously without pauses

#ifdef VULKAN_STANDALONE
        public:
#endif
            Graphics                  m_Graphics;
            Raytracing                m_Raytracing;
            Mats                      m_Mats;
    };
}
