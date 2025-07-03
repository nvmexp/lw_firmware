/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "js_gpio.h"
#include "core/include/script.h"

//-----------------------------------------------------------------------------
JsGpio::JsGpio(Gpio* pGpio)
: m_pGpio(pGpio)
{
    MASSERT(pGpio);
}


//-----------------------------------------------------------------------------
static void C_JsGpio_finalize
(
    JSContext *cx,
    JSObject *obj
)
{
    MASSERT(cx != 0);
    MASSERT(obj != 0);

    JsGpio * pJsGpio;
    //! Delete the C++
    pJsGpio = (JsGpio *)JS_GetPrivate(cx, obj);
    delete pJsGpio;
};

//-----------------------------------------------------------------------------
static JSClass JsGpio_class =
{
    "Gpio",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ColwertStub,
    C_JsGpio_finalize
};

//-----------------------------------------------------------------------------
static SObject JsGpio_Object
(
    "Gpio",
    JsGpio_class,
    0,
    0,
    "Gpio JS Object"
);

//-----------------------------------------------------------------------------
RC JsGpio::CreateJSObject(JSContext* cx, JSObject* obj)
{
    RC rc;

    //! Only create one JSObject per Perf object
    if (m_pJsGpioObj)
    {
        Printf(Tee::PriLow,
               "A JS Object has already been created for this JsGpio.\n");
        return OK;
    }

    m_pJsGpioObj = JS_DefineObject(cx,
                                   obj,
                                   "Gpio", // Property name
                                   &JsGpio_class,
                                   JsGpio_Object.GetJSObject(),
                                   JSPROP_READONLY);

    if (!m_pJsGpioObj)
        return RC::COULD_NOT_CREATE_JS_OBJECT;

    //! Store the current JsGpio instance into the private area
    //! of the new JSOBject.
    if (JS_SetPrivate(cx, m_pJsGpioObj, this) != JS_TRUE)
    {
        Printf(Tee::PriNormal,
               "Unable to set private value of JsGpio.\n");
        return RC::COULD_NOT_CREATE_JS_OBJECT;
    }

    // Add the .Help() to this JSObject
    JS_DefineFunction(cx, m_pJsGpioObj, "Help", &C_Global_Help, 1, 0);

    return OK;
}

//-----------------------------------------------------------------------------
bool JsGpio::GetDirection(UINT32 pinNum)
{
    return m_pGpio->GetDirection(pinNum);
}

//-----------------------------------------------------------------------------
bool JsGpio::GetInput(UINT32 pinNum)
{
    return m_pGpio->GetInput(pinNum);
}

//-----------------------------------------------------------------------------
bool JsGpio::GetOutput(UINT32 pinNum)
{
    return m_pGpio->GetOutput(pinNum);
}

//-----------------------------------------------------------------------------
RC JsGpio::SetActivityLimit
(
    UINT32    pinNum,
    Gpio::Direction dir,
    UINT32    maxNumOclwrances,
    bool      disableWhenMaxedOut
)
{
    return m_pGpio->SetActivityLimit(pinNum, dir, maxNumOclwrances, disableWhenMaxedOut);
}

//-----------------------------------------------------------------------------
RC JsGpio::SetDirection(UINT32 pinNum, bool toOutput)
{
    return m_pGpio->SetDirection(pinNum, toOutput);
}

//-----------------------------------------------------------------------------
RC JsGpio::SetInterruptNotification(UINT32 pinNum, UINT32 dir, bool toEnable)
{
    return m_pGpio->SetInterruptNotification(pinNum, static_cast<Gpio::Direction>(dir), toEnable);
}

//-----------------------------------------------------------------------------
RC JsGpio::SetOutput(UINT32 pinNum, bool toHigh)
{
    return m_pGpio->SetOutput(pinNum, toHigh);
}

//-----------------------------------------------------------------------------
RC JsGpio::StartErrorCounter()
{
    return m_pGpio->StartErrorCounter();
}

//-----------------------------------------------------------------------------
RC JsGpio::StopErrorCounter()
{
    return m_pGpio->StopErrorCounter();
}

//-----------------------------------------------------------------------------
JS_SMETHOD_BIND_ONE_ARG(JsGpio, GetDirection, pinNum, UINT32, "");
JS_SMETHOD_BIND_ONE_ARG(JsGpio, GetInput,     pinNum, UINT32, "");
JS_SMETHOD_BIND_ONE_ARG(JsGpio, GetOutput,    pinNum, UINT32, "");

JS_STEST_BIND_TWO_ARGS(JsGpio, SetDirection,  pinNum, UINT32, toOutput, bool, "");
JS_STEST_BIND_TWO_ARGS(JsGpio, SetOutput,     pinNum, UINT32, toHigh, bool, "");

JS_STEST_BIND_THREE_ARGS(JsGpio, SetInterruptNotification, pinNum, UINT32, dir, UINT32, toEnable, bool, "");

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpio,
                SetActivityLimit,
                4,
                "Set the Gpio Activity Limit")
{
    STEST_HEADER(4, 4, "Usage: TestDevice.Gpio.SetActivityLimit(pinNum, dir, maxNumOclwrances, bDisableWhenMaxedOut)\n");
    STEST_ARG(0, UINT32, pinNum);
    STEST_ARG(1, UINT32, dirInt);
    STEST_ARG(2, UINT32, maxNumOclwrances);
    STEST_ARG(3, bool, disableWhenMaxedOut);
    STEST_PRIVATE(JsGpio, pJsGpio, "Gpio");

    RC rc;
    Gpio::Direction dir = static_cast<Gpio::Direction>(dirInt);

    C_CHECK_RC(pJsGpio->SetActivityLimit(pinNum, dir, maxNumOclwrances, disableWhenMaxedOut));

    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
//! \brief Gpio constants
JS_CLASS(GpioConst);

static SObject GpioConst_Object
(
    "GpioConst",
    GpioConstClass,
    0,
    0,
    "GpioConst JS Object"
);

PROP_CONST(GpioConst, RISING, Gpio::RISING);
PROP_CONST(GpioConst, FALLING, Gpio::FALLING);
