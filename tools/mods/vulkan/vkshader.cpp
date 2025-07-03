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

#include "vkdev.h"
#include "vkshader.h"
#include "vkinstance.h"
#include "core/include/crc.h"
#include <string>
//There is a bug that issues warnings that a template default parameter was redefined on every
//template instaniation. So just ignore them until we can move on to a better more current version.
#if defined(_MSC_VER) && 1900 == _MSC_VER // avoid MSVC bug
#pragma warning(push)
#pragma warning(disable:4348)
#endif

#include <boost/spirit/include/qi.hpp> // for string parsing.

#if defined(_MSC_VER) && 1900 == _MSC_VER // avoid MSVC bug
#pragma warning(pop)
#endif

using namespace boost::spirit;
map<VkPhysicalDevice, TBuiltInResource> VulkanShader::s_Resources;

//--------------------------------------------------------------------
VkResult VulkanShader::GetShaderInSPIRVFormat
(
    VkShaderStageFlagBits stageFlag,
    vector <UINT32> &shaderSPIRVFormat
)
{
    VkResult res = VK_SUCCESS;
    shaderSPIRVFormat.clear();
    constexpr bool dumpSPIRVShader = false;
    if (dumpSPIRVShader)
    {
        Printf(Tee::PriNormal, "Colwerting this shader to SPIR-V\n%s\n",
               m_ShaderSource.c_str());
    }
    res = GlslToSpirv(stageFlag, m_ShaderSource.c_str(), shaderSPIRVFormat);
    if (res != VK_SUCCESS)
    {
        Printf(Tee::PriNormal, "Failed to colwert GLSL to SPIR-V\n");
    }
    if (dumpSPIRVShader)
    {
        Printf(Tee::PriNormal, "SPIR-V\n");
        UINT32 count = static_cast<UINT32>(shaderSPIRVFormat.size());
        for (UINT32 j = 0; j < count;)
        {
            for (UINT32 i = 0; i < 8 && j < count; i++, j++)
            {
                Printf(Tee::PriNormal, "0x%08x ", shaderSPIRVFormat[j]);
            }
            Printf(Tee::PriNormal, "\n");
        }
    }
    return res;
}

EShLanguage VulkanShader::FindLanguage(const VkShaderStageFlagBits shader_type)
{
    switch (shader_type)
    {
        case VK_SHADER_STAGE_VERTEX_BIT:                    return EShLangVertex;
        case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:      return EShLangTessControl;
        case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:   return EShLangTessEvaluation;
        case VK_SHADER_STAGE_GEOMETRY_BIT:                  return EShLangGeometry;
        case VK_SHADER_STAGE_FRAGMENT_BIT:                  return EShLangFragment;
        case VK_SHADER_STAGE_COMPUTE_BIT:                   return EShLangCompute;
        case VK_SHADER_STAGE_TASK_BIT_LW:                   return EShLangTaskLW;
        case VK_SHADER_STAGE_MESH_BIT_LW:                   return EShLangMeshLW;
        case VK_SHADER_STAGE_RAYGEN_BIT_LW:                 return EShLangRayGenLW;
        case VK_SHADER_STAGE_ANY_HIT_BIT_LW:                return EShLangAnyHitLW;
        case VK_SHADER_STAGE_CLOSEST_HIT_BIT_LW:            return EShLangClosestHitLW;
        case VK_SHADER_STAGE_MISS_BIT_LW:                   return EShLangMissLW;
        case VK_SHADER_STAGE_INTERSECTION_BIT_LW:           return EShLangIntersectLW;
        case VK_SHADER_STAGE_CALLABLE_BIT_LW:               return EShLangCallableLW;
        default:                                            return EShLangVertex;
    }
}

//
// These are the default resources for TBuiltInResources, used for both
//  - parsing this string for the case where the user didn't supply one
//  - dumping out a template for user construction of a config file
//
void VulkanShader::InitResources()
{
    VkPhysicalDevice vkDev = m_pVulkanDev->GetPhysicalDevice()->GetVkPhysicalDevice();

    TBuiltInResource& resources = s_Resources[vkDev];

    if (resources.maxLights == 0)
    {
        VulkanInstance* pVulkanInst = m_pVulkanDev->GetVulkanInstance();

        VkPhysicalDeviceProperties properties;
        pVulkanInst->GetPhysicalDeviceProperties(vkDev, &properties);

        VkPhysicalDeviceTransformFeedbackPropertiesEXT xfbProperties =
            { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_PROPERTIES_EXT };
        VkPhysicalDeviceMeshShaderPropertiesLW meshShaderProperties =
            { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_LW, &xfbProperties };
        if (pVulkanInst->GetPhysicalDeviceProperties2)
        {
            VkPhysicalDeviceProperties2 properties2 =
                { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &meshShaderProperties };
            pVulkanInst->GetPhysicalDeviceProperties2(vkDev, &properties2);
        }
        // This values are taken from
        // drivers/cglang/cgc/glasm/gp5_hal.cpp:GetProfileConstantValue_gp5 and are reflected in
        // the GLSL environment as gl_* constants.
        // Resource                                 Value               gl_* constant
        resources.maxAtomicCounterBindings        = 8;                                            //$ gl_MaxAtomicCounterBindings
        resources.maxAtomicCounterBufferSize      = 4 * 16384;                                    //$ gl_MaxAtomicCounterBufferSize
        resources.maxLights                       = 8;                                            //$ gl_MaxLights
        resources.maxClipPlanes                   = properties.limits.maxClipDistances;           //$ gl_MaxClipPlanes
        resources.maxTextureUnits                 = 4;                                            //$ gl_MaxTextureUnits
        resources.maxTextureCoords                = 8;                                            //$ gl_MaxTextureCoords
        resources.maxVertexAttribs                = properties.limits.maxVertexInputAttributes;   //$ gl_MaxVertexAttribs s/b 32;
        resources.maxVertexUniformComponents      = 4096;                                         //$ gl_MaxVertexUniformComponents
        resources.maxVaryingFloats                = 124;                                          //$ gl_MaxVaryingFloats
        resources.maxVertexTextureImageUnits      = 32;                                           //$ gl_MaxVertexTextureImageUnits
        resources.maxCombinedTextureImageUnits    = 192;                                          //$ gl_MaxCombinedTextureImageUnits
        resources.maxTextureImageUnits            = 32;                                           //$ gl_MaxTextureImageUnits
        resources.maxFragmentUniformComponents    = 4096;                                         //$ gl_MaxFragmentUniformComponents
        resources.maxDrawBuffers                  = 8;                                            //$ gl_MaxDrawBuffers
        resources.maxVertexUniformVectors         = 1024;                                         //$ gl_MaxVertexUniformVectors
        resources.maxVaryingVectors               = 31;                                           //$ gl_MaxVaryingVectors
        resources.maxFragmentUniformVectors       = 1024;                                         //$ gl_MaxFragmentUniformVectors
        resources.maxVertexOutputVectors          = 32;                                           //$ gl_MaxVertexOutputVectors
        resources.maxFragmentInputVectors         = 32;                                           //$ gl_MaxFragmentInputVectors
        resources.minProgramTexelOffset           = -8;                                           //$ gl_MinProgramTexelOffset
        resources.maxProgramTexelOffset           = 7;                                            //$ gl_MaxProgramTexelOffset
        resources.maxClipDistances                = properties.limits.maxClipDistances;           //$ gl_MaxClipDistances s/b 8;
        resources.maxComputeWorkGroupCountX       = properties.limits.maxComputeWorkGroupCount[0];//$ gl_MaxComputeWorkGroupCount s/b 65535;
        resources.maxComputeWorkGroupCountY       = properties.limits.maxComputeWorkGroupCount[1];//$ gl_MaxComputeWorkGroupCount s/b 65535;
        resources.maxComputeWorkGroupCountZ       = properties.limits.maxComputeWorkGroupCount[2];//$ gl_MaxComputeWorkGroupCount s/b 65535;
        resources.maxComputeWorkGroupSizeX        = properties.limits.maxComputeWorkGroupSize[0]; //$ gl_MaxComputeWorkGroupSize s/b 1536;
        resources.maxComputeWorkGroupSizeY        = properties.limits.maxComputeWorkGroupSize[1]; //$ gl_MaxComputeWorkGroupSize s/b 1024;
        resources.maxComputeWorkGroupSizeZ        = properties.limits.maxComputeWorkGroupSize[2]; //$ gl_MaxComputeWorkGroupSize s/b 64;
        resources.maxComputeUniformComponents     = 2048;                                         //$ gl_MaxComputeUniformComponents
        resources.maxComputeTextureImageUnits     = 32;                                           //$ gl_MaxComputeTextureImageUnits
        resources.maxComputeImageUniforms         = 8;                                            //$ gl_MaxComputeImageUniforms
        resources.maxComputeAtomicCounters        = 16384;                                        //$ gl_MaxComputeAtomicCounters
        resources.maxComputeAtomicCounterBuffers  = 5;                                            //$ gl_MaxComputeAtomicCounterBuffers
        resources.maxVaryingComponents            = 124;                                          //$ gl_MaxVaryingComponents
        resources.maxVertexOutputComponents       = properties.limits.maxVertexOutputComponents;  //$ gl_MaxVertexOutputComponents s/b 128
        resources.maxGeometryInputComponents      = properties.limits.maxGeometryInputComponents; //$ gl_MaxGeometryInputComponents s/b 128
        resources.maxGeometryOutputComponents     = properties.limits.maxGeometryOutputComponents;//$ gl_MaxGeometryOutputComponents s/b 128;
        resources.maxFragmentInputComponents      = properties.limits.maxFragmentInputComponents; //$ gl_MaxFragmentInputComponents s/b 128;
        resources.maxImageUnits                   = 8;                                            //$ gl_MaxImageUnits
        resources.maxCombinedImageUnitsAndFragmentOutputs = 16;                                   //$ gl_MaxCombinedImageUnitsAndFragmentOutputs
        resources.maxCombinedShaderOutputResources = 16;                                          //$ gl_MaxCombinedShaderOutputResources
        // Need better way of getting this value, Lwdqro = 64, otherwise 32
        resources.maxImageSamples                 = 32;                                           //$ gl_MaxImageSamples
        resources.maxVertexImageUniforms          = 8;                                            //$ gl_MaxVertexImageUniforms
        resources.maxTessControlImageUniforms     = 8;                                            //$ gl_MaxTessControlImageUniforms
        resources.maxTessEvaluationImageUniforms  = 8;                                            //$ gl_MaxTessEvaluationImageUniforms
        resources.maxGeometryImageUniforms        = 8;                                            //$ gl_MaxGeometryImageUniforms
        resources.maxFragmentImageUniforms        = 8;                                            //$ gl_MaxFragmentImageUniforms
        resources.maxCombinedImageUniforms        = 48;                                           //$ gl_MaxCombinedImageUniforms
        resources.maxGeometryTextureImageUnits    = 32;                                           //$ gl_MaxGeometryTextureImageUnits
        resources.maxGeometryOutputVertices       = 1024;                                         //$ gl_MaxGeometryOutputVertices
        resources.maxGeometryTotalOutputComponents = 1024;                                        //$ gl_MaxGeometryTotalOutputComponents
        resources.maxGeometryUniformComponents    = 2048;                                         //$ gl_MaxGeometryUniformComponents
        resources.maxGeometryVaryingComponents    = 124;                                          //$ gl_MaxGeometryVaryingComponents
        resources.maxTessControlInputComponents       = 128;                                      //$ gl_MaxTessControlInputComponents
        resources.maxTessControlOutputComponents      = 128;                                      //$ gl_MaxTessControlOutputComponents
        resources.maxTessControlTextureImageUnits     = 32;                                       //$ gl_MaxTessControlTextureImageUnits
        resources.maxTessControlUniformComponents     = 2048;                                     //$ gl_MaxTessControlUniformComponents
        resources.maxTessControlTotalOutputComponents = 4216;                                     //$ gl_MaxTessControlTotalOutputComponents
        resources.maxTessEvaluationInputComponents    = 128;                                      //$ gl_MaxTessEvaluationInputComponents
        resources.maxTessEvaluationOutputComponents   = 128;                                      //$ gl_MaxTessEvaluationOutputComponents
        resources.maxTessEvaluationTextureImageUnits  = 32;                                       //$ gl_MaxTessEvaluationTextureImageUnits
        resources.maxTessEvaluationUniformComponents  = 2048;                                     //$ gl_MaxTessEvaluationUniformComponents
        resources.maxTessPatchComponents              = 120;                                      //$ gl_MaxTessPatchComponents
        resources.maxPatchVertices                    = 32;                                       //$ gl_MaxPatchVertices
        resources.maxTessGenLevel                     = properties.limits.maxTessellationGenerationLevel; //$ gl_MaxTessGenLevel s/b 64;
        resources.maxViewports                        = properties.limits.maxViewports;           //$ gl_MaxViewports s/b 16;
        resources.maxVertexAtomicCounters             = 16384;                                    //$ gl_MaxVertexAtomicCounters
        resources.maxTessControlAtomicCounters        = 16384;                                    //$ gl_MaxTessControlAtomicCounters
        resources.maxTessEvaluationAtomicCounters     = 16384;                                    //$ gl_MaxTessEvaluationAtomicCounters
        resources.maxGeometryAtomicCounters           = 16384;                                    //$ gl_MaxGeometryAtomicCounters
        resources.maxFragmentAtomicCounters           = 16384;                                    //$ gl_MaxFragmentAtomicCounters
        resources.maxCombinedAtomicCounters           = 6*16384;                                  //$ gl_MaxCombinedAtomicCounters
        resources.maxVertexAtomicCounterBuffers       = 8;                                        //$ gl_MaxVertexAtomicCounterBuffers
        resources.maxTessControlAtomicCounterBuffers  = 8;                                        //$ gl_MaxTessControlAtomicCounterBuffers
        resources.maxTessEvaluationAtomicCounterBuffers = 8;                                      //$ gl_MaxTessEvaluationAtomicCounterBuffers
        resources.maxGeometryAtomicCounterBuffers      = 8;                                       //$ gl_MaxGeometryAtomicCounterBuffers
        resources.maxFragmentAtomicCounterBuffers      = 8;                                       //$ gl_MaxFragmentAtomicCounterBuffers
        resources.maxCombinedAtomicCounterBuffers      = 6*8;                                     //$ gl_MaxCombinedAtomicCounterBuffers
        resources.maxTransformFeedbackBuffers          = xfbProperties.maxTransformFeedbackBuffers; //$ gl_MaxTransformFeedbackBuffers s/b 4;
        resources.maxTransformFeedbackInterleavedComponents   = 128;                              //$ gl_MaxTransformFeedbackInterleavedComponents
        resources.maxLwllDistances                     = properties.limits.maxLwllDistances;      //$ gl_MaxLwllDistances s/b 8;
        resources.maxCombinedClipAndLwllDistances      = properties.limits.maxCombinedClipAndLwllDistances; //$ gl_MaxCombinedClipAndLwllDistances s/b 8;
        resources.maxSamples                           = 32;                                       //$ gl_MaxSamples
        resources.maxMeshOutputVerticesLW   = meshShaderProperties.maxMeshOutputVertices;          //$ gl_MaxMeshOutputVerticesLW s/b 256;
        resources.maxMeshOutputPrimitivesLW = meshShaderProperties.maxMeshOutputPrimitives;        //$ gl_MaxMeshOutputPrimitivesLW s/b 512
        resources.maxMeshWorkGroupSizeX_LW  = meshShaderProperties.maxMeshWorkGroupSize[0];        //$ gl_MaxMeshWorkGroupSizeLW s/b {32, 1, 1}
        resources.maxMeshWorkGroupSizeY_LW  = meshShaderProperties.maxMeshWorkGroupSize[1];        //$ 1;
        resources.maxMeshWorkGroupSizeZ_LW  = meshShaderProperties.maxMeshWorkGroupSize[2];        //$ 1;
        resources.maxTaskWorkGroupSizeX_LW  = meshShaderProperties.maxTaskWorkGroupSize[0];        //$ gl_MaxTaskWorkGroupSizeLW s/b {32, 1, 1}
        resources.maxTaskWorkGroupSizeY_LW  = meshShaderProperties.maxTaskWorkGroupSize[1];        //$ 1;
        resources.maxTaskWorkGroupSizeZ_LW  = meshShaderProperties.maxTaskWorkGroupSize[2];        //$ 1;
        resources.maxMeshViewCountLW        = meshShaderProperties.maxMeshMultiviewViewCount;      //$ gl_MaxMeshViewCountLW s/b 4;

        // Still need to find real values for these.
        resources.limits.nonInductiveForLoops                 = 1;
        resources.limits.whileLoops                           = 1;
        resources.limits.doWhileLoops                         = 1;
        resources.limits.generalUniformIndexing               = 1;
        resources.limits.generalAttributeMatrixVectorIndexing = 1;
        resources.limits.generalVaryingIndexing               = 1;
        resources.limits.generalSamplerIndexing               = 1;
        resources.limits.generalVariableIndexing              = 1;
        resources.limits.generalConstantMatrixVectorIndexing  = 1;
    }
}

VkResult VulkanShader::GlslToSpirv
(
    VkShaderStageFlagBits shader_type,
    const char *pshader,
    std::vector<unsigned int> &spirv
)
{
    EShLanguage stage = FindLanguage(shader_type);
    glslang::TShader shader(stage);
    glslang::TProgram program;
    const char *shaderStrings[1];
    InitResources();

    // Enable SPIR-V and Vulkan rules when parsing GLSL
    EShMessages messages = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules);

    shaderStrings[0] = pshader;
    shader.setStrings(shaderStrings, 1);

    shader.setElwClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_2);
    shader.setElwTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_5);

    if (!shader.parse(
        &s_Resources[m_pVulkanDev->GetPhysicalDevice()->GetVkPhysicalDevice()],
        100, false, messages))
    {
        PrintShader(pshader);
        Printf(Tee::PriNormal, shader.getInfoLog());
        Printf(Tee::PriNormal, shader.getInfoDebugLog());
        return VK_ERROR_ILWALID_SHADER_LW; // something didn't work
    }

    program.addShader(&shader);

    //
    // Program-level processing...
    //

    if (!program.link(messages))
    {
        Printf(Tee::PriNormal, shader.getInfoLog());
        Printf(Tee::PriNormal, shader.getInfoDebugLog());
        return VK_ERROR_ILWALID_SHADER_LW;
    }

    glslang::GlslangToSpv(*program.getIntermediate(stage), spirv);
    return VK_SUCCESS;
}

void VulkanShader::PrintShader(const char * pShader)
{
    // Print out the shader along with each line number
    int lineNum = 1;
    string szShader(pShader);
    std::string::size_type pos = 0;
    std::string::size_type head = 0;
    while ((pos = szShader.find("\n", head)) != std::string::npos)
    {
        Printf(Tee::PriNormal, "%03d:%.*s\n", lineNum, (int)(pos-head), &szShader[head]);
        head = pos+1; // jump passed the "\n"
        lineNum++;
    }
    //print the last line.
    Printf(Tee::PriNormal, "%03d:%s\n", lineNum, &szShader[head]);
}

#ifndef VULKAN_STANDALONE
namespace
{
    const char * VkShaderStageFlagBits2Prefix(VkShaderStageFlagBits stageFlag)
    {
        switch (stageFlag)
        {
            case VK_SHADER_STAGE_VERTEX_BIT: return "vtx";
            case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT: return "tcl";
            case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT: return "tev";
            case VK_SHADER_STAGE_GEOMETRY_BIT: return "geo";
            case VK_SHADER_STAGE_FRAGMENT_BIT: return "fra";
            case VK_SHADER_STAGE_COMPUTE_BIT: return "com";
            case VK_SHADER_STAGE_RAYGEN_BIT_KHR: return "ryg";
            case VK_SHADER_STAGE_ANY_HIT_BIT_KHR: return "anh";
            case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR: return "clh";
            case VK_SHADER_STAGE_MISS_BIT_KHR: return "mis";
            case VK_SHADER_STAGE_INTERSECTION_BIT_KHR: return "irs";
            case VK_SHADER_STAGE_CALLABLE_BIT_KHR:  return "cal";
            case VK_SHADER_STAGE_TASK_BIT_LW: return "tas";
            case VK_SHADER_STAGE_MESH_BIT_LW: return "mes";
            default: return "unk";
        }
    }
}
#endif

//--------------------------------------------------------------------
VkResult VulkanShader::CreateShader
(
    VkShaderStageFlagBits stageFlag,
    const string&         source,
    const string&         entryPointName,
    bool                  enableShaderReplacement,
    VkSpecializationInfo* pSpecializationInfo
)
{
    PROFILE_ZONE(VULKAN)

    MASSERT(m_ShaderStageInfo.module == VK_NULL_HANDLE);

    VkResult res = VK_SUCCESS;

    m_ShaderSource = source;
    m_EntryPoint = entryPointName;

#ifndef VULKAN_STANDALONE
    if (enableShaderReplacement)
    {
        const string fname = Utility::StrPrintf("vksh_%s_%08x.txt",
            VkShaderStageFlagBits2Prefix(stageFlag),
            Crc32c::Callwlate(source.c_str(), static_cast<UINT32>(source.size()), 1, 0));

        if (Xp::DoesFileExist(fname))
        {
            if (Xp::InteractiveFileRead(fname.c_str(), &m_ShaderSource) == OK)
            {
                Printf(Tee::PriLow, "Replaced shader with content from file %s .\n",
                    fname.c_str());
            }
            else
            {
                Printf(Tee::PriWarn, "Unable to read shader file %s .\n",
                    fname.c_str());
            }
        }
        else
        {
            Xp::InteractiveFile intFile;
            const RC rcOpen = intFile.Open(fname.c_str(), Xp::InteractiveFile::Create);
            if (rcOpen != RC::OK)
            {
                Printf(Tee::PriError, "Unable to create shader file %s , rc = %d\n",
                    fname.c_str(), rcOpen.Get());
                return VK_ERROR_UNKNOWN;
            }
            const RC rcWrite = intFile.Write(source);
            if (rcWrite == RC::OK)
            {
                Printf(Tee::PriLow, "Saved shader in %s file.\n", fname.c_str());
            }
            else
            {
                Printf(Tee::PriError, "Unable to write shader file %s , rc = %d\n",
                    fname.c_str(), rcWrite.Get());
                return VK_ERROR_UNKNOWN;
            }
        }
    }
#endif

    vector<UINT32> shaderSPIRVFormat;
    CHECK_VK_RESULT(GetShaderInSPIRVFormat(stageFlag, shaderSPIRVFormat));

    m_ShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    m_ShaderStageInfo.pNext = nullptr;
    m_ShaderStageInfo.pSpecializationInfo = pSpecializationInfo;
    m_ShaderStageInfo.flags = 0;
    m_ShaderStageInfo.stage = stageFlag;
    m_ShaderStageInfo.pName = m_EntryPoint.c_str();

    VkShaderModuleCreateInfo moduleCreateInfo;
    moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleCreateInfo.pNext = nullptr;
    moduleCreateInfo.flags = 0;
    moduleCreateInfo.codeSize = shaderSPIRVFormat.size() * sizeof(UINT32);
    moduleCreateInfo.pCode = shaderSPIRVFormat.data();
    return m_pVulkanDev->CreateShaderModule(&moduleCreateInfo, nullptr, &m_ShaderStageInfo.module);
}

//--------------------------------------------------------------------
void VulkanShader::Cleanup()
{
    if (m_ShaderStageInfo.module)
    {
        m_pVulkanDev->DestroyShaderModule(m_ShaderStageInfo.module);
        m_ShaderStageInfo.module = VK_NULL_HANDLE;
    }
}

//------------------------------------------------------------------------------------------------
// Scan the shader code for a "token = value" string and update pValue if found.
// Returns RC::ILWALID_INPUT on error.
RC VulkanShader::FindToken(const string &token, UINT32* pValue, string *pSource)
{
    MASSERT(pSource);
    string::size_type pos = pSource->find(token);
    if (pos != string::npos)
    {
        pos += token.length();

        pos = pSource->find_first_of("0123456789\n", pos);
        if (pos == string::npos || pSource->at(pos) == '\n')
        {
            return RC::ILWALID_INPUT;
        }
        string::size_type end = pSource->find_first_not_of("0123456789abcdefABCDEFxX", pos);
        if (end == string::npos || end > pSource->length())
        {
            return RC::ILWALID_INPUT;
        }
        // parse for any decimal or hex number.
        if (qi::parse(&(*pSource)[pos],
                      &(*pSource)[end],
                      (qi::no_case["0x"] >> qi::hex) | qi::uint_,
                      *pValue))
        {
            return OK;
        }
    }
    return RC::ILWALID_INPUT;
}

void VulkanShader::TakeResources(VulkanShader&& shader)
{
    if (this != &shader)
    {
        Cleanup();

        m_pVulkanDev      = shader.m_pVulkanDev;
        m_ShaderSource    = move(shader.m_ShaderSource);
        m_EntryPoint      = move(shader.m_EntryPoint);
        m_ShaderStageInfo = shader.m_ShaderStageInfo;

        shader.m_ShaderStageInfo.module = VK_NULL_HANDLE;
    }
}
