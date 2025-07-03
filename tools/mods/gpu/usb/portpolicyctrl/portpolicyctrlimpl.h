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

//! \file portpolicyctrlimpl.h -- concrete implementation for Port Policy Controller

#pragma once

#include "device/interface/portpolicyctrl.h"

class TestDevice;
class FunctionalTestBoard;

class PortPolicyCtrlImpl : public PortPolicyCtrl
{
public:
    PortPolicyCtrlImpl();
    virtual ~PortPolicyCtrlImpl();

protected:
    bool DoIsSupported() const override;
    RC DoInitialize() override;
    RC DoInitializeFtb() override;

    RC DoSetAltMode(UsbAltModes altMode) override;
    RC DoConfirmAltModeConfig(UsbAltModes altMode) override;
    RC DoSetPowerMode(UsbPowerModes powerMode, bool resetPowerMode, bool skipPowerCheck) override;
    RC DoSetLwrrentMarginPercent(FLOAT64 lwrrentMargingPercent) override
    {
        m_LwrrentMarginPercent = lwrrentMargingPercent;
        return OK;
    };
    FLOAT64 DoGetLwrrentMarginPercent() const override
    {
        return m_LwrrentMarginPercent;
    }
    RC DoSetVoltageMarginPercent(FLOAT64 voltageMargingPercent) override
    {
        m_VoltageMarginPercent = voltageMargingPercent;
        return OK;
    };
    FLOAT64 DoGetVoltageMarginPercent() const override
    {
        return m_VoltageMarginPercent;
    }
    RC DoSetPrintPowerValue(bool printPowerValue) override
    {
        m_PrintPowerValue = printPowerValue;
        return OK;
    };
    bool DoGetPrintPowerValue() const override
    {
        return m_PrintPowerValue;
    }
    RC DoSetOrientation(CableOrientation orient, UINT16 orientDelay) override;
    RC DoReverseOrientation() override;
    RC DoSetSkipGpuOrientationCheck(bool skipGpuOrientationCheck) override
    {
        m_SkipGpuOrientationCheck = skipGpuOrientationCheck;
        return OK;
    };
    bool DoGetSkipGpuOrientationCheck() const override
    {
        return m_SkipGpuOrientationCheck;
    }
    RC DoGenerateCableAttachDetach
    (
        CableAttachDetach eventType,
        UINT16 detachAttachDelay
    ) override;
    RC DoResetFtb() override;
    RC DoGenerateInBandWakeup(UINT16 inBandDelay) override;
    RC DoSetDpLoopback(DpLoopback dpLoopback) override;
    RC DoSetIsocDevice(IsocDevice enableIsocDev) override;
    RC DoGetPciDBDF
    (
        UINT32 *pDomain   = nullptr,
        UINT32 *pBus      = nullptr,
        UINT32 *pDevice   = nullptr,
        UINT32 *pFunction = nullptr
    ) const override
    {
        // Implementation added in chip specific class
        return RC::UNSUPPORTED_FUNCTION;
    }
    RC DoPrintFtbConfigDetails() const override;

    // The port policy controller needs some time to 
    // negotiate with the functional test board and
    // configure the MUX on GPU side.
    // On Turing the negotiation can be verified by reading
    // MODS_XVE_PPC_STATUS_ALT_MODE_NEG_STATUS
    RC DoWaitForConfigToComplete() const override
    {
        // Implementation added in chip specific class
        return RC::UNSUPPORTED_FUNCTION;
    }
    RC DoGetLwrrAltMode(UsbAltModes *pLwrrAltMode) const override;
    RC DoIsOrientationFlipped(bool *pIsOrientationFlipped) const override
    {
        // Implementation added in chip specific class
        return RC::UNSUPPORTED_FUNCTION;
    }
    RC DoGetExpectedUsbDevForAltMode
    ( 
        UsbAltModes altMode,
        UINT08 *pNumOfUsb3Dev,
        UINT08 *pNumOfUsb2Dev
    ) const override;

    RC DoGetPpcFwVersion
    (
        string *pPrimaryVersion,
        string *pSecondaryVersion
    ) override;
    RC DoGetPpcDriverVersion(string *pDriverVersion) override;
    RC DoGetFtbFwVersion(string *pFtbFwVersion) override;
    RC DoPrintPowerDrawnByFtb() override;

    RC DoSetSkipUsbEnumerationCheck(bool skipUsbEnumerationCheck) override
    {
        m_SkipUsbEnumerationCheck = skipUsbEnumerationCheck;;
        return OK;
    };
    bool DoGetSkipUsbEnumerationCheck() const override
    {
        return m_SkipUsbEnumerationCheck;
    }
    RC DoGetAspmState(UINT32* pState) override;
    RC DoSetAspmState(UINT32 state) override;

    virtual RC VerifyPrintPowerValues(UsbPowerModes powerMode,
                                      bool skipPowerCheck,
                                      bool printPowerValue) const;
protected:
    virtual TestDevice* GetDevice() = 0;
    virtual const TestDevice* GetDevice() const = 0;

private:
    bool          m_Init            = false;
    UINT32        m_Domain          = ~0U;
    UINT32        m_Bus             = ~0U;
    UINT32        m_Device          = ~0U;
    UINT32        m_Function        = ~0U;
    UsbAltModes   m_UsbAltMode      = USB_UNKNOWN_ALT_MODE;
    UsbPowerModes m_UsbPowerMode    = USB_UNKNOWN_POWER_MODE;
    CableOrientation m_Orient       = USB_UNKNOWN_ORIENTATION;
    DpLoopback    m_DpLoopback      = USB_UNKNOWN_DP_LOOPBACK_STATUS;
    IsocDevice    m_IsocDevice      = USB_UNKNOWN_ISOC_DEVICE_STATUS;
    unique_ptr<FunctionalTestBoard> m_pFtb;

    // Reference for margins values - http://lwbugs/200439030/1
    FLOAT64       m_LwrrentMarginPercent = 15.0;
    FLOAT64       m_VoltageMarginPercent = 15.0;
    bool          m_PrintPowerValue = false;
    bool          m_SkipGpuOrientationCheck = false;

    // Skip/Perform enumerated USB device in ALT mode check
    bool          m_SkipUsbEnumerationCheck = false;

    // Multiple tests claiming a handle to the Port Policy Controller shouldn't
    // modify the configuration of functional test board at the same time.
    void        *m_pAccessPpcMutex   = nullptr;
};
