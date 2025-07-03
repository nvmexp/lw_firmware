/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007,2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/lwrm.h"
#include "core/include/gpu.h"
#include "core/include/jscript.h"
#include "core/include/script.h"
#include "core/include/mgrmgr.h"
#include "core/include/deprecat.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/gpudev.h"
#include "gpu/js_gpudv.h"
#include "ctrl/ctrl0000.h"

//! This class exports the graphics/host/FB caps accesses from GpuDevice to JS.
//!
//! Creating an instance of this class in JS bounds it to the current device,
//! so even if the next device is then chosen, the previous instantiation of
//! this wrapper will still point to the original data.
//! This is kind of a misnomer because it can also export host and FB caps.
class GraphicsCaps
{
public:
    GraphicsCaps(GpuDevice* pGpuDevice);

    GpuDevice *GetDevice();

private:
    GpuDevice *m_GpuDevice;
};

GraphicsCaps::GraphicsCaps(GpuDevice* pGpuDevice)
: m_GpuDevice(pGpuDevice)
{
}

GpuDevice *GraphicsCaps::GetDevice()
{
    return m_GpuDevice;
}

static JSBool C_GraphicsCaps_constructor
(
    JSContext *cx,
    JSObject *obj,
    uintN argc,
    jsval *argv,
    jsval *rval
)
{
    JavaScriptPtr pJs;
    const char usage[] = "Usage: GraphicsCaps(GpuDevice)";

    GpuDevice* pGpuDevice = nullptr;
    if (argc == 0)
    {
        static Deprecation depr("GraphicsCaps", "3/30/2019",
                                "Must specify a GpuDevice argument to the GraphicsCaps constructor\n");
        if (!depr.IsAllowed(cx))
            return JS_FALSE;

        GpuDevMgr* pGpuDevMgr = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr);
        if (pGpuDevMgr)
        {
            pGpuDevice = pGpuDevMgr->GetFirstGpuDevice();
        }
        else
        {
            JS_ReportError(cx, "Failed to initialize GraphicsCaps object, GPU must be init!\n");
            return JS_FALSE;
        }
    }
    else
    {
        JSObject* devObj = nullptr;
        if ((OK != pJs->FromJsval(argv[0], &devObj)))
        {
            JS_ReportError(cx, usage);
            return JS_FALSE;
        }

        JsGpuDevice* pJsGpuDev = nullptr;
        if ((pJsGpuDev = JS_GET_PRIVATE(JsGpuDevice, cx, devObj, "GpuDevice")) == nullptr)
        {
            JS_ReportError(cx, usage);
            return JS_FALSE;
        }

        pGpuDevice = pJsGpuDev->GetGpuDevice();
    }

    GraphicsCaps* pClass = new GraphicsCaps(pGpuDevice);
    MASSERT(pClass);

    return JS_SetPrivate(cx, obj, pClass);
}
static void C_GraphicsCaps_finalize
(
    JSContext *cx,
    JSObject *obj
)
{
    void *pvClass = JS_GetPrivate(cx, obj);
    if (pvClass)
    {
        GraphicsCaps * pClass = (GraphicsCaps *)pvClass;
        JavaScriptPtr pJs;
        pJs->DeferredDelete(pClass);
    }
}
static JSClass GraphicsCaps_class =
{
    "GraphicsCaps",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ColwertStub,
    C_GraphicsCaps_finalize
};
static SObject GraphicsCaps_Object
(
    "GraphicsCaps",
    GraphicsCaps_class,
    0,
    0,
    "Utility class to check graphics/host/FB caps from JavaScript",
    C_GraphicsCaps_constructor
);

PROP_CONST(GraphicsCaps, AA_LINES, GpuDevice::AA_LINES);
PROP_CONST(GraphicsCaps, AA_POLYS, GpuDevice::AA_POLYS);
PROP_CONST(GraphicsCaps, LOGIC_OPS, GpuDevice::LOGIC_OPS);
PROP_CONST(GraphicsCaps, TWOSIDED_LIGHTING, GpuDevice::TWOSIDED_LIGHTING);
PROP_CONST(GraphicsCaps, QUADRO_GENERIC, GpuDevice::QUADRO_GENERIC);
PROP_CONST(GraphicsCaps, TITAN, GpuDevice::TITAN);

C_(GraphicsCaps_PrintCaps)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    GraphicsCaps *pobj = (GraphicsCaps *)JS_GetPrivate(pContext, pObject);
    pobj->GetDevice()->PrintCaps();

    return JS_TRUE;
}

C_(GraphicsCaps_GetProp)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    if (NumArguments != 1)
    {
        JS_ReportError(pContext, "Usage: GraphicsCaps.GetProp(PROP)");
        return JS_FALSE;
    }

    UINT32 bit;
    if (OK != JavaScriptPtr()->FromJsval(pArguments[0], &bit))
    {
        JS_ReportError(pContext, "Cannot colwert argument FromJsval");
        return JS_FALSE;
    }

    if ((GpuDevice::CapsBits)bit == GpuDevice::ILWALID_CAP)
    {
        Printf(Tee::PriHigh,
               "CAP BIT not exported by GraphicsCaps, check your spelling!\n");
        return JS_FALSE;
    }

    GraphicsCaps *pobj = (GraphicsCaps *)JS_GetPrivate(pContext, pObject);

    bool cap = pobj->GetDevice()->CheckCapsBit((GpuDevice::CapsBits)bit);

    if (OK != JavaScriptPtr()->ToJsval(cap, pReturlwalue))
    {
        JS_ReportError(pContext, "Error oclwrred in GraphicsCaps.GetProp()");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

static SMethod GraphicsCaps_PrintCaps
(
    GraphicsCaps_Object,
    "PrintCaps",
    C_GraphicsCaps_PrintCaps,
    0,
    "Print out the graphics caps."
);

static SMethod GraphicsCaps_GetProp
(
    GraphicsCaps_Object,
    "GetProp",
    C_GraphicsCaps_GetProp,
    1,
    "Usage: GetProp(PROPERTY)"
);

//! This function has nothing to do with caps, but I'm sneaking it in here
//! anyway.  If more ConfigGet issues come up in JS then I'll create a
//! separate class to handle such RmControl requests.
//! Used by mods_eng.js
C_(GraphicsCaps_PrintProcType)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    LW0000_CTRL_SYSTEM_GET_CPU_INFO_PARAMS Params = {0};
    if (OK != LwRmPtr()->Control(LwRmPtr()->GetClientHandle(),
                                 LW0000_CTRL_CMD_SYSTEM_GET_CPU_INFO,
                                 &Params, sizeof(Params)))
    {
        Printf(Tee::PriHigh, "Unable to retrieve CPU info.\n");
        return JS_FALSE;
    }

    Printf(Tee::PriHigh, "PROCESSOR_TYPE 0x%06x%02x\n",
           (UINT32)Params.capabilities, (UINT32)Params.type);

    return JS_TRUE;
}

static SMethod GraphicsCaps_PrintProcType
(
    GraphicsCaps_Object,
    "PrintProcType",
    C_GraphicsCaps_PrintProcType,
    0,
    "Print out the processor type."
);
