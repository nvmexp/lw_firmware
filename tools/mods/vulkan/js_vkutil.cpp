/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017, 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------

// Mods Vulkan utilities.
#include "vulkan/vkdev.h"
#include "core/include/tee.h"
#include "core/include/jscript.h"
#include "core/include/script.h"

C_(EnableVkExtension);
static STest S_EnableVkExtension
(
    "EnableVkExtension",
    C_EnableVkExtension,
    1,
    "Force mods Vulkan tests to enable this extension when creating the VkDevice"
);
C_(EnableVkExtension)
{
    JavaScriptPtr pJavaScript;
    string extension;

    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &extension)))
    {
        pJavaScript->Throw(pContext, RC::BAD_PARAMETER,
                           "Usage: EnableVkExtension(\"VK_LW_whatever\")");
        return JS_FALSE;
    }
    VulkanDevice::EnableExt(extension);

    RETURN_RC(OK);
}

C_(DisableVkExtension);
static STest S_DisableVkExtension
(
    "DisableVkExtension",
    C_DisableVkExtension,
    1,
    "Prevent mods Vulkan tests to from enabling this extension when creating the VkDevice"
);
C_(DisableVkExtension)
{
    JavaScriptPtr pJavaScript;
    string extension;

    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &extension)))
    {
        pJavaScript->Throw(pContext, RC::BAD_PARAMETER,
                           "Usage: DisableVkExtension(\"VK_LW_whatever\")");
        return JS_FALSE;
    }
    VulkanDevice::DisableExt(extension);

    RETURN_RC(OK);
}

