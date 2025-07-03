/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/fuse/gpufuse.h"
#include "gpu/include/gpusbdev.h"
#include "gputest.h"
#include "gpu/utility/gpuutils.h"
#include "core/include/utility.h"
#include "core/include/abort.h"
#include "cheetah/include/tegrasocdevice.h"
#include "cheetah/include/tegrasocdeviceptr.h"

#include "gpu/tempctrl/tempctrl.h"

class SetTempCtrl : public GpuTest
{
public:
    SetTempCtrl();
    ~SetTempCtrl();

    virtual RC   Run();
    virtual bool IsSupported();

    /* Just for testing argument handling from gpudecls.js*/
    SETGET_PROP(Id, UINT32);
    SETGET_PROP(UsePercent, bool);
    SETGET_PROP(Val, UINT32);
    SETGET_PROP(Percent, FLOAT32);
    SETGET_PROP(Index, UINT32);

private:
    UINT32 m_Id;
    bool m_UsePercent;
    UINT32 m_Val;
    FLOAT32 m_Percent;
    UINT32 m_Index;
};

//-----------------------------------------------------------------------------

/* This exports the test to the JS interface */
JS_CLASS_INHERIT(SetTempCtrl, GpuTest, "SetTempCtrl");

CLASS_PROP_READWRITE(SetTempCtrl, Id, UINT32,
                     "UINT32: Id of the external ctrl");

CLASS_PROP_READWRITE(SetTempCtrl, UsePercent, bool,
                     "bool: Use specified percent of max value instead of specifying raw value");

CLASS_PROP_READWRITE(SetTempCtrl, Val, UINT32,
                     "UINT32: Value to be set");

CLASS_PROP_READWRITE(SetTempCtrl, Percent, FLOAT32,
                     "FLOAT32: Percentage of max to be set");

CLASS_PROP_READWRITE(SetTempCtrl, Index, UINT32,
                     "UINT32: Index, if there are multiple external ctrls");

//-----------------------------------------------------------------------------

SetTempCtrl::SetTempCtrl()
{
    m_Id = 0;
    m_Index = 0;
    m_UsePercent = false;
    m_Val = 0;
    m_Percent = 0;
    m_Index = 0;
}

SetTempCtrl::~SetTempCtrl()
{
}

RC SetTempCtrl::Run()
{
    RC rc;
    TempCtrlPtr ctrl = GetBoundGpuSubdevice()->GetTempCtrlViaId(m_Id);
    if (ctrl == nullptr)
    {
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }
    if (m_UsePercent)
    {
        CHECK_RC(ctrl->SetTempCtrlPercent(m_Percent, m_Index));
    }
    else
    {
        CHECK_RC(ctrl->SetTempCtrlVal(m_Val, m_Index));
    }
    return rc;
}

bool SetTempCtrl::IsSupported()
{
    return true;
}
