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

#include "device/interface/portpolicyctrl.h"

struct JSContext;
struct JSObject;

class JsPortPolicyCtrl
{
public:
    JsPortPolicyCtrl(PortPolicyCtrl* pPortPolicyCtrl);

    RC CreateJSObject(JSContext* cx, JSObject* obj);

    RC SetAltMode(UINT08 altMode);
    RC SetPowerMode
    (
        UINT08 powerMode,
        bool resetPowerMode,
        bool skipPowerCheck
    );
    RC SetLwrrentMarginPercent(FLOAT64 lwrrentMargingPercent);
    FLOAT64 GetLwrrentMarginPercent() const;
    RC SetVoltageMarginPercent(FLOAT64 voltageMargingPercent);
    FLOAT64 GetVoltageMarginPercent() const;
    RC SetPrintPowerValue(bool printPowerValue);
    bool GetPrintPowerValue() const;
    RC SetOrientation(UINT08 orient, UINT16 orientDelay);
    RC ReverseOrientation();
    RC SetSkipGpuOrientationCheck(bool skipGpuOrientationCheck);
    bool GetSkipGpuOrientationCheck() const;
    RC GenerateCableAttachDetach(UINT08 eventType, UINT16 detachAttachDelay);
    RC ResetFtb();
    RC GenerateInBandWakeup(UINT16 inBandDelay);
    RC SetDpLoopback(bool dpLoopback);
    RC SetIsocDevice(bool enableIsocDev);
    RC PrintFtbConfigDetails() const;
    RC GetPpcFwVersion(string *pPrimaryVersion, string *pSecondaryVersion);
    RC GetPpcDriverVersion(string *pDriverVersion);
    RC GetFtbFwVersion(string *pFtbFwVersion);
    RC PrintPowerDrawnByFtb();
    RC SetSkipUsbEnumerationCheck(bool skipUsbEnumerationCheck);
    bool GetSkipUsbEnumerationCheck() const;
    RC GetAspmState(UINT32* pState);
    RC SetAspmState(UINT32 state);

private:
    PortPolicyCtrl* m_pPortPolicyCtrl = nullptr;
    JSObject* m_pJsPortPolicyCtrlObj = nullptr;
};
