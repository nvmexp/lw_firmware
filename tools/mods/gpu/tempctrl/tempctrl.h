/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once
#include "core/include/setget.h"
#include "core/include/rc.h"
#include "core/include/jscript.h"

#include <string>

class TempCtrl
{
public:
    TempCtrl(UINT32 id, string name, UINT32 min, UINT32 max, UINT32 numInsti, string units);
    virtual ~TempCtrl() = default;

    SETGET_PROP(Id, UINT32);
    SETGET_PROP(Name, string);
    SETGET_PROP(Min, UINT32);
    SETGET_PROP(Max, UINT32);
    SETGET_PROP(NumInst, UINT32);
    SETGET_PROP(Units, string);

    RC Initialize();

    RC GetTempCtrlVal(UINT32 *data, UINT32 index = 0);
    RC SetTempCtrlVal(UINT32 data, UINT32 index = 0);
    RC SetTempCtrlPercent(FLOAT32 percent, UINT32 index = 0);

    virtual RC InitGetSetParams(JSObject *pParams)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

protected:
    virtual RC GetVal(UINT32 *data, UINT32 index)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }
    virtual RC SetVal(UINT32 data, UINT32 index)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }
    virtual RC InitTempController()
    {
        return OK;
    }

private:
    // Id the controller is referenced by
    UINT32 m_Id;

    // Name of the controller
    string m_Name;

    // Boundary values
    UINT32 m_Min, m_Max;

    // Number of instances of this controller
    UINT32 m_NumInst;

    // Units of measurement for the controller
    string m_Units;

    bool m_IsInitialized;
};

