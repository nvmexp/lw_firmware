/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! \file functionaltestboard.h -- Usb Functional Test Board interface definition

#pragma once

#include "device/interface/portpolicyctrl.h"

class FunctionalTestBoard
{
public:
    virtual ~FunctionalTestBoard() {};

    RC Initialize()
        { return DoInitialize(); }
    RC SetAltMode(PortPolicyCtrl::UsbAltModes altMode)
        { return DoSetAltMode(altMode); }
    RC SetPowerMode(PortPolicyCtrl::UsbPowerModes powerMode)
        { return DoSetPowerMode(powerMode); }
    RC SetOrientation(PortPolicyCtrl::CableOrientation orient, UINT16 orientDelay)
        { return DoSetOrientation(orient, orientDelay); }
    RC ReverseOrientation()
        { return DoReverseOrientation(); }
    RC GenerateCableAttachDetach
    (
        PortPolicyCtrl::CableAttachDetach eventType,
        UINT16 detachAttachDelay
    )
        { return DoGenerateCableAttachDetach(eventType, detachAttachDelay); }
    RC Reset()
        { return DoReset(); }
    RC GenerateInBandWakeup(UINT16 inBandDelay)
        { return DoGenerateInBandWakeup(inBandDelay); }
    RC SetDpLoopback(PortPolicyCtrl::DpLoopback dpLoopback)
        { return DoSetDpLoopback(dpLoopback); }
    RC SetIsocDevice(PortPolicyCtrl::IsocDevice enableIsocDev)
        { return DoSetIsocDevice(enableIsocDev); }
    RC GetLwrrUsbAltMode(PortPolicyCtrl::UsbAltModes* pAltMode)
        { return DoGetLwrrUsbAltMode(pAltMode); }
    RC ConfirmUsbAltModeConfig(PortPolicyCtrl::UsbAltModes altMode)
        { return DoConfirmUsbAltModeConfig(altMode); }
    RC GetPpcFwVersion(string *pPrimaryVersion, string *pSecondaryVersion)
        { return DoGetPpcFwVersion(pPrimaryVersion, pSecondaryVersion); }
    RC GetPpcDriverVersion(string *pDriverVersion)
        { return DoGetPpcDriverVersion(pDriverVersion); }
    RC GetFtbFwVersion(string *pFtbFwVersion)
        { return DoGetFtbFwVersion(pFtbFwVersion); }
    RC GetPowerDrawn(FLOAT64 *pVoltageV, FLOAT64 *pLwrrentA)
        { return DoGetPowerDrawn(pVoltageV, pLwrrentA); }

protected:
    virtual RC DoInitialize() = 0;
    virtual RC DoSetAltMode(PortPolicyCtrl::UsbAltModes altMode) = 0;
    virtual RC DoSetPowerMode(PortPolicyCtrl::UsbPowerModes powerMode) = 0;
    virtual RC DoSetOrientation(PortPolicyCtrl::CableOrientation orient, UINT16 orientDelay) = 0;
    virtual RC DoReverseOrientation() = 0;
    virtual RC DoGenerateCableAttachDetach
    (
        PortPolicyCtrl::CableAttachDetach eventType,
        UINT16 detachAttachDelay
    ) = 0;
    virtual RC DoReset() = 0;
    virtual RC DoGenerateInBandWakeup(UINT16 inBandDelay) = 0;
    virtual RC DoSetDpLoopback(PortPolicyCtrl::DpLoopback dpLoopback) = 0;
    virtual RC DoSetIsocDevice(PortPolicyCtrl::IsocDevice enableIsocDev) = 0;
    virtual RC DoGetLwrrUsbAltMode(PortPolicyCtrl::UsbAltModes* pAltMode) = 0;
    virtual RC DoConfirmUsbAltModeConfig(PortPolicyCtrl::UsbAltModes pAltMode) = 0;
    virtual RC DoGetPpcFwVersion(string *pPrimaryVersion, string *pSecondaryVersion) = 0;
    virtual RC DoGetPpcDriverVersion(string *pDriverVersion) = 0;
    virtual RC DoGetFtbFwVersion(string *pFtbFwVersion) = 0;
    virtual RC DoGetPowerDrawn(FLOAT64 *pVoltageV, FLOAT64 *pLwrrentA) = 0;

};

