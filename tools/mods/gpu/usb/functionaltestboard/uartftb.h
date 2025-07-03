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

//! \file uartftb.h -- concrete implementation for Functional Test Board using uart

#pragma once

#include "gpu/usb/functionaltestboard/functionaltestboard.h"
#include "core/include/serial.h"

class Serial;

class UartFtb : public FunctionalTestBoard
{
public:
    UartFtb(const string& ftbDevice): m_FtbDevice(ftbDevice) {};
    virtual ~UartFtb() {};

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
    ) override
    { return RC::UNSUPPORTED_FUNCTION; }
    RC DoGetPpcDriverVersion(string *pDriverVersion) override
    { return RC::UNSUPPORTED_FUNCTION; }
    RC DoGetFtbFwVersion(string *pFtbFwVersion) override;
    RC DoGetPowerDrawn(FLOAT64 *pVoltageV, FLOAT64 *pLwrrentA) override;

private:
    RC SendMessage(const string& command, string* pResponse);
    RC SendCharacter(const char c);
    RC WaitForPrompt(string* pResponse);
    bool WaitForCharacter(UINT32 timeoutMs);
    RC FlipToggle(PortPolicyCtrl::CableOrientation *pNewOrientation);

    string m_FtbDevice;
    Serial* m_pCom = nullptr;
    UINT32 m_ClientId = SerialConst::CLIENT_USB_FTB;

    bool m_CheckForPrompt = true;
    UINT32 m_WaitTimeoutMs = 3000;
    UINT32 m_IdleTimeoutMs = 200;

    bool m_SupportsEnterAltModeUsbC = true;
    bool m_SupportsPowerReadings = true;

    static const UINT32 USB_UART_BAUD_RATE = 9600;
};

