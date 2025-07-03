/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2017,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_JS_THERM_H
#define INCLUDED_JS_THERM_H

#include "jsapi.h"
#include "core/include/rc.h"
#include "gpu/perf/thermsub.h"
#include <string>


class JsThermal
{
public:
    JsThermal();

    RC              CreateJSObject(JSContext *cx, JSObject *obj);
    JSObject *      GetJSObject() {return m_pJsThermalObj;}
    void            SetThermal(Thermal * pFb);
    Thermal *       GetThermal();

    RC              PrintSensors(UINT32 pri);
    static RC       ChannelInfoToJsObj(UINT32 chIdx,
                                       const ThermalSensors::ChannelInfo& chInfo,
                                       JSObject* pJsObj);

private:
    Thermal *       m_pThermal;
    JSObject *      m_pJsThermalObj;
};

#endif // INCLUDED_JS_Therm_H

