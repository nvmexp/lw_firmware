/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
 
#include "js_zpi.h"
#include "core/include/script.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/utility/pcie/pexdev.h"
#include "gpu/js_gpusb.h"

//-----------------------------------------------------------------------------
static JSBool C_JsZpiHandler_constructor
(
    JSContext *cx,
    JSObject *obj,
    uintN argc,
    jsval *argv,
    jsval *rval
)
{
    const char usage[] = "Usage: ZpiHandler()";

    JavaScriptPtr pJs;
    if (argc > 0)
    {
        pJs->Throw(cx, RC::BAD_PARAMETER, usage);
        return JS_FALSE;
    }

    // Add the .Help() to this JSObject
    JS_DefineFunction(cx, obj, "Help", &C_Global_Help, 1, 0);

    JsZpiHandler* pJsZpiHandler = new JsZpiHandler();
    MASSERT(pJsZpiHandler);

    return JS_SetPrivate(cx, obj, pJsZpiHandler);
}

static void C_JsZpiHandler_finalize
(
    JSContext *cx,
    JSObject *obj
)
{
    MASSERT(cx != 0);
    MASSERT(obj != 0);

    JsZpiHandler * pJsZpiHandler;
    //! Delete the C++
    pJsZpiHandler = (JsZpiHandler *)JS_GetPrivate(cx, obj);
    delete pJsZpiHandler;
};

static JSClass JsZpiHandler_class =
{
    "ZpiHandler",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ColwertStub,
    C_JsZpiHandler_finalize
};

static SObject JsZpiHandler_Object
(
    "ZpiHandler",
    JsZpiHandler_class,
    0,
    0,
    "ZpiHandler JS Object",
    C_JsZpiHandler_constructor
);

//-----------------------------------------------------------------------------
RC JsZpiHandler::CreateJSObject(JSContext* cx)
{
    //! Only create one JSObject per ZpiHandler object
    if (m_pJsZpiHandlerObj)
    {
        Printf(Tee::PriLow,
               "A JS Object has already been created for this ZpiHandler.\n");
        return OK;
    }

    m_pJsZpiHandlerObj = JS_NewObject(cx,
                                      &JsZpiHandler_class,
                                      JsZpiHandler_Object.GetJSObject(),
                                      nullptr);

    if (!m_pJsZpiHandlerObj)
        return RC::COULD_NOT_CREATE_JS_OBJECT;

    //! Store the current JsBridge instance into the private area
    //! of the new JSOBject.
    if (JS_SetPrivate(cx, m_pJsZpiHandlerObj, this) != JS_TRUE)
    {
        Printf(Tee::PriNormal,
               "Unable to set private value of ZpiHandler.\n");
        return RC::COULD_NOT_CREATE_JS_OBJECT;
    }

    // Add the .Help() to this JSObject
    JS_DefineFunction(cx, m_pJsZpiHandlerObj, "Help", &C_Global_Help, 1, 0);

    return RC::OK;
}

#define JSZPI_GETSET(name, type, helpmsg)\
RC JsZpiHandler::Set##name(type val)  \
{                                     \
    return m_Zpi.Set##name(val);      \
}                                     \
type JsZpiHandler::Get##name() const  \
{                                     \
    return m_Zpi.Get##name();         \
}                                     \
CLASS_PROP_READWRITE(JsZpiHandler, name, type, helpmsg);

//-----------------------------------------------------------------------------
JSZPI_GETSET(DelayBeforeOOBEntryMs, UINT32, "DelayBeforeOOBEntryMs");
JSZPI_GETSET(DelayAfterOOBExitMs, UINT32, "DelayAfterOOBExitMs");
JSZPI_GETSET(DelayBeforeRescanMs, UINT32, "DelayBeforeRescanMs");
JSZPI_GETSET(RescanRetryDelayMs, UINT32, "RescanRetryDelayMs");
JSZPI_GETSET(IstMode, bool, "IstMode");
JSZPI_GETSET(DelayAfterPexRstAssertMs, UINT32, "DelayAfterPexRstAssertMs");
JSZPI_GETSET(DelayBeforePexDeassertMs, UINT32, "DelayBeforePexDeassertMs");
JSZPI_GETSET(LinkPollTimeoutMs, UINT32, "LinkPollTimeoutMs");
JSZPI_GETSET(DelayAfterExitZpiMs, UINT32, "DelayAfterExitZpiMs");
JSZPI_GETSET(IsSeleneSystem, bool, "IsSeleneSystem");

//-----------------------------------------------------------------------------
RC JsZpiHandler::EnterZPI(UINT32 enterDelayUsec)
{
    return m_Zpi.EnterZPI(enterDelayUsec);
}

//-----------------------------------------------------------------------------
RC JsZpiHandler::ExitZPI()
{
    return m_Zpi.ExitZPI();
}

//-----------------------------------------------------------------------------
RC JsZpiHandler::Cleanup()
{
    return m_Zpi.Cleanup();
}

//-----------------------------------------------------------------------------
JS_STEST_BIND_ONE_ARG(JsZpiHandler, EnterZPI, enterDelayUsec, UINT32, "Enter ZPI on the selected device");
JS_STEST_BIND_NO_ARGS(JsZpiHandler, ExitZPI, "Exit ZPI on the selected device");
JS_STEST_BIND_NO_ARGS(JsZpiHandler, Cleanup, "Cleanup ZPI Handler");

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsZpiHandler, SetupGpu, 1,
                "Setup ZPI handler with GpuSubdevice")
{
    STEST_HEADER(1, 1, "Usage: ZpiHandler.SetupGpu(GpuSubdevice)\n");
    STEST_PRIVATE(JsZpiHandler, pJsZpi, "ZpiHandler");
    STEST_ARG(0, JSObject*, pGpuSubdeviceObject);

    JsGpuSubdevice* pJsGpuSubdevice = nullptr;
    if (NULL == (pJsGpuSubdevice = JS_GET_PRIVATE(JsGpuSubdevice, pContext,
                                                  pGpuSubdeviceObject,
                                                  "GpuSubdevice")))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }
    GpuSubdevice* pGpuSubdevice = pJsGpuSubdevice->GetGpuSubdevice();

    RETURN_RC(pJsZpi->GetZpi().Setup(pGpuSubdevice));
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsZpiHandler, SetupPex, 7,
                "Setup ZPI handler with PexDevice info")
{
    STEST_HEADER(7, 7, "Usage: ZpiHandler.SetupPex(domain, bus, device, function, smbAddress, PexDevice, port)\n");
    STEST_PRIVATE(JsZpiHandler, pJsZpi, "ZpiHandler");
    STEST_ARG(0, UINT32, domain);
    STEST_ARG(1, UINT32, bus);
    STEST_ARG(2, UINT32, device);
    STEST_ARG(3, UINT32, function);
    STEST_ARG(4, UINT08, smbAddress);
    STEST_ARG(5, JSObject*, pPexDevObject);
    STEST_ARG(6, UINT32, port);

    JsPexDev* pJsPexDev = nullptr;
    if (NULL == (pJsPexDev = JS_GET_PRIVATE(JsPexDev, pContext,
                                            pPexDevObject,
                                            "PexDev")))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }
    PexDevice* pPexDev = pJsPexDev->GetPexDev();

    RETURN_RC(pJsZpi->GetZpi().Setup(domain, bus, device, function, smbAddress, pPexDev, port));
}