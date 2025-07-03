/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/utility/trep.h"

#include "mdiag/mdiag.h"
#include "mdiag/sysspec.h"
#include "mdiag/tests/test.h"
#include "mdiag/tests/test_state_report.h"
#include "mdiag/utils/tracefil.h"
#include "mdiag/utils/mdiag_xml.h"

#include "utl.h"
#include "utlchannel.h"
#include "utlevent.h"
#include "utltest.h"
#include "utlusertest.h"
#include "utlutil.h"
#include "utlsmcengine.h"
#include "utltsg.h"
#include "mdiag/tests/testdir.h"
#include "utlsurface.h"

vector<string> UtlTest::s_CfgLines;
map<Test*, unique_ptr<UtlTest>> UtlTest::s_Tests;
UINT32 UtlTest::s_RunningTestCount = 0;

void UtlTest::Register(py::module module)
{
    UtlKwargsMgr* kwargs = UtlKwargsMgr::Instance();
    kwargs->RegisterKwarg("Test.Create", "fromCfgFile", false, "test index in config file");
    kwargs->RegisterKwarg("Test.Create", "fromString", false, "Test command");    
    kwargs->RegisterKwarg("Test.SetSmcEngine", "smcEngine", true, "target SMC Engine");
    kwargs->RegisterKwarg("Test.GetSurfaceByName", "name", true, "name of surface");
    kwargs->RegisterKwarg("Test.GetChannelByName", "name", true, "name of channel");
    kwargs->RegisterKwarg("Test.Run", "deferCleanup", false, "set to false to do test cleanup after the test has been run");
    kwargs->RegisterKwarg("Test.ReadTraceFile", "file", true, "Read file content from a trace file");
    
    py::class_<UtlTest> test(module, "Test");
    test.def_static("Create", &UtlTest::Create,
        UTL_DOCSTRING("Test.Create", "This function creates a Test object that can be used to run a trace. Either fromCfgFile or fromString should be passed to create a test"),
        py::return_value_policy::reference);
    test.def("Setup", &UtlTest::Setup,
        "This function exelwtes the setup phase for a Test.");
    test.def("Run", &UtlTest::Run,
        UTL_DOCSTRING("Test.Run", "This function exelwtes the run phase for a Test."));
    test.def("RunFinished", &UtlTest::RunFinished,
        "This function returns true if the test is finished running.");
    test.def("GetChannels", &UtlTest::GetChannelsPy,
        "This function returns a list of Channel objects that have been created by this Test.");
    test.def("GetChannelByName", &UtlTest::GetChannelByName,
        "This function returns the test's channel whose name matches the passed name.",
        py::return_value_policy::reference);
    test.def_static("GetTests", &UtlTest::GetTestsPy,
        "This static function returns a list of all Test objects that are accessible from the calling script.  If the script is ilwoked by the -utl_script argument, the function will return all Test objects.  If the script is ilwoked by the -utl_test_script argument, only the Test object that is associated with the script will be returned.");
    test.def_static("GetCfgTraceCount", &UtlTest::GetCfgTraceCount,
        "This function returns the total num of tests in cfg file.");
    test.def("SetSmcEngine", &UtlTest::SetSmcEngine,
        UTL_DOCSTRING("Test.SetSmcEngine", "This function sets the SmcEngine for the Test."));
    test.def("GetTsgs", &UtlTest::GetTsgsPy,
        "This function returns a list of Tsg objects that have been created by this Test.");
    test.def("AbortTest", &UtlTest::AbortTest,
        "This function aborts the test.");
    test.def("GetSmcEngine", &UtlTest::GetSmcEngine,
        "This function returns the SmcEngine of the Test.",
        py::return_value_policy::reference);
    test.def("GetSurfaceByName", &UtlTest::GetSurfaceByName,
        UTL_DOCSTRING("Test.GetSurfaceByName", "This function returns the test's surface whose name matches the passed name."),
        py::return_value_policy::reference);
    test.def("GetSurfaces", &UtlTest::GetSurfacesPy,
        "This function returns all surfaces allocated by the test.");
    test.def_property_readonly("testId", &UtlTest::GetTestId,
        "This read-only data member specifies the ID number of the test.  The ID number of a test is the index position of the test within the list returned by the Test.GetTests function.");
    test.def_property("status", &UtlTest::GetStatus, &UtlTest::SetStatus, 
        "This is the current status of the test, must be one of utl.Status");
    test.def("GetUserTestData", &UtlTest::GetUserTestData,
        "If there are any -utl_test_data arguments specified for this test, this function returns a list of the string values passed to every such argument.  If there are no -utl_test_data arguments passed for this test, then an empty list is returned.");
    test.def("GetTestArgs", &UtlTest::GetTestArgs,
        "This function returns per trace arguments of the test.");
    test.def("GetTracePath", &UtlTest::GetTracePath,
        "This function returns trace path of the test.");
    test.def("ReadTraceFile", &UtlTest::ReadTraceFile,
        "This function reads a trace file content.");
    test.def("GetName", &UtlTest::GetName,
        UTL_DOCSTRING("Test.GetName", "This function returns the name of the test."));
    test.def("IsAborted", &UtlTest::IsAborted,
        UTL_DOCSTRING("Test.IsAborted", "This function returns True if the test has been aborted."));
    test.def("__repr__", &UtlTest::ToString);
    test.def("GetScriptTest", &UtlTest::GetScriptTest,
        UTL_DOCSTRING("Test.GetScriptTest", "If the script calling this function was passed as a -utl_test_script argument, this function will return the Test object assocated with the script.  Otherwise, this function will return None."),
        py::return_value_policy::reference);

    py::enum_<Test::TestStatus> status(module, "Status",
        "This enum is used to indicate the status of the entire test run.");
    status.value("NotStarted", Test::TestStatus::TEST_NOT_STARTED);
    status.value("Incomplete", Test::TestStatus::TEST_INCOMPLETE);
    status.value("Succeeded", Test::TestStatus::TEST_SUCCEEDED);
    status.value("Failed", Test::TestStatus::TEST_FAILED);
    status.value("FailedTimeout", Test::TestStatus::TEST_FAILED_TIMEOUT);
    status.value("FailedCrc", Test::TestStatus::TEST_FAILED_CRC);
    status.value("NoResources", Test::TestStatus::TEST_NO_RESOURCES);
    status.value("CrcNonExistant", Test::TestStatus::TEST_CRC_NON_EXISTANT);    
    status.value("FailedPerformance", Test::TestStatus::TEST_FAILED_PERFORMANCE);
    status.value("FailedEcov", Test::TestStatus::TEST_FAILED_ECOV);
    status.value("EcovNonExistant", Test::TestStatus::TEST_ECOV_NON_EXISTANT);
}

// This function can be called by a UTL script to create a new test.
//
UtlTest* UtlTest::Create(py::kwargs kwargs)
{
    UtlUtility::RequirePhase(Utl::InitEventPhase | Utl::RunPhase,
        "Test.Create");

    UINT32 fromCfgFile;
    string fromString;

    if (UtlUtility::GetOptionalKwarg<UINT32>(&fromCfgFile, kwargs, "fromCfgFile", "Test.Create")
        && UtlUtility::GetOptionalKwarg<string>(&fromString, kwargs, "fromString", "Test.Create"))
    {
         UtlUtility::PyError("fromCfgFile and fromString both are specified in Test.Create(). Only one of them has to be specified.");
    }

    if (UtlUtility::GetOptionalKwarg<UINT32>(&fromCfgFile, kwargs, "fromCfgFile", "Test.Create"))
    {
        if (fromCfgFile >= s_CfgLines.size())
        {
            UtlUtility::PyError("fromCfgFile value invalid in Test.Create()");
        }

        UtlGil::Release gil;

        return UtlTest::CreateTest(s_CfgLines[fromCfgFile]);
    }

    if (UtlUtility::GetOptionalKwarg<string>(&fromString, kwargs, "fromString", "Test.Create"))
    {
        UtlGil::Release gil;

        return UtlTest::CreateTest(fromString);
    }

    UtlUtility::PyError("Unable to create a test in Test.Create() because fromCfgFile and fromString unspecified.");
    return nullptr;
}

// Create a test based on a string or index into the <test>.cfg file.
//
UtlTest* UtlTest::CreateTest(const string& testConfig)
{
    size_t pos1 = testConfig.find_first_not_of(" \t");
    size_t pos2 = testConfig.find_first_of(" \t", pos1);
    string testName = testConfig.substr(pos1, pos2 - pos1);
    DebugPrintf("UTL: Creating test of type %s.\n", testName.c_str());

    Utl* utl = Utl::Instance();
    unique_ptr<ArgDatabase> testArgs = make_unique<ArgDatabase>(utl->GetArgDatabase());
    if (pos2 != string::npos)
    {
        string argLine = testConfig.substr(pos2);
        if (OK != testArgs->AddArgLine(argLine.c_str(), ArgDatabase::ARG_MUST_USE))
        {
            UtlUtility::PyError("unable to use test arguments during Test.Create()");
        }
    }

    unique_ptr<Test> ownedTest;
    TestListEntryPtr pos;
    pos = GetTestListHead();
    while (pos)
    {
        if (!strcmp(testName.c_str(), pos->name)) 
        {
            ownedTest.reset((pos->factory_fn)(testArgs.get()));
            break;
        }
        pos = pos->next;
    }
    
    if (ownedTest.get() == nullptr)
    {
        UtlUtility::PyError("unable to create %s test", testName.c_str());
    }

    if (ownedTest->IsEcoverSupported())
    {
        Test::IncreaseNumEcoverSupportedTests();
    }

    ownedTest->getStateReport()->init(testName.c_str());

    //bug 200552325, vpm report expects fixed format.
    if (gXML != NULL)
    {
        XD->XMLStartStdInline("<MDiagTest");
        XD->outputAttribute("SeqId", ownedTest->GetSeqId());
        XD->outputAttribute("TestName", testName.c_str());
        XD->outputAttribute("TestArgs", testConfig.c_str());
        XD->XMLEndLwrrent();
    }

    UtlTest* utlTest = CreateFromTestPtr(ownedTest.get(), testConfig);
    
    utlTest->m_OwnedTest = move(ownedTest);
    utlTest->m_TestArgs = move(testArgs);
    return utlTest;
}

// Create a UtlTest object which will wrap an existing Test object.
// The Test object may have been created by UTL or mdiag.
//
UtlTest* UtlTest::CreateFromTestPtr(Test* test, string testConfig)
{
    MASSERT(s_Tests.count(test) == 0);

    unique_ptr<UtlTest> utlTest(new UtlTest(test));
    UtlTest* returnPtr = utlTest.get();
    s_Tests[test] = move(utlTest);

    // The test config has to be set before UtlTestCreatedEvent is triggered because
    // the event handler might call GetTestArgs() print the testConfig.
    returnPtr->m_TestConfig = testConfig;

    UtlTestCreatedEvent event(returnPtr);
    event.Trigger();

    return returnPtr;
}

// The UtlTest constructor.  It should not be called from outside of UtlTest.
//
UtlTest::UtlTest(Test* test)
{
    m_TestId = s_Tests.size();
    m_Test = test;
}

UtlTest::~UtlTest()
{
    m_OwnedTest.reset(nullptr);
    CleanupUtlTestResources();

    // The ArgDatabase must be freed after the Test pointer is freed because
    // there is an ArgReader object in m_Test that references m_TestArgs.
    m_TestArgs.reset(nullptr);
}

// Called from the Utl class to free the UtlTest objects, which then
// frees the Test specific objects (for example, Trace3DTest).
//
void UtlTest::FreeTests()
{
    s_Tests.clear();
}

// Add a channel to the list of channels associated with this test.
//
void UtlTest::AddChannel(UtlChannel* channel)
{
    m_Channels.push_back(channel);
}

// This function is called by the m_Test object
// whenever a test tsg is created.
//
void UtlTest::AddTsg(UtlTsg* utlTsg)
{
    m_Tsgs.push_back(utlTsg);
}

// This function can be called by UTL script to 
// bind UtlSmcEngine to UtlTest.
//
void UtlTest::SetSmcEngine(py::kwargs kwargs)
{
    // Parse arguments from Python script
    UtlSmcEngine * smcEngine = UtlUtility::GetRequiredKwarg<UtlSmcEngine *>(kwargs, "smcEngine",
                            "Test.SetSmcEngine");

    if (!smcEngine)
    {
        UtlUtility::PyError("Invalid smc engine.");
    }

    UtlGil::Release gil;

    if (m_State >= TEST_STATE_SET_UP_START)
    {
        UtlUtility::PyError("Test.SetSmcEngine must be called before Test.Setup.");
    }
    m_Test->SetSmcEngine(smcEngine->GetSmcEngineRawPtr());
}

// This function can be called from a UTL script to setup the test.
// The function must be called before Run.
//
void UtlTest::Setup()
{
    UtlGil::Release gil;

    if (m_State != TEST_STATE_INITIAL)
    {
        UtlUtility::PyError("Test.Setup run twice.");
    }

    m_State = TEST_STATE_SET_UP_START;

    int stage = 0;

    while (m_State == TEST_STATE_SET_UP_START)
    {
        if (stage == 0)
        {
            m_Test->getStateReport()->beforeSetup();
        }

        if (m_Test->NumSetupStages() > stage)
        {
            m_Test->SetThreadId2TestMap(Tasker::LwrrentThread());
            int status = m_Test->Setup(stage);

            if ((status == 0) || (m_Test->NumSetupStages() <= (stage + 1)))
            {
                m_Test->getStateReport()->afterSetup();

                if (status)
                {
                    DebugPrintf("UTL: Changing test %u state to TEST_STATE_SET_UP_FINISHED\n", m_TestId);
                    m_State = TEST_STATE_SET_UP_FINISHED;
                }
                else
                {
                    DebugPrintf("UTL: Changing test %u state to TEST_STATE_CLEANED due to failure in setup\n", m_TestId);
                    // If we fail to Setup, just mark it cleaned
                    // so we don't keep trying to setup the test.
                    m_State = TEST_STATE_CLEANED;
                }
            }
        }
        ++stage;
    }
}

// This function can be called from a UTL script to run the test.
// The Setup function must be called first.
//
void UtlTest::Run(py::kwargs kwargs)
{   

    if ((Utl::Instance()->GetArgReader()->ParamPresent("-serial")) && (s_RunningTestCount > 0))
    {
        UtlUtility::PyError("Test %u was aborted as conlwrrent tests were launched with serial flag",m_TestId);
    }

    s_RunningTestCount++;
    bool deferCleanup = false;
    UtlUtility::GetOptionalKwarg<bool>(&deferCleanup, kwargs, "deferCleanup", "Test.Run");

    UtlUtility::FcraLogging(Utility::StrPrintf("[FCRA Internal] [UTL, UtlTest::Run] [Text] Info: "),
        Utility::StrPrintf("start running test <%s> on thread id <%d>", m_Test->GetTestName(), Tasker::LwrrentThread()));

    UtlGil::Release gil;

    Mdiag::PostRunSemaphore();

    if (m_State == TEST_STATE_SET_UP_FINISHED)
    {
        DebugPrintf("UTL: Changing test %u state to TEST_STATE_RUN\n", m_TestId);
        m_State = TEST_STATE_RUN;
        m_Test->getStateReport()->beforeRun();
        m_Test->SetThreadId2TestMap(Tasker::LwrrentThread());
        m_Test->PreRun();
        m_Test->Run();
        m_Test->PostRun();
        m_Test->getStateReport()->afterRun();
    }
    else if (m_State == TEST_STATE_INITIAL)
    {
        UtlUtility::PyError("Test was run before setup complete.");
    }

    Cleanup(deferCleanup);
    s_RunningTestCount--;
}

// Do test cleanup after the test has been run.
//
void UtlTest::Cleanup(bool deferCleanup)
{
    if ((m_State == TEST_STATE_SET_UP_FINISHED) || (m_State == TEST_STATE_RUN))
    {
        // Trigger a test cleanup event before the cleanup actually happens.
        UtlTestCleanupEvent event(this);
        event.Trigger();

        m_Test->getStateReport()->beforeCleanup();
        if (!deferCleanup)
        {
            CleanupUtlTestResources();
            for (UINT32 stage = 0; stage < m_Test->NumCleanUpStages(); ++stage)
            {
                m_Test->CleanUp(stage);
            }
        }
        m_Test->getStateReport()->afterCleanup();
        DebugPrintf("UTL: Changing test %u state to TEST_STATE_CLEANED\n", m_TestId);
        m_State = TEST_STATE_CLEANED;
    }
}

// Clear all the Utl test resources list
// This should be done before the underlying mdiag objects are freed so that 
// UtlTest does not store reference to any freed objects
//
void UtlTest::CleanupUtlTestResources()
{
    m_Channels.clear();
    m_Tsgs.clear();
    m_Surfaces.clear();
}

// Update the trep file with the test status.
//
void UtlTest::UpdateTrepStatus()
{
    Trep* trep = Utl::Instance()->GetTrep();

    string buf = Utility::StrPrintf("test #%u: ", m_TestId);
    trep->AppendTrepString(buf.c_str());

    switch (m_Test->GetStatus())
    {
        case Test::TEST_NOT_STARTED:
            trep->AppendTrepString("TEST_NOT_STARTED\n");
            break;

        case Test::TEST_INCOMPLETE:
            trep->AppendTrepString("TEST_INCOMPLETE\n");
            break;

        case Test::TEST_SUCCEEDED:
            buf = Utility::StrPrintf("TEST_SUCCEEDED%s\n",
                m_Test->getStateReport()->isStatusGold() ? " (gold)" : "");
            trep->AppendTrepString(buf.c_str());
            break;

        case Test::TEST_FAILED:
            trep->AppendTrepString("TEST_FAILED\n");
            break;

        case Test::TEST_NO_RESOURCES:
            trep->AppendTrepString("TEST_NO_RESOURCES\n");
            break;

        case Test::TEST_FAILED_CRC:
            trep->AppendTrepString("TEST_FAILED_CRC\n");
            break;

        case Test::TEST_CRC_NON_EXISTANT:
            trep->AppendTrepString("TEST_CRC_NON_EXISTANT\n");
            break;

        case Test::TEST_FAILED_PERFORMANCE:
            trep->AppendTrepString("TEST_CRC_NON_EXISTANT\n");
            break;

        case Test::TEST_FAILED_ECOV:
            trep->AppendTrepString("TEST_FAILED_ECOV_MATCH\n");
            break;

        case Test::TEST_ECOV_NON_EXISTANT:
            trep->AppendTrepString("TEST_FAILED_ECOV_MISSING\n");
            break;

        default:
            trep->AppendTrepString("unknown\n");
            break;
    }
}

// This function can be called from a UTL script to determine if a test is
// finished running.
//
bool UtlTest::RunFinished() const
{
    return m_State == TEST_STATE_CLEANED;
}

// This function can be called from a UTL script to get the list of channels
// created by this test.
//
py::list UtlTest::GetChannelsPy()
{
    return UtlUtility::ColwertPtrVectorToPyList(m_Channels);
}

// This function can be called from a UTL script to get the channel (of the name passed)
// associated with this test.
UtlChannel* UtlTest::GetChannelByName(py::kwargs kwargs)
{
    string channelName = UtlUtility::GetRequiredKwarg<string>(kwargs, "name",
        "Test.GetChannelByName");

    UtlGil::Release gil;

    for (auto pChannel : m_Channels)
    {
        if (pChannel->GetName() == channelName)
        {
            return pChannel;
        }
    }

    return nullptr;
}

// This function can be called from a UTL script to get the list of tsgs
// associated with this test.
py::list UtlTest::GetTsgsPy()
{
    return UtlUtility::ColwertPtrVectorToPyList(m_Tsgs);
}

py::list UtlTest::GetTestsPy()
{
    return UtlUtility::ColwertPtrVectorToPyList(UtlTest::GetTests());
}

// This function can be called from a UTL script to get the list of all
// tests that have been created.
//
vector<UtlTest*> UtlTest::GetTests()
{
    vector<UtlTest*> testList;
    UtlTest* scriptTest = Utl::Instance()->GetTestFromScriptId();

    if (scriptTest != nullptr)
    {
        testList.push_back(scriptTest);
    }
    else
    {
        for (const auto& keyValue : s_Tests)
        {
            testList.push_back(keyValue.second.get());
        }

        // Sort the list so that the tests are always listed in the order
        // they were created.
        sort(testList.begin(), testList.end(),
            [](UtlTest* a, UtlTest* b)
            {
                return a->m_TestId < b->m_TestId;
            });
    }

    return testList;
}

// This function can be called from a UTL script to get the total num of tests
// in cfg file.
//
UINT32 UtlTest::GetCfgTraceCount()
{
    return s_CfgLines.size();
}

// Find the UtlTest object that wraps the specified test id.
//
UtlTest* UtlTest::GetFromTestId(UINT32 testId)
{
    for (const auto & test : s_Tests)
    {
        if (test.second->GetTestId() == testId)
        {
            return test.second.get();
        }
    }

    return nullptr;
}

// Find the UtlTest object that wraps the specified Test pointer.
//
UtlTest* UtlTest::GetFromTestPtr(Test* test)
{
    if (s_Tests.count(test) > 0)
    {
        return s_Tests[test].get();
    }

    return nullptr;
}

// This function can be called from a UTL script to get the SmcEngine
// associated with this test
UtlSmcEngine* UtlTest::GetSmcEngine()
{
    SmcEngine* pSmcEngine = m_Test->GetSmcEngine();
    if (pSmcEngine == nullptr)
    {
        return nullptr;
    }

    UtlSmcEngine* pUtlSmcEngine = UtlSmcEngine::GetFromSmcEnginePtr(pSmcEngine);

    if (pUtlSmcEngine == nullptr)
    {
        UtlUtility::PyError("SmcEngine (SYS%d) does not have a UtlSmcEnginePtr", pSmcEngine->GetSysPipe());
    }

    return pUtlSmcEngine;
}

// This function associates a surface with the Test
void UtlTest::AddSurface(UtlSurface* pUtlSurface)
{
    if (GetSurface(pUtlSurface->GetName()) == nullptr)
    {
        DebugPrintf("Surface %s is being added in UTL Test\n", pUtlSurface->GetName().c_str());
        pUtlSurface->SetTest(this);
        m_Surfaces.push_back(pUtlSurface);
    }
    else
    {
        DebugPrintf("Surface %s already added in UTL Test\n", pUtlSurface->GetName().c_str()); 
    }
}

// This function removes a surface from the test list.
void UtlTest::RemoveSurface(UtlSurface* pUtlSurface)
{
    auto iter = find(m_Surfaces.begin(), m_Surfaces.end(), pUtlSurface);
    if (iter != m_Surfaces.end())
    {
        m_Surfaces.erase(iter);
    }
}

// This function returns the surface whose name matches with the string being passed
UtlSurface* UtlTest::GetSurface(const string &name)
{
    for (auto surface : m_Surfaces)
    {
        if (surface->GetName() == name)
        {
            return surface;
        }
    }
    return nullptr;
}

// This function can be called from a UTL script to get the surface (of the name passed)
// associated with this test.
UtlSurface* UtlTest::GetSurfaceByName(py::kwargs kwargs)
{
    string surfaceName = UtlUtility::GetRequiredKwarg<string>(kwargs, "name",
        "Test.GetSurfaceByName");

    UtlSurface* pSurface = GetSurface(surfaceName);

    // Do not error out if we cannot find a matching surface. Return nullptr as well. 
    // User might be calling this API to check if the surface exists

    return pSurface;
}

py::list UtlTest::GetSurfacesPy()
{
    return UtlUtility::ColwertPtrVectorToPyList(m_Surfaces);
}

// This function can be called from a UTL script to get a list strings that
// the user passed to this test by the -utl_test_data argument.
//
vector<string> UtlTest::GetUserTestData()
{
    vector<string> dataList;
    const ArgReader* params = m_Test->GetParam();

    if (params != nullptr)
    {
        UINT32 argCount = params->ParamPresent("-utl_test_data");

        for (UINT32 argIndex = 0; argIndex < argCount; ++argIndex)
        {
            dataList.push_back(params->ParamNStr("-utl_test_data", argIndex, 0));
        }
    }

    return dataList;
}

string UtlTest::GetTestArgs() const
{
    if (!m_TestConfig.empty())
    {
        return m_TestConfig;
    }

    // In case that the test created from Mdiag.
    //
    // Sequence ID starting from 1 but not 0.
    MASSERT(m_Test->GetSeqId() > 0);
    UINT32 index = m_Test->GetSeqId() - 1;

    if (index >= s_CfgLines.size())
    {
        UtlUtility::PyError("Index %d of test %d out of range CfgLines has. Valid value need be in range 0 - %d.",
            index,
            m_TestId,
            s_CfgLines.size());
    }
    return s_CfgLines[index];
}

string UtlTest::GetTracePath() const
{
    string tracePath = "";
    const ArgReader* params = m_Test->GetParam();

    if (params != nullptr && params->ParamPresent("-i"))
    {
        string traceName = params->ParamStr("-i");
        tracePath = Utility::StripFilename(traceName.c_str());
    }

    return tracePath;
}

vector<UINT08> UtlTest::ReadTraceFile(py::kwargs kwargs)
{
    string file = UtlUtility::GetRequiredKwarg<string>(
        kwargs, "file", "Test.ReadTraceFile");

    UtlGil::Release gil;

    TraceFileMgr* pTraceFileMgr = m_Test->GetTraceFileMgr();
    if (!pTraceFileMgr)
    {
        UtlUtility::PyError("TraceFileMgr not found in Test object.");
    }

    TraceFileMgr::Handle handle;
    RC rc = pTraceFileMgr->Open(file, &handle);
    UtlUtility::PyErrorRC(rc, "Can't open trace file '%s'.", file.c_str());

    size_t size = pTraceFileMgr->Size(handle);
    MASSERT(size > 0);
    vector<UINT08> data(size);
    rc = pTraceFileMgr->Read(&data[0], handle);
    pTraceFileMgr->Close(handle);
    UtlUtility::PyErrorRC(rc, "Failed to read 0x%llx bytes of data from trace file '%s'.",
        UINT64(size),
        file.c_str());

    return data;
}

string UtlTest::GetName()
{
    return m_Test->GetTestName();
}

bool UtlTest::IsAborted()
{
    return m_Test->AbortedTest();
}

string UtlTest::ToString() const
{
    return Utility::StrPrintf("utl.Test id:%u", m_TestId);
}

UtlTest* UtlTest::GetScriptTest()
{
    return Utl::Instance()->GetTestFromScriptId();
}
