/*
 * Copyright (c) 2019, LWPU CORPORATION. All rights reserved.
 *
 * LWPU CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from LWPU CORPORATION is strictly prohibited.
 */

/*!
 * @file    voltpolicy-test.c
 * @brief   Unit tests for logic in VOLT_POLICY.
 */
/* ------------------------ System Includes --------------------------------- */
#include "test-macros.h"

/* ------------------------ Application Includes ---------------------------- */
#include "volt/objvolt.h"
#include "volt/objvolt-test.h"
#include "volt/voltpolicy-mock.h"
#include "volt/voltpolicy_single_rail-mock.h"
#include "volt/voltrail-mock.h"

/* ------------------------ Defines ----------------------------------------- */
/* ------------------------ Mocked Functions -------------------------------- */
/*!
 * @copydoc VoltPolicyOffsetVoltage()
 */
FLCN_STATUS
voltPolicyOffsetVoltage_SINGLE_RAIL
(
    VOLT_POLICY    *pPolicy
)
{
    return FLCN_OK;
}

/* ------------------------ Global Data-------------------------------------- */
static VOLT_RAIL   railObj_vp                = { 0U };
static VOLT_POLICY_SINGLE_RAIL policyObjSR   = { 0U };

/* ------------------------ Type Definitions -------------------------------- */
/*!
 * @brief   Structure describing a set of inputs to client id colwersion interface
 *          and the set of expected results.
 */
typedef struct VOLT_POLICY_CLIENT_COLWERT_TO_IDX_INPUT_OUTPUT
{
    LwU8 clientID;
    LwU8 expectedSeqPolicyIndex;
}VOLT_POLICY_CLIENT_COLWERT_TO_IDX_INPUT_OUTPUT;

/*!
 * @brief   Structure describing a set of inputs to sanity check interface
 *          and the set of expected results.
 */
typedef struct VOLT_POLICY_SANITY_CHECK_INPUT_OUTPUT
{
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST railList;
    LwU32                           count;
    FLCN_STATUS                     expectedResult;
}VOLT_POLICY_SANITY_CHECK_INPUT_OUTPUT;

/*!
 * @brief   Table of positive test conditions for voltPolicyClientColwertToIdx.
 */
static  VOLT_POLICY_CLIENT_COLWERT_TO_IDX_INPUT_OUTPUT voltPolicyClientColwertToIdxPositive_inout[] =
{
    {
        .clientID = LW2080_CTRL_VOLT_VOLT_POLICY_CLIENT_PERF_CORE_VF_SEQ,
        .expectedSeqPolicyIndex = 0,
    },
};

/*!
 * @brief   Table of negative test conditions for voltPolicyClientColwertToIdx.
 */
static  VOLT_POLICY_CLIENT_COLWERT_TO_IDX_INPUT_OUTPUT voltPolicyClientColwertToIdxNegative_inout[] =
{
    {
        .clientID = 0xFF,
        .expectedSeqPolicyIndex = LW2080_CTRL_VOLT_VOLT_POLICY_INDEX_ILWALID,
    },
};

/*!
 * @brief   Table of positive test conditions for voltPolicySanityCheck.
 */
static  VOLT_POLICY_SANITY_CHECK_INPUT_OUTPUT voltPolicySanityCheckPositive_inout[] =
{
    {
      .railList =
       {
            .numRails = 0x01,
            .rails = {{.railIdx = 0, .voltageuV = 700000, .voltageMinNoiseUnawareuV = 650000, .voltOffsetuV = {0,0}},},
       },
       .count = 1,
       .expectedResult = FLCN_OK,
    },
};

/*!
 * @brief   Table of negative test conditions for voltPolicySanityCheck.
 */
static  VOLT_POLICY_SANITY_CHECK_INPUT_OUTPUT voltPolicySanityCheckNegative_inout[] =
{
    {
      .railList =
       {
            .numRails = 0x02,
            .rails = {{.railIdx = 0, .voltageuV = 700000, .voltageMinNoiseUnawareuV = 650000, .voltOffsetuV = {0,0}},},
       },
       .count = 2,
       .expectedResult = FLCN_ERR_ILWALID_ARGUMENT,
    },
};

UT_SUITE_DEFINE(PMU_VOLTPOLICY,
                UT_SUITE_SET_COMPONENT("Unit Volt Policy")
                UT_SUITE_SET_DESCRIPTION("Unit tests for Unit Volt Policy")
                UT_SUITE_SET_OWNER("ngodbole"))

UT_CASE_DEFINE(PMU_VOLTPOLICY, voltPolicyClientColwertToIdxPositive,
  UT_CASE_SET_DESCRIPTION("Test voltPolicyClientColwertToIdxPositive")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test that the Unit Volt Policy client ID colwersion handles all positive
 *             inputs correctly.
 *
 * @details    The test shall ilwoke the Unit Volt Policy method that colwerts client ID
 *             into an internal policy index.
 *
 *             Parameters: void
 *
 *             The test passes if the expected result in the dataset matches with
 *             the actual result received after exelwting the test.
 *
 *             Any other return value, constitutes failure.
 */
UT_CASE_RUN(PMU_VOLTPOLICY,voltPolicyClientColwertToIdxPositive)
{
    LwU8    numEntriesPositive =
        sizeof(voltPolicyClientColwertToIdxPositive_inout) / sizeof(VOLT_POLICY_CLIENT_COLWERT_TO_IDX_INPUT_OUTPUT);
    LwU8     i;
    LwU8     seqPolicyIndex;

    Volt.policyMetadata.perfCoreVFSeqPolicyIdx = 0;
    for (i = 0; i < numEntriesPositive; i++)
    {
        // Run the function under test.
        seqPolicyIndex = voltPolicyClientColwertToIdx_IMPL(voltPolicyClientColwertToIdxPositive_inout[i].clientID);

        // Verify result.
        UT_ASSERT_EQUAL_UINT(seqPolicyIndex, voltPolicyClientColwertToIdxPositive_inout[i].expectedSeqPolicyIndex);
        UT_ASSERT_EQUAL_UINT(0U, UTF_INTRINSICS_MOCK_GET_HALT_COUNT());
    }
}

UT_CASE_DEFINE(PMU_VOLTPOLICY, voltPolicyClientColwertToIdxNegative,
  UT_CASE_SET_DESCRIPTION("Test voltPolicyClientColwertToIdxNegative")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test that the Unit Volt Policy client ID colwersion handles all negative
 *             inputs correctly.
 *
 * @details    The test shall ilwoke the Unit Volt Policy method that colwerts client ID
 *             into an internal policy index.
 *
 *             Parameters: void
 *
 *             The test passes if the expected result in the dataset matches with
 *             the actual result received after exelwting the test.
 *
 *             Any other return value, constitutes failure.
 */
UT_CASE_RUN(PMU_VOLTPOLICY,voltPolicyClientColwertToIdxNegative)
{
    LwU8    numEntriesNegative =
        sizeof(voltPolicyClientColwertToIdxNegative_inout) / sizeof(VOLT_POLICY_CLIENT_COLWERT_TO_IDX_INPUT_OUTPUT);
    LwU8     i;
    LwU8     seqPolicyIndex;

    Volt.policyMetadata.perfCoreVFSeqPolicyIdx = 0;
    for (i = 0; i < numEntriesNegative; i++)
    {
        // Run the function under test.
        seqPolicyIndex = voltPolicyClientColwertToIdx_IMPL(voltPolicyClientColwertToIdxNegative_inout[i].clientID);

        // Verify result.
        UT_ASSERT_EQUAL_UINT(seqPolicyIndex, voltPolicyClientColwertToIdxNegative_inout[i].expectedSeqPolicyIndex);
    }
}


UT_CASE_DEFINE(PMU_VOLTPOLICY, voltPolicySanityCheckPositive,
  UT_CASE_SET_DESCRIPTION("Test voltPolicySanityCheckPositive")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Pre-test setup for testing the set voltage interface for Unit.
 *             Volt Device with different set of inputs.
 *
 * @details    Before the test is run, set the input vales for mocked interfaces.
 */
PRE_TEST_METHOD(voltPolicySanityCheck)
{
    // Mock up the VOLT_RAIL board object group.
    railObj_vp.super.type      = LW2080_CTRL_VOLT_VOLT_DOMAIN_LOGIC;
    railObj_vp.super.grpIdx    = 0U;

    // Mock up the VOLT_POLICY board object group.
    policyObjSR.super.super.type      = LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SINGLE_RAIL;
    policyObjSR.super.super.grpIdx    = 0U;
    policyObjSR.pRail = &railObj_vp;

    boardObjDynamicCastMockInit();
    boardObjDynamicCastMockAddEntry(0, (void *)&policyObjSR);
}

/*!
 * @brief      Post-test setup for testing the set voltage interface for Unit.
 *             Volt Device with different set of inputs.
 *
 * @details    Before the test is run, set the input vales for mocked interfaces.
 */
POST_TEST_METHOD(voltPolicySanityCheck)
{
    memset(&railObj_vp, 0, sizeof(VOLT_RAIL));
    memset(&policyObjSR, 0, sizeof(VOLT_RAIL));
}

/*!
 * @brief      Test that the Unit Volt Policy set voltage interface handles all positive
 *             inputs correctly.
 *
 * @details    The test shall ilwoke the Unit Volt Policy method that sanity checks
 *             input voltage rail list.
 *
 *             Parameters: void
 *
 *             The test passes if the expected result in the dataset matches with
 *             the actual result received after exelwting the test.
 *
 *             Any other return value, constitutes failure.
 */
UT_CASE_RUN(PMU_VOLTPOLICY,voltPolicySanityCheckPositive)
{
    LwU8    numEntriesPositive =
        sizeof(voltPolicySanityCheckPositive_inout) / sizeof(VOLT_POLICY_SANITY_CHECK_INPUT_OUTPUT);
    LwU8     i;
    FLCN_STATUS status;
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST railList;

    memset(&railList, 0, sizeof(railList));

    PRE_TEST_NAME(voltPolicySanityCheck)();

    for (i = 0; i < numEntriesPositive; i++)
    {
        // Copy the data set.
        memcpy(&railList, &voltPolicySanityCheckPositive_inout[i].railList, sizeof(LW2080_CTRL_VOLT_VOLT_RAIL_LIST));

        // Run the function under test.
        status = voltPolicySanityCheck_IMPL((VOLT_POLICY *)&policyObjSR, voltPolicySanityCheckPositive_inout[i].count, railList.rails);

        // Verify result.
        UT_ASSERT_EQUAL_UINT(status, voltPolicySanityCheckPositive_inout[i].expectedResult);
        UT_ASSERT_EQUAL_UINT(0U, UTF_INTRINSICS_MOCK_GET_HALT_COUNT());
    }
    POST_TEST_NAME(voltPolicySanityCheck)();
}

UT_CASE_DEFINE(PMU_VOLTPOLICY, voltPolicySanityCheckNegative,
  UT_CASE_SET_DESCRIPTION("Test voltPolicySanityCheckNegative")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test that the Unit Volt Policy sanity check interface handles all negative
 *             inputs correctly.
 *
 * @details    The test shall ilwoke the Unit Volt Policy method that sanity checks
 *             input voltage rail list.
 *
 *             Parameters: void
 *
 *             The test passes if the expected result in the dataset matches with
 *             the actual result received after exelwting the test.
 *
 *             Any other return value, constitutes failure.
 */
UT_CASE_RUN(PMU_VOLTPOLICY, voltPolicySanityCheckNegative)
{
    LwU8    numEntriesNegative =
        sizeof(voltPolicySanityCheckNegative_inout) / sizeof(VOLT_POLICY_SANITY_CHECK_INPUT_OUTPUT);
    LwU8     i;
    FLCN_STATUS status;
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST railList;

    memset(&railList, 0, sizeof(railList));

    PRE_TEST_NAME(voltPolicySanityCheck)();

    for (i = 0; i < numEntriesNegative; i++)
    {
        // Copy the data set.
        memcpy(&railList, &voltPolicySanityCheckNegative_inout[i].railList, sizeof(LW2080_CTRL_VOLT_VOLT_RAIL_LIST));

        // Run the function under test.
        status = voltPolicySanityCheck_IMPL((VOLT_POLICY *)&policyObjSR, voltPolicySanityCheckNegative_inout[i].count, railList.rails);

        // Verify result.
        UT_ASSERT_EQUAL_UINT(status, voltPolicySanityCheckNegative_inout[i].expectedResult);
    }

    policyObjSR.super.super.type      = 0xFF;
    status = voltPolicySanityCheck_IMPL((VOLT_POLICY *)&policyObjSR, 1, railList.rails);
    UT_ASSERT_EQUAL_UINT(status,  FLCN_ERR_NOT_SUPPORTED);

    POST_TEST_NAME(voltPolicySanityCheck)();
}

UT_CASE_DEFINE(PMU_VOLTPOLICY, voltPoliciesLoadFail,
  UT_CASE_SET_DESCRIPTION("Test voltPoliciesLoadFail")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      
 *
 * @details    
 *
 *             Parameters: 
 *
 *             The test passes if the expected result in the dataset matches with
 *             the actual result received after exelwting the test.
 *
 *             Any other return value, constitutes failure.
 */
UT_CASE_RUN(PMU_VOLTPOLICY, voltPoliciesLoadFail)
{
    FLCN_STATUS status;
    VOLT_POLICY voltPolicy;
    BOARDOBJ   *voltPolicyObjects[1];

    // Mock up the VOLT_RAIL board object group.
    voltPolicy.super.type                       = LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SINGLE_RAIL;
    voltPolicy.super.grpIdx                     = 0U;
    voltPolicyObjects[0]                        = &voltPolicy.super;
    Volt.policies.super.type        = LW2080_CTRL_BOARDOBJGRP_TYPE_E32;
    Volt.policies.super.objSlots    = 1U;
    Volt.policies.super.ppObjects   = voltPolicyObjects;
    boardObjGrpMaskInit_E32(&Volt.policies.objMask);
    boardObjGrpMaskBitSet(&Volt.policies.objMask, BOARDOBJ_GET_GRP_IDX(&voltPolicy));
    RESET_FAKE(voltPolicyLoad_MOCK);
    voltPolicyLoad_MOCK_fake.return_val = FLCN_ERR_STATE_RESET_NEEDED;

    status = voltPoliciesLoad();
    UT_ASSERT_EQUAL_UINT(status,  FLCN_ERR_STATE_RESET_NEEDED);
}

UT_CASE_DEFINE(PMU_VOLTPOLICY, voltPolicySetVoltageClientColwertToIdxFail,
  UT_CASE_SET_DESCRIPTION("Test voltPolicySetVoltageClientColwertToIdxFail")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      
 *
 * @details    
 *
 *             Parameters: 
 *
 *             The test passes if the expected result in the dataset matches with
 *             the actual result received after exelwting the test.
 *
 *             Any other return value, constitutes failure.
 */
UT_CASE_RUN(PMU_VOLTPOLICY, voltPolicySetVoltageClientColwertToIdxFail)
{
    FLCN_STATUS status;
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST railList;

voltPolicyClientColwertToIdx_MOCK_CONFIG.seqPolicyIndex = 0xf0;

    status = voltPolicySetVoltage_IMPL(0, &railList);
    UT_ASSERT_EQUAL_UINT(status,  FLCN_ERR_ILWALID_INDEX);
}

UT_CASE_DEFINE(PMU_VOLTPOLICY, voltPolicySetVoltageSetVoltageInternalFail,
  UT_CASE_SET_DESCRIPTION("Test voltPolicySetVoltageSetVoltageInternalFail")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      
 *
 * @details    
 *
 *             Parameters: 
 *
 *             The test passes if the expected result in the dataset matches with
 *             the actual result received after exelwting the test.
 *
 *             Any other return value, constitutes failure.
 */
UT_CASE_RUN(PMU_VOLTPOLICY, voltPolicySetVoltageSetVoltageInternalFail)
{
    FLCN_STATUS status;
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST railList;
    VOLT_POLICY voltPolicy;
    BOARDOBJ   *voltPolicyObjects[1];

    // Mock up the VOLT_POLICY board object group.
    voltPolicy.super.type                       = 0xAB; // invalid type
    voltPolicy.super.grpIdx                     = 0U;
    voltPolicyObjects[0]                        = &voltPolicy.super;
    Volt.policies.super.type        = LW2080_CTRL_BOARDOBJGRP_TYPE_E32;
    Volt.policies.super.objSlots    = 1U;
    Volt.policies.super.ppObjects   = voltPolicyObjects;
    boardObjGrpMaskInit_E32(&Volt.policies.objMask);
    boardObjGrpMaskBitSet(&Volt.policies.objMask, BOARDOBJ_GET_GRP_IDX(&voltPolicy));

    voltPolicyClientColwertToIdx_MOCK_CONFIG.seqPolicyIndex = 0x00;

    RESET_FAKE(voltPolicySetVoltageInternal_MOCK);

    voltPolicySetVoltageInternal_MOCK_fake.return_val = FLCN_ERR_NOT_SUPPORTED;
    status = voltPolicySetVoltage_IMPL(0, &railList);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_NOT_SUPPORTED);
}

UT_CASE_DEFINE(PMU_VOLTPOLICY, voltPolicySetVoltageSuccess,
  UT_CASE_SET_DESCRIPTION("Test voltPolicySetVoltageSuccess")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      
 *
 * @details    
 *
 *             Parameters: 
 *
 *             The test passes if the expected result in the dataset matches with
 *             the actual result received after exelwting the test.
 *
 *             Any other return value, constitutes failure.
 */
UT_CASE_RUN(PMU_VOLTPOLICY, voltPolicySetVoltageSuccess)
{
    FLCN_STATUS status;
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST railList;
    VOLT_POLICY voltPolicy;
    BOARDOBJ   *voltPolicyObjects[1];

    // Mock up the VOLT_POLICY board object group.
    voltPolicy.super.type                       = LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SINGLE_RAIL;
    voltPolicy.super.grpIdx                     = 0U;
    voltPolicyObjects[0]                        = &voltPolicy.super;
    Volt.policies.super.type        = LW2080_CTRL_BOARDOBJGRP_TYPE_E32;
    Volt.policies.super.objSlots    = 1U;
    Volt.policies.super.ppObjects   = voltPolicyObjects;
    boardObjGrpMaskInit_E32(&Volt.policies.objMask);
    boardObjGrpMaskBitSet(&Volt.policies.objMask, BOARDOBJ_GET_GRP_IDX(&voltPolicy));

    voltPolicyClientColwertToIdx_MOCK_CONFIG.seqPolicyIndex = 0x00;

    RESET_FAKE(voltPolicySetVoltageInternal_MOCK);

    voltPolicySetVoltageInternal_MOCK_fake.return_val = FLCN_OK;
    status = voltPolicySetVoltage_IMPL(0, &railList);
    UT_ASSERT_EQUAL_UINT(status, FLCN_OK);
}

UT_CASE_DEFINE(PMU_VOLTPOLICY, voltPolicyDynamilwpdateSuccess,
  UT_CASE_SET_DESCRIPTION("Test voltPolicyDynamilwpdateSuccess")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      
 *
 * @details    
 *
 *             Parameters: 
 *
 *             The test passes if the expected result in the dataset matches with
 *             the actual result received after exelwting the test.
 *
 *             Any other return value, constitutes failure.
 */
UT_CASE_RUN(PMU_VOLTPOLICY, voltPolicyDynamilwpdateSuccess)
{
    FLCN_STATUS status;
    VOLT_POLICY voltPolicy;

    voltPolicy.super.type = 0xAB; // Hit default case.
    status = voltPolicyDynamilwpdate(&voltPolicy);
    UT_ASSERT_EQUAL_UINT(status, FLCN_OK);
}


UT_CASE_DEFINE(PMU_VOLTPOLICY, voltPolicyLoadFailDefaultCase,
  UT_CASE_SET_DESCRIPTION("Test voltPolicyLoadFailDefaultCase")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      
 *
 * @details    
 *
 *             Parameters: 
 *
 *             The test passes if the expected result in the dataset matches with
 *             the actual result received after exelwting the test.
 *
 *             Any other return value, constitutes failure.
 */
UT_CASE_RUN(PMU_VOLTPOLICY, voltPolicyLoadFailDefaultCase)
{
    FLCN_STATUS status;
    VOLT_POLICY voltPolicy;

    voltPolicy.super.type = 0xAB; // Hit default case.
    status = voltPolicyLoad_IMPL(&voltPolicy);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_NOT_SUPPORTED);
}

UT_CASE_DEFINE(PMU_VOLTPOLICY, voltPolicyLoadFailSingleRailError,
  UT_CASE_SET_DESCRIPTION("Test voltPolicyLoadFailSingleRailError")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      
 *
 * @details    
 *
 *             Parameters: 
 *
 *             The test passes if the expected result in the dataset matches with
 *             the actual result received after exelwting the test.
 *
 *             Any other return value, constitutes failure.
 */
UT_CASE_RUN(PMU_VOLTPOLICY, voltPolicyLoadFailSingleRailError)
{
    FLCN_STATUS status;
    VOLT_POLICY voltPolicy;

    voltPolicy.super.type = LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SINGLE_RAIL;
    RESET_FAKE(voltPolicyLoad_SINGLE_RAIL_MOCK);
    voltPolicyLoad_SINGLE_RAIL_MOCK_fake.return_val = FLCN_ERR_STATE_RESET_NEEDED;
    status = voltPolicyLoad_IMPL(&voltPolicy);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_STATE_RESET_NEEDED);
}

UT_CASE_DEFINE(PMU_VOLTPOLICY, voltPolicyLoadSuccess,
  UT_CASE_SET_DESCRIPTION("Test voltPolicyLoadSuccess")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      
 *
 * @details    
 *
 *             Parameters: 
 *
 *             The test passes if the expected result in the dataset matches with
 *             the actual result received after exelwting the test.
 *
 *             Any other return value, constitutes failure.
 */
UT_CASE_RUN(PMU_VOLTPOLICY, voltPolicyLoadSuccess)
{
    FLCN_STATUS status;
    VOLT_POLICY voltPolicy;

    voltPolicy.super.type = LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SINGLE_RAIL;
    RESET_FAKE(voltPolicyLoad_SINGLE_RAIL_MOCK);
    voltPolicyLoad_SINGLE_RAIL_MOCK_fake.return_val = FLCN_OK;
    status = voltPolicyLoad_IMPL(&voltPolicy);
    UT_ASSERT_EQUAL_UINT(status, FLCN_OK);
}


UT_CASE_DEFINE(PMU_VOLTPOLICY, voltPoliciesDynamilwpdateFailPolicyDynUpdate,
  UT_CASE_SET_DESCRIPTION("Test voltPoliciesDynamilwpdateFailPolicyDynUpdate")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      
 *
 * @details    
 *
 *             Parameters: 
 *
 *             The test passes if the expected result in the dataset matches with
 *             the actual result received after exelwting the test.
 *
 *             Any other return value, constitutes failure.
 */
UT_CASE_RUN(PMU_VOLTPOLICY, voltPoliciesDynamilwpdateFailPolicyDynUpdate)
{
    FLCN_STATUS status;
    VOLT_POLICY voltPolicy;
    BOARDOBJ   *voltPolicyObjects[1];

    // Mock up the VOLT_POLICY board object group.
    voltPolicy.super.type                       = LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SINGLE_RAIL;
    voltPolicy.super.grpIdx                     = 0U;
    voltPolicyObjects[0]                        = &voltPolicy.super;
    Volt.policies.super.type        = LW2080_CTRL_BOARDOBJGRP_TYPE_E32;
    Volt.policies.super.objSlots    = 1U;
    Volt.policies.super.ppObjects   = voltPolicyObjects;
    boardObjGrpMaskInit_E32(&Volt.policies.objMask);
    boardObjGrpMaskBitSet(&Volt.policies.objMask, BOARDOBJ_GET_GRP_IDX(&voltPolicy));

    // Mock error in voltPolicyDynamilwpdate.
    RESET_FAKE(voltPolicyDynamilwpdate_MOCK);
    voltPolicyDynamilwpdate_MOCK_fake.return_val = FLCN_ERR_STATE_RESET_NEEDED;

    status = voltPoliciesDynamilwpdate_IMPL();
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_STATE_RESET_NEEDED);
}

UT_CASE_DEFINE(PMU_VOLTPOLICY, voltPoliciesDynamilwpdateSuccess,
  UT_CASE_SET_DESCRIPTION("Test voltPoliciesDynamilwpdateSuccess")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      
 *
 * @details    
 *
 *             Parameters: 
 *
 *             The test passes if the expected result in the dataset matches with
 *             the actual result received after exelwting the test.
 *
 *             Any other return value, constitutes failure.
 */
UT_CASE_RUN(PMU_VOLTPOLICY, voltPoliciesDynamilwpdateSuccess)
{
    FLCN_STATUS status;
    VOLT_POLICY voltPolicy;
    BOARDOBJ   *voltPolicyObjects[1];

    // Mock up the VOLT_POLICY board object group.
    voltPolicy.super.type                       = LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SINGLE_RAIL;
    voltPolicy.super.grpIdx                     = 0U;
    voltPolicyObjects[0]                        = &voltPolicy.super;
    Volt.policies.super.type        = LW2080_CTRL_BOARDOBJGRP_TYPE_E32;
    Volt.policies.super.objSlots    = 1U;
    Volt.policies.super.ppObjects   = voltPolicyObjects;
    boardObjGrpMaskInit_E32(&Volt.policies.objMask);
    boardObjGrpMaskBitSet(&Volt.policies.objMask, BOARDOBJ_GET_GRP_IDX(&voltPolicy));

    // Mock error in voltPolicyDynamilwpdate.
    RESET_FAKE(voltPolicyDynamilwpdate_MOCK);
    voltPolicyDynamilwpdate_MOCK_fake.return_val = FLCN_OK;

    status = voltPoliciesDynamilwpdate_IMPL();
    UT_ASSERT_EQUAL_UINT(status, FLCN_OK);
}

UT_CASE_DEFINE(PMU_VOLTPOLICY, voltPolicySetVoltageInternalFail,
  UT_CASE_SET_DESCRIPTION("Test voltPolicySetVoltageInternalFail")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      
 *
 * @details    
 *
 *             Parameters: 
 *
 *             The test passes if the expected result in the dataset matches with
 *             the actual result received after exelwting the test.
 *
 *             Any other return value, constitutes failure.
 */
UT_CASE_RUN(PMU_VOLTPOLICY, voltPolicySetVoltageInternalFail)
{
    FLCN_STATUS status;
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST railList;
    VOLT_POLICY voltPolicy;

    voltPolicy.super.type = LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SINGLE_RAIL;
    RESET_FAKE(voltPolicySetVoltage_SINGLE_RAIL_MOCK);
    voltPolicySetVoltage_SINGLE_RAIL_MOCK_fake.return_val = FLCN_ERR_STATE_RESET_NEEDED;
    status = voltPolicySetVoltageInternal_IMPL(&voltPolicy, 1, &railList);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_STATE_RESET_NEEDED);
}

UT_CASE_DEFINE(PMU_VOLTPOLICY, voltPolicySetVoltageInternalSuccess,
  UT_CASE_SET_DESCRIPTION("Test voltPolicySetVoltageInternalSuccess")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      
 *
 * @details    
 *
 *             Parameters: 
 *
 *             The test passes if the expected result in the dataset matches with
 *             the actual result received after exelwting the test.
 *
 *             Any other return value, constitutes failure.
 */
UT_CASE_RUN(PMU_VOLTPOLICY, voltPolicySetVoltageInternalSuccess)
{
    FLCN_STATUS status;
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST railList;
    VOLT_POLICY voltPolicy;

    voltPolicy.super.type = LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SINGLE_RAIL;
    RESET_FAKE(voltPolicySetVoltage_SINGLE_RAIL_MOCK);
    voltPolicySetVoltage_SINGLE_RAIL_MOCK_fake.return_val = FLCN_OK;
    status = voltPolicySetVoltageInternal_IMPL(&voltPolicy, 1, &railList);
    UT_ASSERT_EQUAL_UINT(status, FLCN_OK);
}
