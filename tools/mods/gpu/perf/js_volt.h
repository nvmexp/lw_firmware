/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once
#ifndef INCLUDED_JS_VOLT_H
#define INCLUDED_JS_VOLT_H

#include "core/include/types.h"

class Volt3x;
struct JSObject;
struct JSContext;
class RC;

class JsVolt3x
{
public:
    JsVolt3x();

    RC          CreateJSObject(JSContext *cx, JSObject *obj);
    JSObject *  GetJSObject();
    void        SetVolt3x(Volt3x *pVolt3x);
    Volt3x *    GetVolt3x();

    RC Initialize();

    RC SetVoltageMv(UINT32 voltDom, UINT32 voltMv);
    RC ClearSetVoltage(UINT32 voltDom);
    RC ClearVoltages();

    RC SetRailLimitOffsetuV(UINT32 voltDom, UINT32 voltLimit, INT32 offsetuV);
    RC ClearRailLimitOffsets();

    RC SetRamAssistOffsetuV(UINT32 railIdx, INT32 vCritLowoffsetuV, INT32 vCritHighoffsetuV);

    static RC VoltRailStatusToJsObject
    (
        const LW2080_CTRL_VOLT_VOLT_RAIL_STATUS &railStatus,
        JSObject *pJsStatus
    );

    static RC VoltPolicyStatusToJsObject
    (
        UINT32 policyIdx,
        const LW2080_CTRL_VOLT_VOLT_POLICY_STATUS& status,
        JSObject* pJsStatus
    );

    RC SetInterVoltSettleTimeUs(UINT16 timeUs);
    bool GetSplitRailConstraintAuto();
    RC SetSplitRailConstraintAuto(bool autoEnable);
    bool GetIsPascalSplitRailSupported();
    bool GetIsRamAssistSupported();

    UINT32 GetVoltRailMask();

private:
    Volt3x *      m_pVolt3x;
    JSObject *  m_pJsVolt3xObject;
};

#endif //INCLUDED_JS_CLKAVFS_H
