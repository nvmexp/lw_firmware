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
#ifndef VK_PIPELINE_H
#define VK_PIPELINE_H

#include "vkmain.h"

class VBBindingAttributeDesc;
class VulkanDevice;

//--------------------------------------------------------------------
// TODO: We need to implement more APIs to configure what state is dynamic vs static.
//       When the state is configured to be dynamic, there are a special set of APIs
//       that can be called to dynamically set the state.
//       If the state is configured to be static, the state must be set using the control
//       structures for that specific state. When the state is static it actually prevents
//       the state from being changed using the special set of APIs. This could be a good
//       or bad thing depending on how you want to control this state.
//
//       For example we have the following "Dynamic State"
//          Viewport
//          Scissor
//          LineWidth
//          DepthBias
//          BlendConstants
//          DepthBounds
//          StencilCompareMask
//          StencilWriteMask
//          StencilReference
//          ViewportWScaling
//          DiscardRectangleExt

class VulkanPipeline
{
public:
    VulkanPipeline() = default;
    explicit VulkanPipeline(VulkanDevice* pVulkanDev);
    ~VulkanPipeline();
    VulkanPipeline(VulkanPipeline&& pipeline) { TakeResources(move(pipeline)); }
    VulkanPipeline& operator=(VulkanPipeline&& pipeline) { TakeResources(move(pipeline)); return *this; }
    VulkanPipeline(const VulkanPipeline&) = delete;
    VulkanPipeline& operator=(const VulkanPipeline&) = delete;

    void Cleanup();
    void CreateDynamicState();

    void CreateVertexInputState(VBBindingAttributeDesc *pBindingAttributeDesc);
    void CreateTessellationState();
    void CreateInputAssemblyState();
    void CreateRasterizationState();
    void CreateColorBlendState();
    void CreateViewportAndScissorState();
    void CreateDepthStencilState();
    VkResult CreateMultisampleState(VkSampleCountFlagBits sampleCount);

    VkResult CreateGraphicsPipeline();
    VkResult CreateComputePipeline();
    VkResult CreateRaytracingPipeline(UINT32 maxRayRelwrsionDepth);

    VkResult SetupComputePipeline
    (
        VkDescriptorSetLayout descriptorSetLayout
        ,VkPipelineShaderStageCreateInfo shaderStage
    );
    VkResult SetupGraphicsPipeline
    (
        VBBindingAttributeDesc *pBindingAttributeDesc,
        VkDescriptorSetLayout descriptorSetLayout,
        vector<VkPipelineShaderStageCreateInfo> shaderStages,
        VkRenderPass renderPass,
        UINT32 subpass,
        VkSampleCountFlagBits sampleCount
    );
    VkResult CreateRaytracingPipelineLayout
    (
        VkDescriptorSetLayout descriptorSetLayout
    );
    VkResult CreateRaytracingPipelineLayout
    (
        vector<VkDescriptorSetLayout> layouts
    );

    VkResult SetupRaytracingPipeline
    (
        vector<VkDescriptorSetLayout>                descriptorSetLayouts,
        vector<VkPipelineShaderStageCreateInfo>      shaderStages,
        vector<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups,
        vector<VkPushConstantRange>                  pushConstantRanges,
        UINT32                                       setupFlags,
        UINT32                                       maxRayRelwrsionDepth
    );

    GET_PROP(Pipeline, VkPipeline);
    GET_PROP(PipelineLayout, VkPipelineLayout);
    SETGET_PROP(ParentPipeline, VkPipeline);
    void SetColorBlendState
    (
         const VkPipelineColorBlendAttachmentState& colorBlendAttachmentState,
         VkBool32  logicOpEnable,
         VkLogicOp logicOp,
         float     blendConstants[4]
    );
    void SetColorBlendState
    (
         vector<VkPipelineColorBlendAttachmentState> colorBlendAttachmentStates,
         VkBool32  logicOpEnable,
         VkLogicOp logicOp,
         float     blendConstants[4]
    );

    void SetDepthStencilState
    (
        VkPipelineDepthStencilStateCreateInfo *pDepthStencilState
    );

    // Adds one of the following dynamic states to the dynamic state enables vector.
    // VK_DYNAMIC_STATE_VIEWPORT
    // VK_DYNAMIC_STATE_SCISSOR
    // VK_DYNAMIC_STATE_LINE_WIDTH
    // VK_DYNAMIC_STATE_DEPTH_BIAS
    // VK_DYNAMIC_STATE_BLEND_CONSTANTS
    // VK_DYNAMIC_STATE_DEPTH_BOUNDS
    // VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK
    // VK_DYNAMIC_STATE_STENCIL_WRITE_MASK
    // VK_DYNAMIC_STATE_STENCIL_REFERENCE
    // VK_DYNAMIC_STATE_VIEWPORT_W_SCALING_LW
    // VK_DYNAMIC_STATE_DISCARD_RECTANGLE_EXT
    // Any dynamic state that is enabled requires the appropriate vkCmdSet??? API to be called
    // before any draw calls are made.
    void EnableDynamicState(VkDynamicState state);

    // Normally, the vertices are loaded from the vertex buffer by index in sequential order, but
    // with an element buffer you can specify the indices to use yourself. This allows you to
    // perform optimizations like reusing vertices. If you set the primitiveRestartEnable member to
    // VK_TRUE, then it's possible to break up lines and triangles in the _STRIP topology modes by
    // using a special index of 0xFFFF or 0xFFFFFFFF.
    void SetInputAssemblyTopology(VkPrimitiveTopology topology,
                                  VkBool32 primitiveRestartEnable)
        {
            m_InputAssemblyCreateInfo.topology = topology;
            m_InputAssemblyCreateInfo.primitiveRestartEnable = primitiveRestartEnable;
            m_InputAssemblyStateSet = VK_TRUE;
        }

    void SetRasterizerState(
            VkBool32          bDepthClampEnable
            ,VkBool32         bRasterizerDiscardEnable
            ,VkPolygonMode    polygonMode
            ,VkLwllModeFlags  lwllMode
            ,VkFrontFace      frontFace
            // default the values that don't typically get changed.
            ,VkBool32         depthBiasEnable = VK_FALSE
            ,float            depthBiasConstantFactor = 0.0f
            ,float            depthBiasClamp = 0.0f
            ,float            depthBiasSlopeFactor = 0.0f
            ,float            lineWidth = 1.0f
            );
    // Note: Specifically setting the Viewport & Scissor state disables the
    // dynamic Viewport & Scissor state.
    void SetViewPortAndScissorState(VkPipelineViewportStateCreateInfo *pVpInfo);

    // Note: Selecting "0" points disables the Tessellation state.
    void SetTessellationState(UINT32 patchControlPoints);

    void AddPushConstantRange(const VkPushConstantRange& range);

    void SetCTALaunchQueueIdx(UINT32 queueIdx);

    enum pipelineSetupFlagBits
    {
        CREATE_PIPELINE_LAYOUT  = 0x01,
        CREATE_PIPELINE         = 0x02,
        FULL_SETUP              = 0x03
    };

private:
    void     TakeResources(VulkanPipeline&&);
    VkResult CreateRaytracingPipelineLayout();

    typedef enum dynamicStateEnableFlagBits
    {
        DYNAMIC_STATE_NONE = 0x00,
        DYNAMIC_STATE_VIEWPORT_SCISSOR = 0x1,
        DYNAMIC_STATE_DEPTH_STENCIL = 0x2

    }dynamicStateEnableFlagBits;

    VulkanDevice                           *m_pVulkanDev = nullptr;
    // These objects are used for Compute or Graphics pipelines
    VkPipelineLayout                        m_PipelineLayout = VK_NULL_HANDLE;
    VkPipeline                              m_Pipeline = VK_NULL_HANDLE;

    vector<VkDynamicState>                  m_DynamicStateEnables;
    VkPipelineDynamicStateCreateInfo        m_DynamicStateCreateInfo = {};

    VkPipelineVertexInputStateCreateInfo    m_VertexInputStateCreateInfo = {};
    VkPipelineTessellationStateCreateInfo   m_TessellationStateCreateInfo = {};
    VkPipelineInputAssemblyStateCreateInfo  m_InputAssemblyCreateInfo = {};
    VkPipelineRasterizationStateCreateInfo  m_RasterizationCreateInfo = {};
    vector<VkPipelineColorBlendAttachmentState>     m_AttachmentStates;
    VkPipelineColorBlendStateCreateInfo     m_ColorBlendStateCreateInfo = {};
    VkPipelineViewportStateCreateInfo       m_ViewportStateCreateInfo = {};
    VkPipelineDepthStencilStateCreateInfo   m_DepthStencilCreateInfo = {};
    VkPipelineMultisampleStateCreateInfo    m_MultisampleCreateInfo = {};
    VkGraphicsPipelineCreateInfo            m_GraphicsPipelineCreateInfo = {};
    VkComputePipelineCreateInfo             m_ComputePipelineCreateInfo = {};
#ifndef VULKAN_STANDALONE
    VkInternalSMSCCSelectLW                 m_InternalSMSCCSelectLW = {};
#endif
    VkRayTracingPipelineCreateInfoKHR       m_RaytracingPipelineCreateInfo = {};

    vector<VkDescriptorSetLayout>           m_DescriptorSetLayouts;
    vector<VkPipelineShaderStageCreateInfo> m_ShaderStages;
    vector<VkRayTracingShaderGroupCreateInfoKHR> m_ShaderGroups;
    VkRenderPass                            m_RenderPass = VK_NULL_HANDLE;
    UINT32                                  m_Subpass = 0;
    VkBool32                                m_RasterizerStateSet = VK_FALSE;
    VkBool32                                m_ColorBlendStateSet = VK_FALSE;
    VkBool32                                m_InputAssemblyStateSet = VK_FALSE;
    VkBool32                                m_DepthStencilStateSet = VK_FALSE;
    UINT32                                  m_UseDynamicState = DYNAMIC_STATE_VIEWPORT_SCISSOR;
    UINT32                                  m_CTALaunchQueueIdx = ~0;

    vector<VkPushConstantRange>             m_PushConstantRanges = {};

    VkPipeline                              m_ParentPipeline = VK_NULL_HANDLE;
};

#endif

