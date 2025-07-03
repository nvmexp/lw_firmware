/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/js_uint64.h"
#include "core/include/script.h"
#include "core/include/utility.h"
#include <cerrno>

static JSBool C_JsUINT64_constructor
(
    JSContext *cx,
    JSObject *obj,
    uintN argc,
    jsval *argv,
    jsval *rval
)
{
    const char usage[] = "Usage: UINT64(MostSignificantBits, LeastSignificantBits) or "
                         "UINT64(LeastSignificantBits) or UINT64(string)";

    JavaScriptPtr pJs;
    if ((argc == 0) ||
        ((argc > 1) && JSVAL_IS_STRING(argv[0])) ||
        (argc > 2))
    {
        pJs->Throw(cx, RC::BAD_PARAMETER, usage);
        return JS_FALSE;
    }

    UINT32 msb = 0, lsb = 0;
    UINT64 value = 0;

    if (JSVAL_IS_STRING(argv[0]))
    {
        string strVal;
        if (OK != pJs->FromJsval(argv[0], &strVal))
        {
            pJs->Throw(cx, RC::BAD_PARAMETER, usage);
            return JS_FALSE;
        }
        value = Utility::Strtoull(strVal.c_str(), nullptr, 0);
        if (errno == ERANGE)
        {
            pJs->Throw(cx, RC::BAD_PARAMETER, usage);
            return JS_FALSE;
        }
    }
    else
    {
        if (OK != pJs->FromJsval(argv[0], &msb) ||
            ((argc > 1) &&
             OK != pJs->FromJsval(argv[1], &lsb)))
        {
            pJs->Throw(cx, RC::BAD_PARAMETER, usage);
            return JS_FALSE;
        }

        if (argc == 1)
        {
            // only lsb was specified
            lsb = msb;
            msb = 0;
        }
        value = (static_cast<UINT64>(msb) << 32) | static_cast<UINT64>(lsb);
    }

    // Add the .Help() to this JSObject
    JS_DefineFunction(cx, obj, "Help", &C_Global_Help, 1, 0);

    JsUINT64* pJsUINT64 = new JsUINT64();
    MASSERT(pJsUINT64);
    pJsUINT64->SetValue(value);

    return JS_SetPrivate(cx, obj, pJsUINT64);
}

static void C_JsUINT64_finalize
(
    JSContext *cx,
    JSObject *obj
)
{
    MASSERT(cx != 0);
    MASSERT(obj != 0);

    JsUINT64 * pJsUINT64;
    //! Delete the C++
    pJsUINT64 = (JsUINT64 *)JS_GetPrivate(cx, obj);
    delete pJsUINT64;
};

static JSClass JsUINT64_class =
{
    "UINT64",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ColwertStub,
    C_JsUINT64_finalize
};

static SObject JsUINT64_Object
(
    "UINT64",
    JsUINT64_class,
    0,
    0,
    "UINT64 JS Object",
    C_JsUINT64_constructor
);

//-----------------------------------------------------------------------------
RC JsUINT64::CreateJSObject(JSContext* cx)
{
    //! Only create one JSObject per UINT64 object
    if (m_pJsUINT64Obj)
    {
        Printf(Tee::PriLow,
               "A JS Object has already been created for this UINT64.\n");
        return OK;
    }

    m_pJsUINT64Obj = JS_NewObject(cx,
                                 &JsUINT64_class,
                                 JsUINT64_Object.GetJSObject(),
                                 nullptr);

    if (!m_pJsUINT64Obj)
        return RC::COULD_NOT_CREATE_JS_OBJECT;

    //! Store the current JsBridge instance into the private area
    //! of the new JSOBject.
    if (JS_SetPrivate(cx, m_pJsUINT64Obj, this) != JS_TRUE)
    {
        Printf(Tee::PriNormal,
               "Unable to set private value of JsPexDev.\n");
        return RC::COULD_NOT_CREATE_JS_OBJECT;
    }

    // Add the .Help() to this JSObject
    JS_DefineFunction(cx, m_pJsUINT64Obj, "Help", &C_Global_Help, 1, 0);

    return OK;
}

//-----------------------------------------------------------------------------
UINT32 JsUINT64::GetLsb() const
{
    return m_Value & 0xFFFFFFFF;
}

//-----------------------------------------------------------------------------
UINT32 JsUINT64::GetMsb() const
{
    return (m_Value >> 32) & 0xFFFFFFFF;
}

//-----------------------------------------------------------------------------
RC JsUINT64::SetLsb(UINT32 lsb)
{
    m_Value = (static_cast<UINT64>(GetMsb()) << 32) | static_cast<UINT64>(lsb);
    return OK;
}

//-----------------------------------------------------------------------------
RC JsUINT64::SetMsb(UINT32 msb)
{
    m_Value = (static_cast<UINT64>(msb) << 32) | static_cast<UINT64>(GetLsb());
    return OK;
}

//-----------------------------------------------------------------------------
CLASS_PROP_READWRITE(JsUINT64, Lsb, UINT32, "The least significant 32 bits");
CLASS_PROP_READWRITE(JsUINT64, Msb, UINT32, "The most significant 32 bits");

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM
(
    JsUINT64,
    GetDecStr,
    0,
    "Get the decimal string representation"
)
{
    STEST_HEADER(0, 0, "Usage: UINT64.GetDecStr()");
    STEST_PRIVATE(JsUINT64, pJsUINT64, "UINT64");

    string str = Utility::StrPrintf("%llu", pJsUINT64->GetValue());

    if (OK != pJavaScript->ToJsval(str, pReturlwalue))
    {
        return JS_FALSE;
    }

    return JS_TRUE;
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM
(
    JsUINT64,
    GetHexStr,
    0,
    "Get the hexidecimal string representation"
)
{
    STEST_HEADER(0, 0, "Usage: UINT64.GetDecStr()");
    STEST_PRIVATE(JsUINT64, pJsUINT64, "UINT64");

    string str = Utility::StrPrintf("0x%llx", pJsUINT64->GetValue());

    if (OK != pJavaScript->ToJsval(str, pReturlwalue))
    {
        return JS_FALSE;
    }

    return JS_TRUE;
}
