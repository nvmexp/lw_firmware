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

//! \file vdmftb.h -- concrete implementation for Functional Test Board
//                    using VDM through ppc driver

#pragma once

#include "gpu/usb/functionaltestboard/functionaltestboard.h"

class VdmFtb : public FunctionalTestBoard
{
public:
    VdmFtb(UINT32 domain, UINT32 bus, UINT32 device, UINT32 function);
    virtual ~VdmFtb();

protected:
    RC DoInitialize() override;
    RC DoSetAltMode(PortPolicyCtrl::UsbAltModes altMode) override;
    RC DoSetPowerMode(PortPolicyCtrl::UsbPowerModes powerMode) override;
    RC DoSetOrientation(PortPolicyCtrl::CableOrientation orient, UINT16 orientDelay) override;
    RC DoReverseOrientation() override;
    RC DoGenerateCableAttachDetach
    (
        PortPolicyCtrl::CableAttachDetach eventType,
        UINT16 detachAttachDelay
    ) override;
    RC DoReset() override;
    RC DoGenerateInBandWakeup(UINT16 inBandDelay) override;
    RC DoSetDpLoopback(PortPolicyCtrl::DpLoopback dpLoopback) override;
    RC DoSetIsocDevice(PortPolicyCtrl::IsocDevice enableIsocDev) override;
    RC DoGetLwrrUsbAltMode(PortPolicyCtrl::UsbAltModes* pAltMode) override;
    RC DoConfirmUsbAltModeConfig(PortPolicyCtrl::UsbAltModes altMode) override;
    RC DoGetPpcFwVersion
    (
        string *pPrimaryVersion,
        string *pSecondaryVersion
    ) override;
    RC DoGetPpcDriverVersion(string *pDriverVersion) override;
    RC DoGetFtbFwVersion(string *pFtbFwVersion) override;
    RC DoGetPowerDrawn(FLOAT64 *pVoltageV, FLOAT64 *pLwrrentA) override;

private:
    RC ConfigureAltMode(PortPolicyCtrl::UsbAltModes altMode, UINT32 *pMsgHeader, UINT32 *pMsgBody);
    RC ConfigurePowerMode
    (
        PortPolicyCtrl::UsbPowerModes powerMode,
        UINT32 *pMsgHeader,
        UINT32 *pMsgBody
    );
    RC ConfigureOrientation
    (
        UINT16 delay,
        PortPolicyCtrl::CableOrientation orient,
        UINT32 *pMsgHeader,
        UINT32 *pMsgBody
    );
    RC ConfigureReverseOrientation
    (
        UINT32 *pMsgHeader,
        UINT32 *pMsgBody
    );
    RC ConfigureAttachDetach
    (
        UINT16 delay,
        PortPolicyCtrl::CableAttachDetach eventType,
        UINT32 *pMsgHeader,
        UINT32 *pMsgBody
    );
    RC ConfigureInBandWakeup(UINT16 delay, UINT32 *pMsgHeader, UINT32 *pMsgBody);
    RC ConfigureDpLoopback
    (
        PortPolicyCtrl::DpLoopback dpLoopback,
        UINT32 *pMsgHeader,
        UINT32 *pMsgBody
    );
    RC ConfigureIsocDevice
    (
        PortPolicyCtrl::IsocDevice isocDev,
        UINT32 *pMsgHeader,
        UINT32 *pMsgBody
    );
    RC ConfigureResetFtb(UINT32 *pMsgHeader, UINT32 *pMsgBody);
    RC ConfigureGetFtbFwVersion(UINT32 *pMsgHeader, UINT32 *pMsgBody);
    RC SendMessage
    (
        vector<tuple<UINT32,
        UINT32>> *pVdms,
        PortPolicyCtrl::AddCableDetach addDetachCableVdm
    );

    UINT32        m_Domain          = ~0U;
    UINT32        m_Bus             = ~0U;
    UINT32        m_Device          = ~0U;
    UINT32        m_Function        = ~0U;
};

