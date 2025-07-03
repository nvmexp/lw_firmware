/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "vkerror.h"

//--------------------------------------------------------------------
RC VkUtil::VkErrorToRC(VkResult res)
{
    switch (res)
    {
        case VK_SUCCESS:                        return OK;
        case VK_NOT_READY:                      return RC::MODS_VK_NOT_READY;
        case VK_TIMEOUT:                        return RC::MODS_VK_TIMEOUT;
        case VK_EVENT_SET:                      return RC::MODS_VK_EVENT_SET;
        case VK_EVENT_RESET:                    return RC::MODS_VK_EVENT_RESET;
        case VK_INCOMPLETE:                     return RC::MODS_VK_INCOMPLETE;
        case VK_ERROR_OUT_OF_HOST_MEMORY:       return RC::MODS_VK_ERROR_OUT_OF_HOST_MEMORY;
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:     return RC::MODS_VK_ERROR_OUT_OF_DEVICE_MEMORY;
        case VK_ERROR_INITIALIZATION_FAILED:    return RC::MODS_VK_ERROR_INITIALIZATION_FAILED;
        case VK_ERROR_DEVICE_LOST:              return RC::MODS_VK_ERROR_DEVICE_LOST;
        case VK_ERROR_MEMORY_MAP_FAILED:        return RC::MODS_VK_ERROR_MEMORY_MAP_FAILED;
        case VK_ERROR_LAYER_NOT_PRESENT:        return RC::MODS_VK_ERROR_LAYER_NOT_PRESENT;
        case VK_ERROR_EXTENSION_NOT_PRESENT:    return RC::MODS_VK_ERROR_EXTENSION_NOT_PRESENT;
        case VK_ERROR_FEATURE_NOT_PRESENT:      return RC::MODS_VK_ERROR_FEATURE_NOT_PRESENT;
        case VK_ERROR_INCOMPATIBLE_DRIVER:      return RC::MODS_VK_ERROR_INCOMPATIBLE_DRIVER;
        case VK_ERROR_TOO_MANY_OBJECTS:         return RC::MODS_VK_ERROR_TOO_MANY_OBJECTS;
        case VK_ERROR_FORMAT_NOT_SUPPORTED:     return RC::MODS_VK_ERROR_FORMAT_NOT_SUPPORTED;
        case VK_ERROR_FRAGMENTED_POOL:          return RC::MODS_VK_ERROR_FRAGMENTED_POOL;
        case VK_ERROR_SURFACE_LOST_KHR:         return RC::MODS_VK_ERROR_SURFACE_LOST_KHR;
        case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return RC::MODS_VK_ERROR_NATIVE_WINDOW_IN_USE_KHR;
        case VK_SUBOPTIMAL_KHR:                 return RC::MODS_VK_SUBOPTIMAL_KHR;
        case VK_ERROR_OUT_OF_DATE_KHR:          return RC::MODS_VK_ERROR_OUT_OF_DATE_KHR;
        case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return RC::MODS_VK_ERROR_INCOMPATIBLE_DISPLAY_KHR;
        case VK_ERROR_VALIDATION_FAILED_EXT:    return RC::MODS_VK_ERROR_VALIDATION_FAILED_EXT;
        case VK_ERROR_ILWALID_SHADER_LW:        return RC::MODS_VK_ERROR_ILWALID_SHADER_LW;
        case VK_ERROR_OUT_OF_POOL_MEMORY_KHR:   return RC::MODS_VK_ERROR_OUT_OF_POOL_MEMORY_KHR;
        case VK_ERROR_ILWALID_EXTERNAL_HANDLE_KHR:  return RC::MODS_VK_ERROR_ILWALID_EXTERNAL_HANDLE_KHR; //$
    default:                                    return RC::MODS_VK_GENERIC_ERROR;
    }
}