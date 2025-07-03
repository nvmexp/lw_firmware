/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2022 by LWPU Corporation.  All 
 * rights reserved.  All information contained herein is proprietary and 
 * confidential to LWPU Corporation.  Any use, reproduction, or disclosure 
 * without the written permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first.
#include "stdtest.h"

#include <list>
#include <regex>

#include "test.h"

#include "core/include/cmdline.h"
#include "core/include/platform.h"
#include "gpu/utility/ecov_verifier.h"
#include "testdir.h"
#include "test_state_report.h"

int Test::next_sequential_id = 1;
UINT32 Test::s_NumEcoverSupportedTests = 0;
UINT32 Test::s_SmcEngineTraceIndex = 0;
bool Test::s_IsSmcNamePresent = false;

static list<Test*> all_tests;

int Test::global_values[] = {0, 0, 0};
map <UINT32, Test *> Test::s_ThreadId2Test;

const ParamDecl Test::Params[] = {
  UNSIGNED_PARAM("-usedev",  "Use the specified device instance for the test"),
  SIMPLE_PARAM("-multi_gpu", "init for multiple GPUs"),
  STRING_PARAM("-name", "set the name for the test"),
  STRING_PARAM("-utl_test_data", "passes a string that can be retrieved from a utl.Test object in a UTL script"),
  LAST_PARAM
};

Test::Test()
{
  this->status = TEST_NOT_STARTED;
  this->sequential_id = next_sequential_id++;
  this->stateReport = new TestStateReport;
  istateReport = static_cast<ITestStateReport*>(stateReport);
  if (this->stateReport == 0)
  {
    this->stateReport = &(TestStateReport::s_defaultStateReport);
  }
  all_tests.push_back(this);
  start_time = end_time = 0;
}

Test::~Test(void)
{
    all_tests.remove(this);
    if (stateReport != &(TestStateReport::s_defaultStateReport)) // pointer comparison; are they the same object?
    {
        delete stateReport;
    }

    auto threadId2TestItr = find_if(s_ThreadId2Test.begin(), s_ThreadId2Test.end(),
        [&](const pair<UINT32, Test*>& entry)
        {
            return entry.second == this;
        }
    );

    if (threadId2TestItr != s_ThreadId2Test.end())
    {
        s_ThreadId2Test.erase(threadId2TestItr);
    }
}

void Test::Yield(unsigned min_delay_in_usec /*= 0*/)
{
  LWGpuResource::GetTestDirectory()->Yield(min_delay_in_usec);
}

void
Test::FailAllTests(const char *msg)
{
    if (!msg) msg = "Test::FailAllTests(0)";
    ErrPrintf("%s\n", msg);
    for( list<Test*>::const_iterator
            iter = all_tests.begin();
            iter != all_tests.end();
            ++iter)
    {
        (*iter)->SetStatus(Test::TEST_FAILED);
        (*iter)->getStateReport()->runFailed(msg);
    }

}

/* virtual */ int Test::Setup(int stage) { return Setup(); }
/* virtual */ int Test::NumSetupStages() { return 1; }
/* virtual */ void Test::CleanUp(UINT32 stage) { return CleanUp(); }
/* virtual */ void Test::BeforeCleanup() {}
/* virtual */ UINT32 Test::NumCleanUpStages() const { return 1; }

void Test::PostRun()
{
}

// Return the name of the test - used for informational prints
/* virtual */const char* Test::GetTestName()
{
    return "Test";
}

void Test::EnableEcoverDumpInSim(bool enabled)
{
    if (Platform::GetSimulationMode() == Platform::Fmodel)
    {
        Platform::EscapeWrite("CHECK_ECOVER", 0, 4,
                enabled ? ENABLE_ECOV_DUMPING : DISABLE_ECOV_DUMPING);
    }
}

/*virtual*/SmcEngine* Test::GetSmcEngine()
{
    return nullptr;
}

/*virtual*/ LwRm* Test::GetLwRmPtr()
{
    return nullptr;
}

void Test::SetThreadId2TestMap(UINT32 threadId)
{
    s_ThreadId2Test[threadId] = this;
}

//------------------------------------------------------------------------------
//! \brief Find the test that is running in the thread.
//! \return pointer to the test if the test was found, NULL if not found
Test *Test::FindTestForThread(UINT32 threadId)
{
    if (s_ThreadId2Test.count(threadId))
    {
        return s_ThreadId2Test[threadId];
    }

    return nullptr;
}

Test *Test::GetFirstTestFromMap()
{
    if (!s_ThreadId2Test.empty())
    {
        return s_ThreadId2Test.begin()->second;
    }

    return nullptr;
}

RC Test::SetupEcov
(
    unique_ptr<EcovVerifier>* pEcovVerifier,
    const ArgReader* params,
    bool isCtxSwitchTest,
    const string& testName,
    const string& testChipName
)
{
    RC rc;

    if (Platform::GetSimulationMode() == Platform::Fmodel)
    {
        // Ecov dumping & checking are only enabled when:
        // 1. Not explict disalbed
        // 2.1 "-enable_ecov_checking" is used or
        // 2.2 "-enable_ecov_checking_only" is used, and there's a valid ecov
        //     data file in test directory
        bool bSetupEcov =
            (params->ParamPresent("-nullEcover") == 0) &&
            (params->ParamPresent("-enable_ecov_checking") > 0 ||
            params->ParamPresent("-enable_ecov_checking_only") > 0);

        if (bSetupEcov)
        {
            unique_ptr<EcovVerifier> ecovVerifier = make_unique<EcovVerifier>(params);
            ecovVerifier->SetTestName(testName);
            ecovVerifier->SetTestChipName(testChipName);

            if (params->ParamStr("-ecov_file") > 0)
            {
                UINT32 paramCount = params->ParamPresent("-ecov_file");
                for (UINT32 i = 0; i < paramCount; ++i)
                {
                    ecovVerifier->SetupEcovFile(params->ParamNStr("-ecov_file", i, 0));
                }
            }

            if ((params->ParamPresent("-enable_ecov_checking_only") > 0) &&
                (params->ParamPresent("-enable_ecov_checking") == 0 ))
            {
                const EventCover::Status parseStatus = ecovVerifier->ParseEcovFiles();
                if (parseStatus == EventCover::Status::False)
                {
                    ecovVerifier.reset(0);
                }
                else if (parseStatus == EventCover::Status::Error)
                {
                    ErrPrintf("Failed to parse ecover file\n");
                    return RC::CANNOT_PARSE_FILE;
                }
            }
            *pEcovVerifier = move(ecovVerifier); 
        }

        static bool enableEcovDumping = false;
        // Ecover dump is gpu global and is a PF thing hence VF should not 
        // enable or disable ecover dumping
        if (!Platform::IsVirtFunMode())
        {
            if (!isCtxSwitchTest)
            {
                enableEcovDumping = (*pEcovVerifier).get() ? true : false;
            }
            else if ((*pEcovVerifier).get())
            {
                // Once ecov dumping is enabled in fmodel side,
                // keep dumping until send "LwrrentTestEnded" to ask fmodel write out ecov data
                enableEcovDumping = true;
            }
            Test::EnableEcoverDumpInSim(enableEcovDumping);
        }
    }

    return rc;
}

void Test::VerifyEcov
(
    EcovVerifier* pEcovVerifier,
    const ArgReader* params,
    const LWGpuResource* lwgpu,
    bool isCtxSwitchTest,
    TestStatus* status
)
{
    LWGpuContexScheduler::Pointer lwgpuSked = lwgpu->GetContextScheduler();
    if (lwgpuSked->SyncAllRegisteredCtx(SYNCPOINT("Before dumping ECOV data")) != OK)
    {
        ErrPrintf("Register sync point failed before dumping ECOV data\n");
        *status = Test::TEST_FAILED;
    }

    pEcovVerifier->DumpEcovData(isCtxSwitchTest);

    const bool doNotCheckEcovInCtxsw =  isCtxSwitchTest &&
        Test::GetNumEcoverSupportedTests() > 1  &&
        !(params->ParamPresent("-ctxswSelf")) &&
        !(params->ParamPresent("-allow_ecov_with_ctxswitch"));

    if (doNotCheckEcovInCtxsw)
    {
        InfoPrintf("Ecover checking skipped due to context switching. -ctxswSelf/-allow_ecov_with_ctxswitch is needed\n");
    }
    else if (*status == Test::TEST_SUCCEEDED)
    {
        EcovVerifier::TEST_STATUS ecov_status = pEcovVerifier->VerifyECov();
        switch (ecov_status)
        {
            case EcovVerifier::TEST_SUCCEEDED:
                *status = Test::TEST_SUCCEEDED;
                break;
            case EcovVerifier::TEST_FAILED_ECOV:
                *status = Test::TEST_FAILED_ECOV;
                break;
            case EcovVerifier::TEST_ECOV_NON_EXISTANT:
            default:
                *status = Test::TEST_ECOV_NON_EXISTANT;
        }
    }
    else
    {
        InfoPrintf("Ecov checking skipped since test failed\n");
    }
}

string Test::ParseSmcEngArg()
{
    string smcEngineName;
    const ArgReader* params = GetParam();
    MASSERT(params != nullptr);

    //To check if all traces have -smc_eng_name.
    if ((s_SmcEngineTraceIndex == 0) && (params->ParamPresent("-smc_eng_name")))
    {
        s_IsSmcNamePresent = true;
    }

    std::string modsSmcEngRegex(MODS_SMCENG_NAME);
    modsSmcEngRegex += "[0-9]+";
    smcEngineName = params->ParamStr("-smc_eng_name", UNSPECIFIED_SMC_ENGINE);

    if (params->ParamPresent("-smc_engine_label"))
    {
        //Assign syspipe values as smc_engine_label. deprecated from Hopper
        smcEngineName = params->ParamStr("-smc_engine_label", UNSPECIFIED_SMC_ENGINE);
    }
    else
    {
        //Check if -smc_eng_name is present/absent in all the traces
        if ((s_SmcEngineTraceIndex > 0) && ((params->ParamPresent("-smc_eng_name") && (!s_IsSmcNamePresent)) ||
            ((params->ParamPresent("-smc_eng_name") == 0) && (s_IsSmcNamePresent))))
        {
            ErrPrintf("SMC partitioning API error: '-smc_eng_name' should be either present/absent in all the traces.\n");
            MASSERT(0);
        }
        //Check if MODS internal name is not provided using -smc_eng_name
        else if (std::regex_match(smcEngineName.c_str(), std::regex(modsSmcEngRegex)))
        {
            ErrPrintf("SMC partitioning API error: invalid '-smc_eng_name' used: %s. MODS internal name cannot be used.\n",
                      params->ParamStr("-smc_eng_name", NULL));
            MASSERT(0);
        }
        //If -smc_eng_name is not present but -smc_mem is present, then assign the engine names in the order in which they are filled in the vector SmcEngine::s_SmcEngineNames
        else if ((params->ParamPresent("-smc_eng_name") == 0) &&
            (params->ParamPresent("-smc_mem") > 0))
        {
            if (s_SmcEngineTraceIndex <= GetSmcEngine()->GetSmcEngineNames().size())
            {
                smcEngineName = GetSmcEngine()->GetSmcEngineNames()[s_SmcEngineTraceIndex];
                InfoPrintf("Implicit allocation of SmcEngines to traces since -smc_eng_name not specified\n");
            }
            else
            {
                ErrPrintf("SMC partitioning API error: Number of traces greater than number of SMC engines. There are %d traces and %d smc engines\n",
                          s_SmcEngineTraceIndex, GetSmcEngine()->GetSmcEngineNames().size());
                MASSERT(0);
            }
        }
        //Assign engine names if -smc_eng_name Hopper onwards
        else if (params->ParamPresent("-smc_eng_name"))
        {
            string SmcEngineNameInput;
            SmcEngineNameInput = params->ParamStr("-smc_eng_name", UNSPECIFIED_SMC_ENGINE);

            //Check if the engine name provided is valid and is present in the vector SmcEngine::s_SmcEngineNames
            auto search = find(GetSmcEngine()->GetSmcEngineNames().begin(), GetSmcEngine()->GetSmcEngineNames().end(), SmcEngineNameInput);
            if (search != GetSmcEngine()->GetSmcEngineNames().end())
            {
                smcEngineName = SmcEngineNameInput;
            }
            else
            {
                ErrPrintf("SMC partitioning API error: Invalid SMC Engine name given using '-smc_eng_name' : %s\n",
                          params->ParamStr("-smc_eng_name", NULL));
                MASSERT(0);
            }
        }
    }

    s_SmcEngineTraceIndex++;

    return smcEngineName;
}
