/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_pmufuzzer.cpp
//! \brief Fuzz PMU interface by performing positive & negative testings
//!

#include "gpu/tests/rmtest.h"
#include "gpu/tests/gputestc.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/channel.h"
#include "lwRmReg.h"
#include "lwRmApi.h"
#include "lwos.h"
#include "core/include/utility.h"
#include "core/utility/errloggr.h"
#include <stdio.h>
#include "class/cl85b6.h"  // GT212_SUBDEVICE_PMU
#include "ctrl/ctrl85b6.h" // GT212_SUBDEVICE_PMU CTRL
#include "gpu/perf/pmusub.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "rmpmucmdif.h"
#include "pmu/pmuseqinst.h"
#include "core/include/platform.h"
#include "core/include/xp.h"
#include "core/include/memcheck.h"
#include <stdio.h>
#include "random.h"
#include <cstddef>
//#include <unistd.h>
#include "gpu/tests/rm/pmu/pmuFuzzerParamGenerator.h"

#define ALL_TEST_CASES          0

class PMUfuzzTest : public RmTest
{
public:
    PMUfuzzTest();
    virtual ~PMUfuzzTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    SETGET_PROP(PmuUnitId, LwU32);
    SETGET_PROP(PmuCmdId, LwU32);
    SETGET_PROP(PmuSeed, LwU32);
    SETGET_PROP(PmuSecId, LwU32);
    SETGET_PROP(PmuTestCaseId, LwU32);
    SETGET_PROP(MaxIter, LwU32);

private:
    PMU                *m_pPmu;
    LwU32               m_pmuUcodeStateSaved;
    FLOAT64             m_TimeoutMs;
    GpuSubdevice       *m_Parent;
    Random              m_random;
    LwU32               m_InPayloadSize;
    LwU32               m_PmuUnitId;
    LwU32               m_PmuCmdId;
    LwU32               m_PmuSeed;
    LwU32               m_PmuSecId;
    LwU32               m_MaxIter;
    LwU32               m_PmuTestCaseId;
    Surface2D          *m_pMsgSurface;
    Memory::Location    m_SurfaceLoc;

    RC             SubmitPmuRPC(RM_PMU_RPC_HEADER  *pRpc,
                                LwU32               sizeRpc);
    void           loadCMDFromBinary(RM_FLCN_CMD_PMU *pCmd, LwU8 unitId, LwU8 count);
    void           saveCMDToBinary(RM_FLCN_CMD_PMU *pCmd, LwU8 unitId,
                                   LwU8 cmdId, LwU32 seqNum);
    RC             startFuzzing();
};

//! \brief PMUfuzzTest constructor
//!
//! Placeholder : doesn't do much, much funcationality in Setup()
//!
//! \sa Setup
//-----------------------------------------------------------------------------
PMUfuzzTest::PMUfuzzTest()
{
    SetName("PMUfuzzTest");
    m_pPmu = NULL;
}

//! \brief PMUfuzzTest  destructor
//!
//! Placeholder : doesn't do much, much funcationality in Cleanup()
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
PMUfuzzTest::~PMUfuzzTest()
{
}

//! \brief IsTestSupported(), Looks for whether test can execute in current elw.
//!
//! The test is basic PMU class: Check for Class availibility.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//-----------------------------------------------------------------------------
string PMUfuzzTest::IsTestSupported()
{
    // Returns true only if the class is supported.
    if(IsClassSupported(GT212_SUBDEVICE_PMU))
    {
        return RUN_RMTEST_TRUE;
    }
    else
    {
        Printf(Tee::PriHigh, "%d:PMUfuzzTest is NOT supported.\n", __LINE__);
    }

    return "GT212_SUBDEVICE_PMU class is not supported on current platform";
}

//! \brief Setup(): Generally used for any test level allocation
//!
//! Checking if the bootstrap rom image file is present, also obtaining
//! the instance of PMU class, through which all acces to PMU will be done.
//
//! \return RC::SOFTWARE_ERROR if the PMU bootstrap file is not found,
//! other corresponding RC for other failures
//! \sa Run()
//-----------------------------------------------------------------------------
RC PMUfuzzTest::Setup()
{
    RC     rc;

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    m_TimeoutMs = GetTestConfiguration()->TimeoutMs();
    m_Parent    = GetBoundGpuSubdevice();

    // Get PMU class instance
    rc = (GetBoundGpuSubdevice()->GetPmu(&m_pPmu));
    if (OK != rc)
    {
        Printf(Tee::PriHigh,"PMU not supported\n");
        CHECK_RC(rc);
    }

    //
    // get and save the current PMU ucode state.
    // We should restore the PMU ucode to this state
    // after our tests are done.
    //
    rc = m_pPmu->GetUcodeState(&m_pmuUcodeStateSaved);
    if (OK != rc)
    {
        Printf(Tee::PriHigh,
               "%d: Failed to get PMU ucode state\n",
                __LINE__);
        CHECK_RC(rc);
    }

    // Use the seed provided by the command line as input
    m_random.SeedRandom(m_PmuSeed);
    Printf(Tee::PriHigh,
            "Seed value for random number generator is %d: \n", m_PmuSeed);

    return rc;
}

//! \brief Run(): Used generally for placing all the testing stuff.
//!
//! Run() as said in Setup() has bootstrap and reset tests lwrrently, will
//! be adding more tests for command and msg queue testing.
//!
//! \return OK if the passed, specific RC if failed
//! \sa Setup()
//-----------------------------------------------------------------------------
RC PMUfuzzTest::Run()
{
    RC      rc;
    LwU32   lwrPmuUcodeState;

    Printf(Tee::PriHigh,
               "%s:%d - Starting PMU Fuzzer test\n",
                 __FUNCTION__, __LINE__);
    //
    // We're going to be generating error interrupts.
    // Don't want to choke on those.
    //
    CHECK_RC(ErrorLogger::StartingTest());
    //ErrorLogger::IgnoreErrorsForThisTest();

    // Get current ucode state and startFuzzing
    CHECK_RC(m_pPmu->GetUcodeState(&lwrPmuUcodeState));
    if (lwrPmuUcodeState == LW85B6_CTRL_PMU_UCODE_STATE_READY)
    {
        CHECK_RC(startFuzzing());
    }
    else
    {
        Printf(Tee::PriHigh,"%d:PMUfuzzTest:: Bug 460276: PMU is not bootstrapped.\n", __LINE__);
    }

    return rc;
}

//! \brief Cleanup()
//!
//! As everything done in Run (lwrrently) this cleanup acts again like a
//! placeholder except few buffer free up we might need allocated in Setup
//! \sa Run()
//-----------------------------------------------------------------------------
RC PMUfuzzTest::Cleanup()
{
    ErrorLogger::TestCompleted();
    return OK;
}

//-----------------------------------------------------------------------------
// Private Member Functions
//-----------------------------------------------------------------------------
void PMUfuzzTest::loadCMDFromBinary(RM_FLCN_CMD_PMU *pCmd, LwU8 unitId, LwU8 count)
{
    char  filename[128];
    FILE *fp = NULL;

    // Get the filename from unitId + cmdId + seqNum
    sprintf(filename, "CMDDump/pCmdDump_%d_%d", unitId, count);
    //sprintf(filename, "CMDDump/lwrPmuCmd_%d_%d_274", unitId, count);

    fp = fopen(filename, "rb");
    if (fp != NULL)
    {
        size_t ret = fread(pCmd, sizeof(RM_FLCN_CMD_PMU), 1, fp);
        if (ret != 1)
        {
            Printf(Tee::PriHigh, "Error: %d:PMUfuzzTest::%s: Failed to save to file\n",
                __LINE__,__FUNCTION__);
        }
        fclose(fp);
        fp = NULL;
    }
}

void PMUfuzzTest::saveCMDToBinary(RM_FLCN_CMD_PMU *pCmd, LwU8 unitId, LwU8 cmdId,
        LwU32 seqNum)
{
    char  filename[128];
    FILE *fp = NULL;

    // Set the filename from unitId + cmdId + seqNum
    sprintf(filename, "lwrPmuCmd_%d_%d_%d", unitId, cmdId, seqNum);

    fp = fopen(filename, "wb+");
    if (fp != NULL)
    {
        size_t ret = fwrite(pCmd, sizeof(RM_FLCN_CMD_PMU), 1, fp);
        if (ret != 1)
        {
            Printf(Tee::PriHigh, "Error: %d:PMUfuzzTest::%s: Failed to save to file\n",
                __LINE__,__FUNCTION__);
        }
        fclose(fp);
        fp = NULL;
    }
}

RC PMUfuzzTest::startFuzzing()
{
    RC  rc = OK;
    PmuFuzzerParamGenerator  pmuFuzzerParamGenerator;

    for (LwU32 i = 0; i < m_MaxIter; i++)
    {
        RM_PMU_RPC_HEADER  *pRpc = NULL;
        LwU32               sizeRpc;
        LwU8                j;

        // if the test case id is not provided
        if (m_PmuTestCaseId == ALL_TEST_CASES)
        {
            // run all test cases if the test unit id is not provided either
            if (m_PmuUnitId == 0)
            {
                TestCaseFunction *pTestCaseFunctions =
                    pmuFuzzerParamGenerator.getAllTestCaseFunctions();

                for (j = 0; j < pmuFuzzerParamGenerator.testCaseIdMap.size(); j++)
                {
                    pRpc = (*pTestCaseFunctions[j])(pRpc, &sizeRpc);
                    CHECK_RC(SubmitPmuRPC(pRpc, sizeRpc));
                    delete[] pRpc;
                }
                delete[] pTestCaseFunctions;
            }
            else
            {
                pRpc = pmuFuzzerParamGenerator.randomFuzzing(
                    pRpc, &sizeRpc, m_PmuUnitId, m_PmuSecId, m_pPmu, m_random);

                CHECK_RC(SubmitPmuRPC(pRpc, sizeRpc));
                delete[] pRpc;
            }
        }
        else
        {
            TestCaseFunction pTestCaseFunction =
                pmuFuzzerParamGenerator.findTestCaseFunction(m_PmuTestCaseId);

            if (pTestCaseFunction == NULL)
            {
                pRpc = pmuFuzzerParamGenerator.randomFuzzing(
                    pRpc, &sizeRpc, m_PmuUnitId, m_PmuSecId, m_pPmu, m_random);
            }
            else
            {
                pRpc = (*pTestCaseFunction)(pRpc, &sizeRpc);
            }
            CHECK_RC(SubmitPmuRPC(pRpc, sizeRpc));
            delete[] pRpc;
        }

        m_random.SeedRandom(m_random.GetRandom());
    }

    return rc;
}

RC PMUfuzzTest::SubmitPmuRPC
(
    RM_PMU_RPC_HEADER  *pRpc,
    LwU32               sizeRpc
)
{
    RC                                  rc;
    LW85B6_CTRL_PMU_RPC_EXELWTE_PARAMS  pmuRpcCmd;
    RM_PMU_RPC_HEADER                  *pRpcHdr;

    pRpcHdr = (RM_PMU_RPC_HEADER *)pRpc;

    memset(&pmuRpcCmd, 0, sizeof(pmuRpcCmd));

    sizeRpc = LW_MIN(sizeRpc, sizeof(pmuRpcCmd.rpc));

    Printf(Tee::PriHigh, "%d:PMUfuzzTest::%s: Submitting PMU RPC (unitId:%d) (function:%d)\n",
           __LINE__, __FUNCTION__, pRpcHdr->unitId, pRpcHdr->function);

    // Submit the command.
    pmuRpcCmd.sizeScratch = 0;
    pmuRpcCmd.sizeRpc     = sizeRpc;
    pmuRpcCmd.queueId     = RM_PMU_COMMAND_QUEUE_LPQ;
    // LW_RM_PMU_RPC_FLAGS_TYPE_SYNC is equal to 0 and oclwpies 1:0 bits.
    // This is a temporary fix to make RM send RPC commands to PMU.
    pRpc->flags           = 0x00;
    memcpy(pmuRpcCmd.rpc, pRpc, sizeRpc);

    rc = LwRmPtr()->Control(m_pPmu->GetHandle(),
                            LW85B6_CTRL_CMD_PMU_RPC_EXELWTE_SYNC,
                            &pmuRpcCmd,
                            sizeof(pmuRpcCmd));

    // Response message received, print out the details
    Printf(Tee::PriHigh,
           "%d:PMUfuzzTest::%s: PMU RPC completed, rc = %s\n",
           __LINE__, __FUNCTION__, rc.Message());
    return rc;
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ PMUfuzzTest
//! object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(PMUfuzzTest, RmTest,
"Test new class addition, class named PMUfuzz, class used specifically for PMU");

CLASS_PROP_READWRITE(PMUfuzzTest, PmuUnitId, LwU32,
                     "PMU unit (task) to be fuzzed");

CLASS_PROP_READWRITE(PMUfuzzTest, PmuCmdId, LwU32,
                     "PMU cmd to be fuzzed");

CLASS_PROP_READWRITE(PMUfuzzTest, PmuSeed, LwU32,
                     "PMU seed used for generating random numbers");

CLASS_PROP_READWRITE(PMUfuzzTest, PmuSecId, LwU32,
                     "PMU secondary cmd id to be fuzzed");

CLASS_PROP_READWRITE(PMUfuzzTest, PmuTestCaseId, LwU32,
                     "PMU test case id to be reproduced");

CLASS_PROP_READWRITE(PMUfuzzTest, MaxIter, LwU32,
                     "Maximum iteration to perform for random fuzzing");
