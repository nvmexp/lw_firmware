/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "mdiag/advschd/policymn.h"
#include "mdiag/pm_lwchn.h"
#include "mdiag/resource/lwgpu/verif/InterruptChecker.h"
#include "mdiag/tests/gpu/lwgpu_ch_helper.h"

#include "mdiag/utl/pm_utl.h"
#include "mdiag/utl/utl.h"
#include "mdiag/utl/utlchannel.h"
#include "mdiag/utl/utltsg.h"
#include "mdiag/utl/utlusertest.h"
#include "mdiag/utl/utlutil.h"

extern const ParamDecl utl_params[] =
{
    // These Params are added because there are some arguments (e.g. -loc) that
    // needed to be supported by all mdiag tests.  Eventually, someone should
    // move the appropriate arguments from GpuChannelHelper::Params to
    // global_params in mdiag.cpp.
    PARAM_SUBDECL(GpuChannelHelper::Params),

    // Params from test base class
    PARAM_SUBDECL(test_params),

    STRING_PARAM("-test_path", "specifies the path to the module or package that contains UTL test class"),
    STRING_PARAM("-test_module", "specifies the name of module that contains the UTL test class"),
    STRING_PARAM("-class_name", "specifies the name of the UTL test class to instantiate"),

    SIMPLE_PARAM("-skip_intr_check", "Do NOT check the HW interrupts for a trace3d test.  Only use this option if you know what you're doing..."),
    UNSIGNED_PARAM("-zero_pad_log_strings", "Specifies the length to zero-pad hex numbers (explicitly starting with 0x) in error log strings"),
    SIMPLE_PARAM("-interrupt_check_ignore_order", "Check interrupt strings without considering the order."),
    SIMPLE_PARAM("-crcMissOK", "report missing crc/intr values as OK, so test will pass (log will show miss)"),
    STRING_PARAM("-interrupt_file", "Specify the interrupt file name used by the trace. Default name is 'test.int'"),
    STRING_PARAM("-interrupt_dir", "Specify the directory the interrupt file is located. Default is the test directory"),
    SIMPLE_PARAM("-enable_ecov_checking", "Enable ECover checking"),
    SIMPLE_PARAM("-enable_ecov_checking_only", "Enable ECover checking only if a valid ecov data file exists"),
    SIMPLE_PARAM("-nullEcover", "indicates meaningless ecoverage checking -- this happens sometimes when running test with certain priv registers"),
    { "-ecov_file", "t", ParamDecl::PARAM_MULTI_OK, 0, 0, "Specify the name of ecov checking file. Without this arg, mods will find test.ecov in test dir." },
    SIMPLE_PARAM("-allow_ecov_with_ctxswitch", "By default, the ecov checking will be skipped in \"-ctxswitch\" cases, and this argument will force the ecov checking even with \"-ctxswitch\""),
    SIMPLE_PARAM("-ignore_ecov_file_errors", "Ignore errors found in ECover file"),
    STRING_PARAM("-ignore_ecov_tags", "ignore these ecov tags or class names when doing ecov crc chain matching per ecov line. Format: -ignore_ecov_tags <className>:<className>:..."),
    { "-utl_test_script", "t", ParamDecl::PARAM_MULTI_OK, 0, 0, "Run a UTL script with scope limited to this test." },
    STRING_PARAM("-cpu_model_test", "Allows the user to specify a CPU test and an optional set of arguments for the CPU test." ),
    SIMPLE_PARAM("-disable_location_override_for_mdiag", "Disable the surface location override (e.g. -force_fb) for all mdiag surfaces."),
    { "-RawImageMode", "u", (ParamDecl::PARAM_ENFORCE_RANGE |
                           ParamDecl::GROUP_START | ParamDecl::GROUP_OVERRIDE_OK), RAWSTART, RAWEND, "Read or disable raw (uncompressed) framebuffer images" },
    { "-RawImagesOff", (const char*)RAWOFF,    ParamDecl::GROUP_MEMBER | ParamDecl::GROUP_MEMBER_UNSIGNED, 0, 0, "Disable raw (uncompressed) framebuffer images"},
    { "-RawImages",    (const char*)RAWON,     ParamDecl::GROUP_MEMBER | ParamDecl::GROUP_MEMBER_UNSIGNED, 0, 0, "Read raw (uncompressed) framebuffer images"},
    SIMPLE_PARAM("-using_external_memory", "Use external memory"),
    UNSIGNED64_PARAM("-va_range_limit", "Specifies the limit for virtual address range."),
    STRING_PARAM("-smc_eng_name", "target SMC engine for this test, exclusive with -smc_engine_label."),
    STRING_PARAM("-smc_engine_label", "target SMC engine for this test, exclusive with -smc_eng_name."),
    STRING_PARAM("-smc_mem", "SMC Partition specification Hopper onwards. Exclusive with -smc_engine_label"),

    LAST_PARAM
};

map<PyObject*, UtlUserTest*> UtlUserTest::s_UserTestsMap;

namespace
{
    // Temporary location to pass UtlUserTest pointer to UtlUserTestPy when
    // creating a new test
    UtlUserTest* s_pNewTest = nullptr;
}

void UtlUserTestPy::Register(py::module module)
{
    py::class_<UtlUserTestPy> utlUserTest(module, "UserTest");
    utlUserTest.def(py::init<>());
    utlUserTest.def_readwrite("status", &UtlUserTestPy::m_Status);
    utlUserTest.def("GetUserTestData", &UtlUserTestPy::GetUserTestData,
        "If there are any -utl_test_data arguments specified for this test, this function returns a list of the string values passed to every such argument.  If there are no -utl_test_data arguments passed for this test, then an empty list is returned.");
}

UtlUserTestPy::UtlUserTestPy
(
)
{
    MASSERT(s_pNewTest != nullptr);
    m_pUtlUserTest = s_pNewTest;
    s_pNewTest = nullptr;
}

// This function can be called from a UTL script to get a list strings that
// the user passed to this test by the -utl_test_data argument.
//
vector<string> UtlUserTestPy::GetUserTestData()
{
    vector<string> dataList;
    const ArgReader* params = m_pUtlUserTest->GetParam();

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

UtlUserTest::UtlUserTest()
    : Test()
{
    m_LwRm = LwRmPtr().Get();
    m_GpuResource = LWGpuResource::GetGpuByDeviceInstance(0);
}

UtlUserTest::~UtlUserTest()
{
    m_EcovVerifier.reset(0);
}

Test *UtlUserTest::Factory(ArgDatabase* args)
{
    unique_ptr<ArgReader> reader = make_unique<ArgReader>(utl_params);

    if (!reader.get() || !reader->ParseArgs(args))
    {
        ErrPrintf("unable to parse utl test arguments.\n");
        return nullptr;
    }

    // The -test_path argument is optional.  If omitted, MODS will import
    // the module using the current Python sys.path.
    string testPath;
    if (reader->ParamPresent("-test_path") > 0)
    {
        testPath = reader->ParamStr("-test_path");
    }

    string moduleName;
    if (reader->ParamPresent("-test_module") > 0)
    {
        moduleName = reader->ParamStr("-test_module");
    }
    else
    {
        ErrPrintf("missing -test_module argument for utl test in .cfg file\n.");
        return nullptr;
    }

    string className;
    if (reader->ParamPresent("-class_name") > 0)
    {
        className = reader->ParamStr("-class_name");
    }
    else
    {
        ErrPrintf("missing -class_name argument for utl test in .cfg file\n.");
        return nullptr;
    }

    unique_ptr<UtlUserTest> test(new UtlUserTest());
    test->m_ClassName = className;
    test->m_Reader = move(reader);

    if (test->m_Reader->ParamPresent("-name"))
    {
        test->m_TestName = test->m_Reader->ParamStr("-name");
    }
    else
    {
        test->m_TestName = test->m_ClassName;
    }

    RC rc = test->ImportTestClass(testPath, moduleName);

    if (OK != rc)
    {
        return nullptr;
    }

    Test::IncreaseNumEcoverSupportedTests();

    PolicyManager *policyManager = PolicyManager::Instance();
    rc = policyManager->AddTest(new PmTest_Utl(policyManager, test.get()));
    if (OK != rc)
    {
        return nullptr;
    }

    return test.release();
}

// This function imports the module that contains the Python class that defines
// this UTL user test, then creates an instance of that test class.
RC UtlUserTest::ImportTestClass
(
    const string& testPath,
    const string& moduleName
)
{
    RC rc;
    DebugPrintf("UTL: Importing test class %s from module %s.\n",
        m_ClassName.c_str(), moduleName.c_str());

    if (!Utl::IsSupported())
    {
        ErrPrintf("UTL user tests are not supported on this platform.\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    // This function might be called from the TestDir object instead of the
    // UTL object, so the GIL might not be owned by the current thread.
    // Acquire it here just in case.
    UtlGil::Acquire gil;

    try
    {
        // Save the sys.path list so that it can be restored after the user
        // test class is imported.
        UtlUtility::SysPathCheckpoint checkPoint;

        // If a test path was specified, temporarily overrwrite sys.path
        // so that the correct module gets imported.
        if (!testPath.empty())
        {
            string cmd = "import sys\n";
            cmd += Utility::StrPrintf("sys.path.insert(0, \"%s\")",
                testPath.c_str());
            py::exec(cmd.c_str());
        }

        DebugPrintf("UTL: Importing user test module %s.\n",
            moduleName.c_str());
        py::module testModule = py::module::import(moduleName.c_str());
        py::dict moduleDict = testModule.attr("__dict__");

        DebugPrintf("UTL: Instantiating test class %s.\n", m_ClassName.c_str());
        MASSERT(s_pNewTest == nullptr);
        s_pNewTest = this;
        string pythonCmd = Utility::StrPrintf("%s()", m_ClassName.c_str());
        m_PythonTest = py::eval(pythonCmd.c_str(), py::globals(), moduleDict);

        MASSERT(s_UserTestsMap.find(m_PythonTest.ptr()) == s_UserTestsMap.end());
        s_UserTestsMap[m_PythonTest.ptr()] = this;
    }

    // The Python interpreter can only return errors via exception.
    catch (const py::error_already_set& e)
    {
        UtlUtility::HandlePythonException(e.what());
        return RC::SOFTWARE_ERROR;
    }

    return rc;
}

int UtlUserTest::Setup()
{
    bool isCtxSwitchTest = GpuChannelHelper::IsCtxswEnabledByCmdline(GetParam());
    RC rc = SetupEcov(&m_EcovVerifier, GetParam(), isCtxSwitchTest, m_ClassName, "");
    if (OK != rc)
    {
        ErrPrintf("Failed to set up ecover for UTL user test.\n");
        return 0;
    }

    if (m_EcovVerifier.get())
    {
        LWGpuResource* lwgpu = LWGpuResource::FindFirstResource();
        MASSERT(lwgpu);
        m_EcovVerifier->SetGpuResourceParams(lwgpu->GetLwgpuArgs()); 
    }

    string smcEngineName = ParseSmcEngArg();
    if (!smcEngineName.empty() && (m_GpuResource != nullptr))
    {
        m_SmcEngine = m_GpuResource->GetSmcEngine(smcEngineName);
    }

    UINT32 paramCount = m_Reader->ParamPresent("-utl_test_script");

    for (UINT32 i = 0; i < paramCount; ++i)
    {
        string scriptParam = m_Reader->ParamNStr("-utl_test_script", i, 0);
        if (OK != Utl::Instance()->ReadTestSpecificScript(this, scriptParam))
        {
            return 0;
        }
    }

    // This function might be called from the TestDir object instead of the
    // UTL object, so the GIL might not be owned by the current thread.
    // Acquire it here just in case.
    UtlGil::Acquire gil;

    if (m_Reader->ParamPresent("-interrupt_file") > 0)
    {
        if (m_Reader->ParamPresent("-interrupt_dir") == 0)
        {
            ErrPrintf("the -interrupt_dir argument must also be specified if the -interrupt_file argument is specified for a UTL user test.\n");
            return 0;
        }

        m_CheckForInterrupts = true;
    }

    if (m_CheckForInterrupts)
    {
        RC rc = ErrorLogger::StartingTest();

        if (rc != OK)
        {
            ErrPrintf("unable to initialize the interrupt checker: %s\n",
                rc.Message());
            return 0;
        }
    }

    try
    {
        if (!py::hasattr(m_PythonTest, "Setup"))
        {
            UtlUtility::PyError(
                "UTL user test class %s does not define a function named Setup.",
                m_ClassName.c_str());
        }

        DebugPrintf("UTL: calling m_PythonTest Setup()\n");
        m_PythonTest.attr("Setup")();
        Test::TestStatus status =
            m_PythonTest.attr("status").cast<Test::TestStatus>();

        DebugPrintf("UTL: UtlUserTest::Setup() status = %d.\n", status);
        SetStatus(status);
    }

    // The Python interpreter can only return errors via exception.
    catch (const py::error_already_set& e)
    {
        CleanUp();
        UtlUtility::HandlePythonException(e.what());
        return 0;
    }

    return 1;
}

void UtlUserTest::Run()
{
    PolicyManager *policyManager = PolicyManager::Instance();
    if (policyManager->IsInitialized())
    {
        if (OK != StartTestInPolicyManager())
        {
            ErrPrintf("could not start UTL user test in policy manager.\n");
            SetStatus(Test::TEST_FAILED);
            return;
        }
    }

    {
        // This function might be called from the TestDir object instead of the
        // UTL object, so the GIL might not be owned by the current thread.
        // Acquire it here just in case.
        UtlGil::Acquire gil;

        try
        {
            if (!py::hasattr(m_PythonTest, "Run"))
            {
                UtlUtility::PyError(
                    "UTL user test class %s does not define a function named Run.",
                    m_ClassName.c_str());
            }

            DebugPrintf("UTL: calling m_PythonTest Run()\n");
            m_PythonTest.attr("Run")();

            Test::TestStatus status =
                m_PythonTest.attr("status").cast<Test::TestStatus>();
            DebugPrintf("UTL: UtlUserTest::Run() status = %d.\n", status);
            SetStatus(status);
        }

        // The Python interpreter can only return errors via exception.
        catch (const py::error_already_set& e)
        {
            SetStatus(Test::TEST_NO_RESOURCES);
            CleanUp();
            UtlUtility::HandlePythonException(e.what());
        }
    }

    if (policyManager->IsInitialized())
    {
        if (OK != policyManager->EndTest(policyManager->GetTestFromUniquePtr(this)))
        {
            ErrPrintf("policy manager failed to cleanup properly after UTL user test end.\n");
            SetStatus(Test::TEST_FAILED);
            return;
        }
    }

    Platform::ProcessPendingInterrupts();

    if (m_CheckForInterrupts)
    {
        DebugPrintf("UTL: checking for interrupts\n");
        CheckForInterrupts();

        RC rc = ErrorLogger::TestCompleted();
        if (rc != OK)
        {
            ErrPrintf("problem found with the interrupt checker at the end of the test: %s\n",
                rc.Message());
            SetStatus(Test::TEST_FAILED);
        }
    }

    if (m_EcovVerifier.get())
    {
        LWGpuResource* lwgpu = LWGpuResource::FindFirstResource();
        bool isCtxSwitchTest = GpuChannelHelper::IsCtxswEnabledByCmdline(GetParam());
        Test::TestStatus status = GetStatus();
        VerifyEcov(m_EcovVerifier.get(), GetParam(), lwgpu, isCtxSwitchTest, &status);
        SetStatus(status);
        m_EcovVerifier.reset(0);
    }
}

RC UtlUserTest::StartTestInPolicyManager()
{
    RC rc;
    PolicyManager *policyManager = PolicyManager::Instance();
    PmTest* pmTest = policyManager->GetTestFromUniquePtr(this);
    MASSERT(pmTest != nullptr);

    CHECK_RC(policyManager->StartTest(pmTest));

    return rc;
}

RC UtlUserTest::AddChannelToPolicyManager(LWGpuChannel* lwgpuChannel)
{
    RC rc;
    PolicyManager *policyManager = PolicyManager::Instance();
    if (policyManager->IsInitialized())
    {
        PmTest *pmTest = policyManager->GetTestFromUniquePtr(this);
        unique_ptr<PmChannel_LwGpu> pmChannel = make_unique<PmChannel_LwGpu>
        (
            policyManager,
            pmTest,
            GetGpuDevice(),
            lwgpuChannel->GetName(),
            lwgpuChannel,
            lwgpuChannel->GetLWGpuTsg(),
            lwgpuChannel->GetLwRmPtr()
        );
        DebugPrintf("UTL: adding user channel %s to policy manager.\n",
            lwgpuChannel->GetName().c_str());
        CHECK_RC(policyManager->AddChannel(pmChannel.release()));
    }
    return rc;
}

void UtlUserTest::CleanUp()
{
    // This function might be called from the TestDir object instead of the
    // UTL object, so the GIL might not be owned by the current thread.
    // Acquire it here just in case.
    UtlGil::Acquire gil;

    m_PythonTest.release();
}

void UtlUserTest::CheckForInterrupts()
{
    InterruptChecker checker;
    CrcProfile profile(CrcProfile::NORMAL);
    ICheckInfo info(getStateReport());

    profile.SetFileName(m_Reader->ParamStr("-interrupt_file"));
    checker.StandaloneSetup(m_Reader.get(), &profile);
    TestEnums::TEST_STATUS status = checker.Check(&info);

    // Set the interrupt status, but only if it doesn't override a previous
    // failing status.
    if (GetStatus() == TEST_SUCCEEDED)
    {
        SetStatus(status);
    }
}

void UtlUserTest::AddSurface(const string& name, UtlSurface* surface)
{
    MASSERT(m_Surfaces.find(name) == m_Surfaces.end());
    m_Surfaces[name] = surface;
}

UtlSurface* UtlUserTest::GetSurface(const string& name)
{
    auto iter = m_Surfaces.find(name);
    if (iter != m_Surfaces.end())
    {
        return iter->second;
    }
    return nullptr;
}

void UtlUserTest::AddTsg(const string& name, UtlTsg* tsg)
{
    MASSERT(m_Tsgs.find(name) == m_Tsgs.end());
    m_Tsgs[name] = tsg;
    Utl::Instance()->AddTestTsg(this, tsg->GetLWGpuTsg());
}

UtlTsg* UtlUserTest::GetTsg(const string& name)
{
    auto iter = m_Tsgs.find(name);
    if (iter != m_Tsgs.end())
    {
        return iter->second;
    }
    return nullptr;
}

void UtlUserTest::AddChannel(const string& name, UtlChannel* channel)
{
    MASSERT(m_Channels.find(name) == m_Channels.end());
    m_Channels[name] = channel;

    Utl::Instance()->AddTestChannel(this, channel->GetLwGpuChannel(),
        channel->GetName(), nullptr);
    AddChannelToPolicyManager(channel->GetLwGpuChannel());
}

UtlChannel* UtlUserTest::GetChannel(const string& name)
{
    auto iter = m_Channels.find(name);
    if (iter != m_Channels.end())
    {
        return iter->second;
    }
    return nullptr;
}

UtlUserTest* UtlUserTest::GetTestFromPythonObject(py::object userTestObj)
{
    UtlUserTest* userTest = nullptr;

    auto iter = s_UserTestsMap.find(userTestObj.ptr());
    if (iter != s_UserTestsMap.end())
    {
        userTest = iter->second;
    }

    return userTest;
}

void UtlUserTest::FreePythonObjects()
{
    for (auto iter : s_UserTestsMap)
    {
        iter.second->CleanUp();
    }
}
