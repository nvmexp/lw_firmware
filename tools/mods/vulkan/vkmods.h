/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2017 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/
#pragma once
#ifndef VKMODS_H
#define VKMODS_H

#ifdef VULKAN_STANDALONE

namespace VkMods
{
    UINT32 GetTeeModuleCode();
}

#else

#include "core/include/tee.h"

//-------------------------------------------------------------------------------------------------
// Create a specific namespace for the Vulkan Mods print module. In Vulkan code use
// Printf(Tee::Pri*, VkMods::GetTeeModule(), "Your format string"); and then to filter all prints
// except the VkMods module add the following command line args:
// "-message_disable ModsCore:ModsDrv -verbose"
namespace VkMods
{
    TeeModule * GetTeeModule();
    UINT32 GetTeeModuleCode();
}
#endif

#endif

