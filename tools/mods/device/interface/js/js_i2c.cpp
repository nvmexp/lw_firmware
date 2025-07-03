/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "js_i2c.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/deprecat.h"
#include "core/include/mgrmgr.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/js_gpusb.h"

//-----------------------------------------------------------------------------
JsI2c::JsI2c(I2c* pI2c)
: m_pI2c(pI2c)
{
    MASSERT(m_pI2c);
}

//-----------------------------------------------------------------------------
static void C_JsI2c_finalize
(
    JSContext *cx,
    JSObject *obj
)
{
    MASSERT(cx != 0);
    MASSERT(obj != 0);

    JsI2c * pJsI2c;
    //! Delete the C++
    pJsI2c = (JsI2c *)JS_GetPrivate(cx, obj);
    delete pJsI2c;
};

//-----------------------------------------------------------------------------
static JSClass JsI2c_class =
{
    "I2C",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ColwertStub,
    C_JsI2c_finalize
};

//-----------------------------------------------------------------------------
static SObject JsI2c_Object
(
    "I2C",
    JsI2c_class,
    0,
    0,
    "I2c JS Object"
);

//-----------------------------------------------------------------------------
RC JsI2c::CreateJSObject(JSContext* cx, JSObject* obj)
{
    //! Only create one JSObject per Perf object
    if (m_pJsI2cObj)
    {
        Printf(Tee::PriLow,
               "A JS Object has already been created for this JsI2C.\n");
        return OK;
    }

    m_pJsI2cObj = JS_DefineObject(cx,
                                  obj, // device object
                                  "I2C", // Property name
                                  &JsI2c_class,
                                  JsI2c_Object.GetJSObject(),
                                  JSPROP_READONLY);

    if (!m_pJsI2cObj)
        return RC::COULD_NOT_CREATE_JS_OBJECT;

    //! Store the current JsI2c instance into the private area
    //! of the new JSOBject.
    if (JS_SetPrivate(cx, m_pJsI2cObj, this) != JS_TRUE)
    {
        Printf(Tee::PriNormal,
               "Unable to set private value of JsI2C.\n");
        return RC::COULD_NOT_CREATE_JS_OBJECT;
    }

    // Add the .Help() to this JSObject
    JS_DefineFunction(cx, m_pJsI2cObj, "Help", &C_Global_Help, 1, 0);

    return OK;
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(JsI2c, I2cDev, 2,
                  "Create a new I2cDev object with the specified port and address")
{
    STEST_HEADER(2, 2, "Usage: testdevice.I2C.I2cDev(port, addr);\n");
    STEST_PRIVATE(JsI2c, pJsI2c, "I2C");
    STEST_ARG(0, UINT32, port);
    STEST_ARG(1, UINT32, addr);

    I2c::Dev i2cDev = pJsI2c->GetI2c()->I2cDev(port, addr);
    JsI2cDev* pJsI2cDev = new JsI2cDev(i2cDev);
    if (OK != pJsI2cDev->CreateJSObject(pContext, pObject)
        || OK != pJavaScript->ToJsval(pJsI2cDev->GetJSObject(), pReturlwalue))
    {
        JS_ReportError(pContext,
                       "Error oclwrred in I2C.I2cDev()");
        return JS_FALSE;
    }
    return JS_TRUE;
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsI2c, Find, 1,
                "Find all I2C devices on this device")
{
    STEST_HEADER(0, 1, "Usage: testdevice.I2C.Find([retArray]);\n");
    STEST_PRIVATE(JsI2c, pJsI2c, "I2C");
    STEST_OPTIONAL_ARG(0, JSObject*, pRetArray, nullptr);

    RC rc;

    vector<I2c::Dev> devices;
    C_CHECK_RC(pJsI2c->GetI2c()->Find(&devices));

    if (pRetArray)
    {
        for (UINT32 i = 0; i < devices.size(); i++)
        {
            JsI2cDev* pJsI2cDev = new JsI2cDev(devices[i]);
            C_CHECK_RC(pJsI2cDev->CreateJSObject(pContext, pObject));
            jsval i2cDevVal;
            C_CHECK_RC(pJavaScript->ToJsval(pJsI2cDev->GetJSObject(), &i2cDevVal));
            C_CHECK_RC(pJavaScript->SetElement(pRetArray, i, i2cDevVal));
        }
    }
    else
    {
        for (const auto& dev : devices)
        {
            Printf(Tee::PriNormal,
                   "Found I2C Device at Port 0x%02x Address 0x%02x\n",
                   dev.GetPort(), dev.GetAddr());
        }
    }

    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
// JsI2cDev
//-----------------------------------------------------------------------------
static JSBool C_JsI2cDev_constructor
(
    JSContext *cx,
    JSObject *obj,
    uintN argc,
    jsval *argv,
    jsval *rval
)
{
    static Deprecation depr("I2cDev", "11/15/2018",
                            "Creating an I2cDev on its own is deprecated. Use testDevice.I2C.I2cDev(port,addr) instead.\n");
    if (!depr.IsAllowed(cx))
        return JS_FALSE;

    const char usage[] =
        "Usage: I2cDev(GpuSubdevice, Port, addr)\n"
        "Usage: I2cDev(GpuInstNum, port, addr)";
    JavaScriptPtr pJs;
    GpuSubdevice *pGpuSubdevice;
    UINT32 port = 0;
    UINT32 addr = 0;

    if ((argc != 3) ||
        (OK != pJs->FromJsval(argv[1], &port)) ||
        (OK != pJs->FromJsval(argv[2], &addr)))
    {
        JS_ReportError(cx, usage);
        return JS_FALSE;
    }

    // Colwert first arg into a pGpuSubdevice
    //
    if (JSVAL_IS_NUMBER(argv[0]))
    {
        UINT32 GpuInst;
        GpuDevMgr* pGpuDevMgr = (GpuDevMgr*)(DevMgrMgr::d_GraphDevMgr);
        if (pGpuDevMgr == NULL)
        {
            JS_ReportError(cx,
                           "I2cDev must be created after GpuMgr is setup\n");
            return JS_FALSE;
        }
        if (OK != pJs->FromJsval(argv[0], &GpuInst))
        {
            JS_ReportError(cx, usage);
            return JS_FALSE;
        }
        pGpuSubdevice = pGpuDevMgr->GetSubdeviceByGpuInst(GpuInst);
    }
    else
    {
        JSObject *pGpuSubdeviceObject = NULL;
        JsGpuSubdevice *pJsGpuSubdevice = NULL;
        if ( (OK != pJs->FromJsval(argv[0], &pGpuSubdeviceObject)) ||
             (NULL == (pJsGpuSubdevice = JS_GET_PRIVATE(JsGpuSubdevice, cx,
                                                        pGpuSubdeviceObject,
                                                        "GpuSubdevice"))) )
        {
            JS_ReportError(cx, usage);
            return JS_FALSE;
        }
        pGpuSubdevice = pJsGpuSubdevice->GetGpuSubdevice();
    }
    if (pGpuSubdevice == NULL)
    {
        JS_ReportError(cx, usage);
        return JS_FALSE;
    }

    // Create the private data
    //
    I2c::Dev i2cDev = pGpuSubdevice->GetInterface<I2c>()->I2cDev(port, addr);
    JsI2cDev* pJsI2cDev = new JsI2cDev(i2cDev);
    MASSERT(pJsI2cDev);

    // Add the .Help() to this JSObject
    JS_DefineFunction(cx, obj, "Help", &C_Global_Help, 1, 0);

    //! Finally tie the JsGpuSubdevice wrapper to the new object
    return JS_SetPrivate(cx, obj, pJsI2cDev);
};

//-----------------------------------------------------------------------------
static void C_JsI2cDev_finalize
(
    JSContext *cx,
    JSObject *obj
)
{
    MASSERT(cx != 0);
    MASSERT(obj != 0);

    JsI2cDev *pI2cDev = (JsI2cDev *)JS_GetPrivate(cx, obj);
    if (pI2cDev)
    {
        delete pI2cDev;
    }
};

//-----------------------------------------------------------------------------
static JSClass JsI2cDev_class =
{
    "I2cDev",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ColwertStub,
    C_JsI2cDev_finalize
};

//-----------------------------------------------------------------------------
static SObject JsI2cDev_Object
(
    "I2cDev",
    JsI2cDev_class,
    0,
    0,
    "I2cDev JS Object",
    C_JsI2cDev_constructor
);

//-----------------------------------------------------------------------------
JsI2cDev::JsI2cDev(I2c::Dev& dev)
: m_I2cDev(dev)
{
}

//-----------------------------------------------------------------------------
RC JsI2cDev::CreateJSObject(JSContext* cx, JSObject* obj)
{
    //! Only create one JSObject per Perf object
    if (m_pJsI2cDevObj)
    {
        Printf(Tee::PriLow,
               "A JS Object has already been created for this JsI2cDev.\n");
        return OK;
    }

    m_pJsI2cDevObj = JS_NewObject(cx,
                                  &JsI2cDev_class,
                                  JsI2cDev_Object.GetJSObject(),
                                  NULL);

    if (!m_pJsI2cDevObj)
        return RC::COULD_NOT_CREATE_JS_OBJECT;

    //! Store the current JsI2cDev instance into the private area
    //! of the new JSOBject.
    if (JS_SetPrivate(cx, m_pJsI2cDevObj, this) != JS_TRUE)
    {
        Printf(Tee::PriNormal,
               "Unable to set private value of JsI2cDev.\n");
        return RC::COULD_NOT_CREATE_JS_OBJECT;
    }

    // Add the .Help() to this JSObject
    JS_DefineFunction(cx, m_pJsI2cDevObj, "Help", &C_Global_Help, 1, 0);

    return OK;
}

//-----------------------------------------------------------------------------
RC JsI2cDev::Dump(UINT32 beginOffset, UINT32 endOffset)
{
    return m_I2cDev.Dump(beginOffset, endOffset);
}

//-----------------------------------------------------------------------------
RC JsI2cDev::PrintSpec()
{
    m_I2cDev.PrintSpec(Tee::PriNormal);
    return OK;
}

//-----------------------------------------------------------------------------
RC JsI2cDev::SetSpec(UINT32 deviceType, UINT32 printPri)
{
    return m_I2cDev.SetSpec(static_cast<I2c::I2cDevice>(deviceType),
                            static_cast<Tee::Priority>(printPri));
}

#define JSI2CDEV_SETGET(name, type, helpmsg)\
RC JsI2cDev::Set##name(type val)            \
{                                           \
    return m_I2cDev.Set##name(val);         \
}                                           \
type JsI2cDev::Get##name() const            \
{                                           \
    return m_I2cDev.Get##name();            \
}                                           \
CLASS_PROP_READWRITE(JsI2cDev, name, type, helpmsg);

JSI2CDEV_SETGET(Port, UINT32, "I2C port number (zero-based)");
JSI2CDEV_SETGET(Addr, UINT32, "I2C device address");
JSI2CDEV_SETGET(OffsetLength, UINT32, "Length (in bytes) of the offset");
JSI2CDEV_SETGET(MessageLength, UINT32, "The default length (in bytes) of the data packet");
JSI2CDEV_SETGET(MsbFirst, bool, "Whether Msb data bits are set first");
JSI2CDEV_SETGET(WriteCmd, UINT08, "Byte to send at the start of I2C writes");
JSI2CDEV_SETGET(ReadCmd, UINT08, "Byte to send at the start of I2C reads");
JSI2CDEV_SETGET(VerifyReads, bool, "If true, read data twice andcompare");
JSI2CDEV_SETGET(SpeedInKHz, UINT32, "I2C clock speed in KHz");
JSI2CDEV_SETGET(AddrMode, UINT32, "Number of bits in the address (7 or 10)");
JSI2CDEV_SETGET(Flavor, UINT32, "What kind of I2C to use (SW or HW)");

JS_STEST_BIND_TWO_ARGS(JsI2cDev, Dump, beginOffset, UINT32, endOffset, UINT32, "Print a table of data read from the I2C device");
JS_STEST_BIND_NO_ARGS(JsI2cDev, PrintSpec, "Print the current spec");
JS_STEST_BIND_TWO_ARGS(JsI2cDev, SetSpec, deviceType, UINT32, printPri, UINT32, "Set the I2C parameters using a known device spec");

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsI2cDev, Write, 2,
                "Do I2C write based on specified parameters")
{
    STEST_HEADER(2, 2, "Usage: I2cDev.Write(offset, data)\n");
    STEST_PRIVATE(JsI2cDev, pJsI2cDev, "I2cDev");
    STEST_ARG(0, UINT32, offset);
    STEST_ARG(1, UINT32, data);

    RETURN_RC(pJsI2cDev->GetI2cDev().Write(offset, data));
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsI2cDev, Read, 2,
                "Do I2C read based on specified parameters")
{
    STEST_HEADER(1, 2, "Usage: I2cDev.Read(offset, [retVal])\n");
    STEST_PRIVATE(JsI2cDev, pJsI2cDev, "I2cDev");
    STEST_ARG(0, UINT32, offset);
    STEST_OPTIONAL_ARG(1, JSObject*, pRetval, nullptr);

    RC rc;

    UINT32 data = 0;
    C_CHECK_RC(pJsI2cDev->GetI2cDev().Read(offset, &data));

    if (pRetval)
    {
        C_CHECK_RC(pJavaScript->SetElement(pRetval, 0, data));
    }
    else
    {
        Printf(Tee::PriNormal, "I2C Read Data = 0x%02x\n", data);
    }

    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsI2cDev, WriteArray, 2,
                "Do a series of I2C writes based on specified parameters")
{
    STEST_HEADER(2, 2, "Usage: I2cDev.WriteArray(offset, inputArray)\n");
    STEST_PRIVATE(JsI2cDev, pJsI2cDev, "I2cDev");
    STEST_ARG(0, UINT32, offset);
    STEST_ARG(1, JsArray, inputArray);

    RC rc;

    vector<UINT32> dataArray;
    for (auto jsData : inputArray)
    {
        UINT32 data = 0;
        C_CHECK_RC(pJavaScript->FromJsval(jsData, &data));
        dataArray.push_back(data);
    }

    C_CHECK_RC(pJsI2cDev->GetI2cDev().WriteArray(offset, dataArray));
    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsI2cDev, ReadArray, 3,
                "Do I2C read based on specified parameters")
{
    STEST_HEADER(3, 3, "Usage: I2cDev.ReadArray(offset, numReads, retArray)\n");
    STEST_PRIVATE(JsI2cDev, pJsI2cDev, "I2cDev");
    STEST_ARG(0, UINT32, offset);
    STEST_ARG(1, UINT32, numReads);
    STEST_ARG(2, JSObject*, pRetArray);

    RC rc;

    vector<UINT32> dataArray;
    C_CHECK_RC(pJsI2cDev->GetI2cDev().ReadArray(offset, numReads, &dataArray));

    for (UINT32 i = 0; i < dataArray.size(); i++)
    {
        C_CHECK_RC(pJavaScript->SetElement(pRetArray, i, dataArray[i]));
    }

    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsI2cDev, WriteBlk, 3,
                "Do I2C block write")
{
    STEST_HEADER(3, 3, "Usage: I2cDev.WriteBlk(offset, numBytes, inputArray)");
    STEST_PRIVATE(JsI2cDev, pJsI2cDev, "I2cDev");
    STEST_ARG(0, UINT32,  offset);
    STEST_ARG(1, UINT32,  numBytes);
    STEST_ARG(2, JsArray, inputArray);

    RC rc;

    vector<UINT08> dataArray;
    for (UINT32 i = 0; i < numBytes; i++)
    {
        UINT08 data = 0;
        C_CHECK_RC(pJavaScript->FromJsval(inputArray[i], &data));
        dataArray.push_back(data);
    }

    C_CHECK_RC(pJsI2cDev->GetI2cDev().WriteBlk(offset, numBytes, dataArray));
    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsI2cDev, ReadBlk, 3,
                "Do I2C block read")
{
    STEST_HEADER(3, 3, "Usage: I2cDev.ReadBlk(offset, numBytes, retArray)");
    STEST_PRIVATE(JsI2cDev, pJsI2cDev, "I2cDev");
    STEST_ARG(0, UINT32, offset);
    STEST_ARG(1, UINT32, numBytes);
    STEST_ARG(2, JSObject*, pRetArray);

    RC rc;

    vector<UINT08> dataArray(numBytes);
    C_CHECK_RC(pJsI2cDev->GetI2cDev().ReadBlk(offset, numBytes, &dataArray));
    for (UINT32 i = 0; i < numBytes; i++)
    {
        C_CHECK_RC(pJavaScript->SetElement(pRetArray, i, dataArray[i]));
    }
    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsI2cDev, WriteSequence, 3,
                "Do I2C sequence write")
{
    STEST_HEADER(3, 3, "Usage: I2cDev.WriteSequence(offset, numBytes, inputArray)");
    STEST_PRIVATE(JsI2cDev, pJsI2cDev, "I2cDev");
    STEST_ARG(0, UINT32,  offset);
    STEST_ARG(1, UINT32,  numBytes);
    STEST_ARG(2, JsArray, inputArray);

    RC rc;

    vector<UINT08> dataArray;
    for (UINT32 i = 0; i < numBytes; i++)
    {
        UINT08 data = 0;
        C_CHECK_RC(pJavaScript->FromJsval(inputArray[i], &data));
        dataArray.push_back(data);
    }

    C_CHECK_RC(pJsI2cDev->GetI2cDev().WriteSequence(offset, numBytes, dataArray));
    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsI2cDev, ReadSequence, 3,
                "Do I2C sequence read")
{
    STEST_HEADER(3, 3, "Usage: I2cDev.ReadSequence(offset, numBytes, retArray)");
    STEST_PRIVATE(JsI2cDev, pJsI2cDev, "I2cDev");
    STEST_ARG(0, UINT32, offset);
    STEST_ARG(1, UINT32, numBytes);
    STEST_ARG(2, JSObject*, pRetArray);

    RC rc;

    vector<UINT08> dataArray(numBytes);
    C_CHECK_RC(pJsI2cDev->GetI2cDev().ReadSequence(offset, numBytes, &dataArray));
    for (UINT32 i = 0; i < numBytes; i++)
    {
        C_CHECK_RC(pJavaScript->SetElement(pRetArray, i, dataArray[i]));
    }
    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsI2cDev, Find, 1,
                "Find all I2C devices on this device")
{
    STEST_HEADER(0, 1, "Usage: I2cDev.Find([retArray]);\n");
    STEST_PRIVATE(JsI2cDev, pJsI2cDev, "I2cDev");
    STEST_OPTIONAL_ARG(0, JSObject*, pRetArray, nullptr);

    static Deprecation depr("I2cDev.Find", "11/15/2018",
                            "I2cDev.Find() is deprecated. Use testDevice.I2C.Find() instead.\n");
    if (!depr.IsAllowed(pContext))
        return JS_FALSE;

    RC rc;

    vector<I2c::Dev> devices;
    C_CHECK_RC(pJsI2cDev->GetI2cDev().GetI2c()->Find(&devices));

    if (pRetArray)
    {
        for (UINT32 i = 0; i < devices.size(); i++)
        {
            JsI2cDev* pJsI2cDev = new JsI2cDev(devices[i]);
            C_CHECK_RC(pJsI2cDev->CreateJSObject(pContext, pObject));
            jsval i2cDevVal;
            C_CHECK_RC(pJavaScript->ToJsval(pJsI2cDev->GetJSObject(), &i2cDevVal));
            C_CHECK_RC(pJavaScript->SetElement(pRetArray, i, i2cDevVal));
        }
    }
    else
    {
        for (const auto& dev : devices)
        {
            Printf(Tee::PriNormal,
                   "Found I2C Device at Port 0x%02x Address 0x%02x\n",
                   dev.GetPort(), dev.GetAddr());
        }
    }

    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
JS_CLASS(I2cDevConst);
static SObject I2cDevConst_Object
(
    "I2cDevConst",
    I2cDevConstClass,
    0,
    0,
    "I2cDevConst JS Object"
);
PROP_CONST(I2cDevConst, INA219, I2c::I2C_INA219);
PROP_CONST(I2cDevConst, INA3221, I2c::I2C_INA3221);
PROP_CONST(I2cDevConst, ADT7473, I2c::I2C_ADT7473);
PROP_CONST(I2cDevConst, ADS1112, I2c::I2C_ADS1112);
PROP_CONST(I2cDevConst, NUM_PORTS, I2c::NUM_PORTS);
PROP_CONST(I2cDevConst, DYNAMIC_PORT, I2c::DYNAMIC_PORT);
PROP_CONST(I2cDevConst, FLAVOR_HW, I2c::FLAVOR_HW);
PROP_CONST(I2cDevConst, FLAVOR_SW, I2c::FLAVOR_SW);
