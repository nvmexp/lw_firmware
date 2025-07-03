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

#include "device/interface/i2c.h"

struct JSContext;
struct JSObject;

class JsI2c
{
public:
    JsI2c(I2c* pI2c);

    RC CreateJSObject(JSContext* cx, JSObject* obj);

    I2c* GetI2c() { return m_pI2c; }

private:
    I2c* m_pI2c = nullptr;
    JSObject* m_pJsI2cObj = nullptr;
};

class JsI2cDev
{
public:
    JsI2cDev(I2c::Dev& dev);

    RC CreateJSObject(JSContext* cx, JSObject* obj);

    I2c::Dev& GetI2cDev() { return m_I2cDev; }
    JSObject* GetJSObject() { return m_pJsI2cDevObj; }

    RC Dump(UINT32 beginOffset, UINT32 endOffset);
    RC PrintSpec();
    RC SetSpec(UINT32 deviceType, UINT32 printPri);

    SETGET_PROP_LWSTOM(Port, UINT32);
    SETGET_PROP_LWSTOM(Addr, UINT32);
    SETGET_PROP_LWSTOM(OffsetLength, UINT32);
    SETGET_PROP_LWSTOM(MessageLength, UINT32);
    SETGET_PROP_LWSTOM(MsbFirst, bool);
    SETGET_PROP_LWSTOM(WriteCmd, UINT08);
    SETGET_PROP_LWSTOM(ReadCmd, UINT08);
    SETGET_PROP_LWSTOM(VerifyReads, bool);
    SETGET_PROP_LWSTOM(SpeedInKHz, UINT32);
    SETGET_PROP_LWSTOM(AddrMode, UINT32);
    SETGET_PROP_LWSTOM(Flavor, UINT32);

private:
    I2c::Dev m_I2cDev;
    JSObject* m_pJsI2cDevObj = nullptr;
};
