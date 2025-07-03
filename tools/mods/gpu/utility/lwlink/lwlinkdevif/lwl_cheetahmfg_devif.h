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

#pragma once

#include "core/include/rc.h"
#include "lwl_devif.h"
#include "gpu/include/testdevice.h"
#include <vector>

namespace LwLinkDevIf
{
    //--------------------------------------------------------------------------
    //! \brief Interface definition for interacting with LwSwitch LwLink devices
    class TegraMfgDevInterface : public Interface
    {
    public:
        TegraMfgDevInterface() = default;
        virtual ~TegraMfgDevInterface() = default;
    private:
        // Make all public virtual functions from the base class private
        // in order to forbid external access to this class except through
        // the base class pointer
        virtual bool DevicesInitialized() { return m_bDevicesInitialized; }
        virtual RC FindDevices(vector<TestDevicePtr> *pDevices);
        virtual bool FoundDevices() { return m_bFoundDevices; }
        virtual RC InitializeAllDevices();
        virtual RC Shutdown();

        vector<TestDevicePtr> m_Devices;     //!< List of LwSwitch devices
        bool m_bFoundDevices       = false;  //!< True if devices have been found
        bool m_bDevicesInitialized = false;  //!< True if devices have been initialized
    };
};
