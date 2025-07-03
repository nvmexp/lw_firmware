/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "lwl_libif.h"

#include <vector>

extern "C"
{
    #include "lwswitch_user_api.h"
}

namespace LwLinkDevIf
{
    //--------------------------------------------------------------------------
    //! \brief LwSwitch library interface definition for interacting with the
    //! LwSwitch library when it is compiled directly into MODS
    class LwSwitchLibIfKernel : public LibInterface
    {
    public:
        LwSwitchLibIfKernel() = default;
        virtual ~LwSwitchLibIfKernel();
    private:
        RC Control
        (
            LibDeviceHandle  handle,
            LibControlId     controlId,
            void *           pParams,
            UINT32           paramSize
        ) override;
        RC FindDevices(vector<LibDeviceHandle> *pDevHandles) override;
        RC GetBarInfo
        (
            LibDeviceHandle handle,
            UINT32          barNum,
            UINT64         *pBarAddr,
            UINT64         *pBarSize,
            void          **ppBar0
        ) override;
        RC GetLinkMask(LibDeviceHandle handle, UINT64 *pLinkMask) override;
        RC GetPciInfo
        (
            LibDeviceHandle handle,
            UINT32         *pDomain,
            UINT32         *pBus,
            UINT32         *pDev,
            UINT32         *pFunc
        ) override;
        RC Initialize() override;
        RC InitializeAllDevices() override;
        RC InitializeDevice(LibDeviceHandle handle) override { return RC::UNSUPPORTED_FUNCTION; }
        bool IsInitialized() override { return m_bInitialized; }
        RC Shutdown() override;

    private:
        bool m_bInitialized = false;
        vector<lwswitch_device*> m_Devices;
        
        RC GetLwSwitchDeviceInfo(LibDeviceHandle handle, lwswitch_device** ppDevice);
    };
};
