/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#ifndef INCLUDED_LWL_LWSWITCH_LIBIF_USER_H
#define INCLUDED_LWL_LWSWITCH_LIBIF_USER_H

#include "lwl_libif.h"

namespace LwLinkDevIf
{
    //--------------------------------------------------------------------------
    //! \brief LwSwitch library interface definition for interacting with the
    //! LwSwitch library when it is compiled directly into MODS
    class LwSwitchLibIfUser : public LibInterface
    {
    public:
        LwSwitchLibIfUser() : m_bInitialized(false) {}
        virtual ~LwSwitchLibIfUser();
    private:
        virtual RC Control
        (
            LibDeviceHandle  handle,
            LibControlId     controlId,
            void *           pParams,
            UINT32           paramSize
        );
        virtual RC FindDevices(vector<LibDeviceHandle> *pDevHandles);
        virtual RC GetBarInfo
        (
            LibDeviceHandle handle,
            UINT32          barNum,
            UINT64         *pBarAddr,
            UINT64         *pBarSize,
            void          **ppBar0
        );
        virtual RC GetLinkMask(LibDeviceHandle handle, UINT64 *pLinkMask);
        virtual RC GetPciInfo
        (
            LibDeviceHandle handle,
            UINT32         *pDomain,
            UINT32         *pBus,
            UINT32         *pDev,
            UINT32         *pFunc
        );
        virtual RC Initialize();
        virtual RC InitializeAllDevices();
        virtual RC InitializeDevice(LibDeviceHandle handle)  { return RC::UNSUPPORTED_FUNCTION; }
        virtual bool IsInitialized() { return m_bInitialized; }
        virtual RC Shutdown();

        bool m_bInitialized;
    };
};

#endif
