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

#include "device/interface/js/js_xusbhostctrl.h"

#include "core/include/jscript.h"
#include "core/include/script.h"

//-----------------------------------------------------------------------------
JsXusbHostCtrl::JsXusbHostCtrl(XusbHostCtrl* pXusbHostCtrl)
: m_pXusbHostCtrl(pXusbHostCtrl)
{
    MASSERT(m_pXusbHostCtrl);
}

//-----------------------------------------------------------------------------
static void C_JsXusbHostCtrl_finalize
(
    JSContext *cx,
    JSObject *obj
)
{
    MASSERT(cx != 0);
    MASSERT(obj != 0);

    JsXusbHostCtrl * pJsXusbHostCtrl;
    //! Delete the C++
    pJsXusbHostCtrl = (JsXusbHostCtrl *)JS_GetPrivate(cx, obj);
    delete pJsXusbHostCtrl;
};

//-----------------------------------------------------------------------------
static JSClass JsXusbHostCtrl_class =
{
    "XusbHostCtrl",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ColwertStub,
    C_JsXusbHostCtrl_finalize
};

//-----------------------------------------------------------------------------
static SObject JsXusbHostCtrl_Object
(
    "XusbHostCtrl",
    JsXusbHostCtrl_class,
    0,
    0,
    "XusbHostCtrl JS Object"
);

//-----------------------------------------------------------------------------
RC JsXusbHostCtrl::CreateJSObject(JSContext* cx, JSObject* obj)
{
    //! Only create one JSObject per Perf object
    if (m_pJsXusbHostCtrlObj)
    {
        Printf(Tee::PriLow,
               "A JS Object has already been created for this JsXusbHostCtrl.\n");
        return OK;
    }

    m_pJsXusbHostCtrlObj = JS_DefineObject(cx,
                                           obj,              // device object
                                           "XusbHostCtrl",   // Property name
                                           &JsXusbHostCtrl_class,
                                           JsXusbHostCtrl_Object.GetJSObject(),
                                           JSPROP_READONLY);

    if (!m_pJsXusbHostCtrlObj)
        return RC::COULD_NOT_CREATE_JS_OBJECT;

    //! Store the current JsXusbHostCtrl instance into the private area
    //! of the new JSOBject.
    if (JS_SetPrivate(cx, m_pJsXusbHostCtrlObj, this) != JS_TRUE)
    {
        Printf(Tee::PriNormal,
               "Unable to set private value of JsXusbHostCtrl.\n");
        return RC::COULD_NOT_CREATE_JS_OBJECT;
    }

    // Add the .Help() to this JSObject
    JS_DefineFunction(cx, m_pJsXusbHostCtrlObj, "Help", &C_Global_Help, 1, 0);

    return OK;
}

//-----------------------------------------------------------------------------
RC JsXusbHostCtrl::GetAspmState(UINT32* pState)
{
    return m_pXusbHostCtrl->GetAspmState(pState);
}

RC JsXusbHostCtrl::SetAspmState(UINT32 state)
{
    return m_pXusbHostCtrl->SetAspmState(state);
}

//-----------------------------------------------------------------------------
CLASS_PROP_READONLY(JsXusbHostCtrl, AspmState, UINT32, "Aspm state");

JS_STEST_BIND_ONE_ARG(JsXusbHostCtrl, SetAspmState, state, UINT32, "Set ASPM state");
