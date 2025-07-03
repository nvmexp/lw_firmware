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

#include "device/interface/js/js_portpolicyctrl.h"
#include "core/include/jscript.h"
#include "core/include/script.h"

//-----------------------------------------------------------------------------
JsPortPolicyCtrl::JsPortPolicyCtrl(PortPolicyCtrl* pPortPolicyCtrl)
: m_pPortPolicyCtrl(pPortPolicyCtrl)
{
    MASSERT(m_pPortPolicyCtrl);
}

//-----------------------------------------------------------------------------
static void C_JsPortPolicyCtrl_finalize
(
    JSContext *cx,
    JSObject *obj
)
{
    MASSERT(cx != 0);
    MASSERT(obj != 0);

    JsPortPolicyCtrl * pJsPortPolicyCtrl;
    //! Delete the C++
    pJsPortPolicyCtrl = (JsPortPolicyCtrl *)JS_GetPrivate(cx, obj);
    delete pJsPortPolicyCtrl;
};

//-----------------------------------------------------------------------------
static JSClass JsPortPolicyCtrl_class =
{
    "PortPolicyCtrl",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ColwertStub,
    C_JsPortPolicyCtrl_finalize
};

//-----------------------------------------------------------------------------
static SObject JsPortPolicyCtrl_Object
(
    "PortPolicyCtrl",
    JsPortPolicyCtrl_class,
    0,
    0,
    "PortPolicyCtrl JS Object"
);

//-----------------------------------------------------------------------------
RC JsPortPolicyCtrl::CreateJSObject(JSContext* cx, JSObject* obj)
{
    //! Only create one JSObject per Perf object
    if (m_pJsPortPolicyCtrlObj)
    {
        Printf(Tee::PriLow,
               "A JS Object has already been created for this JsPortPolicyCtrl.\n");
        return OK;
    }

    m_pJsPortPolicyCtrlObj = JS_DefineObject(cx,
                                             obj,              // device object
                                             "PortPolicyCtrl", // Property name
                                             &JsPortPolicyCtrl_class,
                                             JsPortPolicyCtrl_Object.GetJSObject(),
                                             JSPROP_READONLY);

    if (!m_pJsPortPolicyCtrlObj)
        return RC::COULD_NOT_CREATE_JS_OBJECT;

    //! Store the current JsPortPolicyCtrl instance into the private area
    //! of the new JSOBject.
    if (JS_SetPrivate(cx, m_pJsPortPolicyCtrlObj, this) != JS_TRUE)
    {
        Printf(Tee::PriNormal,
               "Unable to set private value of JsPortPolicyCtrl.\n");
        return RC::COULD_NOT_CREATE_JS_OBJECT;
    }

    // Add the .Help() to this JSObject
    JS_DefineFunction(cx, m_pJsPortPolicyCtrlObj, "Help", &C_Global_Help, 1, 0);

    return OK;
}

//-----------------------------------------------------------------------------
RC JsPortPolicyCtrl::ResetFtb()
{
    return m_pPortPolicyCtrl->ResetFtb();
}

//-----------------------------------------------------------------------------
RC JsPortPolicyCtrl::PrintFtbConfigDetails() const
{
    return m_pPortPolicyCtrl->PrintFtbConfigDetails();
}

//-----------------------------------------------------------------------------
RC JsPortPolicyCtrl::SetAltMode(UINT08 altMode)
{
    return m_pPortPolicyCtrl->SetAltMode(static_cast<PortPolicyCtrl::UsbAltModes>(altMode));
}

//-----------------------------------------------------------------------------
RC JsPortPolicyCtrl::SetPowerMode
(
    UINT08 powerMode,
    bool resetPowerMode,
    bool skipPowerCheck
)
{
    return m_pPortPolicyCtrl->SetPowerMode(static_cast<PortPolicyCtrl::UsbPowerModes>(powerMode),
                                           resetPowerMode,
                                           skipPowerCheck);
}

//-----------------------------------------------------------------------------
RC JsPortPolicyCtrl::SetLwrrentMarginPercent(FLOAT64 lwrrentMargingPercent)
{
    return m_pPortPolicyCtrl->SetLwrrentMarginPercent(lwrrentMargingPercent);
}

//-----------------------------------------------------------------------------
FLOAT64 JsPortPolicyCtrl::GetLwrrentMarginPercent() const
{
    return m_pPortPolicyCtrl->GetLwrrentMarginPercent();
}

//-----------------------------------------------------------------------------
RC JsPortPolicyCtrl::SetVoltageMarginPercent(FLOAT64 voltageMargingPercent)
{
    return m_pPortPolicyCtrl->SetVoltageMarginPercent(voltageMargingPercent);
}

//-----------------------------------------------------------------------------
FLOAT64 JsPortPolicyCtrl::GetVoltageMarginPercent() const
{
    return m_pPortPolicyCtrl->GetVoltageMarginPercent();
}

//-----------------------------------------------------------------------------
RC JsPortPolicyCtrl::SetPrintPowerValue(bool printPowerValue)
{
    return m_pPortPolicyCtrl->SetPrintPowerValue(printPowerValue);
}

//-----------------------------------------------------------------------------
bool JsPortPolicyCtrl::GetPrintPowerValue() const
{
    return m_pPortPolicyCtrl->GetPrintPowerValue();
}

//-----------------------------------------------------------------------------
RC JsPortPolicyCtrl::GenerateInBandWakeup(UINT16 inBandDelay)
{
    return m_pPortPolicyCtrl->GenerateInBandWakeup(inBandDelay);
}

//-----------------------------------------------------------------------------
RC JsPortPolicyCtrl::SetDpLoopback(bool dpLoopback)
{
    return m_pPortPolicyCtrl->SetDpLoopback(static_cast<PortPolicyCtrl::DpLoopback>(dpLoopback));
}

//-----------------------------------------------------------------------------
RC JsPortPolicyCtrl::SetIsocDevice(bool enableIsocDev)
{
    return m_pPortPolicyCtrl->SetIsocDevice(
                                static_cast<PortPolicyCtrl::IsocDevice>(enableIsocDev));
}

//-----------------------------------------------------------------------------
RC JsPortPolicyCtrl::SetOrientation(UINT08 orient, UINT16 orientDelay)
{
    return m_pPortPolicyCtrl->SetOrientation(
                                static_cast<PortPolicyCtrl::CableOrientation>(orient), orientDelay);
}

//-----------------------------------------------------------------------------
RC JsPortPolicyCtrl::ReverseOrientation()
{
    return m_pPortPolicyCtrl->ReverseOrientation();
}

//-----------------------------------------------------------------------------
RC JsPortPolicyCtrl::SetSkipGpuOrientationCheck(bool skipGpuOrientationCheck)
{
    return m_pPortPolicyCtrl->SetSkipGpuOrientationCheck(skipGpuOrientationCheck);
}

//-----------------------------------------------------------------------------
bool JsPortPolicyCtrl::GetSkipGpuOrientationCheck() const
{
    return m_pPortPolicyCtrl->GetSkipGpuOrientationCheck();
}

//-----------------------------------------------------------------------------
RC JsPortPolicyCtrl::GenerateCableAttachDetach(UINT08 eventType, UINT16 detachAttachDelay)
{
    return m_pPortPolicyCtrl->GenerateCableAttachDetach(
                                static_cast<PortPolicyCtrl::CableAttachDetach>(eventType), 
                                detachAttachDelay);
}

//-----------------------------------------------------------------------------
RC JsPortPolicyCtrl::GetPpcFwVersion(string *pPrimaryVersion, string *pSecondaryVersion)
{
    return m_pPortPolicyCtrl->GetPpcFwVersion(pPrimaryVersion, pSecondaryVersion);
}

//-----------------------------------------------------------------------------
RC JsPortPolicyCtrl::GetPpcDriverVersion(string *pDriverVersion)
{
    return m_pPortPolicyCtrl->GetPpcDriverVersion(pDriverVersion);
}

//-----------------------------------------------------------------------------
RC JsPortPolicyCtrl::GetFtbFwVersion(string *pFtbFwVersion)
{
    return m_pPortPolicyCtrl->GetFtbFwVersion(pFtbFwVersion);
}

//-----------------------------------------------------------------------------
RC JsPortPolicyCtrl::PrintPowerDrawnByFtb()
{
    return m_pPortPolicyCtrl->PrintPowerDrawnByFtb();
}

RC JsPortPolicyCtrl::GetAspmState(UINT32* pState)
{
    return m_pPortPolicyCtrl->GetAspmState(pState);
}

RC JsPortPolicyCtrl::SetAspmState(UINT32 state)
{
    return m_pPortPolicyCtrl->SetAspmState(state);
}

//-----------------------------------------------------------------------------
RC JsPortPolicyCtrl::SetSkipUsbEnumerationCheck(bool skipUsbEnumerationCheck)
{
    return m_pPortPolicyCtrl->SetSkipUsbEnumerationCheck(skipUsbEnumerationCheck);
}

//-----------------------------------------------------------------------------
bool JsPortPolicyCtrl::GetSkipUsbEnumerationCheck() const
{
    return m_pPortPolicyCtrl->GetSkipUsbEnumerationCheck();
}

//-----------------------------------------------------------------------------

CLASS_PROP_READWRITE(JsPortPolicyCtrl,  LwrrentMarginPercent, FLOAT64, "Margin percentage for Current");
CLASS_PROP_READWRITE(JsPortPolicyCtrl,  VoltageMarginPercent, FLOAT64, "Margin percentage for Voltage");
CLASS_PROP_READWRITE(JsPortPolicyCtrl,  PrintPowerValue,      bool, "Print Power values as part of power mode switches");
CLASS_PROP_READWRITE(JsPortPolicyCtrl,  SkipUsbEnumerationCheck, bool, "Skip check for number of enumerated USB device in each ALT mode");
CLASS_PROP_READWRITE(JsPortPolicyCtrl,  SkipGpuOrientationCheck, bool, "Do not check if GPU has flipped orientation");
CLASS_PROP_READONLY( JsPortPolicyCtrl,  AspmState,            UINT32,  "Aspm state");

JS_STEST_BIND_NO_ARGS(JsPortPolicyCtrl, ResetFtb,              "Reset the functional test board");
JS_STEST_BIND_NO_ARGS(JsPortPolicyCtrl, PrintFtbConfigDetails, "Print functional test boards current configuration");
JS_STEST_BIND_NO_ARGS(JsPortPolicyCtrl, PrintPowerDrawnByFtb,  "Print power drawn by functional test board");
JS_STEST_BIND_NO_ARGS(JsPortPolicyCtrl, ReverseOrientation,    "Reverse Orientation");

JS_STEST_BIND_ONE_ARG(JsPortPolicyCtrl, SetAltMode,           altMode,       UINT08, "Set desired ALT mode");
JS_STEST_BIND_ONE_ARG(JsPortPolicyCtrl, GenerateInBandWakeup, inBandDelay,   UINT16, "Generate InBand Wakeup event");
JS_STEST_BIND_ONE_ARG(JsPortPolicyCtrl, SetDpLoopback,        dpLoopback,    bool,   "Enable/Disable DpLoopback");
JS_STEST_BIND_ONE_ARG(JsPortPolicyCtrl, SetIsocDevice,        enableIsocDev, bool,   "Enable/Disable Isochronous Device");
JS_STEST_BIND_ONE_ARG(JsPortPolicyCtrl, SetAspmState,         state,         UINT32, "Set ASPM state");

JS_STEST_BIND_TWO_ARGS(JsPortPolicyCtrl, SetOrientation,            orient,    UINT08, orientDelay,       UINT16, "Update Orientation of the cable");
JS_STEST_BIND_TWO_ARGS(JsPortPolicyCtrl, GenerateCableAttachDetach, eventType, UINT08, detachAttachDelay, UINT16, "Generate Cable Detach/Attach after Delay ms");

JS_STEST_BIND_THREE_ARGS(JsPortPolicyCtrl, SetPowerMode, powerMode, UINT08, resetPowerMode, bool, skipPowerCheck, bool, "Set desired Power mode");

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(JsPortPolicyCtrl,
                  GetPpcFwVersion,
                  1,
                  "Get Primary and Secondary PPC FW versions")
{
    STEST_HEADER(1, 1, "Usage: TestDevice.PortPolicyCtrl.GetPpcFwVersion(Array)\n");
    STEST_PRIVATE(JsPortPolicyCtrl, pJsPortPolicyCtrl, "PortPolicyCtrl");
    STEST_ARG(0, JSObject*, pArrayToReturn);

    RC rc;
    string primaryVersion, secondaryVersion;
    C_CHECK_RC(pJsPortPolicyCtrl->GetPpcFwVersion(&primaryVersion, &secondaryVersion));
    C_CHECK_RC(pJavaScript->SetElement(pArrayToReturn, 0, primaryVersion));
    C_CHECK_RC(pJavaScript->SetElement(pArrayToReturn, 1, secondaryVersion));

    RETURN_RC(rc);
}

JS_SMETHOD_LWSTOM(JsPortPolicyCtrl,
                  GetPpcDriverVersion,
                  1,
                  "Get PPC driver version")
{
    STEST_HEADER(1, 1, "Usage: TestDevice.PortPolicyCtrl.GetPpcDriverVersion(Array)\n");
    STEST_PRIVATE(JsPortPolicyCtrl, pJsPortPolicyCtrl, "PortPolicyCtrl");
    STEST_ARG(0, JSObject*, pArrayToReturn);

    RC rc;
    string driverVersion;
    C_CHECK_RC(pJsPortPolicyCtrl->GetPpcDriverVersion(&driverVersion));
    C_CHECK_RC(pJavaScript->SetElement(pArrayToReturn, 0, driverVersion));

    RETURN_RC(rc);
}

JS_STEST_LWSTOM(JsPortPolicyCtrl,
                GetFtbFwVersion,
                1,
                "Get funtional test board FW version")
{
    STEST_HEADER(1, 1, "Usage: TestDevice.PortPolicyCtrl.DoGetFtbFwVersion(Array)\n");
    STEST_PRIVATE(JsPortPolicyCtrl, pJsPortPolicyCtrl, "PortPolicyCtrl");
    STEST_ARG(0, JSObject*, pArrayToReturn);

    StickyRC stickyRc;
    string ftbFwVersion;
    stickyRc = pJsPortPolicyCtrl->GetFtbFwVersion(&ftbFwVersion);
    if (!ftbFwVersion.empty())
    {
        stickyRc = pJavaScript->SetElement(pArrayToReturn, 0, ftbFwVersion);
    }

    RETURN_RC(stickyRc);
}
//-----------------------------------------------------------------------------
JS_CLASS(PortPolicyCtrlConst);
static SObject PortPolicyCtrlConst_Object
(
    "PortPolicyCtrlConst",
    PortPolicyCtrlConstClass,
    0,
    0,
    "PortPolicyCtrlConst JS Object"
);

PROP_CONST(PortPolicyCtrlConst, USB_LW_TEST_ALT_MODE,           PortPolicyCtrl::USB_LW_TEST_ALT_MODE);
PROP_CONST(PortPolicyCtrlConst, USB_ALT_MODE_3,                 PortPolicyCtrl::USB_ALT_MODE_3);
PROP_CONST(PortPolicyCtrlConst, USB_ENABLE_DP_LOOPBACK,         PortPolicyCtrl::USB_ENABLE_DP_LOOPBACK);
PROP_CONST(PortPolicyCtrlConst, USB_UNKNOWN_ALT_MODE,           PortPolicyCtrl::USB_UNKNOWN_ALT_MODE);
PROP_CONST(PortPolicyCtrlConst, USB_UNKNOWN_POWER_MODE,         PortPolicyCtrl::USB_UNKNOWN_POWER_MODE);
PROP_CONST(PortPolicyCtrlConst, USB_DEFAULT_POWER_MODE,         PortPolicyCtrl::USB_DEFAULT_POWER_MODE);
PROP_CONST(PortPolicyCtrlConst, USB_UNKNOWN_ORIENTATION,        PortPolicyCtrl::USB_UNKNOWN_ORIENTATION);
PROP_CONST(PortPolicyCtrlConst, USB_UNKNOWN_DP_LOOPBACK_STATUS, PortPolicyCtrl::USB_UNKNOWN_DP_LOOPBACK_STATUS);
PROP_CONST(PortPolicyCtrlConst, USB_UNKNOWN_ISOC_DEVICE_STATUS, PortPolicyCtrl::USB_UNKNOWN_ISOC_DEVICE_STATUS);
