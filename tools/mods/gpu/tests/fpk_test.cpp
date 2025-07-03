/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2010,2016-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "fpk_test.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/tee.h"
#include "core/include/xp.h"

//------------------------------------------------------------------------------
FPickerTest::FPickerTest()
{
    SetName("FPickerTest");
    CreateFancyPickerArray(FPK_FPK_NUM_PICKERS);
}

//------------------------------------------------------------------------------
FPickerTest::~FPickerTest()
{
}

//------------------------------------------------------------------------------
RC FPickerTest::Setup()
{
    return OK;
}

//------------------------------------------------------------------------------
RC FPickerTest::Run()
{
    StickyRC finalRc;

    const char * const pickerNames[FPK_FPK_NUM_PICKERS] =
    {
        "FPK_FPK_RANDOM"
        ,"FPK_FPK_RANDOM_F"
        ,"FPK_FPK_SHUFFLE"
        ,"FPK_FPK_STEP"
        ,"FPK_FPK_LIST"
        ,"FPK_FPK_JS"
    };

    for (UINT32 i = 0; i < FPK_FPK_NUM_PICKERS; i++)
    {
        Callbacks * pcb = GetCallbacks(i);
        CallbackArguments args;

        RC rc = pcb->Fire(Callbacks::STOP_ON_ERROR, args);
        if (OK != rc)
        {
            Printf(Tee::PriHigh, "%s validation failed for %d %s.\n",
                    __FUNCTION__, i, pickerNames[i]);
        }
        finalRc = rc;
    }
    return finalRc;
}

//------------------------------------------------------------------------------
RC FPickerTest::Cleanup()
{
    return OK;
}

//-----------------------------------------------------------------------------
/* virtual */ RC FPickerTest::SetDefaultsForPicker(UINT32 idx)
{
    return (*GetFancyPickerArray())[idx].ConfigConst(0);
}

//-----------------------------------------------------------------------------
/* virtual */ bool FPickerTest::IsSupported()
{
    return true;
}

//------------------------------------------------------------------------------
Callbacks *FPickerTest::GetCallbacks(UINT32 id)
{
    if (id >= FPK_FPK_NUM_PICKERS)
    {
        MASSERT(!"invalid callback idx");
        return NULL;
    }
    return &m_ValidateCallbacks[id];
}

//------------------------------------------------------------------------------
RC FPickerTest::PickAll()
{
    for (int i = 0; i < FPK_FPK_NUM_PICKERS; i++)
    {
        FancyPicker & fpk = (*GetFancyPickerArray())[i];
        if (FancyPicker::T_INT == fpk.Type())
            fpk.Pick();
        else
            fpk.FPick();
    }
    GetFpContext()->LoopNum++;
    return OK;
}

//------------------------------------------------------------------------------
RC FPickerTest::Restart()
{
    GetFpContext()->RestartNum++;
    GetFpContext()->LoopNum = 0;
    GetFpContext()->Rand.SeedRandom(GetTestConfiguration()->Seed());
    GetFancyPickerArray()->Restart();
    return OK;
}

//------------------------------------------------------------------------------
// JS linkage
//------------------------------------------------------------------------------

// inherit (in JS as done in C++) the class object FPickerTest from GpuTest
JS_CLASS_INHERIT(FPickerTest, GpuTest, "Fancy picker SW regression test");

JS_STEST_BIND_NO_ARGS
(
    FPickerTest,
    Restart,
    "Restart FancyPickers"
);
JS_STEST_BIND_NO_ARGS
(
    FPickerTest,
    PickAll,
    "Advance FancyPickers one step"
);
JS_STEST_LWSTOM(FPickerTest, SetValidateCallback, 2, "Set validate callback function")
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);
    if (NumArguments != 2)
    {
        JS_ReportError(pContext, "Usage: FPickerTest.SetValidateCallback(idx, func)");
        return JS_FALSE;
    }

    UINT32 idx;
    if (OK != JavaScriptPtr()->FromJsval(pArguments[0], &idx) ||
        idx >= FPK_FPK_NUM_PICKERS)
    {
        JS_ReportWarning(pContext, "Callback idx not valid.\n");
        RETURN_RC(RC::BAD_PARAMETER);
    }

    FPickerTest *pObj = JS_GET_PRIVATE(FPickerTest, pContext, pObject, NULL);
    if (pObj)
    {
        pObj->GetCallbacks(idx)->Connect(pObject, pArguments[1]);

        RETURN_RC(OK);
    }

    RETURN_RC(RC::SOFTWARE_ERROR);
}

