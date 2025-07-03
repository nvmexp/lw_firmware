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

#include "core/include/jscript.h"
#include "core/include/script.h"
#include "genericipmi.h"

//-----------------------------------------------------------------------------
static JSBool C_IpmiDevice_constructor
(
    JSContext *cx,
    JSObject *obj,
    uintN argc,
    jsval *argv,
    jsval *rval
)
{
    if (argc != 0)
    {
        JavaScriptPtr()->Throw(cx, RC::BAD_PARAMETER, "Usage: IpmiDevice()");
        return JS_FALSE;
    }

    //! Finally tie the IpmiDevice to the new object
    return JS_SetPrivate(cx, obj, new IpmiDevice);
};

//-----------------------------------------------------------------------------
static void C_IpmiDevice_finalize
(
    JSContext *cx,
    JSObject *obj
)
{
    MASSERT(cx != 0);
    MASSERT(obj != 0);

    IpmiDevice * pIpmiDevice;

    //! If a IpmiDevice was associated with this object, make
    //! sure to delete it
    pIpmiDevice = (IpmiDevice *)JS_GetPrivate(cx, obj);
    if (pIpmiDevice)
    {
        delete pIpmiDevice;
    }
};

//-----------------------------------------------------------------------------
static JSClass IpmiDevice_class =
{
    "IpmiDevice",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ColwertStub,
    C_IpmiDevice_finalize
};

//-----------------------------------------------------------------------------
static SObject IpmiDevice_Object
(
    "IpmiDevice",
    IpmiDevice_class,
    0,
    0,
    "IpmiDevice Object",
    C_IpmiDevice_constructor
);

JS_STEST_BIND_NO_ARGS(IpmiDevice, Initialize, "Initialize the IpmiDevice");

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(IpmiDevice,
                WriteSmbus,
                3,
                "Write Smbus")
{
    STEST_HEADER(3, 3, "Usage: IpmiDevice.WriteSmbus(BoardAddr, RegAddr, Values)\n");
    STEST_PRIVATE(IpmiDevice, pIpmiDevice, "IpmiDevice");
    STEST_ARG(0, UINT08, boardAddr);
    STEST_ARG(1, UINT08, regAddr);
    STEST_ARG(2, JsArray, valueArray);
    RC rc;

    vector<UINT08> values;
    for (UINT32 i = 0; i < valueArray.size(); i++)
    {
        UINT08 tempVal;
        C_CHECK_RC(pJavaScript->FromJsval(valueArray[i], &tempVal));
        values.push_back(tempVal);
    }
    C_CHECK_RC(pIpmiDevice->WriteSMBPBI(boardAddr, regAddr, values));
    RETURN_RC(OK);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(IpmiDevice,
                ReadSmbus,
                3,
                "Read ReadSmbus")
{
    STEST_HEADER(3, 3, "Usage: IpmiDevice.ReadSmbus(BoardAddr, RegAddr, ReturnedValues)\n");
    STEST_PRIVATE(IpmiDevice, pIpmiDevice, "IpmiDevice");
    STEST_ARG(0, UINT08, boardAddr);
    STEST_ARG(1, UINT08, regAddr);
    STEST_ARG(2, JSObject *, pArrayToReturn);
    RC rc;

    vector<UINT08> values(4, 0);
    C_CHECK_RC(pIpmiDevice->ReadSMBPBI(boardAddr, regAddr, values));

    for (UINT32 i = 0; i < values.size(); i++)
    {
        C_CHECK_RC(pJavaScript->SetElement(pArrayToReturn, i, values[i]));
    }
    RETURN_RC(OK);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(IpmiDevice,
                RawAccess,
                4,
                "Raw ipmi access")
{
    STEST_HEADER(4, 4, "Usage: IpmiDevice.RawAccess(NetFn, Cmd, SendData, RecvData)\n");
    STEST_PRIVATE(IpmiDevice, pIpmiDevice, "IpmiDevice");
    STEST_ARG(0, UINT08, netFn);
    STEST_ARG(1, UINT08, cmd);
    STEST_ARG(2, JsArray, jsSendData);
    STEST_ARG(3, JSObject *, jsRecvData);
    RC rc;

    vector<UINT08> sendData;
    vector<UINT08> recvData;
    for (UINT32 i = 0; i < jsSendData.size(); i++)
    {
        UINT08 tempVal;
        C_CHECK_RC(pJavaScript->FromJsval(jsSendData[i], &tempVal));
        sendData.push_back(tempVal);
    }

    C_CHECK_RC(pIpmiDevice->RawAccess(netFn, cmd, sendData, recvData));

    for (UINT32 i = 0; i < recvData.size(); i++)
    {
        C_CHECK_RC(pJavaScript->SetElement(jsRecvData, i, recvData[i]));
    }
    RETURN_RC(OK);
}
