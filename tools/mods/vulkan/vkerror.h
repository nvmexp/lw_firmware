/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#pragma once

#include "core/include/rc.h"
#ifndef SKIP_VULKANH
    #ifdef VULKAN_STANDALONE
        #include "vulkan/vulkan.h"
    #else
        #include "vulkan/vulkanlw.h"
    #endif
#endif

//--------------------------------------------------------------------
namespace VkUtil
{
    RC VkErrorToRC(VkResult res);
};

#define CHECK_VK_TO_RC(func) CHECK_RC(VkUtil::VkErrorToRC(func))