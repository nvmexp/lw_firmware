/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "js_pcie.h"
#include "core/include/jscript.h"
#include "core/include/script.h"
#include "device/interface/pcie/pcieuphy.h"
#include "gpu/utility/pcie/pexdev.h"

//-----------------------------------------------------------------------------
JsPcie::JsPcie(Pcie* pPcie)
: m_pPcie(pPcie)
{
    MASSERT(m_pPcie);
}

//-----------------------------------------------------------------------------
static void C_JsPcie_finalize
(
    JSContext *cx,
    JSObject *obj
)
{
    MASSERT(cx != 0);
    MASSERT(obj != 0);

    JsPcie * pJsPcie;
    //! Delete the C++
    pJsPcie = (JsPcie *)JS_GetPrivate(cx, obj);
    delete pJsPcie;
};

//-----------------------------------------------------------------------------
static JSClass JsPcie_class =
{
    "Pcie",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ColwertStub,
    C_JsPcie_finalize
};

//-----------------------------------------------------------------------------
static SObject JsPcie_Object
(
    "Pcie",
    JsPcie_class,
    0,
    0,
    "Pcie JS Object"
);

//-----------------------------------------------------------------------------
RC JsPcie::CreateJSObject(JSContext* cx, JSObject* obj)
{
    //! Only create one JSObject per Perf object
    if (m_pJsPcieObj)
    {
        Printf(Tee::PriLow,
               "A JS Object has already been created for this JsPcie.\n");
        return OK;
    }

    m_pJsPcieObj = JS_DefineObject(cx,
                                  obj, // device object
                                  "Pcie", // Property name
                                  &JsPcie_class,
                                  JsPcie_Object.GetJSObject(),
                                  JSPROP_READONLY);

    if (!m_pJsPcieObj)
        return RC::COULD_NOT_CREATE_JS_OBJECT;

    //! Store the current JsPcie instance into the private area
    //! of the new JSOBject.
    if (JS_SetPrivate(cx, m_pJsPcieObj, this) != JS_TRUE)
    {
        Printf(Tee::PriNormal,
               "Unable to set private value of JsPcie.\n");
        return RC::COULD_NOT_CREATE_JS_OBJECT;
    }

    // Add the .Help() to this JSObject
    JS_DefineFunction(cx, m_pJsPcieObj, "Help", &C_Global_Help, 1, 0);

    return OK;
}

//-----------------------------------------------------------------------------
bool JsPcie::GetAerEnabled() const
{
    return m_pPcie->AerEnabled();
}

//-----------------------------------------------------------------------------
RC JsPcie::GetAspmCyaL1SubState(UINT32* pMask)
{
    return m_pPcie->GetAspmCyaL1SubState(pMask);
}

//-----------------------------------------------------------------------------
UINT32 JsPcie::GetAspmCyaState() const
{
    return m_pPcie->GetAspmCyaState();
}

//-----------------------------------------------------------------------------
RC JsPcie::GetAspmL1ssEnabled(UINT32* pMask)
{
    return m_pPcie->GetAspmL1ssEnabled(pMask);
}

//-----------------------------------------------------------------------------
UINT32 JsPcie::GetAspmCap() const
{
    return m_pPcie->AspmCapability();
}

//-----------------------------------------------------------------------------
UINT32 JsPcie::GetAspmState() const
{
    return m_pPcie->GetAspmState();
}

//-----------------------------------------------------------------------------
UINT32 JsPcie::GetBusNumber() const
{
    return m_pPcie->BusNumber();
}

//-----------------------------------------------------------------------------
UINT32 JsPcie::GetDeviceId() const
{
    return m_pPcie->DeviceId();
}

//-----------------------------------------------------------------------------
UINT32 JsPcie::GetDeviceNumber() const
{
    return m_pPcie->DeviceNumber();
}

//-----------------------------------------------------------------------------
UINT32 JsPcie::GetDomainNumber() const
{
    return m_pPcie->DomainNumber();
}

//-----------------------------------------------------------------------------
RC JsPcie::GetEnableUphyLogging(bool * pbEnableUphyLogging)
{
    if (!m_pPcie->SupportsInterface<PcieUphy>())
    {
        Printf(Tee::PriError, "Pcie device does not support UPHY logging\n");
        return RC::UNSUPPORTED_DEVICE;
    }

    *pbEnableUphyLogging = m_pPcie->GetInterface<PcieUphy>()->GetRegLogEnabled();
    return RC::OK;
}

//-----------------------------------------------------------------------------
RC JsPcie::GetErrorCounts
(
    PexErrorCounts            *pLocCounts,
    PexErrorCounts            *pHostCounts,
    PexDevice::PexCounterType  countType
)
{
    return m_pPcie->GetErrorCounts(pLocCounts, pHostCounts, countType);
}

//-----------------------------------------------------------------------------
UINT32 JsPcie::GetFunctionNumber() const
{
    return m_pPcie->FunctionNumber();
}

//-----------------------------------------------------------------------------
bool JsPcie::GetIsUphyLoggingSupported() const
{
    return m_pPcie->SupportsInterface<PcieUphy>();
}

//-----------------------------------------------------------------------------
UINT32 JsPcie::GetLinkSpeed()
{
    return GetLinkSpeedExpected(Pci::SpeedUnknown);
}

//-----------------------------------------------------------------------------
UINT32 JsPcie::GetLinkSpeedExpected(UINT32 expectedSpeed)
{
    Pci::PcieLinkSpeed expected = static_cast<Pci::PcieLinkSpeed>(expectedSpeed);
    return static_cast<UINT32>(m_pPcie->GetLinkSpeed(expected));
}

//-----------------------------------------------------------------------------
UINT32 JsPcie::GetLinkSpeedCapability()
{
    return static_cast<UINT32>(m_pPcie->LinkSpeedCapability());
}

//-----------------------------------------------------------------------------
UINT32 JsPcie::GetLinkWidth()
{
    return m_pPcie->GetLinkWidth();
}

//-----------------------------------------------------------------------------
RC JsPcie::GetLinkWidths(UINT32* pLocWidth, UINT32* pHostWidth)
{
    return m_pPcie->GetLinkWidths(pLocWidth, pHostWidth);
}

//-----------------------------------------------------------------------------
RC JsPcie::GetLtrEnabled(bool* pVal)
{
    return m_pPcie->GetLTREnabled(pVal);
}

//-----------------------------------------------------------------------------
UINT32 JsPcie::GetSubsystemDeviceId()
{
    return m_pPcie->SubsystemDeviceId();
}

//-----------------------------------------------------------------------------
UINT32 JsPcie::GetSubsystemVendorId()
{
    return m_pPcie->SubsystemVendorId();
}

//-----------------------------------------------------------------------------
UINT32 JsPcie::GetUphyVersion()
{
    if (!m_pPcie->SupportsInterface<PcieUphy>())
        return static_cast<UINT32>(UphyIf::Version::UNKNOWN);
    return static_cast<UINT32>(m_pPcie->GetInterface<PcieUphy>()->GetVersion());
}

//-----------------------------------------------------------------------------
RC JsPcie::GetUpStreamInfo(PexDevice** pPexDev, UINT32* pPort)
{
    MASSERT(pPexDev);
    MASSERT(pPort);
    *pPexDev = nullptr;
    *pPort = ~0U;

    return m_pPcie->GetUpStreamInfo(pPexDev, pPort);
}

//-----------------------------------------------------------------------------
RC JsPcie::ResetErrorCounters()
{
    return m_pPcie->ResetErrorCounters();
}

//-----------------------------------------------------------------------------
RC JsPcie::SetAerEnabled(bool bEnabled)
{
    m_pPcie->EnableAer(bEnabled);
    return OK;
}

//-----------------------------------------------------------------------------
RC JsPcie::SetAspmState(UINT32 state)
{
    return m_pPcie->SetAspmState(state);
}

//-----------------------------------------------------------------------------
RC JsPcie::SetEnableUphyLogging(bool bEnableUphyLogging)
{
    if (!m_pPcie->SupportsInterface<PcieUphy>())
    {
        Printf(Tee::PriError, "Pcie device does not support UPHY logging\n");
        return RC::UNSUPPORTED_DEVICE;
    }

    m_pPcie->GetInterface<PcieUphy>()->SetRegLogEnabled(bEnableUphyLogging);
    return RC::OK;
}

//-----------------------------------------------------------------------------
RC JsPcie::SetLinkSpeed(UINT32 newSpeed)
{
    auto speed = static_cast<Pci::PcieLinkSpeed>(newSpeed);
    return m_pPcie->SetLinkSpeed(speed);
}

//-----------------------------------------------------------------------------
RC JsPcie::SetLinkWidth(UINT32 linkwidth)
{
    return m_pPcie->SetLinkWidth(linkwidth);
}

CLASS_PROP_READWRITE(JsPcie, AerEnabled,             bool,   "Aer enabled flag");
CLASS_PROP_READONLY( JsPcie, AspmCyaState,           UINT32, "GPU ASPM CYA - set per pstate");
CLASS_PROP_READONLY( JsPcie, AspmCap,                UINT32, "GPU ASPM level capability mask");
CLASS_PROP_READONLY( JsPcie, AspmState,              UINT32, "GPU ASPM level enabled");
CLASS_PROP_READONLY( JsPcie, BusNumber,              UINT32, "Bus number for this device");
CLASS_PROP_READONLY( JsPcie, DeviceId,               UINT32, "Device ID for this device");
CLASS_PROP_READONLY( JsPcie, DeviceNumber,           UINT32, "Device number for this device");
CLASS_PROP_READONLY( JsPcie, DomainNumber,           UINT32, "Domain number for this device");
CLASS_PROP_READONLY( JsPcie, FunctionNumber,         UINT32, "Function number for this device");
CLASS_PROP_READWRITE(JsPcie, LinkSpeed,              UINT32, "PCIE Link Speed. Valid values: (2500/5000/8000)");
CLASS_PROP_READONLY( JsPcie, LinkSpeedCapability,    UINT32, "PCIE Link Speed Capability for this device");
CLASS_PROP_READWRITE(JsPcie, LinkWidth,              UINT32, "PCIE Link Width. Valid values: (1/2/4/8/16)");
CLASS_PROP_READONLY( JsPcie, LtrEnabled,             bool,   "PCIE LTR Enablement state");
CLASS_PROP_READONLY( JsPcie, SubsystemDeviceId,      UINT32, "PCIE Subsystem device ID");
CLASS_PROP_READONLY( JsPcie, SubsystemVendorId,      UINT32, "PCIE Subsystem vendor ID");
CLASS_PROP_READONLY( JsPcie, IsUphyLoggingSupported, bool,   "True if uphy logging is supported");
CLASS_PROP_READONLY( JsPcie, UphyVersion,            UINT32, "Version of the pcie uphy");
CLASS_PROP_READWRITE(JsPcie, EnableUphyLogging,      bool,   "Enable PCIE UPHY register logging");

JS_SMETHOD_BIND_NO_ARGS(JsPcie, ResetErrorCounters, "Reset error counters");

JS_STEST_BIND_ONE_ARG(JsPcie, SetAspmState, state, UINT32, "Set ASPM state");

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM
(
    JsPcie,
    GetAspmL1ssEnabled,
    1,
    "Get a Pci.ASPM_SUB_* mask and cya mask of which L1 substates we are allowed to enter"
)
{
    STEST_HEADER(1, 1, "var mask = TestDevice.Pcie.GetAspmL1ssEnabled(object)\n");
    STEST_PRIVATE(JsPcie, pJsPcie, "Pcie");
    STEST_ARG(0, JSObject *, pAspmL1SS);

    RC rc;
    UINT32 mask = 0x0;
    C_CHECK_RC(pJsPcie->GetAspmL1ssEnabled(&mask));
    C_CHECK_RC(pJavaScript->SetProperty(pAspmL1SS, "mask", mask));
    C_CHECK_RC(pJsPcie->GetAspmCyaL1SubState(&mask));
    C_CHECK_RC(pJavaScript->SetProperty(pAspmL1SS, "cya", mask));
    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsPcie,
                GetErrorCounts,
                3,
                "Read PEX error counters")
{
    STEST_HEADER(2, 3,
            "Usage:\n"
            "var locErrors = new Array;\n"
            "var hostErrors = new Array;\n"
            "var testdevice = new TestDevice(0);\n"
            "TestDevice.Pcie.GetErrorCounts(locErrors, hostErrors, countType)\n");
    STEST_PRIVATE(JsPcie, pJsPcie, "Pcie");
    STEST_ARG(0, JSObject*, pRetLocErrors);
    STEST_ARG(1, JSObject*, pRetHostErrors);
    STEST_OPTIONAL_ARG(2, UINT32, countType, PexDevice::PEX_COUNTER_ALL);

    RC rc;

    PexErrorCounts locErrors;
    PexErrorCounts hostErrors;
    C_CHECK_RC(pJsPcie->GetErrorCounts(&locErrors,
                                       &hostErrors,
                                       static_cast<PexDevice::PexCounterType>(countType)));
    C_CHECK_RC(JsPexDev::PexErrorCountsToJs(pRetLocErrors, &locErrors));
    C_CHECK_RC(JsPexDev::PexErrorCountsToJs(pRetHostErrors, &hostErrors));
    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(JsPcie,
                  GetLinkSpeed,
                  1,
                  "PCIE Link Speed. Valid values: (2500/5000/8000)")
{
    STEST_HEADER(1, 1, "Usage: TestDevice.Pcie.GetLinkSpeed(expectedSpeed)");
    STEST_PRIVATE(JsPcie, pObj, "Pcie");
    STEST_ARG(0, UINT32, expectedSpeed);
    if (OK != pJavaScript->ToJsval(pObj->GetLinkSpeedExpected(expectedSpeed), pReturlwalue))
    {
        JS_ReportError(pContext,
                       "Error oclwrred in Pcie.GetLinkSpeed()");
        return JS_FALSE;
    }
    return JS_TRUE;
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsPcie,
                GetLinkWidths,
                1,
                "Read link width on both rcv and trx ends")
{
    STEST_HEADER(1, 1, "Usage: TestDevice.Pcie.GetLinkWidths(Array)\n");
    STEST_PRIVATE(JsPcie, pJsPcie, "Pcie");
    STEST_ARG(0, JSObject*, pArrayToReturn);

    RC rc;
    UINT32 locWidth, hostWidth;
    C_CHECK_RC(pJsPcie->GetLinkWidths(&locWidth, &hostWidth));
    C_CHECK_RC(pJavaScript->SetElement(pArrayToReturn, 0, locWidth));
    C_CHECK_RC(pJavaScript->SetElement(pArrayToReturn, 1, hostWidth));
    RETURN_RC(rc);
}

P_( JsPcie_Get_UpStreamPexDev);
static SProperty UpStreamPexDev
(
    JsPcie_Object,
    "UpStreamPexDev",
    0,
    0,
    JsPcie_Get_UpStreamPexDev,
    0,
    JSPROP_READONLY,
    "acquire the PexDev above - null if nothing"
);
P_( JsPcie_Get_UpStreamPexDev )
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    JavaScriptPtr pJs;
    JsPcie* pJsPcie = nullptr;
    if ((pJsPcie = JS_GET_PRIVATE(JsPcie, pContext, pObject, "Pcie")) != 0)
    {
        PexDevice* pPexDev = nullptr;
        UINT32 port = ~0U;
        if ((pJsPcie->GetUpStreamInfo(&pPexDev, &port) != OK) ||
            !pPexDev)
        {
            *pValue = JSVAL_NULL;
            return JS_TRUE;
        }

        JsPexDev* pJsPexDev = new JsPexDev();
        MASSERT(pJsPexDev);
        pJsPexDev->SetPexDev(pPexDev);
        if (pJsPexDev->CreateJSObject(pContext, pObject) != OK)
        {
            JS_ReportError(pContext, "Unable to create JsPexDev JSObject");
            *pValue = JSVAL_NULL;
            return JS_FALSE;
        }

        if (pJs->ToJsval(pJsPexDev->GetJSObject(), pValue) == OK)
        {
            return JS_TRUE;
        }
    }
    return JS_FALSE;
}

P_( JsPcie_Get_UpStreamIndex);
static SProperty UpStreamIndex
(
    JsPcie_Object,
    "UpStreamIndex",
    0,
    0,
    JsPcie_Get_UpStreamIndex,
    0,
    JSPROP_READONLY,
    "returns upstream device's port number at which the device is connected"
);
P_( JsPcie_Get_UpStreamIndex)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    JavaScriptPtr pJs;
    JsPcie* pJsPcie = nullptr;
    if ((pJsPcie = JS_GET_PRIVATE(JsPcie, pContext, pObject, "Pcie")) != 0)
    {
        PexDevice* pPexDev = nullptr;
        UINT32 port = ~0U;
        if ((pJsPcie->GetUpStreamInfo(&pPexDev, &port) != OK) ||
            !pPexDev)
        {
            *pValue = JSVAL_NULL;
            return JS_TRUE;
        }

        if (pJs->ToJsval(port, pValue) == OK)
        {
            return JS_TRUE;
        }
    }
    return JS_FALSE;
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(JsPcie,
                  GetAerLog,
                  0,
                  "Get the AER log")
{
    STEST_HEADER(0, 0, "Usage: TestDevice.Pcie.GetAerLog()");
    STEST_PRIVATE(JsPcie, pJsPcie, "Pcie");

    auto& pJs = pJavaScript; // For C_CHECK_RC_THROW

    RC rc;
    vector<PexDevice::AERLogEntry> entries;
    C_CHECK_RC_THROW(pJsPcie->GetPcie()->GetAerLog(&entries),
                     "Unable to retrieve AER log");

    C_CHECK_RC_THROW(JsPexDev::AerLogToJs(pContext, pReturlwalue, entries),
                     "Unable to colwert AER log to JS");

    return JS_TRUE;
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(JsPcie,
                  ClearAerLog,
                  0,
                  "Clear the AER log")
{
    STEST_HEADER(0, 0, "Usage: TestDevice.Pcie.ClearAerLog()");
    STEST_PRIVATE(JsPcie, pJsPcie, "Pcie");

    auto& pJs = pJavaScript; // For C_CHECK_RC_THROW

    RC rc;
    C_CHECK_RC_THROW(pJsPcie->GetPcie()->ClearAerLog(),
                     "Unable to clear AER log");

    return JS_TRUE;
}
