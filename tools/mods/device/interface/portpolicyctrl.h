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

//! \file portpolicyctrl.h -- Port Policy Controller interface definition

#pragma once

#include "core/include/rc.h"

class PortPolicyCtrl
{
public:

    // List of supported ALT modes
    enum UsbAltModes : UINT08
    {
        USB_ALT_MODE_0          // 4 Lanes of DP + 2 Lanes of USB 3 - Virtual Link Mode
       ,USB_ALT_MODE_1          // 4 Lanes of DP + 1 Lane of USB 2
       ,USB_ALT_MODE_2          // 2 Lanes of DP + 2 Lanes of USB 3 + 1 Lane of USB 2
       ,USB_ALT_MODE_3          // 2 Lanes of USB 3 + 1 Lane of USB 2 - Standard USB C mode
       ,USB_ALT_MODE_4          // 4 Lanes of DP + 2 Lanes of USB 3 - valve adapter board
       ,USB_LW_TEST_ALT_MODE    // Lwpu Functional Test Board Configuration
       /* New Modes go here */
       ,USB_UNKNOWN_ALT_MODE
    };

    enum UsbPowerModes : UINT08
    {
        USB_POWER_MODE_0         // 5V @ 3A
       ,USB_POWER_MODE_1         // 9V @3A
       ,USB_POWER_MODE_2         // 12V @2.25A
       /* New Modes go here */
       ,USB_DEFAULT_POWER_MODE   // FTB draws only power needed for attached devices
       ,USB_UNKNOWN_POWER_MODE
    };

    enum CableOrientation : UINT08
    {
        USB_CABLE_ORIENT_ORIGINAL  // Default orientation with which the cable was attached
       ,USB_CABLE_ORIENT_FLIPPED   // Flipped programmatically
       ,USB_UNKNOWN_ORIENTATION
    };

    enum DpLoopback : UINT08
    {
        USB_DISABLE_DP_LOOPBACK
       ,USB_ENABLE_DP_LOOPBACK
       ,USB_UNKNOWN_DP_LOOPBACK_STATUS
    };

    enum IsocDevice : UINT08
    {
        USB_ISOC_DEVICE_DISABLE
       ,USB_ISOC_DEVICE_ENABLE
       ,USB_UNKNOWN_ISOC_DEVICE_STATUS
    };

    enum CableAttachDetach : UINT08
    {
        USB_CABLE_ATTACH            // Fake a cable attach
       ,USB_CABLE_DETACH            // Fake a cable detach
    };

    enum AddCableDetach : bool
    {
        USB_CABLE_DETACH_NOT_NEEDED   // No need to fake reconnect the FTB
       ,USB_CABLE_DETACH_NEEDED       // Need to reconnect the FTB
    };

    // After every update to sysfs node, the PPC (port policy controller) driver
    // needs time to negotiate and configure the muxes
    static const UINT64 USB_PPC_CONFIG_TIMEOUT_MS = 5000;
    static const UINT64 USB_PPC_CONFIG_WAIT_TIME_MS = 2000;
    static const UINT64 USB_POWER_CONFIG_WAIT_TIME_MS = 5000;
    static const UINT64 USB_SYSFS_NODE_WAIT_TIME_MS = 1000;

    virtual ~PortPolicyCtrl() {};

    // General Functions
    bool IsSupported() const
        { return DoIsSupported(); }
    RC Initialize()
        { return DoInitialize(); }
    RC InitializeFtb()
        { return DoInitializeFtb(); }
    RC SetAltMode(UsbAltModes altMode)
        { return DoSetAltMode(altMode); }
    RC ConfirmAltModeConfig(UsbAltModes altMode)
        { return DoConfirmAltModeConfig(altMode); }
    RC SetPowerMode(UsbPowerModes powerMode, bool resetPowerMode, bool skipPowerCheck)
        { return DoSetPowerMode(powerMode, resetPowerMode, skipPowerCheck); }
    RC SetLwrrentMarginPercent(FLOAT64 lwrrentMargingPercent)
        { return DoSetLwrrentMarginPercent(lwrrentMargingPercent); }
    FLOAT64 GetLwrrentMarginPercent() const
        { return DoGetLwrrentMarginPercent(); }
    RC SetVoltageMarginPercent(FLOAT64 voltMargingPercent)
        { return DoSetVoltageMarginPercent(voltMargingPercent); }
    FLOAT64 GetVoltageMarginPercent() const
        { return DoGetVoltageMarginPercent(); }
    RC SetPrintPowerValue(bool printPowerValue)
        { return DoSetPrintPowerValue(printPowerValue); }
    bool GetPrintPowerValue() const
        { return DoGetPrintPowerValue(); }
    RC SetOrientation(CableOrientation orient, UINT16 orientDelay)
        { return DoSetOrientation(orient, orientDelay); }
    RC ReverseOrientation()
        { return DoReverseOrientation(); }
    RC SetSkipGpuOrientationCheck(bool skipGpuOrientationCheck)
        { return DoSetSkipGpuOrientationCheck(skipGpuOrientationCheck); }
    bool GetSkipGpuOrientationCheck() const
        { return DoGetSkipGpuOrientationCheck(); }
    RC GenerateCableAttachDetach(CableAttachDetach eventType, UINT16 detachAttachDelay)
        { return DoGenerateCableAttachDetach(eventType, detachAttachDelay); }
    RC ResetFtb()
        { return DoResetFtb(); }
    RC GenerateInBandWakeup(UINT16 inBandDelay)
        { return DoGenerateInBandWakeup(inBandDelay); }
    RC SetDpLoopback(DpLoopback dpLoopback)
        { return DoSetDpLoopback(dpLoopback); }
    RC SetIsocDevice(IsocDevice enableIsocDev)
        { return DoSetIsocDevice(enableIsocDev); }
    RC GetPciDBDF
    (
        UINT32 *pDomain   = nullptr,
        UINT32 *pBus      = nullptr,
        UINT32 *pDevice   = nullptr,
        UINT32 *pFunction = nullptr
    ) const
        { return DoGetPciDBDF(pDomain, pBus, pDevice, pFunction); }
    RC PrintFtbConfigDetails() const
        { return DoPrintFtbConfigDetails(); }
    RC WaitForConfigToComplete() const
        { return DoWaitForConfigToComplete(); }
    RC GetLwrrAltMode(UsbAltModes *pLwrrAltMode) const
        { return DoGetLwrrAltMode(pLwrrAltMode); }
    RC IsOrientationFlipped(bool *pIsOrientationFlipped) const
        { return DoIsOrientationFlipped(pIsOrientationFlipped); }
    RC GetExpectedUsbDevForAltMode
    (
        UsbAltModes altMode,
        UINT08 *pNumOfUsb3Dev,
        UINT08 *pNumOfUsb2Dev
    ) const
        { return DoGetExpectedUsbDevForAltMode(altMode, pNumOfUsb3Dev, pNumOfUsb2Dev); }
    RC GetPpcFwVersion(string *pPrimaryVersion, string *pSecondaryVersion)
        { return DoGetPpcFwVersion(pPrimaryVersion, pSecondaryVersion); }
    RC GetPpcDriverVersion(string *pDriverVersion)
        { return DoGetPpcDriverVersion(pDriverVersion); }
    RC GetFtbFwVersion(string *pFtbFwVersion)
        { return DoGetFtbFwVersion(pFtbFwVersion); }
    RC PrintPowerDrawnByFtb()
        { return DoPrintPowerDrawnByFtb(); }
    RC SetSkipUsbEnumerationCheck(bool skipUsbEnumerationCheck)
        { return DoSetSkipUsbEnumerationCheck(skipUsbEnumerationCheck); }
    bool GetSkipUsbEnumerationCheck() const
        { return DoGetSkipUsbEnumerationCheck(); }
    RC GetAspmState(UINT32* pState)
        { return DoGetAspmState(pState); }
    RC SetAspmState(UINT32 state)
        { return DoSetAspmState(state); }

protected:
    virtual bool DoIsSupported() const = 0;
    virtual RC DoInitialize() = 0;
    virtual RC DoInitializeFtb() = 0;
    virtual RC DoSetAltMode(UsbAltModes altMode) = 0;
    virtual RC DoConfirmAltModeConfig(UsbAltModes altMode) = 0;
    virtual RC DoSetPowerMode
    (
        UsbPowerModes powerMode,
        bool resetPowerMode,
        bool skipPowerCheck
    ) = 0;
    virtual RC DoSetLwrrentMarginPercent(FLOAT64 lwrrentMargingPercent) = 0;
    virtual FLOAT64 DoGetLwrrentMarginPercent() const = 0;
    virtual RC DoSetVoltageMarginPercent(FLOAT64 voltageMargingPercent) = 0;
    virtual FLOAT64 DoGetVoltageMarginPercent() const = 0;
    virtual RC DoSetPrintPowerValue(bool printPowerValue) = 0;
    virtual bool DoGetPrintPowerValue() const = 0;
    virtual RC DoSetOrientation(CableOrientation orient, UINT16 orientDelay) = 0;
    virtual RC DoReverseOrientation() = 0;
    virtual RC DoSetSkipGpuOrientationCheck(bool skipGpuOrientationCheck) = 0;
    virtual bool DoGetSkipGpuOrientationCheck() const = 0;
    virtual RC DoGenerateCableAttachDetach
    (
        CableAttachDetach eventType,
        UINT16 detachAttachDelay
    ) = 0;
    virtual RC DoResetFtb() = 0;
    virtual RC DoGenerateInBandWakeup(UINT16 inBandDelay) = 0;
    virtual RC DoSetDpLoopback(DpLoopback dpLoopback) = 0;
    virtual RC DoSetIsocDevice(IsocDevice enableIsocDev) = 0;
    virtual RC DoGetPciDBDF
    (
        UINT32 *pDomain   = nullptr,
        UINT32 *pBus      = nullptr,
        UINT32 *pDevice   = nullptr,
        UINT32 *pFunction = nullptr
    ) const = 0;
    virtual RC DoPrintFtbConfigDetails() const = 0;
    virtual RC DoWaitForConfigToComplete() const = 0;
    virtual RC DoGetLwrrAltMode(UsbAltModes *pLwrrAltMode) const = 0;
    virtual RC DoIsOrientationFlipped(bool *pIsOrientationFlipped) const = 0;
    virtual RC DoGetExpectedUsbDevForAltMode
    (
        UsbAltModes altMode,
        UINT08 *pNumOfUsb3Dev,
        UINT08 *pNumOfUsb2Dev
    ) const = 0;
    virtual RC DoGetPpcFwVersion(string *pPrimaryVersion, string *pSecondaryVersion) = 0;
    virtual RC DoGetPpcDriverVersion(string *pDriverVersion) = 0;
    virtual RC DoGetFtbFwVersion(string *pFtbFwVersion) = 0;
    virtual RC DoPrintPowerDrawnByFtb() = 0;
    virtual RC DoSetSkipUsbEnumerationCheck(bool skipUsbEnumerationCheck) = 0;
    virtual bool DoGetSkipUsbEnumerationCheck() const = 0;
    virtual RC DoGetAspmState(UINT32* pState) = 0;
    virtual RC DoSetAspmState(UINT32 state) = 0;
};
