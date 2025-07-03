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

#include "js_lwlink.h"
#include "core/include/js_uint64.h"
#include "core/include/jscript.h"
#include "core/include/mgrmgr.h"
#include "core/include/script.h"
#include "device/interface/lwlink/lwlfla.h"
#include "device/interface/lwlink/lwlinkuphy.h"
#include "device/interface/lwlink/lwlpowerstatecounters.h"
#include "device/interface/lwlink/lwlregs.h"
#include "device/interface/lwlink/lwltrex.h"
#include "gpu/include/testdevice.h"
#include "gpu/include/testdevicemgr.h"
#include "gpu/reghal/js_reghal.h"

//-----------------------------------------------------------------------------
JsLwLink::JsLwLink(LwLink* pLwLink)
: m_pLwLink(pLwLink)
{
    MASSERT(m_pLwLink);
}

//-----------------------------------------------------------------------------
static void C_JsLwLink_finalize
(
    JSContext *cx,
    JSObject *obj
)
{
    MASSERT(cx != 0);
    MASSERT(obj != 0);

    JsLwLink * pJsLwLink;
    //! Delete the C++
    pJsLwLink = (JsLwLink *)JS_GetPrivate(cx, obj);
    delete pJsLwLink;
};

//-----------------------------------------------------------------------------
static JSClass JsLwLink_class =
{
    "LwLink",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ColwertStub,
    C_JsLwLink_finalize
};

//-----------------------------------------------------------------------------
static SObject JsLwLink_Object
(
    "LwLink",
    JsLwLink_class,
    0,
    0,
    "LwLink JS Object"
);

//-----------------------------------------------------------------------------
RC JsLwLink::CreateJSObject(JSContext* cx, JSObject* obj)
{
    RC rc;

    //! Only create one JSObject per Perf object
    if (m_pJsLwLinkObj)
    {
        Printf(Tee::PriLow,
               "A JS Object has already been created for this JsLwLink.\n");
        return OK;
    }

    m_pJsLwLinkObj = JS_DefineObject(cx,
                                  obj, // GpuSubdevice object
                                  "LwLink", // Property name
                                  &JsLwLink_class,
                                  JsLwLink_Object.GetJSObject(),
                                  JSPROP_READONLY);

    if (!m_pJsLwLinkObj)
        return RC::COULD_NOT_CREATE_JS_OBJECT;

    JsRegHal* pJsRegHal = new JsRegHal(&(m_pLwLink->GetInterface<LwLinkRegs>()->Regs()));
    MASSERT(pJsRegHal);
    CHECK_RC(pJsRegHal->CreateJSObject(cx, m_pJsLwLinkObj));
    SetJsRegHal(pJsRegHal);

    //! Store the current JsLwLink instance into the private area
    //! of the new JSOBject.
    if (JS_SetPrivate(cx, m_pJsLwLinkObj, this) != JS_TRUE)
    {
        Printf(Tee::PriNormal,
               "Unable to set private value of JsLwLink.\n");
        return RC::COULD_NOT_CREATE_JS_OBJECT;
    }

    // Add the .Help() to this JSObject
    JS_DefineFunction(cx, m_pJsLwLinkObj, "Help", &C_Global_Help, 1, 0);

    return OK;
}

//-----------------------------------------------------------------------------
bool JsLwLink::AreLanesReversed(UINT32 linkId)
{
    return m_pLwLink->AreLanesReversed(linkId);
}

//-----------------------------------------------------------------------------
RC JsLwLink::ClearErrorCounts(UINT32 linkId)
{
    RC rc;
    if (m_pLwLink->IsLinkActive(linkId))
    {
        CHECK_RC(m_pLwLink->ClearErrorCounts(linkId));
    }
    return OK;
}

//-----------------------------------------------------------------------------
RC JsLwLink::ClearLPEntryCount(UINT32 linkId)
{
    RC rc;
    if (m_pLwLink->SupportsInterface<LwLinkPowerStateCounters>())
    {
        if (m_pLwLink->IsLinkActive(linkId))
        {
            auto psCnt = m_pLwLink->GetInterface<LwLinkPowerStateCounters>();
            CHECK_RC(psCnt->ClearLPCounts(linkId));
        }
    }
    return OK;
}

//-----------------------------------------------------------------------------
RC JsLwLink::GetEnableUphyLogging(bool * pbEnableUphyLogging)
{
    if (!m_pLwLink->SupportsInterface<LwLinkUphy>())
    {
        Printf(Tee::PriError, "LwLink device does not support UPHY logging\n");
        return RC::UNSUPPORTED_DEVICE;
    }

    *pbEnableUphyLogging = m_pLwLink->GetInterface<LwLinkUphy>()->GetRegLogEnabled();
    return RC::OK;
}

//-----------------------------------------------------------------------------
RC JsLwLink::EnablePerLaneErrorCounts(bool bEnable)
{
    return m_pLwLink->EnablePerLaneErrorCounts(bEnable);
}

//-----------------------------------------------------------------------------
UINT32 JsLwLink::GetCCICount()
{

    if (m_pLwLink->SupportsInterface<LwLinkCableController>())
    {
        return m_pLwLink->GetInterface<LwLinkCableController>()->GetCount();
    }
    return 0;
}

//-----------------------------------------------------------------------------
RC JsLwLink::GetCCIErrorFlags(LwLinkCableController::CCErrorFlags * pErrorFlags)
{
    RC rc;
    MASSERT(pErrorFlags);
    if (m_pLwLink->SupportsInterface<LwLinkCableController>())
    {
        CHECK_RC(m_pLwLink->GetInterface<LwLinkCableController>()->GetErrorFlags(pErrorFlags));
    }
    return RC::OK;
}

//-----------------------------------------------------------------------------
const char * JsLwLink::GetCCIErrorFlagString(UINT08 cciErrorFlag)
{
    return LwLinkCableController::ErrorFlagToString(static_cast<LwLinkCableController::ErrorFlagType>(cciErrorFlag));
}

string JsLwLink::GetCCIHwRevision(UINT32 cciId)
{
    if (m_pLwLink->SupportsInterface<LwLinkCableController>())
        return m_pLwLink->GetInterface<LwLinkCableController>()->GetHwRevision(cciId);
    return "Unknown";
}

//-----------------------------------------------------------------------------
RC JsLwLink::GetCCILinks(UINT32 cciId, vector<UINT32> *pCciLinks)
{
    RC rc;
    MASSERT(pCciLinks);
    if (m_pLwLink->SupportsInterface<LwLinkCableController>())
    {
        const UINT64 ccLinkMask =
            m_pLwLink->GetInterface<LwLinkCableController>()->GetLinkMask(cciId);
        INT32 lwrLink = Utility::BitScanForward(ccLinkMask);
        while (lwrLink != -1)
        {
            pCciLinks->push_back(static_cast<UINT32>(lwrLink));
            lwrLink = Utility::BitScanForward(ccLinkMask, lwrLink + 1);
        }
    }
    return RC::OK;
}

string JsLwLink::GetCCIPartNumber(UINT32 cciId)
{
    if (m_pLwLink->SupportsInterface<LwLinkCableController>())
        return m_pLwLink->GetInterface<LwLinkCableController>()->GetPartNumber(cciId);
    return "Unknown";
}

//-----------------------------------------------------------------------------
UINT32 JsLwLink::GetCCIPhysicalId(UINT32 cciId)
{
    if (m_pLwLink->SupportsInterface<LwLinkCableController>())
    {
        return m_pLwLink->GetInterface<LwLinkCableController>()->GetPhysicalId(cciId);
    }
    return LwLinkCableController::ILWALID_CCI_ID;
}

string JsLwLink::GetCCISerialNumber(UINT32 cciId)
{
    if (m_pLwLink->SupportsInterface<LwLinkCableController>())
        return m_pLwLink->GetInterface<LwLinkCableController>()->GetSerialNumber(cciId);
    return "Unknown";
}

//-----------------------------------------------------------------------------
RC JsLwLink::GetFlaCapable(bool *pbFlaCapable)
{
    MASSERT(pbFlaCapable);
    *pbFlaCapable = false;
    if (m_pLwLink->SupportsInterface<LwLinkFla>())
    {
        *pbFlaCapable = m_pLwLink->GetInterface<LwLinkFla>()->GetFlaCapable();
    }
    return OK;
}

//-----------------------------------------------------------------------------
RC JsLwLink::GetFlaEnabled(bool *pbFlaEnabled)
{
    MASSERT(pbFlaEnabled);
    *pbFlaEnabled = false;
    if (m_pLwLink->SupportsInterface<LwLinkFla>())
    {
        *pbFlaEnabled = m_pLwLink->GetInterface<LwLinkFla>()->GetFlaEnabled();
    }
    return OK;
}

//-----------------------------------------------------------------------------
RC JsLwLink::GetErrorCounts(UINT32 linkId, LwLink::ErrorCounts* pErrorCounts)
{
    RC rc;
    if (m_pLwLink->IsLinkActive(linkId))
    {
        CHECK_RC(m_pLwLink->GetErrorCounts(linkId, pErrorCounts));
    }
    return OK;
}

//-----------------------------------------------------------------------------
RC JsLwLink::GetErrorFlags(LwLink::ErrorFlags * pErrorFlags)
{
    return m_pLwLink->GetErrorFlags(pErrorFlags);
}

//-----------------------------------------------------------------------------
bool JsLwLink::GetIsUphyLoggingSupported() const
{
    return m_pLwLink->SupportsInterface<LwLinkUphy>();
}

//-----------------------------------------------------------------------------
UINT32 JsLwLink::GetLineRateMbps(UINT32 linkId)
{
    return m_pLwLink->GetLineRateMbps(linkId);
}

//-----------------------------------------------------------------------------
string JsLwLink::GetLinkGroupName()
{
    return m_pLwLink->GetLinkGroupName();
}

//-----------------------------------------------------------------------------
UINT32 JsLwLink::GetLinksPerGroup()
{
    return m_pLwLink->GetLinksPerGroup();
}

//-----------------------------------------------------------------------------
UINT32 JsLwLink::GetMaxLinkGroups()
{
    return m_pLwLink->GetMaxLinkGroups();
}

//-----------------------------------------------------------------------------
UINT32 JsLwLink::GetMaxLinks()
{
    return m_pLwLink->GetMaxLinks();
}

//-----------------------------------------------------------------------------
UINT32 JsLwLink::GetSublinkWidth(UINT32 linkId)
{
    return m_pLwLink->GetSublinkWidth(linkId);
}

//-----------------------------------------------------------------------------
UINT32 JsLwLink::GetTopologyId()
{
    return m_pLwLink->GetTopologyId();
}

//-----------------------------------------------------------------------------
bool JsLwLink::GetSupportsLPCounters()
{
    RC rc;
    JavaScriptPtr pJs;
    JSContext *cx;
    pJs->GetContext(&cx);

    if (m_pLwLink)
    {
        if (m_pLwLink->SupportsInterface<LwLinkPowerStateCounters>())
            return true;

        // The recieving end of a link on this device can ask for a counter from
        // the transmitting end on the connected device. Check below the
        // connected devices.
        UINT32 maxLinks = m_pLwLink->GetMaxLinks();
        for (UINT32 linkId = 0; maxLinks > linkId; ++linkId)
        {
            if (m_pLwLink->IsLinkActive(linkId))
            {
                LwLink::EndpointInfo ei;
                if (OK != (rc = m_pLwLink->GetRemoteEndpoint(linkId, &ei)))
                {
                    pJs->Throw(cx, rc, "Error getting remote endpoint");
                    return false;
                }
                TestDeviceMgr* pTestDeviceMgr = static_cast<TestDeviceMgr*>(DevMgrMgr::d_TestDeviceMgr);
                TestDevicePtr pRemDev;
                for (UINT32 devInst = 0; devInst < pTestDeviceMgr->NumDevices(); devInst++)
                {
                    TestDevicePtr pTestDevice;
                    if (OK != (rc = pTestDeviceMgr->GetDevice(devInst, &pTestDevice)))
                    {
                        pJs->Throw(cx, rc,
                                   "Error getting LwLink device from LwLink device manager");
                        return false;
                    }
                    if (ei.id == pTestDevice->GetId())
                    {
                        pRemDev = pTestDevice;
                        break;
                    }
                }
                if (pRemDev && pRemDev->SupportsInterface<LwLinkPowerStateCounters>())
                {
                    return true;
                }
            }
        }
    }
    return false;
}

//-----------------------------------------------------------------------------
UINT32 JsLwLink::GetUphyVersion()
{
    if (!m_pLwLink->SupportsInterface<LwLinkUphy>())
        return static_cast<UINT32>(UphyIf::Version::UNKNOWN);
    return static_cast<UINT32>(m_pLwLink->GetInterface<LwLinkUphy>()->GetVersion());
}

//-----------------------------------------------------------------------------
bool JsLwLink::IsCCIErrorFlagFatal(UINT08 cciErrorFlag)
{
    return LwLinkCableController::IsErrorFlagFatal(static_cast<LwLinkCableController::ErrorFlagType>(cciErrorFlag));
}

//-----------------------------------------------------------------------------
bool JsLwLink::IsLinkActive(UINT32 linkId)
{
    return m_pLwLink->IsLinkActive(linkId);
}

//-----------------------------------------------------------------------------
bool JsLwLink::IsLinkValid(UINT32 linkId)
{
    return m_pLwLink->IsLinkValid(linkId);
}

//----------------------------------------------------------------------------
RC JsLwLink::SetCollapsedResponses(UINT32 mask)
{
    return m_pLwLink->SetCollapsedResponses(mask);
}

//----------------------------------------------------------------------------
RC JsLwLink::SetLinkState(UINT32 linkId, UINT32 linkState)
{
    return m_pLwLink->SetLinkState(linkId, static_cast<LwLink::SubLinkState>(linkState));
}

//-----------------------------------------------------------------------------
RC JsLwLink::ReadUphyPadLaneReg(UINT32 link, UINT32 lane, UINT16 addr, UINT16 * pReadData)
{
    auto pLwLinkUphy = m_pLwLink->GetInterface<LwLinkUphy>();
    if (pLwLinkUphy == nullptr)
        return RC::UNSUPPORTED_FUNCTION;

    return pLwLinkUphy->ReadPadLaneReg(link, lane, addr, pReadData);
}

//-----------------------------------------------------------------------------
RC JsLwLink::SetEnableUphyLogging(bool bEnableUphyLogging)
{
    if (!m_pLwLink->SupportsInterface<LwLinkUphy>())
    {
        Printf(Tee::PriError, "LwLink device does not support UPHY logging\n");
        return RC::UNSUPPORTED_DEVICE;
    }

    m_pLwLink->GetInterface<LwLinkUphy>()->SetRegLogEnabled(bEnableUphyLogging);
    return RC::OK;
}

//-----------------------------------------------------------------------------
RC JsLwLink::SetNeaLoopbackLinkMask(UINT64 linkMask)
{
    m_pLwLink->SetNeaLoopbackLinkMask(linkMask);
    return RC::OK;
}

//-----------------------------------------------------------------------------
CLASS_PROP_READONLY(JsLwLink,  CCICount,               UINT32, "Get the number of cable controllers");             //$
CLASS_PROP_READONLY(JsLwLink,  LinkGroupName,          string, "Get the hardware name for a link group");          //$
CLASS_PROP_READONLY(JsLwLink,  LinksPerGroup,          UINT32, "Number of links per link group");                  //$
CLASS_PROP_READONLY(JsLwLink,  MaxLinkGroups,          UINT32, "Maximum number of lwlink groups on the device");   //$
CLASS_PROP_READONLY(JsLwLink,  MaxLinks,               UINT32, "Maximum number of lwlinks on the device");         //$
CLASS_PROP_READONLY(JsLwLink,  SupportsLPCounters,     bool,   "Returns true if the device supports LP counters"); //$
CLASS_PROP_READONLY(JsLwLink,  TopologyId,             UINT32, "Topology Id of this lwlink device");
CLASS_PROP_READONLY(JsLwLink,  FlaCapable,             bool,   "Get fabric linear addressing capability");         //$
CLASS_PROP_READONLY(JsLwLink,  FlaEnabled,             bool,   "True if fabric linear addressing is enabled");     //$
CLASS_PROP_READONLY(JsLwLink,  IsUphyLoggingSupported, bool,   "True if uphy logging is supported");               //$
CLASS_PROP_READONLY(JsLwLink,  UphyVersion,            UINT32, "Version of the lwlink uphy");                      //$
CLASS_PROP_READWRITE(JsLwLink, EnableUphyLogging,      bool,   "True to enable UPHY logging (default = false");    //$

JS_SMETHOD_BIND_ONE_ARG(JsLwLink, AreLanesReversed,         linkId,   UINT32, "Check if the lanes are reversed on the specified link");
JS_SMETHOD_BIND_ONE_ARG(JsLwLink, ClearErrorCounts,         linkId,   UINT32, "Clear the LwLink error counters");
JS_SMETHOD_BIND_ONE_ARG(JsLwLink, ClearLPEntryCount,        linkId,   UINT32, "Clear the LwLink LP entries counter");
JS_SMETHOD_BIND_ONE_ARG(JsLwLink, EnablePerLaneErrorCounts, bEnable,  bool,   "Enable or disable per lane error counters");
JS_SMETHOD_BIND_ONE_ARG(JsLwLink, GetCCIErrorFlagString,    errFlag,  UINT08, "Get the string for a CCI error flag");
JS_SMETHOD_BIND_ONE_ARG(JsLwLink, GetCCIHwRevision,         cciId,    UINT32, "Get the cable controller hw revision");            //$
JS_SMETHOD_BIND_ONE_ARG(JsLwLink, GetCCIPartNumber,         cciId,    UINT32, "Get the cable controller part number");            //$
JS_SMETHOD_BIND_ONE_ARG(JsLwLink, GetCCIPhysicalId,         cciId,    UINT32, "Get the physicalId for the given CCI Id");
JS_SMETHOD_BIND_ONE_ARG(JsLwLink, GetCCISerialNumber,       cciId,    UINT32, "Get the cable controller serial number");          //$
JS_SMETHOD_BIND_ONE_ARG(JsLwLink, GetLineRateMbps,          linkId,   UINT32, "Get the rate of the lines in Mbps"); //$
JS_SMETHOD_BIND_ONE_ARG(JsLwLink, GetSublinkWidth,          linkId,   UINT32, "Get the sublink width of the link");
JS_SMETHOD_BIND_ONE_ARG(JsLwLink, IsCCIErrorFlagFatal,      errFlag,  UINT08, "Check if the specified CCI error flag is fatal");
JS_SMETHOD_BIND_ONE_ARG(JsLwLink, IsLinkActive,             linkId,   UINT32, "Check if the specified LinkId is active");
JS_SMETHOD_BIND_ONE_ARG(JsLwLink, IsLinkValid,              linkId,   UINT32, "Check if the specified LinkId is valid");
JS_STEST_BIND_ONE_ARG(  JsLwLink, SetCollapsedResponses,    mask,     UINT32, "Set the Collapsed Responses mask");
JS_STEST_BIND_TWO_ARGS( JsLwLink, SetLinkState,             linkId,   UINT32, state, UINT32, "Set the link state on the specified link"); //$

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(JsLwLink,
                  GetDefaultErrorThreshold,
                  2,
                  "Get the default error threshold for an error type")
{
    STEST_HEADER(2, 2, "TestDevice.LwLink.GetDefaultErrorThreshold(errId, bRateBased)\n");
    STEST_ARG(0, UINT32, nErrId);
    STEST_ARG(1, bool, bRateBased);
    STEST_PRIVATE(JsLwLink, pJsLwLink, "LwLink");
    RC rc;

    if (nErrId >= LwLink::ErrorCounts::LWL_NUM_ERR_COUNTERS)
    {
        pJavaScript->Throw(pContext, RC::BAD_PARAMETER,
                           Utility::StrPrintf("LwLink error ID out of range : %u", nErrId));
        return JS_FALSE;
    }
    LwLink::ErrorCounts::ErrId errId = static_cast<LwLink::ErrorCounts::ErrId>(nErrId);
    if (!LwLink::ErrorCounts::IsThreshold(errId))
    {
        pJavaScript->Throw(pContext, RC::BAD_PARAMETER,
                           Utility::StrPrintf("LwLink error ID %u does not have a threshold",
                                              nErrId));
        return JS_FALSE;
    }
    const FLOAT64 defThresh = pJsLwLink->GetLwLink()->GetDefaultErrorThreshold(errId, bRateBased);
    if (pJavaScript->ToJsval(defThresh, pReturlwalue) != RC::OK)
    {
        pJavaScript->Throw(pContext, RC::BAD_PARAMETER,
                           Utility::StrPrintf("Unable to colwert LwLink threshold to JS value"));
        return JS_FALSE;
    }
    return JS_TRUE;
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsLwLink,
                GetErrorCounts,
                3,
                "Read the LwLink error counters")
{
    STEST_HEADER(2, 3, "Usage:\n"
                       "var vals = new Array\n"
                       "TestDevice.LwLink.GetErrorCounts(linkId, vals, [overflows])\n");
    STEST_ARG(0, UINT32, linkId);
    STEST_ARG(1, JSObject*, pJsErrors);
    STEST_OPTIONAL_ARG(2, JSObject*, pJsOverflows, nullptr);
    STEST_PRIVATE(JsLwLink, pJsLwLink, "LwLink");
    RC rc;

    LwLink::ErrorCounts errCounts;
    C_CHECK_RC(pJsLwLink->GetErrorCounts(linkId, &errCounts));

    for (UINT32 nErrIdx = 0; nErrIdx < LwLink::ErrorCounts::LWL_NUM_ERR_COUNTERS; nErrIdx++)
    {
        LwLink::ErrorCounts::ErrId errIdx =
            static_cast<LwLink::ErrorCounts::ErrId>(nErrIdx);
        if (errCounts.IsValid(errIdx))
        {
            C_CHECK_RC(pJavaScript->SetElement(pJsErrors, errIdx,
                                               static_cast<UINT32>(errCounts.GetCount(errIdx))));
            if (pJsOverflows != nullptr)
            {
                C_CHECK_RC(pJavaScript->SetElement(pJsOverflows,
                                                   errIdx,
                                                   errCounts.DidCountOverflow(errIdx)));
            }
        }
    }

    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
JS_CLASS(Dummy);
namespace
{
    RC ErrorFlagDataToJs
    (
        JSContext * pContext,
        const map<UINT32, vector<string>> & errFlagData,
        string propString,
        JSObject *pRetObject
    )
    {
        RC rc;
        JavaScriptPtr pJavaScript;
        JSObject *pErrorFlagObj = JS_NewObject(pContext, &DummyClass, 0, 0);
        // Objects created with JS_NewObject do not have the root property by default
        // which means they can be garbage collected.  Flag the error object as a root
        // property until it is added to the return object (at which point it inherits
        // the root property from the return object) to prevent it from being garbage
        // collected
        if (JS_TRUE != JS_AddRoot(pContext, &pErrorFlagObj))
        {
            JS_ReportError(pContext, "Could not AddRoot on args in LwLink error flag object");
            return RC::COULD_NOT_CREATE_JS_OBJECT;
        }
        DEFER { JS_RemoveRoot(pContext, &pErrorFlagObj); };
        for (auto lwrErrors : errFlagData)
        {
            JsArray errStrings;
            for (auto lwrErrString : lwrErrors.second)
            {
                jsval jsString;
                CHECK_RC(pJavaScript->ToJsval(lwrErrString, &jsString));
                errStrings.push_back(jsString);
            }
            CHECK_RC(pJavaScript->SetElement(pErrorFlagObj, lwrErrors.first, &errStrings));
        }
        CHECK_RC(pJavaScript->SetProperty(pRetObject, propString.c_str(), pErrorFlagObj));
        return rc;
    }
}
JS_STEST_LWSTOM(JsLwLink,
                GetErrorFlags,
                1,
                "Read the LwLink error flags ")
{
    STEST_HEADER(1, 1, "Usage:\n"
                       "var vals = { }\n"
                 "TestDevice.LwLink.GetErrorFlags(vals)\n");
    STEST_ARG(0, JSObject*, pJsErrors);
    STEST_PRIVATE(JsLwLink, pJsLwLink, "LwLink");
    RC rc;
    LwLink::ErrorFlags errFlags;

    C_CHECK_RC(pJsLwLink->GetErrorFlags(&errFlags));
    C_CHECK_RC(ErrorFlagDataToJs(pContext, errFlags.linkErrorFlags, "linkErrorFlags", pJsErrors));
    C_CHECK_RC(ErrorFlagDataToJs(pContext,
                                 errFlags.linkGroupErrorFlags,
                                 "linkGroupErrorFlags",
                                 pJsErrors));
    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsLwLink,
                GetLinkState,
                2,
                "Retrieve the link state and rx/tx sublink state of the specified LwLink")
{
    STEST_HEADER(2, 2, "Usage:\n"
                       "var lwlState = { };\n"
                       "TestDevice.LwLink.GetLinkState(linkId, lwlState)\n");
    STEST_ARG(0, UINT32, linkId);
    STEST_ARG(1, JSObject*, pSublinkState);
    STEST_PRIVATE(JsLwLink, pJsLwLink, "LwLink");

    RC rc;
    LwLink* pLwLink = pJsLwLink->GetLwLink();
    if (linkId >= pLwLink->GetMaxLinks())
    {
        Printf(Tee::PriError, "Requested link state of invalid link %d > maximum link %d\n",
               linkId, pLwLink->GetMaxLinks());
        RETURN_RC(RC::SOFTWARE_ERROR);
    }

    vector<LwLink::LinkStatus> linkStatus;
    C_CHECK_RC(pLwLink->GetLinkStatus(&linkStatus));
    C_CHECK_RC(pJavaScript->SetProperty(pSublinkState, "linkState",
                                        linkStatus[linkId].linkState));
    C_CHECK_RC(pJavaScript->SetProperty(pSublinkState, "rxSubLinkState",
                                        linkStatus[linkId].rxSubLinkState));
    C_CHECK_RC(pJavaScript->SetProperty(pSublinkState, "txSubLinkState",
                                        linkStatus[linkId].txSubLinkState));

    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsLwLink,
                GetLPEntryCount,
                2,
                "Read the number of times LwLink went to the LP power state")
{
    STEST_HEADER(2, 2, "Usage:\n"
                       "var lpEntries = { };\n"
                       "TestDevice.LwLink.GetLPEntryCount(linkId, lpEntries);\n");
    STEST_ARG(0, UINT32, linkId);
    STEST_ARG(1, JSObject*, pJsLPentries);
    STEST_PRIVATE(JsLwLink, pJsLwLink, "LwLink");

    RC rc;

    C_CHECK_RC(pJavaScript->SetProperty(pJsLPentries, "Supported", false));
    C_CHECK_RC(pJavaScript->SetProperty(pJsLPentries, "Active", false));

    LwLink* pLwLink = pJsLwLink->GetLwLink();
    if (pLwLink->IsLinkActive(linkId))
    {
        C_CHECK_RC(pJavaScript->SetProperty(pJsLPentries, "Active", true));

        // counter for the transmitting end
        if (pLwLink->SupportsInterface<LwLinkPowerStateCounters>())
        {
            UINT32 txCount = 0;
            bool   bOverflow = false;
            auto psCnt = pLwLink->GetInterface<LwLinkPowerStateCounters>();
            C_CHECK_RC(psCnt->GetLPEntryOrExitCount(linkId, true, &txCount, &bOverflow));
            C_CHECK_RC(pJavaScript->SetProperty(pJsLPentries, "Counter", txCount));
            C_CHECK_RC(pJavaScript->SetProperty(pJsLPentries, "Supported", true));
            C_CHECK_RC(pJavaScript->SetProperty(pJsLPentries, "Overflow", bOverflow));
        }
    }

    RETURN_RC(rc);
}

//--------------------------------------------------------------------------
//! \brief Read a 32/64 bit register for the given domain and offset.
//!
//! \param linkId  : Link Id to read the register on
//! \param domain  : one of the known hw domains (DLPL/NTL/GLUE)
//! \param offset  : byte offset from within the domain
//! \param vals    : output array : index 0, the lower 32 bits to write
//!                  index 1, the upper 32 bits to write if required data access = 64bit
//!
//! \return OK if successful, BAD_PARAMETER if any one of the following conditions
//!         exist:
//!         -offset value is not a proper multiple of the data size (4 or 8)
//!         -invalid domain specified
//!         -invalid offset within a domain specified
//!         -null value for pDataHi if 64 bit access is required
//!         -null value for pDataLo
//! Note: If the domain requires 64 bit accesss then the contents of pDataLo and
//!       pDataHi will be combined to produce a 64bit value. Otherwise only the
//!       contents of pDataLo will be used.
JS_STEST_LWSTOM(JsLwLink,
                RegRd,
                4,
                "Read 32/64bit value from one of the LwLink HW blocks at offset x ")
{
    STEST_HEADER(4, 4, "Usage:\n"
                       "let vals = [];\n"
                       "TestDevice.LwLink.RegRd(linkId, domain, offset, vals);\n"
                       "//vals[0] = lower 32 bits, vals[1] = upper 32 bits\n" );
    STEST_ARG(0, UINT32, linkId);
    STEST_ARG(1, UINT32, domain);
    STEST_ARG(2, UINT32, offset);
    STEST_ARG(3, JSObject*, pJsVals);
    STEST_PRIVATE(JsLwLink, pJsLwLink, "LwLink");

    RC rc;
    LwLink* pLwLink = pJsLwLink->GetLwLink();

    UINT32 dataLo = 0x00;
    UINT32 dataHi = 0x00;
    //TODO: Add actual 64bit support to LwLink interface
    auto pLwLinkRegs = pLwLink->GetInterface<LwLinkRegs>();
    C_CHECK_RC(pLwLinkRegs->RegRd(linkId, static_cast<RegHalDomain>(domain), offset, &dataLo));//, &dataHi));
    C_CHECK_RC(pJavaScript->SetElement(pJsVals, 0, dataLo));
    C_CHECK_RC(pJavaScript->SetElement(pJsVals, 1, dataHi));

    RETURN_RC(OK);
}

//--------------------------------------------------------------------------
//! \brief Write a 32/64 bit register for the given domain and offset.
//!
//! \param linkId  : Link Id to write the register on
//! \param domain  : one of the known hw domains (DLPL/NTL/GLUE)
//! \param offset  : byte offset from within the domain
//! \param valLo   : the lower 32 bits to write
//! \param valHi   : (optional) the upper 32 bits to write if required data access = 64bit
//!
//! \return OK if successful, BAD_PARAMETER if any one of the following conditions
//!         exist:
//!         -offset value is not a proper multiple of the data size (4 or 8)
//!         -invalid domain specified
//!         -invalid offset within a domain specified
//! Note: If the domain requires 64 bit accesss then dataLo & dataHi will be combined
//! to produce a 64bit value. Otherwise only dataLo will be used.
JS_STEST_LWSTOM(JsLwLink,
                RegWr,
                5,
                "Write a 32/64bit value to one of the LwLink HW blocks at offset x ")
{
    STEST_HEADER(4, 5, "Usage: TestDevice.LwLink.RegWr(linkId, domain, offset, valLo, [valHi])");
    STEST_ARG(0, UINT32, linkId);
    STEST_ARG(1, UINT32, domain);
    STEST_ARG(2, UINT32, offset);
    STEST_ARG(3, UINT32, valLo);
    STEST_OPTIONAL_ARG(4, UINT32, valHi, 0x00); // valHi defaults to 0x00
    STEST_PRIVATE(JsLwLink, pJsLwLink, "LwLink");

    RC rc;
    LwLink* pLwLink = pJsLwLink->GetLwLink();

    //TODO: Add actual 64bit support to LwLink interface
    auto pLwLinkRegs = pLwLink->GetInterface<LwLinkRegs>();
    C_CHECK_RC(pLwLinkRegs->RegWr(linkId, static_cast<RegHalDomain>(domain), offset, valLo));//, valHi));

    RETURN_RC(OK);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsLwLink,
                ReadUphyPadLaneReg,
                4,
                "Read LwLink UPHY pad lane registers")
{
    STEST_HEADER(4, 4, "Usage:\n"
                       "var readData = []\n"
                       "TestDevice.LwLink.ReadUphyPadLaneReg(link, lane, addr, readData)\n");
    STEST_ARG(0, UINT32, link);
    STEST_ARG(1, UINT32, lane);
    STEST_ARG(2, UINT16, addr);
    STEST_ARG(3, JSObject*, pJsRegData);
    STEST_PRIVATE(JsLwLink, pJsLwLink, "LwLink");
    RC rc;

    UINT16 readData;
    C_CHECK_RC(pJsLwLink->ReadUphyPadLaneReg(link, lane, addr, &readData));
    C_CHECK_RC(pJavaScript->SetElement(pJsRegData, 0, readData));
    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsLwLink,
                IsTrunkLink,
                2,
                "Determine whether a link is a trunk link")
{
    STEST_HEADER(2, 2, "Usage:\n"
                       "var vals = []\n"
                       "TestDevice.LwLink.IsTrunkLink(linkId, vals)\n");
    STEST_ARG(0, UINT32, linkId);
    STEST_ARG(1, JSObject*, pJsTrunkLink);
    STEST_PRIVATE(JsLwLink, pJsLwLink, "LwLink");
    RC rc;

    bool bTrunkLink = false;
    LwLink* pLwLink = pJsLwLink->GetLwLink();
    if (pLwLink->IsLinkActive(linkId))
    {
        LwLink::EndpointInfo ei;
        C_CHECK_RC(pLwLink->GetRemoteEndpoint(linkId, &ei));
        bTrunkLink = ei.devType == TestDevice::TYPE_LWIDIA_LWSWITCH;
        C_CHECK_RC(pJavaScript->SetElement(pJsTrunkLink, 0, bTrunkLink));
    }
    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsLwLink,
                SetNeaLoopbackLinkMask,
                1,
                "Set the specified links in Nea Loopback mode")
{
    STEST_HEADER(1, 1, "Usage: TestDevice.LwLink.SetNeaLoopbackLinkMask(linkMask)\n"
                       "   linkMask = JS UINT64 object\n");
    STEST_ARG(0, JSObject*, pJsLinkMaskObj);
    STEST_PRIVATE(JsLwLink, pJsLwLink, "LwLink");

    JSClass* pJsClass = JS_GetClass(pJsLinkMaskObj);
    if (!(pJsClass->flags & JSCLASS_HAS_PRIVATE) || strcmp(pJsClass->name, "UINT64") != 0)
    {
        Printf(Tee::PriError, "SetNeaLoopbackLinkMask expects a UINT64 JS object as a parameter\n");
        RETURN_RC(RC::BAD_PARAMETER);
    }

    JsUINT64* pJsUINT64 = static_cast<JsUINT64*>(JS_GetPrivate(pContext, pJsLinkMaskObj));

    RC rc;
    C_CHECK_RC(pJsLwLink->SetNeaLoopbackLinkMask(pJsUINT64->GetValue()));
    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsLwLink,
                SetNedLoopbackLinkMask,
                1,
                "Set the specified links in Ned Loopback mode")
{
    STEST_HEADER(1, 1, "Usage: TestDevice.LwLink.SetNedLoopbackLinkMask(linkMask)\n"
                       "   linkMask = JS UINT64 object\n");
    STEST_ARG(0, JSObject*, pJsLinkMaskObj);
    STEST_PRIVATE(JsLwLink, pJsLwLink, "LwLink");

    JSClass* pJsClass = JS_GetClass(pJsLinkMaskObj);
    if (!(pJsClass->flags & JSCLASS_HAS_PRIVATE) || strcmp(pJsClass->name, "UINT64") != 0)
    {
        Printf(Tee::PriError, "SetNedLoopbackLinkMask expects a UINT64 JS object as a parameter\n");
        RETURN_RC(RC::BAD_PARAMETER);
    }

    JsUINT64* pJsUINT64 = static_cast<JsUINT64*>(JS_GetPrivate(pContext, pJsLinkMaskObj));

    pJsLwLink->GetLwLink()->SetNedLoopbackLinkMask(pJsUINT64->GetValue());
    RETURN_RC(RC::OK);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsLwLink,
                SetUphyRegLogLinkMask,
                1,
                "Set the link mask to log uphy values for")
{
    STEST_HEADER(1, 1, "Usage: TestDevice.LwLink.SetUphyRegLogLinkMask(linkMask)\n"
                       "   linkMask = JS UINT64 object\n");
    STEST_ARG(0, JSObject*, pJsLinkMaskObj);
    STEST_PRIVATE(JsLwLink, pJsLwLink, "LwLink");

    JSClass* pJsClass = JS_GetClass(pJsLinkMaskObj);
    if (!(pJsClass->flags & JSCLASS_HAS_PRIVATE) || strcmp(pJsClass->name, "UINT64") != 0)
    {
        Printf(Tee::PriError, "SetUphyRegLogLinkMask expects a UINT64 JS object as a parameter\n");
        RETURN_RC(RC::BAD_PARAMETER);
    }

    JsUINT64* pJsUINT64 = static_cast<JsUINT64*>(JS_GetPrivate(pContext, pJsLinkMaskObj));
    RC rc;

    LwLink* pLwLink = pJsLwLink->GetLwLink();
    if (!pLwLink->SupportsInterface<LwLinkUphy>())
    {
        Printf(Tee::PriError, "LwLink device does not support UPH logging\n");
        RETURN_RC(RC::UNSUPPORTED_DEVICE);
    }
    pLwLink->GetInterface<LwLinkUphy>()->SetRegLogLinkMask(pJsUINT64->GetValue());
    RETURN_RC(RC::OK);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsLwLink,
                DumpRegs,
                1,
                "Dump the registers on a link")
{
    STEST_HEADER(1, 1, "Usage: TestDevice.LwLink.DumpRegs(linkId)");
    STEST_PRIVATE(JsLwLink, pJsLwLink, "LwLink");
    STEST_ARG(0, UINT32, linkId);

    RETURN_RC(pJsLwLink->GetLwLink()->DumpRegs(linkId));
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsLwLink,
                GetRemoteLinkStrings,
                1,
                "Get the strings for the remote connection of each link")
{
    STEST_HEADER(1, 1, "Usage: TestDevice.LwLink.GetRemoteLinkInfo(remoteLinkInfoArray)\n");
    STEST_ARG(0, JSObject*, pJsRemoteLinkInfoObj);
    STEST_PRIVATE(JsLwLink, pJsLwLink, "LwLink");

    RC rc;
    LwLink * pLwLink = pJsLwLink->GetLwLink();
    TestDeviceMgr* pTestDeviceMgr = static_cast<TestDeviceMgr*>(DevMgrMgr::d_TestDeviceMgr);
    for (UINT32 link = 0; link < pLwLink->GetMaxLinks(); link++)
    {
        string remoteLinkStr = "Unknown";
        if (pLwLink->IsPostDiscovery())
        {
            if (pLwLink->IsLinkValid(link))
            {
                if (pLwLink->IsLinkActive(link))
                {
                    LwLink::EndpointInfo ep;
                    C_CHECK_RC(pLwLink->GetRemoteEndpoint(link, &ep));

                    if (ep.devType != TestDevice::TYPE_UNKNOWN)
                    {
                        if (ep.devType != TestDevice::TYPE_TREX)
                        {
                            TestDevicePtr pTestDevice;
                            C_CHECK_RC(pTestDeviceMgr->GetDevice(ep.id, &pTestDevice));

                            remoteLinkStr = pTestDevice->GetName();
                        }
                        else
                        {
                            remoteLinkStr = "TREX";
                        }
                        remoteLinkStr += Utility::StrPrintf(", Link %d", ep.linkNumber);
                    }
                    else
                    {
                        remoteLinkStr = "Not Connected";
                    }
                }
                else
                {
                    remoteLinkStr = "Inactive";
                }
            }
            else
            {
                remoteLinkStr = "Invalid";
            }
        }
        jsval jsString;
        C_CHECK_RC(pJavaScript->ToJsval(remoteLinkStr, &jsString));
        C_CHECK_RC(pJavaScript->SetElement(pJsRemoteLinkInfoObj, link, jsString));
    }
    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsLwLink,
                SetTrexDataPattern,
                1,
                "Set the TREX Data Pattern")
{
    STEST_HEADER(1, 1, "Usage: TestDevice.LwLink.SetTrexDataPattern(pattern)\n");
    STEST_ARG(0, UINT32, iPattern);
    STEST_PRIVATE(JsLwLink, pJsLwLink, "LwLink");

    if (!pJsLwLink->GetLwLink()->SupportsInterface<LwLinkTrex>())
        RETURN_RC(RC::UNSUPPORTED_DEVICE);

    LwLinkTrex::DataPattern pattern = static_cast<LwLinkTrex::DataPattern>(iPattern);
    LwLinkTrex* pTrex = pJsLwLink->GetLwLink()->GetInterface<LwLinkTrex>();

    RETURN_RC(pTrex->SetTrexDataPattern(pattern));
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsLwLink,
                SetTrexSrc,
                1,
                "Set the TREX NCISOC SRC")
{
    STEST_HEADER(1, 1, "Usage: TestDevice.LwLink.SetTrexSrc(src)\n");
    STEST_ARG(0, UINT32, iSrc);
    STEST_PRIVATE(JsLwLink, pJsLwLink, "LwLink");

    if (!pJsLwLink->GetLwLink()->SupportsInterface<LwLinkTrex>())
        RETURN_RC(RC::UNSUPPORTED_DEVICE);

    LwLinkTrex::NcisocSrc src = static_cast<LwLinkTrex::NcisocSrc>(iSrc);
    LwLinkTrex* pTrex = pJsLwLink->GetLwLink()->GetInterface<LwLinkTrex>();

    RETURN_RC(pTrex->SetTrexSrc(src));
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsLwLink,
                SetTrexDst,
                1,
                "Set the TREX NCISOC DST")
{
    STEST_HEADER(1, 1, "Usage: TestDevice.LwLink.SetTrexDst(dst)\n");
    STEST_ARG(0, UINT32, iDst);
    STEST_PRIVATE(JsLwLink, pJsLwLink, "LwLink");

    if (!pJsLwLink->GetLwLink()->SupportsInterface<LwLinkTrex>())
        RETURN_RC(RC::UNSUPPORTED_DEVICE);

    LwLinkTrex::NcisocDst dst = static_cast<LwLinkTrex::NcisocDst>(iDst);
    LwLinkTrex* pTrex = pJsLwLink->GetLwLink()->GetInterface<LwLinkTrex>();

    RETURN_RC(pTrex->SetTrexDst(dst));
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsLwLink,
                GetCCIErrorFlags,
                1,
                "Read the LwLink cable controller error flags ")
{
    STEST_HEADER(1, 1, "Usage:\n"
                       "var vals = { }\n"
                 "TestDevice.LwLink.GetCCIErrorFlags(vals)\n");
    STEST_ARG(0, JSObject*, pJsErrors);
    STEST_PRIVATE(JsLwLink, pJsLwLink, "LwLink");
    RC rc;
    LwLinkCableController::CCErrorFlags errFlags;

    C_CHECK_RC(pJsLwLink->GetCCIErrorFlags(&errFlags));

    for (UINT32 i = 0; i < errFlags.size(); i++)
    {
        JSObject* jsError = JS_NewObject(pContext, &DummyClass, NULL, NULL);
        C_CHECK_RC(pJavaScript->SetProperty(jsError, "cciId",     errFlags[i].cciId));
        C_CHECK_RC(pJavaScript->SetProperty(jsError, "lane",      errFlags[i].lane));
        C_CHECK_RC(pJavaScript->SetProperty(jsError, "errFlag",   static_cast<UINT08>(errFlags[i].errFlag)));
        C_CHECK_RC(pJavaScript->SetElement(pJsErrors, i, jsError));
    }
    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsLwLink,
                GetCCILinks,
                2,
                "Get the links associated with a cable controller id")
{
    STEST_HEADER(2, 2, "Usage:\n"
                       "let links = { }\n"
                 "TestDevice.LwLink.GetCCILinks(cciId, links)\n");
    STEST_ARG(0, UINT32, cciId);
    STEST_ARG(1, JSObject*, pJsCCLinks);
    STEST_PRIVATE(JsLwLink, pJsLwLink, "LwLink");
    RC rc;
    vector<UINT32> ccLinks;

    C_CHECK_RC(pJsLwLink->GetCCILinks(cciId, &ccLinks));

    for (INT32 idx = 0; idx < static_cast<INT32>(ccLinks.size()); ++idx)
    {
        C_CHECK_RC(pJavaScript->SetElement(pJsCCLinks, idx, ccLinks[idx]));
    }
    RETURN_RC(rc);
}

//------------------------------------------------------------------------------
// LwLinkTrex::ReductionOp constants
//-----------------------------------------------------------------------------
JS_CLASS(TrexReduceOp);
static SObject TrexReduceOp_Object
(
    "TrexReduceOp",
    TrexReduceOpClass,
    0,
    0,
    "TrexReduceOp JS Object"
);

PROP_CONST(TrexReduceOp, S16_FADD, LwLinkTrex::RO_S16_FADD);
PROP_CONST(TrexReduceOp, S16_FADD_MP, LwLinkTrex::RO_S16_FADD_MP);
PROP_CONST(TrexReduceOp, S16_FMIN, LwLinkTrex::RO_S16_FMIN);
PROP_CONST(TrexReduceOp, S16_FMAX, LwLinkTrex::RO_S16_FMAX);
PROP_CONST(TrexReduceOp, S32_IMIN, LwLinkTrex::RO_S32_IMIN);
PROP_CONST(TrexReduceOp, S32_IMAX, LwLinkTrex::RO_S32_IMAX);
PROP_CONST(TrexReduceOp, S32_IXOR, LwLinkTrex::RO_S32_IXOR);
PROP_CONST(TrexReduceOp, S32_IAND, LwLinkTrex::RO_S32_IAND);
PROP_CONST(TrexReduceOp, S32_IOR, LwLinkTrex::RO_S32_IOR);
PROP_CONST(TrexReduceOp, S32_IADD, LwLinkTrex::RO_S32_IADD);
PROP_CONST(TrexReduceOp, S32_FADD, LwLinkTrex::RO_S32_FADD);
PROP_CONST(TrexReduceOp, S64_IMIN, LwLinkTrex::RO_S64_IMIN);
PROP_CONST(TrexReduceOp, S64_IMAX, LwLinkTrex::RO_S64_IMAX);
PROP_CONST(TrexReduceOp, U16_FADD, LwLinkTrex::RO_U16_FADD);
PROP_CONST(TrexReduceOp, U16_FADD_MP, LwLinkTrex::RO_U16_FADD_MP);
PROP_CONST(TrexReduceOp, U16_FMIN, LwLinkTrex::RO_U16_FMIN);
PROP_CONST(TrexReduceOp, U16_FMAX, LwLinkTrex::RO_U16_FMAX);
PROP_CONST(TrexReduceOp, U32_IMIN, LwLinkTrex::RO_U32_IMIN);
PROP_CONST(TrexReduceOp, U32_IMAX, LwLinkTrex::RO_U32_IMAX);
PROP_CONST(TrexReduceOp, U32_IXOR, LwLinkTrex::RO_U32_IXOR);
PROP_CONST(TrexReduceOp, U32_IAND, LwLinkTrex::RO_U32_IAND);
PROP_CONST(TrexReduceOp, U32_IOR, LwLinkTrex::RO_U32_IOR);
PROP_CONST(TrexReduceOp, U32_IDD, LwLinkTrex::RO_U32_IADD);
PROP_CONST(TrexReduceOp, U64_IMIN, LwLinkTrex::RO_U64_IMIN);
PROP_CONST(TrexReduceOp, U64_IMAX, LwLinkTrex::RO_U64_IMAX);
PROP_CONST(TrexReduceOp, U64_IXOR, LwLinkTrex::RO_U64_IXOR);
PROP_CONST(TrexReduceOp, U64_IAND, LwLinkTrex::RO_U64_IAND);
PROP_CONST(TrexReduceOp, U64_IOR, LwLinkTrex::RO_U64_IOR);
PROP_CONST(TrexReduceOp, U64_IADD, LwLinkTrex::RO_U64_IADD);
PROP_CONST(TrexReduceOp, U64_FADD, LwLinkTrex::RO_U64_FADD);
