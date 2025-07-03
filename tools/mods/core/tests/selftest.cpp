/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008,2014 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "modstest.h"

class SelfTest: public ModsTest
{
public:
    SelfTest();
    virtual ~SelfTest();

    virtual bool IsSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    bool m_IsSupportedReturn;
    INT32 m_SetupReturn;
    INT32 m_RunReturn;
    INT32 m_CleanupReturn;

public:
    SETGET_PROP(IsSupportedReturn, bool);
    SETGET_PROP(SetupReturn,       INT32);
    SETGET_PROP(RunReturn,         INT32);
    SETGET_PROP(CleanupReturn,     INT32);
};

SelfTest::SelfTest() :
    m_IsSupportedReturn(true),
    m_SetupReturn(OK),
    m_RunReturn(OK),
    m_CleanupReturn(OK)
{
    SetName("SelfTest");
}

/* virtual */ SelfTest::~SelfTest()
{
}

/* virtual */ bool SelfTest::IsSupported()
{
    return m_IsSupportedReturn;
}

/* virtual */ RC SelfTest::Setup()
{
    return m_SetupReturn;
}

/* virtual */ RC SelfTest::Run()
{
    return m_RunReturn;
}

/* virtual */ RC SelfTest::Cleanup()
{
    return m_CleanupReturn;
}

JS_CLASS_INHERIT(SelfTest, ModsTest, "Generic test for regression testing.");

CLASS_PROP_READWRITE(SelfTest, IsSupportedReturn, bool,
                     "Return value from ModsTest::IsSupported().");
CLASS_PROP_READWRITE(SelfTest, SetupReturn, INT32,
                     "Return value from ModsTest::Setup().");
CLASS_PROP_READWRITE(SelfTest, RunReturn, INT32,
                     "Return value from ModsTest::Run().");
CLASS_PROP_READWRITE(SelfTest, CleanupReturn, INT32,
                     "Return value from ModsTest::Cleanup().");

