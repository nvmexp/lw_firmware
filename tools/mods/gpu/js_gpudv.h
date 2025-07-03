/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2007,2010-2012,2016,2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_JS_GPUDV_H
#define INCLUDED_JS_GPUDV_H

#ifndef INCLUDED_JSAPI_H
#include "jsapi.h"
#define INCLUDED_JSAPI_H
#endif
#ifndef INCLUDED_RC_H
#include "core/include/rc.h"
#endif
#ifndef INCLUDED_STL_STRING
#include <string>
#define INCLUDED_STL_STRING
#endif

class GpuDevice;

class JsGpuDevice
{
public:
    JsGpuDevice();

    void            SetGpuDevice(GpuDevice * pSubdev);
    GpuDevice *     GetGpuDevice();
    RC              CreateJSObject(JSContext *cx, JSObject *obj);
    JSObject *      GetJSObject() {return m_pJsGpuDevObj;}

    // Interfaces to GpuDevice methods
    UINT32          GetDeviceInst();
    UINT32          GetNumSubdevices();
    bool            GetSupportsRenderToSysMem();
    UINT64          GetBigPageSize();
    UINT32          GetRealDisplayClassFamily();
    RC              SetUseRobustChannelCallback(bool val);
    bool            GetUseRobustChannelCallback();
    UINT32          GetFamily();
    RC              AllocFlaVaSpace();

private:
    GpuDevice *     m_pGpuDevice;
    JSObject *      m_pJsGpuDevObj;
};

#endif // INCLUDED_JS_GPUDV_H
