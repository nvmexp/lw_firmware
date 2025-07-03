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
#ifndef VK_INSTANCE_H
#define VK_INSTANCE_H

#include "vkmain.h"
#include "vkdev.h"
namespace debugReport
{
    //Note: pMsgFmt is the format string used in a sscanf() call where only the last character in
    //      the string will be stored and compared against the lastChar variable. All other format
    //      specifiers should be using the %*x to bypass any unknown value to decode. For example
    //      the string:
    // "Queue 0x7fffc8c04730 is signaling semaphore 0x7fffc94dbd10 that was previously signaled."
    // should use this format string
    // "Queue %*x is signaling semaphore %*x that was previously signaled%c" and the lastChar
    // should be '.'
    struct IgnoredMessage
    {
        VkDebugReportObjectTypeEXT objectType;
        INT32 messageCode;
        const char *pLayerPrefix;
        const char *pMsgFmt;    // the format string for the message to decode with %c at the end
                                // to decode and store the last char in the string.
        const char lastChar;    // the expected last character in the string to ignore.
        IgnoredMessage(
            VkDebugReportObjectTypeEXT ot
            ,INT32 mc
            ,const char * pLP
            ,const char * pFmt
            ,const char ch)
        : objectType(ot), messageCode(mc), pLayerPrefix(pLP), pMsgFmt(pFmt), lastChar(ch){}
    };
    void EnableErrorLogger(bool bEnable);
};

//--------------------------------------------------------------------
class VulkanInstance : public VkLayerInstanceDispatchTable
{
public:
    VulkanInstance() = default;
#ifdef ENABLE_UNIT_TESTING
    VulkanInstance(UINT32 unitTestingVariantIndex);
#endif
    ~VulkanInstance();

    VkResult InitVulkan
    (
         bool enableDebug
        ,UINT32 gpuIdx
        ,const vector<string>& extensionNames
        ,bool initializeDevice = true
        ,bool enableValidationLayers = false
        ,bool printExtensions = false
    );

    GET_PROP(VkInstance, VkInstance);
    GET_PROP(EnableDebug, bool);
    VulkanDevice* GetVulkanDevice() const { return m_VulkanDevice.get(); }
    static void AddDebugMessageToIgnore(debugReport::IgnoredMessage dbgMsg);
    static void ClearDebugMessagesToIgnore();
    void       DestroyVulkan();

private:
    VkInstance                       m_VkInstance = nullptr;

    // TODO: Support multiple VulkanDevices
    unique_ptr<VulkanDevice>         m_VulkanDevice = nullptr;

    bool                             m_EnableDebug = false;
    bool                             m_EnableValidationLayers = false;
    bool                             m_InitializedGlsl = false;

    PFN_vkCreateDebugReportCallbackEXT  m_DbgCreateDebugReportCallback = nullptr;
    PFN_vkDestroyDebugReportCallbackEXT m_DbgDestroyDebugReportCallback = nullptr;

#ifdef ENABLE_UNIT_TESTING
    UINT32                           m_UnitTestingVariantIndex = 0;
#endif

    vector<string>                   m_InstanceExtensions;
    vector<string>                   m_InstanceLayers;
    VkDebugReportCallbackEXT         m_DebugReportCallback = 0;

    VkResult   GetInstanceLayersAndExtensions();

    VkResult   CreateInstance();
    void       DestroyInstance();

    VkResult   CreateDevice
    (
        UINT32 gpuIdx
        ,bool initializeDevice
        ,const vector<string>& extensionNames
        ,bool printExtensions
    );

};

#endif
