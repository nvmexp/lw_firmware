/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_JS_PMU_H
#define INCLUDED_JS_PMU_H

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

class PMU;

class JsPmu
{
public:
    JsPmu();

    RC         CreateJSObject(JSContext *cx, JSObject *obj);
    JSObject * GetJSObject() {return m_pJsPmuObj;}
    void       SetPmuObj(PMU * pPmu);
    PMU *      GetPmuObj();

private:
    PMU *          m_pPmu;
    JSObject *     m_pJsPmuObj;
};

#endif // INCLUDED_JS_PMU_H

