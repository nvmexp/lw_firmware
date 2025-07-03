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

#include "device/interface/xusbhostctrl.h"

struct JSContext;
struct JSObject;

class JsXusbHostCtrl
{
public:
    JsXusbHostCtrl(XusbHostCtrl* pXusbHostCtrl);

    RC GetAspmState(UINT32* pState);
    RC SetAspmState(UINT32 state);

    RC CreateJSObject(JSContext* cx, JSObject* obj);

private:
    XusbHostCtrl* m_pXusbHostCtrl = nullptr;
    JSObject* m_pJsXusbHostCtrlObj = nullptr;
};
