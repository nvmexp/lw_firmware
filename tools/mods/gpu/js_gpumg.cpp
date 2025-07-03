/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2012,2016-2017,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/device.h"
#include "gpu/include/gpudev.h"
#include "core/include/mgrmgr.h"
#include "gpu/include/gpumgr.h"
#include "core/include/jscript.h"
#include "core/include/script.h"
#include "core/include/deprecat.h"
#include "jsapi.h"
#include "js_gpudv.h"
#include "js_gpusb.h"
#include "core/include/xp.h"
#include "core/include/rc.h"

//-----------------------------------------------------------------------------
static JSClass JsGpuDevMgr_class =
{
    "GpuDevMgr",
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

//-----------------------------------------------------------------------------
static SObject JsGpuDevMgr_Object
(
    "GpuDevMgr",
    JsGpuDevMgr_class,
    0,
    0,
    "GpuDevMgr JS Object"
);

//-----------------------------------------------------------------------------
// GpuDevMgr JS Methods
JS_STEST_LWSTOM(JsGpuDevMgr,
                ListDevices,
                1,
                "List all the GPU devices")
{
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    const char usage[] = "Usage: GpuDevMgr.ListDevices(Priority)";

    if (NumArguments != 1)
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JavaScriptPtr pJavaScript;
    UINT32 pri;

    if (OK != pJavaScript->FromJsval(pArguments[0], &pri))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    if (DevMgrMgr::d_GraphDevMgr == 0)
    {
        JS_ReportError(pContext,
                       "GpuDevMgr is not initialized.");
        return JS_FALSE;
    }

    GpuDevMgr * pGpuDevMgr = (GpuDevMgr *)(DevMgrMgr::d_GraphDevMgr);
    pGpuDevMgr->ListDevices((Tee::Priority)pri);
    return JS_TRUE;
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(JsGpuDevMgr,
                  GetGpuSubdeviceByGpuIdx,
                  1,
                  "Retrieve a GpuSubdevice with an index number")
{
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    const char usage[] = "Usage: GpuDevMgr.GetGpuSubdeviceByGpuIdx(index)";

    if (NumArguments != 1)
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JavaScriptPtr pJavaScript;
    UINT32 index;

    if (OK != pJavaScript->FromJsval(pArguments[0], &index))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    RC rc;

    if (DevMgrMgr::d_GraphDevMgr == 0)
    {
        JS_ReportError(pContext,
                       "GpuDevMgr is not initialized.");
        return JS_FALSE;
    }

    GpuDevMgr * pGpuDevMgr = (GpuDevMgr *)(DevMgrMgr::d_GraphDevMgr);
    GpuSubdevice * pGpuSubdevice = pGpuDevMgr->GetSubdeviceByGpuInst(index);

    if (!pGpuSubdevice)
    {
        Printf(Tee::PriNormal, "JsGpuDevMgr: No GpuSubdevice at index %d\n", index);
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    // Create a JsGpuDevice object to return to JS
    // Note:  This will be cleaned up by JS Garbage Collector
    JsGpuSubdevice * pJsGpuSubdevice = new JsGpuSubdevice();
    MASSERT(pJsGpuSubdevice);
    pJsGpuSubdevice->SetGpuSubdevice(pGpuSubdevice);
    C_CHECK_RC(pJsGpuSubdevice->CreateJSObject(pContext, pObject));

    if (pJavaScript->ToJsval(pJsGpuSubdevice->GetJSObject(), pReturlwalue) != OK)
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }
    return JS_TRUE;
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(JsGpuDevMgr,
                  IsDeviceIndexValid,
                  1,
                  "Check whether there is a device with the specified index")
{
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    const char usage[] = "Usage: GpuDevMgr.IsDeviceIndexValid(index)";

    if (NumArguments != 1)
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JavaScriptPtr pJavaScript;
    UINT32 index;

    if (OK != pJavaScript->FromJsval(pArguments[0], &index))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    if (DevMgrMgr::d_GraphDevMgr == 0)
    {
        JS_ReportError(pContext,
                       "GpuDevMgr is not initialized.");
        return JS_FALSE;
    }

    GpuDevMgr * pGpuDevMgr = (GpuDevMgr *)(DevMgrMgr::d_GraphDevMgr);
    Device *device;
    if (pJavaScript->ToJsval(OK == pGpuDevMgr->GetDevice(index, &device), pReturlwalue) != OK)
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    return JS_TRUE;
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(JsGpuDevMgr,
                  GetLwrrentGpuSubdevice,
                  0,
                  "Retrieve the current GpuSubdevice")
{
    static Deprecation depr("GpuDevMgr.GetLwrrentGpuSubdevice", "3/30/2019",
                            "There is no longer a concept of a 'current' device\n");
    if (!depr.IsAllowed(pContext))
        return JS_FALSE;

    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    const char usage[] = "Usage: GpuDevMgr.GetLwrrentGpuSubdevice()";

    if (NumArguments != 0)
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JavaScriptPtr pJavaScript;
    RC rc;

    if (DevMgrMgr::d_GraphDevMgr == 0)
    {
        JS_ReportError(pContext,
                       "GpuDevMgr is not initialized.");
        return JS_FALSE;
    }

    GpuDevMgr * pGpuDevMgr = (GpuDevMgr *)(DevMgrMgr::d_GraphDevMgr);
    GpuSubdevice * pGpuSubdevice = pGpuDevMgr->GetFirstGpu();

    if (!pGpuSubdevice)
    {
        Printf(Tee::PriNormal, "JsGpuDevMgr: No current GpuSubdevice\n");
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    // Create a JsGpuSubdevice object to return to JS
    // Note:  This will be cleaned up by JS Garbage Collector
    JsGpuSubdevice * pJsGpuSubdevice = new JsGpuSubdevice();
    MASSERT(pJsGpuSubdevice);
    pJsGpuSubdevice->SetGpuSubdevice(pGpuSubdevice);
    C_CHECK_RC(pJsGpuSubdevice->CreateJSObject(pContext, pObject));

    if (pJavaScript->ToJsval(pJsGpuSubdevice->GetJSObject(), pReturlwalue) != OK)
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }
    return JS_TRUE;
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(JsGpuDevMgr,
                  GetLwrrentGpuDevice,
                  0,
                  "Retrieve the current GpuDevice")
{
    static Deprecation depr("GpuDevMgr.GetLwrrentGpuDevice", "3/30/2019",
                            "The concept of a 'current' Gpu is deprecated. Just returning the first.\n");
    if (!depr.IsAllowed(pContext))
        return JS_FALSE;

    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    const char usage[] = "Usage: GpuDevMgr.GetLwrrentGpuDevice()";

    if (NumArguments != 0)
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JavaScriptPtr pJavaScript;
    RC rc;

    if (DevMgrMgr::d_GraphDevMgr == 0)
    {
        JS_ReportError(pContext,
                       "GpuDevMgr is not initialized.");
        return JS_FALSE;
    }

    GpuDevMgr * pGpuDevMgr = (GpuDevMgr *)(DevMgrMgr::d_GraphDevMgr);
    GpuDevice * pGpuDevice = pGpuDevMgr->GetFirstGpuDevice();
    if (!pGpuDevice)
    {
        Printf(Tee::PriNormal, "JsGpuDevMgr: No current GpuDevice\n");
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    // Create a JsGpuDevice object to return to JS
    // Note:  This will be cleaned up by JS Garbage Collector
    JsGpuDevice * pJsGpuDevice = new JsGpuDevice();
    MASSERT(pJsGpuDevice);
    pJsGpuDevice->SetGpuDevice(pGpuDevice);
    C_CHECK_RC(pJsGpuDevice->CreateJSObject(pContext, pObject));

    if (pJavaScript->ToJsval(pJsGpuDevice->GetJSObject(), pReturlwalue) != OK)
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }
    return JS_TRUE;
}

PROP_READONLY_CHECKNULL(JsGpuDevMgr, ((GpuDevMgr *)(DevMgrMgr::d_GraphDevMgr)),
              IsInitialized, bool, "Return whether the GpuDevMgr is initialized");
PROP_READONLY_CHECKNULL(JsGpuDevMgr, ((GpuDevMgr *)(DevMgrMgr::d_GraphDevMgr)),
              IsPreInitialized, bool, "Return whether the GpuDevMgr has any preinitialized devices");
PROP_READONLY_CHECKNULL(JsGpuDevMgr, ((GpuDevMgr *)(DevMgrMgr::d_GraphDevMgr)),
              NumDevices, UINT32, "Return the number of GpuDevices");
PROP_READONLY_CHECKNULL(JsGpuDevMgr, ((GpuDevMgr *)(DevMgrMgr::d_GraphDevMgr)),
              NumGpus, UINT32, "Return the number of Gpus");
PROP_READWRITE(JsGpuDevMgr, ((GpuDevMgr *)(DevMgrMgr::d_GraphDevMgr)),
               NumUserdAllocs, UINT32, "Number of pre-allocated USERD regions per GPU device");
PROP_READWRITE(JsGpuDevMgr, ((GpuDevMgr *)(DevMgrMgr::d_GraphDevMgr)),
               UserdLocation, UINT32, "USERD location");
PROP_READWRITE(JsGpuDevMgr, ((GpuDevMgr *)(DevMgrMgr::d_GraphDevMgr)), VasReverse, bool,
               "Whether to reverse GPU virual addresses on core mods allocations");

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuDevMgr,
                Link,
                2,
                "Link 2+ gpus into an SLI device")
{
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    const char usage[] = "Usage: GpuDevMgr.Link([gpuInst, gpuInst, ...],dispInst)";

    // Check the arguments.
    UINT32 Gpus[LW0000_CTRL_GPU_MAX_ATTACHED_GPUS];
    UINT32 DispInst;
    JsArray jsa;
    JavaScriptPtr pJs;

    if ((NumArguments != 2) ||
        (OK != pJs->FromJsval(pArguments[0], &jsa)) ||
        (OK != pJs->FromJsval(pArguments[1], &DispInst)) ||
        (jsa.size() < 2) ||
        (jsa.size() > LW0000_CTRL_GPU_MAX_ATTACHED_GPUS))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    for (UINT32 i = 0; i < jsa.size(); i++)
    {
        if (OK != pJs->FromJsval(jsa[i], &Gpus[i]))
        {
            JS_ReportError(pContext, usage);
            return JS_FALSE;
        }
    }

    if (DevMgrMgr::d_GraphDevMgr == 0)
    {
        JS_ReportError(pContext,
                       "GpuDevMgr is not initialized.");
        return JS_FALSE;
    }
    RETURN_RC(((GpuDevMgr *)DevMgrMgr::d_GraphDevMgr)->LinkGpus(UINT32(jsa.size()), Gpus,DispInst));
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuDevMgr,
                Unlink,
                1,
                "Unlink one SLI multi-gpu device into N single-gpu devices")
{
    JavaScriptPtr pJs;

    // Check the arguments.
    UINT32 Dev;
    if (1 != NumArguments
        || (OK != pJs->FromJsval(pArguments[0], &Dev))
        )
    {
        JS_ReportError(pContext, "Usage: GpuDevMgr.Unlink(DeviceInstance)");
        return JS_FALSE;
    }

    if (DevMgrMgr::d_GraphDevMgr == 0)
    {
        JS_ReportError(pContext,
                       "GpuDevMgr is not initialized.");
        return JS_FALSE;
    }

    RETURN_RC(((GpuDevMgr *)DevMgrMgr::d_GraphDevMgr)->UnlinkDevice(Dev));
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuDevMgr,
                GetFirstGpuDevice,
                0,
                "Return a JsGpuDevice representing the first GpuDevice in the system")
{
    JavaScriptPtr pJs;
    const char usage[] = "Usage: GpuDevMgr.GetFirstGpuDevice()";

    // Check the arguments.
    if (0 != NumArguments)
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    if (DevMgrMgr::d_GraphDevMgr == 0)
    {
        JS_ReportError(pContext,
                       "GpuDevMgr is not initialized.");
        return JS_FALSE;
    }

    RC rc;
    GpuDevice * pGpuDev = ((GpuDevMgr *)DevMgrMgr::d_GraphDevMgr)->GetFirstGpuDevice();
    if (!pGpuDev) // No more devices
    {
        if (pJs->ToJsval((JSObject *)NULL, pReturlwalue) != OK)
        {
            JS_ReportError(pContext, usage);
            return JS_FALSE;
        }
    }
    else
    {
        JsGpuDevice * pJsGpuDevice = new JsGpuDevice();
        MASSERT(pJsGpuDevice);
        pJsGpuDevice->SetGpuDevice(pGpuDev);
        C_CHECK_RC(pJsGpuDevice->CreateJSObject(pContext, pObject));

        if (pJs->ToJsval(pJsGpuDevice->GetJSObject(), pReturlwalue) != OK)
        {
            JS_ReportError(pContext, usage);
            return JS_FALSE;
        }
    }
    return JS_TRUE;
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuDevMgr,
                GetNextGpuDevice,
                1,
                "Return the next JsGpuDevice representing in the system")
{
    JavaScriptPtr pJs;
    const char usage[] = "Usage: GpuDevMgr.GetNextGpuDevice(JsGpuDevice)";

    // Check the arguments.
    JSObject * pJsObj;
    if (1 != NumArguments
        || (OK != pJs->FromJsval(pArguments[0], &pJsObj))
       )
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    if (DevMgrMgr::d_GraphDevMgr == 0)
    {
        JS_ReportError(pContext,
                       "GpuDevMgr is not initialized.");
        return JS_FALSE;
    }

    RC rc;
    // First get the JsGpuDevice from the JSObject
    JsGpuDevice *pJsGpuDevice;
    if ((pJsGpuDevice = JS_GET_PRIVATE(JsGpuDevice, pContext, pJsObj,
                                       "GpuDevice")) == 0)
    {
        JS_ReportError(pContext, "Invalid JsGpuDevice\n");
        return JS_FALSE;
    }

    GpuDevice * pGpuDev = ((GpuDevMgr *)DevMgrMgr::d_GraphDevMgr)->
                               GetNextGpuDevice(pJsGpuDevice->GetGpuDevice());
    if (!pGpuDev) // No more devices
    {
        if (pJs->ToJsval((JSObject *)NULL, pReturlwalue) != OK)
        {
            JS_ReportError(pContext, usage);
            return JS_FALSE;
        }
    }
    else
    {
        JsGpuDevice * pJsGpuDevice = new JsGpuDevice();
        MASSERT(pJsGpuDevice);
        pJsGpuDevice->SetGpuDevice(pGpuDev);
        C_CHECK_RC(pJsGpuDevice->CreateJSObject(pContext, pObject));

        if (pJs->ToJsval(pJsGpuDevice->GetJSObject(), pReturlwalue) != OK)
        {
            JS_ReportError(pContext, usage);
            return JS_FALSE;
        }
    }
    return JS_TRUE;
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuDevMgr,
                GetFirstGpu,
                0,
                "Return a JsGpuSubdevice representing the first GpuSubdevice in the system")
{
    JavaScriptPtr pJs;
    const char usage[] = "Usage: GpuDevMgr.GetFirstGpu()";

    // Check the arguments.
    if (0 != NumArguments)
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    if (DevMgrMgr::d_GraphDevMgr == 0)
    {
        JS_ReportError(pContext,
                       "GpuDevMgr is not initialized.");
        return JS_FALSE;
    }

    RC rc;
    GpuDevMgr *pGpuDevMgr = (GpuDevMgr *)DevMgrMgr::d_GraphDevMgr;
    GpuSubdevice * pGpuSubdev = pGpuDevMgr->GetFirstGpu();
    if (!pGpuSubdev) // No more subdevices
    {
        if (pJs->ToJsval((JSObject *)NULL, pReturlwalue) != OK)
        {
            JS_ReportError(pContext, usage);
            return JS_FALSE;
        }
    }
    else
    {
        JsGpuSubdevice * pJsGpuSubdevice = new JsGpuSubdevice();
        MASSERT(pJsGpuSubdevice);
        pJsGpuSubdevice->SetGpuSubdevice(pGpuSubdev);
        C_CHECK_RC(pJsGpuSubdevice->CreateJSObject(pContext, pObject));

        if (pJs->ToJsval(pJsGpuSubdevice->GetJSObject(), pReturlwalue) != OK)
        {
            JS_ReportError(pContext, usage);
            return JS_FALSE;
        }
    }
    return JS_TRUE;
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuDevMgr,
                GetNextGpu,
                1,
                "Return the next JsGpuSubdevice representing in the system")
{
    JavaScriptPtr pJs;
    const char usage[] = "Usage: GpuDevMgr.GetNextGpu(JsGpuSubdevice)";

    // Check the arguments.
    JSObject * pJsObj;
    if (1 != NumArguments
        || (OK != pJs->FromJsval(pArguments[0], &pJsObj))
       )
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    if (DevMgrMgr::d_GraphDevMgr == 0)
    {
        JS_ReportError(pContext,
                       "GpuDevMgr is not initialized.");
        return JS_FALSE;
    }

    RC rc;
    // First get the JsGpuSubdevice from the JSObject
    JsGpuSubdevice *pJsGpuSubdevice;
    if ((pJsGpuSubdevice = JS_GET_PRIVATE(JsGpuSubdevice, pContext, pJsObj,
                                          "GpuSubdevice")) == 0)
    {
        JS_ReportError(pContext, "Invalid JsGpuSubdevice\n");
        return JS_FALSE;
    }

    GpuDevMgr *pGpuDevMgr = (GpuDevMgr *)DevMgrMgr::d_GraphDevMgr;
    GpuSubdevice * pGpuSubdev =
        pGpuDevMgr->GetNextGpu(pJsGpuSubdevice->GetGpuSubdevice());
    if (!pGpuSubdev) // No more devices
    {
        if (pJs->ToJsval((JSObject *)NULL, pReturlwalue) != OK)
        {
            JS_ReportError(pContext, usage);
            return JS_FALSE;
        }
    }
    else
    {
        JsGpuSubdevice * pJsGpuSubdevice = new JsGpuSubdevice();
        MASSERT(pJsGpuSubdevice);
        pJsGpuSubdevice->SetGpuSubdevice(pGpuSubdev);
        C_CHECK_RC(pJsGpuSubdevice->CreateJSObject(pContext, pObject));

        if (pJs->ToJsval(pJsGpuSubdevice->GetJSObject(), pReturlwalue) != OK)
        {
            JS_ReportError(pContext, usage);
            return JS_FALSE;
        }
    }
    return JS_TRUE;
}
