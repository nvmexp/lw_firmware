/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "gpu/tests/gputest.h"
#include "core/include/jscript.h"
#include "core/include/script.h"
#include "jsapi.h"
#include "core/include/rc.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/utility.h"
#include "core/include/deprecat.h"
#include "core/include/js_uint64.h"
static JSClass JSGpuTest_class =
{
    "JSGpuTest",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ColwertStub,
    JS_FinalizeStub
};
static SObject JSGpuTest_Object
(
    "JSGpuTest",
    JSGpuTest_class,
    0,
    0,
    "GpuTest JS Object"
);
CLASS_PROP_READWRITE_LWSTOM_FULL
(
    JSGpuTest,
    ErrorCheckPeriodMS,
    "Error Check Period for CheckExitStackFrame",
    0,
    0
);
P_(JSGpuTest_Get_ErrorCheckPeriodMS)
{
    UINT64 result = GpuTest::GetErrorCheckPeriodMS();
    RC rc = JavaScriptPtr()->ToJsval(result, pValue);
    if (rc != RC::OK)
    {
        JavaScriptPtr()->Throw(pContext, rc, "Failed to get GpuTest.ErrorCheckPeriodMS");
        *pValue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}
P_(JSGpuTest_Set_ErrorCheckPeriodMS)
{
    UINT64 Value;
    RC rc = JavaScriptPtr()->FromJsval(*pValue, &Value);
    if (rc != RC::OK)
    {
        JavaScriptPtr()->Throw(pContext, rc, "Failed to set JSGpuTest.ErrorCheckPeriodMS");
        return JS_FALSE;
    }
    rc = GpuTest::SetErrorCheckPeriodMS(Value);
    if (rc != RC::OK)
    {
        JavaScriptPtr()->Throw(pContext, rc, "Error Setting JSGpuTest.ErrorCheckPeriodMS");
        *pValue = JSVAL_NULL;
        return JS_TRUE;
    }
    return JS_TRUE;
}