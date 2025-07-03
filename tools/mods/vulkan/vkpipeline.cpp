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

#include "vkbuffer.h"
#include "vkdev.h"
#include "vkpipeline.h"

#ifndef VULKAN_STANDALONE
#include "core/include/tasker.h"
#endif

//-------------------------------------------------------------------------------------------------
VulkanPipeline::VulkanPipeline(VulkanDevice* pVulkanDev) :
    m_pVulkanDev(pVulkanDev)
{
}

//-------------------------------------------------------------------------------------------------
VulkanPipeline::~VulkanPipeline()
{
    Cleanup();
}

//-------------------------------------------------------------------------------------------------
void VulkanPipeline::Cleanup()
{
    if (!m_pVulkanDev)
    {
        MASSERT(m_PipelineLayout == VK_NULL_HANDLE);
        MASSERT(m_Pipeline  == VK_NULL_HANDLE);
        return;
    }

    if (m_Pipeline)
    {
        m_pVulkanDev->DestroyPipeline(m_Pipeline);
        m_Pipeline  = VK_NULL_HANDLE;
    }

    if (m_PipelineLayout)
    {
        m_pVulkanDev->DestroyPipelineLayout(m_PipelineLayout);
        m_PipelineLayout = VK_NULL_HANDLE;
    }

    m_pVulkanDev = nullptr;
}

//-------------------------------------------------------------------------------------------------
void VulkanPipeline::TakeResources(VulkanPipeline&& pipeline)
{
    if (this == &pipeline)
    {
        return;
    }

    Cleanup();

    m_pVulkanDev = pipeline.m_pVulkanDev;
    pipeline.m_pVulkanDev = nullptr;

    m_Pipeline = pipeline.m_Pipeline;
    pipeline.m_Pipeline = VK_NULL_HANDLE;

    m_PipelineLayout = pipeline.m_PipelineLayout;
    pipeline.m_PipelineLayout = VK_NULL_HANDLE;

    m_DynamicStateEnables          = move(pipeline.m_DynamicStateEnables);
    m_DynamicStateCreateInfo       = move(pipeline.m_DynamicStateCreateInfo);
    m_VertexInputStateCreateInfo   = move(pipeline.m_VertexInputStateCreateInfo);
    m_TessellationStateCreateInfo  = move(pipeline.m_TessellationStateCreateInfo);
    m_InputAssemblyCreateInfo      = move(pipeline.m_InputAssemblyCreateInfo);
    m_RasterizationCreateInfo      = move(pipeline.m_RasterizationCreateInfo);
    m_AttachmentStates             = move(pipeline.m_AttachmentStates);
    m_ColorBlendStateCreateInfo    = move(pipeline.m_ColorBlendStateCreateInfo);
    m_ViewportStateCreateInfo      = move(pipeline.m_ViewportStateCreateInfo);
    m_DepthStencilCreateInfo       = move(pipeline.m_DepthStencilCreateInfo);
    m_MultisampleCreateInfo        = move(pipeline.m_MultisampleCreateInfo);
    m_GraphicsPipelineCreateInfo   = move(pipeline.m_GraphicsPipelineCreateInfo);
    m_ComputePipelineCreateInfo    = move(pipeline.m_ComputePipelineCreateInfo);
#ifndef VULKAN_STANDALONE
    m_InternalSMSCCSelectLW        = move(pipeline.m_InternalSMSCCSelectLW);
#endif
    m_RaytracingPipelineCreateInfo = move(pipeline.m_RaytracingPipelineCreateInfo);
    m_DescriptorSetLayouts         = move(pipeline.m_DescriptorSetLayouts);
    m_ShaderStages                 = move(pipeline.m_ShaderStages);
    m_ShaderGroups                 = move(pipeline.m_ShaderGroups);
    m_RenderPass                   = move(pipeline.m_RenderPass);
    m_Subpass                      = move(pipeline.m_Subpass);
    m_RasterizerStateSet           = move(pipeline.m_RasterizerStateSet);
    m_ColorBlendStateSet           = move(pipeline.m_ColorBlendStateSet);
    m_InputAssemblyStateSet        = move(pipeline.m_InputAssemblyStateSet);
    m_DepthStencilStateSet         = move(pipeline.m_DepthStencilStateSet);
    m_UseDynamicState              = move(pipeline.m_UseDynamicState);
    m_CTALaunchQueueIdx            = move(pipeline.m_CTALaunchQueueIdx);
    m_PushConstantRanges           = move(pipeline.m_PushConstantRanges);
    m_ParentPipeline               = move(pipeline.m_ParentPipeline);

    if (m_ColorBlendStateCreateInfo.pAttachments)
        m_ColorBlendStateCreateInfo.pAttachments = m_AttachmentStates.data();
}

//-------------------------------------------------------------------------------------------------
void VulkanPipeline::CreateDynamicState()
{
    // Dynamic state will be set inside the renderpass
    m_DynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    m_DynamicStateCreateInfo.pNext = nullptr;
    m_DynamicStateCreateInfo.pDynamicStates = m_DynamicStateEnables.data();
    m_DynamicStateCreateInfo.dynamicStateCount =
        static_cast<uint32_t>(m_DynamicStateEnables.size());
}
//-------------------------------------------------------------------------------------------------
// CreateVertexInputState()
// The VertexInputStateCreateInfo struct describes the format of the vertex data that will be
// passed to the vertex shader. It describes it in two ways:
// 1.Bindings: spacing between data and whether the data is "per-vertex" or "per-instance"
// 2.Attribute descriptions: type of the attributes passed to the vertex shader, which binding to
//   load them from and at which offset.
void VulkanPipeline::CreateVertexInputState(VBBindingAttributeDesc *pBindingAttributeDesc)
{
    MASSERT(pBindingAttributeDesc);
    m_VertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    m_VertexInputStateCreateInfo.pNext = nullptr;
    m_VertexInputStateCreateInfo.flags = 0;
    m_VertexInputStateCreateInfo.vertexBindingDescriptionCount =
        static_cast<uint32_t>(pBindingAttributeDesc->m_BindingDesc.size());
    m_VertexInputStateCreateInfo.pVertexBindingDescriptions =
        pBindingAttributeDesc->m_BindingDesc.data();
    m_VertexInputStateCreateInfo.vertexAttributeDescriptionCount =
        static_cast<uint32_t>(pBindingAttributeDesc->m_AttributesDesc.size());
    m_VertexInputStateCreateInfo.pVertexAttributeDescriptions =
        pBindingAttributeDesc->m_AttributesDesc.data();
}

//-------------------------------------------------------------------------------------------------
void VulkanPipeline::CreateTessellationState()
{
    m_TessellationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
    m_TessellationStateCreateInfo.pNext = nullptr;
    m_TessellationStateCreateInfo.flags = 0;
}

//-------------------------------------------------------------------------------------------------
// Normally, the vertices are loaded from the vertex buffer by index in sequential order, but with
// an element buffer you can specify the indices to use yourself. This allows you to perform
// optimizations like reusing vertices. If you set the primitiveRestartEnable member to VK_TRUE,
// then it's possible to break up lines and triangles in the _STRIP topology modes by using a
// special index of 0xFFFF or 0xFFFFFFFF.
//
void VulkanPipeline::CreateInputAssemblyState()
{
    m_InputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    m_InputAssemblyCreateInfo.pNext = nullptr;
    m_InputAssemblyCreateInfo.flags = 0;
    if (!m_InputAssemblyStateSet)
    {
        // topology should be set using the accessor
        m_InputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;
        m_InputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    }
}

//-------------------------------------------------------------------------------------------------
void VulkanPipeline::SetRasterizerState
(
    VkBool32          depthClampEnable
    ,VkBool32         rasterizerDiscardEnable
    ,VkPolygonMode    polygonMode
    ,VkLwllModeFlags  lwllMode
    ,VkFrontFace      frontFace
    ,VkBool32         depthBiasEnable
    ,float            depthBiasConstantFactor
    ,float            depthBiasClamp
    ,float            depthBiasSlopeFactor
    ,float            lineWidth
 )
{
    // The polygonMode determines how fragments are generated for geometry. The following modes are
    // available:
    // VK_POLYGON_MODE_FILL: fill the area of the polygon with fragments
    // VK_POLYGON_MODE_LINE: polygon edges are drawn as lines
    // VK_POLYGON_MODE_POINT: polygon vertices are drawn as points
    // Using any mode other than fill requires enabling a GPU feature.
    m_RasterizationCreateInfo.polygonMode = polygonMode;
    // The lwllMode variable determines the type of face lwlling to use. You can disable lwlling,
    // lwll the front faces, lwll the back faces or both.
    m_RasterizationCreateInfo.lwllMode = lwllMode;

    // The frontFace variable specifies the vertex order for faces to be considered front-facing
    // and can be clockwise or counterclockwise.
    m_RasterizationCreateInfo.frontFace = frontFace;

    // If depthClampEnable is set to VK_TRUE, then fragments that are beyond the near and far
    // planes are clamped to them as opposed to discarding them. This is useful in some special
    // cases like shadow maps. Using this requires enabling a GPU feature
    m_RasterizationCreateInfo.depthClampEnable = depthClampEnable;

    // If rasterizerDiscardEnable is set to VK_TRUE, then geometry never passes through the
    // rasterizer stage. This basically disables any output to the framebuffer.
    m_RasterizationCreateInfo.rasterizerDiscardEnable = rasterizerDiscardEnable;

    // The rasterizer can alter the depth values by adding a constant value or biasing them based
    // on a fragment's slope. This is sometimes used for shadow mapping, but we don't typically
    // use it.
    m_RasterizationCreateInfo.depthBiasEnable = depthBiasEnable;
    m_RasterizationCreateInfo.depthBiasConstantFactor = depthBiasConstantFactor;
    m_RasterizationCreateInfo.depthBiasClamp = depthBiasClamp;
    m_RasterizationCreateInfo.depthBiasSlopeFactor = depthBiasSlopeFactor;
    // The lineWidth member is straightforward, it describes the thickness of lines in terms of
    // number of fragments. The maximum line width that is supported depends on the hardware and
    // any line thicker than 1.0f requires you to enable the wideLines GPU feature.
    m_RasterizationCreateInfo.lineWidth = lineWidth;

    m_RasterizerStateSet = true;
}

//-------------------------------------------------------------------------------------------------
// The rasterizer takes the geometry that is shaped by the vertices from the vertex shader and
// turns it into fragments to be colored by the fragment shader. It also performs depth testing,
// face lwlling and the scissor test, and it can be configured to output fragments that fill entire
// polygons or just the edges (wireframe rendering). All this is configured using the
// VkPipelineRasterizationStateCreateInfo structure below.
void VulkanPipeline::CreateRasterizationState()
{
    m_RasterizationCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    m_RasterizationCreateInfo.pNext = nullptr;
    m_RasterizationCreateInfo.flags = 0;
    if (!m_RasterizerStateSet)
    {
        m_RasterizationCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
        m_RasterizationCreateInfo.lwllMode = VK_LWLL_MODE_NONE;
        m_RasterizationCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        m_RasterizationCreateInfo.depthClampEnable = VK_TRUE;
        m_RasterizationCreateInfo.rasterizerDiscardEnable = VK_FALSE;
        m_RasterizationCreateInfo.depthBiasEnable = VK_FALSE;
        m_RasterizationCreateInfo.depthBiasConstantFactor = 0;
        m_RasterizationCreateInfo.depthBiasClamp = 0;
        m_RasterizationCreateInfo.depthBiasSlopeFactor = 0;
        m_RasterizationCreateInfo.lineWidth = 1.0;
    }
}

// ------------------------------------------------------------------------------------------------
// Implementation Notes:
// After a fragment shader has returned a color, it needs to be combined with the color that is
// already in the framebuffer. This transformation is known as color blending and there are two
// ways to do it:
// 1. Mix the old and new value to produce a final color
// 2. Combine the old and new value using a bitwise operation
// There are two types of structs to configure color blending. The first struct,
// VkPipelineColorBlendAttachmentState contains the configuration per attached framebuffer and
// the second struct, VkPipelineColorBlendStateCreateInfo contains the global color blending
// settings.
// The per-framebuffer struct "VkPipelineColorBlendAttachmentState" allows you to configure the
// first way of color blending. The operations performed are best described using the following
// pseudo code:
// if (blendEnable)
// {
//     finalColor.rgb = (srcColorBlendFactor * newColor.rgb) <colorBlendOp> (dstColorBlendFactor * oldColor.rgb); //$
//     finalColor.a = (srcAlphaBlendFactor * newColor.a) <alphaBlendOp> (dstAlphaBlendFactor * oldColor.a); //$
// } else
// {
//     finalColor = newColor;
// }
//
// finalColor = finalColor & colorWriteMask;
//
// If blendEnable is set to VK_FALSE, then the new color from the fragment shader is passed
// through unmodified. Otherwise, the two mixing operations are performed to compute a new
// color. The resulting color is AND'd with the colorWriteMask to determine which channels are
// actually passed through.
//
// The most common way to use color blending is to implement alpha blending, where we want the
// new color to be blended with the old color based on its opacity. The finalColor should then
// be computed as follows:
// finalColor.rgb = newAlpha * newColor + (1 - newAlpha) * oldColor;
// finalColor.a = newAlpha.a;
// This can be accomplished with the following parameters:
// VkPipelineColorBlendAttachmentState colorBlendAttachment;
// colorBlendAttachment.blendEnable = VK_TRUE;
// colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
// colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
// colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
// colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
// colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
// colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
// You can find all of the possible operations in the VkBlendFactor and VkBlendOp enumerations in
// the specification.
//
// If you want to use the second method of blending (bitwise combination), then you should set
// logicOpEnable to VK_TRUE. The bitwise operation can then be specified in the logicOp field.
// Note that this will automatically disable the first method, as if you had set blendEnable to
// VK_FALSE for every attached framebuffer! However the colorWriteMask will be used in this mode to
// determine which channels in the framebuffer will actually be affected. It is also possible to
// disable both modes, as we've done here by default, in which case the fragment colors will be
// written to the framebuffer unmodified.
// Note: If the independant blending feature (see VkUtil::m_DeviceFeatures.independantBlend) is not
//       enabled on the device, then all VkPipelineColorBlendAttachmentState elements in the
//       pAttachments array must be identical.
void VulkanPipeline::SetColorBlendState
(
    vector<VkPipelineColorBlendAttachmentState> colorBlendAttachmentStates,
    VkBool32  logicOpEnable,
    VkLogicOp logicOp,
    float     blendConstants[4]
)
{
    m_AttachmentStates = move(colorBlendAttachmentStates);
    m_ColorBlendStateCreateInfo.logicOpEnable = logicOpEnable;
    m_ColorBlendStateCreateInfo.logicOp = logicOp;
    m_ColorBlendStateCreateInfo.blendConstants[0] = blendConstants[0];
    m_ColorBlendStateCreateInfo.blendConstants[1] = blendConstants[1];
    m_ColorBlendStateCreateInfo.blendConstants[2] = blendConstants[2];
    m_ColorBlendStateCreateInfo.blendConstants[3] = blendConstants[3];
    m_ColorBlendStateSet = VK_TRUE;
}

void VulkanPipeline::SetColorBlendState
(
    const VkPipelineColorBlendAttachmentState& colorBlendAttachmentState,
    VkBool32  logicOpEnable,
    VkLogicOp logicOp,
    float     blendConstants[4]
)
{
    vector<VkPipelineColorBlendAttachmentState> states(1, colorBlendAttachmentState);
    SetColorBlendState(move(states), logicOpEnable, logicOp, blendConstants);
}

//-------------------------------------------------------------------------------------------------
void VulkanPipeline::CreateColorBlendState()
{

    if (!m_ColorBlendStateSet)
    {
        // default to no color blending
        VkPipelineColorBlendAttachmentState cbas;
        cbas.colorWriteMask = 0xf;  //output all channels RGBA
        cbas.blendEnable = VK_FALSE;
        cbas.alphaBlendOp = VK_BLEND_OP_ADD;
        cbas.colorBlendOp = VK_BLEND_OP_ADD;
        cbas.srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        cbas.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        cbas.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        cbas.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        m_AttachmentStates.push_back(cbas);

        m_ColorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
        m_ColorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_NO_OP;
        m_ColorBlendStateCreateInfo.blendConstants[0] = 1.0f;
        m_ColorBlendStateCreateInfo.blendConstants[1] = 1.0f;
        m_ColorBlendStateCreateInfo.blendConstants[2] = 1.0f;
        m_ColorBlendStateCreateInfo.blendConstants[3] = 1.0f;
    }

    m_ColorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    m_ColorBlendStateCreateInfo.pNext = nullptr;
    m_ColorBlendStateCreateInfo.flags = 0;
    m_ColorBlendStateCreateInfo.attachmentCount = static_cast<UINT32>(m_AttachmentStates.size());
    m_ColorBlendStateCreateInfo.pAttachments =
        (m_AttachmentStates.size() == 0) ? nullptr : m_AttachmentStates.data();
}

//-------------------------------------------------------------------------------------------------
void VulkanPipeline::EnableDynamicState(VkDynamicState state)
{
    for (UINT32 i = 0; i < static_cast<UINT32>(m_DynamicStateEnables.size()); i++)
    {
        if (m_DynamicStateEnables[i] == state)
        {
            return;
        }
    }
    m_DynamicStateEnables.push_back(state);
}

//-------------------------------------------------------------------------------------------------
// Set the static Viewport and Scissor state.
// Note: This disables the dynamic Viewport & Scissor settings when the pipeline
void VulkanPipeline::SetViewPortAndScissorState
(
    VkPipelineViewportStateCreateInfo *pVpInfo
)
{
    m_ViewportStateCreateInfo = *pVpInfo;
    m_UseDynamicState &= ~DYNAMIC_STATE_VIEWPORT_SCISSOR;
}

//-------------------------------------------------------------------------------------------------
void VulkanPipeline::SetTessellationState(UINT32 patchControlPoints)
{
    m_TessellationStateCreateInfo.patchControlPoints = patchControlPoints;
}

//-------------------------------------------------------------------------------------------------
void VulkanPipeline::CreateViewportAndScissorState()
{
    if (m_UseDynamicState & DYNAMIC_STATE_VIEWPORT_SCISSOR)
    {
        // Scissor and viewports will be set dynamically
        // This means that you must call VkCmdSetViewport() and VkCmdSetScissor() before any draw
        // calls during the render pass.
        m_ViewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        m_ViewportStateCreateInfo.pNext = nullptr;
        m_ViewportStateCreateInfo.flags = 0;
        m_ViewportStateCreateInfo.pViewports = nullptr;
        m_ViewportStateCreateInfo.pScissors = nullptr;

        m_ViewportStateCreateInfo.viewportCount = 1;
        m_ViewportStateCreateInfo.scissorCount = 1;
        EnableDynamicState(VK_DYNAMIC_STATE_VIEWPORT);
        EnableDynamicState(VK_DYNAMIC_STATE_SCISSOR);
    }
}

//-------------------------------------------------------------------------------------------------
void VulkanPipeline::SetDepthStencilState
(
    VkPipelineDepthStencilStateCreateInfo * pDepthStencilState
)
{
    m_DepthStencilCreateInfo = *pDepthStencilState;
    m_DepthStencilStateSet = VK_TRUE;
}

//-------------------------------------------------------------------------------------------------
void VulkanPipeline::CreateDepthStencilState()
{
    if (!m_DepthStencilStateSet)
    {
        m_DepthStencilCreateInfo.sType =
            VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        m_DepthStencilCreateInfo.pNext = nullptr;
        m_DepthStencilCreateInfo.flags = 0;
        m_DepthStencilCreateInfo.depthTestEnable = VK_TRUE;
        m_DepthStencilCreateInfo.depthWriteEnable = VK_TRUE;
        m_DepthStencilCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        m_DepthStencilCreateInfo.depthBoundsTestEnable = VK_FALSE;
        m_DepthStencilCreateInfo.minDepthBounds = 0;
        m_DepthStencilCreateInfo.maxDepthBounds = 0;
        m_DepthStencilCreateInfo.stencilTestEnable = VK_FALSE;
        m_DepthStencilCreateInfo.back.failOp = VK_STENCIL_OP_KEEP;
        m_DepthStencilCreateInfo.back.passOp = VK_STENCIL_OP_KEEP;
        m_DepthStencilCreateInfo.back.compareOp = VK_COMPARE_OP_ALWAYS;
        m_DepthStencilCreateInfo.back.compareMask = 0;
        m_DepthStencilCreateInfo.back.reference = 0;
        m_DepthStencilCreateInfo.back.depthFailOp = VK_STENCIL_OP_KEEP;
        m_DepthStencilCreateInfo.back.writeMask = 0;
        m_DepthStencilCreateInfo.front = m_DepthStencilCreateInfo.back;
    }
}

//-------------------------------------------------------------------------------------------------
VkResult VulkanPipeline::CreateMultisampleState(VkSampleCountFlagBits sampleCount)
{
    VkResult res = VK_SUCCESS;

    m_MultisampleCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    m_MultisampleCreateInfo.pNext = nullptr;
    m_MultisampleCreateInfo.flags = 0;
    m_MultisampleCreateInfo.pSampleMask = nullptr;
    m_MultisampleCreateInfo.rasterizationSamples = sampleCount;

    if (!m_pVulkanDev->GetPhysicalDevice()->IsSampleCountSupported(sampleCount))
    {
        return VK_ERROR_TOO_MANY_OBJECTS;
    }

    if (sampleCount != VK_SAMPLE_COUNT_1_BIT &&
        m_pVulkanDev->GetPhysicalDevice()->GetFeatures2().features.sampleRateShading == VK_TRUE)
    {
        // Run the fragment shader for each sample
        m_MultisampleCreateInfo.sampleShadingEnable = VK_TRUE;
        m_MultisampleCreateInfo.minSampleShading = 1.0f;
    }
    else
    {
        m_MultisampleCreateInfo.sampleShadingEnable = VK_FALSE;
        m_MultisampleCreateInfo.minSampleShading = 0.0f;
    }

    m_MultisampleCreateInfo.alphaToCoverageEnable = VK_FALSE;
    m_MultisampleCreateInfo.alphaToOneEnable = VK_FALSE;

    return res;
}

//-------------------------------------------------------------------------------------------------
VkResult VulkanPipeline::CreateGraphicsPipeline()
{
    PROFILE_ZONE(VULKAN)

    MASSERT(m_Pipeline == VK_NULL_HANDLE);

    VkResult res = VK_SUCCESS;

    //Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.pNext = nullptr;
    pipelineLayoutCreateInfo.pushConstantRangeCount =
        static_cast<UINT32>(m_PushConstantRanges.size());
    pipelineLayoutCreateInfo.pPushConstantRanges =
        m_PushConstantRanges.empty() ? nullptr : m_PushConstantRanges.data();
    pipelineLayoutCreateInfo.setLayoutCount = static_cast<UINT32>(m_DescriptorSetLayouts.size());
    pipelineLayoutCreateInfo.pSetLayouts = m_DescriptorSetLayouts.data();
    CHECK_VK_RESULT(m_pVulkanDev->CreatePipelineLayout(&pipelineLayoutCreateInfo,
        nullptr, &m_PipelineLayout));

    m_GraphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    m_GraphicsPipelineCreateInfo.pNext = nullptr;
    m_GraphicsPipelineCreateInfo.flags = VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT |
        (m_ParentPipeline != VK_NULL_HANDLE ? VK_PIPELINE_CREATE_DERIVATIVE_BIT : 0);
    m_GraphicsPipelineCreateInfo.layout = m_PipelineLayout;
    m_GraphicsPipelineCreateInfo.basePipelineHandle = m_ParentPipeline;
    m_GraphicsPipelineCreateInfo.basePipelineIndex = -1;

    m_GraphicsPipelineCreateInfo.pVertexInputState   = &m_VertexInputStateCreateInfo;
    m_GraphicsPipelineCreateInfo.pInputAssemblyState = &m_InputAssemblyCreateInfo;
    m_GraphicsPipelineCreateInfo.pRasterizationState = &m_RasterizationCreateInfo;
    m_GraphicsPipelineCreateInfo.pColorBlendState    = &m_ColorBlendStateCreateInfo;
    m_GraphicsPipelineCreateInfo.pTessellationState  =
        (!m_TessellationStateCreateInfo.patchControlPoints) ?
            nullptr : &m_TessellationStateCreateInfo;
    m_GraphicsPipelineCreateInfo.pMultisampleState   = &m_MultisampleCreateInfo;
    m_GraphicsPipelineCreateInfo.pDynamicState       =
        (!m_DynamicStateCreateInfo.dynamicStateCount) ? nullptr : &m_DynamicStateCreateInfo;
    m_GraphicsPipelineCreateInfo.pViewportState      = &m_ViewportStateCreateInfo;
    m_GraphicsPipelineCreateInfo.pDepthStencilState  = &m_DepthStencilCreateInfo;

    m_GraphicsPipelineCreateInfo.pStages = m_ShaderStages.data();
    m_GraphicsPipelineCreateInfo.stageCount = static_cast<uint32_t>(m_ShaderStages.size());
    m_GraphicsPipelineCreateInfo.renderPass = m_RenderPass;
    m_GraphicsPipelineCreateInfo.subpass = m_Subpass;

    CHECK_VK_RESULT(m_pVulkanDev->CreateGraphicsPipelines(
        m_pVulkanDev->GetPipelineCache(), 1, &m_GraphicsPipelineCreateInfo, nullptr, &m_Pipeline));
    return res;
}

//-------------------------------------------------------------------------------------------------
VkResult VulkanPipeline::SetupGraphicsPipeline
(
    VBBindingAttributeDesc *pBindingAttributeDesc,
    VkDescriptorSetLayout descriptorSetLayout,
    vector<VkPipelineShaderStageCreateInfo> shaderStages,
    VkRenderPass renderPass,
    UINT32 subpass,
    VkSampleCountFlagBits sampleCount
)
{
    VkResult res = VK_SUCCESS;

    CreateVertexInputState(pBindingAttributeDesc);
    CreateTessellationState();
    CreateInputAssemblyState();
    CreateRasterizationState();
    CreateColorBlendState();
    CreateViewportAndScissorState();
    CreateDepthStencilState();
    CHECK_VK_RESULT(CreateMultisampleState(sampleCount));

    CreateDynamicState();

    m_DescriptorSetLayouts.clear();
    m_DescriptorSetLayouts.emplace_back(descriptorSetLayout);
    m_ShaderStages = move(shaderStages);
    m_RenderPass = renderPass;
    m_Subpass = subpass;
    CHECK_VK_RESULT(CreateGraphicsPipeline());
    return res;
}

//-------------------------------------------------------------------------------------------------
void VulkanPipeline::AddPushConstantRange(const VkPushConstantRange& range)
{
    MASSERT(m_Pipeline  == VK_NULL_HANDLE);
    m_PushConstantRanges.push_back(range);
}

void VulkanPipeline::SetCTALaunchQueueIdx(UINT32 queueIdx)
{
    m_CTALaunchQueueIdx = queueIdx;
}

//-------------------------------------------------------------------------------------------------
VkResult VulkanPipeline::SetupComputePipeline
(
    VkDescriptorSetLayout descriptorSetLayout
    ,VkPipelineShaderStageCreateInfo shaderStage
)
{
    VkResult res = VK_SUCCESS;
    if (!m_ShaderStages.empty())
    {
        m_ShaderStages.clear();
    }
    m_ShaderStages.emplace_back(shaderStage);
    m_DescriptorSetLayouts.clear();
    m_DescriptorSetLayouts.emplace_back(descriptorSetLayout);

    CHECK_VK_RESULT(CreateComputePipeline());
    return res;
}

//-------------------------------------------------------------------------------------------------
VkResult VulkanPipeline::CreateComputePipeline()
{
    PROFILE_ZONE(VULKAN)

    MASSERT(m_Pipeline == VK_NULL_HANDLE);

    VkResult res = VK_SUCCESS;

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.pNext = nullptr;
    pipelineLayoutCreateInfo.pushConstantRangeCount =
        static_cast<UINT32>(m_PushConstantRanges.size());
    pipelineLayoutCreateInfo.pPushConstantRanges =
        m_PushConstantRanges.empty() ? nullptr : m_PushConstantRanges.data();
    pipelineLayoutCreateInfo.setLayoutCount = static_cast<UINT32>(m_DescriptorSetLayouts.size());
    pipelineLayoutCreateInfo.pSetLayouts = m_DescriptorSetLayouts.data();
    CHECK_VK_RESULT(m_pVulkanDev->CreatePipelineLayout(&pipelineLayoutCreateInfo,
        nullptr, &m_PipelineLayout));

    m_ComputePipelineCreateInfo.pNext = nullptr;
#ifndef VULKAN_STANDALONE
    if (m_CTALaunchQueueIdx != ~0U)
    {
        m_InternalSMSCCSelectLW.sType = VK_STRUCTURE_TYPE_INTERNAL_SMSCC_SELECT_LW;
        m_InternalSMSCCSelectLW.pNext = nullptr;
        m_InternalSMSCCSelectLW.mode = VK_INTERNAL_COMPUTE_LAUNCH_QUEUE_MODE_OVERRIDE_LW;
        m_InternalSMSCCSelectLW.launchQueue = m_CTALaunchQueueIdx;
        m_ComputePipelineCreateInfo.pNext = &m_InternalSMSCCSelectLW;
    }
#endif
    m_ComputePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    m_ComputePipelineCreateInfo.layout = m_PipelineLayout;
    m_ComputePipelineCreateInfo.stage = m_ShaderStages[0];
    m_ComputePipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
    m_ComputePipelineCreateInfo.basePipelineIndex = 0;

    CHECK_VK_RESULT(m_pVulkanDev->CreateComputePipelines(m_pVulkanDev->GetPipelineCache(),
                                              1,
                                              &m_ComputePipelineCreateInfo,
                                              nullptr,
                                              &m_Pipeline));
    return res;
}

VkResult VulkanPipeline::SetupRaytracingPipeline
(
    vector<VkDescriptorSetLayout>                descriptorSetLayouts,
    vector<VkPipelineShaderStageCreateInfo>      shaderStages,
    vector<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups,
    vector<VkPushConstantRange>                  pushConstantRanges,
    UINT32                                       setupFlags,
    UINT32                                       maxRayRelwrsionDepth
)
{
    VkResult res = VK_SUCCESS;

    if (!descriptorSetLayouts.empty())
    {
        m_DescriptorSetLayouts = move(descriptorSetLayouts);
    }

    if (!shaderStages.empty())
    {
        m_ShaderStages = move(shaderStages);
    }

    if (!shaderGroups.empty())
    {
        m_ShaderGroups = move(shaderGroups);
    }

    if (!pushConstantRanges.empty())
    {
        m_PushConstantRanges = move(pushConstantRanges);
    }

    if (setupFlags & CREATE_PIPELINE_LAYOUT)
    {
        CHECK_VK_RESULT(CreateRaytracingPipelineLayout());
    }

    if (setupFlags & CREATE_PIPELINE)
    {
        CHECK_VK_RESULT(CreateRaytracingPipeline(maxRayRelwrsionDepth));
    }

    return res;
}

VkResult VulkanPipeline::CreateRaytracingPipelineLayout
(
    vector<VkDescriptorSetLayout> layouts
)
{
    m_DescriptorSetLayouts = move(layouts);
    return CreateRaytracingPipelineLayout();
}

VkResult VulkanPipeline::CreateRaytracingPipelineLayout
(
    VkDescriptorSetLayout descriptorSetLayout
)
{
    m_DescriptorSetLayouts.clear();
    m_DescriptorSetLayouts.push_back(descriptorSetLayout);
    return CreateRaytracingPipelineLayout();
}

VkResult VulkanPipeline::CreateRaytracingPipelineLayout()
{
    PROFILE_ZONE(VULKAN)

    MASSERT(m_PipelineLayout == VK_NULL_HANDLE);

    VkResult res = VK_SUCCESS;

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.pNext = nullptr;
    pipelineLayoutCreateInfo.pushConstantRangeCount =
        static_cast<UINT32>(m_PushConstantRanges.size());
    pipelineLayoutCreateInfo.pPushConstantRanges =
        m_PushConstantRanges.empty() ? nullptr : m_PushConstantRanges.data();
    pipelineLayoutCreateInfo.setLayoutCount = static_cast<UINT32>(m_DescriptorSetLayouts.size());
    pipelineLayoutCreateInfo.pSetLayouts = m_DescriptorSetLayouts.data();
    CHECK_VK_RESULT(m_pVulkanDev->CreatePipelineLayout(&pipelineLayoutCreateInfo,
        nullptr, &m_PipelineLayout));

    return res;
}

VkResult VulkanPipeline::CreateRaytracingPipeline(UINT32 maxRayRelwrsionDepth)
{
    PROFILE_ZONE(VULKAN)

    MASSERT(m_Pipeline == VK_NULL_HANDLE);

    // The rtcore library uses native mutexes internally (e.g. std::mutex).
    // They cause deadlock for MODS attached threads.
    // Probably the best solution would be to replace std::mutex with
    // ModsDrvAcquireMutex/ModsDrvReleaseMutex, but there is no platform
    // abstraction layer, so this is non trivial effort.
    // For now use DetachThread here to prevent the deadlock as the only
    // tracked down problematic location is in rtcore::CompiledModule::uploadModule
    // which in MODS is called only from CreateRayTracingPipelinesKHR.
    // More details in bug 3224369.
    Tasker::DetachThread detach;

    VkResult res = VK_SUCCESS;

    m_RaytracingPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    m_RaytracingPipelineCreateInfo.pNext = nullptr;
    m_RaytracingPipelineCreateInfo.flags = 0;
    m_RaytracingPipelineCreateInfo.layout = m_PipelineLayout;
    m_RaytracingPipelineCreateInfo.stageCount = static_cast<UINT32>(m_ShaderStages.size());
    m_RaytracingPipelineCreateInfo.pStages = m_ShaderStages.data();
    m_RaytracingPipelineCreateInfo.groupCount = static_cast<UINT32>(m_ShaderGroups.size());
    m_RaytracingPipelineCreateInfo.pGroups = m_ShaderGroups.data();
    m_RaytracingPipelineCreateInfo.maxPipelineRayRelwrsionDepth = maxRayRelwrsionDepth;
    m_RaytracingPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
    m_RaytracingPipelineCreateInfo.basePipelineIndex = 0;

    CHECK_VK_RESULT(m_pVulkanDev->CreateRayTracingPipelinesKHR(VK_NULL_HANDLE,
                                                               VK_NULL_HANDLE,
                                                               1,
                                                               &m_RaytracingPipelineCreateInfo,
                                                               nullptr,
                                                               &m_Pipeline));

    return res;

}
