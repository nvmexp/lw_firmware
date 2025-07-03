/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2010 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "js_perfmon.h"
#include "core/include/jscript.h"
#include "core/include/script.h"
#include "jsapi.h"
#include "core/include/xp.h"
#include "core/include/rc.h"

JsPerfmon::JsPerfmon()
    : m_pPerfmon(NULL), m_pJsPerfmonObj(NULL)
{
}

//-----------------------------------------------------------------------------
static void C_JsPerfmon_finalize
(
    JSContext *cx,
    JSObject *obj
)
{
    MASSERT(cx != 0);
    MASSERT(obj != 0);

    JsPerfmon * pJsPerfmon;
    //! Delete the C++
    pJsPerfmon = (JsPerfmon *)JS_GetPrivate(cx, obj);
    delete pJsPerfmon;
};

//-----------------------------------------------------------------------------
static JSClass JsPerfmon_class =
{
    "JsPerfmon",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ColwertStub,
    C_JsPerfmon_finalize
};

//-----------------------------------------------------------------------------
static SObject JsPerfmon_Object
(
    "JsPerfmon",
    JsPerfmon_class,
    0,
    0,
    "Perfmon JS Object"
);

//------------------------------------------------------------------------------
//! \brief Create a JS Object representation of the current associated
//! Perfmon object
RC JsPerfmon::CreateJSObject(JSContext *cx, JSObject *obj)
{
    //! Only create one JSObject per Perfmon object
    if (m_pJsPerfmonObj)
    {
        Printf(Tee::PriLow,
               "A JS Object has already been created for this JsPerfmon.\n");
        return OK;
    }

    m_pJsPerfmonObj = JS_DefineObject(cx,
                                    obj, // GpuSubdevice object
                                    "Perfmon", // Property name
                                    &JsPerfmon_class,
                                    JsPerfmon_Object.GetJSObject(),
                                    JSPROP_READONLY);

    if (!m_pJsPerfmonObj)
        return RC::COULD_NOT_CREATE_JS_OBJECT;

    //! Store the current JsPerfmon instance into the private area
    //! of the new JSOBject.
    if (JS_SetPrivate(cx, m_pJsPerfmonObj, this) != JS_TRUE)
    {
        Printf(Tee::PriNormal,
               "Unable to set private value of JsPerfmon.\n");
        return RC::COULD_NOT_CREATE_JS_OBJECT;
    }

    // Add the .Help() to this JSObject
    JS_DefineFunction(cx, m_pJsPerfmonObj, "Help", &C_Global_Help, 1, 0);

    return OK;
}

//------------------------------------------------------------------------------
//! \brief Return the private JS data - C++ Perfmon object.
//!
GpuPerfmon * JsPerfmon::GetPerfmonObj()
{
    MASSERT(m_pPerfmon);
    return m_pPerfmon;
}

//------------------------------------------------------------------------------
//! \brief Set the associated Perfmon object.
//!
//! This is called by JS GpuSubdevice Initialize
void JsPerfmon::SetPerfmonObj(GpuPerfmon * pPerfmon)
{
    m_pPerfmon = pPerfmon;
}

//------------------------------------------------------------------------------
//! \brief Add an element to the JsPerfmon experiment vector
//!
UINT32 JsPerfmon::ExpVecAdd(const GpuPerfmon::Experiment* pExp)
{
    m_ExpVector.push_back(pExp);
    return (UINT32)m_ExpVector.size() - 1;
}

RC JsPerfmon::ExpVecSet(UINT32 index, const GpuPerfmon::Experiment* pExp)
{
    if(index > m_ExpVector.size() - 1)
    {
        Printf(Tee::PriNormal, "Invalid index passed to JsPerfmon::ExpVecSet\n");
        return RC::SOFTWARE_ERROR;
    }

    m_ExpVector[index] = pExp;
    return OK;
}

const GpuPerfmon::Experiment* JsPerfmon::ExpVecGet(UINT32 index)
{
    if(index > m_ExpVector.size() - 1)
    {
        Printf(Tee::PriNormal, "Invalid index passed to JsPerfmon::ExpVecGet\n");
        return NULL;
    }

    return m_ExpVector[index];
}

JS_STEST_LWSTOM(JsPerfmon,
                CreateL2CacheExperiment,
                3,
                "Create a L2 experiment and add it to the monitor")
{
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    JavaScriptPtr pJavaScript;
    UINT32 FbpNum;
    UINT32 L2Slice;
    JSObject* pReturnExperimentIndex = 0;

    const char usage[] = "Usage: subdev.Perfmon.CreateL2CacheExperiment(FbpNum, L2slice, RetExpIndex)\n";

    if ((NumArguments != 3) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &FbpNum))||
        (OK != pJavaScript->FromJsval(pArguments[1], &L2Slice))||
        (OK != pJavaScript->FromJsval(pArguments[2], &pReturnExperimentIndex)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsPerfmon *pJsPerfmon;
    if ((pJsPerfmon = JS_GET_PRIVATE(JsPerfmon, pContext, pObject, "JsPerfmon")) != 0)
    {
        RC rc;
        const GpuPerfmon::Experiment* l2Experiment;
        GpuPerfmon* pPerfmon = pJsPerfmon->GetPerfmonObj();
        C_CHECK_RC(pPerfmon->CreateL2CacheExperiment(FbpNum, L2Slice, &l2Experiment));

        UINT32 index = pJsPerfmon->ExpVecAdd(l2Experiment);
        RETURN_RC(pJavaScript->SetElement(pReturnExperimentIndex, 0, index));
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsPerfmon,
                BeginExperiments,
                0,
                "Begin running all the perf monitor's experiments")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);

    const char usage[] = "Usage: subdev.Perfmon.BeginExperiments()";

    if(NumArguments != 0)
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsPerfmon *pJsPerfmon;
    if ((pJsPerfmon =
            JS_GET_PRIVATE(JsPerfmon, pContext, pObject, "JsPerfmon")) != 0)
    {
        GpuPerfmon* pPerfmon = pJsPerfmon->GetPerfmonObj();
        RETURN_RC(pPerfmon->BeginExperiments());
    }

    return JS_FALSE;
}

JS_STEST_LWSTOM(JsPerfmon,
                EndExperiments,
                0,
                "Stop running all the perf monitor's experiments")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);

    const char usage[] = "Usage: subdev.Perfmon.EndExperiments()";

    if(NumArguments != 0)
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsPerfmon *pJsPerfmon;
    if ((pJsPerfmon =
            JS_GET_PRIVATE(JsPerfmon, pContext, pObject, "JsPerfmon")) != 0)
    {
        GpuPerfmon* pPerfmon = pJsPerfmon->GetPerfmonObj();
        RETURN_RC(pPerfmon->EndExperiments());
    }

    return JS_FALSE;
}

JS_STEST_LWSTOM(JsPerfmon,
                GetResults,
                2,
                "Fetch the results of a perfmon experiment")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);

    JavaScriptPtr pJavaScript;
    UINT32 ExpIdx;
    JSObject* pRetArray;

    const char usage[] = "Usage: subdev.Perf.GetResults(ExperimentIdx, RetArray)";

    if((NumArguments != 2) ||
       (OK != pJavaScript->FromJsval(pArguments[0], &ExpIdx))||
       (OK != pJavaScript->FromJsval(pArguments[1], &pRetArray)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsPerfmon *pJsPerfmon;
    if ((pJsPerfmon = JS_GET_PRIVATE(JsPerfmon, pContext, pObject, "JsPerfmon")) != 0)
    {
        RC rc;
        const GpuPerfmon::Experiment* pExperiment =
                pJsPerfmon->ExpVecGet(ExpIdx);
        if(pExperiment == NULL)
        {
            return JS_FALSE;
        }

        GpuPerfmon* pPerfmon = pJsPerfmon->GetPerfmonObj();
        GpuPerfmon::Results ExpResults;
        pPerfmon->GetResults(pExperiment, &ExpResults);
        C_CHECK_RC(pJavaScript->SetElement(pRetArray, 0,
                                           ExpResults.Data.CacheResults.HitCount));
        C_CHECK_RC(pJavaScript->SetElement(pRetArray, 1,
                                           ExpResults.Data.CacheResults.MissCount));
        RETURN_RC(rc);
    }

    return JS_FALSE;
}

JS_STEST_LWSTOM(JsPerfmon,
                DestroyExperiment,
                1,
                "Destroy an experiment and remove it from the perfmon")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);

    JavaScriptPtr pJavaScript;
    UINT32 ExpIdx;

    const char usage[] = "Usage: subdev.Perf.DestroyExperiment(ExperimentIdx)";

    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &ExpIdx)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsPerfmon *pJsPerfmon;
    if ((pJsPerfmon = JS_GET_PRIVATE(JsPerfmon, pContext, pObject, "JsPerfmon")) != 0)
    {
        RC rc;

        GpuPerfmon* pPerfmon = pJsPerfmon->GetPerfmonObj();
        const GpuPerfmon::Experiment* pExperiment =
                pJsPerfmon->ExpVecGet(ExpIdx);
        if(pExperiment == NULL)
        {
            return JS_FALSE;
        }
        C_CHECK_RC(pPerfmon->DestroyExperiment(pExperiment));
        C_CHECK_RC(pJsPerfmon->ExpVecSet(ExpIdx, NULL));
        RETURN_RC(rc);
    }

    return JS_FALSE;
}
