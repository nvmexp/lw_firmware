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
#ifndef VK_SHADER_H
#define VK_SHADER_H

#include "vkmain.h"
#include "glslang/Public/ShaderLang.h"  // Support for SPIR-V shader code
#include "SPIRV/GlslangToSpv.h"         // Support for SPIR-V shader code

class VulkanDevice;

//--------------------------------------------------------------------
class VulkanShader
{
public:
    VulkanShader() = default;
    explicit VulkanShader(VulkanDevice* pVulkanDev) : m_pVulkanDev(pVulkanDev) { }
    VulkanShader(const VulkanShader& shader) = delete;
    VulkanShader& operator=(const VulkanShader& shader) = delete;
    VulkanShader(VulkanShader&& shader)
    {
        TakeResources(move(shader));
    }
    VulkanShader& operator=(VulkanShader&& shader)
    {
        TakeResources(move(shader));
        return *this;
    }
    ~VulkanShader() { Cleanup(); }

    VkResult CreateShader
    (
        VkShaderStageFlagBits stageFlag,
        const string&         source,
        const string&         entryPointName,
        bool                  enableShaderReplacement,
        VkSpecializationInfo* pSpecializationInfo = nullptr
    );
    VkResult GetShaderInSPIRVFormat
    (
        VkShaderStageFlagBits stageFlag,
        vector <UINT32> &shaderSPIRVFormat
    );
    void Cleanup();

    // Parse the shader for a specific token and update pValue with its value.
    static RC FindToken(const string &token, UINT32* pValue, string *pShaderSource = nullptr);
    GET_PROP(ShaderStageInfo, VkPipelineShaderStageCreateInfo);
    string * GetShaderSource() { return &m_ShaderSource; }
    VkResult GlslToSpirv
    (
        VkShaderStageFlagBits shader_type,
        const char *pshader,
        std::vector<unsigned int> &spirv
    );
    EShLanguage FindLanguage
    (
        const VkShaderStageFlagBits shader_type
    );
    void InitResources();

private:
    VulkanDevice *m_pVulkanDev = nullptr;
    string       m_ShaderSource;
    string       m_EntryPoint;
    VkPipelineShaderStageCreateInfo m_ShaderStageInfo = {};

    static map<VkPhysicalDevice, TBuiltInResource> s_Resources;    // Support for SPIR-V shader code

    void PrintShader(const char * pShader);
    void TakeResources(VulkanShader&& shader);
};

#endif
