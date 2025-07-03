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

//! \file xusbhostctrl.h -- XUSB Host Controller interface definition

#pragma once

#include "core/include/rc.h"

class XusbHostCtrl
{
public:
    virtual ~XusbHostCtrl() {};

    enum HostCtrlDataRate : UINT08
    {
        USB_HIGH_SPEED,
        USB_SUPER_SPEED,
        USB_SUPER_SPEED_PLUS,
        USB_UNKNOWN_DATA_RATE
    };
    typedef HostCtrlDataRate UsbDeviceSpeed;

    enum UsbEomMode
    {
        EYEO_X,
        EYEO_Y
    };

    struct LaneDeviceInfo
    {
        INT08 portNum = -1;
        UINT08 laneNum = numeric_limits<UINT08>::max();
        UsbDeviceSpeed speed = USB_UNKNOWN_DATA_RATE;
    };

    static const UINT32 USB_REENUMERATE_DEVICE_WAIT_TIME_MS = 2000;

    // General Functions
    bool IsSupported() const
        { return DoIsSupported(); }
    RC GetPciDBDF
    (
        UINT32 *pDomain   = nullptr,
        UINT32 *pBus      = nullptr,
        UINT32 *pDevice   = nullptr,
        UINT32 *pFunction = nullptr
    ) const
        { return DoGetPciDBDF(pDomain, pBus, pDevice, pFunction); }
    RC Initialize()
        { return DoInitialize(); }
    RC MapAvailBARs()
        { return DoMapAvailBARs(); }
    RC UnMapAvailBARs()
        { return DoUnMapAvailBARs(); }
    RC IsLowPowerStateEnabled(bool *pIsHcInLowPowerMode)
        { return DoIsLowPowerStateEnabled(pIsHcInLowPowerMode); }
    RC LimitDataRate(HostCtrlDataRate dataRate, bool skipDataRateCheck)
        { return DoLimitDataRate(dataRate, skipDataRateCheck); }
    RC EnableLowPowerState(UINT32 autoSuspendDelayMs)
        { return DoEnableLowPowerState(autoSuspendDelayMs); }
    RC GetNumOfConnectedUsbDev(UINT08 *pUsb3_1, UINT08 *pUsb3_0, UINT08 *pUsb2)
        { return DoGetNumOfConnectedUsbDev(pUsb3_1, pUsb3_0, pUsb2); }
    RC GetEomStatus
    (
        UsbEomMode mode,
        FLOAT64 timeoutMs,
        bool printCfgReg,
        UINT32 *pStatus,
        LaneDeviceInfo *pLaneDeviceInfo
    )
        { return DoGetEomStatus(mode, timeoutMs, printCfgReg, pStatus, pLaneDeviceInfo); }
    RC UsbDeviceSpeedEnumToString
    (
        UsbDeviceSpeed speed,
        string *pSpeedShortStr,
        string *pSpeedLongStr
    ) const
        { return DoUsbDeviceSpeedEnumToString(speed, pSpeedShortStr, pSpeedLongStr); }
    RC GetAspmState(UINT32* pState)
        { return DoGetAspmState(pState); }
    RC SetAspmState(UINT32 state)
        { return DoSetAspmState(state); }

protected:
    virtual bool DoIsSupported() const = 0;
    virtual RC DoGetPciDBDF
    (
        UINT32 *pDomain   = nullptr,
        UINT32 *pBus      = nullptr,
        UINT32 *pDevice   = nullptr,
        UINT32 *pFunction = nullptr
    ) const = 0;
    virtual RC DoInitialize() = 0;
    virtual RC DoMapAvailBARs() = 0;
    virtual RC DoUnMapAvailBARs() = 0;
    virtual RC DoIsLowPowerStateEnabled(bool *pIsHcInLowPowerMode) = 0;
    virtual RC DoLimitDataRate(HostCtrlDataRate dataRate, bool skipDataRateCheck) = 0;
    virtual RC DoEnableLowPowerState(UINT32 autoSuspendDelayMs) = 0;
    virtual RC DoGetNumOfConnectedUsbDev
    (
        UINT08 *pUsb3_1,
        UINT08 *pUsb3_0,
        UINT08 *pUsb2
    ) = 0;
    virtual RC DoGetEomStatus
    (
        UsbEomMode mode,
        FLOAT64 timeoutMs,
        bool printCfgReg,
        UINT32 *pStatus,
        LaneDeviceInfo *pLaneDeviceInfo
    ) = 0;
    virtual RC DoUsbDeviceSpeedEnumToString
    (
        UsbDeviceSpeed speed,
        string *pSpeedShortStr,
        string *pSpeedLongStr
    ) const = 0;
    virtual RC DoGetAspmState(UINT32* pState) = 0;
    virtual RC DoSetAspmState(UINT32 state) = 0;
};
