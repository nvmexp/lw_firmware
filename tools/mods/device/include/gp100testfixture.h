/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2016 by LWPU Corporation. All rights reserved. All information
* contained herein is proprietary and confidential to LWPU Corporation. Any
* use, reproduction, or disclosure without the written permission of LWPU
* Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

#ifdef PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#ifndef GP100TESTFIXTURE_H
#define GP100TESTFIXTURE_H

#include <map>
#include <string>
#include <vector>

#include "core/include/rc.h"
#include "core/include/types.h"
#include "core/include/serial.h"

class GP100TestFixture
{
    struct SafeBoolHelper
    {
        int x;
    };
    typedef int SafeBoolHelper::* SafeBool;

public:
    GP100TestFixture();

    bool IsInited() const
    {
        return nullptr != m_Ser;
    }

    operator SafeBool() const
    {
        return IsInited() ? &SafeBoolHelper::x : 0;
    }

    RC GetTemperature(UINT32 *pTempCelsius);
    RC GetTachometer(map<size_t, UINT32> *pFansRPM);

    RC GetIntakeFans(vector<size_t> *pIntakeIdx) const;
    bool IsIntake(size_t fanNum) const;
    RC GetOuttakeFans(vector<size_t> *pOuttakeIdx) const;
    bool IsOuttake(size_t fanNum) const;

    RC SetPWM(UINT32 pwm);
    RC SetClock(UINT32 clockMHz);

private:
    RC ExecCommand(const string &cmd, string *response);

    Serial *m_Ser;
};

#endif
