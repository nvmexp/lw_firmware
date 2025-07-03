/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "device/interface/gpio.h"

struct JSContext;
struct JSObject;

class JsGpio
{
public:
    JsGpio(Gpio* pGpio);

    RC CreateJSObject(JSContext* cx, JSObject* obj);

    bool GetDirection(UINT32 pinNum);
    bool GetInput(UINT32 pinNum);
    bool GetOutput(UINT32 pinNum);
    RC SetActivityLimit
    (
        UINT32    pinNum,
        Gpio::Direction dir,
        UINT32    maxNumOclwrances,
        bool      disableWhenMaxedOut
    );
    RC SetDirection(UINT32 pinNum, bool toOutput);
    Gpio* GetGpio() { return m_pGpio; }
    void SetGpio(Gpio* pGpio) { m_pGpio = pGpio; }
    RC SetInterruptNotification(UINT32 pinNum, UINT32 dir, bool toEnable);
    RC SetOutput(UINT32 pinNum, bool toHigh);
    RC StartErrorCounter();
    RC StopErrorCounter();

private:
    Gpio* m_pGpio = nullptr;
    JSObject* m_pJsGpioObj = nullptr;
};
