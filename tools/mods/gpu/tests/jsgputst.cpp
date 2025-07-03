/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2009,2014,2016-2017,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file jsgputst.cpp
 */

#include "gputest.h"

class JsGpuTest: public GpuTest
{
public:
    // constructor
    JsGpuTest();
    virtual ~JsGpuTest();

    virtual bool GetTestsAllSubdevices() {return false;}

    virtual bool IsSupportedVf() { return true; }

    virtual RC Run();
};

JsGpuTest::JsGpuTest()
{
    SetName("JsGpuTest");
}

/*virtual*/
JsGpuTest::~JsGpuTest()
{
}

RC JsGpuTest::Run()
{
    RC rc;
    UINT32 val;
    jsval jsRc;
    JsArray args;
    JavaScriptPtr pJs;

    // Call the JS function that will do the real test
    CHECK_RC(pJs->CallMethod(GetJSObject(), "JSRunFunction", args, &jsRc));

    // Colwert the jsRc into a C++ RC
    CHECK_RC(pJs->FromJsval(jsRc, &val));
    rc = val;

    return rc;
}

JS_CLASS_INHERIT(JsGpuTest, GpuTest, "A generic JavaScript Test");
