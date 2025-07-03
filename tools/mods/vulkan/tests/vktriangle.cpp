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

#include "vktriangle.h"

//--------------------------------------------------------------------
VkTriangle::VkTriangle()
{
    m_pGolden = GetGoldelwalues();
}

#ifndef VULKAN_STANDALONE

//--------------------------------------------------------------------
bool VkTriangle::IsSupported()
{
    return GetBoundGpuSubdevice()->HasFeature(Device::GPUSUB_SUPPORTS_VULKAN);
}

//--------------------------------------------------------------------
RC VkTriangle::InitFromJs()
{
    RC rc;
    CHECK_RC(GpuTest::InitFromJs());
    return rc;
}

//--------------------------------------------------------------------
void VkTriangle::PrintJsProperties(Tee::Priority pri)
{
    const char * TF[] = { "false", "true" };
    const char * geo[] = { "triangle", "torus", "pyramid" };

    Printf(pri, "\t%-32s %s\n", "CompressTexture:",              TF[m_CompressTexture]);
    Printf(pri, "\t%-32s %s\n", "EnableValidation:",             TF[m_EnableValidation]);
    Printf(pri, "\t%-32s %u\n", "Num Frames To Draw:",           m_NumFramesToDraw);
    Printf(pri, "\t%-32s %f\n", "Triangle axis rotation angle:", m_TriRotationAngle);
    Printf(pri, "\t%-32s %s\n", "Indexed Draw:",                 TF[m_IndexedDraw]);
    Printf(pri, "\t%-32s %s\n", "Skip Present:",                 TF[m_SkipPresent]);
    Printf(pri, "\t%-32s %u\n", "TestMode:",                     m_TestMode);
    Printf(pri, "\t%-32s %s\n", "Geometry:",                     geo[m_GeometryType]);
    Printf(pri, "\t%-32s %u\n", "SampleCount:",                  m_SampleCount);
}
#endif

//--------------------------------------------------------------------
RC VkTriangle::SetupDepthBuffer()
{
    RC rc;
    m_DepthImage->PickDepthFormat();
    m_DepthImage->SetImageProperties(GetTestConfiguration()->DisplayWidth(),
                                    GetTestConfiguration()->DisplayHeight(),
                                    1,
                                    VulkanImage::ImageType::DEPTH);
    CHECK_VK_TO_RC(m_DepthImage->CreateImage(
        VkImageCreateFlags(0),
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT, // For goldens/pngs
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_TILING_OPTIMAL,
        static_cast<VkSampleCountFlagBits>(m_SampleCount)));
    CHECK_VK_TO_RC(m_DepthImage->AllocateAndBindMemory(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    CHECK_VK_TO_RC(m_DepthImage->SetImageLayout(
        m_InitCmdBuffer.get()
        ,m_DepthImage->GetImageAspectFlags()
        ,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL   //new layout
        ,VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT       //new access
        ,VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT));      //new stage
    CHECK_VK_TO_RC(m_DepthImage->CreateImageView());
    return rc;
}

//------------------------------------------------------------------------------------------------
RC VkTriangle::SetupMsaaImage()
{
    RC rc;

    m_ColorMsaaImage->SetFormat(m_SwapChain->GetSurfaceFormat());
    m_ColorMsaaImage->SetImageProperties(GetTestConfiguration()->DisplayWidth(),
                                         GetTestConfiguration()->DisplayHeight(),
                                         1,
                                         VulkanImage::ImageType::COLOR);

    m_ColorMsaaImage->CreateImage(
        VkImageCreateFlags(0),
        VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_TILING_OPTIMAL,
        static_cast<VkSampleCountFlagBits>(m_SampleCount));
    CHECK_VK_TO_RC(m_ColorMsaaImage->AllocateAndBindMemory(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

    CHECK_VK_TO_RC(m_ColorMsaaImage->SetImageLayout(
        m_InitCmdBuffer.get()
        ,m_ColorMsaaImage->GetImageAspectFlags()
        ,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL          // new layout
        ,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT              // new access
        ,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT));  // new stage
    CHECK_VK_TO_RC(m_ColorMsaaImage->CreateImageView());

    return rc;
}

//------------------------------------------------------------------------------------------------
RC VkTriangle::SetupBuffers()
{
    RC rc;

    //uniform buffer for matrix
    CHECK_RC(m_Camera.SetProjectionMatrix(45.0f, 1.0f, 0.1f, 100.0f));
    // Torus is larger than the triangle or pyramid so move the camera back so we can see it.
    const float zpos = (m_GeometryType == VkGeometry::GEOMETRY_TORUS) ? -20.0f : -10.0f;
    m_Camera.SetCameraPosition(0, 0, zpos); //Scene is at origin and camera at (0, 0, zpos)

    GLMatrix modelMatrix = m_Camera.GetModelMatrix(0, 0, 0, 0, 0, 0, 1, 1, 1);
    GLMatrix mvpMatrix = m_Camera.GetMVPMatrix(modelMatrix);
    UINT32 mvpMatrixSize = sizeof(mvpMatrix.mat);

    CHECK_VK_TO_RC(m_HUniformBufferMVPMatrix->CreateBuffer(
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT
        ,mvpMatrixSize));
    CHECK_VK_TO_RC(m_HUniformBufferMVPMatrix->AllocateAndBindMemory(
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
    CHECK_VK_TO_RC(m_HUniformBufferMVPMatrix->SetData(&mvpMatrix.mat[0][0], mvpMatrixSize, 0));

    CHECK_VK_TO_RC(m_UniformBufferMVPMatrix->CreateBuffer(
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
        ,mvpMatrixSize));
    CHECK_VK_TO_RC(m_UniformBufferMVPMatrix->AllocateAndBindMemory(
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    CHECK_VK_TO_RC(VkUtil::CopyBuffer(m_InitCmdBuffer.get(),
        m_HUniformBufferMVPMatrix.get(), m_UniformBufferMVPMatrix.get()));
    m_HUniformBufferMVPMatrix->Cleanup();

    //Geometry
    vector<UINT32> indices;
    vector<float> positions;
    vector<float> normals;
    vector<float> colors;
    vector<float> txCoords;

    m_Geometry.SetGeometryType(m_GeometryType);
    m_Geometry.GetVertexData(indices, positions, normals, colors, txCoords);
    m_NumIndicesToDraw = static_cast<UINT32>(indices.size());
    m_NumVerticesToDraw = static_cast<UINT32>(positions.size() / m_Geometry.GetPositionSize());
    MASSERT(m_NumVerticesToDraw > 0 && m_NumIndicesToDraw > 0);

    // Create Buffers
    // positions
    UINT32 bufferSize = static_cast<UINT32>(positions.size() * sizeof(float));
    CHECK_VK_TO_RC(m_VBHPositions->CreateBuffer(
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT
        ,bufferSize));
    CHECK_VK_TO_RC(m_VBHPositions->AllocateAndBindMemory(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
    CHECK_VK_TO_RC(m_VBHPositions->SetData(positions.data(), bufferSize, 0));

    CHECK_VK_TO_RC(m_VBPositions->CreateBuffer(
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
        ,bufferSize));
    CHECK_VK_TO_RC(m_VBPositions->AllocateAndBindMemory(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    CHECK_VK_TO_RC(VkUtil::CopyBuffer(m_InitCmdBuffer.get(),
        m_VBHPositions.get(), m_VBPositions.get()));
    m_VBHPositions->Cleanup();

    // normals
    bufferSize = static_cast<UINT32>(normals.size() * sizeof(float));
    CHECK_VK_TO_RC(m_VBHNormals->CreateBuffer(
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT
        ,bufferSize));
    CHECK_VK_TO_RC(m_VBHNormals->AllocateAndBindMemory(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
    CHECK_VK_TO_RC(m_VBHNormals->SetData(normals.data(), bufferSize, 0));

    CHECK_VK_TO_RC(m_VBNormals->CreateBuffer(
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
        ,bufferSize));
    CHECK_VK_TO_RC(m_VBNormals->AllocateAndBindMemory(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    CHECK_VK_TO_RC(VkUtil::CopyBuffer(m_InitCmdBuffer.get(),
        m_VBHNormals.get(), m_VBNormals.get()));
    m_VBHNormals->Cleanup();

    // Colors
    bufferSize = static_cast<UINT32>(colors.size() * sizeof(float));
    CHECK_VK_TO_RC(m_VBHColors->CreateBuffer(
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT
        ,bufferSize));
    CHECK_VK_TO_RC(m_VBHColors->AllocateAndBindMemory(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
    CHECK_VK_TO_RC(m_VBHColors->SetData(colors.data(), bufferSize, 0));

    CHECK_VK_TO_RC(m_VBColors->CreateBuffer(
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
        ,bufferSize));
    CHECK_VK_TO_RC(m_VBColors->AllocateAndBindMemory(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    CHECK_VK_TO_RC(VkUtil::CopyBuffer(m_InitCmdBuffer.get(),
        m_VBHColors.get(), m_VBColors.get()));
    m_VBHColors->Cleanup();

    // Tex Coordinates
    bufferSize = static_cast<UINT32>(txCoords.size() * sizeof(float));
    CHECK_VK_TO_RC(m_VBHTexCoords->CreateBuffer(
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        bufferSize));
    CHECK_VK_TO_RC(m_VBHTexCoords->AllocateAndBindMemory(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
    CHECK_VK_TO_RC(m_VBHTexCoords->SetData(txCoords.data(), bufferSize, 0));

    CHECK_VK_TO_RC(m_VBTexCoords->CreateBuffer(
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
        ,bufferSize));
    CHECK_VK_TO_RC(m_VBTexCoords->AllocateAndBindMemory(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    CHECK_VK_TO_RC(VkUtil::CopyBuffer(m_InitCmdBuffer.get(),
        m_VBHTexCoords.get(), m_VBTexCoords.get()));
    m_VBHTexCoords->Cleanup();

    // Create index buffer
    bufferSize = static_cast<UINT32>(indices.size() * sizeof(UINT32));
    CHECK_VK_TO_RC(m_HIndexBuffer->CreateBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                                                VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                                VK_BUFFER_USAGE_INDEX_BUFFER_BIT
                                                ,bufferSize));
    CHECK_VK_TO_RC(m_HIndexBuffer->AllocateAndBindMemory(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
    CHECK_VK_TO_RC(m_HIndexBuffer->SetData(indices.data(), bufferSize, 0));

    CHECK_VK_TO_RC(m_IndexBuffer->CreateBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                               VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                               VK_BUFFER_USAGE_INDEX_BUFFER_BIT
                                               ,bufferSize));
    CHECK_VK_TO_RC(m_IndexBuffer->AllocateAndBindMemory(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    CHECK_VK_TO_RC(VkUtil::CopyBuffer(m_InitCmdBuffer.get(),
        m_HIndexBuffer.get(), m_IndexBuffer.get()));
    m_HIndexBuffer->Cleanup();

    //Set the input binding and attribute descriptions of vertex data
    VkVertexInputBindingDescription posBinding;
    posBinding.binding = m_VBBindingPositions;
    posBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    posBinding.stride = m_Geometry.GetPositionSize() * sizeof(float);
    m_VBBindingAttributeDesc.m_BindingDesc.push_back(posBinding);

    VkVertexInputBindingDescription normBinding;
    normBinding.binding = m_VBBindingNormals;
    normBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    normBinding.stride = m_Geometry.GetNormalSize() * sizeof(float);
    m_VBBindingAttributeDesc.m_BindingDesc.push_back(normBinding);

    VkVertexInputBindingDescription colorBinding;
    colorBinding.binding = m_VBBindingColors;
    colorBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    colorBinding.stride = m_Geometry.GetColorSize() * sizeof(float);
    m_VBBindingAttributeDesc.m_BindingDesc.push_back(colorBinding);

    VkVertexInputBindingDescription texBinding;
    texBinding.binding = m_VBBindingTexCoords;
    texBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    texBinding.stride = m_Geometry.GetTxCoordSize() * sizeof(float);
    m_VBBindingAttributeDesc.m_BindingDesc.push_back(texBinding);

    VkVertexInputAttributeDescription attributesDesc;
    attributesDesc.binding = m_VBBindingPositions;
    attributesDesc.location = 0;
    attributesDesc.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributesDesc.offset = 0;
    m_VBBindingAttributeDesc.m_AttributesDesc.push_back(attributesDesc);

    attributesDesc.binding = m_VBBindingNormals;
    attributesDesc.location = 1;
    attributesDesc.format = VK_FORMAT_R32G32B32_SFLOAT;
    attributesDesc.offset = 0;
    m_VBBindingAttributeDesc.m_AttributesDesc.push_back(attributesDesc);

    attributesDesc.binding = m_VBBindingColors;
    attributesDesc.location = 2;
    attributesDesc.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributesDesc.offset = 0;
    m_VBBindingAttributeDesc.m_AttributesDesc.push_back(attributesDesc);

    attributesDesc.binding = m_VBBindingTexCoords;
    attributesDesc.location = 3;
    attributesDesc.format = VK_FORMAT_R32G32_SFLOAT;
    attributesDesc.offset = 0;
    m_VBBindingAttributeDesc.m_AttributesDesc.push_back(attributesDesc);

    return rc;
}

//--------------------------------------------------------------------
RC VkTriangle::SetupTextures()
{
#define TEXWIDTH  300
#define TEXHEIGHT 300

    RC rc;
    m_HTexture->SetTextureDimensions(TEXWIDTH, TEXHEIGHT, 1);
    if (m_CompressTexture)
    {
        m_HTexture->SetFormat(VK_FORMAT_BC7_UNORM_BLOCK);
    }
    else
    {
        m_HTexture->SetFormat(VK_FORMAT_R8G8B8A8_UNORM);
    }
    m_HTexture->SetFormatPixelSizeBytes(4);

    CHECK_VK_TO_RC(m_HTexture->AllocateImage(VK_IMAGE_USAGE_TRANSFER_SRC_BIT
        ,0
        ,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
        ,VK_IMAGE_LAYOUT_PREINITIALIZED
        ,VK_IMAGE_TILING_LINEAR
        ,"HostTexture"));
    if (!m_HTexture->IsSamplingSupported())
    {
        return RC::MODS_VK_ERROR_FORMAT_NOT_SUPPORTED;
    }
    CHECK_VK_TO_RC(m_HTexture->GetTexImage().SetImageLayout(
        m_InitCmdBuffer.get()
        ,m_HTexture->GetTexImage().GetImageAspectFlags()
        ,VK_IMAGE_LAYOUT_GENERAL              //new layout
        ,VK_ACCESS_HOST_WRITE_BIT             //new access
        ,VK_PIPELINE_STAGE_HOST_BIT));        //new stage

    CHECK_VK_TO_RC(m_HTexture->FillTexture());

    // Create texture that resides in device memory
    m_Texture->SetTextureDimensions(TEXWIDTH, TEXHEIGHT, 1);
    m_Texture->SetFormat(m_HTexture->GetFormat());
    m_Texture->SetFormatPixelSizeBytes(4);

    CHECK_VK_TO_RC(m_Texture->AllocateImage(VK_IMAGE_USAGE_TRANSFER_DST_BIT
        ,0
        ,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        ,VK_IMAGE_LAYOUT_UNDEFINED
        ,VK_IMAGE_TILING_OPTIMAL
        ,"DeviceTexture"));
    if (!m_Texture->IsSamplingSupported())
    {
        return RC::MODS_VK_ERROR_FORMAT_NOT_SUPPORTED;
    }

    CHECK_VK_TO_RC(VkUtil::CopyImage(
        m_InitCmdBuffer.get(),
        VK_NULL_HANDLE,
        m_HTexture->GetTexImage(),  //srcImage
        VK_IMAGE_LAYOUT_GENERAL,    //srcImageLwrrentLayout
        VK_ACCESS_HOST_WRITE_BIT,   //srcImageLwrrentAccess
        VK_PIPELINE_STAGE_HOST_BIT, //srcImageLwrrentStageMask
        m_Texture->GetTexImage(),   //dstImage
        VK_IMAGE_LAYOUT_UNDEFINED,  //dstImageLwrrentLayout
        0,                          //dstImageLwrrentAccessMask
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT)); //dstImageLwrrentStageMask
    m_HTexture->Cleanup();

    m_Sampler.CreateSampler();
    m_Texture->SetVkSampler(m_Sampler); // make m_Texture a combined_image_sampler

    return rc;
}

//--------------------------------------------------------------------
RC VkTriangle::CreateRenderPass()
{
    RC rc;

    // Add attachments
    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = m_SwapChain->GetSurfaceFormat();
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    //clear the buffer at beginning of renderpass:
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    //storing is necessary for display:
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    m_RenderPass->PushAttachmentDescription(VkUtil::AttachmentType::COLOR, &colorAttachment);

    VkAttachmentDescription depthAttachment = {};
    depthAttachment.samples = static_cast<VkSampleCountFlagBits>(m_SampleCount);
    depthAttachment.format = m_DepthImage->GetFormat();
    //clear depth buffer at beginning of renderpass:
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    //dont care about depth buffer after draw as we dont render it:
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    m_RenderPass->PushAttachmentDescription(VkUtil::AttachmentType::DEPTH, &depthAttachment);

    if (m_SampleCount > 1)
    {
        colorAttachment.samples = static_cast<VkSampleCountFlagBits>(m_SampleCount);
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        m_RenderPass->PushAttachmentDescription(VkUtil::AttachmentType::COLOR, &colorAttachment);
    }

    // Add subpasses
    VkSubpassDescription subpassDesc = {};
    subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDesc.colorAttachmentCount = 1;

    VkAttachmentReference colorReference =
        { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

    VkAttachmentReference resolveReference =
        { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

    if (m_SampleCount != 1)
    {
        colorReference.attachment = 2;

        subpassDesc.pResolveAttachments = &resolveReference;
    }

    subpassDesc.pColorAttachments = &colorReference;

    VkAttachmentReference depthReference =
        { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
    subpassDesc.pDepthStencilAttachment = &depthReference;

    m_RenderPass->PushSubpassDescription(&subpassDesc);

    CHECK_VK_TO_RC(m_RenderPass->CreateRenderPass());
    return rc;
}

//--------------------------------------------------------------------
RC VkTriangle::SetupDescriptors()
{
    RC rc;
    const UINT32 maxDescriptorSets = 1; //using only 1 descriptor set
    const UINT32 maxDescriptors    = 2; //using 2 descriptors in the set

    //create descriptor layout
    VkDescriptorSetLayoutBinding layoutBindings[maxDescriptors];
    memset(layoutBindings, 0, sizeof(VkDescriptorSetLayoutBinding) * maxDescriptors);

    //Descriptor 0: UBO in VS
    layoutBindings[0].binding = 0;
    layoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutBindings[0].descriptorCount = 1;
    layoutBindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    layoutBindings[0].pImmutableSamplers = nullptr;

    //Descriptor 1: sampler in FS
    layoutBindings[1].binding = 1;
    layoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutBindings[1].descriptorCount = 1;
    layoutBindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutBindings[1].pImmutableSamplers = nullptr;

    CHECK_VK_TO_RC(m_DescriptorInfo->CreateDescriptorSetLayout(
        0, maxDescriptors, layoutBindings));

    //list all the descriptors used
    vector<VkDescriptorPoolSize> descPoolSizeInfo;
    descPoolSizeInfo.push_back({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 });
    descPoolSizeInfo.push_back({ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 });
    //create descriptor pool
    CHECK_VK_TO_RC(m_DescriptorInfo->CreateDescriptorPool(maxDescriptorSets, descPoolSizeInfo));

    //allocate descriptor sets
    CHECK_VK_TO_RC(m_DescriptorInfo->AllocateDescriptorSets(0, maxDescriptorSets, 0));

    //update the descriptors
    vector<VkWriteDescriptorSet> writeData(maxDescriptors);

    //Descriptor 0: UBO in VS
    writeData[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeData[0].pNext = nullptr;
    writeData[0].dstSet = m_DescriptorInfo->GetDescriptorSet(0);
    writeData[0].descriptorCount = 1;
    writeData[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    VkDescriptorBufferInfo bufferInfo = m_UniformBufferMVPMatrix->GetBufferInfo();
    writeData[0].pBufferInfo = &bufferInfo;
    writeData[0].dstArrayElement = 0;
    writeData[0].dstBinding = 0;

    //Descriptor 1: sampler
    writeData[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeData[1].dstSet = m_DescriptorInfo->GetDescriptorSet(0);
    writeData[1].dstBinding = 1;
    writeData[1].descriptorCount = 1;
    writeData[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    VkDescriptorImageInfo imgInfo = m_Texture->GetDescriptorImageInfo();
    writeData[1].pImageInfo = &imgInfo;
    writeData[1].dstArrayElement = 0;

    //multiple descriptors across multiple sets can be updated at once
    m_DescriptorInfo->UpdateDescriptorSets(writeData.data(),
        static_cast<UINT32>(writeData.size()));
    return rc;
}

//--------------------------------------------------------------------
RC VkTriangle::SetupShaders()
{
    RC rc;
    string vertexShader =
        "#version 400\n"
        "#extension GL_ARB_separate_shader_objects : enable\n"
        "#extension GL_ARB_shading_language_420pack : enable\n"
        "layout (std140, binding = 0) uniform bufferVals {\n"
        "    mat4 mvp;\n"
        "} myBufferVals;\n"
        "layout (location = 0) in vec4 pos;\n"
        "layout (location = 1) in vec3 normal;\n"
        "layout (location = 2) in vec4 color;\n"
        "layout (location = 3) in vec2 txCoords;\n"

        "layout (location = 0) out vec3 outNormal;\n"
        "layout (location = 1) out vec4 outColor;\n"
        "layout (location = 2) out vec2 outTxCoords;\n"

        "out gl_PerVertex { \n"
        "    vec4 gl_Position;\n"
        "};\n"
        "void main() {\n"
        "   outNormal = normal;\n"
        "   outColor = color;\n"
        "   outTxCoords = txCoords;\n"
        "   gl_Position = pos * myBufferVals.mvp;\n"
        "}\n";

    CHECK_VK_TO_RC(m_VertexShader->CreateShader(VK_SHADER_STAGE_VERTEX_BIT,
        vertexShader, "main", m_ShaderReplacement));

    string fragShader =
        "#version 400\n"
        "#extension GL_ARB_separate_shader_objects : enable\n"
        "#extension GL_ARB_shading_language_420pack : enable\n"
        "layout (binding = 1) uniform sampler2D tex;\n"

        "layout (location = 0) in vec3 normal;\n"
        "layout (location = 1) in vec4 color;\n"
        "layout (location = 2) in vec2 texcoord;\n"

        "layout (location = 0) out vec4 outColor;\n"
        "void main() {\n"
        "   outColor = color;\n";
        if (m_GeometryType != VkGeometry::GEOMETRY_PYRAMID)
        {

            fragShader += "   outColor = color + textureLod(tex, texcoord, 0.0);\n";
        }
        fragShader += "}\n";

    CHECK_VK_TO_RC(m_FragmentShader->CreateShader(VK_SHADER_STAGE_FRAGMENT_BIT,
        fragShader, "main", m_ShaderReplacement));

    return rc;
}

//--------------------------------------------------------------------
RC VkTriangle::SetupFrameBuffer()
{
    RC rc;

    //Create a FB for each swap chain image
    m_FrameBuffers.reserve(m_SwapChain->GetNumImages());
    for (UINT32 i = 0; i < m_SwapChain->GetNumImages(); i++)
    {
        m_FrameBuffers.emplace_back(make_unique<VulkanFrameBuffer>(m_pVulkanDev));
    }

    for (UINT32 i = 0; i < m_FrameBuffers.size(); i++)
    {
        vector<VkImageView> attachments;
        //push color image
        MASSERT(m_RenderPass->GetAttachmentType(0) == VkUtil::AttachmentType::COLOR);
        attachments.push_back(m_SwapChain->GetImageView(i));

        //push depth image
        MASSERT(m_RenderPass->GetAttachmentType(1) == VkUtil::AttachmentType::DEPTH);
        attachments.push_back(m_DepthImage->GetImageView());

        if (m_SampleCount > 1)
        {
            MASSERT(m_RenderPass->GetAttachmentType(2) == VkUtil::AttachmentType::COLOR);
            attachments.push_back(m_ColorMsaaImage->GetImageView());
        }

        MASSERT(m_SwapChain->GetSwapChainExtent().width == m_DepthImage->GetWidth());
        m_FrameBuffers[i]->SetWidth(m_SwapChain->GetSwapChainExtent().width);

        MASSERT(m_SwapChain->GetSwapChainExtent().height == m_DepthImage->GetHeight());
        m_FrameBuffers[i]->SetHeight(m_SwapChain->GetSwapChainExtent().height);

        CHECK_VK_TO_RC(m_FrameBuffers[i]->CreateFrameBuffer(attachments,
            m_RenderPass->GetRenderPass()));
    }
    return rc;
}

//--------------------------------------------------------------------
RC VkTriangle::CreatePipeline()
{
    RC rc;
    vector<VkPipelineShaderStageCreateInfo> shaderStages;
    shaderStages.push_back(m_VertexShader->GetShaderStageInfo());
    shaderStages.push_back(m_FragmentShader->GetShaderStageInfo());

    CHECK_VK_TO_RC(m_Pipeline->SetupGraphicsPipeline(&m_VBBindingAttributeDesc,
        m_DescriptorInfo->GetDescriptorSetLayout(0),
        shaderStages,
        m_RenderPass->GetRenderPass(),
        0/*subpass*/,
        static_cast<VkSampleCountFlagBits>(m_SampleCount)));

    return rc;
}

//--------------------------------------------------------------------
void VkTriangle::CleanupAfterSetup()
{
    m_VertexShader->Cleanup();
    m_FragmentShader->Cleanup();
}

//--------------------------------------------------------------------
RC VkTriangle::SetupSemaphoresAndFences()
{
    RC rc;

    VkSemaphoreCreateInfo renderCompleteSemInfo;
    renderCompleteSemInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    renderCompleteSemInfo.pNext = nullptr;
    renderCompleteSemInfo.flags = 0;
    CHECK_VK_TO_RC(m_pVulkanDev->CreateSemaphore(&renderCompleteSemInfo,
        nullptr, &m_RenderCompleteToGoldensSem));

    VkSemaphoreCreateInfo presentCompleteSemInfo;
    presentCompleteSemInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    presentCompleteSemInfo.pNext = nullptr;
    presentCompleteSemInfo.flags = 0;
    CHECK_VK_TO_RC(m_pVulkanDev->CreateSemaphore(&presentCompleteSemInfo,
        nullptr, &m_PresentCompleteSem));

    VkFenceCreateInfo fenceInfo;
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.pNext = nullptr;
    fenceInfo.flags = 0;
    CHECK_VK_TO_RC(m_pVulkanDev->CreateFence(&fenceInfo, nullptr, &m_Fence));
    return rc;
}

//--------------------------------------------------------------------
RC VkTriangle::BuildDrawCmdBuffers()
{
    RC rc;
    m_BuildDrawTimeCallwlator.StartTimeRecording();

    m_DrawCmdBuffers.reserve(m_FrameBuffers.size());
    for (UINT32 i = 0; i < m_FrameBuffers.size(); i++)
    {
        m_DrawCmdBuffers.emplace_back(make_unique<VulkanCmdBuffer>(m_pVulkanDev));
    }
    for (UINT32 i = 0; i < m_DrawCmdBuffers.size(); i++)
    {
        CHECK_VK_TO_RC(m_DrawCmdBuffers[i]->AllocateCmdBuffer(m_CmdPool.get()));

        CHECK_VK_TO_RC(m_DrawCmdBuffers[i]->BeginCmdBuffer());

        vector<VkClearValue> clearValues(m_SampleCount > 1 ? 3 : 2);
        clearValues[0].color = m_ClearColor;
        clearValues[1].depthStencil = { 1.0f, 0 };
        if (m_SampleCount > 1)
        {
            clearValues[2].color = m_ClearColor;
        }

        VkRenderPassBeginInfo renderPassBeginInfo = {};
        renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBeginInfo.pNext = nullptr;
        renderPassBeginInfo.renderPass = m_RenderPass->GetRenderPass();
        renderPassBeginInfo.renderArea.offset.x = 0;
        renderPassBeginInfo.renderArea.offset.y = 0;
        renderPassBeginInfo.renderArea.extent.width = m_FrameBuffers[i]->GetWidth();
        renderPassBeginInfo.renderArea.extent.height = m_FrameBuffers[i]->GetHeight();
        renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassBeginInfo.pClearValues = clearValues.data();
        renderPassBeginInfo.framebuffer = m_FrameBuffers[i]->GetVkFrameBuffer();
        m_pVulkanDev->CmdBeginRenderPass(m_DrawCmdBuffers[i]->GetCmdBuffer(),
            &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        // do all bindings
        VkBuffer buffers[] =
        {
            m_VBPositions->GetBuffer(), m_VBNormals->GetBuffer(),
            m_VBColors->GetBuffer(), m_VBTexCoords->GetBuffer()
        };
        VkDeviceSize bufferOffsets[] = { 0, 0, 0, 0};
        m_pVulkanDev->CmdBindVertexBuffers(m_DrawCmdBuffers[i]->GetCmdBuffer(),
            m_VBBindingPositions, 4, buffers, bufferOffsets);

        if (m_IndexedDraw)
        {
            m_pVulkanDev->CmdBindIndexBuffer(m_DrawCmdBuffers[i]->GetCmdBuffer(),
                m_IndexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
        }

        m_pVulkanDev->CmdBindPipeline(m_DrawCmdBuffers[i]->GetCmdBuffer(),
            VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline->GetPipeline());

        VkDescriptorSet descSet[1] = { m_DescriptorInfo->GetDescriptorSet(0) };
        m_pVulkanDev->CmdBindDescriptorSets(m_DrawCmdBuffers[i]->GetCmdBuffer(),
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_Pipeline->GetPipelineLayout(), 0,
            1,
            descSet, 0, nullptr);

        // Set dynamic viewport and scissor state
        m_Viewport.width = static_cast<float>(m_FrameBuffers[i]->GetWidth());
        m_Viewport.height = static_cast<float>(m_FrameBuffers[i]->GetHeight());
        m_Viewport.minDepth = 0.0f;
        m_Viewport.maxDepth = 1.0f;
        m_Viewport.x = 0;
        m_Viewport.y = 0;
        m_pVulkanDev->CmdSetViewport(m_DrawCmdBuffers[i]->GetCmdBuffer(), 0, 1, &m_Viewport);

        m_Scissor.extent.width = m_FrameBuffers[i]->GetWidth();
        m_Scissor.extent.height = m_FrameBuffers[i]->GetHeight();
        m_Scissor.offset.x = 0;
        m_Scissor.offset.y = 0;
        m_pVulkanDev->CmdSetScissor(m_DrawCmdBuffers[i]->GetCmdBuffer(), 0, 1, &m_Scissor);

        //draw
        if (m_IndexedDraw)
        {
            m_pVulkanDev->CmdDrawIndexed(m_DrawCmdBuffers[i]->GetCmdBuffer(),
                m_NumIndicesToDraw, 1, 0, 0, 0);
        }
        else
        {
            m_pVulkanDev->CmdDraw(m_DrawCmdBuffers[i]->GetCmdBuffer(),
                m_NumVerticesToDraw, 1, 0, 0);
        }

        m_pVulkanDev->CmdEndRenderPass(m_DrawCmdBuffers[i]->GetCmdBuffer());

        CHECK_VK_TO_RC(m_DrawCmdBuffers[i]->EndCmdBuffer());
    }

    m_BuildDrawTimeCallwlator.StopTimeRecording();
    return rc;
}

//--------------------------------------------------------------------
RC VkTriangle::SubmitCmdBufferAndPresent(UINT32 loopIdx)
{
    RC rc;

    UINT32 lwrrentBuffer = m_SwapChain->GetNextSwapChainImage(m_PresentCompleteSem);

    CHECK_RC(m_pGolden->SetSurfaces(m_GoldenSurfaces[lwrrentBuffer].get()));
    m_pGolden->SetLoop(loopIdx); // SetSurfaces resets the loop counter

#ifndef VULKAN_STANDALONE
    VkSemaphore presentSemaphore = m_PresentCompleteSem;
#else
    VkSemaphore presentSemaphore = VK_NULL_HANDLE;
#endif

    //change layout back to present mode and signal the presentSemaphore
    CHECK_VK_TO_RC(m_SwapChain->GetSwapChainImage(lwrrentBuffer)->SetImageLayout(
        m_InitCmdBuffer.get()
        ,m_SwapChain->GetSwapChainImage(lwrrentBuffer)->GetImageAspectFlags()
        ,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL           //new layout
        ,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT               //new access
        ,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT));   // new stage

    if (presentSemaphore != VK_NULL_HANDLE)
    {   //Still need to signal the presentSemaphore
        VkSubmitInfo submit_info =
        {
            VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr,
            0, nullptr,         //waitSemaphoreCount & pWaitSemaphores
            nullptr,
            0, nullptr,         //commandBufferCount & pCommandBuffers
            1, &presentSemaphore //signalSemaphoreCount & pSignalSemaphores
        };

        CHECK_VK_TO_RC(m_pVulkanDev->QueueSubmit(
            m_CmdPool.get()->GetQueue()
            ,1
            ,&submit_info
            ,VK_NULL_HANDLE));
    }

    CHECK_VK_TO_RC(m_DrawCmdBuffers[lwrrentBuffer]->ExelwteCmdBuffer(
        m_PresentCompleteSem, 1, &m_RenderCompleteToGoldensSem,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, true, m_Fence) );

#ifdef VULKAN_STANDALONE
    if (m_CheckGoldens)
#endif
        CHECK_RC(m_pGolden->Run());

    //change layout back to present mode
    CHECK_VK_TO_RC(m_SwapChain->GetSwapChainImage(lwrrentBuffer)->SetImageLayout(
        m_InitCmdBuffer.get()
        ,m_SwapChain->GetSwapChainImage(lwrrentBuffer)->GetImageAspectFlags()
        ,VK_IMAGE_LAYOUT_PRESENT_SRC_KHR        //newLayout
        ,VK_ACCESS_MEMORY_READ_BIT              //newAccess
        ,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT));//dstStageMask

    CHECK_VK_TO_RC(m_SwapChain->PresentImage(lwrrentBuffer,
        0, // No need to synchronize as the goldens have already done it
        m_PresentCompleteSem, !m_SkipPresent));

    return rc;
}

//--------------------------------------------------------------------
RC VkTriangle::UpdateBuffersPerFrame()
{
    RC rc;
    if (m_TestMode == TEST_MODE_STATIC)
    {
        return OK;
    }

    //change the MVP matrices to rotate horizontally, vertically, clockwize, counter clockwise
    m_ModelOrientationDeg += m_TriRotationAngle;
    if (m_ModelOrientationDeg >= 720)
    {
        m_TriRotationAngle = -m_TriRotationAngle; // count down
    }
    else if (m_ModelOrientationDeg <= 0.0)
    {
        m_TriRotationAngle = abs(m_TriRotationAngle); //count up
    }

    if (m_ModelOrientationDeg >= 0 && m_ModelOrientationDeg < 360.0)
    {
        // switch to y asis rotation
        m_ModelOrientationYDeg += m_TriRotationAngle;
        m_ModelOrientationXDeg = 0.0;
    }
    else if (m_ModelOrientationDeg >= 360.0 && m_ModelOrientationDeg < 720)
    {
        m_ModelOrientationYDeg = 0.0;
        m_ModelOrientationXDeg += m_TriRotationAngle;
    }

    GLMatrix modelMatrix = m_Camera.GetModelMatrix(0, 0, 0
                                                   ,m_ModelOrientationXDeg
                                                   ,m_ModelOrientationYDeg
                                                   ,0
                                                   ,1, 1, 1);
    GLMatrix mvpMatrix = m_Camera.GetMVPMatrix(modelMatrix);
    const UINT32 mvpMatrixSize = sizeof(mvpMatrix.mat);

    //Re use setup cmd buffer to do buffer update..
    m_InitCmdBuffer->BeginCmdBuffer();
    m_pVulkanDev->CmdUpdateBuffer(m_InitCmdBuffer->GetCmdBuffer(),
        m_UniformBufferMVPMatrix->GetBuffer(),
        0, mvpMatrixSize, reinterpret_cast<UINT32 *>(&mvpMatrix.mat[0][0]));

    VkBufferMemoryBarrier bufferBarrier = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
    bufferBarrier.buffer = m_UniformBufferMVPMatrix->GetBuffer();
    bufferBarrier.offset = 0;
    bufferBarrier.size = mvpMatrixSize;
    bufferBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    bufferBarrier.dstAccessMask = VK_ACCESS_UNIFORM_READ_BIT;
    m_pVulkanDev->CmdPipelineBarrier(m_InitCmdBuffer->GetCmdBuffer(),
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
        0, 0, nullptr, 1, &bufferBarrier, 0, nullptr);

    CHECK_VK_TO_RC(m_InitCmdBuffer->EndCmdBuffer());
    CHECK_VK_TO_RC(m_InitCmdBuffer->ExelwteCmdBuffer(true, true));
    return rc;
}

RC VkTriangle::CreateGoldenSurfaces()
{
    RC rc;

    const UINT32 swapChainSize = static_cast<UINT32>(m_SwapChain->GetNumImages());
    for (UINT32 surfaceIdx = 0; surfaceIdx < swapChainSize; surfaceIdx++)
    {
        m_GoldenSurfaces.emplace_back(make_unique<VulkanGoldenSurfaces>(&m_VulkanInst));
        auto &gs = m_GoldenSurfaces[surfaceIdx];

        gs->SelectWaitSemaphore(m_RenderCompleteToGoldensSem);

        CHECK_VK_TO_RC(gs->AddSurface("c"
           ,m_SwapChain->GetSwapChainImage(surfaceIdx)
           ,m_InitCmdBuffer.get()));

        // TODO: Enable capturing the Depth image once validation layers
        // stop complaining about non-filled content:
        //CHECK_VK_TO_RC(gs->AddSurface("z", m_DepthImage,
        //    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        //    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        //    &m_InitCmdBuffer));
    }

    return rc;
}

//--------------------------------------------------------------------
void VkTriangle::AllocateVulkanDeviceResources()
{
    m_pVulkanDev = m_VulkanInst.GetVulkanDevice();

    // Need to set the timeout threshold for the VulkanDevice before creating
    // any command buffers that run on it so that they inherit this timeout.
    m_pVulkanDev->SetTimeoutNs(
        static_cast<UINT64>(GetTestConfiguration()->TimeoutMs()) * 1'000'000ULL);

    m_VulkanInst.ClearDebugMessagesToIgnore();
    // The driver hides VK_LW_glsl_shader extension now and we have
    // "a couple months" to implement bug 2025150:
    m_VulkanInst.AddDebugMessageToIgnore(
    {
        VK_DEBUG_REPORT_OBJECT_TYPE_UNKNOWN_EXT
        ,5
        ,"SC"   //pre 1.1.101.0 version
        ,"SPIR-V module not valid: Invalid SPIR-V magic number%c"
        ,'.'
    });

    m_VulkanInst.AddDebugMessageToIgnore(
    {
        VK_DEBUG_REPORT_OBJECT_TYPE_UNKNOWN_EXT
        ,0
        ,"Validation"
        ,"SPIR-V module not valid: Invalid SPIR-V magic number%c"
        ,'.'
    });
    m_VulkanInst.AddDebugMessageToIgnore(
    {
        VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_BUFFER_EXT
        ,0
        ,"Validation"
        ,"Submitted command buffer expects VkImage %*x[] (subresource: aspectMask 0x1 array layer 0, mip level 0) to be in layout VK_IMAGE_LAYOUT_GENERAL--instead, current layout is VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL%c" //$
        ,'.'
    });

#ifndef VULKAN_STANDALONE
    m_SwapChain.reset(new SwapChainMods(&m_VulkanInst, m_pVulkanDev));
#else
    m_SwapChain.reset(new SwapChainKHR(&m_VulkanInst, m_pVulkanDev));
#endif

    m_DepthImage.reset(new VulkanImage(m_pVulkanDev));
    m_HUniformBufferMVPMatrix.reset(new VulkanBuffer(m_pVulkanDev));
    m_UniformBufferMVPMatrix.reset(new VulkanBuffer(m_pVulkanDev));
    m_VBHPositions.reset(new VulkanBuffer(m_pVulkanDev));
    m_VBHNormals.reset(new VulkanBuffer(m_pVulkanDev));
    m_VBHColors.reset(new VulkanBuffer(m_pVulkanDev));
    m_VBHTexCoords.reset(new VulkanBuffer(m_pVulkanDev));
    m_VBPositions.reset(new VulkanBuffer(m_pVulkanDev));
    m_VBNormals.reset(new VulkanBuffer(m_pVulkanDev));
    m_VBColors.reset(new VulkanBuffer(m_pVulkanDev));
    m_VBTexCoords.reset(new VulkanBuffer(m_pVulkanDev));
    m_HIndexBuffer.reset(new VulkanBuffer(m_pVulkanDev));
    m_IndexBuffer.reset(new VulkanBuffer(m_pVulkanDev));
    m_HTexture.reset(new VulkanTexture(m_pVulkanDev));
    m_Texture.reset(new VulkanTexture(m_pVulkanDev));
    m_CmdPool.reset(new VulkanCmdPool(m_pVulkanDev));
    m_InitCmdBuffer.reset(new VulkanCmdBuffer(m_pVulkanDev));
    m_RenderPass.reset(new VulkanRenderPass(m_pVulkanDev));
    m_DescriptorInfo.reset(new DescriptorInfo(m_pVulkanDev));
    m_VertexShader.reset(new VulkanShader(m_pVulkanDev));
    m_FragmentShader.reset(new VulkanShader(m_pVulkanDev));
    m_Pipeline.reset(new VulkanPipeline(m_pVulkanDev));
    m_ColorMsaaImage = make_unique<VulkanImage>(m_pVulkanDev);
    m_Sampler = VulkanSampler(m_pVulkanDev);
}

//--------------------------------------------------------------------
RC VkTriangle::SetupTest()
{
    RC rc;
    m_SetupTimeCallwlator.StartTimeRecording();

#ifndef VULKAN_STANDALONE
    UINT32 gpuIndex = GetBoundGpuSubdevice()->GetGpuInst();
#else
    UINT32 gpuIndex = GPUINDEX;
#endif

    const bool initializeDevice = true;
    //This is a simple application, no extra device extensions reqired.
    vector<string> extensionNames;
    CHECK_VK_TO_RC(m_VulkanInst.InitVulkan(ENABLE_VK_DEBUG
                                           ,gpuIndex
                                           ,extensionNames
                                           ,initializeDevice
                                           ,m_EnableValidation));

    AllocateVulkanDeviceResources();

    //Setup semaphores
    CHECK_RC(SetupSemaphoresAndFences());

    //create swap chain
    VkResult res = m_SwapChain->Init(
#ifndef VULKAN_STANDALONE
        GetTestConfiguration()->DisplayWidth()
        ,GetTestConfiguration()->DisplayHeight()
        ,SwapChain::SINGLE_IMAGE_MODE
        ,m_pVulkanDev->GetPhysicalDevice()->GetGraphicsQueueFamilyIdx()
        ,0 //deviceQueueIndex
#else
        m_Hinstance
        ,m_HWindow
        ,SwapChainKHR::MULTI_IMAGE_MODE
        ,m_Fence
        ,m_pVulkanDev->GetPhysicalDevice()->GetGraphicsQueueFamilyIdx()
        ,0 //deviceQueueIndex
#endif
    );
    CHECK_VK_TO_RC(res);
    CHECK_VK_TO_RC(m_CmdPool->InitCmdPool(
        m_pVulkanDev->GetPhysicalDevice()->GetGraphicsQueueFamilyIdx(), 0 /*queueIdx*/));

    //create command buffers
    CHECK_VK_TO_RC(m_InitCmdBuffer->AllocateCmdBuffer(m_CmdPool.get()));

    //create depth buffer
    CHECK_RC(SetupDepthBuffer());

    if (m_SampleCount > 1)
    {
        CHECK_RC(SetupMsaaImage());
    }

    //setup buffers
    CHECK_RC(SetupBuffers());

    //set up textures
    CHECK_RC(SetupTextures());

    //Render pass
    CHECK_RC(CreateRenderPass());

    //set up descriptors
    CHECK_RC(SetupDescriptors());

    DEFER
    {
        CleanupAfterSetup();
    };

    //set up shaders
    CHECK_RC(SetupShaders());

    //set up fb
    CHECK_RC(SetupFrameBuffer());

    //Create pipeline
    CHECK_RC(CreatePipeline());

    CHECK_RC(CreateGoldenSurfaces());

    m_SetupTimeCallwlator.StopTimeRecording();
    return rc;
}

//--------------------------------------------------------------------
RC VkTriangle::RunTest()
{
    RC rc;

    m_ModelOrientationYDeg = 0;

    //
    // Turn off unexpected interrupt checking because nonblocking interrupts
    // may be mis-attributed
    //
    m_RunTimeCallwlatorPerFrame.StartTimeRecording();

    CHECK_RC(BuildDrawCmdBuffers());

    for (UINT32 i = 0; i < m_NumFramesToDraw; i++)
    {
        CHECK_RC(SubmitCmdBufferAndPresent(i));
        CHECK_RC(UpdateBuffersPerFrame());
    }
    m_RunTimeCallwlatorPerFrame.StopTimeRecording();

    CHECK_VK_TO_RC(m_pVulkanDev->QueueWaitIdle(m_pVulkanDev->GetGraphicsQueue(0)));

    VerbosePrintf(
        "Time Taken: Setup %f seconds, Build %f seconds, Runtime %f seconds (%f per frame)\n",
        m_SetupTimeCallwlator.GetTimeSinceLastRecordingUS()/1000000.0,
        m_BuildDrawTimeCallwlator.GetTimeSinceLastRecordingUS() / 1000000.0,
        m_RunTimeCallwlatorPerFrame.GetTimeSinceLastRecordingUS() / 1000000.0,
        m_RunTimeCallwlatorPerFrame.GetTimeSinceLastRecordingUS() / m_NumFramesToDraw / 1000000.0);

    return rc;
}

//--------------------------------------------------------------------
RC VkTriangle::CleanupTest()
{
    StickyRC rc;
    // Only attempt to free Vulkan device resources if the VulkanDevice object
    // was created successfully.
    if (!m_pVulkanDev)
    {
        return RC::OK;
    }

    rc = m_pVulkanDev->DeviceWaitIdle();

    if (m_DepthImage.get())
    {
        m_DepthImage->Cleanup();
    }
    if (m_HUniformBufferMVPMatrix.get())
    {
        m_HUniformBufferMVPMatrix->Cleanup();
    }
    if (m_UniformBufferMVPMatrix.get())
    {
        m_UniformBufferMVPMatrix->Cleanup();
    }
    if (m_HTexture.get())
    {
        m_HTexture->Cleanup();
    }
    if (m_VBHPositions.get())
    {
        m_VBHPositions->Cleanup();
    }
    if (m_VBHNormals.get())
    {
        m_VBHNormals->Cleanup();
    }
    if (m_VBHColors.get())
    {
        m_VBHColors->Cleanup();
    }
    if (m_VBHTexCoords.get())
    {
        m_VBHTexCoords->Cleanup();
    }
    if (m_VBPositions.get())
    {
        m_VBPositions->Cleanup();
    }
    if (m_VBNormals.get())
    {
        m_VBNormals->Cleanup();
    }
    if (m_VBColors.get())
    {
        m_VBColors->Cleanup();
    }
    if (m_VBTexCoords.get())
    {
        m_VBTexCoords->Cleanup();
    }
    if (m_HIndexBuffer.get())
    {
        m_HIndexBuffer->Cleanup();
    }
    if (m_IndexBuffer.get())
    {
        m_IndexBuffer->Cleanup();
    }

    m_VBBindingAttributeDesc.Reset();

    if (m_HTexture.get())
    {
        m_HTexture->Cleanup();
    }
    if (m_Texture.get())
    {
        m_Texture->Cleanup();
    }

    m_Sampler.Cleanup();

    if (m_RenderPass.get())
    {
        m_RenderPass->Cleanup();
    }

    if (m_DescriptorInfo.get())
    {
        rc = VkUtil::VkErrorToRC(m_DescriptorInfo->Cleanup());
    }

    if (m_InitCmdBuffer.get())
    {
        rc = VkUtil::VkErrorToRC(m_InitCmdBuffer->ResetCmdBuffer(
                                    VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT));
        m_InitCmdBuffer->FreeCmdBuffer();
    }

    for (UINT32 i = 0; i < m_DrawCmdBuffers.size(); i++)
    {
        if (m_DrawCmdBuffers[i].get())
        {
            rc = VkUtil::VkErrorToRC(m_DrawCmdBuffers[i]->ResetCmdBuffer(
                VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT));
        }
    }

    //destroying framebuffers explicitly
    for (const auto& fb : m_FrameBuffers)
    {
        if (fb.get())
        {
             fb->Cleanup();
        }
    }
    m_FrameBuffers.clear();

    //destroy Fences and semaphores
    m_pVulkanDev->DestroyFence(m_Fence);
    m_pVulkanDev->DestroySemaphore(m_RenderCompleteToGoldensSem);
    m_pVulkanDev->DestroySemaphore(m_PresentCompleteSem);

    for (UINT32 i = 0; i < m_DrawCmdBuffers.size(); i++)
    {
        if (m_DrawCmdBuffers[i].get())
        {
            m_DrawCmdBuffers[i]->FreeCmdBuffer();
        }
    }
    m_DrawCmdBuffers.clear();

    if (m_CmdPool.get())
    {
        m_CmdPool->DestroyCmdPool();
    }
    if (m_SwapChain.get())
    {
        m_SwapChain->Cleanup();
    }

    if (m_Pipeline.get())
    {
        m_Pipeline->Cleanup();
    }

    for (const auto& surface : m_GoldenSurfaces)
    {
        if (surface.get())
        {
            surface->Cleanup();
        }
    }
    m_GoldenSurfaces.clear();

    if (m_ColorMsaaImage.get())
    {
        m_ColorMsaaImage->Cleanup();
    }

    m_pVulkanDev->Shutdown();
    m_pVulkanDev = nullptr;
    m_VulkanInst.DestroyVulkan();

    return rc;
}

//--------------------------------------------------------------------
RC VkTriangle::Setup()
{
    RC rc;

    CHECK_RC(GpuTest::Setup());

#ifndef VULKAN_STANDALONE
    // Init dgl stuff
    CHECK_RC(m_mglTestContext.SetProperties(
                GetTestConfiguration(),
                false,      // double buffer
                GetBoundGpuDevice(),
                GetDispMgrReqs(),
                0,          //don't override color format
                false,      //we explicitly enforce FBOs depending on the testmode.
                false,      //don't render to sysmem
                0));        //do not use layered FBO
    if (!m_mglTestContext.IsSupported())
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    CHECK_RC(m_mglTestContext.Setup());
#endif

    CHECK_RC(SetupTest());
    return rc;
}

//--------------------------------------------------------------------
RC VkTriangle::Run()
{
    StickyRC rc = RunTest();
    rc = m_pGolden->ErrorRateTest(GetJSObject());
    rc.Clear(); // Bug 1936332
    return rc;
}

//--------------------------------------------------------------------
RC VkTriangle::Cleanup()
{
    StickyRC rc = CleanupTest();

#ifndef VULKAN_STANDALONE
    rc = m_mglTestContext.Cleanup();
#endif

    rc = GpuTest::Cleanup();

    return rc;
}

#ifndef VULKAN_STANDALONE

JS_CLASS_INHERIT(VkTriangle, GpuTest, "Draw a triangle using Vulkan.");

CLASS_PROP_READWRITE(VkTriangle, CompressTexture,           bool,
    "Use compressed textures");
CLASS_PROP_READWRITE(VkTriangle, EnableValidation,          bool,
    "Enable/Disable validation layers (default = false)");
CLASS_PROP_READWRITE(VkTriangle, NumFramesToDraw,           UINT32,
    "Number of times to draw the frame");
CLASS_PROP_READWRITE(VkTriangle, TriRotationAngle,          float,
    "Angle of rotation for triangle");
CLASS_PROP_READWRITE(VkTriangle, IndexedDraw,               bool,
    "Use indexed rendering");
CLASS_PROP_READWRITE(VkTriangle, SkipPresent,               bool,
    "Do not present to screen");
CLASS_PROP_READWRITE(VkTriangle, ShaderReplacement,         bool,
    "Shader replacement");
CLASS_PROP_READWRITE(VkTriangle, TestMode,                  UINT32,
    "TestMode:Static(0):Buffers not updated between draw frames. Dynamic(1): They are.");
CLASS_PROP_READWRITE(VkTriangle, GeometryType,              UINT32,
    "GeometryType to draw 0=Triangle, 1=Torus, 2=Pyramid (default=0)");
CLASS_PROP_READWRITE(VkTriangle, SampleCount,               UINT32,
    "Number of samples to take per fragment");
#endif
