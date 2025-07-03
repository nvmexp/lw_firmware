/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2004-2007,2016,2019,2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//
// Javascript hooks into mdiag test and trep reporting mechanism
//

#include <sstream>

#include "core/include/script.h"
#include "core/include/jscript.h"

#include "core/include/tee.h"
#include "core/include/platform.h"
#include "core/include/xp.h"

#include "mdiag/tests/test_state_report.h"
#include "core/utility/trep.h"
#include "mdiag/tests/test.h"
#include "mdiagrpt.h"

TestReport* TestReport::test_report = 0;

TestReport::TestReport() :
    TestStateReport(),
    Trep(),
    status(Test::TEST_NOT_STARTED)
{
}

TestReport::~TestReport()
{
}

void TestReport::init(const char* testname)
{
    test_report = this;
    s_TestName = testname;
    setGlobalOutputName(testname);
    TestStateReport::init(testname);
    enable();
}

int TestReport::genTrepReport ()
{
    bool isGold = false;

    stringstream ss;
    ss << "test #0: ";
    AppendTrepString( ss.str() );

    switch (GetStatus()) {
    case Test::TEST_NOT_STARTED:
        RawPrintf("not started!\n");
        AppendTrepString( "TEST_NOT_STARTED\n" );
        break;

    case Test::TEST_INCOMPLETE:
        RawPrintf("incomplete!\n");
        AppendTrepString( "TEST_INCOMPLETE\n" );
        break;

    case Test::TEST_SUCCEEDED:
        RawPrintf("passed\n");
        isGold = this->isStatusGold();
        ss.str("");  // clear previous output to the stream
        ss << "TEST_SUCCEEDED" << (isGold ? " (gold)" : "") << "\n";
        AppendTrepString( ss.str() );
        break;

    case Test::TEST_FAILED:
        RawPrintf("failed!\n");
        AppendTrepString( "TEST_FAILED\n" );
        break;

    case Test::TEST_NO_RESOURCES:
        RawPrintf("no resources!\n");
        AppendTrepString( "TEST_NO_RESOURCES\n" );
        break;

    case Test::TEST_FAILED_CRC:
        RawPrintf("image CRCs didn't match!\n");
        AppendTrepString( "TEST_FAILED_CRC\n" );
        break;

    case Test::TEST_CRC_NON_EXISTANT:
        RawPrintf("CRC profile not present!\n");
        AppendTrepString( "TEST_CRC_NON_EXISTANT\n" );
        break;

    case Test::TEST_FAILED_PERFORMANCE:
        RawPrintf("Performance too low!\n");
        AppendTrepString( "TEST_CRC_NON_EXISTANT\n" );
        break;

    default:
        RawPrintf("unknown result (%d)!\n", this->GetStatus());
        AppendTrepString( "unknown\n" );
        break;
    }
    return 0;
}

int TestReport::genReport()
{
    this->genTrepReport();
    return 0;
}

void TestReport::runFailed(const char *report)
{
    SetStatus(Test::TEST_FAILED);
    TestStateReport::runFailed();
}
void TestReport::runInterrupted()
{
    SetStatus(Test::TEST_INCOMPLETE);
    TestStateReport::runInterrupted();
}
void TestReport::runSucceeded(bool override)
{
    SetStatus(Test::TEST_SUCCEEDED);
    TestStateReport::runSucceeded(override);
}
void TestReport::crcCheckPassed()
{
    SetStatus(Test::TEST_SUCCEEDED);
    TestStateReport::crcCheckPassed();
}
void TestReport::crcCheckPassedGold()
{
    SetStatus(Test::TEST_SUCCEEDED);
    TestStateReport::crcCheckPassedGold();
}
void TestReport::crlwsingOldVersion()
{
    SetStatus(Test::TEST_CRC_NON_EXISTANT);
    TestStateReport::crlwsingOldVersion();
}
void TestReport::crcNoFileFound()
{
    SetStatus(Test::TEST_CRC_NON_EXISTANT);
    TestStateReport::crcNoFileFound();
}
void TestReport::goldCrcMissing()
{
    SetStatus(Test::TEST_CRC_NON_EXISTANT);
    TestStateReport::goldCrcMissing();
}
void TestReport::crcCheckFailed(ICrcProfile *crc)
{
    SetStatus(Test::TEST_FAILED_CRC);
    TestStateReport::crcCheckFailed(crc);
}

void TestReport::crcCheckFailed(const char *report)
{
    SetStatus(Test::TEST_FAILED_CRC);
    TestStateReport::crcCheckFailed(report);
}
void TestReport::error(const char *report)
{
    SetStatus(Test::TEST_FAILED);
    TestStateReport::error(report);
}

//------------------------------------------------------------------------------
// JavaScript linkage

JS_CLASS_NO_ARGS(TestReport, "mdiag test state report class", "Usage: new TestStateReport");

C_(TestReport_init);
static STest TestReport_init
(
    TestReport_Object,
    "init",
    C_TestReport_init,
    0,
    "init"
    );

C_(TestReport_init)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    string str;
    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &str))) {
        JS_ReportError(pContext, "Usage: init(testname)");
        return JS_FALSE;
    }

    TestReport* test_report = (TestReport*) JS_GetPrivate(pContext, pObject);

    test_report->init(str.c_str());

    RETURN_RC(OK);
}

C_(TestReport_setupFailed);
static STest TestReport_setupFailed
(
    TestReport_Object,
    "setupFailed",
    C_TestReport_setupFailed,
    0,
    "setupFailed"
    );

C_(TestReport_setupFailed)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    string str;
    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &str))) {
        JS_ReportError(pContext, "Usage: setupFailed(report)");
        return JS_FALSE;
    }

    TestReport* test_report = (TestReport*) JS_GetPrivate(pContext, pObject);

    test_report->setupFailed(str.c_str());

    RETURN_RC(OK);;
}

C_(TestReport_runFailed);
static STest TestReport_runFailed
(
    TestReport_Object,
    "runFailed",
    C_TestReport_runFailed,
    0,
    "runFailed"
    );

C_(TestReport_runFailed)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    if (NumArguments > 1) {
        JS_ReportError(pContext, "Usage: runFailed([report])");
        return JS_FALSE;
    }

    TestReport* test_report = (TestReport*) JS_GetPrivate(pContext, pObject);

    string str;

    if (NumArguments == 1) {
        if (OK != pJavaScript->FromJsval(pArguments[0], &str)) {
            JS_ReportWarning(pContext, "Usage: runFailed(report)");
        }
        test_report->runFailed(str.c_str());
    } else {
        test_report->runFailed();
    }

    RETURN_RC(OK);;
}

C_(TestReport_runInterrupted);
static STest TestReport_runInterrupted
(
    TestReport_Object,
    "runInterrupted",
    C_TestReport_runInterrupted,
    0,
    "runInterrupted"
    );

C_(TestReport_runInterrupted)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    if (NumArguments != 0) {
        JS_ReportError(pContext, "Usage: runInterrupted()");
        return JS_FALSE;
    }

    TestReport* test_report = (TestReport*) JS_GetPrivate(pContext, pObject);
    test_report->runInterrupted();
    RETURN_RC(OK);;
}

C_(TestReport_runSucceeded);
static STest TestReport_runSucceeded
(
    TestReport_Object,
    "runSucceeded",
    C_TestReport_runSucceeded,
    0,
    "runSucceeded"
    );

C_(TestReport_runSucceeded)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    if (NumArguments != 0) {
        JS_ReportError(pContext, "Usage: runSucceeded()");
        return JS_FALSE;
    }

    TestReport* test_report = (TestReport*) JS_GetPrivate(pContext, pObject);
    test_report->runSucceeded();
    RETURN_RC(OK);;
}

C_(TestReport_crcCheckPassed);
static STest TestReport_crcCheckPassed
(
    TestReport_Object,
    "crcCheckPassed",
    C_TestReport_crcCheckPassed,
    0,
    "crcCheckPassed"
    );

C_(TestReport_crcCheckPassed)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    if (NumArguments != 0) {
        JS_ReportError(pContext, "Usage: crcCheckPassed()");
        return JS_FALSE;
    }

    TestReport* test_report = (TestReport*) JS_GetPrivate(pContext, pObject);
    test_report->crcCheckPassed();

    RETURN_RC(OK);;
}

C_(TestReport_crcCheckPassedGold);
static STest TestReport_crcCheckPassedGold
(
    TestReport_Object,
    "crcCheckPassedGold",
    C_TestReport_crcCheckPassedGold,
    0,
    "crcCheckPassedGold"
    );

C_(TestReport_crcCheckPassedGold)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    if (NumArguments != 0) {
        JS_ReportError(pContext, "Usage: crcCheckPassedGold()");
        return JS_FALSE;
    }

    TestReport* test_report = (TestReport*) JS_GetPrivate(pContext, pObject);
    test_report->crcCheckPassedGold();

    RETURN_RC(OK);;
}

C_(TestReport_crlwsingOldVersion);
static STest TestReport_crlwsingOldVersion
(
    TestReport_Object,
    "crlwsingOldVersion",
    C_TestReport_crlwsingOldVersion,
    0,
    "crlwsingOldVersion"
    );

C_(TestReport_crlwsingOldVersion)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    if (NumArguments != 0) {
        JS_ReportError(pContext, "Usage: crlwsingOldVersion()");
        return JS_FALSE;
    }

    TestReport* test_report = (TestReport*) JS_GetPrivate(pContext, pObject);
    test_report->crlwsingOldVersion();

    RETURN_RC(OK);;
}

C_(TestReport_crcNoFileFound);
static STest TestReport_crcNoFileFound
(
    TestReport_Object,
    "crcNoFileFound",
    C_TestReport_crcNoFileFound,
    0,
    "crcNoFileFound"
    );

C_(TestReport_crcNoFileFound)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    if (NumArguments != 0) {
        JS_ReportError(pContext, "Usage: crcNoFileFound()");
        return JS_FALSE;
    }

    TestReport* test_report = (TestReport*) JS_GetPrivate(pContext, pObject);
    test_report->crcNoFileFound();

    RETURN_RC(OK);;
}

C_(TestReport_goldCrcMissing);
static STest TestReport_goldCrcMissing
(
    TestReport_Object,
    "goldCrcMissing",
    C_TestReport_goldCrcMissing,
    0,
    "goldCrcMissing"
    );

C_(TestReport_goldCrcMissing)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    if (NumArguments != 0) {
        JS_ReportError(pContext, "Usage: goldCrcMissing()");
        return JS_FALSE;
    }

    TestReport* test_report = (TestReport*) JS_GetPrivate(pContext, pObject);
    test_report->goldCrcMissing();

    RETURN_RC(OK);;
}

C_(TestReport_crcCheckFailed);
static STest TestReport_crcCheckFailed
(
    TestReport_Object,
    "crcCheckFailed",
    C_TestReport_crcCheckFailed,
    0,
    "crcCheckFailed"
    );

C_(TestReport_crcCheckFailed)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    string str;
    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &str))) {
        JS_ReportError(pContext, "Usage: crcCheckFailed(report)");
        return JS_FALSE;
    }

    TestReport* test_report = (TestReport*) JS_GetPrivate(pContext, pObject);

    test_report->crcCheckFailed(str.c_str());

    RETURN_RC(OK);;
}

C_(TestReport_error);
static STest TestReport_error
(
    TestReport_Object,
    "error",
    C_TestReport_error,
    0,
    "error"
    );

C_(TestReport_error)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    if (NumArguments != 0) {
        JS_ReportError(pContext, "Usage: error()");
        return JS_FALSE;
    }

    TestReport* test_report = (TestReport*) JS_GetPrivate(pContext, pObject);
    test_report->error();

    RETURN_RC(OK);;
}

C_(TestReport_isStatusGold);
static STest TestReport_isStatusGold
(
    TestReport_Object,
    "isStatusGold",
    C_TestReport_isStatusGold,
    0,
    "isStatusGold"
    );

C_(TestReport_isStatusGold)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    if (NumArguments != 0) {
        JS_ReportError(pContext, "Usage: isStatusGold()");
        return JS_FALSE;
    }

    TestReport* test_report = (TestReport*) JS_GetPrivate(pContext, pObject);
    test_report->isStatusGold();

    RETURN_RC(OK);;
}

C_(TestReport_disable);
static STest TestReport_disable
(
    TestReport_Object,
    "disable",
    C_TestReport_disable,
    0,
    "disable"
    );

C_(TestReport_disable)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    if (NumArguments != 0) {
        JS_ReportError(pContext, "Usage: disable()");
        return JS_FALSE;
    }

    TestReport* test_report = (TestReport*) JS_GetPrivate(pContext, pObject);
    test_report->disable();

    RETURN_RC(OK);;
}

C_(TestReport_enable);
static STest TestReport_enable
(
    TestReport_Object,
    "enable",
    C_TestReport_enable,
    0,
    "enable"
    );

C_(TestReport_enable)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    if (NumArguments != 0) {
        JS_ReportError(pContext, "Usage: enable()");
        return JS_FALSE;
    }

    TestReport* test_report = (TestReport*) JS_GetPrivate(pContext, pObject);
    test_report->enable();

    RETURN_RC(OK);;
}

C_(TestReport_create_status_filename);
static STest TestReport_create_status_filename
(
    TestReport_Object,
    "create_status_filename",
    C_TestReport_create_status_filename,
    0,
    "create_status_filename"
    );

C_(TestReport_create_status_filename)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    string str;
    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &str))) {
        JS_ReportError(pContext, "Usage: create_status_filename(filename)");
        return JS_FALSE;
    }

    TestReport* test_report = (TestReport*) JS_GetPrivate(pContext, pObject);

    test_report->create_status_filename(str.c_str());

    RETURN_RC(OK);;
}

C_(TestReport_createNooFileIfNeeded);
static STest TestReport_createNooFileIfNeeded
(
    TestReport_Object,
    "createNooFileIfNeeded",
    C_TestReport_createNooFileIfNeeded,
    0,
    "createNooFileIfNeeded"
    );

C_(TestReport_createNooFileIfNeeded)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    string str;
    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &str))) {
        JS_ReportError(pContext, "Usage: createNooFileIfNeeded(outputName)");
        return JS_FALSE;
    }

    TestReport::createNooFileIfNeeded(str.c_str());

    RETURN_RC(OK);;
}

C_(TestReport_setGlobalOutputName);
static STest TestReport_setGlobalOutputName
(
    TestReport_Object,
    "setGlobalOutputName",
    C_TestReport_setGlobalOutputName,
    0,
    "setGlobalOutputName"
    );

C_(TestReport_setGlobalOutputName)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    string str;
    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &str))) {
        JS_ReportError(pContext, "Usage: setGlobalOutputName(outputName)");
        return JS_FALSE;
    }

    TestReport::setGlobalOutputName(str.c_str());

    RETURN_RC(OK);;
}

C_(TestReport_globalError);
static STest TestReport_globalError
(
    TestReport_Object,
    "globalError",
    C_TestReport_globalError,
    0,
    "globalError"
    );

C_(TestReport_globalError)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    string str;
    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &str))) {
        JS_ReportError(pContext, "Usage: globalError(errString)");
        return JS_FALSE;
    }

    TestReport::globalError(str.c_str());

    RETURN_RC(OK);;
}

C_(TestReport_beforeSetup);
static STest TestReport_beforeSetup
(
    TestReport_Object,
    "beforeSetup",
    C_TestReport_beforeSetup,
    0,
    "beforeSetup"
    );

C_(TestReport_beforeSetup)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    if (NumArguments != 0) {
        JS_ReportError(pContext, "Usage: beforeSetup()");
        return JS_FALSE;
    }

    TestReport* test_report = (TestReport*) JS_GetPrivate(pContext, pObject);
    test_report->beforeSetup();

    RETURN_RC(OK);;
}

C_(TestReport_afterSetup);
static STest TestReport_afterSetup
(
    TestReport_Object,
    "afterSetup",
    C_TestReport_afterSetup,
    0,
    "afterSetup"
    );

C_(TestReport_afterSetup)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    if (NumArguments != 0) {
        JS_ReportError(pContext, "Usage: afterSetup()");
        return JS_FALSE;
    }

    TestReport* test_report = (TestReport*) JS_GetPrivate(pContext, pObject);
    test_report->afterSetup();

    RETURN_RC(OK);;
}

C_(TestReport_beforeRun);
static STest TestReport_beforeRun
(
    TestReport_Object,
    "beforeRun",
    C_TestReport_beforeRun,
    0,
    "beforeRun"
    );

C_(TestReport_beforeRun)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    if (NumArguments != 0) {
        JS_ReportError(pContext, "Usage: beforeRun()");
        return JS_FALSE;
    }

    TestReport* test_report = (TestReport*) JS_GetPrivate(pContext, pObject);
    test_report->beforeRun();

    RETURN_RC(OK);;
}

C_(TestReport_afterRun);
static STest TestReport_afterRun
(
    TestReport_Object,
    "afterRun",
    C_TestReport_afterRun,
    0,
    "afterRun"
    );

C_(TestReport_afterRun)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    if (NumArguments != 0) {
        JS_ReportError(pContext, "Usage: afterRun()");
        return JS_FALSE;
    }

    TestReport* test_report = (TestReport*) JS_GetPrivate(pContext, pObject);
    test_report->afterRun();

    RETURN_RC(OK);;
}

C_(TestReport_beforeCleanup);
static STest TestReport_beforeCleanup
(
    TestReport_Object,
    "beforeCleanup",
    C_TestReport_beforeCleanup,
    0,
    "beforeCleanup"
    );

C_(TestReport_beforeCleanup)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    if (NumArguments != 0) {
        JS_ReportError(pContext, "Usage: beforeCleanup()");
        return JS_FALSE;
    }

    TestReport* test_report = (TestReport*) JS_GetPrivate(pContext, pObject);
    test_report->beforeCleanup();

    RETURN_RC(OK);;
}

C_(TestReport_afterCleanup);
static STest TestReport_afterCleanup
(
    TestReport_Object,
    "afterCleanup",
    C_TestReport_afterCleanup,
    0,
    "afterCleanup"
    );

C_(TestReport_afterCleanup)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    if (NumArguments > 1) {
        JS_ReportError(pContext, "Usage: afterCleanup()\n"
                                 "-or-\n"
                                 "       afterCleanup(pixel_trace_filename)\n");
        return JS_FALSE;
    }

    TestReport* test_report = (TestReport*) JS_GetPrivate(pContext, pObject);
    test_report->afterCleanup();

    // if we're running on rtl then we may need to delete pixel trace file
    if (Platform::GetSimulationMode() == Platform::RTL && NumArguments == 1)
    {
        JavaScriptPtr pJavaScript;
        string pt_file;
        if (OK != pJavaScript->FromJsval(pArguments[0], &pt_file))
        {
            JS_ReportError(pContext, "afterCleanup(): can not read name of pixel trace file\n");
            return JS_FALSE;
        }

        if (Xp::DoesFileExist(pt_file))
        {
            Printf(Tee::PriNormal, "Info: deleting %s...\n", pt_file.c_str());
            // need to return the JS status
            if( Xp::EraseFile(pt_file) != OK ) {
                JS_ReportError(pContext,
                    "afterCleanup(): Failed to delete file\n");
                return JS_FALSE;
            }
        }
    }

    RETURN_RC(OK);;
}

C_(TestReport_Passed);
static STest TestReport_Passed
(
    TestReport_Object,
    "Passed",
    C_TestReport_Passed,
    0,
    "Set PASSED status"
    );

C_(TestReport_Passed)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    if (NumArguments != 0) {
        JS_ReportError(pContext, "Usage: Passed()");
        return JS_FALSE;
    }

    TestReport::GetTestReport()->crcCheckPassed();
    RETURN_RC(OK);;
}

C_(TestReport_SetTrepFileName);
static STest TestReport_SetTrepFileName
(
    TestReport_Object,
    "SetTrepFileName",
    C_TestReport_SetTrepFileName,
    0,
    "SetTrepFileName"
    );

C_(TestReport_SetTrepFileName)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    string str;
    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &str))) {
        JS_ReportError(pContext, "Usage: SetTrepFileName(trepName)");
        return JS_FALSE;
    }

    TestReport* trep = (TestReport*) JS_GetPrivate(pContext, pObject);

    trep->SetTrepFileName(str.c_str());

    RETURN_RC(OK);;
}

C_(TestReport_AppendTrepString);
static STest TestReport_AppendTrepString
(
    TestReport_Object,
    "AppendTrepString",
    C_TestReport_AppendTrepString,
    0,
    "AppendTrepString"
    );

C_(TestReport_AppendTrepString)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    string str;
    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &str))) {
        JS_ReportError(pContext, "Usage: AppendTrepString(str)");
        return JS_FALSE;
    }

    TestReport* trep = (TestReport*) JS_GetPrivate(pContext, pObject);

    trep->AppendTrepString(str);

    RETURN_RC(OK);;
}
