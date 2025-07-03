/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011-2014, 2017,2021 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/include/gpusbdev.h"
#include "gpu/tests/pcie/pextest.h"

// Maybe it would just be sufficient to just use GpuTest?
// Lwrrently, PEX is not decoupled from PState, so it might still be worth
// inheriting from PexTest

class PerfSwitch: public PexTest
{
public:
    PerfSwitch();
    ~PerfSwitch();

    virtual RC   Setup();
    virtual RC   RunTest();
    virtual RC   Cleanup();
    virtual bool IsSupported();

private:
};
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(PerfSwitch, PexTest, "PerfSwiching test");

//-----------------------------------------------------------------------------

PerfSwitch::PerfSwitch()
{
}
//-----------------------------------------------------------------------------
PerfSwitch::~PerfSwitch()
{
}
//-----------------------------------------------------------------------------
RC PerfSwitch::Setup()
{
    StickyRC rc;
    CHECK_RC(PexTest::Setup());

    JsArray Args;
    jsval RetValJs = JSVAL_NULL;
    JavaScriptPtr pJs;
    rc = pJs->CallMethod(GetJSObject(), "JsFuncSetup",Args,&RetValJs);
    if (OK == rc)
    {
        UINT32 RetVal = RC::SOFTWARE_ERROR;
        rc = pJs->FromJsval(RetValJs, &RetVal);
        rc = RetVal;
    }
    return rc;
}
//-----------------------------------------------------------------------------
RC PerfSwitch::RunTest()
{
    StickyRC rc;
    Perf::PerfPoint OrgPerfPt;
    CHECK_RC(GetBoundGpuSubdevice()->GetPerf()->GetLwrrentPerfPoint(&OrgPerfPt));

    JavaScriptPtr pJs;
    JsArray Args;
    jsval RetValJs  = JSVAL_NULL;
    rc = pJs->CallMethod(GetJSObject(), "JsFuncRun", Args, &RetValJs);
    if (OK == rc)
    {
        UINT32 JSRetVal = RC::SOFTWARE_ERROR;
        rc = pJs->FromJsval(RetValJs, &JSRetVal);
        rc = JSRetVal;
    }

    // restore Perf setting (this needs to be done before we return to PexTest
    // wrapper
    rc = GetBoundGpuSubdevice()->GetPerf()->SetPerfPoint(OrgPerfPt);
    return rc;
}
//-----------------------------------------------------------------------------
RC PerfSwitch::Cleanup()
{
    StickyRC rc;
    JavaScriptPtr pJs;
    JsArray Args;
    jsval RetValJs  = JSVAL_NULL;
    rc = pJs->CallMethod(GetJSObject(), "JsFuncCleanup", Args, &RetValJs);
    if (OK == rc)
    {
        UINT32 JSRetVal = RC::SOFTWARE_ERROR;
        rc = pJs->FromJsval(RetValJs, &JSRetVal);
        rc = JSRetVal;
    }
    rc = PexTest::Cleanup();
    return rc;
}
//-----------------------------------------------------------------------------
bool PerfSwitch::IsSupported()
{
    Perf* pPerf = GetBoundGpuSubdevice()->GetPerf();

    if (pPerf->IsPState20Supported())
    {
        if (pPerf->HasPStates())
        {
            return true;
        }
        else
        {
            VerbosePrintf("PerfSwitch not supported when PStates are disabled\n");
        }
    }

    return false;
}
