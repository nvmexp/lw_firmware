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

//! \file xusbhostctrlimpl.h -- Generic Implemetation for XUSB Host Controller

#pragma once

#include "device/interface/xusbhostctrl.h"

class XusbHostCtrlImpl : public XusbHostCtrl
{
public:
    XusbHostCtrlImpl() = default;
    virtual ~XusbHostCtrlImpl();

protected:
    bool DoIsSupported() const override;
    RC DoGetPciDBDF
    (
        UINT32 *pDomain   = nullptr,
        UINT32 *pBus      = nullptr,
        UINT32 *pDevice   = nullptr,
        UINT32 *pFunction = nullptr
    ) const override;
    RC DoInitialize() override;
    RC DoMapAvailBARs() override;
    RC DoUnMapAvailBARs() override;
    RC DoIsLowPowerStateEnabled(bool *pIsHcInLowPowerMode) override;
    RC DoEnableLowPowerState(UINT32 autoSuspendDelayMs) override;

    // Supported only on Turing
    RC DoLimitDataRate(HostCtrlDataRate dataRate, bool skipDataRateCheck) override
    {
        return RC::UNSUPPORTED_FUNCTION; 
    }

    RC DoGetEomStatus
    (
        UsbEomMode mode,
        FLOAT64 timeoutMs,
        bool printCfgReg,
        UINT32 *pStatus,
        LaneDeviceInfo *pLaneDeviceInfo
    ) override
    {
        return RC::UNSUPPORTED_FUNCTION; 
    }

    RC DoUsbDeviceSpeedEnumToString
    (
        UsbDeviceSpeed speed,
        string *pSpeedShortStr,
        string *pSpeedLongStr
    ) const override;

    RC DoGetNumOfConnectedUsbDev
    (
        UINT08 *pUsb3_1,
        UINT08 *pUsb3_0,
        UINT08 *pUsb2
    ) override;

    RC DoGetAspmState(UINT32* pState) override;
    RC DoSetAspmState(UINT32 state) override;

    virtual RC SupportInitHelper();
    virtual RC FindHostController(UINT32 domain, UINT32 bus, UINT32 device, UINT32 function);
    virtual RC MapBAR(UINT32 barIndex, void **ppBarAddr);
    virtual bool IsBarSupported(UINT32 barIndex);
    virtual RC GetNumOfPorts(UINT32 *pNumOfPorts);
    virtual RC GetPortNumSpeedOfConnectedDev
    (
        LaneDeviceInfo *pLaneDeviceUsb3Info,
        LaneDeviceInfo *pLaneDeviceUsb2Info
    );
    virtual RC ReadReg32
    (
        void *pBar,
        ModsGpuRegAddress regAddr,
        UINT32 *pRegVal
    );
    virtual RC ReadReg32
    (
        void *pBar,
        ModsGpuRegAddress regAddr,
        UINT32 idx,
        UINT32 *pRegVal
    );
    virtual RC ReadReg32
    (
        void *pBar,
        UINT32 regOffset,
        UINT32 *pRegVal
    );
    virtual RC WriteReg32
    (
        void *pBar,
        ModsGpuRegAddress regAddr,
        UINT32 regVal
    );
    virtual RC WriteReg32
    (
        void *pBar,
        UINT32 regOffset,
        UINT32 regVal
    );

    bool          m_Init       = false;     // Is Host Contoller intialized
    bool          m_IsChecked  = false;     // is the system checked for XUSB HC
    UINT32        m_Domain     = ~0U;       // Domain number of host controller
    UINT32        m_Bus        = ~0U;       // Bus number of host controller
    UINT32        m_Device     = ~0U;       // Device number of host controller
    UINT32        m_Function   = ~0U;       // Function number of host controller
    void         *m_pBar0Addr  = nullptr;   // address for mapped BAR0

protected:
    virtual TestDevice* GetDevice() = 0;
    virtual const TestDevice* GetDevice() const = 0;
};
