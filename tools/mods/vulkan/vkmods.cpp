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
#include "vkmain.h"

#ifdef VULKAN_STANDALONE

UINT32 VkMods::GetTeeModuleCode()
{
    return 0;
}

#else

namespace VkMods
{
    //! Tee module for printing within the VkUtil namespace. To print only these messages use the
    //! following command lines. "-message_disable ModsCore:ModsDrv -verbose"
    TeeModule s_VkMods("VkMods");
};

TeeModule * VkMods::GetTeeModule()
{
    return &s_VkMods;
}

UINT32 VkMods::GetTeeModuleCode()
{
    return s_VkMods.GetCode();
}

#endif
