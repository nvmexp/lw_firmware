/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016,2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_JS_REGHAL_H
#define INCLUDED_JS_REGHAL_H

#include "gpu/reghal/reghal.h"
#include "core/include/rc.h"

struct JSContext;
struct JSObject;

class JsRegHal
{
public:
    JsRegHal(RegHal*);
    RC              CreateJSObject(JSContext *cx, JSObject *obj);
    RegHal *        GetRegHalObj();
    JSObject *      GetJSObject() {return m_pJsRegHalObj;}

private:
    JSObject *      m_pJsRegHalObj;
    RegHal *        m_pRegHal;
};

#endif // INCLUDED_JS_REGHAL_H
