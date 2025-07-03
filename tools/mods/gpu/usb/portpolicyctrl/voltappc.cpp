/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/tasker.h"
#include "gpu/usb/portpolicyctrl/voltappc.h"

//-----------------------------------------------------------------------------
/* virtual */ RC VoltaPPC::DoGetPciDBDF
(
    UINT32 *pDomain,
    UINT32 *pBus,
    UINT32 *pDevice,
    UINT32 *pFunction
) const
{
    RC rc;

    // These values are hard coded by the PPC driver. MODS will only
    // use this these values to find the correct port directory
    if (pDomain)
    {
        *pDomain = 0xffff;
    }
    if (pBus)
    {
        *pBus = 0xff;
    }
    if (pDevice)
    {
        *pDevice = 0x1f;
    }
    if (pFunction)
    {
        *pFunction = 0x07;
    }

    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ RC VoltaPPC::DoWaitForConfigToComplete() const
{
    JavaScriptPtr pJs;
    UINT32 configWaitTimeMs = USB_PPC_CONFIG_WAIT_TIME_MS;
    pJs->GetProperty(pJs->GetGlobalObject(), "g_UsbPpcConfigWaitTimeMs", &configWaitTimeMs);
    Tasker::Sleep(configWaitTimeMs);
    return OK;
}
