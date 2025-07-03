/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2004-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file rmtest.h
 * @brief Base class for all RM (GPU Resource Manager) tests
 */
#ifndef INCLUDED_RMTEST_H
#define INCLUDED_RMTEST_H

#ifndef INCLUDED_GPUTEST_H
#include "gputest.h"
#endif

#define RUN_RMTEST_TRUE "RunRMTest"
#include "Lwcm.h"

extern SObject RmTest_Object;

class RmTest: public GpuTest
{
public:
    RmTest();
    virtual ~RmTest();
    virtual bool IsSupported();    // Removing Pure virtuality as now IsTestSupported() method of a test will be called instead of IsSupported.
    virtual bool IsClassSupported(UINT32 classID);
    bool IsSupportedVf() { return true; } // Allow all rmtests to run on VFs by default.
    virtual string IsTestSupported() = 0;
    virtual RC Setup() = 0;
    virtual RC Run() = 0;
    virtual RC Cleanup() = 0;
    virtual bool RequiresSoftwareTreeValidation() const override { return false; }
    virtual RC GoToSuspendAndResume(UINT32 sleepInSec);
    virtual void SuspendResumeSupported();
    virtual bool IsTestType(TestType tt);
    SETGET_PROP(SuspendResumeSupported, bool);
    SETGET_PROP(EnableSRCall, bool);
    bool m_SuspendResumeSupported;
    bool m_EnableSRCall;
    LwRm::Handle m_hCh;
    Channel *    m_pCh;

    bool IsDVS();

protected:
    RC StartRCTest();
    RC EndRCTest();
private:

    LW2080_CTRL_CMD_RC_INFO_PARAMS m_rcInfoParams;
    bool m_bInRcTest;

};

#endif // INCLUDED_RMTEST_H
