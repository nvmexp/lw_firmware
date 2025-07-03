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
#ifndef VK_GEOMETRY_H
#define VK_GEOMETRY_H

#include "vkmain.h"
#include "util.h"
#include "vkinstance.h"
#include "vkimage.h"
#include "vkcmdbuffer.h"
#include "vkbuffer.h"
#include "vksampler.h"
#include "vktexture.h"
#include "vkpipeline.h"
#include "vkframebuffer.h"
#include "vkshader.h"
#include "vkrenderpass.h"
#include "vkdescriptor.h"
#include "swapchain.h"
#include "vkgoldensurfaces.h"

#ifndef VULKAN_STANDALONE
#include "core/include/jscript.h"
#include "gpu/tests/gputest.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#endif

#define ENABLE_VK_DEBUG         true
#define GPUINDEX                0    // Init Vulkan on first device

//--------------------------------------------------------------------
//! \brief Vulkan based test
//!
class VkTriangle : public GpuTest
{
public:
    VkTriangle();

#ifndef VULKAN_STANDALONE
    bool                      IsSupported() override;
#endif

    RC                        Setup() override;
    RC                        Run() override;
    RC                        Cleanup() override;

#ifndef VULKAN_STANDALONE
    void                      PrintJsProperties(Tee::Priority pri) override;
    RC                        InitFromJs() override;
#endif

    RC                        SetupDepthBuffer();
    RC                        SetupBuffers();
    RC                        SetupTextures();

    RC                        CreateRenderPass();

    RC                        SetupDescriptors();
    RC                        SetupShaders();
    RC                        SetupFrameBuffer();

    RC                        CreatePipeline();
    RC                        SetupSemaphoresAndFences();

    RC                        BuildDrawCmdBuffers();
    RC                        SubmitCmdBufferAndPresent(UINT32 loopIdx);
    RC                        UpdateBuffersPerFrame();

    void                      CleanupAfterSetup();

    SETGET_PROP(CompressTexture,    bool);
    SETGET_PROP(EnableValidation,   bool);
    SETGET_PROP(NumFramesToDraw,    UINT32);
    SETGET_PROP(TriRotationAngle,   float);
    SETGET_PROP(IndexedDraw,        bool);
    SETGET_PROP(SkipPresent,        bool);
    SETGET_PROP(ShaderReplacement,  bool);
    SETGET_PROP(TestMode,           UINT32);
    SETGET_PROP(GeometryType,       UINT32);
    SETGET_PROP(SampleCount,        UINT32);
#ifdef VULKAN_STANDALONE
    SETGET_PROP(CheckGoldens,       bool);
#endif
private:
    enum
    {
        TEST_MODE_STATIC,   // Draw command buffers are created once and stored statically
        TEST_MODE_DYNAMIC   // Draw command buffers are created just before
                            // presenting, data is changed dynamically
    };

#ifndef VULKAN_STANDALONE
    mglTestContext            m_mglTestContext;
#endif

    //Vulkan
    VulkanInstance            m_VulkanInst;
    VulkanDevice             *m_pVulkanDev = nullptr;

#ifndef VULKAN_STANDALONE
    unique_ptr<SwapChainMods> m_SwapChain = nullptr;
#else
    unique_ptr<SwapChainKHR>  m_SwapChain = nullptr;
    bool                      m_CheckGoldens = true;
#endif
    unique_ptr<VulkanImage>   m_DepthImage = nullptr;

    unique_ptr<VulkanBuffer>  m_HUniformBufferMVPMatrix = nullptr;    //host accessible memory
    unique_ptr<VulkanBuffer>  m_UniformBufferMVPMatrix = nullptr;

    //Vertex buffers, ost acce
    unique_ptr<VulkanBuffer>  m_VBHPositions = nullptr;
    unique_ptr<VulkanBuffer>  m_VBHNormals = nullptr;
    unique_ptr<VulkanBuffer>  m_VBHColors = nullptr;
    unique_ptr<VulkanBuffer>  m_VBHTexCoords = nullptr;

    //Vertex buffers, on devic
    unique_ptr<VulkanBuffer>  m_VBPositions = nullptr;
    unique_ptr<VulkanBuffer>  m_VBNormals = nullptr;
    unique_ptr<VulkanBuffer>  m_VBColors = nullptr;
    unique_ptr<VulkanBuffer>  m_VBTexCoords = nullptr;

    //Index buffer
    unique_ptr<VulkanBuffer>  m_HIndexBuffer = nullptr;
    unique_ptr<VulkanBuffer>  m_IndexBuffer = nullptr;

    VBBindingAttributeDesc    m_VBBindingAttributeDesc;

    //buffer bindings
    UINT32                    m_VBBindingPositions = 0;
    UINT32                    m_VBBindingNormals = 1;
    UINT32                    m_VBBindingColors = 2;
    UINT32                    m_VBBindingTexCoords = 3;

    unique_ptr<VulkanTexture> m_HTexture = nullptr;
    unique_ptr<VulkanTexture> m_Texture = nullptr;
    VulkanSampler             m_Sampler = {};

    //Command buffers
    unique_ptr<VulkanCmdPool> m_CmdPool = nullptr;
    unique_ptr<VulkanCmdBuffer> m_InitCmdBuffer = nullptr;
    vector<unique_ptr<VulkanCmdBuffer>> m_DrawCmdBuffers;

    unique_ptr<VulkanRenderPass> m_RenderPass = nullptr;

    unique_ptr<DescriptorInfo> m_DescriptorInfo = nullptr;

    //Shaders
    unique_ptr<VulkanShader>  m_VertexShader = nullptr;
    unique_ptr<VulkanShader>  m_FragmentShader = nullptr;

    vector<unique_ptr<VulkanFrameBuffer>> m_FrameBuffers;

    unique_ptr<VulkanPipeline> m_Pipeline = nullptr;

    VkSemaphore               m_RenderCompleteToGoldensSem = VK_NULL_HANDLE;
    VkSemaphore               m_PresentCompleteSem = VK_NULL_HANDLE;

    VkFence                   m_Fence = VK_NULL_HANDLE;

    VkViewport                m_Viewport = {};
    VkRect2D                  m_Scissor = {};

    //Geometry and color related properties
    VkGeometry                m_Geometry;
    UINT32                    m_NumVerticesToDraw = 0;
    UINT32                    m_NumIndicesToDraw = 0;
    VkClearColorValue         m_ClearColor = {{ 0.5f, 0.5f, 0.5f, 1.0f }};

    GLCamera                  m_Camera;

    //Callwlate time
    Time                      m_SetupTimeCallwlator;
    Time                      m_BuildDrawTimeCallwlator;
    Time                      m_RunTimeCallwlatorPerFrame;

    //Test properties
    bool                      m_CompressTexture = false;
    bool                      m_EnableValidation = false;
    UINT32                    m_NumFramesToDraw = 3;
    float                     m_TriRotationAngle = 1.0f;
    float                     m_ModelOrientationDeg = 0.0f;
    float                     m_ModelOrientationXDeg = 0.0f;
    float                     m_ModelOrientationYDeg = 0.0f;
    bool                      m_IndexedDraw = true;
    bool                      m_SkipPresent = false;
    bool                      m_ShaderReplacement = false;

    UINT32                    m_TestMode = TEST_MODE_DYNAMIC;
    UINT32                    m_GeometryType = VkGeometry::GEOMETRY_TRIANGLE;

    vector<unique_ptr<VulkanGoldenSurfaces>> m_GoldenSurfaces;
    Goldelwalues             *m_pGolden = nullptr;

    RC                        CreateGoldenSurfaces();

    void                      AllocateVulkanDeviceResources();

    RC                        SetupTest();
    RC                        RunTest();
    RC                        CleanupTest();

    UINT32                    m_SampleCount = 4;

    unique_ptr<VulkanImage>   m_ColorMsaaImage = nullptr;
    RC                        SetupMsaaImage();
};

#endif
