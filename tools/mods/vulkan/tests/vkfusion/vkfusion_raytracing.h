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

#include "vkfusion_subtest.h"
#include "vulkan/vkshader.h"
#include "vulkan/vkasgen.h"

namespace VkFusion
{
    class Raytracing final: public Subtest
    {
        public:
            Raytracing() : Subtest("Raytracing", 4) { }

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
            SETGET_PROP(SurfaceWidth,  UINT32);
            SETGET_PROP(SurfaceHeight, UINT32);
            SETGET_PROP(NumLwbesXY,    UINT32);
            SETGET_PROP(NumLwbesZ,     UINT32);
            SETGET_PROP(BLASSize,      UINT32);
            SETGET_PROP(MinLwbeSize,   double);
            SETGET_PROP(MaxLwbeSize,   double);
            SETGET_PROP(MinLwbeOffset, double);
            SETGET_PROP(MaxLwbeOffset, double);
            SETGET_PROP(GeometryType,  UINT32);
            SETGET_PROP(GeometryLevel, UINT32);
            SETGET_PROP(Check,         bool);
            SETGET_PROP(ShaderReplacement, bool);
            GET_PROP(   TotalRays,     UINT64);
            GET_PROP(   RaysPerSecond, double);

        private:
            enum Behavior
            {
                Render, // write to target, write 0 to errors
                Check   // read from target, test, write errors
            };

            struct Job
            {
                VulkanCmdBuffer cmdBuf[2];   // Secondary command buffers with RT commands
                VulkanImage     target;      // Image where RT output is stored
                VulkanImage     errors;      // Image containing per-pixel error counts
                VulkanImage     dumpImage;   // Host image when -dump_png is enabled (for debugging)
                UINT32          numRenders     = 0;
                UINT32          rayPerturbSeed = 0;
            };

            RC SetupJobResources(UINT32 jobId, VulkanCmdBuffer& auxCmdBuf);
            RC SetupGeometry(VulkanCmdBuffer& auxCmdBuf);
            RC SetupPipeline(VulkanCmdBuffer& auxCmdBuf);
            void UpdateDescriptorSets(UINT32 jobId);
            RC BuildCmdBuffer(UINT32 jobId, Behavior behavior);
            RC FillCmdBuffer(VulkanCmdBuffer& cmdBuf, UINT32 jobId, Behavior behavior);

            // Test args
            UINT32 m_SurfaceWidth  = 0;
            UINT32 m_SurfaceHeight = 0;
            UINT32 m_NumLwbesXY    = 100;
            UINT32 m_NumLwbesZ     = 100;
            UINT32 m_BLASSize      = 6;
            double m_MinLwbeSize   = 0.1;
            double m_MaxLwbeSize   = 0.4;
            double m_MinLwbeOffset = -0.1;
            double m_MaxLwbeOffset = 0.1;
            UINT32 m_GeometryType  = 0;
            UINT32 m_GeometryLevel = 1;
            bool   m_Check         = true;
            bool   m_ShaderReplacement = false;
            UINT64 m_TotalRays     = 0;
            double m_RaysPerSecond = 0;

            VkPhysicalDeviceRayTracingPipelinePropertiesKHR    m_RTProperties = { };
            VkPhysicalDeviceAccelerationStructurePropertiesKHR m_ASProperties = { };

            Random                  m_Random;
            UINT32                  m_ShaderGroupHandleSize = 0;

            VulkanRaytracingBuilder m_Builder;
            DescriptorInfo          m_DescriptorInfo;
            VulkanPipeline          m_RTPipeline;
            VulkanCmdPool           m_CmdPool;

            VulkanBuffer            m_GeometryData;
            VulkanBuffer            m_ShaderBindingTable;

            VulkanShader            m_RayGenRenderShader;
            VulkanShader            m_RayGenCheckShader;
            VulkanShader            m_IntersectAABBShader;
            VulkanShader            m_HitShader;
            VulkanShader            m_MissShader;

            vector<Job>             m_Jobs;
    };
}
