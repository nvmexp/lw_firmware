/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/perf/js_perf.h"
#include "core/include/jscript.h"
#include "core/include/script.h"
#include "jsapi.h"
#include "core/include/rc.h"
#include "gpu/perf/perfsub.h"
#include "gpu/perf/perfsub_30.h"
#include "gpu/perf/perfsub_40.h"
#include "gpu/perf/perfutil.h"
#include "ctrl/ctrl2080.h"
#include "ctrl/ctrl2080/ctrl2080perf.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/utility.h"
#include "gpu/perf/pwrsub.h"
#include "gpu/perf/thermsub.h"
#include "core/include/deprecat.h"
#include "core/include/mle_protobuf.h"

JsPerf::JsPerf()
    : m_pPerf(NULL), m_pJsPerfObj(NULL)
{
}

//-----------------------------------------------------------------------------
static void C_JsPerf_finalize
(
    JSContext *cx,
    JSObject *obj
)
{
    MASSERT(cx != 0);
    MASSERT(obj != 0);

    JsPerf * pJsPerf;
    //! Delete the C++
    pJsPerf = (JsPerf *)JS_GetPrivate(cx, obj);
    delete pJsPerf;
};

//-----------------------------------------------------------------------------
static JSClass JsPerf_class =
{
    "JsPerf",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ColwertStub,
    C_JsPerf_finalize
};

//-----------------------------------------------------------------------------
static SObject JsPerf_Object
(
    "JsPerf",
    JsPerf_class,
    0,
    0,
    "Perf JS Object"
);

//------------------------------------------------------------------------------
//! \brief Create a JS Object representation of the current associated
//! Perf object
RC JsPerf::CreateJSObject(JSContext *cx, JSObject *obj)
{
    //! Only create one JSObject per Perf object
    if (m_pJsPerfObj)
    {
        Printf(Tee::PriLow,
               "A JS Object has already been created for this JsPerf.\n");
        return OK;
    }

    m_pJsPerfObj = JS_DefineObject(cx,
                                  obj, // GpuSubdevice object
                                  "Perf", // Property name
                                  &JsPerf_class,
                                  JsPerf_Object.GetJSObject(),
                                  JSPROP_READONLY);

    if (!m_pJsPerfObj)
        return RC::COULD_NOT_CREATE_JS_OBJECT;

    //! Store the current JsPerf instance into the private area
    //! of the new JSOBject.
    if (JS_SetPrivate(cx, m_pJsPerfObj, this) != JS_TRUE)
    {
        Printf(Tee::PriNormal,
               "Unable to set private value of JsPerf.\n");
        return RC::COULD_NOT_CREATE_JS_OBJECT;
    }

    // Add the .Help() to this JSObject
    JS_DefineFunction(cx, m_pJsPerfObj, "Help", &C_Global_Help, 1, 0);

    return OK;
}

//------------------------------------------------------------------------------
//! \brief Return the private JS data - C++ Perf object.
//!
Perf * JsPerf::GetPerfObj()
{
    MASSERT(m_pPerf);
    return m_pPerf;
}

//------------------------------------------------------------------------------
//! \brief Set the associated Perf object.
//!
//! This is called by JS GpuSubdevice Initialize
void JsPerf::SetPerfObj(Perf * pPerf)
{
    m_pPerf = pPerf;
}

UINT08 JsPerf::GetVersion()
{
    return static_cast<UINT08>(m_pPerf->GetPStateVersion());
}

bool JsPerf::GetIsPState30Supported()
{
    return m_pPerf->IsPState30Supported();
}

bool JsPerf::GetIsPState35Supported()
{
    return m_pPerf->IsPState35Supported();
}

bool JsPerf::GetIsPState40Supported()
{
    return m_pPerf->IsPState40Supported();
}


#define JSPERF_GETSETPROP(rettype, funcname,helpmsg)                                         \
P_( JsPerf_Get##funcname );                                                                  \
P_( JsPerf_Set##funcname );                                                                  \
static SProperty Get##funcname                                                               \
(                                                                                            \
    JsPerf_Object,                                                                           \
    #funcname,                                                                               \
    0,                                                                                       \
    0,                                                                                       \
    JsPerf_Get##funcname,                                                                    \
    JsPerf_Set##funcname,                                                                    \
    0,                                                                                       \
    helpmsg                                                                                  \
);                                                                                           \
P_( JsPerf_Set##funcname )                                                                   \
{                                                                                            \
    MASSERT(pContext != 0);                                                                  \
    MASSERT(pValue   != 0);                                                                  \
    JavaScriptPtr pJavaScript;                                                               \
    JsPerf *pJsPerf;                                                                         \
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)                \
    {                                                                                        \
        RC rc;                                                                               \
        Perf* pPerf = pJsPerf->GetPerfObj();                                                 \
        rettype RetVal;                                                                      \
        if (OK != pJavaScript->FromJsval(*pValue, &RetVal))                                  \
        {                                                                                    \
            JS_ReportError(pContext, "Failed to colwert Set"#funcname"");                    \
            return JS_FALSE;                                                                 \
        }                                                                                    \
        char msg[256];                                                                       \
        if ((rc = pPerf->Set##funcname(RetVal))!= OK)                                        \
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
P_( JsPerf_Get##funcname )                                                                   \
{                                                                                            \
    MASSERT(pContext != 0);                                                                  \
    MASSERT(pValue   != 0);                                                                  \
    JavaScriptPtr pJavaScript;                                                               \
    JsPerf *pJsPerf;                                                                         \
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)                \
    {                                                                                        \
        RC rc;                                                                               \
        Perf* pPerf = pJsPerf->GetPerfObj();                                                 \
        rettype RetVal = rettype();                                                          \
        char msg[256];                                                                       \
        rc = pPerf->Get##funcname(&RetVal);                                                  \
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
JSPERF_GETSETPROP(UINT32, CoreVoltageMv, "core GPU voltage in milivolts");

// Keep legacy Perf.UseHardPStateLocks to avoid breaking old CS/SLT scripts
JSPERF_GETSETPROP(bool,   UseHardPStateLocks, "Use \"hard\" pstate lock instead of \"soft\" MODS lock (i.e. do not allow rm to reduce clocks/voltages to prevent HW damage)");
JSPERF_GETSETPROP(bool,   ForceUnlockBeforeLock, "Always clear pstate locks before setting a new pstate/PerfPoint lock (restore glitchy traditional behavior)");
JSPERF_GETSETPROP(bool,   DumpVfTable, "Print out the parsed VfTable at a more visible priority");
JSPERF_GETSETPROP(bool,   DumpLimitsOnPerfSwitch, "Print all perf limits MODS sets BEFORE it sets them");
JSPERF_GETSETPROP(UINT32, LimitVerbosity, "Which limits to print on perf switch");
JSPERF_GETSETPROP(UINT32, VfSwitchTimePriority, "Print priority for perf switch time");
JSPERF_GETSETPROP(bool,   UnlockChangeSequencer, "Unlock the RM perf change sequencer after init");
JSPERF_GETSETPROP(UINT32, VfePollingPeriodMs, "Controls how often VFE equations are evaluated");

P_( JsPerf_Get_LwrrentPState );
static SProperty LwrrentPState
(
    JsPerf_Object,
    "LwrrentPState",
    0,
    0,
    JsPerf_Get_LwrrentPState,
    0,
    JSPROP_READONLY,
    "acquire the current pState"
);
P_( JsPerf_Get_LwrrentPState )
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        JavaScriptPtr pJavaScript;
        Perf* pPerf = pJsPerf->GetPerfObj();
        UINT32 PState = 0xFFFFFFFF;
        RC rc;
        if ((rc = pPerf->GetLwrrentPState(&PState))!= OK)
        {
            char msg[256];
            sprintf(msg, "Error %d calling GetLwrrentPState\n", (UINT32)rc);
            JS_ReportError(pContext, msg);
            *pValue = JSVAL_NULL;
            return JS_FALSE;
        }

        if (pJavaScript->ToJsval(PState, pValue) == 0)
        {
            return JS_TRUE;
        }
    }
    return JS_FALSE;
}

P_( JsPerf_Get_NumPStates );
static SProperty NumPStates
(
    JsPerf_Object,
    "NumPStates",
    0,
    0,
    JsPerf_Get_NumPStates,
    0,
    JSPROP_READONLY,
    "acquire number of pStates"
);
P_( JsPerf_Get_NumPStates )
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        JavaScriptPtr pJavaScript;
        Perf* pPerf = pJsPerf->GetPerfObj();
        UINT32 NumPState;
        RC rc;
        if ((rc = pPerf->GetNumPStates(&NumPState))!= OK)
        {
            char msg[256];
            sprintf(msg, "Error %d reading calling GetNumPState\n", (UINT32)rc);
            JS_ReportError(pContext, msg);
            *pValue = JSVAL_NULL;
            return JS_FALSE;
        }
        if (pJavaScript->ToJsval(NumPState, pValue) == 0)
        {
            return JS_TRUE;
        }
    }
    return JS_FALSE;
}
P_(JsPerf_Get_LwrrVfIdx);
static SProperty LwrrVfIdx
(
    JsPerf_Object,
    "LwrrVfIdx",
    0,
    0,
    JsPerf_Get_LwrrVfIdx,
    0,
    JSPROP_READONLY,
    "Get the current VF entry index"
);
P_(JsPerf_Get_LwrrVfIdx)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        JavaScriptPtr pJavaScript;
        Perf* pPerf = pJsPerf->GetPerfObj();
        UINT32 VfIdx = 0xFFFFFFFF;
        RC rc;
        if ((rc = pPerf->GetPStateErrorCodeDigits(&VfIdx))!= OK)
        {
            char msg[256];
            sprintf(msg, "Error %d calling GetLwrrVfIdx\n", (UINT32)rc);
            JS_ReportError(pContext, msg);
            *pValue = JSVAL_NULL;
            return JS_FALSE;
        }
        if (pJavaScript->ToJsval(VfIdx, pValue) == 0)
        {
            return JS_TRUE;
        }
    }
    return JS_FALSE;
}

P_(JsPerf_Get_PStateErrorCodeDigits);
static SProperty PStateErrorCodeDigits
(
    JsPerf_Object,
    "PStateErrorCodeDigits",
    0,
    0,
    JsPerf_Get_PStateErrorCodeDigits,
    0,
    JSPROP_READONLY,
    "Get the 3 current PState error code digits"
);
P_(JsPerf_Get_PStateErrorCodeDigits)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        UINT32 pstateECDigits = 999;
        RC rc;
        if ((rc = pJsPerf->GetPerfObj()->GetPStateErrorCodeDigits(&pstateECDigits)) != OK)
        {
            JS_ReportError(pContext, "Error calling Perf.GetStateErrorCodeDigits()\n");
            *pValue = JSVAL_NULL;
            return JS_FALSE;
        }

        if (JavaScriptPtr()->ToJsval(pstateECDigits, pValue) == 0)
        {
            return JS_TRUE;
        }
    }
    return JS_FALSE;
}

P_(JsPerf_Get_IsPState20Supported);
static SProperty IsPState20Supported
(
    JsPerf_Object,
    "IsPState20Supported",
    0,
    0,
    JsPerf_Get_IsPState20Supported,
    0,
    JSPROP_READONLY,
    "query whether PState 2.0 is supported"
);
P_( JsPerf_Get_IsPState20Supported)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        JavaScriptPtr pJavaScript;
        Perf* pPerf = pJsPerf->GetPerfObj();
        if (pJavaScript->ToJsval(pPerf->IsPState20Supported(), pValue) == 0)
        {
            return JS_TRUE;
        }
    }
    return JS_FALSE;
}

CLASS_PROP_READONLY_FULL(JsPerf,
                         Version,
                         UINT08,
                         "Returns PStates version",
                         JSPROP_READONLY,
                         0);

CLASS_PROP_READONLY_FULL(JsPerf,
                         IsPState30Supported,
                         bool,
                         "query whether pstate 3.0 is supported",
                         JSPROP_READONLY,
                         0);

CLASS_PROP_READONLY_FULL(JsPerf,
                         IsPState35Supported,
                         bool,
                         "query whether pstate 3.5 is supported",
                         JSPROP_READONLY,
                         0);


CLASS_PROP_READONLY_FULL(JsPerf,
                         IsPState40Supported,
                         bool,
                         "query whether pstate 4.0 is supported",
                         JSPROP_READONLY,
                         0);

P_(JsPerf_Get_InflectionPtStr);
static SProperty GetInflectionPtStr
(
    JsPerf_Object,
    "InflectionPtStr",
    0,
    0,
    JsPerf_Get_InflectionPtStr,
    0,
    JSPROP_READONLY,
    "get the current inflection point (string)"
);
P_(JsPerf_Get_InflectionPtStr)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        JavaScriptPtr pJs;
        Perf* pPerf = pJsPerf->GetPerfObj();
        if (pJs->ToJsval(pPerf->GetInflectionPtStr(), pValue) == 0)
        {
            return JS_TRUE;
        }
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsPerf,
                SetTegraPerfPointInfo,
                2,
                "Set the information about CheetAh perf point")
{
    STEST_HEADER(2, 2, "Usage: Perf.SetTegraPerfPointInfo(index, name)");

    STEST_PRIVATE(JsPerf, pJsPerf, "JsPerf");
    STEST_ARG(0, UINT32, index);
    STEST_ARG(1, string, name);

    RC rc;
    Perf *const pPerf = pJsPerf->GetPerfObj();
    RETURN_RC(pPerf->SetTegraPerfPointInfo(index, name));
}

P_(JsPerf_Get_VoltTuningOffsetMv);
static SProperty GetVoltTuningOffsetMv
(
    JsPerf_Object,
    "VoltTuningOffsetMv",
    0,
    0,
    JsPerf_Get_VoltTuningOffsetMv,
    0,
    JSPROP_READONLY,
    "get the volt tuning offset in mV"
);
P_(JsPerf_Get_VoltTuningOffsetMv)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        JavaScriptPtr pJs;
        Perf* pPerf = pJsPerf->GetPerfObj();
        if (pJs->ToJsval(pPerf->GetVoltTuningOffsetMv(), pValue) == 0)
        {
            return JS_TRUE;
        }
    }
    return JS_FALSE;
}

P_( JsPerf_Get_ClearPerfLevelsForUi );
P_( JsPerf_Set_ClearPerfLevelsForUi );
static SProperty ClearPerfLevelsForUi
(
    JsPerf_Object,
    "ClearPerfLevelsForUi",
    0,
    0,
    JsPerf_Get_ClearPerfLevelsForUi,
    JsPerf_Set_ClearPerfLevelsForUi,
    0,
    "On Mac platform, clear perf suspend levels to allow RM to take for modesets."
);
P_( JsPerf_Get_ClearPerfLevelsForUi )
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        JavaScriptPtr pJavaScript;
        Perf* pPerf = pJsPerf->GetPerfObj();

        if (OK == pJavaScript->ToJsval(pPerf->GetClearPerfLevelsForUi(), pValue))
        {
            return JS_TRUE;
        }
    }
    return JS_FALSE;
}

P_( JsPerf_Set_ClearPerfLevelsForUi )
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    JavaScriptPtr pJavaScript;
    bool bClearPerfLevelsForUi = false;

    if (OK != pJavaScript->FromJsval(*pValue, &bClearPerfLevelsForUi))
    {
        return JS_FALSE;
    }

    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        Perf* pPerf = pJsPerf->GetPerfObj();
        pPerf->SetClearPerfLevelsForUi(bClearPerfLevelsForUi);
        return JS_TRUE;
    }
    return JS_FALSE;                                                                         \
}

P_( JsPerf_Get_PStateLockType );
static SProperty PStateLockType
(
    JsPerf_Object,
    "PStateLockType",
    0,
    0,
    JsPerf_Get_PStateLockType,
    0,
    JSPROP_READONLY,
    "PState lock type: PerfConst.NotLocked, .HardLock, .SoftLock, "
    ".VirtualLock, .DefaultLock, or .StrictLock"
);
P_( JsPerf_Get_PStateLockType )
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        UINT32 PStateLockType = pJsPerf->GetPerfObj()->GetPStateLockType();
        if (JavaScriptPtr()->ToJsval(PStateLockType, pValue) == 0)
        {
            return JS_TRUE;
        }
    }
    return JS_FALSE;
}

P_( JsPerf_Get_ForcedPStateLockType );
P_( JsPerf_Set_ForcedPStateLockType );
static SProperty ForcedPStateLockType
(
    JsPerf_Object,
    "ForcedPStateLockType",
    0,
    0,
    JsPerf_Get_ForcedPStateLockType,
    JsPerf_Set_ForcedPStateLockType,
    0,
    "PState lock type: PerfConst.NotLocked, .HardLock, .SoftLock, or .VirtualLock"
);
P_( JsPerf_Set_ForcedPStateLockType )
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    JavaScriptPtr pJavaScript;
    UINT32 lockTypeNum;
    if (OK != pJavaScript->FromJsval(*pValue, &lockTypeNum))
    {
        return JS_FALSE;
    }
    Perf::PStateLockType lockType = static_cast<Perf::PStateLockType>(lockTypeNum);
    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        RC rc;
        CHECK_RC(pJsPerf->GetPerfObj()->SetForcedPStateLockType(lockType));
        return JS_TRUE;
    }
    return JS_FALSE;
}
P_( JsPerf_Get_ForcedPStateLockType )
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        RC rc;
        Perf::PStateLockType lockType;
        CHECK_RC(pJsPerf->GetPerfObj()->GetForcedPStateLockType(&lockType));
        if (JavaScriptPtr()->ToJsval(static_cast<UINT32>(lockType), pValue) == 0)
        {
            return JS_TRUE;
        }
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsPerf,
                GetDefaultClockAtPState,
                3,
                "Get a domain Clock Freq of a PState (default value)")
{
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    JavaScriptPtr pJavaScript;
    UINT32     PStateNum;
    INT32      Domain;
    JSObject * pReturnClkKHz = 0;
    const char usage[] = "Usage: subdev.Perf.GetDefaultClockAtPState(Pstate,Gpu.ClkDomain, RetValHz)\n";

    if (3 != pJavaScript->UnpackArgs(pArguments, NumArguments, "Iio",
                                     &PStateNum, &Domain, &pReturnClkKHz))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        RC rc;
        UINT64 ClkHz;
        Perf* pPerf = pJsPerf->GetPerfObj();
        C_CHECK_RC(pPerf->GetDefaultClockAtPState(PStateNum,
                                           (Gpu::ClkDomain)Domain,
                                           &ClkHz));
        RETURN_RC(pJavaScript->SetElement(pReturnClkKHz, 0, ClkHz));
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsPerf,
                GetClockAtPState,
                3,
                "Get a domain Clock Freq of a PState")
{
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    JavaScriptPtr pJavaScript;
    UINT32     PStateNum;
    INT32      Domain;
    JSObject * pReturnClkKHz = 0;
    const char usage[] = "Usage: subdev.Perf.GetClockAtPState(Pstate,Gpu.ClkDomain, RetValHz)\n";

    if (3 != pJavaScript->UnpackArgs(pArguments, NumArguments, "Iio",
                                     &PStateNum, &Domain, &pReturnClkKHz))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        RC rc;
        UINT64 ClkHz;
        Perf* pPerf = pJsPerf->GetPerfObj();
        C_CHECK_RC(pPerf->GetClockAtPState(PStateNum,
                                           (Gpu::ClkDomain)Domain,
                                           &ClkHz));
        RETURN_RC(pJavaScript->SetElement(pReturnClkKHz, 0, ClkHz));
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsPerf,
                GetClockAtPerfPoint,
                3,
                "Get a domain Clock Freq of a PerfPoint")
{
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    JavaScriptPtr pJavaScript;
    JSObject * pJsPerfPoint;
    INT32      Domain;
    JSObject * pReturnClkHz = 0;
    const char usage[] = "Usage: subdev.Perf.GetClockAtPerfPoint(PerfPoint,Gpu.ClkDomain, [RtnHz])\n";

    if (3 != pJavaScript->UnpackArgs(pArguments, NumArguments, "oio",
                                     &pJsPerfPoint, &Domain, &pReturnClkHz))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        RC rc;
        UINT64 ClkHz;
        Perf* pPerf = pJsPerf->GetPerfObj();
        Perf::PerfPoint perfPoint;
        C_CHECK_RC(pPerf->JsObjToPerfPoint(pJsPerfPoint, &perfPoint));
        C_CHECK_RC(pPerf->GetClockAtPerfPoint(
                       perfPoint, (Gpu::ClkDomain)Domain, &ClkHz));
        RETURN_RC(pJavaScript->SetElement(pReturnClkHz, 0, ClkHz));
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsPerf,
                MeasureClock,
                2,
                "Measure the real frequency for a clock by using clock counters")
{
    STEST_HEADER(2, 2, "Usage: Perf.MeasureClock(ClkWhich, RetVal)");

    STEST_PRIVATE(JsPerf, pJsPerf, "JsPerf");
    STEST_ARG(0, UINT32, WhichClk);
    STEST_ARG(1, JSObject*, pReturlwal);

    RC rc;
    UINT64 FreqHz;
    Perf *pPerf = pJsPerf->GetPerfObj();
    C_CHECK_RC(pPerf->MeasureClock(
        (Gpu::ClkDomain)WhichClk, &FreqHz));
    RETURN_RC(pJavaScript->SetElement(pReturlwal, 0, FreqHz));
}

JS_STEST_LWSTOM(JsPerf,
                MeasurePllClock,
                3,
                "Measure the real frequency of any particular PLL available "
                "for a clock by using clock counters")
{
    STEST_HEADER(3, 3, "Usage: Perf.MeasureClock(ClkWhich, PllIndex, RetVal)");

    STEST_PRIVATE(JsPerf, pJsPerf, "JsPerf");
    STEST_ARG(0, UINT32, WhichClk);
    STEST_ARG(1, UINT32, PllIndex);
    STEST_ARG(2, JSObject*, pReturlwal);

    RC rc;
    UINT64 FreqHz;
    Perf *pPerf = pJsPerf->GetPerfObj();
    C_CHECK_RC(pPerf->MeasurePllClock(
        (Gpu::ClkDomain)WhichClk, PllIndex, &FreqHz));
    RETURN_RC(pJavaScript->SetElement(pReturlwal, 0, FreqHz));
}

JS_STEST_LWSTOM(JsPerf,
                MeasurePartitionedClock,
                2,
                "Measure the real frequency of a paritioned (core) clock")
{
    STEST_HEADER(2, 2,
                 "Usage: Perf.MeasurePartitionedClock(ClkWhich, RetObj)");
    STEST_PRIVATE(JsPerf, pJsPerf, "JsPerf");
    STEST_ARG(0, UINT32, clk);
    STEST_ARG(1, JSObject*, pReturnedFreqs);
    Perf *pPerf = pJsPerf->GetPerfObj();

    RC rc;
    UINT32 partitionMask = pPerf->GetClockPartitionMask((Gpu::ClkDomain)clk);
    vector<UINT64> allEffectiveFreqsHz;
    UINT64 overallClkDomHz;

    C_CHECK_RC(pPerf->MeasureCoreClock((Gpu::ClkDomain)clk,
                                               &overallClkDomHz));

    // If the clock doesn't have any partitions, make sure that
    // an empty array is returned in "partitionClocksHz"
    if (partitionMask)
    {
        C_CHECK_RC(pPerf->MeasureClockPartitions(
            (Gpu::ClkDomain)clk, partitionMask, &allEffectiveFreqsHz));
    }
    jsval jsOverall;
    C_CHECK_RC(pJavaScript->ToJsval(overallClkDomHz, &jsOverall));
    C_CHECK_RC(pJavaScript->SetPropertyJsval(pReturnedFreqs, "overallClockHz", jsOverall));
    JsArray jsArrayAllEffectiveFreqsHz;
    for (UINT32 i = 0; i < allEffectiveFreqsHz.size(); ++i)
    {
        jsval tmp;
        C_CHECK_RC(pJavaScript->ToJsval(allEffectiveFreqsHz[i], &tmp));
        jsArrayAllEffectiveFreqsHz.push_back(tmp);
    }
    jsval jsvalAllEffectiveFreqsHz;
    C_CHECK_RC(pJavaScript->ToJsval(&jsArrayAllEffectiveFreqsHz,
                                    &jsvalAllEffectiveFreqsHz));
    RETURN_RC(pJavaScript->SetPropertyJsval(
        pReturnedFreqs, "partitionClocksHz", jsvalAllEffectiveFreqsHz));
}

namespace
{
    void MaybeWarnUnknownGpcClk(Perf* pPerf, const Perf::PerfPoint& pt, UINT64 freqHz)
    {
        if (!freqHz)
        {
            Printf(Tee::PriWarn,
                   "PerfPoint %s GpcPerfHz is unknown.\n",
                   pt.name(pPerf->IsPState30Supported()).c_str());
        }
    }
}

JS_STEST_LWSTOM(JsPerf,
                SetRatioForSlaveEntries,
                1,
                "Set Ratio for slave entries of Clk-VF relationships")
{
    STEST_HEADER(1, 1, "Usage: Input is a JSArray. where each object has two fields: "
                       "Vf Rel Index and array of slave entries for Vf Type ratio");
    STEST_PRIVATE(JsPerf, pJsPerf, "JsPerf");
    STEST_ARG(0, JsArray, jsSlaveEntriesRatio);

    RC rc;
    Perf40* pPerf40 = dynamic_cast<Perf40*>(pJsPerf->GetPerfObj());
    if (!pPerf40)
        RETURN_RC(RC::UNSUPPORTED_FUNCTION);
    LW2080_CTRL_CLK_CLK_VF_RELS_CONTROL clkVfRelsControl = {};
    C_CHECK_RC(pPerf40->GetClkVfRelsControl(&clkVfRelsControl));

    for (UINT32 i = 0; i < jsSlaveEntriesRatio.size(); i++)
    {
        JSObject *pOneEntry;
        C_CHECK_RC(pJavaScript->FromJsval(jsSlaveEntriesRatio[i], &pOneEntry));
        UINT32 vfRelIdx;
        JsArray slaveEntriesRatio;
        C_CHECK_RC(pJavaScript->UnpackFields(pOneEntry, "Ia",
                                         "VfRelIdx", &vfRelIdx,
                                         "SlaveEntriesRatio", &slaveEntriesRatio));

        if (vfRelIdx >= LW2080_CTRL_BOARDOBJGRP_E255_MAX_OBJECTS)
        {
            Printf(Tee::PriError, "VfRelIdx = %u is greater than "
                                  "the size of LW2080_CTRL_CLK_CLK_VF_RELS_CONTROL.vfRels "
                                  "which is = %u", vfRelIdx, LW2080_CTRL_BOARDOBJGRP_E255_MAX_OBJECTS);
            return JS_FALSE;
        }
            switch (clkVfRelsControl.vfRels[vfRelIdx].super.type)
            {
                case LW2080_CTRL_CLK_CLK_VF_REL_TYPE_RATIO_VOLT:
                case LW2080_CTRL_CLK_CLK_VF_REL_TYPE_RATIO_FREQ:
                    for (UINT32 j = 0; j < slaveEntriesRatio.size(); j++)
                    {
                        JSObject *pOneEntryRatio;
                        UINT32 ratio;
                        UINT32 slaveIdx;
                        C_CHECK_RC(pJavaScript->FromJsval(slaveEntriesRatio[j], &pOneEntryRatio));
                    C_CHECK_RC(pJavaScript->UnpackFields(pOneEntryRatio, "II",
                               "slaveIdx", &slaveIdx, "ratio", &ratio));
                    if (slaveIdx >= LW2080_CTRL_CLK_CLK_VF_REL_RATIO_SECONDARY_ENTRIES_MAX)
                    {
                        Printf(Tee::PriError, "slave index = %u is greater than max "
                                              "supported slave entries, which is = %u \n",
                                              slaveIdx,
                                              LW2080_CTRL_CLK_CLK_VF_REL_RATIO_SECONDARY_ENTRIES_MAX);
                        return JS_FALSE;
                    }
                    clkVfRelsControl.vfRels[vfRelIdx].data.ratio.slaveEntries[slaveIdx].ratio =
                                                    CHECKED_CAST(UINT08, ratio);
                    }
                    break;
                default:
                    Printf(Tee::PriError, "RM VF relationship type is not one of "
                                          "LW2080_CTRL_CLK_CLK_VF_REL_TYPE_RATIO_VOLT or "
                                          "LW2080_CTRL_CLK_CLK_VF_REL_TYPE_RATIO_FREQ; "
                                          "VfRelIdx = %u, Type = %u \n", vfRelIdx,
                                           clkVfRelsControl.vfRels[vfRelIdx].type);
                    return JS_FALSE;
             }
    }
    C_CHECK_RC(pPerf40->SetClkVfRelsControl(&clkVfRelsControl));
    RETURN_RC(rc);
}

JS_STEST_LWSTOM(JsPerf,
                SetRatioForPropTopRelTypeRatio,
                1,
                "Set Ratio for Clock Propagation Toplogy Relationship")
{
    STEST_HEADER(1, 1, "Usage: Input is a JSArray. where each object has two fields: "
                       "Table Entry Index and ratio relationship between source and "
                       "destination ");
    STEST_PRIVATE(JsPerf, pJsPerf, "JsPerf");
    STEST_ARG(0, JsArray, jsPropTopRelRatio);

    RC rc;
    Perf40* pPerf40 = dynamic_cast<Perf40*>(pJsPerf->GetPerfObj());
    if (!pPerf40)
        RETURN_RC(RC::UNSUPPORTED_FUNCTION);

    // LW2080_CTRL_CMD_CLK_CLK_PROP_TOP_RELS_GET_CONTROL
    LW2080_CTRL_CLK_CLK_PROP_TOP_RELS_CONTROL clkPropTopRelsControl = {};
    C_CHECK_RC(pPerf40->GetClkPropTopRelsControl(&clkPropTopRelsControl));

    for (UINT32 i = 0; i < jsPropTopRelRatio.size(); i++)
    {
        JSObject *pOneEntry;
        C_CHECK_RC(pJavaScript->FromJsval(jsPropTopRelRatio[i], &pOneEntry));
        UINT32 ratioIdx;
        FLOAT32 propTopRelRatio;
        C_CHECK_RC(pJavaScript->UnpackFields(pOneEntry, "If",
                                             "ratioIdx", &ratioIdx,
                                             "propTopRelRatio", &propTopRelRatio));

        // Set this for only type _1X_RATIO
        if (clkPropTopRelsControl.propTopRels[ratioIdx].type ==
                LW2080_CTRL_CLK_CLK_PROP_TOP_REL_TYPE_1X_RATIO)
        {
            clkPropTopRelsControl.propTopRels[ratioIdx].data.ratio.ratio =
                                            Utility::ColwertFloatTo16_16(propTopRelRatio);
        }
        else
        {
            Printf(Tee::PriError, "Unknown Clk Propagation Topology Relationship"
                                  "RatioIdx = %u, Type = %u \n",
                                  ratioIdx,
                                  clkPropTopRelsControl.propTopRels[ratioIdx].type);
            return JS_FALSE;
        }
    }

    // LW2080_CTRL_CMD_CLK_CLK_PROP_TOP_RELS_SET_CONTROL
    C_CHECK_RC(pPerf40->SetClkPropTopRelsControl(&clkPropTopRelsControl));

    RETURN_RC(rc);
}

JS_STEST_LWSTOM(JsPerf,
                GetGpc2PerfOfPerfPoint,
                2,
                "Get Gpc2PerfHz of a PerfPoint")
{
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    Printf(Tee::PriWarn,
           "GetGpc2PerfOfPerfPoint() is deprecated. Use GetGpcPerfOfPerfPoint() instead\n");

    JavaScriptPtr pJavaScript;
    JSObject * pJsPerfPoint;
    JSObject * pReturnHz = 0;
    const char usage[] = "Usage: subdev.Perf.GetGpc2PerfOfPerfPoint(PerfPoint, [RtnHz])\n";

    if (2 != pJavaScript->UnpackArgs(pArguments, NumArguments, "oo",
                                     &pJsPerfPoint, &pReturnHz))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        RC rc;
        UINT64 Hz;
        Perf* pPerf = pJsPerf->GetPerfObj();
        Perf::PerfPoint perfPoint;
        C_CHECK_RC(pPerf->JsObjToPerfPoint(pJsPerfPoint, &perfPoint));
        C_CHECK_RC(pPerf->GetGpcFreqOfPerfPoint(
                       perfPoint,  &Hz));

        MaybeWarnUnknownGpcClk(pPerf, perfPoint, Hz);

        // User wants gpc2clk, but MODS stores as gpcclk so multiply by 2
        Hz *= 2;
        RETURN_RC(pJavaScript->SetElement(pReturnHz, 0, Hz));
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsPerf, GetGpcPerfOfPerfPoint, 2,
               "Get the gpcclk frequency of a PerfPoint")
{
    RC rc;
    STEST_HEADER(2, 2, "Usage: subdev.Perf.GetGpcPerfOfPerfPoint(PerfPoint, [RtnHz])\n");
    STEST_PRIVATE(JsPerf, pJsPerf, "JsPerf");
    STEST_ARG(0, JSObject*, pJsPerfPoint);
    STEST_ARG(1, JSObject*, pOutFreqHz);

    Perf* pPerf = pJsPerf->GetPerfObj();
    Perf::PerfPoint perfPoint;
    UINT64 freqHz;
    C_CHECK_RC(pPerf->JsObjToPerfPoint(pJsPerfPoint, &perfPoint));
    C_CHECK_RC(pPerf->GetGpcFreqOfPerfPoint(perfPoint, &freqHz));

    MaybeWarnUnknownGpcClk(pPerf, perfPoint, freqHz);

    RETURN_RC(pJavaScript->SetElement(pOutFreqHz, 0, freqHz));
}

JS_STEST_LWSTOM(JsPerf,
                GetClockRange,
                3,
                "Get Min/Max frequencies of a clock for one PState")
{
    MASSERT(pArguments && pReturlwalue);

    JavaScriptPtr pJS;
    UINT32     PStateNum;
    INT32      Domain;
    JSObject * pRetObj = 0;
    const char usage[] = "Usage: subdev.Perf.GetClockRange(Pstate,Gpu.ClkDomain, RetObj)\n";

    if (3 != pJS->UnpackArgs(pArguments, NumArguments, "Iio",
                                     &PStateNum, &Domain, &pRetObj))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) == 0)
    {
        return JS_FALSE;
    }
    RC rc;
    Perf* pPerf = pJsPerf->GetPerfObj();
    Perf::ClkRange NewRange;
    C_CHECK_RC(pPerf->GetClockRange(PStateNum,
                                    (Gpu::ClkDomain)Domain,
                                    &NewRange));
    if (Domain != Gpu::ClkPexGen)
    {
        NewRange.MinKHz /= 1000;
        NewRange.MaxKHz /= 1000;
    }
    C_CHECK_RC(pJS->PackFields(pRetObj, "II",
                               "MinMHz", NewRange.MinKHz,
                               "MaxMHz", NewRange.MaxKHz));
    RETURN_RC(rc);
}

JS_STEST_LWSTOM(JsPerf,
                IsOwned,
                0,
                "Determine whether PStates are held by some test")
{
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    JavaScriptPtr pJavaScript;
    const char usage[] = "Usage: subdev.Perf.IsOwned()\n";

    if (NumArguments != 0)
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        Perf* pPerf = pJsPerf->GetPerfObj();
        if (OK == pJavaScript->ToJsval(pPerf->IsOwned(), pReturlwalue))
        {
            return JS_TRUE;
        }
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsPerf,
                GetVoltageAtPState,
                3,
                "Get a domain Voltage Freq of a PState")
{
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    JavaScriptPtr pJavaScript;
    UINT32     PStateNum;
    INT32      Domain;
    JSObject * pReturlwoltMv = 0;
    const char usage[] = "Usage: subdev.Perf.GetVoltageAtPState(Pstate, Gpu.VoltageDomain, RetVal)\n"
                         "Example:\n"
                         "  var sub0 = new GpuSubdevice(0,0)\n"
                         "  var RetLwvdd = new Array\n"
                         "  sub0.Perf.GetVoltageAtPState(8, Gpu.CoreV, RetLwvdd)\n"
                         "  Out.Printf(Out.PriNormal, \"lwvdd at P%%d is "
                         "%%dmV\", PState, RetLwvdd[0])";

    if (3 != pJavaScript->UnpackArgs(pArguments, NumArguments, "Iio",
                                     &PStateNum, &Domain, &pReturlwoltMv))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        RC rc;
        UINT32 VoltageMv;
        Perf* pPerf = pJsPerf->GetPerfObj();
        C_CHECK_RC(pPerf->GetVoltageAtPState(PStateNum,
                                           (Gpu::VoltageDomain)Domain,
                                           &VoltageMv));
        RETURN_RC(pJavaScript->SetElement(pReturlwoltMv, 0, VoltageMv));
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsPerf,
                GetPexSpeedAtPState,
                2,
                "Get max PEX speed a PState")
{
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    JavaScriptPtr pJavaScript;
    UINT32     PStateNum;
    JSObject * pReturnPexSpeed = 0;
    const char usage[] = "Usage: subdev.Perf.GetPexSpeedAtPState(Pstate, RetVal)\n";

    if (2 != pJavaScript->UnpackArgs(pArguments, NumArguments, "Io",
                                     &PStateNum, &pReturnPexSpeed))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        RC rc;
        UINT32 PexSpeed;
        Perf* pPerf = pJsPerf->GetPerfObj();
        C_CHECK_RC(pPerf->GetPexSpeedAtPState(PStateNum, &PexSpeed));
        RETURN_RC(pJavaScript->SetElement(pReturnPexSpeed, 0, PexSpeed));
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsPerf,
                GetPexLinkWidthAtPState,
                2,
                "Get PEX link width at PState")
{
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    JavaScriptPtr pJavaScript;
    UINT32     PStateNum;
    JSObject * pReturnWidth = 0;
    const char usage[] = "Usage: subdev.Perf.GetPexLinkWidthAtPState(Pstate, RetValArray)\n";

    if (2 != pJavaScript->UnpackArgs(pArguments, NumArguments, "Io",
                                     &PStateNum, &pReturnWidth))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        RC rc;
        UINT32 Width;
        Perf* pPerf = pJsPerf->GetPerfObj();
        C_CHECK_RC(pPerf->GetLinkWidthAtPState(PStateNum, &Width));
        RETURN_RC(pJavaScript->SetElement(pReturnWidth, 0, Width));
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsPerf,
                GetClkDomClkObjIndex,
                2,
                "Get the Clock Domain for Clock Object Index")
{
    STEST_HEADER(2, 2, "Usage: subdev.Perf.GetClkDomClkObjIndex(clkObjIndex, RetValObject)");
    STEST_ARG(0, UINT32, clkObjIndex);
    STEST_ARG(1, JSObject*, pReturnObj);
    STEST_PRIVATE(JsPerf, pJsPerf, "JsPerf");
    Perf40* pPerf40 = dynamic_cast<Perf40*>(pJsPerf->GetPerfObj());
    if (!pPerf40)
        return RC::UNSUPPORTED_FUNCTION;
    RC rc;
    Gpu::ClkDomain clkDom = pPerf40->GetClkObjIndexClkDomain(clkObjIndex);
    pJavaScript->SetElement(pReturnObj, 0, clkDom);
    RETURN_RC(rc);
}

JS_STEST_LWSTOM(JsPerf,
                GetClkPropRegimeIndex,
                2,
                "Get the Array Index for Clock Propagation Regime")
{
    STEST_HEADER(2, 2, "Usage: subdev.Perf.GetClkPropRegimeIndex(propRegimeID, RetValObject)");
    STEST_ARG(0, UINT32, propRegimeID);
    STEST_ARG(1, JSObject*, pReturnObj);
    STEST_PRIVATE(JsPerf, pJsPerf, "JsPerf");
    Perf40* pPerf40 = dynamic_cast<Perf40*>(pJsPerf->GetPerfObj());
    if (!pPerf40)
        return RC::UNSUPPORTED_FUNCTION;
    RC rc;
    UINT32 index;
    C_CHECK_RC(pPerf40->GetClkPropRegimeIndex(propRegimeID, &index));
    pJavaScript->SetElement(pReturnObj, 0, index);
    RETURN_RC(rc);
}

JS_STEST_LWSTOM(JsPerf,
                GetPStates,
                1,
                "Get an array of available PStates")
{
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    JavaScriptPtr pJavaScript;
    JSObject * pReturnArray = 0;
    const char usage[] = "Usage: subdev.Perf.GetPStates(RetArray)\n";

    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &pReturnArray)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        RC rc;
        Perf* pPerf = pJsPerf->GetPerfObj();
        vector<UINT32> PStates;
        C_CHECK_RC(pPerf->GetAvailablePStates(&PStates));
        for (UINT32 i = 0; i < PStates.size(); i++)
        {
            C_CHECK_RC(pJavaScript->SetElement(pReturnArray, i, PStates[i]));
        }
        RETURN_RC(rc);
    }
    return JS_FALSE;
}

JS_SMETHOD_LWSTOM(JsPerf,
                  DoesPStateExist,
                  1,
                  "check if the given state exists on the current board")
{
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    JavaScriptPtr pJavaScript;
    UINT32 PStateNum;
    const char usage[] = "Usage: subdev.Perf.DoesPStateExist(10)\n";

    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &PStateNum)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    bool DoesExist;
    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        RC rc;
        Perf* pPerf = pJsPerf->GetPerfObj();
        C_CHECK_RC(pPerf->DoesPStateExist(PStateNum, &DoesExist));
        if (pJavaScript->ToJsval(DoesExist, pReturlwalue) == OK)
        {
            return JS_TRUE;
        }
    }
    return JS_FALSE;
}

JS_SMETHOD_LWSTOM(JsPerf,
                  ClearDIPState,
                  0,
                  "Empty the stored array of DIPStates")
{
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);
    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        Perf* pPerf = pJsPerf->GetPerfObj();
        MASSERT(pPerf);
        if (OK == pPerf->ClearDIPStates())
        {
            return JS_TRUE;
        }
    }
    return JS_FALSE;
}

JS_SMETHOD_LWSTOM(JsPerf,
                  SetDIPStates,
                  1,
                  "Set the array of DIPStates")
{
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);
    JavaScriptPtr pJs;
    JsPerf *pJsPerf;
    JsArray InputDIPstates;

    if ((NumArguments != 1) ||
        (OK != pJs->FromJsval(pArguments[0], &InputDIPstates)))
    {
        JS_ReportError(pContext, "Failed to set supported DIPStates"
                " usage: GpuSubdev.Perf.SetDIPStates([pstate0, psate1])");
        return JS_FALSE;
    }
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        Perf* pPerf = pJsPerf->GetPerfObj();

        UINT32 DIPstate;
        vector <UINT32> DIPstates;
        for (UINT32 i = 0; i < InputDIPstates.size(); i++)
        {
            if (OK != (pJs->FromJsval(InputDIPstates[i], &DIPstate)))
            {
                JS_ReportError(pContext, "Failed to set supported DIPStates");
                return JS_FALSE;
            }
            DIPstates.push_back(DIPstate);
        }
        if (OK == pPerf->SetDIPStates(DIPstates))
        {
            return JS_TRUE;
        }
    }
    return JS_FALSE;
}

JS_SMETHOD_LWSTOM(JsPerf,
                  GetDIPStates,
                  0,
                  "Get the array of DIPStates")
{
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        JavaScriptPtr pJavaScript;
        Perf* pPerf = pJsPerf->GetPerfObj();
        vector <UINT32> pstates;
        RC rc;
        if ((rc = pPerf->GetDIPStates(&pstates))!= OK)
        {
            JS_ReportError(pContext, "Error in reading supported DIPstates"
                    " usage: Arr = GpuSubdev.Perf.GetDIPStates()");
            *pReturlwalue = JSVAL_NULL;
            return JS_FALSE;
        }

        if (pJavaScript->ToJsval(pstates, pReturlwalue) == 0)
        {
            return JS_TRUE;
        }
    }
    return JS_FALSE;
}

// Purpose of 'Fallback'
// There are only several PState entries available for each sku.
// If the user selects a state number that does not exist, 'Fallback'
// will pick the next higher or next lower pState for the user
PROP_CONST(JsPerf, FallbackHi,    LW2080_CTRL_PERF_PSTATE_FALLBACK_HIGHER_PERF);
PROP_CONST(JsPerf, FallbackLo,  LW2080_CTRL_PERF_PSTATE_FALLBACK_LOWER_PERF);

JS_STEST_LWSTOM(JsPerf,
                SetPState,
                2,
                "Attempt to set the current pState")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);

    const char usage[] = "Usage: subdev.Perf.SetPState(TargetState, Fallback)";

    if((NumArguments != 1) &&
       (NumArguments != 2))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JavaScriptPtr pJavaScript;
    UINT32 TargetState;
    UINT32 Fallback = LW2080_CTRL_PERF_PSTATE_FALLBACK_RETURN_ERROR;

    if (OK != pJavaScript->FromJsval(pArguments[0], &TargetState))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }
    if ((NumArguments == 2) &&
        (OK != pJavaScript->FromJsval(pArguments[1], &Fallback)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        RC rc;
        Perf* pPerf = pJsPerf->GetPerfObj();
        if (pPerf->IsPState30Supported())
        {
            C_CHECK_RC(pPerf->SetInflectionPoint(TargetState, Perf::GpcPerf_MAX));
        }
        else
        {
            C_CHECK_RC(pPerf->ForcePState(TargetState, Fallback));
        }
        RETURN_RC(rc);
    }

    return JS_FALSE;
}

JS_STEST_LWSTOM(JsPerf,
                SuggestPState,
                2,
                "Attempt to set the suggested PState")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);

    const char usage[] = "Usage: subdev.Perf.SuggestPState(TargetState)";

    JavaScriptPtr pJS;
    UINT32 TargetState;

    if((NumArguments != 1) ||
       (OK != pJS->FromJsval(pArguments[0], &TargetState)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        RC rc;
        Perf* pPerf = pJsPerf->GetPerfObj();
        C_CHECK_RC(pPerf->SuggestPState(TargetState,
                                        LW2080_CTRL_PERF_PSTATE_FALLBACK_RETURN_ERROR));
        RETURN_RC(rc);
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsPerf,
                SetPStateVoltage,
                3,
                "Attempt to the core voltage for a given pstate")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);

    const char usage[] = "Usage: subdev.Perf.SetPStateVoltage(Domain, VoltMv, PState)";
    JavaScriptPtr pJavaScript;
    UINT32 Domain;
    UINT32 VoltageMv;
    UINT32 PStateNum;   // maybe make this default?
    if (3 != pJavaScript->UnpackArgs(pArguments, NumArguments, "III",
                                     &Domain, &VoltageMv, &PStateNum))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        RC rc;
        Perf* pPerf = pJsPerf->GetPerfObj();
        C_CHECK_RC(pPerf->SetVoltage((Gpu::VoltageDomain)Domain,
                                     VoltageMv,
                                     PStateNum));
        RETURN_RC(rc);
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsPerf,
                SetPStateVoltTuningOffsetMv,
                2,
                "Set a pstate specific voltage offset in a PState 2.0 setting")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);

    const char usage[] = "Usage: subdev.Perf.SetPStateVoltTuningOffsetMv(OffsetInMv,PState)";
    JavaScriptPtr pJavaScript;
    INT32 OffsetMv;
    UINT32 PState;
    if (2 != pJavaScript->UnpackArgs(pArguments, NumArguments, "iI",
                                     &OffsetMv, &PState))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        RC rc;
        Perf* pPerf = pJsPerf->GetPerfObj();
        C_CHECK_RC(pPerf->SetPStateVoltTuningOffsetMv(OffsetMv,PState));
        RETURN_RC(rc);
    }
    return JS_FALSE;
}

RC JsPerf::SetOverrideOVOC(bool override)
{
    return m_pPerf->SetOverrideOVOC(override);
}
JS_STEST_BIND_ONE_ARG(JsPerf, SetOverrideOVOC,
                      Override, bool,
                      "Override clock domain OV/OC limits");

RC JsPerf::SetRailVoltTuningOffsetMv(UINT32 Rail, FLOAT32 OffsetInMv)
{
    Gpu::SplitVoltageDomain voltDom = m_pPerf->RailIdxToGpuSplitVoltageDomain(Rail);
    return m_pPerf->SetRailVoltTuningOffsetuV(voltDom, static_cast<INT32>(OffsetInMv * 1000));
}
JS_STEST_BIND_TWO_ARGS(JsPerf, SetRailVoltTuningOffsetMv,
                       Rail, UINT32,
                       OffsetInMv, FLOAT32,
                       "Set a rail specific voltage offset in a PState 3.0 setting");

RC JsPerf::SetRailClkDomailwoltTuningOffsetMv(UINT32 Rail, UINT32 ClkDomain, FLOAT32 OffsetInMv)
{
    Gpu::SplitVoltageDomain voltDom = m_pPerf->RailIdxToGpuSplitVoltageDomain(Rail);
    return m_pPerf->SetRailClkDomailwoltTuningOffsetuV(
        voltDom, static_cast<Gpu::ClkDomain>(ClkDomain),
        static_cast<INT32>(OffsetInMv * 1000));
}
JS_STEST_BIND_THREE_ARGS(JsPerf, SetRailClkDomailwoltTuningOffsetMv,
                       Rail, UINT32,
                       ClkDomain, UINT32,
                       OffsetInMv, FLOAT32,
                       "Set a rail and clock domain specific voltage offset in a PState 3.0 setting");

RC JsPerf::SetFreqClkDomainOffsetkHz(UINT32 ClkDomain, INT32 FrequencyInkHz)
{
    return m_pPerf->SetFreqClkDomainOffsetkHz(
        static_cast<Gpu::ClkDomain>(ClkDomain), FrequencyInkHz);
}
JS_STEST_BIND_TWO_ARGS(JsPerf, SetFreqClkDomainOffsetkHz,
                       ClkDomain, UINT32,
                       FrequencyInkHz, INT32,
                       "Set a clock domian specific frequency offset in a PState 3.0 setting in kHz");

RC JsPerf::SetFreqClkDomainOffsetPercent(UINT32 ClkDomain, FLOAT32 percent)
{
    return m_pPerf->SetFreqClkDomainOffsetPercent(
        static_cast<Gpu::ClkDomain>(ClkDomain), percent);
}
JS_STEST_BIND_TWO_ARGS(JsPerf, SetFreqClkDomainOffsetPercent,
                       ClkDomain, UINT32,
                       percent, FLOAT32,
                       "Set a clock domain frequency offset based on a percentage");

JS_STEST_LWSTOM(JsPerf,
                GetClkProgEntriesMHz,
                3,
                "Get all clock programming entries per PState/clock domain")
{
    STEST_HEADER(3, 3,
        "Usage: Perf.GetClkProgEntries(pstateNum, clkDomain, outObjEntries");
    STEST_PRIVATE(JsPerf, pJsPerf, "JsPerf");
    STEST_ARG(0, UINT32, pstateNum);
    STEST_ARG(1, UINT32, clkDomNum);
    STEST_ARG(2, JSObject*, pReturnObj);
    map<LwU8, Perf::clkProgEntry> entriesMHz;

    RC rc;
    Perf* pPerf = pJsPerf->GetPerfObj();
    C_CHECK_RC(pPerf->GetClkProgEntriesMHz(pstateNum,
                                           static_cast<Gpu::ClkDomain>(clkDomNum),
                                           Perf::ProgEntryOffset::ENABLED,
                                           &entriesMHz));
    for (map<LwU8, Perf::clkProgEntry>::const_iterator progEntryItr = entriesMHz.begin();
         progEntryItr != entriesMHz.end();
         ++progEntryItr)
    {
        C_CHECK_RC(pJavaScript->SetElement(pReturnObj,
                   progEntryItr->first, progEntryItr->second.entryFreqMHz));
    }

    RETURN_RC(rc);
}

RC JsPerf::SetClkProgrammingTableOffsetkHz(UINT32 TableIdx, INT32 FrequencyInkHz)
{
    return m_pPerf->SetClkProgrammingTableOffsetkHz(TableIdx, FrequencyInkHz);
}
JS_STEST_BIND_TWO_ARGS(JsPerf, SetClkProgrammingTableOffsetkHz,
                       TableIdx, UINT32,
                       FrequencyInkHz, INT32,
                       "Set a Clock Programming Table entry frequency offset in a PState 3.0 setting in kHz");

RC JsPerf::SetClkVfRelationshipOffsetkHz(UINT32 RelIdx, INT32 FrequencyInkHz)
{
    Perf40* pPerf40 = dynamic_cast<Perf40*>(GetPerfObj());
    if (!pPerf40)
        return RC::UNSUPPORTED_FUNCTION;
    return pPerf40->SetClkVfRelationshipOffsetkHz(RelIdx, FrequencyInkHz);
}
JS_STEST_BIND_TWO_ARGS(JsPerf, SetClkVfRelationshipOffsetkHz,
                       RelIdx, UINT32,
                       FrequencyInkHz, INT32,
                       "Set a Clock VF relationship entry freq offset in khz in PState 4.0");

RC JsPerf::GetClkProgrammingTableSlaveRatio
(
    UINT32  tableIdx,
    UINT32  slaveEntryIdx,
    UINT08 *pRatio
)
{
    return m_pPerf->GetClkProgrammingTableSlaveRatio(tableIdx, slaveEntryIdx, pRatio);
}
JS_STEST_LWSTOM(JsPerf,
                GetClkProgrammingTableSlaveRatio,
                3,
                "Get a Clock Programming Table entry frequency offset in a PState 3.0 setting in kHz")  //$
{
    STEST_HEADER(3, 3,
        "Usage: Perf.GetClkProgrammingTableSlaveRatio(tableIdx, slaveEntryIdx, outRatioArr");
    STEST_PRIVATE(JsPerf, pJsPerf, "JsPerf");
    STEST_ARG(0, UINT32, tableIdx);
    STEST_ARG(1, UINT32, slaveEntryIdx);
    STEST_ARG(2, JSObject*, pReturnObj);

    RC rc;
    UINT08 ratio;
    C_CHECK_RC(pJsPerf->GetClkProgrammingTableSlaveRatio(tableIdx,
                                                         slaveEntryIdx,
                                                         &ratio));
    C_CHECK_RC(pJavaScript->SetElement(pReturnObj, 0, ratio));
    RETURN_RC(rc);
}

RC JsPerf::SetClkProgrammingTableSlaveRatio
(
    UINT32 tableIdx,
    UINT32 slaveEntryIdx,
    UINT08 ratio
)
{
    return m_pPerf->SetClkProgrammingTableSlaveRatio(tableIdx, slaveEntryIdx, ratio);
}
JS_STEST_BIND_THREE_ARGS(
    JsPerf, SetClkProgrammingTableSlaveRatio,
    tableIdx, UINT32,
    slaveEntryIdx, UINT32,
    ratio, UINT08,
    "Change the master to slave ratio for the specified clock programming index and slave");

JS_STEST_LWSTOM(JsPerf,
                GetClkVFRelEntriesMHz,
                3,
                "Get all clock VF Rel entries per PState/clock domain")
{
    STEST_HEADER(3, 3,
        "Usage: Perf.GetClkVFRelEntriesMHz(pstateNum, clkDomain, outObjEntries");
    STEST_PRIVATE(JsPerf, pJsPerf, "JsPerf");
    STEST_ARG(0, UINT32, pstateNum);
    STEST_ARG(1, UINT32, clkDomNum);
    STEST_ARG(2, JSObject*, pReturnObj);

    // Reuse the Clock Programming Entry map as the data being fetched
    // is same even from Clock VF relationship
    map<LwU8, Perf::clkProgEntry> entriesMHz;

    RC rc;
    Perf* pPerf = pJsPerf->GetPerfObj();
    C_CHECK_RC(pPerf->GetVfRelEntriesMHz(pstateNum,
                                         static_cast<Gpu::ClkDomain>(clkDomNum),
                                         Perf::ProgEntryOffset::ENABLED,
                                         &entriesMHz));
    for (map<LwU8, Perf::clkProgEntry>::const_iterator progEntryItr = entriesMHz.begin();
         progEntryItr != entriesMHz.end();
         ++progEntryItr)
    {
        C_CHECK_RC(pJavaScript->SetElement(pReturnObj,
                   progEntryItr->first, progEntryItr->second.entryFreqMHz));
    }

    RETURN_RC(rc);
}

RC JsPerf::SetVFPointOffsetkHz(UINT32 TableIdx, INT32 FrequencyInkHz)
{
    return m_pPerf->SetVFPointsOffsetkHz(TableIdx, TableIdx, FrequencyInkHz);
}
JS_STEST_BIND_TWO_ARGS(JsPerf, SetVFPointOffsetkHz,
                       TableIdx, UINT32,
                       FrequencyInkHz, INT32,
                       "Set a VF point frequency offset in a PState 3.0 setting in kHz");

RC JsPerf::SetVFPointOffsetMv(UINT32 TableIdx, FLOAT32 OffsetInMv)
{
    return m_pPerf->SetVFPointsOffsetuV(TableIdx, TableIdx,
                            static_cast<INT32>(OffsetInMv * 1000));
}
JS_STEST_BIND_TWO_ARGS(JsPerf, SetVFPointOffsetMv,
                       TableIdx, UINT32,
                       OffsetInMv, FLOAT32,
                       "Set a VF point voltage offset in a PState 3.0 setting in mV");

RC JsPerf::SetVFPointsOffsetkHz(UINT32 TableIdxStart, UINT32 TableIdxEnd, INT32 FrequencyInkHz)
{
    return m_pPerf->SetVFPointsOffsetkHz(TableIdxStart, TableIdxEnd, FrequencyInkHz);
}
JS_STEST_BIND_THREE_ARGS(JsPerf, SetVFPointsOffsetkHz,
    TableIdxStart, UINT32,
    TableIdxEnd, UINT32,
    FrequencyInkHz, INT32,
    "Set VF points frequency offset in a PState 3.0 setting in kHz");

RC JsPerf::SetVFPointsOffsetMv(UINT32 TableIdxStart, UINT32 TableIdxEnd, FLOAT32 OffsetInMv)
{

    return m_pPerf->SetVFPointsOffsetuV(TableIdxStart, TableIdxEnd,
                                        static_cast<INT32>(OffsetInMv * 1000));
}
JS_STEST_BIND_THREE_ARGS(JsPerf, SetVFPointsOffsetMv,
    TableIdxStart, UINT32,
    TableIdxEnd, UINT32,
    OffsetInMv, FLOAT32,
    "Set VF points voltage offset in a PState 3.0 setting in mV");


JS_STEST_LWSTOM(JsPerf,
                SetVFPointsOffsetkHzForLwrve,
                4,
                "Set VF points frequency offset in KHz")
{
    STEST_HEADER(4, 4, "Usage: GpuSub.Perf.SetVFPointsOffsetkHzForLwrve(LwrveIdx, TableIdxStart, TableIdxEnd, FreqInkHz)");
    STEST_PRIVATE(JsPerf, pJsPerf, "JsPerf");
    STEST_ARG(0, UINT32, LwrveIdx);
    STEST_ARG(1, UINT32, TableIdxStart);
    STEST_ARG(2, UINT32, TableIdxEnd);
    STEST_ARG(3, UINT32, FreqInkHz);

    Perf40* pPerf40 = dynamic_cast<Perf40*>(pJsPerf->GetPerfObj());
    RC rc;
    if (!pPerf40)
        RETURN_RC(RC::UNSUPPORTED_FUNCTION);

    RETURN_RC(pPerf40->SetVFPointsOffsetkHz(TableIdxStart, TableIdxEnd, FreqInkHz, LwrveIdx));
}

JS_STEST_LWSTOM(JsPerf, 
                OverrideDvcoOffsetCodePerVFPoint,
                4,
                "Override DVCO offset code ")
{
    STEST_HEADER(4, 4, "Usage: GpuSub.Perf.OverrideDvcoOffsetCodePerVFPoint(LwrveIdx, TableIdxStart, TableIdxEnd, DvcoOffsetCode)");
    STEST_PRIVATE(JsPerf, pJsPerf, "JsPerf");
    STEST_ARG(0, UINT32, LwrveIdx);
    STEST_ARG(1, UINT32, TableIdxStart);
    STEST_ARG(2, UINT32, TableIdxEnd);
    STEST_ARG(3, UINT32, DvcoOffsetCode);

    Perf40* pPerf40 = dynamic_cast<Perf40*>(pJsPerf->GetPerfObj());
    RC rc;
    if (!pPerf40)
        RETURN_RC(RC::UNSUPPORTED_FUNCTION);

    RETURN_RC(pPerf40->SetVFPointsOffsetDvcoCode(TableIdxStart, TableIdxEnd, DvcoOffsetCode, LwrveIdx));
}

JS_STEST_LWSTOM(JsPerf,
                SetGlobalVoltTuningOffsetMv,
                1,
                "Set a voltage offset in a PState 2.0 setting")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);

    const char usage[] = "Usage: subdev.Perf.SetGlobalVoltTuningOffsetMv(OffsetInMv)";
    JavaScriptPtr pJavaScript;
    INT32 OffsetMv;
    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &OffsetMv)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        RC rc;
        Perf* pPerf = pJsPerf->GetPerfObj();
        C_CHECK_RC(pPerf->SetGlobalVoltTuningOffsetMv(OffsetMv));
        RETURN_RC(rc);
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsPerf,
                SetPerfPtVoltTuningOffsetMv,
                1,
                "Set a current perfpoint voltage offset in a PState 2.0 setting")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);

    const char usage[] = "Usage: subdev.Perf.SetPerfPtVoltTuningOffsetMv(OffsetInMv)";
    JavaScriptPtr pJavaScript;
    INT32 OffsetMv;
    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &OffsetMv)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        RC rc;
        Perf* pPerf = pJsPerf->GetPerfObj();
        C_CHECK_RC(pPerf->SetPerfPtVoltTuningOffsetMv(OffsetMv));
        RETURN_RC(rc);
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsPerf,
                SetClientOffsetMv,
                2,
                "Set a voltage arbiter client offset (PState 2.0)")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);

    const char usage[] = "Usage: subdev.Perf.SetClientOffsetMv(clientNum, offsetMv)";
    JavaScriptPtr pJavaScript;
    INT32 OffsetMv;
    UINT32 Client;
    if ((NumArguments != 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Client)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &OffsetMv)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        RC rc;
        Perf* pPerf = pJsPerf->GetPerfObj();
        C_CHECK_RC(pPerf->SetClientOffsetMv(Client, OffsetMv));
        RETURN_RC(rc);
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsPerf,
                SuggestVminFloor,
                3,
                "Suggest a vmin floor to voltage arbiter (PState 2.0)")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);

    const char usage[] = "Usage: subdev.Perf.SuggestVminFloor(mode, modeArg, offsetMv)";
    JavaScriptPtr pJavaScript;
    UINT32 Mode;
    UINT32 ModeArg;
    INT32 OffsetMv;
    if ((NumArguments != 3) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Mode)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &ModeArg)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &OffsetMv)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        RC rc;
        Perf* pPerf = pJsPerf->GetPerfObj();
        C_CHECK_RC(pPerf->SuggestVminFloor(static_cast<Perf::VminFloorMode>(Mode),
                                           ModeArg, OffsetMv));
        RETURN_RC(rc);
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsPerf,
                SetPStateOffsetMv,
                2,
                "Set a pstate min voltage offset (PState 2.0)")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);

    const char usage[] = "Usage: subdev.Perf.SetPStateOffsetMv(PStateNum, offsetMv)";
    JavaScriptPtr pJavaScript;
    INT32 OffsetMv;
    UINT32 PState;
    if ((NumArguments != 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &PState)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &OffsetMv)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        RC rc;
        Perf* pPerf = pJsPerf->GetPerfObj();
        C_CHECK_RC(pPerf->SetPStateOffsetMv(PState, OffsetMv));
        RETURN_RC(rc);
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsPerf,
                SetVdtTuningOffsetMv,
                1,
                "Set voltage offset in the VDT table tuning entry.")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);

    const char usage[] = "Usage: subdev.Perf.SetVdtTuningOffsetMv(OffsetInMv)";
    JavaScriptPtr pJavaScript;
    INT32 OffsetMv;
    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &OffsetMv)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        RC rc;
        Perf* pPerf = pJsPerf->GetPerfObj();
        C_CHECK_RC(pPerf->SetVdtTuningOffsetMv(OffsetMv));
        RETURN_RC(rc);
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsPerf,
                GetFreqTuningOffsetKHz,
                1,
                "Get a freq. offset for a given pstate (PState 2.0 only).")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);

    const char usage[] = "Usage: subdev.Perf.GetFreqTuningOffsetKHz(pstate)";
    JavaScriptPtr pJS;
    INT32 PState;
    if ((NumArguments != 1) ||
        (OK != pJS->FromJsval(pArguments[0], &PState)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsPerf *pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf");
    if (pJsPerf != 0)
    {
        Perf* pPerf = pJsPerf->GetPerfObj();
        if (pJS->ToJsval(pPerf->GetFreqTuningOffsetKHz(PState),pReturlwalue) == OK)
        {
            return JS_TRUE;
        }
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsPerf,
                SetFreqTuningOffsetKHz,
                2,
                "Set a freq. offset for a given pstate (PState 2.0 only).")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);

    const char usage[] = "Usage: subdev.Perf.SetFreqTuningOffsetKHz(OffsetInKHz,pstate)";
    JavaScriptPtr pJavaScript;
    INT32 OffsetKHz;
    UINT32 PState;
    if (2 != pJavaScript->UnpackArgs(pArguments, NumArguments, "iI",
                                     &OffsetKHz, &PState))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        RC rc;
        Perf* pPerf = pJsPerf->GetPerfObj();
        C_CHECK_RC(pPerf->SetFreqTuningOffsetKHz(OffsetKHz,PState));
        RETURN_RC(rc);
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsPerf,
                SetGpc2PerfClk,
                2,
                "Attempt set GPC2 Perf clock. With PState 2.0, pull related perf parameters with GPC2")
{
    MASSERT(pContext && pArguments);

    Printf(Tee::PriWarn, "SetGpc2PerfClk() is deprecated. Use SetGpcPerfClk() instead\n");

    JavaScriptPtr pJs;
    UINT64 FreqHz;
    UINT32 PStateNum;
    if (2 != pJs->UnpackArgs(pArguments, NumArguments, "II",
                             &PStateNum, &FreqHz))
    {
        JS_ReportError(pContext, "Usage: subdev.Perf.SetGpc2PerfClk(PState, FreqHz)");
        return JS_FALSE;
    }

    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        RC rc;
        Perf* pPerf = pJsPerf->GetPerfObj();

        // User has provided 2x clock, but MODS uses 1x clock internally
        // so divide input frequency by 2
        C_CHECK_RC(pPerf->SetGpcPerfClk(PStateNum, FreqHz / 2));
        RETURN_RC(rc);
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsPerf,
                SetGpcPerfClk,
                2,
                "Attempt to set GPC Perf clock")
{
    RC rc;
    STEST_HEADER(2, 2, "Usage: GpuSub.Perf.SetGpcPerfClk(PStateNum, FreqHz)");
    STEST_PRIVATE(JsPerf, pJsPerf, "JsPerf");
    STEST_ARG(0, UINT32, pstateNum);
    STEST_ARG(1, UINT32, freqHz);

    Perf* pPerf = pJsPerf->GetPerfObj();
    C_CHECK_RC(pPerf->SetGpcPerfClk(pstateNum, freqHz));

    RETURN_RC(rc);
}

JS_STEST_LWSTOM(JsPerf,
                SetInflectionPoint,
                2,
                "Select an inflection point")
{
    MASSERT(pContext && pArguments);

    JavaScriptPtr pJs;
    UINT32 PStateNum;
    string LocationStr;
    if (2 != pJs->UnpackArgs(pArguments, NumArguments, "Is",
                             &PStateNum, &LocationStr))
    {
        JS_ReportError(pContext, "Usage: subdev.Perf.SetInflectionPoint(PState, min/max/nom)");
        return JS_FALSE;
    }

    LocationStr = Utility::ToLowerCase(LocationStr);
    UINT32 Location = Perf::StringToSpecialPt(LocationStr);
    JsPerf *pJsPerf;

    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        RC rc;
        Perf* pPerf = pJsPerf->GetPerfObj();
        if (pPerf->IsPState30Supported() &&
            static_cast<Perf::GpcPerfMode>(Location) == Perf::GpcPerf_INTERSECT)
        {
            Perf::PerfPoint pp;
            pp.PStateNum = PStateNum;
            pp.SpecialPt = Perf::GpcPerf_INTERSECT;
            Perf::IntersectSetting is;
            is.Type = Perf::IntersectPState;
            if (LocationStr == "intersect" ||
                LocationStr == "intersect.lwvdd" ||
                LocationStr == "intersect.logic")
            {
                is.VoltageDomain = Gpu::VOLTAGE_LOGIC;
            }
            else if (LocationStr == "intersect.lwvdds" ||
                     LocationStr == "intersect.sram")
            {
                is.VoltageDomain = Gpu::VOLTAGE_SRAM;
            }
            else if (LocationStr == "intersect.msvdd")
            {
                is.VoltageDomain = Gpu::VOLTAGE_MSVDD;
            }
            pp.IntersectSettings.insert(is);
            C_CHECK_RC(pPerf->SetPerfPoint(pp));
        }
        else
        {
            C_CHECK_RC(pPerf->SetInflectionPoint(PStateNum, Location));
        }
        RETURN_RC(rc);
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsPerf,
                OverrideInflectionPt,
                3,
                "Override an inflection point with a PerfPoint")
{
    MASSERT(pContext && pArguments);

    JavaScriptPtr pJs;
    UINT32 PStateNum;
    string LocationStr;
    JSObject * pJsPerfPt = 0;
    if (3 != pJs->UnpackArgs(pArguments, NumArguments, "Iso",
                             &PStateNum, &LocationStr, &pJsPerfPt))
    {
        JS_ReportError(pContext, "Usage: subdev.Perf.OverrideInflectionPt(PState, min/max/nom/intersect/tdp/midpoint/turbo, PerfPoint)");
        return JS_FALSE;
    }

    LocationStr = Utility::ToLowerCase(LocationStr);
    UINT32 Location = Perf::StringToSpecialPt(LocationStr);

    RC rc;
    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        Perf* pPerf = pJsPerf->GetPerfObj();
        Perf::PerfPoint NewPerfPt;
        C_CHECK_RC(pPerf->JsObjToPerfPoint(pJsPerfPt, &NewPerfPt));
        C_CHECK_RC(pPerf->OverrideInflectionPt(PStateNum, Location, NewPerfPt));
        RETURN_RC(rc);
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsPerf,
                SetCallbacks,
                2,
                "Set function names of the callback functions")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);

    const char usage[] = "Usage: subdev.Perf.SetCallbacks(CallbackType, CallbackName)";

    JavaScriptPtr pJavaScript;
    UINT32 CallbackType;
    string CallbackName;
    if (2 != pJavaScript->UnpackArgs(pArguments, NumArguments, "Is",
                                     &CallbackType, &CallbackName))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        RC rc;
        Perf* pPerf = pJsPerf->GetPerfObj();
        C_CHECK_RC(pPerf->SetUseCallbacks((Perf::PerfCallback)CallbackType,
                                          CallbackName));
        RETURN_RC(rc);
    }

    return JS_FALSE;
}

JS_STEST_LWSTOM(JsPerf,
                PrintPStateTable,
                0,
                "print out the pstate table")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);

    const char usage[] = "Usage: subdev.Perf.PrintPStateTable()";

    if(NumArguments != 0)
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        RC rc;
        Perf* pPerf = pJsPerf->GetPerfObj();
        C_CHECK_RC(pPerf->PrintAllPStateInfo());
        RETURN_RC(rc);
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsPerf,
                RestorePStateTable,
                1,
                "RestorePStateTable")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);

    JavaScriptPtr pJavaScript;
    UINT32 pstate = Perf::ILWALID_PSTATE;

    if ((NumArguments == 1 &&
         (OK != pJavaScript->FromJsval(pArguments[0], &pstate))) ||
        (NumArguments > 1))
    {
        JS_ReportError(pContext, "Usage: subdev.Perf.RestorePStateTable(<pstate>)");
        return JS_FALSE;
    }

    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        RC rc;
        Perf* pPerf = pJsPerf->GetPerfObj();
        C_CHECK_RC(pPerf->RestorePStateTable(pPerf->PStateNumTo2080Bit(pstate),
                                             Perf::AllDomains));
        RETURN_RC(rc);
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsPerf,
                ClearVoltTuningOffsets,
                0,
                "ClearVoltTuningOffsets")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);

    if (NumArguments != 0)
    {
        JS_ReportError(pContext, "Usage: subdev.Perf.ClearVoltTuningOffsets()");
        return JS_FALSE;
    }

    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        Perf* pPerf = pJsPerf->GetPerfObj();
        RETURN_RC(pPerf->ClearVoltTuningOffsets());
    }
    return JS_FALSE;
}

//! This will clear the PState for RM. At the same time, this will turn
// off MODS's PState monitor thread
JS_STEST_LWSTOM(JsPerf,
                ClearPState,
                0,
                "ClearPState")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);

    const char usage[] = "Usage: subdev.Perf.ClearPState()";

    if(NumArguments != 0)
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        RC rc;
        Perf* pPerf = pJsPerf->GetPerfObj();
        C_CHECK_RC(pPerf->ClearForcedPState());
        RETURN_RC(rc);
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsPerf,
                PrintVFTable,
                0,
                "Print VF Table")
{
    STEST_HEADER(0, 0, "Usage: subdev.Perf.PrintVFTable()");
    STEST_PRIVATE(JsPerf, pJsPerf, "JsPerf");
    Perf40* pPerf40 = dynamic_cast<Perf40*>(pJsPerf->GetPerfObj());
    RC rc;
    if (!pPerf40)
        RETURN_RC(RC::UNSUPPORTED_FUNCTION);

    RETURN_RC(pPerf40->PrintVFTable());
}


JS_CLASS(ClockEnum);

JS_STEST_LWSTOM(JsPerf,
                GetClkEnumsInfo,
                1,
                "Get the Clock Enumerations Information")
{
    STEST_HEADER(1, 1, "Usage: subdev.Perf.GetClkEnumsInfo(RetArray)");
    STEST_ARG(0, JSObject*, pClkEnumsInfo);
    STEST_PRIVATE(JsPerf, pJsPerf, "JsPerf");
    Perf40* pPerf40 = dynamic_cast<Perf40*>(pJsPerf->GetPerfObj());
    RC rc;
    UINT32 i = 0;
    if (!pPerf40)
        RETURN_RC(RC::UNSUPPORTED_FUNCTION);

    const LW2080_CTRL_CLK_CLK_ENUMS_INFO* clkEnums = pPerf40->GetClkEnumsInfo();

    // Each CLK_ENUM Object
    LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(i, &clkEnums->super.objMask.super)
    {
         const LW2080_CTRL_CLK_CLK_ENUM_INFO  *pEnumRmInfo = &clkEnums->enums[i];

         JSObject* clkEnumInfo = JS_NewObject(pContext, &ClockEnumClass, NULL, NULL);
         C_CHECK_RC(pJavaScript->SetProperty(clkEnumInfo, "type", pEnumRmInfo->super.type));
         C_CHECK_RC(pJavaScript->SetProperty(clkEnumInfo, "bOCOVEnabled", pEnumRmInfo->bOCOVEnabled));
         C_CHECK_RC(pJavaScript->SetProperty(clkEnumInfo, "freqMinMHz", pEnumRmInfo->freqMinMHz));
         C_CHECK_RC(pJavaScript->SetProperty(clkEnumInfo, "freqMaxMHz", pEnumRmInfo->freqMaxMHz));
         C_CHECK_RC(pJavaScript->SetProperty(clkEnumInfo, "clkDomain", pPerf40->GetDomainFromClockProgrammingOrder(i)));
         C_CHECK_RC(pJavaScript->SetElement(pClkEnumsInfo, i, clkEnumInfo));
    }
    LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END;
    RETURN_RC(rc);
}

JS_CLASS(ClockPropTopInfo);

JS_STEST_LWSTOM(JsPerf,
                GetClkPropTopsInfo,
                1,
                "Get the clock propagation topologies information")
{
    STEST_HEADER(1, 1, "Usage: subdev.Perf.GetClkPropTopsInfo(RetArray)");
    STEST_ARG(0, JSObject*, pJsPropTopsInfo);
    STEST_PRIVATE(JsPerf, pJsPerf, "JsPerf");
    Perf40* pPerf40 = dynamic_cast<Perf40*>(pJsPerf->GetPerfObj());
    RC rc;
    UINT32 i = 0, ii = 0, iii = 0;
    if (!pPerf40)
    {
        Printf(Tee::PriError,
               "GetClkPropTopsInfo is only supported on pstates 4.0+ chips\n");
        RETURN_RC(RC::UNSUPPORTED_FUNCTION);
    }

    const LW2080_CTRL_CLK_CLK_PROP_TOPS_INFO* clkPropTops = pPerf40->GetClkPropTopsInfo();
    // Go through all valid topologies
    LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(i, &clkPropTops->super.objMask.super)
        const LW2080_CTRL_CLK_CLK_PROP_TOP_INFO &propTopInfo = clkPropTops->propTops[i];
        JSObject* pJsPropTopInfo = JS_NewObject(pContext, &ClockPropTopInfoClass, NULL, NULL);
        C_CHECK_RC(pJavaScript->SetProperty(pJsPropTopInfo, "topId", propTopInfo.topId));
        C_CHECK_RC(pJavaScript->SetProperty(pJsPropTopInfo, "clkPropTopRelMask", propTopInfo.clkPropTopRelMask.super.pData[0]));

        JSObject* pJsPropTopRels = JS_NewArrayObject(pContext, 0, NULL);
        // Go through all valid topology relationships
        LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(ii, &propTopInfo.clkPropTopRelMask.super)
            JSObject* pJsPropTopRelDst = JS_NewArrayObject(pContext, 0, NULL);
            LW2080_CTRL_CLK_CLK_PROP_TOP_CLK_DOMAIN_DST_PATH srcDstPath = propTopInfo.domainsDstPath.domainDstPath[ii];
            LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(iii, &propTopInfo.clkPropTopRelMask.super)
                // This value can be LW2080_CTRL_BOARDOBJ_IDX_ILWALID
                UINT32 relIndex = srcDstPath.dstPath[iii];
                if (relIndex == LW2080_CTRL_BOARDOBJ_IDX_ILWALID)
                {
                    continue;
                }
                C_CHECK_RC(pJavaScript->SetElement(pJsPropTopRelDst, iii, relIndex));
            LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END
            C_CHECK_RC(pJavaScript->SetElement(pJsPropTopRels, ii, pJsPropTopRelDst));
        LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END
        C_CHECK_RC(pJavaScript->SetProperty(pJsPropTopInfo, "domainsDstPath", pJsPropTopRels));
        C_CHECK_RC(pJavaScript->SetElement(pJsPropTopsInfo, propTopInfo.topId, pJsPropTopInfo));
    LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END
    RETURN_RC(rc);
}


JS_STEST_LWSTOM(JsPerf,
                SetPropagationTopology,
                1,
                "Set the index of clock propagation topology")
{
    STEST_HEADER(1, 1, "Usage: subdev.Perf.SetPropagationTopology(topId)");
    STEST_ARG(0, UINT32, topId);
    STEST_PRIVATE(JsPerf, pJsPerf, "JsPerf");
    Perf40* pPerf40 = dynamic_cast<Perf40*>(pJsPerf->GetPerfObj());
    RC rc;
    if (!pPerf40)
    {
        Printf(Tee::PriError,
               "SetPropagationTopology is only supported on pstates 4.0+ chips\n");
        RETURN_RC(RC::UNSUPPORTED_FUNCTION);
    }

    // Check for valid topId
    UINT32 i = 0;
    bool validTopId = false;
    const LW2080_CTRL_CLK_CLK_PROP_TOPS_INFO* clkPropTops = pPerf40->GetClkPropTopsInfo();
    LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(i, &clkPropTops->super.objMask.super)
        const LW2080_CTRL_CLK_CLK_PROP_TOP_INFO &propTopInfo = clkPropTops->propTops[i];
        if (propTopInfo.topId == topId)
        {
            validTopId = true;
            break;
        }
    LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END

    if (!validTopId)
    {
        Printf(Tee::PriError,
               "Invalid topId passed to subdev.Perf.SetPropagationTopology(topId)\n");
        RETURN_RC(RC::SOFTWARE_ERROR);
    }

    // Get control
    LW2080_CTRL_CLK_CLK_PROP_TOPS_CONTROL clkPropTopControl = {};
    C_CHECK_RC(pPerf40->GetClkPropTopControl(&clkPropTopControl));

    // Set control
    clkPropTopControl.activeTopIdForced = topId;
    C_CHECK_RC(pPerf40->SetClkPropTopControl(&clkPropTopControl));

    RETURN_RC(rc);
}

JS_STEST_LWSTOM(JsPerf,
                GetLwrrentPropagationTopology,
                1,
                "Get the index of current clock propagation topology")
{
    STEST_HEADER(1, 1, "Usage: subdev.Perf.GetLwrrentPropagationTopology(topId)");
    STEST_ARG(0, JSObject *, topId);
    STEST_PRIVATE(JsPerf, pJsPerf, "JsPerf");
    Perf40* pPerf40 = dynamic_cast<Perf40*>(pJsPerf->GetPerfObj());
    RC rc;
    if (!pPerf40)
    {
        Printf(Tee::PriError,
               "GetLwrrentPropagationTopology is only supported on pstates 4.0+ chips\n");
        RETURN_RC(RC::UNSUPPORTED_FUNCTION);
    }

    LW2080_CTRL_CLK_CLK_PROP_TOPS_STATUS clkPropTopsStatus = {};
    C_CHECK_RC(pPerf40->GetClkPropTopStatus(&clkPropTopsStatus));
    C_CHECK_RC(pJavaScript->SetElement(topId, 0, clkPropTopsStatus.activeTopId));
    RETURN_RC(rc);
}

JS_SMETHOD_LWSTOM(JsPerf,
                  GetDomainFromClockProgrammingOrder,
                  1,
                  "Return the corresponding Gpu.ClkXXX enum value for a given clock entry index to Clock Domain Enumeration table")
{
    STEST_HEADER(1, 1, "Usage: let clkDom = subdev.Perf.GetDomainFromClockProgrammingOrder(clkIdx)");
    STEST_ARG(0, UINT32, clkIdx);
    STEST_PRIVATE(JsPerf, pJsPerf, "JsPerf");
    Perf *pPerf = pJsPerf->GetPerfObj();

    RC rc;
    const Gpu::ClkDomain clkDom = pPerf->GetDomainFromClockProgrammingOrder(clkIdx);
    rc = pJavaScript->ToJsval(clkDom, pReturlwalue);
    if (RC::OK != rc)
    {
        pJavaScript->Throw(pContext, rc, "Error oclwrred in .Perf.GetDomainFromClockProgrammingOrder");
        return JS_FALSE;
    }

    return JS_TRUE;
}

JS_CLASS(ClockVFRel);
JS_CLASS(VfEntryPri);
JS_CLASS(VfEntrySec);
JS_CLASS(VfEntrySecPerIndex);
JS_CLASS(VfRelRatioFreq);
JS_CLASS(VfRelRatioFreqSlave);
JS_CLASS(VfRelRatioVolt);
JS_CLASS(VfRelTableFreq);
JS_CLASS(VfRelTableFreqSlave);

JS_STEST_LWSTOM(JsPerf,
                GetClkVfRelsInfo,
                1,
                "Get the clock VF relationships information")
{
    STEST_HEADER(1, 1, "Usage: subdev.Perf.GetClkVfRelsInfo(RetArray)");
    STEST_ARG(0, JSObject*, pJsClkVfRelsInfo);
    STEST_PRIVATE(JsPerf, pJsPerf, "JsPerf");
    Perf40* pPerf40 = dynamic_cast<Perf40*>(pJsPerf->GetPerfObj());
    RC rc;
    UINT32 i = 0;
    if (!pPerf40)
        RETURN_RC(RC::UNSUPPORTED_FUNCTION);

    const LW2080_CTRL_CLK_CLK_VF_RELS_INFO* pClkVfRels = pPerf40->GetClkVfRelsInfo();
    LW2080_CTRL_BOARDOBJGRP_MASK_E255_FOR_EACH_INDEX(i, &pClkVfRels->super.objMask.super)
    {
        const LW2080_CTRL_CLK_CLK_VF_REL_INFO* pClkVfRelInfo = &pClkVfRels->vfRels[i];
        JSObject* pJsClkVfRelInfo = JS_NewObject(pContext, &ClockVFRelClass, NULL, NULL);
        C_CHECK_RC(pJavaScript->SetProperty(pJsClkVfRelInfo, "type", pClkVfRelInfo->super.type));
        //  Index of the VOLTAGE_RAIL for this CLK_VF_REL object.
        C_CHECK_RC(pJavaScript->SetProperty(pJsClkVfRelInfo, "railIdx", pClkVfRelInfo->railIdx));
        // Boolean flag indicating whether this entry supports OC/OV
        C_CHECK_RC(pJavaScript->SetProperty(pJsClkVfRelInfo, "bOCOVEnabled",
                                            pClkVfRelInfo->bOCOVEnabled));
        //  Maximum frequency for this CLK_VF_REL entry
        C_CHECK_RC(pJavaScript->SetProperty(pJsClkVfRelInfo, "freqMaxMHz",
                                            pClkVfRelInfo->freqMaxMHz));

        // Params to generate Primary VF lwrve for this VF_REL class
        // VFE index which describes the VF equation for this VF_REL entry
        JSObject* pJsVfEntryPri = JS_NewObject(pContext, &VfEntryPriClass, NULL, NULL);
        C_CHECK_RC(pJavaScript->SetProperty(pJsVfEntryPri, "vfeIdx",
                                            pClkVfRelInfo->vfEntryPri.vfeIdx));
        C_CHECK_RC(pJavaScript->SetProperty(pJsVfEntryPri, "cpmMaxFreqOffsetVfeIdx",
                                            pClkVfRelInfo->vfEntryPri.cpmMaxFreqOffsetVfeIdx));
        // The set described by the range [vfPointIdxFirst, @ref vfPointIdxLast]
        // represents the VF lwrve described by this object.
        C_CHECK_RC(pJavaScript->SetProperty(pJsVfEntryPri, "vfPointIdxFirst",
                                            pClkVfRelInfo->vfEntryPri.vfPointIdxFirst));
        C_CHECK_RC(pJavaScript->SetProperty(pJsVfEntryPri, "vfPointIdxLast",
                                            pClkVfRelInfo->vfEntryPri.vfPointIdxLast));
        C_CHECK_RC(pJavaScript->SetProperty(pJsClkVfRelInfo, "vfLwrvePrimary",
                                            pJsVfEntryPri));

        //Array of secondary VF Lwrve parameters
        JSObject* pJsVfEntrySec = JS_NewObject(pContext, &VfEntrySecClass, NULL, NULL);
        for (UINT32 index = 0; index < pClkVfRels->vfEntryCountSec; index++)
        {
            JSObject* vfEntrySecPerIndex = JS_NewObject(pContext, &VfEntrySecPerIndexClass, NULL,
                                                        NULL);
            C_CHECK_RC(pJavaScript->SetProperty(vfEntrySecPerIndex, "vfeIdx",
                       pClkVfRelInfo->vfEntriesSec[index].vfeIdx));
            C_CHECK_RC(pJavaScript->SetProperty(vfEntrySecPerIndex, "dvcoOffsetVfeIdx",
                       pClkVfRelInfo->vfEntriesSec[index].dvcoOffsetVfeIdx));
            C_CHECK_RC(pJavaScript->SetProperty(vfEntrySecPerIndex, "vfPointIdxFirst",
                       pClkVfRelInfo->vfEntriesSec[index].vfPointIdxFirst));
            C_CHECK_RC(pJavaScript->SetProperty(vfEntrySecPerIndex, "vfPointIdxLast",
                       pClkVfRelInfo->vfEntriesSec[index].vfPointIdxLast));
            C_CHECK_RC(pJavaScript->SetElement(pJsVfEntrySec, index, vfEntrySecPerIndex));
        }
        C_CHECK_RC(pJavaScript->SetProperty(pJsClkVfRelInfo, "vfLwrveSecondary", pJsVfEntrySec));

        //type-specific data union
        switch (pClkVfRelInfo->super.type)
        {
            // SLAVE Clock Domain's VF lwrve as a ratio function of the MASTER Clock
            // Domain's VF lwrve.
            case LW2080_CTRL_CLK_CLK_VF_REL_TYPE_RATIO_FREQ:
            {
                JSObject* pJsVfRelRatioFreq = JS_NewObject(pContext, &VfRelRatioFreqClass, NULL, NULL);
                for (UINT32 index = 0; index < pClkVfRels->slaveEntryCount; index++)
                {
                    JSObject* pJsVfRelRatioFreqSlave = JS_NewObject(pContext,
                                                       &VfRelRatioFreqSlaveClass, NULL, NULL);
                    C_CHECK_RC(pJavaScript->SetProperty(pJsVfRelRatioFreqSlave, "clkDomIdx",
                               pClkVfRelInfo->data.ratio.slaveEntries[index].clkDomIdx));
                    C_CHECK_RC(pJavaScript->SetProperty(pJsVfRelRatioFreqSlave, "ratio",
                               pClkVfRelInfo->data.ratio.slaveEntries[index].ratio));
                    C_CHECK_RC(pJavaScript->SetElement(pJsVfRelRatioFreq, index,
                               pJsVfRelRatioFreqSlave));
                }
                C_CHECK_RC(pJavaScript->SetProperty(pJsClkVfRelInfo, "vfRelTypeRatioFreq",
                                                    pJsVfRelRatioFreq));
                break;
            }
            case LW2080_CTRL_CLK_CLK_VF_REL_TYPE_RATIO_VOLT:
            {
                JSObject* pJsVfRelRatioVolt = JS_NewObject(pContext, &VfRelRatioVoltClass, NULL, NULL);
                JSObject* pJsVfRelRatioVoltFreq = JS_NewObject(pContext, &VfRelRatioFreqClass, NULL, NULL);
                C_CHECK_RC(pJavaScript->SetProperty(pJsVfRelRatioVolt, "baseVFSmoothVoltuV",
                           pClkVfRelInfo->data.ratioVolt.vfSmoothData.baseVFSmoothVoltuV));
                C_CHECK_RC(pJavaScript->SetProperty(pJsVfRelRatioVolt, "maxVFRampRate",
                           pClkVfRelInfo->data.ratioVolt.vfSmoothData.maxVFRampRate));
                C_CHECK_RC(pJavaScript->SetProperty(pJsVfRelRatioVolt, "maxFreqStepSizeMHz",
                           pClkVfRelInfo->data.ratioVolt.vfSmoothData.maxFreqStepSizeMHz));
                for (UINT32 index = 0; index < pClkVfRels->slaveEntryCount; index++)
                {
                    JSObject* pJsVfRelRatioFreqSlave = JS_NewObject(pContext,
                                                       &VfRelRatioFreqSlaveClass, NULL, NULL);
                    C_CHECK_RC(pJavaScript->SetProperty(pJsVfRelRatioFreqSlave, "clkDomIdx",
                               pClkVfRelInfo->data.ratioVolt.super.slaveEntries[index].clkDomIdx));
                    C_CHECK_RC(pJavaScript->SetProperty(pJsVfRelRatioFreqSlave, "ratio",
                               pClkVfRelInfo->data.ratioVolt.super.slaveEntries[index].ratio));
                    C_CHECK_RC(pJavaScript->SetElement(pJsVfRelRatioVoltFreq, index,
                               pJsVfRelRatioFreqSlave));
                }
                C_CHECK_RC(pJavaScript->SetProperty(pJsVfRelRatioVolt, "vfRelTypeRatioVoltFreq",
                                                    pJsVfRelRatioVoltFreq));
                C_CHECK_RC(pJavaScript->SetProperty(pJsClkVfRelInfo, "vfRelTypeRatioVolt",
                                                    pJsVfRelRatioVolt));
                break;
            }
            // SLAVE Clock Domain's VF lwrve is a table-lookup function of the
            // MASTER Clock Domain's VF lwrve.
            case LW2080_CTRL_CLK_CLK_VF_REL_TYPE_TABLE_FREQ:
            {
                JSObject* pJsVfRelTableFreq = JS_NewObject(pContext, &VfRelTableFreqClass, NULL, NULL);
                for (UINT32 index = 0; index < pClkVfRels->slaveEntryCount; index++)
                {
                    JSObject* pJsVfRelTableFreqSlaveEntry = JS_NewObject(pContext,
                                                            &VfRelTableFreqSlaveClass, NULL, NULL);
                    C_CHECK_RC(pJavaScript->SetProperty(pJsVfRelTableFreqSlaveEntry, "clkDomIdx",
                               pClkVfRelInfo->data.table.slaveEntries[index].clkDomIdx));
                    C_CHECK_RC(pJavaScript->SetProperty(pJsVfRelTableFreqSlaveEntry, "freqMHz",
                               pClkVfRelInfo->data.table.slaveEntries[index].freqMHz));
                    C_CHECK_RC(pJavaScript->SetElement(pJsVfRelTableFreq, index,
                                                       pJsVfRelTableFreqSlaveEntry));
                }
                C_CHECK_RC(pJavaScript->SetProperty(pJsClkVfRelInfo, "vfRelInfoTable",
                                                    pJsVfRelTableFreq));
                break;
            }
        }
        C_CHECK_RC(pJavaScript->SetElement(pJsClkVfRelsInfo, i, pJsClkVfRelInfo));
    }
    LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END;
    RETURN_RC(rc);
}

JS_CLASS(VfRelControlRatioFreqSlave);
JS_STEST_LWSTOM(JsPerf,
                GetSlaveFreqRatioForClkVfRel,
                2,
                "Get the ratio of slave entry for the given VF rel")
{
    STEST_HEADER(2, 2, "Usage: Populate the return array with the slave frequency ratios");
    UINT32 vfRelIdx;
    if (RC::OK != pJavaScript->FromJsval(pArguments[0], &vfRelIdx))
    {
        pJavaScript->Throw(pContext, RC::SOFTWARE_ERROR,
            "Usage: GpuSubdev.Perf.GetSlaveFreqRatioForClkVfRel"
            "(inputVfRelIdx, RetArray)");
        return JS_FALSE;
    }
    STEST_ARG(1, JSObject*, pJsClkVfRelSlaveEntryRatio);

    STEST_PRIVATE(JsPerf, pJsPerf, "JsPerf");
    Perf40* pPerf40 = dynamic_cast<Perf40*>(pJsPerf->GetPerfObj());
    RC rc;
    if (!pPerf40)
        RETURN_RC(RC::UNSUPPORTED_FUNCTION);

    LW2080_CTRL_CLK_CLK_VF_RELS_CONTROL clkVfRelsControl;
    C_CHECK_RC(pPerf40->GetClkVfRelsControl(&clkVfRelsControl));
    const LW2080_CTRL_CLK_CLK_VF_RELS_INFO* relsInfo = pPerf40->GetClkVfRelsInfo();

    if (vfRelIdx >= LW2080_CTRL_BOARDOBJGRP_E255_MAX_OBJECTS)
    {
        Printf(Tee::PriError, "vfRelIdx = %u is invalid \n", vfRelIdx);
        return RC::SOFTWARE_ERROR;
    }
    const  LW2080_CTRL_CLK_CLK_VF_REL_CONTROL* pClkVfRelControl =
                               &clkVfRelsControl.vfRels[vfRelIdx];
        if (pClkVfRelControl->super.type != LW2080_CTRL_CLK_CLK_VF_REL_TYPE_RATIO_FREQ
            && pClkVfRelControl->super.type != LW2080_CTRL_CLK_CLK_VF_REL_TYPE_RATIO_VOLT)
        {
            Printf(Tee::PriError,
                   "type = %u is not LW2080_CTRL_CLK_CLK_VF_REL_TYPE_RATIO_FREQ"
                   "or LW2080_CTRL_CLK_CLK_VF_REL_TYPE_RATIO_VOLT \n",
                   pClkVfRelControl->super.type);
            return RC::SOFTWARE_ERROR;
        }
        for (UINT32 index = 0; index < relsInfo->slaveEntryCount; index++)
        {
            JSObject* pJsVfRelRatioFreqSlave = JS_NewObject(pContext,
                                               &VfRelControlRatioFreqSlaveClass, NULL, NULL);
            C_CHECK_RC(pJavaScript->SetProperty(pJsVfRelRatioFreqSlave, "ratio",
                       pClkVfRelControl->data.ratio.slaveEntries[index].ratio));
            C_CHECK_RC(pJavaScript->SetElement(pJsClkVfRelSlaveEntryRatio, index,
                       pJsVfRelRatioFreqSlave));
    }
    RETURN_RC(rc);
}

//JS class used in GetClkProRegimesInfo
JS_CLASS(ClockPropRegime);

//Return value is a JS Object with the clock propogation table header (array).
//Each member of the array contains regimeId and clkDomainMask
//Propagation Regimes defines the mask of clock domains that needs to be programmed together to meet the
//expected functionality where the expectation could be optimal Perf, Power or some functional dependency
JS_STEST_LWSTOM(JsPerf,
                GetClkProRegimesInfo,
                1,
                "Get the Clock Propagation Regimes Information")
{
    STEST_HEADER(1, 1, "Usage: subdev.Perf.GetClkPropRegimesInfo(RetArray)");
    STEST_ARG(0, JSObject*, pClkPropRegimesInfo);
    STEST_PRIVATE(JsPerf, pJsPerf, "JsPerf");
    Perf40* pPerf40 = dynamic_cast<Perf40*>(pJsPerf->GetPerfObj());
    RC rc;
    UINT32 i = 0;
    if (!pPerf40)
        RETURN_RC(RC::UNSUPPORTED_FUNCTION);

    LW2080_CTRL_CLK_CLK_PROP_REGIMES_INFO clkPropRegimes;
    CHECK_RC(pPerf40->GetClkPropRegimesInfo(&clkPropRegimes));

    // Each CLK_ENUM Object
    LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(i, &clkPropRegimes.super.objMask.super)
    {
         const LW2080_CTRL_CLK_CLK_PROP_REGIME_INFO  *pProgRegimeInfo =
             &clkPropRegimes.propRegimes[i];

         JSObject* propRegimeInfo = JS_NewObject(pContext, &ClockPropRegimeClass, NULL, NULL);
         C_CHECK_RC(pJavaScript->SetProperty(propRegimeInfo, "type", pProgRegimeInfo->super.type));
         C_CHECK_RC(pJavaScript->SetProperty(propRegimeInfo, "regimeID", pProgRegimeInfo->regimeId));
         C_CHECK_RC(pJavaScript->SetProperty(propRegimeInfo, "clockDomainMask",
                                             pProgRegimeInfo->clkDomainMask.super.pData[0]));
         C_CHECK_RC(pJavaScript->SetElement(pClkPropRegimesInfo, i, propRegimeInfo));
    }
    LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END;
    RETURN_RC(rc);
}

//Return value is a JS Object with the clock propogation table header (array).
//Each member of the array contains regimeId and clkDomainMask
//Propagation Regimes defines the mask of clock domains that needs to be programmed together to meet the
//expected functionality where the expectation could be optimal Perf, Power or some functional dependency
JS_STEST_LWSTOM(JsPerf,
                GetClkDomainMaskForPropRegime,
                2,
                "Get the Clock Propagation Regimes Information")
{
    STEST_HEADER(2, 2, "Usage: subdev.Perf.GetClkPropRegimesInfo(RetArray)");
    STEST_ARG(0, UINT32, propRegimeId);
    STEST_ARG(1, JSObject*, pReturnObj);
    STEST_PRIVATE(JsPerf, pJsPerf, "JsPerf");
    Perf40* pPerf40 = dynamic_cast<Perf40*>(pJsPerf->GetPerfObj());
    RC rc;
    if (!pPerf40)
    {
        Printf(Tee::PriError, "GetClkDomainMaskForPropRegime is only supported on pstates 4.0+ chips\n");
        RETURN_RC(RC::UNSUPPORTED_FUNCTION);
    }

    LW2080_CTRL_CLK_CLK_PROP_REGIMES_CONTROL clkPropRegimesControl;
    pPerf40->GetClkPropRegimesControl(&clkPropRegimesControl);

    if (propRegimeId >= LW2080_CTRL_BOARDOBJGRP_E32_MAX_OBJECTS)
    {
        Printf(Tee::PriError, "Propogation Regime Id = %d\n", propRegimeId);
        RETURN_RC(RC::SOFTWARE_ERROR);
    }
    pJavaScript->SetElement(pReturnObj, 0,
    static_cast<UINT32>(clkPropRegimesControl.propRegimes[propRegimeId].clkDomainMask.super.pData[0]));
    RETURN_RC(rc);
}

//JS classes used in GetClkTopPropRelsInfo
JS_CLASS(ClockTopPropRel);
JS_CLASS(TopRelEntryRatio);
JS_CLASS(TopRelEntryTable);
JS_CLASS(TopRelEntryVolt);
JS_CLASS(TopRelEntryVfe);

//Return value is a JS Object with the clock topology propogation relationship table header (array).
JS_STEST_LWSTOM(JsPerf,
                GetClkTopPropRelsInfo,
                1,
                "Get the Clock Toplogy Propagation Relationship Information")
{
    STEST_HEADER(1, 1, "Usage: subdev.Perf.GetClkTopPropRelsInfo(RetArray)");
    STEST_ARG(0, JSObject*, pJsClkTopPropRelsInfo);
    STEST_PRIVATE(JsPerf, pJsPerf, "JsPerf");
    Perf40* pPerf40 = dynamic_cast<Perf40*>(pJsPerf->GetPerfObj());
    RC rc;
    UINT32 i = 0;
    if (!pPerf40)
        RETURN_RC(RC::UNSUPPORTED_FUNCTION);

    const LW2080_CTRL_CLK_CLK_PROP_TOP_RELS_INFO* pClkTopPropRels = pPerf40->GetClkTopPropRelsInfo();


    // Each CLK_ENUM Object
    LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(i, &pClkTopPropRels->super.objMask.super)
    {
        const  LW2080_CTRL_CLK_CLK_PROP_TOP_REL_INFO  *pPropRelInfo = &pClkTopPropRels->propTopRels[i];

        JSObject* pJspropRelInfo = JS_NewObject(pContext, &ClockTopPropRelClass, NULL, NULL);
        C_CHECK_RC(pJavaScript->SetProperty(pJspropRelInfo, "type", pPropRelInfo->super.type));
        // Source Clock Domain Index
        C_CHECK_RC(pJavaScript->SetProperty(pJspropRelInfo, "clkDomainIdxSrc",
                                            pPropRelInfo->clkDomainIdxSrc));
        // Destination Clock Domain Index
        C_CHECK_RC(pJavaScript->SetProperty(pJspropRelInfo, "clkDomainIdxDst",
                                            pPropRelInfo->clkDomainIdxDst));
        // Boolean tracking whether bidirectional relationship enabled
        C_CHECK_RC(pJavaScript->SetProperty(pJspropRelInfo, "bBiDirectional",
                                            pPropRelInfo->bBiDirectional));

        switch (pPropRelInfo->super.type)
        {
            // VFE equation index
            case LW2080_CTRL_CLK_CLK_PROP_TOP_REL_TYPE_1X_VFE:
            {
                JSObject* pJsTopRelEntryVfe = JS_NewObject(pContext, &TopRelEntryVfeClass,
                                                           NULL, NULL);
                C_CHECK_RC(pJavaScript->SetProperty(pJsTopRelEntryVfe, "vfeIdx",
                                                    pPropRelInfo->data.vfe.vfeIdx));
                C_CHECK_RC(pJavaScript->SetProperty(pJspropRelInfo, "vfe", pJsTopRelEntryVfe));

                break;
            }
            // Voltage Rail Index
            case LW2080_CTRL_CLK_CLK_PROP_TOP_REL_TYPE_1X_VOLT:
            {
                JSObject* pJsTopRelEntryVolt = JS_NewObject(pContext, &TopRelEntryVoltClass,
                                                            NULL, NULL);
                C_CHECK_RC(pJavaScript->SetProperty(pJsTopRelEntryVolt, "voltRailIdx",
                                                    pPropRelInfo->data.volt.voltRailIdx));
                C_CHECK_RC(pJavaScript->SetProperty(pJspropRelInfo, "volt", pJsTopRelEntryVolt));

                break;
            }
            // Table Relationship
            case LW2080_CTRL_CLK_CLK_PROP_TOP_REL_TYPE_1X_TABLE:
            {
                JSObject* pJsTopRelEntryTable = JS_NewObject(pContext, &TopRelEntryTableClass,
                                                             NULL, NULL);
                C_CHECK_RC(pJavaScript->SetProperty(pJsTopRelEntryTable, "tableRelIdxFirst",
                                                    pPropRelInfo->data.table.tableRelIdxFirst));
                C_CHECK_RC(pJavaScript->SetProperty(pJsTopRelEntryTable, "tableRelIdxLast",
                                                    pPropRelInfo->data.table.tableRelIdxLast));
                C_CHECK_RC(pJavaScript->SetProperty(pJspropRelInfo, "table", pJsTopRelEntryTable));

                break;
            }
            case LW2080_CTRL_CLK_CLK_PROP_TOP_REL_TYPE_1X_RATIO:
            {
                JSObject* pJsTopRelEntryRatio = JS_NewObject(pContext, &TopRelEntryRatioClass,
                                                             NULL, NULL);
                C_CHECK_RC(pJavaScript->SetProperty(pJsTopRelEntryRatio, "ratio",
                                                    pPropRelInfo->data.ratio.ratio));
                C_CHECK_RC(pJavaScript->SetProperty(pJsTopRelEntryRatio, "ratioIlwerse",
                                                    pPropRelInfo->data.ratio.ratioIlwerse));
                C_CHECK_RC(pJavaScript->SetProperty(pJspropRelInfo, "ratio", pJsTopRelEntryRatio));

                break;
            }
        }
        C_CHECK_RC(pJavaScript->SetElement(pJsClkTopPropRelsInfo, i, pJspropRelInfo));
    }
    LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END;
    RETURN_RC(rc);
}

JS_STEST_LWSTOM(JsPerf,
                GetLwrrClkTopPropRelsInfo,
                2,
                "Get the Clock Toplogy Propagation Relationship Ratio via index")
{
    STEST_HEADER(2, 2, "Usage: subdev.Perf.GetLwrrClkTopPropRelsInfo(index, RetObject)");
    STEST_ARG(0, UINT32, relIdx);
    STEST_ARG(1, JSObject*, pJsClkTopPropRelRatio);
    STEST_PRIVATE(JsPerf, pJsPerf, "JsPerf");
    Perf40* pPerf40 = dynamic_cast<Perf40*>(pJsPerf->GetPerfObj());
    RC rc;
    if (!pPerf40)
        RETURN_RC(RC::UNSUPPORTED_FUNCTION);

    if (relIdx >= LW2080_CTRL_BOARDOBJGRP_E255_MAX_OBJECTS)
    {
        Printf(Tee::PriError, "relIdx = %u is invalid \n", relIdx);
        RETURN_RC(RC::SOFTWARE_ERROR);
    }

    LW2080_CTRL_CLK_CLK_PROP_TOP_RELS_CONTROL clkPropTopsRelsCtrl = {};
    C_CHECK_RC(pPerf40->GetClkPropTopRelsControl(&clkPropTopsRelsCtrl));

    const  LW2080_CTRL_CLK_CLK_PROP_TOP_REL_CONTROL* pClkPropTopsRelCtrl =
                               &clkPropTopsRelsCtrl.propTopRels[relIdx];

    if (pClkPropTopsRelCtrl->super.type != LW2080_CTRL_CLK_CLK_PROP_TOP_REL_TYPE_1X_RATIO)
    {
        Printf(Tee::PriError, "Invalid Clock Propagation Toplogy Relationship type = %u \n",
                               pClkPropTopsRelCtrl->super.type);
        RETURN_RC(RC::SOFTWARE_ERROR);
    }

    C_CHECK_RC(pJavaScript->SetProperty(pJsClkTopPropRelRatio, "ratio",
                   Utility::Colwert16_16ToFloat(pClkPropTopsRelCtrl->data.ratio.ratio)));


    RETURN_RC(rc);
}

JS_STEST_LWSTOM(JsPerf,
                GetPerfSpecTable,
                1,
                "Get the Performance Test Spec Table")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);

    const char usage[] = "Usage: subdev.Perf.GetPerfSpecTable(RtnObj)";

    JavaScriptPtr pJS;
    JSObject * pRetObj;

    if ((NumArguments != 1) ||
        (OK != pJS->FromJsval(pArguments[0], &pRetObj)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        RC rc;
        Perf* pPerf = pJsPerf->GetPerfObj();
        LW2080_CTRL_PERF_GET_PERF_TEST_SPEC_TABLE_INFO_PARAMS SpecTable = {0};
        C_CHECK_RC(pPerf->GetPerfSpecTable(&SpecTable));

        C_CHECK_RC(pJS->PackFields(pRetObj, "IIIIIIII",
                                   "tjTempC", SpecTable.tjTempC,
                                   "maxTachSpeedRPM", SpecTable.maxTachSpeedRPM,
                                   "basePwrLimitmW",  SpecTable.basePwrLimitmW,
                                   "pwrVectIdx", SpecTable.pwrVectIdx,
                                   "preheatTimeS", SpecTable.preheatTimeS,
                                   "baseTestTimeS", SpecTable.baseTestTimeS,
                                   "boostTestTimeS", SpecTable.boostTestTimeS,
                                   "fjTempC", SpecTable.fjTempC));

        RETURN_RC(rc);
    }
    return JS_FALSE;
}

JS_CLASS(RmVfPoint);
JS_STEST_LWSTOM(JsPerf,
                GetVfPointsStatusAtPState,
                3,
                "Get the dynamic list of VF points whose frequencies are in the PState")
{
    RC rc;
    STEST_HEADER(3, 3,
                 "Usage: Perf.GetVfPointsStatusAtPState(pstateNum, dom, [outVfPoints])");
    STEST_PRIVATE(JsPerf, pJsPerf, "JsPerf");
    STEST_ARG(0, UINT32, pstateNum);
    STEST_ARG(1, UINT32, clkDom);
    STEST_ARG(2, JSObject *, pJsObjLwrrentVfPoints);
    Perf *pPerf = pJsPerf->GetPerfObj();
    map<UINT32, LW2080_CTRL_CLK_CLK_VF_POINT_STATUS> lwrrentVfPoints;
    C_CHECK_RC(pPerf->GetVfPointsAtPState(pstateNum,
        static_cast<Gpu::ClkDomain>(clkDom), &lwrrentVfPoints));

    UINT32 ii = 0;
    for (map<UINT32, LW2080_CTRL_CLK_CLK_VF_POINT_STATUS>::const_iterator pointsItr = lwrrentVfPoints.begin();
         pointsItr != lwrrentVfPoints.end(); ++pointsItr)
    {
        JSObject *pJsObjVfPoint = JS_NewObject(pContext, &RmVfPointClass,
                                               nullptr, nullptr);
        C_CHECK_RC(JsPerf::RmVfPointToJsObj(pointsItr->first, pointsItr->second, pJsObjVfPoint));
        jsval tmp;
        C_CHECK_RC(pJavaScript->ToJsval(pJsObjVfPoint, &tmp));
        C_CHECK_RC(pJavaScript->SetElement(pJsObjLwrrentVfPoints, ii++, tmp));
    }
    RETURN_RC(rc);
}

JS_STEST_LWSTOM(JsPerf,
                DumpPerfLimits,
                2,
                "Dump the perf limits")
{
    RC rc;
    STEST_HEADER(2, 2,
        "Usage: subdev.Perf.DumpPerfLimits(\"mods/rm/all\", \"all/enabled\")");
    STEST_PRIVATE(JsPerf, pJsPerf, "JsPerf");
    STEST_ARG(0, string, modsRmStr);
    STEST_ARG(1, string, enableStr);
    modsRmStr = Utility::ToLowerCase(modsRmStr);
    enableStr = Utility::ToLowerCase(enableStr);

    UINT32 limitVerbosityMask = PerfUtil::NO_LIMITS;
    if (modsRmStr == "mods")
        limitVerbosityMask = PerfUtil::MODS_LIMITS;
    else if (modsRmStr == "rm")
        limitVerbosityMask = PerfUtil::RM_LIMITS;
    else if (modsRmStr == "all")
        limitVerbosityMask = PerfUtil::MODS_LIMITS | PerfUtil::RM_LIMITS;
    else
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    if (enableStr == "enabled")
        limitVerbosityMask |= PerfUtil::ONLY_ENABLED_LIMITS;
    else if (enableStr != "all")
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    Perf* pPerf = pJsPerf->GetPerfObj();
    if (!pPerf->IsPState30Supported())
    {
        Printf(Tee::PriError,
            "The API is only supported on pstates 3.0+ chips\n");
        RETURN_RC(RC::UNSUPPORTED_FUNCTION);
    }

    // Fetch the original verbosity mask.
    UINT32 oriLimitVerbosityMask = PerfUtil::NO_LIMITS;
    C_CHECK_RC(pPerf->GetLimitVerbosity(&oriLimitVerbosityMask));

    Utility::CleanupOnReturnArg<Perf, RC, UINT32> limitVerbosity
        (pPerf, &Perf::SetLimitVerbosity, oriLimitVerbosityMask);

    // Override with the mask requested by the user.
    C_CHECK_RC(pPerf->SetLimitVerbosity(limitVerbosityMask));

    // Print the perf limits.
    C_CHECK_RC(pPerf->PrintPerfLimits());

    RETURN_RC(rc);
}

JS_SMETHOD_LWSTOM(JsPerf,
                  VoltFreqLookup,
                  4,
                  "Voltage Frequency Lookup")
{
    RC rc;
    STEST_HEADER(4, 4,
        "Usage: var vfOutput = subdev.Perf.VoltFreqLookup"
        "(clkDom, railIdx, lookupType, vfInput)");
    STEST_PRIVATE(JsPerf, pJsPerf, "JsPerf");
    STEST_ARG(0, UINT32, clkDom);
    STEST_ARG(1, UINT32, railIdx);
    STEST_ARG(2, UINT32, lookupType);
    STEST_ARG(3, UINT32, vfInput);

    Perf* pPerf = pJsPerf->GetPerfObj();
    Gpu::SplitVoltageDomain splitVoltDom = pPerf->RailIdxToGpuSplitVoltageDomain(railIdx);
    UINT32 vfOutput;
    rc = pPerf->VoltFreqLookup(static_cast<Gpu::ClkDomain>(clkDom),
        splitVoltDom, static_cast<Perf::VFLookupType>(lookupType),
        vfInput, &vfOutput);
    if (OK != rc || OK != pJavaScript->ToJsval(vfOutput, pReturlwalue))
    {
        JS_ReportError(pContext, "Error oclwrred in Perf.VoltFreqLookup");
        return JS_FALSE;
    }
    return JS_TRUE;
}

JS_STEST_LWSTOM(JsPerf,
                GetStaticFreqsKHz,
                2,
                "Get the static list of frequencies for a clock domain")
{
    STEST_HEADER(2, 2,
                 "Usage: Perf.GetStaticFreqsKHz(dom, [outFreqPoints])");
    STEST_PRIVATE(JsPerf, pJsPerf, "JsPerf");
    STEST_ARG(0, UINT32, clkDom);
    STEST_ARG(1, JSObject *, pReturnArray);
    RC rc;
    Perf* pPerf = pJsPerf->GetPerfObj();
    C_CHECK_RC(pPerf->GetVfPointsToJs(static_cast<Gpu::ClkDomain>(clkDom),
                                      pContext,
                                      pReturnArray));
    RETURN_RC(rc);
}

JS_STEST_LWSTOM(JsPerf,
                GetStaticFreqsAtPStateKHz,
                3,
                "Get the static list of frequencies for a clock domain "
                "bounded by the PState range")
{
    MASSERT((pContext !=0) || (pArguments !=0));

    const char usage[] = "Usage: subdev.Perf.GetStaticFreqsAtPStateKHz"
                         "(pstateNum, clkDomain, outFreqPoints)";
    JavaScriptPtr pJs;
    UINT32 PStateNum;
    UINT32 Domain;
    JSObject * pReturnArray = 0;
    if (3 != pJs->UnpackArgs(pArguments, NumArguments, "IIo",
                             &PStateNum, &Domain, &pReturnArray))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    RC rc;
    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) == 0)
    {
        return JS_FALSE;
    }
    Perf* pPerf = pJsPerf->GetPerfObj();
    C_CHECK_RC(pPerf->GetStaticFreqsToJs(PStateNum,
                                         static_cast<Gpu::ClkDomain>(Domain),
                                         pReturnArray));
    RETURN_RC(rc);
}

JS_STEST_LWSTOM(JsPerf,
                GetStaticVfPointsAtPState,
                3,
                "Get the VF points retrieved from RM during init "
                "bounded by PState freq range")
{
    MASSERT((pContext !=0) || (pArguments !=0));

    const char usage[] = "Usage: subdev.Perf.GetStaticVfPointsAtPState(pstateNum, clkDomain, outFreqPoints)";
    JavaScriptPtr pJs;
    UINT32 PStateNum;
    UINT32 Domain;
    JSObject * pReturnArray = 0;
    if (3 != pJs->UnpackArgs(pArguments, NumArguments, "IIo",
                             &PStateNum, &Domain, &pReturnArray))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    RC rc;
    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) == 0)
    {
        return JS_FALSE;
    }
    Perf* pPerf = pJsPerf->GetPerfObj();
    C_CHECK_RC(pPerf->GetVfPointsToJs(PStateNum,
                                      static_cast<Gpu::ClkDomain>(Domain),
                                      pContext, pReturnArray));

    RETURN_RC(rc);
}

JS_STEST_LWSTOM(JsPerf,
                GetVdt,
                1,
                "Get Vdt")
{
    MASSERT((pContext !=0) || (pArguments !=0));

    const char usage[] = "Usage: \n"
                     "var subdev = new GpuSubdevice(0,0);\n"
                     "var obj = new Object;\n"
                     "rc = subdev.Perf.GetVdt(obj)\n"
                     "--- Properties in each object:----\n"
                     "hwSpeedo: Cached HW SPEEDO fuse\n"
                     "hwSpeedoVersion: Cached HW SPEEDO version fuse\n"
                     "speedoVersion: SPEEDO version to be used for this table\n"
                     "tempPollingPeriodms: Polling period for re-evaluating any temperature-based requested\n"
                     "nominalP0VdtEntry :Original/nominal P0 voltage before voltage tuning\n"
                     "reliabilityLimitEntry : Voltage Reliability Limit entry\n"
                     "overVoltageLimitEntry: User Over-Voltage Limit entry\n"
                     "voltageTuningEntry: Voltage Tuning Entry\n"
                     "VdtEntries: Table of VDT objects\n"
                     "   lwrrTargetVoltageuV\n"
                     "   localUnboundVoltageuV\n"
                     "   type\n"
                     "   nextEntry\n"
                     "   voltageMinuV\n"
                     "   voltageMaxuV\n"
                     "   coefficient0\n"
                     "   coefficient1\n"
                     "   coefficient2\n"
                     "   coefficient3 - for CVB20\n"
                     "   coefficient4 - for CVB20\n"
                     "   coefficient5 - for CVB20\n";

    JavaScriptPtr pJs;
    JSObject * pReturnObj = 0;
    if((NumArguments != 1) ||
       (OK != pJs->FromJsval(pArguments[0], &pReturnObj)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    RC rc;
    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) == 0)
        return JS_FALSE;

    Perf* pPerf = pJsPerf->GetPerfObj();
    C_CHECK_RC(pPerf->GetVdtToJs(pContext, pReturnObj));
    RETURN_RC(rc);
}

JS_SMETHOD_LWSTOM(JsPerf,
                  ClkDomainType,
                  1,
                  "Returns one of PerfConst.FIXED, .PSTATE, .DECOUPLED, or .RATIO")
{
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    JavaScriptPtr pJavaScript;
    UINT32 modsClkDomain;
    const char usage[] = "Usage: subdev.Perf.ClkDomainType(Gpu.ClkXXX)\n";

    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &modsClkDomain)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        const UINT32 type = pJsPerf->GetPerfObj()->ClkDomainType(
            static_cast<Gpu::ClkDomain>(modsClkDomain));
        if (pJavaScript->ToJsval(type, pReturlwalue) == OK)
        {
            return JS_TRUE;
        }
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsPerf,
                GetClkDomainInfos,
                1,
                "List the information on the clock domains - provide info on type, ratio, slave domain, etc")
{
    MASSERT((pContext !=0) || (pArguments !=0));

    const char usage[] = "Usage: \n"
                         "var subdev = new GpuSubdevice(0,0);\n"
                         "var ClkDoms = new Array;\n"
                         "rc = subdev.Perf.GetClkDomainInfos(ClkDoms)\n"
                         "--- Properties in each ClkDoms:----\n"
                         "Domain : MODS clock domains\n"
                         "RmDomain : clock domain in RM bits\n"
                         "Type: PerfConst.FIXED, PerfConst.PSTATE, PerfConst.DECOUPLED, PerfConst.RATIO\n"
                         "RatioDomain : if this is a ratio'ed domain, RatioDomain is the 'master' domain\n"
                         "RmRatioDomain : 'master' domain in RM's domain format\n"
                         "Ratio: ratio (percentage) of mater domain\n";

    JavaScriptPtr pJs;
    JSObject * pReturnArray = 0;
    if((NumArguments != 1) ||
       (OK != pJs->FromJsval(pArguments[0], &pReturnArray)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    RC rc;
    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) == 0)
    {
        return JS_FALSE;
    }
    Perf* pPerf = pJsPerf->GetPerfObj();
    C_CHECK_RC(pPerf->GetClkDomainInfoToJs(pContext, pReturnArray));
    RETURN_RC(rc);
}

JS_STEST_LWSTOM(JsPerf,
                GetVoltDomainInfo,
                3,
                "Gather logical and VDT entry value of the a VoltDomain at a PState")
{
    MASSERT((pContext !=0) || (pArguments !=0));

    const char usage[] = "Usage: \n"
                         "subdev.Perf.GetVoltDomainInfo(PStateNum, Domain, OutObj)\n"
                         "var subdev = new GpuSubdevice(0,0);\n"
                         "var VoltObj = new Object;\n"
                         "rc = subdev.Perf.GetVoltDomainInfo(8, Gpu.CoreV, VoltObj)\n"
                         "----Properties in each VoltObj:----\n"
                         "RmDomain : volt domain in RM bits\n"
                         "LogicalMv : logical voltage in mV\n"
                         "VdtIdx: VDT index\n";

    JavaScriptPtr pJs;
    UINT32 PStateNum;
    UINT32 VoltDom;
    JSObject * pRetObj = 0;
    if (3 != pJs->UnpackArgs(pArguments, NumArguments, "IIo",
                             &PStateNum, &VoltDom, &pRetObj))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    RC rc;
    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) == 0)
    {
        return JS_FALSE;
    }
    Perf* pPerf = pJsPerf->GetPerfObj();
    C_CHECK_RC(pPerf->GetVoltDomainInfoToJs(PStateNum,
                                            (Gpu::VoltageDomain)VoltDom,
                                            pContext,
                                            pRetObj));
    RETURN_RC(rc);
}

C_(JsPerf_GetPerfPoints);
C_(JsPerf_GetInflectionPoints);

// Old (bad) name for this function, for backwards compatibility.
static STest JsPerf_GetInflectionPoints
(
    JsPerf_Object,
    "GetInflectionPoints",
    C_JsPerf_GetInflectionPoints,
    1,
    "DEPRECATED -- use GetPerfPoints instead."
);
C_(JsPerf_GetInflectionPoints)
{
    Printf(Tee::PriWarn,
           "GetInflectionPoints is deprecated!\n"
           "Please use GetPerfPoints instead.\n");

    return C_JsPerf_GetPerfPoints(pContext, pObject,
                                  NumArguments, pArguments,
                                  pReturlwalue);
}

// Because JS_NewObject with a NULL class* is unsafe in JS 1.4.
JS_CLASS(PerfPoint);

// New (correct) name.
static STest JsPerf_GetPerfPoints
(
    JsPerf_Object,
    "GetPerfPoints",
    C_JsPerf_GetPerfPoints,
    1,
    "Get an array holding the standard PerfPoints."
);
C_(JsPerf_GetPerfPoints)
{
    MASSERT((pContext !=0) || (pArguments !=0));

    const char usage[] = "Usage: subdev.Perf.GetPerfPoints(Array)";
    JavaScriptPtr pJs;
    JSObject * pReturnArray = 0;
    if((NumArguments != 1) ||
       (OK != pJs->FromJsval(pArguments[0], &pReturnArray)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }
    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) == 0)
    {
        return JS_FALSE;
    }

    RC rc;
    Perf* pPerf = pJsPerf->GetPerfObj();
    Perf::PerfPoints perfPoints;
    C_CHECK_RC(pPerf->GetPerfPoints(&perfPoints));

    UINT32 Idx = 0;
    for (Perf::PerfPoints::const_iterator ppiter = perfPoints.begin();
         ppiter != perfPoints.end(); ++ppiter)
    {
        JSObject *pObj = JS_NewObject(pContext, &PerfPointClass, NULL, NULL);
        C_CHECK_RC(pPerf->PerfPointToJsObj(*ppiter, pObj, pContext));

        jsval PerfJsval;
        C_CHECK_RC(pJs->ToJsval(pObj, &PerfJsval));
        C_CHECK_RC(pJs->SetElement(pReturnArray, Idx, PerfJsval));
        Idx++;
    }
    RETURN_RC(rc);
}

JS_STEST_LWSTOM(JsPerf, GetMatchingStdPerfPoint, 2,
                "Retrieve the standard PerfPoint from m_PerfPoints which "
                "corresponds to the supplied PerfPoint")
{
    RC rc;
    STEST_HEADER(2, 2, "Usage: subdev.Perf.GetMatchingStdPerfPoint"
                       "(inPerfPoint, outStdPerfPoint)");
    STEST_PRIVATE(JsPerf, pJsPerf, "JsPerf");
    STEST_ARG(0, JSObject *, pInPerfPoint);
    STEST_ARG(1, JSObject *, pOutStdPerfPoint);
    Perf *pPerf = pJsPerf->GetPerfObj();

    Perf::PerfPoint perfPointFromJs;
    C_CHECK_RC(pPerf->JsObjToPerfPoint(pInPerfPoint, &perfPointFromJs));
    const Perf::PerfPoint matchingStdPerfPoint =
        pPerf->GetMatchingStdPerfPoint(perfPointFromJs);
    if (matchingStdPerfPoint.SpecialPt != Perf::NUM_GpcPerfMode)
    {
        C_CHECK_RC(pPerf->PerfPointToJsObj(matchingStdPerfPoint,
                                           pOutStdPerfPoint,
                                           pContext));
    }
    else
    {
        // pOutStdPerfPoint is untouched in this case
        JS_ReportError(pContext,
                       Utility::StrPrintf("Failed GetMatchingStdPerfPoint for %s\n",
                            perfPointFromJs.name(pPerf->IsPState30Supported()).c_str())
                                .c_str());
        return JS_FALSE;
    }
    RETURN_RC(rc);
}

JS_STEST_LWSTOM(JsPerf,
                GetVfPointPerfPoints,
                2,
                "Get an array holding PerfPoints for each VF point of each pstate."
)
{
    MASSERT((pContext !=0) || (pArguments !=0));

    const char usage[] = "Usage: subdev.Perf.GetVfPointPerfPoints(Array, clkDomain, clkDomain, ...)";
    JavaScriptPtr pJs;
    JSObject * pReturnArray = 0;
    if((NumArguments < 1) ||
       (OK != pJs->FromJsval(pArguments[0], &pReturnArray)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }
    vector<Gpu::ClkDomain> domains;
    if (NumArguments > 1)
    {
        for (UINT32 ii = 1; ii < NumArguments; ii++)
        {
            UINT32 domain = 0;
            if (OK != pJs->FromJsval(pArguments[ii], &domain))
            {
                JS_ReportError(pContext, usage);
                return JS_FALSE;
            }
            domains.push_back(static_cast<Gpu::ClkDomain>(domain));
        }
    }
    else
    {
        domains.push_back(Gpu::ClkGpc);
    }
    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) == 0)
    {
        return JS_FALSE;
    }

    RC rc;
    Perf* pPerf = pJsPerf->GetPerfObj();
    vector<Perf::PerfPoint> perfPoints;
    C_CHECK_RC(pPerf->GetVfPointPerfPoints(&perfPoints, domains));

    UINT32 Idx = 0;
    for (vector<Perf::PerfPoint>::const_iterator ppiter = perfPoints.begin();
         ppiter != perfPoints.end(); ++ppiter)
    {
        JSObject *pObj = JS_NewObject(pContext, &PerfPointClass, NULL, NULL);
        C_CHECK_RC(pPerf->PerfPointToJsObj(*ppiter, pObj, pContext));

        jsval PerfJsval;
        C_CHECK_RC(pJs->ToJsval(pObj, &PerfJsval));
        C_CHECK_RC(pJs->SetElement(pReturnArray, Idx, PerfJsval));
        Idx++;
    }
    RETURN_RC(rc);
}

RC JsPerf::RmVfPointToJsObj
(
    UINT32 rmVfIdx,
    const LW2080_CTRL_CLK_CLK_VF_POINT_STATUS &vfPoint,
    JSObject *pJsVfPoint
)
{
    RC rc;
    JavaScriptPtr pJs;
    CHECK_RC(pJs->SetProperty(pJsVfPoint, "type", vfPoint.super.type));
    CHECK_RC(pJs->SetProperty(pJsVfPoint,
                              "voltageuV",
                              static_cast<UINT32>(vfPoint.voltageuV)));
    CHECK_RC(pJs->SetProperty(pJsVfPoint, "freqMHz", vfPoint.freqMHz));
    CHECK_RC(pJs->SetProperty(pJsVfPoint, "rmVfIdx", rmVfIdx));
    return rc;
}

JS_STEST_LWSTOM(JsPerf, GetVfPointsStatusForDomain, 2,
                "Get the current relationship between voltage and frequency "
                "for the given clock domain. This can change over time due to "
                "temperature or across boards due to speedo.")
{
    RC rc;
    STEST_HEADER(2, 2,
                 "Usage: Perf.GetVfPointsStatusForDomain(dom, [outVfPoints])");
    STEST_PRIVATE(JsPerf, pJsPerf, "JsPerf");
    STEST_ARG(0, UINT32, clkDom);
    STEST_ARG(1, JSObject *, pJsObjLwrrentVfPoints);
    Perf *pPerf = pJsPerf->GetPerfObj();
    map<UINT32, LW2080_CTRL_CLK_CLK_VF_POINT_STATUS> lwrrentVfPoints;
    C_CHECK_RC(pPerf->GetVfPointsStatusForDomain(
        static_cast<Gpu::ClkDomain>(clkDom), &lwrrentVfPoints));

    UINT32 ii = 0;
    for (map<UINT32, LW2080_CTRL_CLK_CLK_VF_POINT_STATUS>::const_iterator pointsItr = lwrrentVfPoints.begin();
         pointsItr != lwrrentVfPoints.end(); ++pointsItr)
    {
        JSObject *pJsObjVfPoint = JS_NewObject(pContext, &RmVfPointClass,
                                               nullptr, nullptr);
        C_CHECK_RC(JsPerf::RmVfPointToJsObj(pointsItr->first, pointsItr->second, pJsObjVfPoint));
        jsval tmp;
        C_CHECK_RC(pJavaScript->ToJsval(pJsObjVfPoint, &tmp));
        C_CHECK_RC(pJavaScript->SetElement(pJsObjLwrrentVfPoints, ii++, tmp));
    }
    RETURN_RC(rc);
}

JS_STEST_LWSTOM(JsPerf, SetAllowedPerfSwitchTimeNs, 2,
                "Set the amount of time SetPerfPoint is allowed to "
                "take before it returns a timeout error")
{
    STEST_HEADER(2, 2, "Usage: Perf.SetAllowedPerfSwitchTimeNs(type, allowedPerfSwitchTimeNs)");
    STEST_PRIVATE(JsPerf, pJsPerf, "JsPerf");
    STEST_ARG(0, string, type);
    STEST_ARG(1, UINT32, allowedPerfSwitchTimeNs);
    RC rc;
    Perf *pPerf = pJsPerf->GetPerfObj();
    type = Utility::ToLowerCase(type);
    Perf::VfSwitchVal vfswitchVal = Perf::VfSwitchVal::Mods;
    if (type == "mods")
        vfswitchVal = Perf::VfSwitchVal::Mods;
    else if (type == "rmlimits")
        vfswitchVal = Perf::VfSwitchVal::RmLimits;
    else if (type == "pmu")
        vfswitchVal = Perf::VfSwitchVal::Pmu;
    pPerf->SetAllowedPerfSwitchTimeNs(vfswitchVal, allowedPerfSwitchTimeNs);
    RETURN_RC(rc);
}

JS_STEST_LWSTOM(JsPerf,
                GetPerfPoint,
                1,
                "Get a PerfPoint matching current gpu state.")
{
    MASSERT((pContext !=0) || (pArguments !=0));

    const char usage[] = "Usage: subdev.Perf.GetPerfPoint(NewObject)";
    JavaScriptPtr pJs;
    JSObject * pReturnObj = 0;
    if((NumArguments != 1) ||
       (OK != pJs->FromJsval(pArguments[0], &pReturnObj)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }
    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        RC rc;
        Perf* pPerf = pJsPerf->GetPerfObj();
        Perf::PerfPoint perfPoint;
        C_CHECK_RC(pPerf->GetLwrrentPerfPoint(&perfPoint));
        C_CHECK_RC(pPerf->PerfPointToJsObj(perfPoint, pReturnObj, pContext));
        RETURN_RC(rc);
    }
    return JS_FALSE;
}

// This is VERY similar to SetPStateTable.
// Differences
// 1) Really sets to a PState when the function finishes. SetPStateTable changes
//    only the table, settings are only applied when we are at the PState.
// 2) option to Set GpcPerf parameter -> MODS to figure out what is the
//    corresponding perf parameters associated with the GPC clock with
//    RM's help
JS_STEST_LWSTOM(JsPerf,
                SetPerfPoint,
                1,
                "Set a Perf Point and changes PState")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);

    JavaScriptPtr pJs;
    JSObject * pJsPerfPt = 0;
    if ((NumArguments != 1) ||
        (OK != pJs->FromJsval(pArguments[0], &pJsPerfPt)))
    {
        JS_ReportError(pContext, "Usage: subdev.Perf.SetPerfPoint(PerfPtObj)");
        return JS_FALSE;
    }

    RC rc;
    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        Perf* pPerf = pJsPerf->GetPerfObj();
        Perf::PerfPoint NewPerfPt;
        C_CHECK_RC(pPerf->JsObjToPerfPoint(pJsPerfPt, &NewPerfPt));

        // If the user calls this function with an empty clocks array and no
        // gpcclk specified but they specify a SpecialPt such as "min"
        // then we should use SetInflectionPoint() instead.
        if (NewPerfPt.Clks.size() == 0 &&
            NewPerfPt.IntersectSettings.size() == 0 &&
            NewPerfPt.SpecialPt != Perf::GpcPerf_EXPLICIT)
        {
            C_CHECK_RC(pPerf->SetInflectionPoint(NewPerfPt.PStateNum, NewPerfPt.SpecialPt));
        }
        else
        {
            C_CHECK_RC(pPerf->SetPerfPoint(NewPerfPt));
        }
        Mle::Print(Mle::Entry::test_info).pstate(pPerf->GetInflectionPtStr());
        RETURN_RC(rc);
    }
    return JS_FALSE;
}

RC JsPerf::JsObjToClkSetting
(
    JSObject *pJsClkSetting,
    Perf::ClkSetting *clkSetting
)
{
    RC rc;
    JavaScriptPtr pJs;

    CHECK_RC(pJs->GetProperty(pJsClkSetting, "Domain", (UINT32*)&clkSetting->Domain));
    CHECK_RC(pJs->GetProperty(pJsClkSetting, "FreqHz", &clkSetting->FreqHz));

    // Flags are optional
    clkSetting->Flags = 0;
    pJs->GetProperty(pJsClkSetting, "Flags", &clkSetting->Flags);
    // Regime is also optional, default will cause no change in regime
    clkSetting->Regime = Perf::RegimeSetting_NUM;
    bool UseFixedFrequencyRegime;
    if (OK == pJs->GetProperty(pJsClkSetting,
                               "UseFixedFrequencyRegime",
                               &UseFixedFrequencyRegime))
    {
        if (UseFixedFrequencyRegime)
        {
            clkSetting->Regime = Perf::FIXED_FREQUENCY_REGIME;
        }
        else
        {
            // The current clock and voltage setting determines whether
            // a domain is in the voltage or frequency regime. Thus, there
            // isn't a single knob to force a clock into VR or FR. You can
            // only force a clock into the FFR or un-force it out of FFR.
            clkSetting->Regime = Perf::DEFAULT_REGIME;
        }
    }

    return rc;
}

RC JsPerf::JsObjToSplitVoltageSetting
(
    JSObject *pJsVoltSetting,
    Perf::SplitVoltageSetting *voltSetting
)
{
    RC rc;
    JavaScriptPtr pJs;

    CHECK_RC(pJs->GetProperty(pJsVoltSetting, "Domain",
             (UINT32*)&voltSetting->domain));
    CHECK_RC(pJs->GetProperty(pJsVoltSetting, "VoltMv", &voltSetting->voltMv));

    return rc;
}

RC JsPerf::JsObjToInjectedPerfPoint
(
    JSObject *pJsPerfPt,
    Perf::InjectedPerfPoint *pBdPerfPt
)
{
    MASSERT(pJsPerfPt && pBdPerfPt);
    RC rc;
    JavaScriptPtr pJs;

    jsval clkJsval;
    if (OK == pJs->GetPropertyJsval(pJsPerfPt, "Clks", &clkJsval))
    {
        JsArray clkJsArray;
        CHECK_RC(pJs->FromJsval(clkJsval, &clkJsArray));
        for (UINT32 i = 0; i < clkJsArray.size(); i++)
        {
            Perf::ClkSetting newClk;
            JSObject *pClkObj = nullptr;
            CHECK_RC(pJs->FromJsval(clkJsArray[i], &pClkObj));
            CHECK_RC(JsObjToClkSetting(pClkObj, &newClk));

            pBdPerfPt->clks.push_back(newClk);
        }
    }

    jsval voltJsval;
    if (OK == pJs->GetPropertyJsval(pJsPerfPt, "Voltages", &voltJsval))
    {
        JsArray voltJsArray;
        CHECK_RC(pJs->FromJsval(voltJsval, &voltJsArray));
        for (UINT32 i = 0; i < voltJsArray.size(); i++)
        {
            Perf::SplitVoltageSetting newVolt;
            JSObject *pVoltObj = nullptr;
            CHECK_RC(pJs->FromJsval(voltJsArray[i], &pVoltObj));
            CHECK_RC(JsObjToSplitVoltageSetting(pVoltObj, &newVolt));
            pBdPerfPt->voltages.push_back(newVolt);
        }
    }

    return rc;
}

JS_STEST_LWSTOM(JsPerf,
                InjectPerfPoint,
                1,
                "Set specific voltage and frequency using the inject API")
{
    RC rc;
    STEST_HEADER(1, 1, "Usage: Perf.InjectPerfPoint(perfPoint)");

    STEST_PRIVATE(JsPerf, pJsPerf, "JsPerf");
    STEST_ARG(0, JSObject*, jsObjInjectedPerfPoint);
    Perf::InjectedPerfPoint injectedPerfPoint;
    C_CHECK_RC(JsPerf::JsObjToInjectedPerfPoint(
        jsObjInjectedPerfPoint, &injectedPerfPoint));

    Perf *pPerf = pJsPerf->GetPerfObj();
    RETURN_RC(pPerf->InjectPerfPoint(injectedPerfPoint));
}

RC JsPerf::SetClkFFREnableState(UINT32 clkDomain, bool enable)
{
    Perf::RegimeSetting regime = enable ?
        Perf::FIXED_FREQUENCY_REGIME : Perf::DEFAULT_REGIME;
    return m_pPerf->SetRegime(
        static_cast<Gpu::ClkDomain>(clkDomain), regime);
}

JS_STEST_BIND_TWO_ARGS(
    JsPerf, SetClkFFREnableState,
    clkDomain, UINT32,
    enableFFR, bool,
    "Force or clear FFR (fixed frequency regime) for the specified clock domain");

RC JsPerf::SetRegime(UINT32 clkDomain, UINT32 regime)
{
    return m_pPerf->SetRegime(static_cast<Gpu::ClkDomain>(clkDomain),
                              static_cast<Perf::RegimeSetting>(regime));
}
JS_STEST_BIND_TWO_ARGS(
    JsPerf, SetRegime,
    clkDomain, UINT32,
    regime, UINT32,
    "Force a regime for the specified clock domain");

JS_STEST_LWSTOM(JsPerf, GetRegime, 2,
                "Get the regime for an NAFLL clock domain"
                " (only looks at first NAFLL)")
{
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;
    UINT32 domainInt;
    JSObject* returnRegime;

    if (2 != pJavaScript->UnpackArgs(pArguments, NumArguments, "Io",
                                     &domainInt, &returnRegime))
    {
        JS_ReportError(pContext, "Usage: subdev.Perf.GetRegime(Domain, [rtn])\n");
        return JS_FALSE;
    }
    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        RC rc;
        Perf::RegimeSetting regime;
        C_CHECK_RC(pJsPerf->GetPerfObj()->GetRegime(
                       static_cast<Gpu::ClkDomain>(domainInt), &regime));
        RETURN_RC(pJavaScript->SetElement(returnRegime, 0,
                                          static_cast<UINT32>(regime)));
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsPerf,
                SetPStateTable,
                3,
                "Override PState data (clocks and voltages)")
{
    MASSERT(pContext && pArguments);
    const char usage[] =
        "Usage: subdev.Perf.SetPStateTable(pstate, "
        "[[ckdom, khz, flags],...], [[vdom, mv, flags],...])";

    JavaScriptPtr pJS;
    UINT32        pstate;
    JsArray       jsaClock;
    JsArray       jsaVolt;

    const size_t n = pJS->UnpackArgs(pArguments, NumArguments,
                                     "Iaa", &pstate, &jsaClock, &jsaVolt);
    if (n < 2 || NumArguments > 3 ||
        (jsaClock.size() == 0 && jsaVolt.size() == 0))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    vector<LW2080_CTRL_PERF_CLK_DOM_INFO> vClock;
    for (size_t i = 0; i < jsaClock.size(); i++)
    {
        UINT32 domain = 0;
        UINT32 hz = 0;
        UINT32 flags = 0;

        const size_t n = pJS->UnpackJsval(jsaClock[i],
                                          "III", &domain, &hz, &flags);
        if (n < 2) // flags is optional
        {
            JS_ReportError(
                pContext,
                "Each clock item must be [domain, khz] or [domain, khz, flags]");
            return JS_FALSE;
        }

        LW2080_CTRL_PERF_CLK_DOM_INFO ci = {0};
        ci.domain = PerfUtil::GpuClkDomainToCtrl2080Bit(Gpu::ClkDomain(domain));
        ci.flags  = flags;
        ci.freq   = hz;
        vClock.push_back(ci);
    }

    vector<LW2080_CTRL_PERF_VOLT_DOM_INFO> vVolt;
    for (size_t i = 0; i < jsaVolt.size(); i++)
    {
        UINT32 domain = 0;
        UINT32 mv = 0;
        UINT32 flags = 0;

        const size_t n = pJS->UnpackJsval(jsaVolt[i],
                                          "III", &domain, &mv, &flags);
        if (n < 2) // flags is optional
        {
            JS_ReportError(
                pContext,
                "Each volt item must be [domain, mv] or [domain, mv, flags]");
            return JS_FALSE;
        }

        LW2080_CTRL_PERF_VOLT_DOM_INFO vi = {0};
        vi.domain = PerfUtil::GpuVoltageDomainToCtrl2080Bit(Gpu::VoltageDomain(domain));
        vi.flags  = flags;
        vi.type   = LW2080_CTRL_PERF_VOLT_DOM_INFO_TYPE_LOGICAL;
        vi.data.logical.logicalVoltageuV = mv * 1000;
        vVolt.push_back(vi);
    }

    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        Perf* pPerf = pJsPerf->GetPerfObj();
        RETURN_RC(pPerf->SetPStateTable(pstate, vClock, vVolt));
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsPerf, StartClockChangeCounter, 0,
                      "Start watching for clock changes")
{
    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        Perf* pPerf = pJsPerf->GetPerfObj();
        RETURN_RC(pPerf->GetClockChangeCounter().InitializeCounter());
    }
    return FALSE;
}

JS_STEST_LWSTOM(JsPerf, StopClockChangeCounter, 0,
                      "Stop watching for clock changes")
{
    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        Perf* pPerf = pJsPerf->GetPerfObj();
        RETURN_RC(pPerf->GetClockChangeCounter().ShutDown(true));
    }
    return FALSE;
}

JS_STEST_LWSTOM(JsPerf, ClockChangeCounterVerbose, 0,
                "Display clock changes at Pri::Normal")
{
    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        Perf* pPerf = pJsPerf->GetPerfObj();
        pPerf->GetClockChangeCounter().SetVerbose(true);
        return TRUE;
    }
    return FALSE;
}

JS_STEST_LWSTOM(JsPerf, StartPStateChangeCounter, 0,
                      "Enable failure if RM unexpectedly lowers pstate")
{
    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        Perf* pPerf = pJsPerf->GetPerfObj();
        RETURN_RC(pPerf->GetPStateChangeCounter().InitializeCounter());
    }
    return FALSE;
}

JS_STEST_LWSTOM(JsPerf, StopPStateChangeCounter, 0,
                      "Stop watching for unexpected pstate changes")
{
    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        Perf* pPerf = pJsPerf->GetPerfObj();
        RETURN_RC(pPerf->GetPStateChangeCounter().ShutDown(true));
    }
    return FALSE;
}

P_(JsPerf_Get_PStateChangeCounterIsInitialized);
static SProperty PStateChangeCounterIsInitialized
(
    JsPerf_Object,
    "PStateChangeCounterIsInitialized",
    0,
    0,
    JsPerf_Get_PStateChangeCounterIsInitialized,
    0,
    JSPROP_READONLY,
    "true if we are lwrrently watching for unexpected pstate changes"
);
P_(JsPerf_Get_PStateChangeCounterIsInitialized)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        JavaScriptPtr pJavaScript;
        Perf* pPerf = pJsPerf->GetPerfObj();
        if (pJavaScript->ToJsval(
                pPerf->GetPStateChangeCounter().IsInitialized(),
                pValue) == 0)
        {
            return JS_TRUE;
        }
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsPerf, PrintRmPerfLimits, 1,
                      "Print all RM PERF_LIMITS lwrrently constraining pstate/clocks.")
{
    JavaScriptPtr pJs;
    const char * useage = "Useage: subdev.Perf.PrintRmPerfLimits(Out.PriNormal)";
    UINT32 pri;
    if (NumArguments == 0)
    {
        pri = Tee::PriNormal;
    }
    else if (NumArguments == 1)
    {
        if (OK != pJs->FromJsval(pArguments[0], &pri))
        {
            JS_ReportError(pContext, useage);
            return JS_FALSE;
        }
    }
    else if (NumArguments > 1)
    {
        JS_ReportError(pContext, useage);
        return JS_FALSE;
    }
    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        Perf* pPerf = pJsPerf->GetPerfObj();
        RETURN_RC(pPerf->PrintRmPerfLimits(static_cast<Tee::Priority>(pri)));
    }
    return FALSE;
}

JS_STEST_LWSTOM(JsPerf, GetThermalCapRange, 2,
                "Get min,rated,max values of a thermal-cap policy")
{
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    JavaScriptPtr pJavaScript;
    UINT32 policyIdx = 0;
    JSObject * pReturnObj = 0;

    if (2 != pJavaScript->UnpackArgs(pArguments, NumArguments, "Io",
                                     &policyIdx, &pReturnObj))
    {
        JS_ReportError(pContext,
                       "Usage: subdev.Perf.GetThermalCapRange(policyIdx, {rtn})\n");
        return JS_FALSE;
    }
    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        RC rc;
        float min, rated, max;
        Thermal* pThermal = pJsPerf->GetPerfObj()->GetGpuSubdevice()->GetThermal();
        C_CHECK_RC(pThermal->GetCapRange((Thermal::ThermalCapIdx)policyIdx,
                                                &min, &rated, &max));
        C_CHECK_RC(pJavaScript->PackFields(pReturnObj, "fff",
                                           "Min", min,
                                           "Rated", rated,
                                           "Max", max));
        RETURN_RC(OK);
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsPerf, GetThermalCap, 2, "Get temperature limit of a policy")
{
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    JavaScriptPtr pJavaScript;
    UINT32 policyIdx = 0;
    JSObject * pReturnCap = 0;

    if (2 != pJavaScript->UnpackArgs(pArguments, NumArguments, "Io",
                                     &policyIdx, &pReturnCap))
    {
        JS_ReportError(pContext, "Usage: subdev.Perf.GetThermalCap(policyIdx, [rtn])\n");
        return JS_FALSE;
    }
    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        RC rc;
        float CapDegC;
        Thermal* pThermal = pJsPerf->GetPerfObj()->GetGpuSubdevice()->GetThermal();
        C_CHECK_RC(pThermal->GetCap((Thermal::ThermalCapIdx)policyIdx, &CapDegC));
        RETURN_RC(pJavaScript->SetElement(pReturnCap, 0, CapDegC));
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsPerf, SetThermalCap, 1, "Set temperature limit of a policy")
{
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    JavaScriptPtr pJavaScript;
    UINT32 policyIdx = 0;
    double limitDegC = 0;

    if (2 != pJavaScript->UnpackArgs(pArguments, NumArguments, "Id",
                                     &policyIdx, &limitDegC))
    {
        JS_ReportError(pContext, "Usage: subdev.Perf.SetThermalCap(policyIdx, LimitDegC)\n");
        return JS_FALSE;
    }
    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        Thermal* pThermal = pJsPerf->GetPerfObj()->GetGpuSubdevice()->GetThermal();
        RETURN_RC(pThermal->SetCap((Thermal::ThermalCapIdx)policyIdx,
                                          static_cast<float>(limitDegC)));
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsPerf,
                PrintThermalCapPolicyTable,
                1,
                "Show thermal-capping policy tables.")
{
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    JavaScriptPtr pJavaScript;
    UINT32 pri = Tee::PriNormal;

    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &pri)))
    {
        JS_ReportError(pContext, "syntax: Perf.PrintThermalCapPolicyTable(Out.PriNormal)");
        return JS_FALSE;
    }

    JsPerf *pJsPerf;
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        Thermal* pThermal = pJsPerf->GetPerfObj()->GetGpuSubdevice()->GetThermal();
        RETURN_RC(pThermal->PrintCapPolicyTable((Tee::Priority)pri));
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsPerf,
                GetVfeVars,
                1,
                "Get all VFE independent variables and overrides")
{
    STEST_HEADER(1, 1, "Usage: Perf.GetVfeVars([vfeVars])");
    STEST_PRIVATE(JsPerf, pJsPerf, "JsPerf");
    STEST_ARG(0, JSObject *, vfeVars);
    if ((pJsPerf = JS_GET_PRIVATE(JsPerf, pContext, pObject, "JsPerf")) != 0)
    {
        Perf *pPerf = pJsPerf->GetPerfObj();
        RC rc;
        C_CHECK_RC(pPerf->GetVfeIndepVarsToJs(pContext, vfeVars));
        RETURN_RC(rc);
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsPerf,
                PrintVfeVars,
                0,
                "Prints all VFE independent variables and overrides")
{
    STEST_HEADER(0, 0, "Usage: Perf.PrintVfeVars()");
    STEST_PRIVATE(JsPerf, pJsPerf, "JsPerf");
    Perf *pPerf = pJsPerf->GetPerfObj();
    RETURN_RC(pPerf->PrintVfeIndepVars());
}

JS_STEST_LWSTOM(JsPerf,
                PrintVfeEqus,
                0,
                "Prints all VFE equations")
{
    STEST_HEADER(0, 0, "Usage: Perf.PrintVfeEqus()");
    STEST_PRIVATE(JsPerf, pJsPerf, "JsPerf");
    Perf *pPerf = pJsPerf->GetPerfObj();
    RETURN_RC(pPerf->PrintVfeEqus());
}

JS_STEST_LWSTOM(JsPerf,
                OverrideVfeVar,
                4,
                "Override an independent variable in the VFE table")
{
    const char* usageStr =
        "Perf.OverrideVfeVar(PerfConst.CLASS_*, index, PerfConst.OVERRIDE_*, value)\n"
        " value must be a float (e.g. 1234.56 or 234 or 234.00)\n"
        " index corresponds to the variable entry in the VFE table\n"
        "   (use -dump_vfe_vars to see overridable entries)\n"
        " index < 0 means the override applies to all variables of that class\n";

    STEST_HEADER(4, 4, usageStr);
    STEST_PRIVATE(JsPerf, pJsPerf, "JsPerf");

    Perf::VfeOverrideType ot;
    Perf::VfeVarClass varClass;
    INT32 index;
    FLOAT32 val;

    if (4 != pJavaScript->UnpackArgs(pArguments, NumArguments, "iiif",
                &varClass, &index, &ot, &val))
    {
        JS_ReportError(pContext, usageStr);
        return JS_FALSE;
    }

    RETURN_RC(pJsPerf->GetPerfObj()->OverrideVfeVar(ot, varClass, index, val));
}

JS_STEST_LWSTOM(JsPerf,
                OverrideVfeVarRange,
                4,
                "Override an independent variable range in the VFE table")
{
    const char* usageStr =
        "Perf.OverrideVfeVarRange(PerfConst.CLASS_*, index, rangemin, rangemax)\n"
        " value must be a float (e.g. 1234.56 or 234 or 234.00)\n"
        " index corresponds to the variable entry in the VFE table\n"
        "   (use -dump_vfe_vars to see overridable entries)\n"
        " index < 0 means the override applies to all variables of that class\n";

    STEST_HEADER(4, 4, usageStr);
    STEST_PRIVATE(JsPerf, pJsPerf, "JsPerf");

    Perf::VfeVarClass varClass;
    INT32 index;
    FLOAT32 rangemin;
    FLOAT32 rangemax;

    if (4 != pJavaScript->UnpackArgs(pArguments, NumArguments, "iiff",
                &varClass, &index, &rangemin, &rangemax))
    {
        JS_ReportError(pContext, usageStr);
        return JS_FALSE;
    }

    RETURN_RC(pJsPerf->GetPerfObj()->OverrideVfeVarRange(varClass, index, make_pair(rangemin, rangemax)));
}

JS_STEST_LWSTOM(JsPerf,
                OverrideVfeEquQuadratic,
                4,
                "Change the coefficients for a QUADRATIC Vfe equ")
{
    const char* usageStr =
        "Perf.OverrideVfeEquQuadratic(index, coeff0, coeff1, coeff2)\n"
        " index - which equation in the VFE table to modify\n"
        " coeff0 - float that corresponds to C0\n";

    STEST_HEADER(4, 4, usageStr);
    STEST_PRIVATE(JsPerf, pJsPerf, "JsPerf");

    LW2080_CTRL_PERF_VFE_EQU_CONTROL vfeEquControl = {};
    UINT32 idx;
    FLOAT32 coeffs[3];

    if (4 != pJavaScript->UnpackArgs(pArguments, NumArguments, "Ifff",
                                     &idx, &coeffs[0], &coeffs[1], &coeffs[2]))
    {
        JS_ReportError(pContext, usageStr);
        return JS_FALSE;
    }

    for (UINT32 ii = 0; ii < 3; ii++)
    {
        vfeEquControl.data.quadratic.coeffs[ii] = *reinterpret_cast<UINT32*>(&coeffs[ii]);
    }

    vfeEquControl.super.type = LW2080_CTRL_PERF_VFE_EQU_TYPE_QUADRATIC;
    RETURN_RC(pJsPerf->GetPerfObj()->OverrideVfeEqu(idx, vfeEquControl));
}

JS_STEST_LWSTOM(JsPerf,
                OverrideVfeEquMinMax,
                2,
                "Change whether to use a min or max for a MINMAX VFE equ")
{
    const char* usageStr =
        "Perf.OverrideVfeEquMinMax(index, bMax)\n"
        " index - which equation in the VFE table to modify\n"
        " bMax  - boolean which specifies min or max\n";

    STEST_HEADER(2, 2, usageStr);
    STEST_PRIVATE(JsPerf, pJsPerf, "JsPerf");

    LW2080_CTRL_PERF_VFE_EQU_CONTROL vfeEquControl = {};
    UINT32 idx;
    bool bMax;
    if (2 != pJavaScript->UnpackArgs(pArguments, NumArguments, "I?",
                                     &idx, &bMax))
    {
        JS_ReportError(pContext, usageStr);
        return JS_FALSE;
    }
    vfeEquControl.data.minmax.bMax = bMax;

    vfeEquControl.super.type = LW2080_CTRL_PERF_VFE_EQU_TYPE_MINMAX;
    RETURN_RC(pJsPerf->GetPerfObj()->OverrideVfeEqu(idx, vfeEquControl));
}

JS_STEST_LWSTOM(JsPerf,
                OverrideVfeEquCompare,
                3,
                "Change the next func and comparison criteria for a COMPARE Vfe equ")
{
    const char* usageStr =
        "Perf.OverrideVfeEquCompare(index, funcId, criteria)\n"
        " index    - which equation in the VFE table to modify\n"
        " funcId   - 8 bit integer that is an index into the VFE equ table\n"
        " criteria - floating point number used for comparison\n";

    STEST_HEADER(3, 3, usageStr);
    STEST_PRIVATE(JsPerf, pJsPerf, "JsPerf");

    LW2080_CTRL_PERF_VFE_EQU_CONTROL vfeEquControl = {};
    UINT32 idx;
    UINT08 funcId;
    FLOAT32 criteria;
    if (3 != pJavaScript->UnpackArgs(pArguments, NumArguments, "Ibf",
                                     &idx, &funcId, &criteria))
    {
        JS_ReportError(pContext, usageStr);
        return JS_FALSE;
    }
    vfeEquControl.data.compare.funcId = funcId;
    vfeEquControl.data.compare.criteria = *reinterpret_cast<UINT32*>(&criteria);
    vfeEquControl.super.type = LW2080_CTRL_PERF_VFE_EQU_TYPE_COMPARE;
    RETURN_RC(pJsPerf->GetPerfObj()->OverrideVfeEqu(idx, vfeEquControl));
}

JS_STEST_BIND_THREE_ARGS(JsPerf, OverrideVfeEquOutputRange,
                         idx, UINT32,
                         bMax, bool,
                         val, FLOAT32,
                         "Override the output range for a VFE equation");
RC JsPerf::OverrideVfeEquOutputRange(UINT32 idx, bool bMax, FLOAT32 val)
{
    return m_pPerf->OverrideVfeEquOutputRange(idx, bMax, val);
}

JS_STEST_BIND_NO_ARGS(JsPerf, UseIMPCompatibleLimits,
                      "Use IMP compatible CLIENT_LOW priority perf limits");
RC JsPerf::UseIMPCompatibleLimits()
{
    return m_pPerf->UseIMPCompatibleLimits();
}

JS_STEST_BIND_NO_ARGS(JsPerf, RestoreDefaultLimits,
                      "Restore CLIENT_STRICT priority perf limits (back to default behavior)");
RC JsPerf::RestoreDefaultLimits()
{
    return m_pPerf->RestoreDefaultLimits();
}

JS_SMETHOD_LWSTOM(JsPerf,
                  Is1xClockDomain,
                  1,
                  "Return true if clock domain is a 1x clock (e.g. gpcclk)")
{
    STEST_HEADER(1, 1, "Usage: var bIs1xDom = subdev.Perf.Is1xClockDomain(dom)");
    STEST_PRIVATE(JsPerf, pJsPerf, "JsPerf");
    STEST_ARG(0, INT32, clkDom);

    if (OK != pJavaScript->ToJsval(PerfUtil::Is1xClockDomain(static_cast<Gpu::ClkDomain>(clkDom)), pReturlwalue))
    {
        JS_ReportError(pContext, "Error oclwrred in Perf.Is1xClockDomain");
        return JS_FALSE;
    }

    return JS_TRUE;
}

JS_SMETHOD_LWSTOM(JsPerf,
                  IsDomainTypeMaster,
                  1,
                  "Return true if clock domain is a master")
{
    STEST_HEADER(1, 1, "Usage: var bIs1xDom = subdev.Perf.IsDomainTypeMaster(dom)");
    STEST_PRIVATE(JsPerf, pJsPerf, "JsPerf");
    STEST_ARG(0, INT32, clkDom);
    Perf40* pPerf40 = dynamic_cast<Perf40*>(pJsPerf->GetPerfObj());
    if (!pPerf40)
    {
        Printf(Tee::PriError, "Perf.IsDomainTypeMaster is supported only in pstates 40");
        return JS_FALSE;
    }

    if (RC::OK != pJavaScript->ToJsval(pPerf40->IsDomainTypeMaster(
                                       static_cast<Gpu::ClkDomain>(clkDom)),
                                       pReturlwalue))
    {
        Printf(Tee::PriError, "Error oclwrred in Perf.IsDomainTypeMaster");
        return JS_FALSE;
    }

    return JS_TRUE;
}

JS_SMETHOD_LWSTOM(JsPerf,
                  Is2xClockDomain,
                  1,
                  "Return true if clock domain is a 1x clock (e.g. gpcclk)")
{
    STEST_HEADER(1, 1, "Usage: var bIs2xDom = subdev.Perf.Is2xClockDomain(dom)");
    STEST_PRIVATE(JsPerf, pJsPerf, "JsPerf");
    STEST_ARG(0, INT32, clkDom);

    if (OK != pJavaScript->ToJsval(PerfUtil::Is2xClockDomain(static_cast<Gpu::ClkDomain>(clkDom)), pReturlwalue))
    {
        JS_ReportError(pContext, "Error oclwrred in Perf.Is2xClockDomain");
        return JS_FALSE;
    }

    return JS_TRUE;
}

JS_SMETHOD_LWSTOM(JsPerf,
                  ColwertTo1xClockDomain,
                  1,
                  "Colwert a 2x domain to a 1x domain (e.g. gpc2clk to gpcclk)")
{
    STEST_HEADER(1, 1,
                 "Usage: var dom1x = subdev.Perf.ColwertTo1xClockDomain(dom2x)");
    STEST_PRIVATE(JsPerf, pJsPerf, "JsPerf");
    STEST_ARG(0, INT32, clkDom);

    if (OK != pJavaScript->ToJsval(PerfUtil::ColwertTo1xClockDomain(static_cast<Gpu::ClkDomain>(clkDom)), pReturlwalue))
    {
        JS_ReportError(pContext, "Error oclwrred in Perf.ColwertTo1xClockDomain");
        return JS_FALSE;
    }

    return JS_TRUE;
}

JS_SMETHOD_LWSTOM(JsPerf,
                  ColwertTo2xClockDomain,
                  1,
                  "Colwert a 1x domain to a 2x domain (e.g. gpcclk to gpc2clk)")
{
    STEST_HEADER(1, 1,
                 "Usage: var dom2x = subdev.Perf.ColwertTo2xClockDomain(dom1x)");
    STEST_PRIVATE(JsPerf, pJsPerf, "JsPerf");
    STEST_ARG(0, INT32, clkDom);

    if (OK != pJavaScript->ToJsval(PerfUtil::ColwertTo2xClockDomain(static_cast<Gpu::ClkDomain>(clkDom)), pReturlwalue))
    {
        JS_ReportError(pContext, "Error oclwrred in Perf.ColwertTo2xClockDomain");
        return JS_FALSE;
    }

    return JS_TRUE;
}

RC JsPerf::EnableArbitration(UINT32 clkDomain)
{
    return m_pPerf->EnableArbitration(static_cast<Gpu::ClkDomain>(clkDomain));
}
JS_STEST_BIND_ONE_ARG(JsPerf, EnableArbitration,
                      clkDomain, UINT32,
                      "Enable perf arbitration for a clock domain");

RC JsPerf::DisableArbitration(UINT32 clkDomain)
{
    return m_pPerf->DisableArbitration(static_cast<Gpu::ClkDomain>(clkDomain));
}
JS_STEST_BIND_ONE_ARG(JsPerf, DisableArbitration,
                      clkDomain, UINT32,
                      "Disable perf arbitration for a clock domain");

RC JsPerf::LockArbiter()
{
    return m_pPerf->LockArbiter();
}
JS_STEST_BIND_NO_ARGS(JsPerf, LockArbiter,
                      "Lock the Arbiter");

RC JsPerf::UnlockArbiter()
{
    return m_pPerf->UnlockArbiter();
}
JS_STEST_BIND_NO_ARGS(JsPerf, UnlockArbiter,
                      "Unlock the Arbiter");

JS_SMETHOD_LWSTOM(JsPerf,
                  IsArbitrated,
                  1,
                  "Query RM perf arbitration status for a clock domain")
{
    STEST_HEADER(1, 1,
        "Usage: var isArbitrated = GpuSub.Perf.IsArbitrated(Gpu.ClkPexGen)");
    STEST_PRIVATE(JsPerf, pJsPerf, "JsPerf");
    STEST_ARG(0, INT32, clkDom);

    Perf *pPerf = pJsPerf->GetPerfObj();
    if (OK != pJavaScript->ToJsval(pPerf->IsArbitrated(
        static_cast<Gpu::ClkDomain>(clkDom)), pReturlwalue))
    {
        JS_ReportError(pContext, "Error oclwrred in Perf.IsArbitrated");
        return JS_FALSE;
    }

    return JS_TRUE;
}

RC JsPerf::DisableLwstomerBoostMaxLimit()
{
    return m_pPerf->DisablePerfLimit(LW2080_CTRL_PERF_LIMIT_LWSTOMER_BOOST_MAX);
}
JS_STEST_BIND_NO_ARGS(JsPerf, DisableLwstomerBoostMaxLimit,
                      "Disable the LWSTOMER_BOOST_MAX perf limit");

RC JsPerf::DisablePerfLimit(UINT32 limitId)
{
    return m_pPerf->DisablePerfLimit(limitId);
}
JS_STEST_BIND_ONE_ARG(JsPerf, DisablePerfLimit,
                      limitId, UINT32,
                      "Disable the specified perf limit");

UINT32 JsPerf::GetImpFirstIdx() const
{
    UINT32 idx;
    if (m_pPerf->GetVPStateIdx(LW2080_CTRL_PERF_VPSTATES_IDX_IMPFIRST, &idx) != OK)
    {
        Printf(Tee::PriError, "Unable to retrieve first IMP vpstate index\n");
    }
    return idx;
}

UINT32 JsPerf::GetImpLastIdx() const
{
    UINT32 idx;
    if (m_pPerf->GetVPStateIdx(LW2080_CTRL_PERF_VPSTATES_IDX_IMPLAST, &idx) != OK)
    {
        Printf(Tee::PriError, "Unable to retrieve last IMP vpstate index\n");
    }
    return idx;
}

CLASS_PROP_READONLY(JsPerf, ImpFirstIdx, UINT32, "Index of the first IMP vpstate");
CLASS_PROP_READONLY(JsPerf, ImpLastIdx, UINT32, "Index of the last IMP vpstate");

bool JsPerf::GetArbiterEnabled() const
{
    return m_pPerf->GetArbiterEnabled();
}

CLASS_PROP_READONLY(JsPerf, ArbiterEnabled, bool, "true if the RM arbiter is enabled");

JS_SMETHOD_LWSTOM(JsPerf,
                  GetClockSamples,
                  1,
                  "Get current frequency for specified clocks in kHz")
{
    STEST_HEADER(1, 1,
                 "Usage: subdev.Perf.GetClockSamples(pClkSamples)");
    STEST_PRIVATE(JsPerf, pJsPerf, "JsPerf");
    STEST_ARG(0, JsArray, ClkSamples);

    StickyRC rc;
    UINT32 domain;
    size_t numDomains = ClkSamples.size();
    vector<Perf::ClkSample> samples(numDomains);
    for (UINT32 i = 0; i < numDomains; i++)
    {
        JSObject* pClk;
        C_CHECK_RC(pJavaScript->FromJsval(ClkSamples[i], &pClk));
        C_CHECK_RC(pJavaScript->GetProperty(pClk, "Domain",
                                            &domain));
        samples[i].Domain = static_cast<Gpu::ClkDomain>(domain);
    }

    // If any of the clk domains returns an error, rc != OK
    // But we still can fill out the info available for the other
    // domains
    rc = pJsPerf->GetPerfObj()->GetClockSamples(&samples);

    JSObject* pClk;
    for (UINT32 i = 0; i < samples.size(); i++)
    {
        rc = pJavaScript->FromJsval(ClkSamples[i], &pClk);
        rc = pJavaScript->SetProperty(pClk, "TargetkHz",
                                      samples[i].TargetkHz);
        rc = pJavaScript->SetProperty(pClk, "ActualkHz",
                                      samples[i].ActualkHz);
        rc = pJavaScript->SetProperty(pClk, "EffectivekHz",
                                      samples[i].EffectivekHz);
        rc = pJavaScript->SetProperty(pClk, "Src",
                                      samples[i].Src);
    }

    RETURN_RC(rc);
}

JS_SMETHOD_LWSTOM(JsPerf,
                  GetReadableClkDomains,
                  1,
                  "Get all readable clock domains")
{
    STEST_HEADER(1, 1,
                 "Usage: subdev.Perf.GetReadableClkDomains(pClks)");
    STEST_PRIVATE(JsPerf, pJsPerf, "JsPerf");
    STEST_ARG(0, JSObject*, pClks);

    RC rc;
    vector<Gpu::ClkDomain> clks;
    C_CHECK_RC(pJsPerf->GetPerfObj()->GetRmClkDomains(&clks, Perf::ClkDomainProp::READABLE));
    for (UINT32 i = 0; i < clks.size(); i++)
    {
        C_CHECK_RC(pJavaScript->SetElement(pClks, i, clks[i]));
    }

    RETURN_RC(rc);
}

JS_SMETHOD_LWSTOM(JsPerf,
                  GetWriteableClkDomains,
                  1,
                  "Get all writeable clock domains")
{
    STEST_HEADER(1, 1,
                 "Usage: subdev.Perf.GetWriteableClkDomains(pClks)");
    STEST_PRIVATE(JsPerf, pJsPerf, "JsPerf");
    STEST_ARG(0, JSObject*, pClks);

    RC rc;
    vector<Gpu::ClkDomain> clks;
    C_CHECK_RC(pJsPerf->GetPerfObj()->GetRmClkDomains(&clks, Perf::ClkDomainProp::WRITEABLE));
    for (UINT32 i = 0; i < clks.size(); i++)
    {
        C_CHECK_RC(pJavaScript->SetElement(pClks, i, clks[i]));
    }

    RETURN_RC(rc);
}

JS_STEST_LWSTOM(JsPerf, GetActivePerfLimits, 1, "Get the names of all active perf limits")
{
    STEST_HEADER(1, 1,
                 "Usage: rc = subdev.Perf.GetActivePerfLimits(limitsObj)");
    STEST_PRIVATE(JsPerf, pJsPerf, "JsPerf");
    STEST_ARG(0, JSObject*, pLimitsObj);

    RC rc;
    Perf* pPerf = pJsPerf->GetPerfObj();
    C_CHECK_RC(pPerf->GetActivePerfLimitsToJs(pContext, pLimitsObj));

    RETURN_RC(rc);
}

RC JsPerf::SetFmonTestPoint(UINT32 gpcclk_MHz, UINT32 margin_MHz, UINT32 margin_uV)
{
    return m_pPerf->SetFmonTestPoint(gpcclk_MHz, margin_MHz, margin_uV);
}
JS_STEST_BIND_THREE_ARGS(JsPerf, SetFmonTestPoint,
                         gpcclk_MHz, UINT32,
                         margin_MHz, UINT32,
                         margin_uV, UINT32,
                         "Set gpcclk with a VF margin applied for FMON testing");

RC JsPerf::ScaleSpeedo(FLOAT32 factor)
{
    return m_pPerf->ScaleSpeedo(factor);
}
JS_STEST_BIND_ONE_ARG(JsPerf, ScaleSpeedo,
                      factor, FLOAT32,
                      "Scale speedo by a floating point factor");

RC JsPerf::OverrideKappa(FLOAT32 factor)
{
    return m_pPerf->OverrideKappa(factor);
}
JS_STEST_BIND_ONE_ARG(JsPerf, OverrideKappa,
                      factor, FLOAT32,
                      "Override Kappa by a floating point number");

JS_STEST_LWSTOM(JsPerf, UnlockClockDomains, 1,
    "Allow any clock frequency in any pstate for the specified domain(s)")
{
    STEST_HEADER(1, 1, "Usage: rc = subdev.Perf.UnlockClockDomains([Gpu.ClkM, Gpu.ClkGpc])");
    STEST_PRIVATE(JsPerf, pJsPerf, "JsPerf");
    STEST_ARG(0, JsArray, clkDoms);

    RC rc;

    vector<Gpu::ClkDomain> clkDomsToUnlock;
    for (const auto& clkDom : clkDoms)
    {
        UINT32 clkDomNum;
        C_CHECK_RC(pJavaScript->FromJsval(clkDom, &clkDomNum));
        clkDomsToUnlock.push_back(static_cast<Gpu::ClkDomain>(clkDomNum));
    }
    C_CHECK_RC(pJsPerf->GetPerfObj()->UnlockClockDomains(clkDomsToUnlock));
    RETURN_RC(rc);
}

JS_STEST_LWSTOM(JsPerf,
                SetUseMinSramBin,
                1,
                "Setup SRAM_VMIN based on POR JSON values")
{
    STEST_HEADER(1, 1, "Usage: rc = subdev.Perf.SetUseMinSramBin(SkuName)");
    STEST_PRIVATE(JsPerf, pJsPerf, "JsPerf");

    STEST_ARG(0, string, SkuNameStr);
    if (1 != pJavaScript->UnpackArgs(pArguments, NumArguments, "s",
                &SkuNameStr))
    {
        Printf(Tee::PriError, "SetUseMinSramMin - Check arguments\n");
        return JS_FALSE;
    }

    RETURN_RC(pJsPerf->GetPerfObj()->SetUseMinSramBin(SkuNameStr));
}

JS_STEST_LWSTOM(JsPerf,
                SetClockPropRegime,
                1,
                "Set the CLK_PROP_REGIME structures.")
{
    STEST_HEADER(1, 1, "Usage: rc = subdev.Perf40.SetClkPropRegimeStructures(JSArray)");
    STEST_PRIVATE(JsPerf, pJsPerf, "JsPerf");
    STEST_ARG(0, JsArray, JsClkDomainMaskArray);
    RC rc;
    Perf40* pPerf40 = dynamic_cast<Perf40*>(pJsPerf->GetPerfObj());
    if (!pPerf40)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }
    LW2080_CTRL_CLK_CLK_PROP_REGIMES_CONTROL clkPropRegimesControl = {};
    C_CHECK_RC(pPerf40->GetClkPropRegimesControl(&clkPropRegimesControl));
    for (UINT32 i = 0; i < JsClkDomainMaskArray.size(); i++)
    {
        JSObject *OneEntry;
        C_CHECK_RC(pJavaScript->FromJsval(JsClkDomainMaskArray[i], &OneEntry));
        UINT32 regimeId;
        UINT32 clkDomainMask;
        C_CHECK_RC(pJavaScript->UnpackFields(OneEntry, "II",
                                         "PropRegimeId", &regimeId,
                                         "ClockDomainMask", &clkDomainMask));
        clkPropRegimesControl.propRegimes[regimeId].clkDomainMask.super.pData[0] = clkDomainMask;
    }
    RETURN_RC(pPerf40->SetClkPropRegimesControl(&clkPropRegimesControl));
    return JS_FALSE;
}

//-----------------------------------------------------------------------------
//! \brief PmuConst javascript interface class
//!
//! Create a JS class to declare Perf constants on
//!
JS_CLASS(PerfConst);

JS_STEST_LWSTOM(JsPerf,
                GetOverclockRange,
                2,
                "Returns the overclocking range for a clock domain")
{
    STEST_HEADER(2, 2, "Usage: rc = subdev.Perf.GetOverclockRange(Gpu.ClkGpc, outObj)");
    STEST_PRIVATE(JsPerf, pJsPerf, "JsPerf");

    STEST_ARG(0, UINT32, clkDomNum);
    STEST_ARG(1, JSObject*, pRetObj);

    RC rc;
    Perf* pPerf = pJsPerf->GetPerfObj();
    Gpu::ClkDomain clkDom = static_cast<Gpu::ClkDomain>(clkDomNum);
    Perf::OverclockRange ocRange;

    C_CHECK_RC(pPerf->GetOverclockRange(clkDom, &ocRange));

    C_CHECK_RC(pJavaScript->PackFields(pRetObj, "ii",
        "MinOffsetkHz", ocRange.MinOffsetkHz,
        "MaxOffsetkHz", ocRange.MaxOffsetkHz));

    RETURN_RC(rc);
}
//-----------------------------------------------------------------------------
//! \brief ParamConst javascript interface object
//!
//! ParamConst javascript interface
//!
static SObject PerfConst_Object
(
    "PerfConst",
    PerfConstClass,
    0,
    0,
    "PerfConst JS Object"
);

PROP_CONST(PerfConst, ILWALID_PSTATE, Perf::ILWALID_PSTATE);
PROP_CONST(PerfConst, PRE_PSTATE, Perf::PRE_PSTATE);
PROP_CONST(PerfConst, POST_PSTATE, Perf::POST_PSTATE);
PROP_CONST(PerfConst, PRE_CLEANUP, Perf::PRE_CLEANUP);
PROP_CONST(PerfConst, SET_POWER_RAIL_VOLTAGE, Power::SET_POWER_RAIL_VOLTAGE); // deprecated
PROP_CONST(PerfConst, CHECK_POWER_RAIL_LEAKAGE, Power::CHECK_POWER_RAIL_LEAKAGE); // deprecated
PROP_CONST(PerfConst, FORCE_PLL, Perf::FORCE_PLL);
PROP_CONST(PerfConst, FORCE_BYPASS, Perf::FORCE_BYPASS);
PROP_CONST(PerfConst, FORCE_NAFLL, Perf::FORCE_NAFLL);
PROP_CONST(PerfConst, FORCE_LOOSE_LIMIT, Perf::FORCE_LOOSE_LIMIT);
PROP_CONST(PerfConst, FORCE_STRICT_LIMIT, Perf::FORCE_STRICT_LIMIT);
PROP_CONST(PerfConst, FORCE_IGNORE_LIMIT, Perf::FORCE_IGNORE_LIMIT);
PROP_CONST(PerfConst, FORCE_PERF_LIMIT_MIN, Perf::FORCE_PERF_LIMIT_MIN);
PROP_CONST(PerfConst, FORCE_PERF_LIMIT_MAX, Perf::FORCE_PERF_LIMIT_MAX);
PROP_CONST(PerfConst, VOLTAGE_IDX, Power::VOLTAGE_IDX); // deprecated
PROP_CONST(PerfConst, POWER_IDX, Power::POWER_IDX); // deprecated
PROP_CONST(PerfConst, LWRRENT_IDX, Power::LWRRENT_IDX); // deprecated
PROP_CONST(PerfConst, UNKNOWN_PT, Perf::GpcPerf_EXPLICIT);
PROP_CONST(PerfConst, EXPLICIT_PT, Perf::GpcPerf_EXPLICIT);
PROP_CONST(PerfConst, MAX_PT, Perf::GpcPerf_MAX);
PROP_CONST(PerfConst, MID_PT, Perf::GpcPerf_MID);
PROP_CONST(PerfConst, NOM_PT, Perf::GpcPerf_NOM);
PROP_CONST(PerfConst, MIN_PT, Perf::GpcPerf_MIN);
PROP_CONST(PerfConst, INTERSECT_PT, Perf::GpcPerf_INTERSECT);
PROP_CONST(PerfConst, TDP_PT, Perf::GpcPerf_TDP);
PROP_CONST(PerfConst, MIDPOINT_PT, Perf::GpcPerf_MIDPOINT);
PROP_CONST(PerfConst, TURBO_PT, Perf::GpcPerf_TURBO);
PROP_CONST(PerfConst, RTP, Power::RTP); // deprecated
PROP_CONST(PerfConst, TGP, Power::TGP); // deprecated
PROP_CONST(PerfConst, GPS, Thermal::GPS); // deprecated
PROP_CONST(PerfConst, Acoustic, Thermal::Acoustic); // deprecated
PROP_CONST(PerfConst, MEMORY, Thermal::MEMORY);
PROP_CONST(PerfConst, DEFAULT_INFLECTION_PT, string(Perf::DEFAULT_INFLECTION_PT));
PROP_CONST(PerfConst, FIXED, Perf::FIXED);
PROP_CONST(PerfConst, PSTATE, Perf::PSTATE);
PROP_CONST(PerfConst, DECOUPLED, Perf::DECOUPLED);
PROP_CONST(PerfConst, RATIO, Perf::RATIO);
PROP_CONST(PerfConst, PROGRAMMABLE, Perf::PROGRAMMABLE);
PROP_CONST(PerfConst, UNKNOWN_TYPE, Perf::UNKNOWN_TYPE);
PROP_CONST(PerfConst, NotLocked, Perf::NotLocked);
PROP_CONST(PerfConst, SoftLock, Perf::SoftLock);
PROP_CONST(PerfConst, DefaultLock, Perf::DefaultLock);
PROP_CONST(PerfConst, StrictLock, Perf::StrictLock);
PROP_CONST(PerfConst, HardLock, Perf::HardLock);
PROP_CONST(PerfConst, VirtualLock, Perf::VirtualLock);
PROP_CONST(PerfConst, CLIENT_PSTATE,
           LW2080_CTRL_PMGR_VOLTAGE_REQUEST_CLIENT_PSTATE);
PROP_CONST(PerfConst, CLIENT_GPC2CLK,
           LW2080_CTRL_PMGR_VOLTAGE_REQUEST_CLIENT_GPC2CLK);
PROP_CONST(PerfConst, CLIENT_DISPCLK,
           LW2080_CTRL_PMGR_VOLTAGE_REQUEST_CLIENT_DISPCLK);
PROP_CONST(PerfConst, VOLTAGE_LOGIC, Gpu::VOLTAGE_LOGIC);
PROP_CONST(PerfConst, VOLTAGE_SRAM, Gpu::VOLTAGE_SRAM);
PROP_CONST(PerfConst, VOLTAGE_MSVDD, Gpu::VOLTAGE_MSVDD);
PROP_CONST(PerfConst, VMINFLOOR_MODS_PSTATE, Perf::PStateVDTIdxMode);
PROP_CONST(PerfConst, VMINFLOOR_MODS_VDT, Perf::VDTEntryIdxMode);
PROP_CONST(PerfConst, VMINFLOOR_MODS_LOGICAL, Perf::LogicalVoltageMode);
PROP_CONST(PerfConst, DEFAULT_MAX_PERF_SWITCH_TIME_NS, Perf::DEFAULT_MAX_PERF_SWITCH_TIME_NS);
PROP_CONST(PerfConst, OVERRIDE_NONE, Perf::OVERRIDE_NONE);
PROP_CONST(PerfConst, OVERRIDE_VALUE, Perf::OVERRIDE_VALUE);
PROP_CONST(PerfConst, OVERRIDE_OFFSET, Perf::OVERRIDE_OFFSET);
PROP_CONST(PerfConst, OVERRIDE_SCALE, Perf::OVERRIDE_SCALE);
PROP_CONST(PerfConst, CLASS_FREQ, Perf::CLASS_FREQ);
PROP_CONST(PerfConst, CLASS_VOLT, Perf::CLASS_VOLT);
PROP_CONST(PerfConst, CLASS_TEMP, Perf::CLASS_TEMP);
PROP_CONST(PerfConst, CLASS_FUSE, Perf::CLASS_FUSE);
PROP_CONST(PerfConst, CLASS_FUSE_20, Perf::CLASS_FUSE_20);
PROP_CONST(PerfConst, CLASS_PRODUCT, Perf::CLASS_PRODUCT);
PROP_CONST(PerfConst, CLASS_SUM, Perf::CLASS_SUM);
PROP_CONST(PerfConst, CLASS_GLOBAL, Perf::CLASS_GLOBAL);
PROP_CONST(PerfConst, COMPARE_EQUAL, LW2080_CTRL_PERF_VFE_EQU_COMPARE_FUNCTION_EQUAL);
PROP_CONST(PerfConst, COMPARE_GREATER_EQ, LW2080_CTRL_PERF_VFE_EQU_COMPARE_FUNCTION_GREATER_EQ);
PROP_CONST(PerfConst, COMPARE_GREATER, LW2080_CTRL_PERF_VFE_EQU_COMPARE_FUNCTION_GREATER);
PROP_CONST(PerfConst, INTERSECT_LOGICAL, Perf::IntersectLogical);
PROP_CONST(PerfConst, INTERSECT_VDT, Perf::IntersectVDT);
PROP_CONST(PerfConst, INTERSECT_VFE, Perf::IntersectVFE);
PROP_CONST(PerfConst, INTERSECT_PSTATE, Perf::IntersectPState);
PROP_CONST(PerfConst, INTERSECT_VPSTATE, Perf::IntersectVPState);
PROP_CONST(PerfConst, INTERSECT_VOLTFREQUENCY, Perf::IntersectVoltFrequency);
PROP_CONST(PerfConst, INTERSECT_FREQUENCY, Perf::IntersectFrequency);
PROP_CONST(PerfConst, FIXED_FREQUENCY_REGIME, Perf::FIXED_FREQUENCY_REGIME);
PROP_CONST(PerfConst, VOLTAGE_REGIME, Perf::VOLTAGE_REGIME);
PROP_CONST(PerfConst, FREQUENCY_REGIME, Perf::FREQUENCY_REGIME);
PROP_CONST(PerfConst, VR_ABOVE_NOISE_UNAWARE_VMIN, Perf::VR_ABOVE_NOISE_UNAWARE_VMIN);
PROP_CONST(PerfConst, FFR_BELOW_DVCO_MIN, Perf::FFR_BELOW_DVCO_MIN);
PROP_CONST(PerfConst, VR_WITH_CPM, Perf::VR_WITH_CPM);
PROP_CONST(PerfConst, DEFAULT_REGIME, Perf::DEFAULT_REGIME);
PROP_CONST(PerfConst, MODS_LIMITS, PerfUtil::MODS_LIMITS);
PROP_CONST(PerfConst, RM_LIMITS, PerfUtil::RM_LIMITS);
PROP_CONST(PerfConst, NO_LIMITS, PerfUtil::NO_LIMITS);
PROP_CONST(PerfConst, ONLY_ENABLED_LIMITS, PerfUtil::ONLY_ENABLED_LIMITS);
PROP_CONST(PerfConst, VERBOSE_LIMITS, PerfUtil::VERBOSE_LIMITS);
PROP_CONST(PerfConst, LIMIT_LWSTOMER_BOOST_MAX, LW2080_CTRL_PERF_LIMIT_LWSTOMER_BOOST_MAX);
PROP_CONST(PerfConst, LIMIT_ECC_MAX, LW2080_CTRL_PERF_LIMIT_ECC_MAX);
PROP_CONST(PerfConst, VoltToFreq, Perf::VoltToFreq);
PROP_CONST(PerfConst, FreqToVolt, Perf::FreqToVolt);
PROP_CONST(PerfConst, ILWALID_VPSTATE_IDX, LW2080_CTRL_PERF_VPSTATE_INDEX_ILWALID);
PROP_CONST(PerfConst, PSTATE_UNSUPPORTED, static_cast<UINT08>(Perf::PStateVersion::PSTATE_UNSUPPORTED)); //$
PROP_CONST(PerfConst, PSTATE_10, static_cast<UINT08>(Perf::PStateVersion::PSTATE_10));
PROP_CONST(PerfConst, PSTATE_20, static_cast<UINT08>(Perf::PStateVersion::PSTATE_20));
PROP_CONST(PerfConst, PSTATE_30, static_cast<UINT08>(Perf::PStateVersion::PSTATE_30));
PROP_CONST(PerfConst, PSTATE_35, static_cast<UINT08>(Perf::PStateVersion::PSTATE_35));
PROP_CONST(PerfConst, PSTATE_40, static_cast<UINT08>(Perf::PStateVersion::PSTATE_40));
PROP_CONST(PerfConst, ClkTypeMaster, Perf::ClockDomainType::Master);
PROP_CONST(PerfConst, ClkTypeSlave,  Perf::ClockDomainType::Slave);
PROP_CONST(PerfConst, ClkTypeNone,   Perf::ClockDomainType::None);
PROP_CONST(PerfConst, MODS_VFSWITCH_VAL, Perf::VfSwitchVal::Mods);
PROP_CONST(PerfConst, RMLIMITS_VFSWITCH_VAL, Perf::VfSwitchVal::RmLimits);
PROP_CONST(PerfConst, PMU_VFSWITCH_VAL, Perf::VfSwitchVal::Pmu);
PROP_CONST(PerfConst, GpcStrictPropRegime, LW2080_CTRL_CLK_CLK_PROP_REGIME_ID_GPC_STRICT);
