/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#ifndef INCLUDED_LWL_DEVIF_H
#define INCLUDED_LWL_DEVIF_H

#include "core/include/rc.h"
#include "gpu/include/testdevice.h"
#include "lwl_lwlink_libif.h"
#include <vector>
#include <memory>

namespace LwLinkDevIf
{
    enum TopologyMatch
    {
        TM_DEFAULT
       ,TM_MIN  // Match as much of the topology as possible
                // and disregard any links/devices that fail to match
       ,TM_FILE // Match as much of the topology as possible using all available links/devices,
                // even if they don't actually detect a connection.
    };

    enum HwDetectMethod
    {
        HWD_NONE
       ,HWD_EXPLORER_NORMAL
       ,HWD_EXPLORER_ILWERT
    };

    enum LinkConfig
    {
        LC_NONE
       ,LC_MANUAL
       ,LC_AUTO
    };

    enum TopologyFlag
    {
        TF_NONE                 = 0x0
       ,TF_TREX                 = (1 << 0)
       ,TF_DISABLE_AUTO_MAPPING = (1 << 1)
    };

    //--------------------------------------------------------------------------
    //! \brief Interface definition for interacting with a LwLink device
    //! interface.  Each type of LwLink device has its own interface since each
    //! type of device has a different type of low level implementation (e.g.
    //! ebridge devices need to communicate with an ebridge library, gpu devices
    //! need to communicate with GpuSubdevice classes)
    class Interface
    {
        public:
            virtual ~Interface() { };
            virtual bool DevicesInitialized() = 0;
            virtual RC FindDevices(vector<TestDevicePtr> *pDevices) = 0;
            virtual bool FoundDevices() = 0;
            virtual RC InitializeAllDevices() = 0;
            virtual RC Shutdown() = 0;

    };

    TeeModule * GetTeeModule();
    LwLinkLibIfPtr GetLwLinkLibIf();
    LibIfPtr GetTegraLibIf();
    RC Initialize();
    RC Shutdown();

    typedef shared_ptr<Interface> InterfacePtr;
};

#endif
