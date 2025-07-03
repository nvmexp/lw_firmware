/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_pmgr.cpp
//! \brief Runs different pmgr tests.
//!

#include <sstream>
#include <string>
#include "gpu/tests/rmtest.h"
#include "core/include/jscript.h"
#include "core/include/platform.h"
#include "lwRmApi.h"
#include "core/include/memcheck.h"
#include "rmt_pmgr.h"
#include "ctrl/ctrl2080/ctrl2080pmgr.h"
using namespace std;

#define OPTION_TEST_EXHAUSTIVE 0
#define OPTION_TEST_INCLUDE    1
#define OPTION_TEST_EXCLUDE    2

#define PMGRGENERICTESTPARAMS_INDEX_ILWALID 0xFFFFFFFF

class PmgrGenericTest: public RmTest
{
public:
    PmgrGenericTest();
    virtual ~PmgrGenericTest();
    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    // user supplied command-line option 0: exhaustive (default), 1: include, 2: exclude.
    SETGET_PROP(uiNumCmdLineOption, UINT32);

    // user supplied command-line set of tests to include or exclude.
    SETGET_PROP(sPmgrgenerictestLwstomList, string);

    // user supplied command-line option to skip further tests, upon detecting a failure.
    SETGET_PROP(uiSkipOnFirstFailure, UINT32);

private:
    UINT32 m_uiNumCmdLineOption;
    string m_sPmgrgenerictestLwstomList;
    UINT32 m_uiSkipOnFirstFailure;
    stringstream ssInputOptions;
    LW2080_CTRL_PMGR_GENERIC_TEST_PARAMS pmgrGenericTestParams;

    RC ParseCommandLineOptions();
};

//! \brief PmgrGenericTest constructor.
//!
//! Initializes some basic variables as default values in the resepctive tests.
//! \sa Setup
//-----------------------------------------------------------------------------
PmgrGenericTest::PmgrGenericTest()
{
    SetName("PmgrGenericTest");
}

//! \brief PmgrGenericTest destructor.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
PmgrGenericTest::~PmgrGenericTest()
{
    // Nothing to do here.
}

//! \brief PmgrGenericTest Cleanup.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
RC PmgrGenericTest::Cleanup()
{
    RC rc;
    return rc;
}

//! \brief IsSupported Function.
//!
//! The test is supported on HW.
//! \sa Setup
//-----------------------------------------------------------------------------
string PmgrGenericTest::IsTestSupported()
{
#if defined(DEBUG) || defined(DEVELOP) || defined(LW_VERIF_FEATURES)
    if (Platform::GetSimulationMode() != Platform::Hardware)
    {
        return "Pmgr Tests are supported only on HW";
    }

    return RUN_RMTEST_TRUE;
#else
    return "RM TEST Unsupported";
#endif
}

//! \brief Setup Function.
//!
//-----------------------------------------------------------------------------
RC PmgrGenericTest::Setup()
{
    RC rc;
    // Ask our base class(es) to setup internal state based on JS settings.
    CHECK_RC(InitFromJs());
    CHECK_RC(ParseCommandLineOptions());

    return rc;
}

//! \brief Run Function.
//!
//! Run function is the primary function which exelwtes the test i.e. PmgrGenericTest.
//! \return OK if the tests passed, specific RC if failed
//-----------------------------------------------------------------------------
RC PmgrGenericTest::Run()
{
    RC rc = OK;
    LwRmPtr pLwRm;
    LwRm::Handle hSubdevice = pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice());
    bool bIterate = true;
    bool bExclude = false;
    INT32 uiPmgrTest;

    INT32 i = -1;
    while (bIterate)
    {
        switch(m_uiNumCmdLineOption)
        {
            case OPTION_TEST_EXHAUSTIVE:
                i++;
                break;
            case OPTION_TEST_INCLUDE:
                if (!(ssInputOptions >> i))
                {
                    bIterate = false;
                }
                break;
            case OPTION_TEST_EXCLUDE:
                i++;
                bExclude = false;
                ssInputOptions.clear();
                ssInputOptions.seekg(0,  std::ios::beg);
                while (ssInputOptions >> uiPmgrTest)
                {
                    if (uiPmgrTest == i)
                        bExclude = true;
                }
                ssInputOptions.flush();
                break;
        }

        if (bIterate && !bExclude)
        {
            pmgrGenericTestParams.index = i;
            CHECK_RC(pLwRm->Control(hSubdevice,
                         LW2080_CTRL_CMD_PMGR_GENERIC_TEST,
                         (void*)&pmgrGenericTestParams,
                         sizeof(pmgrGenericTestParams)));

            if ((pmgrGenericTestParams.outStatus !=
                    LW2080_CTRL_PMGR_GENERIC_TEST_NOT_IMPLEMENTED) ||
                (m_uiNumCmdLineOption == OPTION_TEST_INCLUDE))
            {
               Printf(Tee::PriNormal, "Test#: %04d\t", i);
               Printf(Tee::PriNormal, "Status: %20s\t",
                  (pmgrGenericTestParams.outStatus ==
                      LW2080_CTRL_PMGR_GENERIC_TEST_SUCCESS ? "SUCCESS":
                   pmgrGenericTestParams.outStatus ==
                      LW2080_CTRL_PMGR_GENERIC_TEST_NOT_SUPPORTED ? "NOT_SUPPORTED":
                   pmgrGenericTestParams.outStatus ==
                      LW2080_CTRL_PMGR_GENERIC_TEST_NOT_IMPLEMENTED ? "NOT_IMPLEMENTED":
                   pmgrGenericTestParams.outStatus ==
                      LW2080_CTRL_PMGR_GENERIC_TEST_INSUFFICIENT_PRIVILEGE ?
                      "INSUFFICIENT PRIVILEDGE" : "ERROR_GENERIC"));
               if (pmgrGenericTestParams.outStatus ==
                   LW2080_CTRL_PMGR_GENERIC_TEST_INSUFFICIENT_PRIVILEGE)
               {
                   Printf(Tee::PriNormal, "%s \n",
                       "Failure Reason: insufficient priviledge to execute test. Please enable test mode");
               }
               else if (pmgrGenericTestParams.outStatus ==
                            LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC)
               {
                   Printf(Tee::PriNormal, "Test#: %04d\t", i);
                   if (pmgrGenericTestParams.outData == LW2080_CTRL_PMGR_TEST_STATUS(ADC_INIT, NON_ZERO_IIR_VALUE))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: non-zero IIR value post init and reset (ERROR: NON_ZERO_IIR_VALUE)");
                       Printf(Tee::PriNormal, "Observed IIR Value = %llu\n, Expected IIR Value = %llu\n",
                           LwU64_ALIGN32_VAL(&(pmgrGenericTestParams.observedVal)), LwU64_ALIGN32_VAL(&(pmgrGenericTestParams.expectedVal)));
                   }
                   else if (pmgrGenericTestParams.outData == LW2080_CTRL_PMGR_TEST_STATUS(ADC_INIT, NON_ZERO_ACC_LB_VALUE))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: non-zero acc LB value post init and reset (ERROR: NON_ZERO_ACC_LB_VALUE)");
                       Printf(Tee::PriNormal, "Observed ACC_LB Value = %llu\n, Expected ACC_LB Value = %llu\n",
                           LwU64_ALIGN32_VAL(&(pmgrGenericTestParams.observedVal)), LwU64_ALIGN32_VAL(&(pmgrGenericTestParams.expectedVal)));
                   }
                   else if (pmgrGenericTestParams.outData == LW2080_CTRL_PMGR_TEST_STATUS(ADC_INIT, NON_ZERO_ACC_UB_VALUE))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: non-zero acc UB value post init and reset (ERROR: NON_ZERO_ACC_UB_VALUE)");
                       Printf(Tee::PriNormal, "Observed ACC_UB Value = %llu\n, Expected ACC_UB Value = %llu\n",
                           LwU64_ALIGN32_VAL(&(pmgrGenericTestParams.observedVal)), LwU64_ALIGN32_VAL(&(pmgrGenericTestParams.expectedVal)));
                   }
                   else if (pmgrGenericTestParams.outData == LW2080_CTRL_PMGR_TEST_STATUS(ADC_INIT, NON_ZERO_ACC_SCNT_VALUE))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: non-zero acc SMPCNT value post init and reset (ERROR: NON_ZERO_ACC_SCNT_VALUE)");
                       Printf(Tee::PriNormal, "Observed ACC_SCNT Value = %llu\n, Expected ACC_SCNT Value = %llu\n",
                           LwU64_ALIGN32_VAL(&(pmgrGenericTestParams.observedVal)), LwU64_ALIGN32_VAL(&(pmgrGenericTestParams.expectedVal)));
                   }
                   else if (pmgrGenericTestParams.outData == LW2080_CTRL_PMGR_TEST_STATUS(ADC_INIT, NON_ZERO_MUL_VALUE))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: non-zero power value post init and reset (ERROR: NON_ZERO_MUL_VALUE)");
                       Printf(Tee::PriNormal, "Observed MUL Value = %llu\n, Expected MUL Value = %llu\n",
                           LwU64_ALIGN32_VAL(&(pmgrGenericTestParams.observedVal)), LwU64_ALIGN32_VAL(&(pmgrGenericTestParams.expectedVal)));
                   }
                   else if (pmgrGenericTestParams.outData == LW2080_CTRL_PMGR_TEST_STATUS(ADC_INIT, NON_ZERO_MUL_ACC_LB_VALUE))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: non-zero acc LB power value post init and reset (ERROR: NON_ZERO_MUL_ACC_LB_VALUE)");
                       Printf(Tee::PriNormal, "Observed MUL_ACC_LB Value = %llu\n, Expected MUL_ACC_LB Value = %llu\n",
                           LwU64_ALIGN32_VAL(&(pmgrGenericTestParams.observedVal)), LwU64_ALIGN32_VAL(&(pmgrGenericTestParams.expectedVal)));
                   }
                   else if (pmgrGenericTestParams.outData == LW2080_CTRL_PMGR_TEST_STATUS(ADC_INIT, NON_ZERO_MUL_ACC_UB_VALUE))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: non-zero acc UB power value post init and reset (ERROR: NON_ZERO_MUL_ACC_UB_VALUE)");
                       Printf(Tee::PriNormal, "Observed MUL_ACC_UB Value = %llu\n, Expected MUL_ACC_UB Value = %llu\n",
                           LwU64_ALIGN32_VAL(&(pmgrGenericTestParams.observedVal)), LwU64_ALIGN32_VAL(&(pmgrGenericTestParams.expectedVal)));
                   }
                   else if (pmgrGenericTestParams.outData == LW2080_CTRL_PMGR_TEST_STATUS(ADC_INIT, NON_ZERO_MUL_ACC_SCNT_VALUE))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: non-zero acc SMPCNT power value post init and reset (ERROR: NON_ZERO_MUL_ACC_SCNT_VALUE)");
                       Printf(Tee::PriNormal, "Observed MUL_ACC_SCNT Value = %llu\n, Expected MUL_ACC_SCNT Value = %llu\n",
                           LwU64_ALIGN32_VAL(&(pmgrGenericTestParams.observedVal)), LwU64_ALIGN32_VAL(&(pmgrGenericTestParams.expectedVal)));
                   }
                   else if (pmgrGenericTestParams.outData == LW2080_CTRL_PMGR_TEST_STATUS(ADC_CHECK, IIR_VALUE_MISMATCH))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: HW and SW IIR values mismatched (ERROR: IIR_VALUE_MISMATCH)");
                       Printf(Tee::PriNormal, "Hardware IIR Value = %llu\n, Software IIR Value = %llu\n",
                           LwU64_ALIGN32_VAL(&(pmgrGenericTestParams.observedVal)), LwU64_ALIGN32_VAL(&(pmgrGenericTestParams.expectedVal)));
                   }
                   else if (pmgrGenericTestParams.outData == LW2080_CTRL_PMGR_TEST_STATUS(ADC_CHECK, ACC_VALUE_MISMATCH))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: Acc HW and SW ACC values mismatched (ERROR: ACC_VALUE_MISMATCH)");
                       Printf(Tee::PriNormal, "Hardware ACC Value = %llu\n, Software ACC Value = %llu\n",
                           LwU64_ALIGN32_VAL(&(pmgrGenericTestParams.observedVal)), LwU64_ALIGN32_VAL(&(pmgrGenericTestParams.expectedVal)));
                   }
                   else if (pmgrGenericTestParams.outData == LW2080_CTRL_PMGR_TEST_STATUS(ADC_CHECK, MUL_VALUE_MISMATCH))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: HW and SW MUL values mismatched (ERROR: MUL_VALUE_MISMATCH)");
                       Printf(Tee::PriNormal, "Hardware MUL Value = %llu\n, Software MUL Value = %llu\n",
                           LwU64_ALIGN32_VAL(&(pmgrGenericTestParams.observedVal)), LwU64_ALIGN32_VAL(&(pmgrGenericTestParams.expectedVal)));
                   }
                   else if (pmgrGenericTestParams.outData == LW2080_CTRL_PMGR_TEST_STATUS(ADC_CHECK, MUL_ACC_VALUE_MISMATCH))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: Acc HW and SW MUL values mismatched (ERROR: MUL_ACC_VALUE_MISMATCH)");
                       Printf(Tee::PriNormal, "Hardware MUL_ACC Value = %llu\n, Software MUL_ACC Value = %llu\n",
                           LwU64_ALIGN32_VAL(&(pmgrGenericTestParams.observedVal)), LwU64_ALIGN32_VAL(&(pmgrGenericTestParams.expectedVal)));
                   }
                   else if (pmgrGenericTestParams.outData == LW2080_CTRL_PMGR_TEST_STATUS(HI_OFFSET_MAX_CHECK, HI_OFFSET_A_FAILURE))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: Unexpected value of HI_OFFSET_A (ERROR: HI_OFFSET_A_FAILURE)");
                   }
                   else if (pmgrGenericTestParams.outData == LW2080_CTRL_PMGR_TEST_STATUS(HI_OFFSET_MAX_CHECK, HI_OFFSET_B_FAILURE))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: Unexpected value of HI_OFFSET_B (ERROR: HI_OFFSET_B_FAILURE)");
                   }
                   else if (pmgrGenericTestParams.outData == LW2080_CTRL_PMGR_TEST_STATUS(HI_OFFSET_MAX_CHECK, HI_OFFSET_C_FAILURE))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: Unexpected value of HI_OFFSET_C (ERROR: HI_OFFSET_C_FAILURE)");
                   }
                   else if (pmgrGenericTestParams.outData == LW2080_CTRL_PMGR_TEST_STATUS(HI_OFFSET_MAX_CHECK, SINGLE_IPC_CH_FAILURE))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: HI_OFFSET Mismatch (ERROR: SINGLE_IPC_CH_FAILURE)");
                       Printf(Tee::PriNormal, "Observed HI_OFFSET = %llu\n, Expected HI_OFFSET = %llu\n",
                           LwU64_ALIGN32_VAL(&(pmgrGenericTestParams.observedVal)), LwU64_ALIGN32_VAL(&(pmgrGenericTestParams.expectedVal)));
                   }
                   else if (pmgrGenericTestParams.outData == LW2080_CTRL_PMGR_TEST_STATUS(HI_OFFSET_MAX_CHECK, THREE_IPC_CH_FAILURE))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: HI_OFFSET Mismatch (ERROR: THREE_IPC_CH_FAILURE)");
                       Printf(Tee::PriNormal, "Observed HI_OFFSET = %llu\n, Expected HI_OFFSET = %llu\n",
                           LwU64_ALIGN32_VAL(&(pmgrGenericTestParams.observedVal)), LwU64_ALIGN32_VAL(&(pmgrGenericTestParams.expectedVal)));
                   }
                   else if (pmgrGenericTestParams.outData == LW2080_CTRL_PMGR_TEST_STATUS(HI_OFFSET_MAX_CHECK, SINGLE_IPC_CHP_FAILURE))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: HI_OFFSET Mismatch (ERROR: SINGLE_IPC_CHP_FAILURE)");
                       Printf(Tee::PriNormal, "Observed HI_OFFSET = %llu\n, Expected HI_OFFSET = %llu\n",
                           LwU64_ALIGN32_VAL(&(pmgrGenericTestParams.observedVal)), LwU64_ALIGN32_VAL(&(pmgrGenericTestParams.expectedVal)));
                   }
                   else if (pmgrGenericTestParams.outData == LW2080_CTRL_PMGR_TEST_STATUS(HI_OFFSET_MAX_CHECK, THREE_IPC_CHP_FAILURE))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: HI_OFFSET Mismatch (ERROR: THREE_IPC_CHP_FAILURE)");
                       Printf(Tee::PriNormal, "Observed HI_OFFSET = %llu\n, Expected HI_OFFSET = %llu\n",
                           LwU64_ALIGN32_VAL(&(pmgrGenericTestParams.observedVal)), LwU64_ALIGN32_VAL(&(pmgrGenericTestParams.expectedVal)));
                   }
                   else if (pmgrGenericTestParams.outData == LW2080_CTRL_PMGR_TEST_STATUS(HI_OFFSET_MAX_CHECK, SINGLE_IPC_SUM_FAILURE))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: HI_OFFSET Mismatch (ERROR: SINGLE_IPC_SUM_FAILURE)");
                       Printf(Tee::PriNormal, "Observed HI_OFFSET = %llu\n, Expected HI_OFFSET = %llu\n",
                           LwU64_ALIGN32_VAL(&(pmgrGenericTestParams.observedVal)), LwU64_ALIGN32_VAL(&(pmgrGenericTestParams.expectedVal)));
                   }
                   else if (pmgrGenericTestParams.outData == LW2080_CTRL_PMGR_TEST_STATUS(HI_OFFSET_MAX_CHECK, THREE_IPC_SUM_SUM_CH_FAILURE))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: HI_OFFSET Mismatch (ERROR: THREE_IPC_SUM_SUM_CH_FAILURE)");
                       Printf(Tee::PriNormal, "Observed HI_OFFSET = %llu\n, Expected HI_OFFSET = %llu\n",
                           LwU64_ALIGN32_VAL(&(pmgrGenericTestParams.observedVal)), LwU64_ALIGN32_VAL(&(pmgrGenericTestParams.expectedVal)));
                   }
                   else if (pmgrGenericTestParams.outData == LW2080_CTRL_PMGR_TEST_STATUS(IPC_PARAMS_CHECK, CMOV_IIR_FAILURE))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: IIR value didn't change in CMOV mode (ERROR: CMOV_IIR_FAILURE)");
                   }
                   else if (pmgrGenericTestParams.outData == LW2080_CTRL_PMGR_TEST_STATUS(IPC_PARAMS_CHECK, CMOV_MUL_FAILURE))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: MUL value didn't change in CMOV mode (ERROR: CMOV_MUL_FAILURE)");
                   }
                   else if (pmgrGenericTestParams.outData == LW2080_CTRL_PMGR_TEST_STATUS(IPC_PARAMS_CHECK, CMOV_HI_OFFSET_FAILURE))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: HI_OFFSET value didn't change in CMOV mode (ERROR: CMOV_HI_OFFSET_FAILURE)");
                   }
                   else if (pmgrGenericTestParams.outData == LW2080_CTRL_PMGR_TEST_STATUS(IPC_PARAMS_CHECK, IPC_ALL_EN_FALIURE))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: Unexpected value of HI_OFFSET when all IPC instances are enabled (ERROR: IPC_ALL_EN_FALIURE)");
                   }
                   else if (pmgrGenericTestParams.outData == LW2080_CTRL_PMGR_TEST_STATUS(IPC_PARAMS_CHECK, IIR_DOWNSHIFT_FAILURE))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: Unexpected value of HI_OFFSET when IIR_DOWNSHIFT was programmed (ERROR: IIR_DOWNSHIFT_FAILURE)");
                   }
                   else if (pmgrGenericTestParams.outData == LW2080_CTRL_PMGR_TEST_STATUS(IPC_PARAMS_CHECK, PROP_DOWNSHIFT_FAILURE))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: Unexpected value of HI_OFFSET when PROP_DOWNSHIFT was programmed (ERROR: PROP_DOWNSHIFT_FAILURE)");
                   }
                   else if (pmgrGenericTestParams.outData == LW2080_CTRL_PMGR_TEST_STATUS(IPC_PARAMS_CHECK, IIR_AND_PROP_DOWNSHIFT_FAILURE))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: Unexpected value of HI_OFFSET when IIR_DOWNSHIFT and PROP_DOWNSHIFT was programmed (ERROR: IIR_AND_PROP_DOWNSHIFT_FAILURE)");
                   }
                   else if (pmgrGenericTestParams.outData == LW2080_CTRL_PMGR_TEST_STATUS(IPC_PARAMS_CHECK, IIR_DOWNSHIFT_CHANGE_FAILURE))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: Unexpected value of HI_OFFSET when IIR_DOWNSHIFT was changed (ERROR: IIR_DOWNSHIFT_CHANGE_FAILURE)");
                   }
                   else if (pmgrGenericTestParams.outData == LW2080_CTRL_PMGR_TEST_STATUS(IPC_PARAMS_CHECK, IIR_GAIN_DOWNSHIFT_FAILURE))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: Unexpected value of HI_OFFSET when IIR_GAIN and IIR_DOWNSHIFT was programmed (ERROR: IIR_GAIN_DOWNSHIFT_FAILURE)");
                   }
                   else if (pmgrGenericTestParams.outData == LW2080_CTRL_PMGR_TEST_STATUS(BEACON_CHECK, BEACON1_INTERRUPT_NOT_PENDING))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: BEACON1 did not raise interrupt when tested alone (ERROR: BEACON1_INTERRUPT_NOT_PENDING)");
                   }
                   else if (pmgrGenericTestParams.outData == LW2080_CTRL_PMGR_TEST_STATUS(BEACON_CHECK, BEACON2_INTERRUPT_NOT_PENDING))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: BEACON2 did not raise interrupt when tested alone (ERROR: BEACON2_INTERRUPT_NOT_PENDING)");
                   }
                   else if (pmgrGenericTestParams.outData == LW2080_CTRL_PMGR_TEST_STATUS(BEACON_CHECK, BOTH_BEACON_INTERRUPTS_NOT_PENDING))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: when tested simultaneously both BEACON1 and BEACON2 did not raise interrupt (ERROR: BOTH_BEACON_INTERRUPTS_NOT_PENDING)");
                   }
                   else if (pmgrGenericTestParams.outData == LW2080_CTRL_PMGR_TEST_STATUS(BEACON_CHECK, BEACON1_INTERRUPT_NOT_PENDING_BEACON2_INTERRUPT_PENDING))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: when tested simultaneously BEACON1 did not raise interrupt even though BEACON2 did (ERROR: BEACON1_INTERRUPT_NOT_PENDING_BEACON2_INTERRUPT_PENDING)");
                   }
                   else if (pmgrGenericTestParams.outData == LW2080_CTRL_PMGR_TEST_STATUS(BEACON_CHECK, BEACON2_INTERRUPT_NOT_PENDING_BEACON1_INTERRUPT_PENDING))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: when tested simultaneously BEACON2 did not raise interrupt even though BEACON1 did (ERROR: BEACON2_INTERRUPT_NOT_PENDING_BEACON1_INTERRUPT_PENDING)");
                   }
                   else if (pmgrGenericTestParams.outData == LW2080_CTRL_PMGR_TEST_STATUS(OFFSET_CHECK, BOTH_OFFSETS_IIR_VALUE_ERROR))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: both OFFSET1 and OFFSET2 action did not provide expected IIR value (ERROR: BOTH_OFFSETS_IIR_VALUE_ERROR)");
                   }
                   else if (pmgrGenericTestParams.outData == LW2080_CTRL_PMGR_TEST_STATUS(OFFSET_CHECK, OFFSET1_IIR_VALUE_ERROR))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: OFFSET1 did not provide expected IIR value even though OFFSET2 did (ERROR: OFFSET1_IIR_VALUE_ERROR)");
                   }
                   else if (pmgrGenericTestParams.outData == LW2080_CTRL_PMGR_TEST_STATUS(OFFSET_CHECK, OFFSET2_IIR_VALUE_ERROR))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: OFFSET2 did not provide expected IIR value even though OFFSET1 did (ERROR: OFFSET2_IIR_VALUE_ERROR)");
                   }
                   else if (pmgrGenericTestParams.outData == LW2080_CTRL_PMGR_TEST_STATUS(OFFSET_CHECK, BOTH_OFFSETS_IIR_ACC_VALUE_ERROR))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: both OFFSET1 and OFFSET2 action did not provide expected IIR ACC value (ERROR: BOTH_OFFSETS_IIR_ACC_VALUE_ERROR)");
                   }
                   else if (pmgrGenericTestParams.outData == LW2080_CTRL_PMGR_TEST_STATUS(OFFSET_CHECK, OFFSET1_IIR_ACC_VALUE_ERROR))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: OFFSET1 did not provide expected IIR ACC value even though OFFSET2 did (ERROR: OFFSET1_IIR_ACC_VALUE_ERROR)");
                   }
                   else if (pmgrGenericTestParams.outData == LW2080_CTRL_PMGR_TEST_STATUS(OFFSET_CHECK, OFFSET2_IIR_ACC_VALUE_ERROR))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: OFFSET2 did not provide expected IIR ACC value even though OFFSET1 did (ERROR: OFFSET2_IIR_ACC_VALUE_ERROR)");
                   }
                   else if (pmgrGenericTestParams.outData == LW2080_CTRL_PMGR_TEST_STATUS(OFFSET_CHECK, BOTH_OFFSETS_MUL_VALUE_ERROR))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: both OFFSET1 and OFFSET2 action did not provide expected MUL value (ERROR: BOTH_OFFSETS_MUL_VALUE_ERROR)");
                   }
                   else if (pmgrGenericTestParams.outData == LW2080_CTRL_PMGR_TEST_STATUS(OFFSET_CHECK, OFFSET1_MUL_VALUE_ERROR))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: OFFSET1 did not provide expected MUL value even though OFFSET2 did (ERROR: OFFSET1_MUL_VALUE_ERROR)");
                   }
                   else if (pmgrGenericTestParams.outData == LW2080_CTRL_PMGR_TEST_STATUS(OFFSET_CHECK, OFFSET2_MUL_VALUE_ERROR))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: OFFSET2 did not provide expected MUL value even though OFFSET1 did (ERROR: OFFSET2_MUL_VALUE_ERROR)");
                   }
                   else if (pmgrGenericTestParams.outData == LW2080_CTRL_PMGR_TEST_STATUS(OFFSET_CHECK, BOTH_OFFSETS_MUL_ACC_VALUE_ERROR))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: both OFFSET1 and OFFSET2 action did not provide expected MUL ACC value (ERROR: BOTH_OFFSETS_MUL_ACC_VALUE_ERROR)");
                   }
                   else if (pmgrGenericTestParams.outData == LW2080_CTRL_PMGR_TEST_STATUS(OFFSET_CHECK, OFFSET1_MUL_ACC_VALUE_ERROR))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: OFFSET1 did not provide expected MUL ACC value even though OFFSET2 did (ERROR: OFFSET1_MUL_ACC_VALUE_ERROR)");
                   }
                   else if (pmgrGenericTestParams.outData == LW2080_CTRL_PMGR_TEST_STATUS(OFFSET_CHECK, OFFSET2_MUL_ACC_VALUE_ERROR))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: OFFSET2 did not provide expected MUL ACC value even though OFFSET1 did (ERROR: OFFSET2_MUL_ACC_VALUE_ERROR)");
                   }
               }
               else
               {
                   Printf(Tee::PriNormal, "\n (ERROR: status = %d, data = %d",
                       pmgrGenericTestParams.outStatus, pmgrGenericTestParams.outData );
               }

               if ((UINT32)i < (sizeof(pmgrtest_name_a)/sizeof(pmgrtest_name_a[0])))
               {
                  Printf(Tee::PriNormal, "%s \n", pmgrtest_name_a[i].c_str());
               }
               else
               {
                  Printf(Tee::PriNormal, "%s \n", "Test not implemented");
               }
            }

            if (pmgrGenericTestParams.outStatus != LW2080_CTRL_PMGR_GENERIC_TEST_SUCCESS)
            {
                rc = RC::LWRM_ERROR;
                break;
            }

            // End iteration when we hit LW2080_CTRL_PMGR_GENERIC_TEST_NOT_IMPLEMENTED.
            // Skip remaining tests, if the user specifies so and if a failure has oclwred
            // to enable print TEST_NOT_IMPLEMENTED.
            bIterate = !((m_uiSkipOnFirstFailure &&
                         (pmgrGenericTestParams.outStatus ==
                             LW2080_CTRL_PMGR_GENERIC_TEST_ERROR_GENERIC)) ||
                         (pmgrGenericTestParams.outStatus ==
                             LW2080_CTRL_PMGR_GENERIC_TEST_NOT_IMPLEMENTED)) ||
                         (m_uiNumCmdLineOption == OPTION_TEST_INCLUDE);
        }
    }

    return rc;
}

//! \brief Setup test using cmd line arguments provided by user.
//!
//! Parses the command line arguments given and accordingly, sets up the test.
//!
//! \return OK if the options are valid, specific RC if failed
//-----------------------------------------------------------------------------
RC PmgrGenericTest::ParseCommandLineOptions()
{
    RC rc;

    switch(m_uiNumCmdLineOption)
    {
        case OPTION_TEST_INCLUDE:
            ssInputOptions << m_sPmgrgenerictestLwstomList;
            break;
        case OPTION_TEST_EXCLUDE:
            ssInputOptions << m_sPmgrgenerictestLwstomList;
            break;
        default:
            break;
    }
    return rc;
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

//! \brief Linkage to JavaScript.
//!
//! Set up a JavaScript class that creates and owns a C++ PmgrGenericTest.
//! object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(PmgrGenericTest, RmTest,
                      "Run a generic set of halified pmgr tests");
CLASS_PROP_READWRITE(PmgrGenericTest, uiNumCmdLineOption, UINT32,
                      "User supplied command line option: Default = 0.");
CLASS_PROP_READWRITE(PmgrGenericTest, sPmgrgenerictestLwstomList, string,
                      "custom list of pmgr tests");
CLASS_PROP_READWRITE(PmgrGenericTest, uiSkipOnFirstFailure, UINT32,
                      "User supplied command line option: Default = 0.");
