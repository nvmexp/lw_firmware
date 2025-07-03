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
#include "vulkan/swapchain.h"
#include "vulkan/vkasgen.h"
#include "vulkan/vkcmdbuffer.h"
#include "vulkan/vkbuffer.h"
#include "vulkan/vkdescriptor.h"
#include "vulkan/vkfence.h"
#include "vulkan/vkframebuffer.h"
#include "vulkan/vkpipeline.h"
#include "vulkan/vkrenderpass.h"
#include "vulkan/vksampler.h"
#include "vulkan/vksemaphore.h"
#include "vulkan/vkshader.h"

namespace VkFusion
{
    class Graphics final: public Subtest
    {
        public:
            Graphics();

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
            RC Present(VkSemaphore presentSema) override;
            RC CheckFrame(UINT32 jobId, UINT64 frameId, bool finalFrame) override;
            RC Dump(UINT32 jobId, UINT64 frameId) override;
            void UpdateAndPrintStats(UINT64 elapsedMs) override;

            // Accessor functions for test configuration
            SETGET_PROP(SurfaceWidth,    UINT32);
            SETGET_PROP(SurfaceHeight,   UINT32);
            SETGET_PROP(SurfaceBits,     UINT32);
            SETGET_PROP(DrawsPerFrame,   UINT32);
            SETGET_PROP(NumLights,       UINT32);
            SETGET_PROP(GeometryType,    UINT32);
            SETGET_PROP(NumVertices,     UINT32);
            SETGET_PROP(Fov,             UINT32);
            SETGET_PROP(MajorR,          double);
            SETGET_PROP(MinorR,          double);
            SETGET_PROP(TorusMajor,      bool);
            SETGET_PROP(Lwll,            bool);
            SETGET_PROP(Rotation,        double);
            SETGET_PROP(RotationStep,    double);
            SETGET_PROP(NumTexReads,     UINT32);
            SETGET_PROP(NumTextures,     UINT32);
            SETGET_PROP(TexSize,         UINT32);
            SETGET_PROP(MipLodBias,      double);
            SETGET_PROP(DebugMipMaps,    bool);
            SETGET_PROP(RaysPerPixel,    UINT32);
            SETGET_PROP(RTGeometryType,  UINT32);
            SETGET_PROP(RTTLASWidth,     UINT32);
            SETGET_PROP(RTXMask,         UINT32);
            SETGET_PROP(Check,           bool);
            SETGET_PROP(ShaderReplacement, bool);
            GET_PROP(   TotalDraws,      UINT64);
            GET_PROP(   TotalPixels,     UINT64);
            GET_PROP(   DrawsPerSecond,  double);
            GET_PROP(   PixelsPerSecond, double);

            static constexpr UINT32 defaultDrawsPerFrame = ~0U;

        private:
            struct Job
            {
                VulkanImage     goldenImage;
                VulkanImage     dumpImage;
                VulkanSemaphore waitSema;
                VulkanSemaphore swapSema;
                UINT64          numChecks = 0;
            };

            RC SetupJob(VulkanCmdBuffer& auxCmdBuf, UINT32 jobId);
            RC SetupSwapChain();
            RC SetupDepthImages(VulkanCmdBuffer& auxCmdBuf);
            RC SetupGoldenImages(VulkanCmdBuffer& auxCmdBuf);
            RC SetupGeometry(VulkanCmdBuffer& auxCmdBuf);
            RC SetupRTGeometry(VulkanCmdBuffer& auxCmdBuf);
            RC GenerateTorus
            (
                VulkanCmdBuffer& auxCmdBuf,
                UINT32           vtxPerRib,
                float            majorR,
                float            minorR,
                bool             torusMajor,
                UINT32*          pNumVertices,
                UINT32*          pNumIndices,
                VulkanBuffer*    pVertexBuf,
                VulkanBuffer*    pIndexBuf
            );
            RC GenerateLwbe
            (
                VulkanCmdBuffer& auxCmdBuf,
                UINT32*          pNumVertices,
                UINT32*          pNumIndices,
                VulkanBuffer*    pVertexBuf,
                VulkanBuffer*    pIndexBuf
            );
            RC SetupRenderPasses();
            RC SetupFrameBuffers();
            RC SetupDrawRenderPass(VulkanRenderPass* pRenderPass, VkAttachmentLoadOp loadOp);
            RC SetupPipeline(VulkanCmdBuffer& auxCmdBuf);
            RC SetupTransforms(VulkanCmdBuffer& auxCmdBuf);
            RC SetupSamplers();
            RC SetupTextures(VulkanCmdBuffer& auxCmdBuf);
            RC SetupErrorCounter(VulkanCmdBuffer& auxCmdBuf);
            static void DebugFillMipMap(UINT32* pFirstPixel, UINT32 numPixels, UINT32 mipLevel);
            void UpdateDescriptorSets(UINT32 jobId);
            RC BuildCmdBuffer(UINT32 jobId);
            RC FillCmdBuffer(VulkanCmdBuffer& cmdBuf, UINT32 jobId, UINT32 iDraw);
            RC CheckResults(VulkanCmdBuffer& cmdBuf, VulkanImage& srcImg, UINT32 jobId);
            RC CopyOutputImage(VulkanCmdBuffer& cmdBuf, VulkanImage& srcImg, VulkanImage& dstImg);
            void ColwertSignedFloatToUser(void* ptr, UINT64 numBytes);

            // Values of the GeometryType testarg
            enum GeometryType
            {
                geomVertexBuffer = 0,
                geomTessellation = 1,
            };

            enum RTGeometryType
            {
                geomLwbe  = 0,
                geomTorus = 1,
            };

            // Test args
            UINT32 m_SurfaceWidth    = 0;
            UINT32 m_SurfaceHeight   = 0;
            UINT32 m_SurfaceBits     = 64;
            UINT32 m_DrawsPerFrame   = defaultDrawsPerFrame;
            UINT32 m_NumLights       = 4;
            UINT32 m_GeometryType    = geomVertexBuffer;
            UINT32 m_NumVertices     = 2332800;
            UINT32 m_Fov             = 41;
            double m_MajorR          = 0.7;
            double m_MinorR          = 0.67;
            bool   m_TorusMajor      = false;
            bool   m_Lwll            = true;
            double m_Rotation        = 85;
            double m_RotationStep    = 180.0 / 256.0;
            UINT32 m_NumTexReads     = 9;
            UINT32 m_NumTextures     = 2;
            UINT32 m_TexSize         = 256;
            double m_MipLodBias      = 0;
            bool   m_DebugMipMaps    = false;
            UINT32 m_RaysPerPixel    = 1;
            UINT32 m_RTGeometryType  = geomLwbe;
            UINT32 m_RTTLASWidth     = 2;
            UINT32 m_RTXMask         = 1;
            bool   m_Check           = true;
            bool   m_ShaderReplacement = false;
            UINT64 m_TotalDraws      = 0;
            UINT64 m_TotalPixels     = 0;
            double m_DrawsPerSecond  = 0;
            double m_PixelsPerSecond = 0;

            Random                    m_Random;
            vector<Job>               m_Jobs;
            VulkanCmdPool             m_CmdPool;
            unique_ptr<SwapChain>     m_pSwapChain;
            vector<VulkanImage>       m_DepthImages;
            vector<VulkanFrameBuffer> m_FrameBuffers;
            VulkanFrameBuffer         m_ErrorCountFrameBuffer;
            VulkanImage               m_ErrorCountImage;
            VulkanRenderPass          m_DrawRenderPass;
            VulkanRenderPass          m_ClearRenderPass;
            VulkanRenderPass          m_ErrorCountRenderPass;
            VulkanPipeline            m_DrawPipeline;
            VulkanPipeline            m_ErrorCountPipeline;
            VulkanBuffer              m_VertexBuf;
            VulkanBuffer              m_IndexBuf;
            VulkanBuffer              m_UniformDataBuf;
            VulkanFence               m_PresentFence;
            VulkanSampler             m_ErrorCountSampler;
            VulkanQuery               m_StatsQuery;
            vector<VulkanSampler>     m_Samplers;
            vector<VulkanImage>       m_Textures;
            UINT32                    m_VtxPerRib        = 0;
            UINT32                    m_NumIndices       = 0;
            UINT32                    m_UniformDataSize  = 0;
            UINT32                    m_LastSwapChainIdx = 0;
            UINT32                    m_LastJobId        = 0;
            DescriptorInfo            m_DrawDescriptorInfo;
            DescriptorInfo            m_ErrorCountDescriptorInfo;

            struct RT
            {
                VulkanRaytracingBuilder builder;
                UINT32                  numVertices = 10;
                UINT32                  numIndices  = 0;
            };

            RT                        m_RT;

            VkPhysicalDeviceAccelerationStructurePropertiesKHR m_ASProperties = { };
    };
}
