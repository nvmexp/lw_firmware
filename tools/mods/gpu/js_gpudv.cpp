/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2012,2016-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "js_gpudv.h"
#include "js_gpusb.h"
#include "gpu/display/js_disp.h"
#include "gpu/include/gpudev.h"
#include "core/include/mgrmgr.h"
#include "core/include/display.h"
#include "gpu/include/gpumgr.h"
#include "core/include/jscript.h"
#include "core/include/script.h"
#include "jsapi.h"
#include "core/include/xp.h"
#include "core/include/rc.h"

JsGpuDevice::JsGpuDevice()
    : m_pGpuDevice(NULL), m_pJsGpuDevObj(NULL)
{
}

void JsGpuDevice::SetGpuDevice(GpuDevice * pGpudev)
{
    MASSERT(pGpudev);
    m_pGpuDevice = pGpudev;
}

//-----------------------------------------------------------------------------
//! \brief Constructor for a GpuDevice object.
//!
//! This ctor takes in one argument that determines which Gpu device
//! the user wants access to.  It then creates a JsGpuDevice wrapper
//! and associates it with the given GpuDevice.
//!
//! \note This function is only called from JS when a GpuDevice object
//! is created in JS (i.e. sb = new GpuDevice(0)).
static JSBool C_JsGpuDevice_constructor
(
    JSContext *cx,
    JSObject *obj,
    uintN argc,
    jsval *argv,
    jsval *rval
)
{
    const char usage[] = "Usage: GpuDevice(GpuDeviceInst)";

    if (argc != 1)
    {
        JS_ReportError(cx, usage);
        return JS_FALSE;
    }

    JavaScriptPtr pJs;
    UINT32       gpuDevInst;
    Device       *pGpuDev = 0;

    if ((OK != pJs->FromJsval(argv[0], &gpuDevInst)))
    {
        JS_ReportError(cx, usage);
        return JS_FALSE;
    }

    if (!DevMgrMgr::d_GraphDevMgr)
    {
        Printf(Tee::PriHigh, "JsGpuDevice constructor called "
                             "before Gpu.Initialize\n");
        return JS_FALSE;
    }

    //! Try and retrieve the specified GpuDevice
    if (DevMgrMgr::d_GraphDevMgr->GetDevice(gpuDevInst, &pGpuDev) != OK)
    {
        JS_ReportWarning(cx, "Invalid Gpu device index");
        JS_ReportError(cx, usage);
        return JS_FALSE;
    }

    // Add the .Help() to this JSObject
    JS_DefineFunction(cx, obj, "Help", &C_Global_Help, 1, 0);

    //! Create a JsGpuDevice wrapper and associate it with the
    //! given GpuDevice.
    JsGpuDevice * pJsGpuDevice = new JsGpuDevice();
    MASSERT(pJsGpuDevice);
    pJsGpuDevice->SetGpuDevice((GpuDevice *)pGpuDev);

    // For each new JsGpuDevice, create a JsDisplay object
    // to associate with it
    JsDisplay * pJsDisplay = new JsDisplay();
    MASSERT(pJsDisplay);
    pJsDisplay->SetDisplay(((GpuDevice *)pGpuDev)->GetDisplay());
    pJsDisplay->CreateJSObject(cx, obj);

    //! Finally tie the JsGpuDevice wrapper to the new object
    return JS_SetPrivate(cx, obj, pJsGpuDevice);
};

//-----------------------------------------------------------------------------
static void C_JsGpuDevice_finalize
(
    JSContext *cx,
    JSObject *obj
)
{
    MASSERT(cx != 0);
    MASSERT(obj != 0);

    JsGpuDevice * pJsGpuDevice;

    //! If a JsGpuDevice was associated with this object, make
    //! sure to delete it
    pJsGpuDevice = (JsGpuDevice *)JS_GetPrivate(cx, obj);
    if (pJsGpuDevice)
    {
        delete pJsGpuDevice;
    }
};

//-----------------------------------------------------------------------------
static JSClass JsGpuDevice_class =
{
    "GpuDevice",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ColwertStub,
    C_JsGpuDevice_finalize
};

//-----------------------------------------------------------------------------
static SObject JsGpuDevice_Object
(
    "GpuDevice",
    JsGpuDevice_class,
    0,
    0,
    "GpuDevice JS Object",
    C_JsGpuDevice_constructor
);

//------------------------------------------------------------------------------
//! \brief Create a JS Object representation of the current associated
//! GpuDevice.
//!
//! \note The JSOBjects created will be freed by the JS GC.
//! \note The JSObject is prototyped from JsGpuDevice_Object
//! \sa SetGpuDevice
RC JsGpuDevice::CreateJSObject(JSContext *cx, JSObject *obj)
{
    RC rc;

    //! Only create one JSObject per GpuDevice
    if (m_pJsGpuDevObj)
    {
        Printf(Tee::PriLow,
               "A JS Object has already been created for this JsGpuDevice.\n");
        return OK;
    }

    m_pJsGpuDevObj = JS_DefineObject(cx,
                                     obj, // GpuTest object
                                     "BoundGpuDevice", // Property name
                                     &JsGpuDevice_class,
                                     JsGpuDevice_Object.GetJSObject(),
                                     JSPROP_READONLY);

    if (!m_pJsGpuDevObj)
        return RC::COULD_NOT_CREATE_JS_OBJECT;

    //! Store the current JsGpuDevice instance into the private area
    //! of the new JSOBject.  This will tie the two together.
    if (JS_SetPrivate(cx, m_pJsGpuDevObj, this) != JS_TRUE)
    {
        Printf(Tee::PriNormal,
               "Unable to set private value of JsGpuDevice.\n");
        return RC::COULD_NOT_CREATE_JS_OBJECT;
    }

    // Add the .Help() to this JSObject
    JS_DefineFunction(cx, m_pJsGpuDevObj, "Help", &C_Global_Help, 1, 0);

    // Create the display JS Object
    // For each new JsGpuDevice, create a JsDisplay object
    // to associate with it
    JsDisplay * pJsDisplay = new JsDisplay();
    MASSERT(pJsDisplay);
    pJsDisplay->SetDisplay(GetGpuDevice()->GetDisplay());
    CHECK_RC(pJsDisplay->CreateJSObject(cx, m_pJsGpuDevObj));

    return OK;
}

//------------------------------------------------------------------------------
//! \brief Return the associated GpuDevice.
//!
//! Return the GpuDevice that is being wrapped by this JsGpuDevice.  We
//! should never have a NULL pointer to a GpuDevice when calling this
//! function.
GpuDevice * JsGpuDevice::GetGpuDevice()
{
    MASSERT(m_pGpuDevice);

    return m_pGpuDevice;
}

//------------------------------------------------------------------------------
UINT32 JsGpuDevice::GetDeviceInst()
{
    return GetGpuDevice()->GetDeviceInst();
}

//------------------------------------------------------------------------------
UINT32 JsGpuDevice::GetNumSubdevices()
{
    return GetGpuDevice()->GetNumSubdevices();
}

//------------------------------------------------------------------------------
bool JsGpuDevice::GetSupportsRenderToSysMem()
{
    return GetGpuDevice()->SupportsRenderToSysMem();
}

//------------------------------------------------------------------------------
UINT64 JsGpuDevice::GetBigPageSize()
{
    return GetGpuDevice()->GetBigPageSize();
}

//------------------------------------------------------------------------------
UINT32 JsGpuDevice::GetRealDisplayClassFamily()
{
    return GetGpuDevice()->GetDisplay()->GetDisplayClassFamily();
}

//------------------------------------------------------------------------------
RC JsGpuDevice::SetUseRobustChannelCallback(bool val)
{
    return GetGpuDevice()->SetUseRobustChannelCallback(val);
}

//------------------------------------------------------------------------------
bool JsGpuDevice::GetUseRobustChannelCallback()
{
    return GetGpuDevice()->GetUseRobustChannelCallback();
}
//------------------------------------------------------------------------------
UINT32 JsGpuDevice::GetFamily()
{
    return GetGpuDevice()->GetFamily();
}
//------------------------------------------------------------------------------
RC JsGpuDevice::AllocFlaVaSpace()
{
    return GetGpuDevice()->AllocFlaVaSpace();
}

//-----------------------------------------------------------------------------
// GpuDevice JS Properties
CLASS_PROP_READONLY(JsGpuDevice, DeviceInst, UINT32,
                    "Return the resman device instance number");
CLASS_PROP_READONLY(JsGpuDevice, NumSubdevices, UINT32,
                    "Number of subdevices for this GPU device.");
CLASS_PROP_READONLY(JsGpuDevice, SupportsRenderToSysMem, bool,
                    "Determines if rendering to system memory is supported");
CLASS_PROP_READONLY(JsGpuDevice, BigPageSize, UINT64,
                    "Return the big page size");
CLASS_PROP_READONLY(JsGpuDevice, RealDisplayClassFamily, UINT32,
                    "Family of the device's real display engine class.");
CLASS_PROP_READWRITE
(
    JsGpuDevice, UseRobustChannelCallback, bool,
    "Use a callback for robust-channel errors (aka two-stage recovery"
);
CLASS_PROP_READONLY(JsGpuDevice, Family, UINT32,
                    "Family of the device's 3d graphics class (i.e. Gpu.Fermi).");

JS_STEST_BIND_NO_ARGS(JsGpuDevice, AllocFlaVaSpace, "Allocate the FLA VASpace on the GPU");

//-----------------------------------------------------------------------------
// GpuDevice JS Methods
JS_SMETHOD_LWSTOM(JsGpuDevice,
                  CheckCapsBit,
                  1,
                  "Check the value of a given CapsBit")
{
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    const char usage[] = "Usage: GpuDevice.CheckCapsBit(bit)";

    if (NumArguments != 1)
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JavaScriptPtr pJavaScript;
    UINT32 bit;

    if (OK != pJavaScript->FromJsval(pArguments[0], &bit))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsGpuDevice *pJsGpuDevice;
    if ((pJsGpuDevice = JS_GET_PRIVATE(JsGpuDevice, pContext, pObject, "GpuDevice")) != 0)
    {
        pJavaScript->ToJsval(
            pJsGpuDevice->GetGpuDevice()->CheckCapsBit((GpuDevice::CapsBits)bit),
            pReturlwalue);
        return JS_TRUE;
    }
    return JS_FALSE;
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(JsGpuDevice,
                  HasBug,
                  1,
                  "Check to see if the GpuDevice has the given bug number")
{
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    const char usage[] = "Usage: GpuDevice.HasBug(bug)";

    if (NumArguments != 1)
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JavaScriptPtr pJavaScript;
    UINT32 bug;

    if (OK != pJavaScript->FromJsval(pArguments[0], &bug))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsGpuDevice *pJsGpuDevice;
    if ((pJsGpuDevice = JS_GET_PRIVATE(JsGpuDevice, pContext, pObject, "GpuDevice")) != 0)
    {
        pJavaScript->ToJsval(pJsGpuDevice->GetGpuDevice()->HasBug(bug),
                             pReturlwalue);
        return JS_TRUE;
    }
    return JS_FALSE;
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuDevice,
                PrintCaps,
                0,
                "Print all the cap values")
{
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    const char usage[] = "Usage: GpuDevice.PrintCaps()";

    if (NumArguments != 0)
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsGpuDevice *pJsGpuDevice;
    if ((pJsGpuDevice = JS_GET_PRIVATE(JsGpuDevice, pContext, pObject, "GpuDevice")) != 0)
    {
        pJsGpuDevice->GetGpuDevice()->PrintCaps();
        RETURN_RC(OK);
    }
    return JS_FALSE;
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuDevice,
                GetGpuSubdevice,
                1,
                "Return a JsGpuSubDevice at the given index")
{
    JavaScriptPtr pJs;
    const char usage[] = "Usage: JsGpuDevice.GetGpuSubdevice(index)";

    // Check the arguments.
    UINT32 index;
    if (1 != NumArguments
        || (OK != pJs->FromJsval(pArguments[0], &index))
       )
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    RC rc;
    JsGpuDevice *pJsGpuDevice;
    if ((pJsGpuDevice = JS_GET_PRIVATE(JsGpuDevice, pContext, pObject,
                                       "GpuDevice")) != 0)
    {
        GpuDevice * pGpuDev = pJsGpuDevice->GetGpuDevice();
        GpuSubdevice * pSubdev = pGpuDev->GetSubdevice(index);

        if (!pSubdev)
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
            pJsGpuSubdevice->SetGpuSubdevice(pSubdev);
            C_CHECK_RC(pJsGpuSubdevice->CreateJSObject(pContext, pObject));

            if (pJs->ToJsval(pJsGpuSubdevice->GetJSObject(), pReturlwalue) != OK)
            {
                JS_ReportError(pContext, usage);
                return JS_FALSE;
            }
        }
        return JS_TRUE;
    }
    return JS_FALSE;
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuDevice,
                SetContextOverride,
                3,
                "Override initial ctxswitched register value.")
{
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);
    JavaScriptPtr pJs;
    const char usage[] = "Usage: GpuDevice.SetContextOverride(regAddr, andMask, orMask)";

    UINT32 regAddr;
    UINT32 andMask;
    UINT32 orMask;

    if ((NumArguments != 3) ||
        (OK != pJs->FromJsval(pArguments[0], &regAddr)) ||
        (OK != pJs->FromJsval(pArguments[1], &andMask)) ||
        (OK != pJs->FromJsval(pArguments[2], &orMask )))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsGpuDevice *pJsGpuDevice;
    if ((pJsGpuDevice = JS_GET_PRIVATE(JsGpuDevice, pContext, pObject, "GpuDevice")) != 0)
    {
        RETURN_RC(pJsGpuDevice->GetGpuDevice()->SetContextOverride(
                regAddr, andMask, orMask));
    }
    return JS_FALSE;
}
//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsGpuDevice,
                SetupSimulatedEdids,
                0,
                "Setup simulated Edids.")
{
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);
    const char usage[] = "Usage: GpuDevice.SetupSimulatedEdids()";

    if (NumArguments != 0)
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsGpuDevice *pJsGpuDevice;
    if ((pJsGpuDevice = JS_GET_PRIVATE(JsGpuDevice, pContext, pObject, "GpuDevice")) != 0)
    {
        pJsGpuDevice->GetGpuDevice()->SetupSimulatedEdids();
        RETURN_RC(OK);
    }
    return JS_FALSE;
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(JsGpuDevice,
                  GetHdcpEn,
                  1,
                  "Get Hdcp enable fuse bit")
{
    STEST_HEADER(1, 1, "Usage: GpuDevice.GetHdcpEn(En)\n");
    STEST_PRIVATE(JsGpuDevice, pJsGpuDevice, "GpuDevice");
    STEST_ARG(0, JSObject*, pRetVal);

    RC rc;
    GpuDevice* pDev = pJsGpuDevice->GetGpuDevice();
    UINT32 en;

    C_CHECK_RC(pDev->GetDisplay()->GetHdcpEnable(&en));
    RETURN_RC(pJavaScript->SetElement(pRetVal, 0, en));
}

