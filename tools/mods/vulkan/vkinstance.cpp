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

#include "vkinstance.h"
#include "vkcmdbuffer.h"
#include "vklayers.h"
#ifndef VULKAN_STANDALONE
#include "gpu/include/gpusbdev.h"
#endif

#include "glslang/Public/ShaderLang.h"

using namespace VkMods;

//--------------------------------------------------------------------
VkResult VulkanInstance::InitVulkan
(
    bool enableDebug
    ,UINT32 gpuIdx
    ,const vector<string>& extensionNames
    ,bool initializeDevice
    ,bool enableValidationLayers
    ,bool printExtensions
)
{
    PROFILE_ZONE(VULKAN)

    VkResult res = VK_SUCCESS;
    m_EnableDebug = enableDebug;
    m_EnableValidationLayers = enableValidationLayers;

    CHECK_VK_RESULT(VulkanLayers::Init(enableValidationLayers));
    CHECK_VK_RESULT(CreateInstance());
    CHECK_VK_RESULT(CreateDevice(gpuIdx, initializeDevice, extensionNames, printExtensions));

    // Initialize GLSL compiler
    MASSERT(!m_InitializedGlsl);
    glslang::InitializeProcess();
    m_InitializedGlsl = true;

    return res;
}

//--------------------------------------------------------------------
VkResult VulkanInstance::GetInstanceLayersAndExtensions()
{
    VkResult res = VK_SUCCESS;
    m_InstanceExtensions.clear();
    m_InstanceLayers.clear();

    //get global extensions available
    UINT32 instanceCount = 0;
    CHECK_VK_RESULT(VulkanLayers::EnumerateInstanceExtensionProperties(nullptr,
        &instanceCount, nullptr));
    vector<VkExtensionProperties> extensionProperties;
    extensionProperties.resize(instanceCount);
    CHECK_VK_RESULT(VulkanLayers::EnumerateInstanceExtensionProperties(nullptr,
        &instanceCount, extensionProperties.data()));

    bool debugExtensionFound = false;
    for (UINT32 i = 0; i < instanceCount; i++)
    {
        if (!strcmp(VK_EXT_DEBUG_REPORT_EXTENSION_NAME, extensionProperties[i].extensionName))
        {
            debugExtensionFound = true;
        }
        m_InstanceExtensions.push_back(extensionProperties[i].extensionName);
    }

    if (m_EnableDebug && !debugExtensionFound)
    {
        Printf(Tee::PriNormal, "VK Debugging requested, but extension unavailable.\n");
        m_EnableDebug = false;
    }

    //get all layers
    UINT32 layerCount = 0;
    CHECK_VK_RESULT(VulkanLayers::EnumerateInstanceLayerProperties(&layerCount, nullptr));

    vector<VkLayerProperties> layerProperties(layerCount);
    CHECK_VK_RESULT(VulkanLayers::EnumerateInstanceLayerProperties(&layerCount,
        layerProperties.data()));
    Printf(Tee::PriLow, GetTeeModuleCode(), "Found %d layers:\n", layerCount);

    for (UINT32 i = 0; i < layerCount; i++)
    {
        m_InstanceLayers.push_back(layerProperties[i].layerName);
    }

    //Remove layers/extensions that have caused problems ?
    //TODO: Provide an API to add to the list of layersThatCauseProblems
    vector<string> layersToDisable =
    {
        "VK_LAYER_LUNARG_vktrace"
        ,"VK_LAYER_RENDERDOC_Capture"
        ,"VK_LAYER_GOOGLE_unique_objects" //lots of device extensions not suppored by this layer
        ,"VK_LAYER_LUNARG_api_dump" //this one is just too verbose!
        ,"VK_LAYER_MESA_device_select"
        ,"VK_LAYER_MESA_overlay"
        ,"VK_LAYER_LW_nomad"
    };

    if (!m_EnableValidationLayers)
    {
        // We can't just disable all the layers because in the stand alone build
        // there is still "VK_LAYER_LW_nsight" that is needed for VS debug.
        layersToDisable.push_back("VK_LAYER_LUNARG_core_validation");
        layersToDisable.push_back("VK_LAYER_LUNARG_object_tracker");
        layersToDisable.push_back("VK_LAYER_LUNARG_parameter_validation");
        layersToDisable.push_back("VK_LAYER_LUNARG_swapchain");
        layersToDisable.push_back("VK_LAYER_GOOGLE_threading");
        layersToDisable.push_back("VK_LAYER_LUNARG_standard_validation");
        layersToDisable.push_back("VK_LAYER_KHRONOS_validation");
    }

    for (UINT32 i = 0; i < layersToDisable.size(); i++)
    {
        for (UINT32 j = 0; j < m_InstanceLayers.size(); j++)
        {
            if (m_InstanceLayers[j].find(layersToDisable[i]) != string::npos)
            {
                Printf(Tee::PriLow, GetTeeModuleCode(), " Removing Layer:%s\n",
                       m_InstanceLayers[j].c_str());
                m_InstanceLayers.erase(m_InstanceLayers.begin() + j);
                break;
            }
        }
    }

    //get per layer extensions
    for (const string& layer : m_InstanceLayers)
    {
        UINT32 layerInstanceCount = 0;
        CHECK_VK_RESULT(VulkanLayers::EnumerateInstanceExtensionProperties(
            layer.c_str(), &layerInstanceCount, nullptr));

        vector<VkExtensionProperties> layerExtentionPropertiesList(layerInstanceCount);
        CHECK_VK_RESULT(VulkanLayers::EnumerateInstanceExtensionProperties(
            layer.c_str(), &layerInstanceCount,
            layerExtentionPropertiesList.data()));

        const auto layerProp = find_if(layerProperties.begin(), layerProperties.end(),
                [&layer](const VkLayerProperties& lwrProp) -> bool
                {
                    return lwrProp.layerName == layer;
                });
        MASSERT(layerProp != layerProperties.end());
        Printf(Tee::PriLow, GetTeeModuleCode(), "Layer:%s (spec v.%i.%i.%i; impl v.%i.%i.%i)\n"
            , layer.c_str()
            , VK_VERSION_MAJOR(layerProp->specVersion)
            , VK_VERSION_MINOR(layerProp->specVersion)
            , VK_VERSION_PATCH(layerProp->specVersion)
            , VK_VERSION_MAJOR(layerProp->implementatiolwersion)
            , VK_VERSION_MINOR(layerProp->implementatiolwersion)
            , VK_VERSION_PATCH(layerProp->implementatiolwersion)
        );

        for (UINT32 j = 0; j < layerInstanceCount && layerExtentionPropertiesList.size(); j++)
        {
            Printf(Tee::PriLow, GetTeeModuleCode(), " Extension Properties:%s\n",
                   layerExtentionPropertiesList[j].extensionName);
            m_InstanceExtensions.push_back(layerExtentionPropertiesList[j].extensionName);
        }
    }

    return res;
}

//--------------------------------------------------------------------
namespace debugReport
{
    vector<debugReport::IgnoredMessage> IgnoredMessages;
    bool m_EnableErrorLogger = true;

    VKAPI_ATTR VkBool32 VKAPI_CALL
    vkDebugCallbackFunction
    (
        VkDebugReportFlagsEXT flags,
        VkDebugReportObjectTypeEXT objectType,
        uint64_t object,
        size_t location,
        INT32 messageCode,
        const char* pLayerPrefix,
        const char* pMessage,
        void* pUserData
    )
    {
        // The reporting format changed with Validation layers starting around version 1.1.101.0.
        // The new format will prepend additional strings like "Object:0x?? (Type=??) | "
        // to the original message. So search for the last "| " string to find the start of the
        // message. If "|" doesn't exist then use the entire message for comparisons.
        const char * pDbgMsg = strrchr(pMessage, '|');
        pDbgMsg = (pDbgMsg) ? pDbgMsg + 2 : pMessage;

        for (size_t ignoredMessageIdx = 0;
             ignoredMessageIdx < IgnoredMessages.size();
             ignoredMessageIdx++)
        {
            const auto &ignoredMessage = IgnoredMessages[ignoredMessageIdx];
            char lastChar;
            if ((objectType == ignoredMessage.objectType) &&
                (messageCode == ignoredMessage.messageCode) &&
                (strcmp(pLayerPrefix, ignoredMessage.pLayerPrefix) == 0) &&
                (sscanf(pDbgMsg, ignoredMessage.pMsgFmt, &lastChar) == 1) &&
                (lastChar == ignoredMessage.lastChar))
            {
                return 0;
            }
        }
        string header;

        if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
            header += "VK ERROR: ";
        if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
            header += "VK WARNING: ";
        if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
            header += "VK PERF WARNING: ";
        if (flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT)
            header += "VK INFO: ";
        if (flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT)
            header += "VK DEBUG: ";

        char message[4096];
        snprintf(message, sizeof(message),
            "%s Object type %u, messageCode %d, Layer: %s, message: %s\n",
            header.c_str(),
            objectType,
            messageCode,
            pLayerPrefix,
            pMessage);

        Printf(Tee::PriNormal, "%s", message);
        Tee::FlushToDisk();

        if ((flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) && m_EnableErrorLogger)
        {
            ErrorLogger::LogError(message, ErrorLogger::LT_ERROR);
        }

        return 0;
    }
    void EnableErrorLogger(bool bEnable)
    {
        m_EnableErrorLogger = bEnable;
    }
}

void VulkanInstance::ClearDebugMessagesToIgnore()
{
    debugReport::IgnoredMessages.clear();
}

void VulkanInstance::AddDebugMessageToIgnore(debugReport::IgnoredMessage dbgMsg)
{
    for (size_t ignoredMessageIdx = 0;
         ignoredMessageIdx < debugReport::IgnoredMessages.size();
         ignoredMessageIdx++)
    {
        const auto &ignoredMessage = debugReport::IgnoredMessages[ignoredMessageIdx];
        if ((dbgMsg.objectType == ignoredMessage.objectType) &&
            (dbgMsg.messageCode == ignoredMessage.messageCode) &&
            (strcmp(dbgMsg.pLayerPrefix, ignoredMessage.pLayerPrefix) == 0) &&
            (strcmp(dbgMsg.pMsgFmt, ignoredMessage.pMsgFmt) == 0) &&
            (dbgMsg.lastChar == ignoredMessage.lastChar))
        {
            return; // message already found.
        }
    }
    debugReport::IgnoredMessages.push_back(dbgMsg);
}
//--------------------------------------------------------------------
VkResult VulkanInstance::CreateInstance()
{
    VkResult res = VK_SUCCESS;
    CHECK_VK_RESULT(GetInstanceLayersAndExtensions());

    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "MODS Vulkan Test";
    appInfo.pEngineName = "MODS Vulkan Test";
    appInfo.apiVersion = VulkanLayers::EnumerateInstanceVersion();

    // If validation is enabled we need to disable the unique handle wrapping because we
    // have LWPU unique APIs that are not compatible with validation that are used for
    // direct display.
    VkValidationFeatureDisableEXT disabledFeatures[] =
    {
        VK_VALIDATION_FEATURE_DISABLE_UNIQUE_HANDLES_EXT
    };
    VkValidationFeaturesEXT validationFeatures = {};
    validationFeatures.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
    validationFeatures.pNext = nullptr;
    validationFeatures.enabledValidationFeatureCount = 0;
    validationFeatures.pEnabledValidationFeatures = nullptr;
    validationFeatures.disabledValidationFeatureCount =
        static_cast<UINT32>(NUMELEMS(disabledFeatures));
    validationFeatures.pDisabledValidationFeatures = disabledFeatures;

    VkInstanceCreateInfo instanceCreateInfo = {};
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pNext = &validationFeatures;
    instanceCreateInfo.pApplicationInfo = &appInfo;
    instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(m_InstanceExtensions.size());
    vector<const char*> extensionNames(m_InstanceExtensions.size());
    for (UINT32 i = 0; i < m_InstanceExtensions.size(); i++)
    {
        extensionNames[i] = m_InstanceExtensions[i].data();
    }
    instanceCreateInfo.ppEnabledExtensionNames = extensionNames.data();
    instanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(m_InstanceLayers.size());
    vector<const char*> layerNames(m_InstanceLayers.size());
    for (UINT32 i = 0; i < m_InstanceLayers.size(); i++)
    {
        layerNames[i] = m_InstanceLayers[i].c_str();
    }
    instanceCreateInfo.ppEnabledLayerNames = layerNames.data();
    res = VulkanLayers::CreateInstance(&instanceCreateInfo, nullptr, &m_VkInstance, this);

#ifdef VULKAN_STANDALONE
    // retry without non debug extensions
    if (res == VK_ERROR_EXTENSION_NOT_PRESENT || res == VK_ERROR_LAYER_NOT_PRESENT)
    {
        Printf(Tee::PriWarn, "Can not create Vulkan instance, with extensions or layers, trying without.\n");
        const char *dbgExtension = VK_EXT_DEBUG_REPORT_EXTENSION_NAME;
        instanceCreateInfo.enabledExtensionCount = m_EnableDebug ? 1 : 0;
        instanceCreateInfo.ppEnabledExtensionNames = m_EnableDebug ? &dbgExtension : nullptr;
        instanceCreateInfo.enabledLayerCount = 0;
        instanceCreateInfo.ppEnabledLayerNames = nullptr;
        CHECK_VK_RESULT(VulkanLayers::CreateInstance(&instanceCreateInfo,
            nullptr, &m_VkInstance, this));
    }
#endif

    if (res == VK_SUCCESS && m_EnableDebug)
    {
        m_DbgCreateDebugReportCallback =
            reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(
            VkLayerInstanceDispatchTable::GetInstanceProcAddr(m_VkInstance,
                "vkCreateDebugReportCallbackEXT"));
        m_DbgDestroyDebugReportCallback =
            reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(
            VkLayerInstanceDispatchTable::GetInstanceProcAddr(m_VkInstance,
                "vkDestroyDebugReportCallbackEXT"));

        if (!m_DbgCreateDebugReportCallback || !m_DbgDestroyDebugReportCallback)
        {
            m_DbgCreateDebugReportCallback = nullptr;
            m_DbgDestroyDebugReportCallback = nullptr;
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }

        VkDebugReportCallbackCreateInfoEXT dbgCreateInfo = {};
        dbgCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
        dbgCreateInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT |
            VK_DEBUG_REPORT_WARNING_BIT_EXT |
//            VK_DEBUG_REPORT_INFORMATION_BIT_EXT | VK_DEBUG_REPORT_DEBUG_BIT_EXT |
            VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;

        dbgCreateInfo.pfnCallback = debugReport::vkDebugCallbackFunction;
        dbgCreateInfo.pUserData = nullptr;
        CHECK_VK_RESULT(m_DbgCreateDebugReportCallback(m_VkInstance,
            &dbgCreateInfo, nullptr, &m_DebugReportCallback));
    }

    return res;
}

//--------------------------------------------------------------------
VkResult VulkanInstance::CreateDevice
(
    UINT32 gpuIdx
    ,bool initializeDevice
    ,const vector <string>& extensionNames
    ,bool printExtensions
)
{
    UINT32 numGpus = 0;
    VkResult res = VK_SUCCESS;

    CHECK_VK_RESULT(VkLayerInstanceDispatchTable::EnumeratePhysicalDevices(m_VkInstance,
        &numGpus, nullptr));
    vector<VkPhysicalDevice> physicalDevices(numGpus);
    if (numGpus == 0)
    {
        Printf(Tee::PriError, "No GPUs detected.\n");
        return VK_ERROR_DEVICE_LOST;
    }

    CHECK_VK_RESULT(VkLayerInstanceDispatchTable::EnumeratePhysicalDevices(m_VkInstance,
        &numGpus, physicalDevices.data()));

    // This sorts VkPhysicalDevices by their PCIE address similar to MODS
    auto comparator = [](const VulkanDevice::PexAddr& addr1,
                         const VulkanDevice::PexAddr& addr2) -> bool
    {
        if (addr1.domain < addr2.domain)
            return true;
        if (addr1.bus < addr2.bus)
            return true;
        if (addr1.device < addr2.device)
            return true;
        if (addr1.function < addr2.function)
            return true;

        return false;
    };
    map<VulkanDevice::PexAddr, UINT32, decltype(comparator)> physDevPexAddrs(comparator);

#ifndef VULKAN_STANDALONE
    // Insert all Vulkan-capable LWPU GPUs into a map that is sorted by PCIE address
    for (UINT32 physDevIdx = 0; physDevIdx < numGpus; physDevIdx++)
    {
        auto pVkDev = make_unique<VulkanDevice>(physicalDevices[physDevIdx], this, false);
        if (pVkDev->HasExt(VulkanDevice::ExtVK_EXT_pci_bus_info))
        {
            VkPhysicalDevicePCIBusInfoPropertiesEXT pexProps =
                { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PCI_BUS_INFO_PROPERTIES_EXT };
            VkPhysicalDeviceProperties2 properties2 =
                { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &pexProps };
            VkLayerInstanceDispatchTable::GetPhysicalDeviceProperties2(
                physicalDevices[physDevIdx], &properties2);

            if (properties2.properties.vendorID == 0x10DE)
            {
                VulkanDevice::PexAddr pexAddr =
                {
                    pexProps.pciDomain,
                    pexProps.pciBus,
                    pexProps.pciDevice,
                    pexProps.pciFunction
                };
                physDevPexAddrs[pexAddr] = physDevIdx;
            }
        }
    }
#endif

    if (!physDevPexAddrs.empty())
    {
        if (static_cast<UINT64>(gpuIdx) >= physDevPexAddrs.size())
        {
            Printf(Tee::PriError, "Invalid GPU index - %u\n", gpuIdx);
            return VK_ERROR_INITIALIZATION_FAILED;
        }
        UINT32 gpu = 0;
        for (const auto& physDevPexAddr : physDevPexAddrs)
        {
            if (gpu == gpuIdx)
            {
                m_VulkanDevice = make_unique<VulkanDevice>(
                    physicalDevices[physDevPexAddr.second], this, printExtensions);
                break;
            }
            gpu++;
        }
    }
    else if (gpuIdx >= numGpus)
    {
        Printf(Tee::PriError, "Invalid GPU index - %u\n", gpuIdx);
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    else
    {
#ifdef VULKAN_STANDALONE
        VkPhysicalDeviceProperties2 props = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
        VkLayerInstanceDispatchTable::GetPhysicalDeviceProperties2(physicalDevices[gpuIdx], &props);

        Printf(Tee::PriLow, "Using device %u vendor 0x%04x: %s\n",
               gpuIdx,
               props.properties.vendorID,
               props.properties.deviceName);
#endif

        m_VulkanDevice = make_unique<VulkanDevice>(physicalDevices[gpuIdx], this, printExtensions);
    }
    MASSERT(m_VulkanDevice.get());

    if (initializeDevice)
    {
#ifdef ENABLE_UNIT_TESTING
        CHECK_VK_RESULT(m_VulkanDevice->Initialize(m_UnitTestingVariantIndex));
#else
        CHECK_VK_RESULT(m_VulkanDevice->Initialize(extensionNames, printExtensions));
#endif
    }

    return res;
}

//--------------------------------------------------------------------
void VulkanInstance::DestroyInstance()
{
    if (m_VkInstance)
    {
        if (m_EnableDebug && m_DbgDestroyDebugReportCallback)
        {
            m_DbgDestroyDebugReportCallback(m_VkInstance, m_DebugReportCallback, nullptr);
        }
        VkLayerInstanceDispatchTable::DestroyInstance(m_VkInstance, nullptr);
        m_VkInstance = nullptr;
    }
}

//--------------------------------------------------------------------
void VulkanInstance::DestroyVulkan()
{
    // Shut down GLSL compiler
    if (m_InitializedGlsl)
    {
        glslang::FinalizeProcess();
        m_InitializedGlsl = false;
    }

    // TODO: Make VulkanInstance RAII managed so we don't have to explicitly
    // free m_VulkanDevice before destroying the instance
    m_VulkanDevice.reset(nullptr);
    DestroyInstance();
}

//--------------------------------------------------------------------
VulkanInstance::~VulkanInstance()
{
    DestroyVulkan();
}

