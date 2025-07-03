/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Headers from outside mods
#include "ctrl/ctrl2080.h"
#include "jsapi.h"

#include "gpu/perf/js_avfs.h"

#include "gpu/perf/perfutil.h"
#include "core/include/jscript.h"
#include "core/include/rc.h"
#include "core/include/script.h"
#include "core/include/utility.h"

JsAvfs::JsAvfs() :
     m_pAvfs(nullptr)
    ,m_pJsAvfsObject(nullptr)
{
}

static void C_JsAvfs_finalize
(
    JSContext *cx,
    JSObject *obj
)
{
    MASSERT(cx != 0);
    MASSERT(obj != 0);

    JsAvfs *pJsAvfs;
    pJsAvfs = (JsAvfs *)JS_GetPrivate(cx, obj);
    delete pJsAvfs;
}

static JSClass JsAvfs_class =
{
    "JsAvfs",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ColwertStub,
    C_JsAvfs_finalize
};

static SObject JsAvfs_Object
(
    "JsAvfs",
    JsAvfs_class,
    0,
    0,
    "AVFS JS object"
);

//------------------------------------------------------------------------------
//! \brief Create a JS Object representation of the current associated
//! Avfs object
RC JsAvfs::CreateJSObject(JSContext *cx, JSObject *obj)
{
    //! Only create one JSObject per Avfs object
    if (m_pJsAvfsObject)
    {
        Printf(Tee::PriLow,
               "A JS Object has already been created for this JsAvfs.\n");
        return OK;
    }

    m_pJsAvfsObject = JS_DefineObject(cx,
                                      obj, // GpuSubdevice object
                                      "Avfs", // Property name
                                      &JsAvfs_class,
                                      JsAvfs_Object.GetJSObject(),
                                      JSPROP_READONLY);

    if (!m_pJsAvfsObject)
        return RC::COULD_NOT_CREATE_JS_OBJECT;

    //! Store the current JsAvfs instance into the private area
    //! of the new JSOBject.
    if (JS_SetPrivate(cx, m_pJsAvfsObject, this) != JS_TRUE)
    {
        Printf(Tee::PriNormal,
               "Unable to set private value of JsAvfs.\n");
        return RC::COULD_NOT_CREATE_JS_OBJECT;
    }

    // Add the .Help() to this JSObject
    JS_DefineFunction(cx, m_pJsAvfsObject, "Help", &C_Global_Help, 1, 0);

    return OK;
}

Avfs *JsAvfs::GetAvfs()
{
    MASSERT(m_pAvfs);
    return m_pAvfs;
}

void JsAvfs::SetAvfs(Avfs *pAvfs)
{
    m_pAvfs = pAvfs;
}

RC JsAvfs::Initialize()
{
    return GetAvfs()->Initialize();
}

#define JSAVFS_GETSETPROP(rettype, funcname, helpmsg)                                        \
P_( JsAvfs_Get##funcname );                                                                  \
P_( JsAvfs_Set##funcname );                                                                  \
static SProperty Get##funcname                                                               \
(                                                                                            \
    JsAvfs_Object,                                                                           \
    #funcname,                                                                               \
    0,                                                                                       \
    0,                                                                                       \
    JsAvfs_Get##funcname,                                                                    \
    JsAvfs_Set##funcname,                                                                    \
    0,                                                                                       \
    helpmsg                                                                                  \
);                                                                                           \
P_( JsAvfs_Set##funcname )                                                                   \
{                                                                                            \
    MASSERT(pContext != 0);                                                                  \
    MASSERT(pValue   != 0);                                                                  \
    JavaScriptPtr pJavaScript;                                                               \
    JsAvfs *pJsAvfs;                                                                         \
    if ((pJsAvfs = JS_GET_PRIVATE(JsAvfs, pContext, pObject, "JsAvfs")) != 0)                \
    {                                                                                        \
        RC rc;                                                                               \
        Avfs* pAvfs = pJsAvfs->GetAvfs();                                                    \
        rettype RetVal;                                                                      \
        if (OK != pJavaScript->FromJsval(*pValue, &RetVal))                                  \
        {                                                                                    \
            JS_ReportError(pContext, "Failed to colwert Set"#funcname"");                    \
            return JS_FALSE;                                                                 \
        }                                                                                    \
        char msg[256];                                                                       \
        if ((rc = pAvfs->Set##funcname(RetVal))!= OK)                                        \
        {                                                                                    \
            sprintf(msg, "Error %d reading calling Set"#funcname"", (UINT32)rc);             \
            JS_ReportError(pContext, msg);                                                   \
            *pValue = JSVAL_NULL;                                                            \
            return JS_FALSE;                                                                 \
        }                                                                                    \
        return JS_TRUE;                                                                      \
    }                                                                                        \
    return JS_FALSE;                                                                         \
}                                                                                            \
P_( JsAvfs_Get##funcname )                                                                   \
{                                                                                            \
    MASSERT(pContext != 0);                                                                  \
    MASSERT(pValue   != 0);                                                                  \
    JavaScriptPtr pJavaScript;                                                               \
    JsAvfs *pJsAvfs;                                                                         \
    if ((pJsAvfs = JS_GET_PRIVATE(JsAvfs, pContext, pObject, "JsAvfs")) != 0)                \
    {                                                                                        \
        RC rc;                                                                               \
        Avfs* pAvfs = pJsAvfs->GetAvfs();                                                    \
        rettype RetVal;                                                                      \
        char msg[256];                                                                       \
        rc = pAvfs->Get##funcname(&RetVal);                                                  \
        if (RC::LWRM_NOT_SUPPORTED == rc)                                                    \
        {                                                                                    \
            RetVal = 0;                                                                      \
            rc.Clear();                                                                      \
        }                                                                                    \
        else if (rc != OK)                                                                   \
        {                                                                                    \
            sprintf(msg, "Error %d reading calling Get"#funcname"", (UINT32)rc);             \
            JS_ReportError(pContext, msg);                                                   \
            *pValue = JSVAL_NULL;                                                            \
            return JS_FALSE;                                                                 \
        }                                                                                    \
        if (OK != pJavaScript->ToJsval(RetVal, pValue))                                      \
        {                                                                                    \
            JS_ReportError(pContext, "Failed to colwert Get" #funcname"");                   \
            *pValue = JSVAL_NULL;                                                            \
            return JS_FALSE;                                                                 \
        }                                                                                    \
        return JS_TRUE;                                                                      \
    }                                                                                        \
    return JS_FALSE;                                                                         \
}

JSAVFS_GETSETPROP(UINT32, AdcInfoPrintPri, "Print priority for ADC info");

JS_STEST_BIND_NO_ARGS(JsAvfs, Initialize, "Initialize the mods AVFS class");

RC JsAvfs::AdcDeviceStatusToJsObject
(
    const LW2080_CTRL_CLK_ADC_DEVICE_STATUS &devStatus,
    JSObject *pJsStatus
)
{
    JavaScriptPtr pJavaScript;
    return pJavaScript->PackFields(
        pJsStatus, "BBBII",
        "type", devStatus.type,
        "overrideCode", devStatus.overrideCode,
        "sampledCode", devStatus.sampledCode,
        "actualVoltageuV", devStatus.actualVoltageuV,
        "correctedVoltageuV", devStatus.correctedVoltageuV);
}

JS_CLASS(LutVfEntry);

RC JsAvfs::LutVfEntryToJsObject
(
    const LW2080_CTRL_CLK_LUT_VF_ENTRY &lutVfEntry,
    JSObject *pJsLutVfEntry
)
{
    JavaScriptPtr pJavaScript;
    RC rc;

    if (lutVfEntry.type == LW2080_CTRL_CLK_NAFLL_DEVICE_TYPE_V10)
    {
        return pJavaScript->PackFields(
            pJsLutVfEntry, "BHH",
            "type",   lutVfEntry.type,
            "ndiv",   lutVfEntry.data.lut10.ndiv,
            "vfgain", lutVfEntry.data.lut10.vfgain);
    }
    else if (lutVfEntry.type == LW2080_CTRL_CLK_NAFLL_DEVICE_TYPE_V20)
    {
        return pJavaScript->PackFields(
            pJsLutVfEntry, "BHBB",
            "type",       lutVfEntry.type,
            "ndiv",       lutVfEntry.data.lut20.ndiv,
            "ndivOffset", lutVfEntry.data.lut20.ndivOffset,
            "dvcoOffset", lutVfEntry.data.lut20.dvcoOffset);
    }
    else if (lutVfEntry.type == LW2080_CTRL_CLK_NAFLL_DEVICE_TYPE_V30)
    {
        JsArray ndivOffsets;
        JsArray dvcoOffsets;
        for (UINT32 ii = 0; ii < LW2080_CTRL_CLK_NAFLL_LUT_VF_LWRVE_SEC_MAX; ii++)
        {
            jsval jsDom;
            rc = pJavaScript->ToJsval(lutVfEntry.data.lut30.ndivOffset[ii], &jsDom);
            ndivOffsets.push_back(jsDom);
            jsval jsDom2;
            rc = pJavaScript->ToJsval(lutVfEntry.data.lut30.dvcoOffset[ii], &jsDom2);
            dvcoOffsets.push_back(jsDom2);
        }

        return pJavaScript->PackFields(
            pJsLutVfEntry, "BHaaH",
            "type",             lutVfEntry.type,
            "ndiv",             lutVfEntry.data.lut30.ndiv,
            "ndivOffset",       ndivOffsets,
            "dvcoOffset",       dvcoOffsets,
            "cpmMaxNdivOffset", lutVfEntry.data.lut30.cpmMaxNdivOffset);
    }
    else
    {
        return pJavaScript->PackFields(
            pJsLutVfEntry, "BI",
            "type",  lutVfEntry.type,
            "value", lutVfEntry.data.value);
    }
}

RC JsAvfs::LutVfLwrveToJsObject
(
    JSContext *pContext,
    const LW2080_CTRL_CLK_LUT_VF_LWRVE &lutVfLwrve,
    UINT32 numEntries,
    JSObject *pJsLutVfLwrve
)
{
    RC rc;
    JavaScriptPtr pJavaScript;
    for (UINT32 i = 0; i < numEntries; ++i)
    {
        JSObject *pJsLutVfEntry = JS_NewObject(
            pContext, &LutVfEntryClass, nullptr, nullptr);
        CHECK_RC(JsAvfs::LutVfEntryToJsObject(
            lutVfLwrve.lutVfEntries[i], pJsLutVfEntry));
        CHECK_RC(pJavaScript->SetElement(
            pJsLutVfLwrve, (INT32)i, pJsLutVfEntry));
    }
    return rc;
}

RC JsAvfs::NafllDeviceStatusToJsObject
(
    JSContext *pContext,
    const LW2080_CTRL_CLK_NAFLL_DEVICE_STATUS &devStatus,
    UINT32 lutNumEntries,
    JSObject *pJsStatus
)
{
    RC rc;
    JavaScriptPtr pJavaScript;
    CHECK_RC(pJavaScript->PackFields(
        pJsStatus, "B",
        "type", devStatus.type));
    JSObject *pJsLutVfLwrve = JS_NewArrayObject(pContext, 0, nullptr);
    CHECK_RC(JsAvfs::LutVfLwrveToJsObject(
        pContext, devStatus.lutVfLwrve, lutNumEntries, pJsLutVfLwrve));
    CHECK_RC(pJavaScript->SetProperty(pJsStatus, "regime",
        Perf::Regime2080CtrlBitToRegimeSetting(devStatus.lwrrentRegimeId)));
    CHECK_RC(pJavaScript->SetProperty(pJsStatus, "dvcoMinFreqMHz",
        devStatus.dvcoMinFreqMHz));
    return pJavaScript->SetProperty(pJsStatus, "lutVfLwrve", pJsLutVfLwrve);
}

JS_CLASS(AdcDeviceStatus);

JS_STEST_LWSTOM(JsAvfs,
                GetAdcDeviceStatus,
                1,
                "Retrieve the status information from all ADCs on the chip")
{
    RC rc;
    STEST_HEADER(1, 1, "Usage: Avfs.GetAdcDeviceStatus({outAllAdcStatuses})");
    STEST_PRIVATE(JsAvfs, pJsAvfs, "JsAvfs");
    STEST_ARG(0, JSObject *, outAdcStatus);
    Avfs *pAvfs = pJsAvfs->GetAvfs();
    LW2080_CTRL_CLK_ADC_DEVICES_STATUS_PARAMS adcStatusParams = {};
    C_CHECK_RC(pAvfs->GetAdcDeviceStatus(&adcStatusParams));

    for (UINT32 i = 0; i < LW2080_CTRL_CLK_ADC_MAX_DEVICES; ++i)
    {
        UINT32 mask = 1 << i;
        // If no object present, push a 'null' into array
        if (!(adcStatusParams.super.objMask.super.pData[0] & mask))
        {
            C_CHECK_RC(pJavaScript->SetElement(outAdcStatus,
                                               (INT32)i,
                                               JSVAL_NULL));
            continue;
        }

        LW2080_CTRL_CLK_ADC_DEVICE_STATUS &devStatus = adcStatusParams.devices[i];
        JSObject *pStatus = JS_NewObject(pContext, &AdcDeviceStatusClass, nullptr, nullptr);
        C_CHECK_RC(JsAvfs::AdcDeviceStatusToJsObject(devStatus, pStatus));
        jsval tmp;
        C_CHECK_RC(pJavaScript->ToJsval(pStatus, &tmp));
        C_CHECK_RC(pJavaScript->SetElement(outAdcStatus, (INT32)i, tmp));
    }
    RETURN_RC(rc);
}

JS_STEST_LWSTOM(JsAvfs,
                GetAdcDeviceStatusViaId,
                2,
                "Retrieve ADC status for the given ADC ID")
{
    RC rc;
    STEST_HEADER(2, 2,
                 "Usage: Avfs.GetAdcDeviceStatusViaId(AdcId, {adcStatus})");
    STEST_PRIVATE(JsAvfs, pJsAvfs, "JsAvfs");
    STEST_ARG(0, UINT32, adcId);
    STEST_ARG(1, JSObject *, outAdcStatus);
    Avfs *pAvfs = pJsAvfs->GetAvfs();
    LW2080_CTRL_CLK_ADC_DEVICE_STATUS adcStatus = {};
    C_CHECK_RC(pAvfs->GetAdcDeviceStatusViaId((Avfs::AdcId)adcId, &adcStatus));
    RETURN_RC(JsAvfs::AdcDeviceStatusToJsObject(adcStatus, outAdcStatus));
}

JS_CLASS(NafllDeviceStatus);

JS_STEST_LWSTOM(JsAvfs,
                GetNafllDeviceStatus,
                1,
                "Retrieve NAFLL status for all NAFLL devices on the chip")
{
    RC rc;
    STEST_HEADER(1, 1,
                 "Usage: Avfs.GetNafllDeviceStatus([outAllNafllStatuses])");
    STEST_PRIVATE(JsAvfs, pJsAvfs, "JsAvfs");
    STEST_ARG(0, JSObject *, outNafllStatus);
    Avfs *pAvfs = pJsAvfs->GetAvfs();
    LW2080_CTRL_CLK_NAFLL_DEVICES_STATUS_PARAMS nafllStatusParams = {};
    C_CHECK_RC(pAvfs->GetNafllDeviceStatus(&nafllStatusParams));

    for (UINT32 i = 0; i < LW2080_CTRL_CLK_NAFLL_MAX_DEVICES; ++i)
    {
        UINT32 mask = 1 << i;
        if (!(nafllStatusParams.super.objMask.super.pData[0] & mask))
        {
            C_CHECK_RC(pJavaScript->SetElement(
                outNafllStatus, i, JSVAL_NULL));
            continue;
        }

        LW2080_CTRL_CLK_NAFLL_DEVICE_STATUS &devStatus = nafllStatusParams.devices[i];
        JSObject *pStatus = JS_NewObject(pContext, &NafllDeviceStatusClass, nullptr, nullptr);
        C_CHECK_RC(pJsAvfs->NafllDeviceStatusToJsObject(
            pContext, devStatus, pAvfs->GetLutNumEntries(), pStatus));
        jsval tmp;
        C_CHECK_RC(pJavaScript->ToJsval(pStatus, &tmp));
        C_CHECK_RC(pJavaScript->SetElement(outNafllStatus, i, tmp));
    }
    RETURN_RC(rc);
}

JS_STEST_LWSTOM(JsAvfs,
                GetNafllDomainFFRLimitMHz,
                1,
                "Retrieve NAFLL clock domain's FFR limiting frequency in MHz")
{
    RC rc;
    STEST_HEADER(1, 1,
        "Usage: Avfs.GetNafllDomainFFRLimitMHz([outNafllDomaintoFFRLimitMHz])");
    STEST_PRIVATE(JsAvfs, pJsAvfs, "JsAvfs");
    STEST_ARG(0, JSObject *, outNafllDomainLimitMHz);
    Avfs *pAvfs = pJsAvfs->GetAvfs();

    map<Gpu::ClkDomain, UINT32> clockDomainToFFRLimitMHz;
    pAvfs->GetNafllClockDomainFFRLimitMHz(&clockDomainToFFRLimitMHz);

    for (map<Gpu::ClkDomain, UINT32>::const_iterator domItr = clockDomainToFFRLimitMHz.begin();
         domItr != clockDomainToFFRLimitMHz.end(); ++domItr)
    {
        C_CHECK_RC(pJavaScript->SetElement(outNafllDomainLimitMHz, domItr->first, domItr->second));
    }
    RETURN_RC(rc);
}

JS_STEST_LWSTOM(JsAvfs,
                GetNafllDeviceStatusViaId,
                2,
                "Retrieve NAFLL status for the given NAFLL id")
{
    RC rc;
    STEST_HEADER(2, 2,
                 "Usage: Avfs.GetNafllDeviceStatusViaId(NafllId, {nafllStatus})");
    STEST_PRIVATE(JsAvfs, pJsAvfs, "JsAvfs");
    STEST_ARG(0, UINT32, nafllId);
    STEST_ARG(1, JSObject *, outNafllStatus);
    Avfs *pAvfs = pJsAvfs->GetAvfs();
    LW2080_CTRL_CLK_NAFLL_DEVICE_STATUS nafllStatus = {};
    C_CHECK_RC(pAvfs->GetNafllDeviceStatusViaId((Avfs::NafllId)nafllId, &nafllStatus));
    RETURN_RC(pJsAvfs->NafllDeviceStatusToJsObject(
        pContext, nafllStatus, pAvfs->GetLutNumEntries(), outNafllStatus));
}

JS_SMETHOD_LWSTOM(JsAvfs,
                  IsNafllClkDomain,
                  1,
                  "Return true if NAFLL supports the given clock domain")
{
    STEST_HEADER(1, 1,
                 "Usage: Avfs.IsNafllClkDomain(NafllId)");
    STEST_PRIVATE(JsAvfs, pJsAvfs, "JsAvfs");
    STEST_ARG(0, UINT32, clkDomain);
    Avfs *pAvfs = pJsAvfs->GetAvfs();
    if (OK != pJavaScript->ToJsval(pAvfs->IsNafllClkDomain((Gpu::ClkDomain)clkDomain),
                                   pReturlwalue))
    {
        JS_ReportError(pContext,
                       "Error oclwrred in Avfs.IsNafllClkDomain");
        return JS_FALSE;
    }
    return JS_TRUE;
}

JS_SMETHOD_LWSTOM(JsAvfs,
                  ClkDomToNafllId,
                  1,
                  "Colwert clock domain to NAFLL id")
{
    STEST_HEADER(1, 1, "Usage: var nafllId = subdev.Avfs.ClkDomToNafllId(clkDom)");
    STEST_PRIVATE(JsAvfs, pJsAvfs, "JsAvfs");
    STEST_ARG(0, UINT32, clkDom);
    Avfs *pAvfs = pJsAvfs->GetAvfs();
    Avfs::NafllId nafllId = pAvfs->ClkDomToNafllId(static_cast<Gpu::ClkDomain>(clkDom));
    if (OK != pJavaScript->ToJsval(static_cast<UINT32>(nafllId), pReturlwalue))
    {
        JS_ReportError(pContext, "Error oclwrred in subdev.Avfs.ClkDomToNafllId");
        return JS_FALSE;
    }
    return JS_TRUE;
}

JS_SMETHOD_LWSTOM(JsAvfs,
                  GetNafllClkDomains,
                  0,
                  "Return all NAFLL clock domains")
{
    STEST_HEADER(0, 0, "Usage: var doms = subdev.Avfs.GetNafllClkDomains()");
    STEST_PRIVATE(JsAvfs, pJsAvfs, "JsAvfs");
    Avfs* pAvfs = pJsAvfs->GetAvfs();
    vector<Gpu::ClkDomain> doms;
    pAvfs->GetNafllClkDomains(&doms);

    StickyRC rc;
    JsArray jsClkDoms;
    for (UINT32 ii = 0; ii < doms.size(); ii++)
    {
        jsval jsDom;
        rc = pJavaScript->ToJsval(doms[ii], &jsDom);
        jsClkDoms.push_back(jsDom);
    }
    rc = pJavaScript->ToJsval(&jsClkDoms, pReturlwalue);

    if (OK != rc)
    {
        JS_ReportError(pContext, "Error oclwred in Avfs.GetNafllClkDomains()");
        return JS_FALSE;
    }

    return JS_TRUE;
}

JS_SMETHOD_LWSTOM(JsAvfs,
                  GetAvailableAdcIds,
                  0,
                  "Return an array of available ADC ids")
{
    STEST_HEADER(0, 0,
                 "Usage: var allAdcIds = subdev.Avfs.GetAvailableAdcIds()");
    STEST_PRIVATE(JsAvfs, pJsAvfs, "JsAvfs");
    Avfs *pAvfs = pJsAvfs->GetAvfs();
    vector<Avfs::AdcId> availableAdcIds;
    pAvfs->GetAvailableAdcIds(&availableAdcIds);

    JsArray jsAvailableAdcIds;
    StickyRC rc;
    for (UINT32 i = 0; i < availableAdcIds.size(); ++i)
    {
        jsval tmp;
        rc = pJavaScript->ToJsval(availableAdcIds[i], &tmp);
        jsAvailableAdcIds.push_back(tmp);
    }
    rc = pJavaScript->ToJsval(&jsAvailableAdcIds, pReturlwalue);
    if (OK != rc)
    {
        JS_ReportError(pContext,
                       "Error oclwrred in GetAvailableAdcIds");
        return JS_FALSE;
    }
    return JS_TRUE;
}

JS_SMETHOD_LWSTOM(JsAvfs,
                  AdcIdToStr,
                  1,
                  "Colwert ADC Id to string")
{
    STEST_HEADER(1, 1, "Usage: var adcIdStr = subdev.Avfs.AdcIdToStr(adcId)");
    STEST_ARG(0, INT32, adcId);
    const char* adcIdStr = PerfUtil::AdcIdToStr(static_cast<Avfs::AdcId>(adcId));
    if (pJavaScript->ToJsval(adcIdStr, pReturlwalue) != RC::OK)
    {
        JS_ReportError(pContext, "Error oclwrred in subdev.Avfs.AdcIdToStr");
        return JS_FALSE;
    }
    return JS_TRUE;
}

JS_SMETHOD_LWSTOM(JsAvfs,
                  NafllIdToStr,
                  1,
                  "Colwert NAFLL Id to string")
{
    STEST_HEADER(1, 1, "Usage: var nafllIdStr = subdev.Avfs.NafllIdToStr(nafllId)");
    STEST_ARG(0, INT32, nafllId);
    const char* nafllIdStr = PerfUtil::NafllIdToStr(static_cast<Avfs::NafllId>(nafllId));
    if (pJavaScript->ToJsval(nafllIdStr, pReturlwalue) != RC::OK)
    {
        JS_ReportError(pContext, "Error oclwrred in subdev.Avfs.NafllIdToStr");
        return JS_FALSE;
    }
    return JS_TRUE;
}

JS_SMETHOD_LWSTOM(JsAvfs,
                  GetAvailableNafllIds,
                  0,
                  "Return an array of available ADC ids")
{
    STEST_HEADER(0, 0,
                 "Usage: var allNafllIds = subdev.Avfs.GetAvailableNafllIds()");
    STEST_PRIVATE(JsAvfs, pJsAvfs, "JsAvfs");
    Avfs *pAvfs = pJsAvfs->GetAvfs();
    vector<Avfs::NafllId> availableNafllIds;
    pAvfs->GetAvailableNafllIds(&availableNafllIds);

    JsArray jsAvailableNafllIds;
    StickyRC rc;
    for (UINT32 i = 0; i < availableNafllIds.size(); ++i)
    {
        jsval tmp;
        rc = pJavaScript->ToJsval(availableNafllIds[i], &tmp);
        jsAvailableNafllIds.push_back(tmp);
    }
    rc = pJavaScript->ToJsval(&jsAvailableNafllIds, pReturlwalue);
    if (OK != rc)
    {
        JS_ReportError(pContext,
                       "Error oclwrred in GetAvailableNafllIds");
        return JS_FALSE;
    }
    return JS_TRUE;
}

#define JS_CREATE_AVFS_TRANSLATE_API(funcName, inType, outType, help)          \
    JS_STEST_LWSTOM(JsAvfs,                                                    \
                    funcName,                                                  \
                    2,                                                         \
                    help)                                                      \
    {                                                                          \
        RC rc;                                                                 \
        STEST_HEADER(2, 2,                                                     \
                    "Usage: Avfs." #funcName "(id, [outDevIndex])");           \
        STEST_PRIVATE(JsAvfs, pJsAvfs, "JsAvfs");                              \
        STEST_ARG(0, UINT32, avfsId);                                          \
        STEST_ARG(1, JSObject *, outDevIndex);                                 \
        Avfs *pAvfs = pJsAvfs->GetAvfs();                                      \
        outType outVal;                                                        \
        C_CHECK_RC(pAvfs->funcName((inType)avfsId, &outVal));                  \
        jsval tmp;                                                             \
        C_CHECK_RC(pJavaScript->ToJsval((UINT32)outVal, &tmp));                \
        RETURN_RC(pJavaScript->SetElement(outDevIndex, 0, tmp));               \
    }

// Useful when used in conjuction with GetAdcDeviceStatus
JS_CREATE_AVFS_TRANSLATE_API(TranslateAdcIdToDevIndex,
                             Avfs::AdcId,
                             UINT32,
                             "Given an ADC id, return the device index");
// Useful when used with GetNafllDeviceStatus
JS_CREATE_AVFS_TRANSLATE_API(TranslateNafllIdToDevIndex,
                             Avfs::NafllId,
                             UINT32,
                             "Given an NAFLL id, return the NAFLL device index");
JS_CREATE_AVFS_TRANSLATE_API(TranslateNafllIdToClkDomain,
                             Avfs::NafllId,
                             Gpu::ClkDomain,
                             "Given an NAFLL id, return the Gpu clock domain");

JS_CLASS(AdcCalibrationState);

RC JsAvfs::AdcCalibrationStateToJsObject
(
    const Avfs::AdcCalibrationState &cal,
    JSObject *pJsCal
)
{
    JavaScriptPtr pJavaScript;
    switch (cal.adcCalType)
    {
        case LW2080_CTRL_CLK_ADC_CAL_TYPE_V10:
            return pJavaScript->PackFields(
                pJsCal, "Ibb",
                "type", cal.adcCalType,
                "slope", cal.v10.slope,
                "intercept", cal.v10.intercept);
        case LW2080_CTRL_CLK_ADC_CAL_TYPE_V20:
            return pJavaScript->PackFields(
                pJsCal, "Ibb",
                "type", cal.adcCalType,
                "offset", cal.v20.offset,
                "gain", cal.v20.gain);
        case Avfs::MODS_SHIM_ADC_CAL_TYPE_V30:
            return pJavaScript->PackFields(
                pJsCal, "IbbbbBBBBb",
                "type", cal.adcCalType,
                "offset", (cal.v30.offset & 0x7F) * ((cal.v30.offset & 0x80) ? -1 : 1),
                "gain", (cal.v30.gain & 0x7F) * ((cal.v30.gain & 0x80) ? -1 : 1),
                "coarseOffset", (cal.v30.coarseOffset & 0x7F) * ((cal.v30.coarseOffset & 0x80) ? -1 : 1), //$
                "coarseGain", (cal.v30.coarseGain & 0x7F) * ((cal.v30.coarseGain & 0x80) ? -1 : 1), //$
                "lowTempLowVoltErr", cal.v30.lowTempLowVoltErr,
                "lowTempHighVoltErr", cal.v30.lowTempHighVoltErr,
                "highTempLowVoltErr", cal.v30.highTempLowVoltErr,
                "highTempHighVoltErr", cal.v30.highTempHighVoltErr,
                "adcCodeCorrectionOffset", cal.v30.adcCodeCorrectionOffset);
        default:
            Printf(Tee::PriError, "Invalid calibration type\n");
            return RC::AVFS_ILWALID_ADC_CAL_TYPE;
    }
    return RC::OK;
}

RC JsAvfs::JsObjectToAdcCalibrationState
(
    JSObject *pJsCal,
    Avfs::AdcCalibrationState *cal
)
{
    RC rc;

    JavaScriptPtr pJs;
    jsval jval;

    if (pJs->GetPropertyJsval(pJsCal, "type", &jval) != RC::OK)
    {
        Printf(Tee::PriError, "ADC calibration type must be specified\n");
        return RC::AVFS_ILWALID_ADC_CAL_TYPE;
    }
    UINT32 calType;
    CHECK_RC(pJs->FromJsval(jval, &calType));

    switch (calType)
    {
        case LW2080_CTRL_CLK_ADC_CAL_TYPE_V10:
            cal->adcCalType = LW2080_CTRL_CLK_ADC_CAL_TYPE_V10;
            CHECK_RC(pJs->GetPropertyJsval(pJsCal, "slope", &jval));
            CHECK_RC(pJs->FromJsval(jval, &cal->v10.slope));
            CHECK_RC(pJs->GetPropertyJsval(pJsCal, "intercept", &jval));
            CHECK_RC(pJs->FromJsval(jval, &cal->v10.intercept));
            break;
        case LW2080_CTRL_CLK_ADC_CAL_TYPE_V20:
            cal->adcCalType = LW2080_CTRL_CLK_ADC_CAL_TYPE_V20;
            CHECK_RC(pJs->GetPropertyJsval(pJsCal, "offset", &jval));
            CHECK_RC(pJs->FromJsval(jval, &cal->v20.offset));
            CHECK_RC(pJs->GetPropertyJsval(pJsCal, "gain", &jval));
            CHECK_RC(pJs->FromJsval(jval, &cal->v20.gain));
            break;
        case Avfs::MODS_SHIM_ADC_CAL_TYPE_V30:
        {
            LwU8 signBit, mag;
            LwS8 tmp;

            cal->adcCalType = Avfs::MODS_SHIM_ADC_CAL_TYPE_V30;

            CHECK_RC(pJs->GetPropertyJsval(pJsCal, "offset", &jval));
            CHECK_RC(pJs->FromJsval(jval, &tmp));
            // Colwert from 2's complement to sign-magnitude because that is what RM expects
            signBit = tmp < 0 ? 0x80 : 0;
            mag = static_cast<LwU8>(signBit ? (~tmp + 1) : tmp);
            cal->v30.offset = signBit | mag;

            CHECK_RC(pJs->GetPropertyJsval(pJsCal, "gain", &jval));
            CHECK_RC(pJs->FromJsval(jval, &tmp));
            // Colwert from 2's complement to sign-magnitude because that is what RM expects
            signBit = tmp < 0 ? 0x80 : 0;
            mag = static_cast<LwU8>(signBit ? (~tmp + 1) : tmp);
            cal->v30.gain = signBit | mag;

            CHECK_RC(pJs->GetPropertyJsval(pJsCal, "coarseOffset", &jval));
            CHECK_RC(pJs->FromJsval(jval, &tmp));
            // Colwert from 2's complement to sign-magnitude because that is what RM expects
            signBit = tmp < 0 ? 0x80 : 0;
            mag = static_cast<LwU8>(signBit ? (~tmp + 1) : tmp);
            cal->v30.coarseOffset = signBit | mag;

            CHECK_RC(pJs->GetPropertyJsval(pJsCal, "coarseGain", &jval));
            CHECK_RC(pJs->FromJsval(jval, &tmp));
            // Colwert from 2's complement to sign-magnitude because that is what RM expects
            signBit = tmp < 0 ? 0x80 : 0;
            mag = static_cast<LwU8>(signBit ? (~tmp + 1) : tmp);
            cal->v30.coarseGain = signBit | mag;

            CHECK_RC(pJs->GetPropertyJsval(pJsCal, "lowTempLowVoltErr", &jval));
            CHECK_RC(pJs->FromJsval(jval, &cal->v30.lowTempLowVoltErr));
            CHECK_RC(pJs->GetPropertyJsval(pJsCal, "lowTempHighVoltErr", &jval));
            CHECK_RC(pJs->FromJsval(jval, &cal->v30.lowTempHighVoltErr));
            CHECK_RC(pJs->GetPropertyJsval(pJsCal, "highTempLowVoltErr", &jval));
            CHECK_RC(pJs->FromJsval(jval, &cal->v30.highTempLowVoltErr));
            CHECK_RC(pJs->GetPropertyJsval(pJsCal, "highTempHighVoltErr", &jval));
            CHECK_RC(pJs->FromJsval(jval, &cal->v30.highTempHighVoltErr));
            CHECK_RC(pJs->GetPropertyJsval(pJsCal, "adcCodeCorrectionOffset", &jval));
            CHECK_RC(pJs->FromJsval(jval, &cal->v30.adcCodeCorrectionOffset));

            break;
        }
        default:
            Printf(Tee::PriError, "Invalid calibration object\n");
            return RC::AVFS_ILWALID_ADC_CAL_TYPE;
    }

    return rc;
}

#define JS_CREATE_GET_ADC_CAL_API(funcName, help)                                         \
    JS_STEST_LWSTOM(JsAvfs, funcName, 1, help)                                            \
    {                                                                                     \
        RC rc;                                                                            \
        STEST_HEADER(1, 1, "Usage: Avfs." #funcName "([outAdcCalStates])");               \
        STEST_PRIVATE(JsAvfs, pJsAvfs, "JsAvfs");                                         \
        STEST_ARG(0, JSObject *, outAdcCals);                                             \
        Avfs *pAvfs = pJsAvfs->GetAvfs();                                                 \
        vector<Avfs::AdcCalibrationState> adcCalStates;                                   \
        C_CHECK_RC(pAvfs->funcName(&adcCalStates));                                       \
        UINT32 adcDevMask = pAvfs->GetAdcDevMask();                                       \
        UINT32 adcStatesInd = 0;                                                          \
        for (UINT32 i = 0; i < LW2080_CTRL_CLK_ADC_MAX_DEVICES; ++i)                       \
        {                                                                                 \
            if (!(adcDevMask & (1 << i)))                                                 \
            {                                                                             \
                C_CHECK_RC(pJavaScript->SetElement(outAdcCals, i, JSVAL_NULL));           \
                continue;                                                                 \
            }                                                                             \
            JSObject *pAdcCal = JS_NewObject(pContext, &AdcCalibrationStateClass,         \
                                            nullptr, nullptr);                            \
            C_CHECK_RC(JsAvfs::AdcCalibrationStateToJsObject(                             \
                adcCalStates[adcStatesInd], pAdcCal));                                    \
            jsval tmp;                                                                    \
            C_CHECK_RC(pJavaScript->ToJsval(pAdcCal, &tmp));                              \
            C_CHECK_RC(pJavaScript->SetElement(outAdcCals, i, tmp));                      \
            adcStatesInd++;                                                               \
        }                                                                                 \
        RETURN_RC(rc);                                                                    \
    }

JS_CREATE_GET_ADC_CAL_API(GetOriginalAdcCalibrationState,
                          "Retrieve original (startup) ADC "
                          "calibration state from all ADCs");

JS_CREATE_GET_ADC_CAL_API(GetLwrrentAdcCalibrationState,
                          "Retrieve current ADC calibration "
                          "state from all ADCs");

JS_STEST_LWSTOM(JsAvfs, GetAdcCalibrationStateViaId, 3,
                "Given a single ADC id, return that ADC's calibration settings")
{
    RC rc;
    STEST_HEADER(3, 3,
                 "Usage: "
                 "Avfs.GetAdcCalibrationStateViaId(AdcId, bDefault, {adcCalibration})");
    STEST_PRIVATE(JsAvfs, pJsAvfs, "JsAvfs");
    STEST_ARG(0, UINT32, adcId);
    STEST_ARG(1, bool, bDefault);
    STEST_ARG(2, JSObject *, pAdcCalibration);
    Avfs *pAvfs = pJsAvfs->GetAvfs();
    Avfs::AdcCalibrationState calState;
    C_CHECK_RC(pAvfs->GetAdcCalibrationStateViaId(
        (Avfs::AdcId)adcId, bDefault, &calState));
    RETURN_RC(JsAvfs::AdcCalibrationStateToJsObject(calState, pAdcCalibration));
}

JS_STEST_LWSTOM(JsAvfs, SetAdcCalibrationStateViaId, 2,
                "Set the ADC calibration state of the ADC specified by id")
{
    RC rc;
    STEST_HEADER(2, 2,
                 "Usage: Avfs.SetAdcCalibrationStateViaId(AdcId, {slope, intercept})");
    STEST_PRIVATE(JsAvfs, pJsAvfs, "JsAvfs");
    STEST_ARG(0, UINT32, adcId);
    STEST_ARG(1, JSObject *, jsAdcCal);
    Avfs *pAvfs = pJsAvfs->GetAvfs();
    Avfs::AdcCalibrationState newAdcCal = {};
    C_CHECK_RC(JsAvfs::JsObjectToAdcCalibrationState(jsAdcCal, &newAdcCal));
    RETURN_RC(pAvfs->SetAdcCalibrationStateViaId((Avfs::AdcId)adcId, newAdcCal));
}

JS_STEST_LWSTOM(JsAvfs, SetAdcCalibrationState, 1,
                "Set the calibration state via an array of new calibration settings")
{
    RC rc;
    STEST_HEADER(1, 1,
                 "Usage: "
                 "Avfs.SetAdcCalibrationState([{slope, intercept}, {slope, intercept}...])");
    STEST_PRIVATE(JsAvfs, pJsAvfs, "JsAvfs");
    STEST_ARG(0, JsArray, jsAdcCals);
    Avfs *pAvfs = pJsAvfs->GetAvfs();

    UINT32 adcDevMask = 0;
    vector<Avfs::AdcCalibrationState> newAdcCals;
    for (UINT32 i = 0; i < jsAdcCals.size(); ++i)
    {
        if (jsAdcCals[i] == JSVAL_NULL)
        {
            continue;
        }
        JSObject *jsAdcCal;
        C_CHECK_RC(pJavaScript->FromJsval(jsAdcCals[i], &jsAdcCal));

        Avfs::AdcCalibrationState newAdcCal = {};
        C_CHECK_RC(JsAvfs::JsObjectToAdcCalibrationState(jsAdcCal, &newAdcCal));
        newAdcCals.push_back(newAdcCal);
        adcDevMask |= 1 << i;
    }

    RETURN_RC(pAvfs->SetAdcCalibrationStateViaMask(adcDevMask, newAdcCals));
}

RC JsAvfs::SetQuickSlowdown(UINT32 vfLwrveIdx, UINT32 nafllDevMask, bool enable)
{
    return GetAvfs()->SetQuickSlowdown(vfLwrveIdx,
                                       nafllDevMask,
                                       enable);
}
JS_STEST_BIND_THREE_ARGS(JsAvfs, SetQuickSlowdown,
                         vfLwrveIdx, UINT32,
                         nafllDevMask, UINT32,
                         enable, bool,
                         "Enable/Disable slowing down to secondary/Ternary VF lwrve on a NAFLL device");

RC JsAvfs::SetFreqControllerEnable(INT32 dom, bool bEnable)
{
    return GetAvfs()->SetFreqControllerEnable(static_cast<Gpu::ClkDomain>(dom),
                                               bEnable);
}
JS_STEST_BIND_TWO_ARGS(JsAvfs, SetFreqControllerEnable,
                       clkDom, UINT32,
                       bEnable, bool,
                       "Enable/Disable all frequency controller(s) for a clock domain");

RC JsAvfs::SetFreqControllersEnable(bool bEnable)
{
    return GetAvfs()->SetFreqControllerEnable(bEnable);
}
JS_STEST_BIND_ONE_ARG(JsAvfs, SetFreqControllersEnable,
                      bEnable, bool,
                      "Enable/Disable all frequency controller(s) for all NAFLL domains");

JS_SMETHOD_LWSTOM(JsAvfs, GetFreqControllerEnable, 1,
                  "Return true if at least one freq controller is enabled")
{
    STEST_HEADER(1, 1,
        "Usage: var bEnabled = subdev.Avfs.GetFreqControllerEnable(clkDom);");
    STEST_PRIVATE(JsAvfs, pJsAvfs, "JsAvfs");
    STEST_ARG(0, UINT32, dom);

    Avfs *pAvfs = pJsAvfs->GetAvfs();
    bool bEnabled;

    RC rc;
    rc = pAvfs->IsFreqControllerEnabled(static_cast<Gpu::ClkDomain>(dom), &bEnabled);
    if (rc == RC::UNSUPPORTED_FUNCTION)
    {
        rc.Clear();
        bEnabled = false;
    }
    C_CHECK_RC(rc);

    pJavaScript->ToJsval(bEnabled, pReturlwalue);
    return JS_TRUE;
}

RC JsAvfs::DisableClvc()
{
    return GetAvfs()->DisableClvc();
}
JS_STEST_BIND_NO_ARGS(JsAvfs, DisableClvc, "Disable CLVC for all voltage domains");

RC JsAvfs::EnableClvc()
{
    return GetAvfs()->EnableClvc();
}
JS_STEST_BIND_NO_ARGS(JsAvfs, EnableClvc, "Enable CLVC for all voltage domains");

JS_CLASS(AvfsConst);

static SObject AvfsConst_Object
(
    "AvfsConst",
    AvfsConstClass,
    0,
    0,
    "AvfsConst JS object"
);

PROP_CONST(AvfsConst, ADC_SYS,  Avfs::ADC_SYS);
PROP_CONST(AvfsConst, ADC_LTC,  Avfs::ADC_LTC);
PROP_CONST(AvfsConst, ADC_XBAR, Avfs::ADC_XBAR);
PROP_CONST(AvfsConst, ADC_LWD,  Avfs::ADC_LWD);
PROP_CONST(AvfsConst, ADC_HOST, Avfs::ADC_HOST);
PROP_CONST(AvfsConst, ADC_GPC0, Avfs::ADC_GPC0);
PROP_CONST(AvfsConst, ADC_GPC1, Avfs::ADC_GPC1);
PROP_CONST(AvfsConst, ADC_GPC2, Avfs::ADC_GPC2);
PROP_CONST(AvfsConst, ADC_GPC3, Avfs::ADC_GPC3);
PROP_CONST(AvfsConst, ADC_GPC4, Avfs::ADC_GPC4);
PROP_CONST(AvfsConst, ADC_GPC5, Avfs::ADC_GPC5);
PROP_CONST(AvfsConst, ADC_GPC6, Avfs::ADC_GPC6);
PROP_CONST(AvfsConst, ADC_GPC7, Avfs::ADC_GPC7);
PROP_CONST(AvfsConst, ADC_GPC8, Avfs::ADC_GPC8);
PROP_CONST(AvfsConst, ADC_GPC9, Avfs::ADC_GPC9);
PROP_CONST(AvfsConst, ADC_GPC10, Avfs::ADC_GPC10);
PROP_CONST(AvfsConst, ADC_GPC11, Avfs::ADC_GPC11);
PROP_CONST(AvfsConst, ADC_GPCS, Avfs::ADC_GPCS);
PROP_CONST(AvfsConst, ADC_SRAM, Avfs::ADC_SRAM);
PROP_CONST(AvfsConst, ADC_SYS_ISINK, Avfs::ADC_SYS_ISINK);
PROP_CONST(AvfsConst, AdcId_NUM, Avfs::AdcId_NUM);
PROP_CONST(AvfsConst, NAFLL_SYS,  Avfs::NAFLL_SYS);
PROP_CONST(AvfsConst, NAFLL_LTC,  Avfs::NAFLL_LTC);
PROP_CONST(AvfsConst, NAFLL_XBAR, Avfs::NAFLL_XBAR);
PROP_CONST(AvfsConst, NAFLL_LWD,  Avfs::NAFLL_LWD);
PROP_CONST(AvfsConst, NAFLL_HOST, Avfs::NAFLL_HOST);
PROP_CONST(AvfsConst, NAFLL_GPC0, Avfs::NAFLL_GPC0);
PROP_CONST(AvfsConst, NAFLL_GPC1, Avfs::NAFLL_GPC1);
PROP_CONST(AvfsConst, NAFLL_GPC2, Avfs::NAFLL_GPC2);
PROP_CONST(AvfsConst, NAFLL_GPC3, Avfs::NAFLL_GPC3);
PROP_CONST(AvfsConst, NAFLL_GPC4, Avfs::NAFLL_GPC4);
PROP_CONST(AvfsConst, NAFLL_GPC5, Avfs::NAFLL_GPC5);
PROP_CONST(AvfsConst, NAFLL_GPC6, Avfs::NAFLL_GPC6);
PROP_CONST(AvfsConst, NAFLL_GPC7, Avfs::NAFLL_GPC7);
PROP_CONST(AvfsConst, NAFLL_GPC8, Avfs::NAFLL_GPC8);
PROP_CONST(AvfsConst, NAFLL_GPC9, Avfs::NAFLL_GPC9);
PROP_CONST(AvfsConst, NAFLL_GPC10, Avfs::NAFLL_GPC10);
PROP_CONST(AvfsConst, NAFLL_GPC11, Avfs::NAFLL_GPC11);
PROP_CONST(AvfsConst, NAFLL_GPCS, Avfs::NAFLL_GPCS);
PROP_CONST(AvfsConst, NafllId_NUM, Avfs::NafllId_NUM);
PROP_CONST(AvfsConst, NAFLL_TYPE_V10, LW2080_CTRL_CLK_NAFLL_DEVICE_TYPE_V10);
PROP_CONST(AvfsConst, NAFLL_TYPE_V20, LW2080_CTRL_CLK_NAFLL_DEVICE_TYPE_V20);
PROP_CONST(AvfsConst, NAFLL_TYPE_V30, LW2080_CTRL_CLK_NAFLL_DEVICE_TYPE_V30);
PROP_CONST(AvfsConst, ADC_CAL_TYPE_V10, LW2080_CTRL_CLK_ADC_CAL_TYPE_V10);
PROP_CONST(AvfsConst, ADC_CAL_TYPE_V20, LW2080_CTRL_CLK_ADC_CAL_TYPE_V20);
PROP_CONST(AvfsConst, ADC_CAL_TYPE_V30, Avfs::MODS_SHIM_ADC_CAL_TYPE_V30);
