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

#pragma once

#include "core/include/rc.h"

class Gpio
{
public:
    enum Direction
    {
        FALLING = 0,
        RISING  = (1<<31)
    };

    virtual ~Gpio() = default;

    bool GetDirection(UINT32 pinNum)
        { return DoGetDirection(pinNum); }
    bool GetInput(UINT32 pinNum)
        { return DoGetInput(pinNum); }
    bool GetOutput(UINT32 pinNum)
        { return DoGetOutput(pinNum); }
    RC IncrementCounter(UINT32 key)
        { return DoIncrementCounter(key); }
    RC SetActivityLimit
    (
        UINT32    pinNum,
        Direction dir,
        UINT32    maxNumOclwrances,
        bool      disableWhenMaxedOut
    )
        { return DoSetActivityLimit(pinNum, dir, maxNumOclwrances, disableWhenMaxedOut); }
    RC SetDirection(UINT32 pinNum, bool toOutput)
        { return DoSetDirection(pinNum, toOutput); }
    RC SetInterruptNotification(UINT32 pinNum, Direction dir, bool toEnable)
        { return DoSetInterruptNotification(pinNum, dir, toEnable); }
    RC SetOutput(UINT32 pinNum, bool toHigh)
        { return DoSetOutput(pinNum, toHigh); }
    RC Shutdown()
        { return DoShutdown(); }
    RC StartErrorCounter()
        { return DoStartErrorCounter(); }
    RC StopErrorCounter()
        { return DoStopErrorCounter(); }

private:
    virtual bool DoGetDirection(UINT32 pinNum) = 0;
    virtual bool DoGetInput(UINT32 pinNum) = 0;
    virtual bool DoGetOutput(UINT32 pinNum) = 0;
    virtual RC DoIncrementCounter(UINT32 key) = 0;
    virtual RC DoSetActivityLimit
    (
        UINT32    pinNum,
        Direction dir,
        UINT32    maxNumOclwrances,
        bool      disableWhenMaxedOut
    ) = 0;
    virtual RC DoSetDirection(UINT32 pinNum, bool toOutput) = 0;
    virtual RC DoSetInterruptNotification(UINT32 pinNum, Direction dir, bool toEnable) = 0;
    virtual RC DoSetOutput(UINT32 pinNum, bool toHigh) = 0;
    virtual RC DoShutdown() = 0;
    virtual RC DoStartErrorCounter() = 0;
    virtual RC DoStopErrorCounter() = 0;
};
