/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "core/include/rc.h"
#include "device/interface/i2c.h"

#include <string>

class GpuSubdevice;
struct JSFunction;

class FuseSource
{
public:
    enum Level
    {
        TO_LOW,
        TO_HIGH,
        ILWALID_LEVEL
    };

    enum BlowMethod
    {
        UNKNOWN_METHOD,
        GPIO,
        IO_EXPANDER,
        JS_FUNC
    };

    FuseSource();
    ~FuseSource();

    RC Initialize(GpuSubdevice* pSubdev,
                  string MethodString);

    RC Toggle(Level Value);

private:
    RC InitializeIOExpander(GpuSubdevice* pSubdev,
                            UINT32        I2CPort,
                            UINT32        I2CAddr);

    RC InitializeGpio(GpuSubdevice* pSubdev, UINT32 GpioNum);
    RC InitializeJsFunc(GpuSubdevice* pSubdev, string JsFuncName);

    GpuSubdevice *m_pSubdev;
    JSFunction   *m_pJsFunc;
    BlowMethod    m_Method;
    UINT32        m_GpioNum;
    unique_ptr<I2c::Dev> m_pI2cDev;

    RC ByIoExpander(Level Value);
    RC ByGpio(Level Value);
    RC ByJsFunc(Level Value);
};
