/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2004-2007,2009-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "rmtest.h"
#include "gpu/perf/powermgmtutil.h"

//! \brief Default name for RmTests
//------------------------------------------------------------------------------
RmTest::RmTest()
:
  m_hCh(0)
, m_pCh(NULL)
{
    SetName("RmTest");
    m_SuspendResumeSupported = false;
    m_EnableSRCall = false;
    m_rcInfoParams = {};
    m_bInRcTest = false;
}

//! \brief RmTest no longer holds any state, which is probably as it should be
//------------------------------------------------------------------------------
RmTest::~RmTest()
{
    MASSERT(!m_bInRcTest);
}

/*string RmTest::IsSupportedCheck()
{
    return "None";
}*/
//! \brief RmTest::SuspendResumeSupported() : Function to enable the test
//! as SuspenResume Supported
//------------------------------------------------------------------------------
void RmTest::SuspendResumeSupported()
{
    m_SuspendResumeSupported = true;
}

//! \brief Identify the type of ModsTest subclass
//-----------------------------------------------------------------------------
/* virtual */ bool RmTest::IsTestType(TestType tt)
{
    return (tt == RM_TEST) || GpuTest::IsTestType(tt);
}
//Defining IsSupported which is been called from modstest.cpp and redirecting it to call IsTestSupported method of a test.
bool RmTest::IsSupported()
{
   string skipReason = IsTestSupported();
   if(!skipReason.compare(RUN_RMTEST_TRUE))
   {
      return true;
   }
   else
   {
      Printf(Tee::PriLow,"### Skip Reason: %s ###\n",skipReason.c_str());
      return false;
   }
}

//Defining IsClassSupported which is been called from modstest.cpp and redirecting it to call IsTestSupported method of a test.
bool RmTest::IsClassSupported(UINT32 classID)
{
    return LwRmPtr()->IsClassSupported(classID, GetBoundGpuDevice());
}
//! \brief Call before starting to test RC
//! Disables error logger and RC breakpoints.
RC RmTest::StartRCTest()
{
    RC rc;
    LwRmPtr pLwRm;
    LW2080_CTRL_CMD_RC_INFO_PARAMS newRcInfoParams = {};

    MASSERT(!m_bInRcTest);

    memset(&m_rcInfoParams, 0, sizeof(m_rcInfoParams));

    CHECK_RC(pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                                LW2080_CTRL_CMD_GET_RC_INFO,
                                &m_rcInfoParams,
                                sizeof(m_rcInfoParams)));

    newRcInfoParams.rcMode = LW2080_CTRL_CMD_RC_INFO_MODE_ENABLE;
    newRcInfoParams.rcBreak = LW2080_CTRL_CMD_RC_INFO_BREAK_DISABLE;
    CHECK_RC(pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                                LW2080_CTRL_CMD_SET_RC_INFO,
                                &newRcInfoParams, sizeof(newRcInfoParams)));

    CHECK_RC(ErrorLogger::StartingTest());
    ErrorLogger::IgnoreErrorsForThisTest();
    m_bInRcTest = true;

    return rc;
}

//! \brief call when completing an RC test
//! Restores to state before StartRCTest() was called
RC RmTest::EndRCTest()
{
    RC rc;
    LwRmPtr pLwRm;
    LwU32 dummy;

    MASSERT(m_bInRcTest);
    ErrorLogger::TestCompleted();

    CHECK_RC(pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                                LW2080_CTRL_CMD_SET_RC_INFO,
                                &m_rcInfoParams, sizeof(m_rcInfoParams)));

    CHECK_RC(pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                                LW2080_CTRL_CMD_RC_SET_CLEAN_ERROR_HISTORY,
                                &dummy, sizeof(dummy)));
    m_bInRcTest = false;

    return rc;
}

bool RmTest::IsDVS()
{
   JavaScriptPtr pJs;
   string mode;
   pJs->GetProperty(pJs->GetGlobalObject(), "g_SpecName", &mode);

   return mode == "dvs";
}

//! \brief RmTest::GoToSuspendAndResume : Create a common function to perfom S/R cycle
//! Which will be inherited by all RMTESTs
//------------------------------------------------------------------------------
RC RmTest::GoToSuspendAndResume(UINT32 sleepInSec)
{
    RC rc;
    PowerMgmtUtil    powerMgmtUtil;
    powerMgmtUtil.BindGpuSubdevice(GetBoundGpuSubdevice());

    if (m_EnableSRCall)
    {
        CHECK_RC(powerMgmtUtil.GoToSuspendAndResume(sleepInSec));
    }

    return rc;
}

C_(RmTest_GoToSuspendAndResume);
static STest RmTest_GoToSuspendResume
(
    RmTest_Object,
    "GoToSuspendAndResume",
    C_RmTest_GoToSuspendAndResume,
    1,
    "Carry out S/R cycle."
);

C_(RmTest_GoToSuspendAndResume)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    UINT32 sleepDelay;

    JavaScriptPtr pJavaScript;

   if ((NumArguments != 1) || (OK != pJavaScript->FromJsval(pArguments[0], &sleepDelay)))

   {
      JS_ReportError(pContext, "Error in Rmtest.GoToSuspendAndResume()");
      return JS_FALSE;
   }

    RmTest *pTest = (RmTest *)JS_GetPrivate(pContext, pObject);
    RETURN_RC(pTest->GoToSuspendAndResume(sleepDelay));
}

//------------------------------------------------------------------------------
// JS linkage
//------------------------------------------------------------------------------
JS_CLASS_VIRTUAL_INHERIT(RmTest, GpuTest, "RM test base class");
CLASS_PROP_READWRITE(RmTest, SuspendResumeSupported, bool,
    "Flag to specify that S/R cycle is supported for the test.");
CLASS_PROP_READWRITE(RmTest, EnableSRCall, bool,
    "Enable Flag for call to S/R cycle.");
