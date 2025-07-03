/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2007, 2015-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once
#ifndef INCLUDED_JS_POWER_H
#define INCLUDED_JS_POWER_H

#include "core/include/types.h"

class Power;
class RC;
struct JSObject;
struct JSContext;

class JsPower
{
public:
    JsPower();

    RC              CreateJSObject(JSContext* cx, JSObject* obj);
    JSObject*       GetJSObject() {return m_pJsPowerObj;}
    void            SetPowerObj(Power* pFb);
    Power*          GetPowerObj();

    RC PrintChannelsInfo(UINT32 mask);
    RC PrintPoliciesInfo(UINT32 mask);
    RC PrintPoliciesStatus(UINT32 mask);
    RC PrintViolationsInfo(UINT32 mask);
    RC PrintViolationsStatus(UINT32 mask);

    bool GetIsClkUpScaleSupported();

private:
    Power*          m_pPower;
    JSObject *      m_pJsPowerObj;
};

#endif

