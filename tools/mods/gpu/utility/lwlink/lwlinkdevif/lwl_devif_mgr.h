/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#ifndef INCLUDED_LWL_DEVIF_MGR_H
#define INCLUDED_LWL_DEVIF_MGR_H

#include "core/include/rc.h"
#include "gpu/include/testdevice.h"
#include <vector>

namespace LwLinkDevIf
{
    //--------------------------------------------------------------------------
    //! \brief Namespace that manages interaction to all of the LwLink device
    //! interfaces
    namespace Manager
    {
        bool AllDevicesFound();
        bool AllDevicesInitialized();
        bool AllDevicesShutdown();
        RC FindDevices(vector<TestDevicePtr> *pDevices);
        bool GetIgnoreLwSwitch();
        bool GetIgnoreIbmNpu();
        bool GetIgnoreTegra();
        UINT32 GetNumSimEbridge();
        UINT32 GetNumSimGpu();
        UINT32 GetNumSimIbmNpu();
        UINT32 GetNumSimLwSwitch();
        UINT32 GetLwSwitchPrintLevel();
        Device::LwDeviceId GetSimLwSwitchType();
        RC InitializeAllDevices();
        RC Shutdown();
        RC SetIgnoreLwSwitch(bool bIgnore);
        RC SetIgnoreIbmNpu(bool bIgnore);
        RC SetIgnoreTegra(bool bIgnore);
        RC SetNumSimEbridge(UINT32 num);
        RC SetNumSimGpu(UINT32 num);
        RC SetNumSimIbmNpu(UINT32 num);
        RC SetNumSimLwSwitch(UINT32 num);
        RC SetLwSwitchPrintLevel(UINT32 printLevel);
        RC SetSimLwSwitchType(Device::LwDeviceId type);
    };
};

#endif
