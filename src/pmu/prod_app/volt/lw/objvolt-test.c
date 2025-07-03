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
 * @file    objvolt-test.c
 * @brief   Unit tests for logic in OBJVOLT.
 */

/* ------------------------ System Includes --------------------------------- */
#include "test-macros.h"

/* ------------------------ Application Includes ---------------------------- */
#include "volt/objvolt.h"
#include "volt/objvolt-test.h"
#include "volt/objvolt-mock.h"
#include "volt/voltrail-mock.h"
#include "lwstatus.h"
#include "volt/voltpolicy-mock.h"
#include "lib_semaphore-mock.h"

/* ------------------------ Defines ----------------------------------------- */
/* ------------------------ Mocked Functions -------------------------------- */
/*!
 * @brief      Mocked Unit Locks method for acquiring a reader-writer lock.
 *
 * @details    This is being used in functions other than what are intended to
 *             be tested in this file, thereby causing a linker script failure
 *             when the UTF tries to build the falcon source files.
 *             Mocking this function helps us get around that issue without
 *             impacting testing. These will be revisited as/when new tests are
 *             added which will actually call these functions.
 */
void
osSemaphoreRWTakeWR
(
    PSEMAPHORE_RW   pSem
)
{
}

/*!
 * @brief      Mocked Unit Locks method for releasing a reader-writer lock.
 *
 * @details    This is being used in functions other than what are intended to
 *             be tested in this file, thereby causing a linker script failure
 *             when the UTF tries to build the falcon source files.
 *             Mocking this function helps us get around that issue without
 *             impacting testing. These will be revisited as/when new tests are
 *             added which will actually call these functions.
 */
void
osSemaphoreRWGiveWR
(
    PSEMAPHORE_RW   pSem
)
{
}

/* ------------------------ Type Definitions -------------------------------- */
/*!
 * @brief   Structure describing a set of inputs to voltage sanity check interface
 *          and the set of expected results.
 */
typedef struct VOLT_VOLT_SANITY_CHECK_INPUT_OUTPUT
{
    LwU8                            clientID;
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST railList;
    LwBool                          bVoltLoaded;
    FLCN_STATUS                     expectedResult;
    FLCN_STATUS                     voltPolicySanityCheckStatus;
}VOLT_VOLT_SANITY_CHECK_INPUT_OUTPUT;

/*!
 * @brief   Structure describing a set of inputs to voltage set voltage interface
 *          and the set of expected results.
 */
typedef struct VOLT_VOLT_SET_VOLTAGE_INPUT_OUTPUT
{
    FLCN_STATUS     voltVoltSanityCheckStatus;
    FLCN_STATUS     voltRailSetNoiseUnawareVminStatus;
    FLCN_STATUS     voltPolicySetVoltageStatus;
    FLCN_STATUS     expectedResult;
}VOLT_VOLT_SET_VOLTAGE_INPUT_OUTPUT;

/*!
 * @brief   Structure describing a set of inputs to dynamic update interface
 *          and the set of expected results.
 */
typedef struct VOLT_PROCESS_DYNAMIC_UPDATE_INPUT_OUTPUT
{
    LwBool          bVoltLoaded;
    FLCN_STATUS     voltRailsDynamilwpdateStatus;
    FLCN_STATUS     voltPoliciesDynamilwpdateStatus;
    FLCN_STATUS     expectedResult;
}VOLT_PROCESS_DYNAMIC_UPDATE_INPUT_OUTPUT;

/* ------------------------ Global Data-------------------------------------- */
VOLT_RAIL   railObj     = { 0U };
VOLT_POLICY policyObj   = { 0U };
BOARDOBJ   *railObjects[1];
BOARDOBJ   *policyObjects[1];

/* ------------------------ Local Data -------------------------------------- */
/*!
 * @brief   Table of negative test conditions for voltVoltSanityCheck.
 */
static  VOLT_VOLT_SANITY_CHECK_INPUT_OUTPUT voltVoltSanityCheckNegative_inout[] =
{
    // numRails = 0
    { .clientID = 0,
      .railList =
       {
            .numRails = 0x00,
            .rails = {{.railIdx = 0, .voltageuV = 0, .voltageMinNoiseUnawareuV = 650000, .voltOffsetuV = {0,0}},},
       },
      .bVoltLoaded = LW_TRUE,
      .expectedResult = FLCN_ERR_ILWALID_ARGUMENT,
      .voltPolicySanityCheckStatus = FLCN_OK
    },

    // numRails = 0xff
    { .clientID = 0,
      .railList =
       {
            .numRails = 0xff,
            .rails = {{.railIdx = 0, .voltageuV = 0, .voltageMinNoiseUnawareuV = 650000, .voltOffsetuV = {0,0}},},
       },
      .bVoltLoaded = LW_TRUE,
      .expectedResult = FLCN_ERR_ILWALID_ARGUMENT,
      .voltPolicySanityCheckStatus = FLCN_OK
    },

    // numRails > LW2080_CTRL_VOLT_VOLT_RAIL_CLIENT_MAX_RAILS
    { .clientID = 0,
      .railList =
       {
            .numRails = 0x05,
            .rails = 
             {
                {
                    .railIdx = 0,
                    .voltageuV = 0,
                    .voltageMinNoiseUnawareuV = 650000,
                    .voltOffsetuV = {0,0}
                },
             },
       },
      .bVoltLoaded = LW_TRUE,
      .expectedResult = FLCN_ERR_ILWALID_ARGUMENT,
      .voltPolicySanityCheckStatus = FLCN_OK
    },

    // railIdx = INVALID
    { .clientID = 0,
      .railList =
       {
            .numRails = 0x01,
            .rails =
             {
                {
                    .railIdx = LW2080_CTRL_VOLT_VOLT_RAIL_INDEX_ILWALID,
                    .voltageuV = 650000,
                    .voltageMinNoiseUnawareuV = 650000,
                    .voltOffsetuV = {0,0}
                },
             },
       },
      .bVoltLoaded = LW_TRUE,
      .expectedResult = FLCN_ERR_ILWALID_ARGUMENT,
      .voltPolicySanityCheckStatus = FLCN_OK
    },

    // voltageuV = 0
    { .clientID = 0,
      .railList =
       {
            .numRails = 0x01,
            .rails =
             {
                {
                     .railIdx = 0,
                     .voltageuV = 0,
                     .voltageMinNoiseUnawareuV = 650000,
                     .voltOffsetuV = {0,0}
                },
             },
       },
      .bVoltLoaded = LW_TRUE,
      .expectedResult = FLCN_ERR_ILWALID_ARGUMENT,
      .voltPolicySanityCheckStatus = FLCN_OK
    },

    // voltageuV < voltageMinNoiseUnawareuV
    { .clientID = 0,
      .railList =
       {
           .numRails = 0x01,
           .rails = 
            {
                {
                    .railIdx = 0,
                    .voltageuV = 550000,
                    .voltageMinNoiseUnawareuV = 650000,
                    .voltOffsetuV = {0,0}
                },
            },
       },
      .bVoltLoaded = LW_TRUE,
      .expectedResult = FLCN_ERR_ILWALID_ARGUMENT,
      .voltPolicySanityCheckStatus = FLCN_OK
    },

    // PMU_VOLT_IS_LOADED is false
    { .clientID = 0,
      .railList =
       {
            .numRails = 0x01,
            .rails =
             {
                {
                     .railIdx = 0,
                     .voltageuV = 650000,
                     .voltageMinNoiseUnawareuV = 650000,
                     .voltOffsetuV = {0,0}
                },
             },
       },
      .bVoltLoaded = LW_FALSE,
      .expectedResult = FLCN_ERR_ILWALID_STATE,
      .voltPolicySanityCheckStatus = FLCN_OK
    },

    // Invalid clientID
    { .clientID = 9,
      .railList =
       {
            .numRails = 0x01,
            .rails =
             {
                 {
                     .railIdx = 0,
                     .voltageuV = 650000,
                     .voltageMinNoiseUnawareuV = 650000,
                     .voltOffsetuV = {0,0}
                 },
             },
       },
      .bVoltLoaded = LW_TRUE,
      .expectedResult = FLCN_ERR_ILWALID_ARGUMENT,
      .voltPolicySanityCheckStatus = FLCN_OK
    },

    // voltPolicySanityCheck() fail
    { .clientID = 0,
      .railList =
       {
            .numRails = 0x01,
            .rails =
             {
                 {
                     .railIdx = 0,
                     .voltageuV = 650000,
                     .voltageMinNoiseUnawareuV = 650000,
                     .voltOffsetuV = {0,0}
                 },
             },
       },
      .bVoltLoaded = LW_TRUE,
      .expectedResult = FLCN_ERR_ILWALID_ARGUMENT,
      .voltPolicySanityCheckStatus = FLCN_ERR_ILWALID_ARGUMENT
    },
};

/*!
 * @brief   Table of positive test conditions for voltVoltSanityCheck.
 */
static  VOLT_VOLT_SANITY_CHECK_INPUT_OUTPUT voltVoltSanityCheckPositive_inout[] =
{
    { .clientID = 0,
      .railList =
       {
            .numRails = 0x01,
            .rails = {{.railIdx = 0, .voltageuV = 650000, .voltageMinNoiseUnawareuV = 650000, .voltOffsetuV = {0,0}},},
       },
      .bVoltLoaded = LW_TRUE,
      .expectedResult = FLCN_OK,
      .voltPolicySanityCheckStatus = FLCN_OK
    },
};

/*!
 * @brief   Table of negative test conditions for voltVoltSetVoltage.
 */
static  VOLT_VOLT_SET_VOLTAGE_INPUT_OUTPUT voltVoltSetVoltageNegative_inout[] =
{
    {
        .voltVoltSanityCheckStatus         = FLCN_ERR_ILWALID_ARGUMENT,
        .voltRailSetNoiseUnawareVminStatus = FLCN_OK,
        .voltPolicySetVoltageStatus        = FLCN_OK,
        .expectedResult                    = FLCN_ERR_ILWALID_ARGUMENT,
    },
    {
        .voltVoltSanityCheckStatus         = FLCN_OK,
        .voltRailSetNoiseUnawareVminStatus = FLCN_ERR_ILWALID_INDEX,
        .voltPolicySetVoltageStatus        = FLCN_OK,
        .expectedResult                    = FLCN_ERR_ILWALID_INDEX,
    },
    {
        .voltVoltSanityCheckStatus         = FLCN_OK,
        .voltRailSetNoiseUnawareVminStatus = FLCN_OK,
        .voltPolicySetVoltageStatus        = FLCN_ERR_ILWALID_INDEX,
        .expectedResult                    = FLCN_ERR_ILWALID_INDEX,
    },
};

/*!
 * @brief   Table of positive test conditions for voltVoltSetVoltage.
 */
static  VOLT_VOLT_SET_VOLTAGE_INPUT_OUTPUT voltVoltSetVoltagePositive_inout[] =
{
    {
        .voltVoltSanityCheckStatus         = FLCN_OK,
        .voltRailSetNoiseUnawareVminStatus = FLCN_OK,
        .voltPolicySetVoltageStatus        = FLCN_OK,
        .expectedResult                    = FLCN_OK,
    },
};

/*!
 * @brief   Table of positive test conditions for voltProcessDynamilwpdate.
 */
static  VOLT_PROCESS_DYNAMIC_UPDATE_INPUT_OUTPUT voltProcessDynamilwpdatePositive_inout[] =
{
    { 
        .bVoltLoaded                     = LW_TRUE,
        .voltRailsDynamilwpdateStatus    = FLCN_OK,
        .voltPoliciesDynamilwpdateStatus = FLCN_OK,
        .expectedResult                  = FLCN_OK,
    },
};

/*!
 * @brief   Table of negative test conditions for voltProcessDynamilwpdate.
 */
static  VOLT_PROCESS_DYNAMIC_UPDATE_INPUT_OUTPUT voltProcessDynamilwpdateNegative_inout[] =
{
    {
        .bVoltLoaded                     = LW_FALSE,
        .voltRailsDynamilwpdateStatus    = FLCN_OK,
        .voltPoliciesDynamilwpdateStatus = FLCN_OK,
        .expectedResult                  = FLCN_OK,
    },
    {
        .bVoltLoaded                     = LW_TRUE,
        .voltRailsDynamilwpdateStatus    = FLCN_ERR_ILWALID_INDEX,
        .voltPoliciesDynamilwpdateStatus = FLCN_OK,
        .expectedResult                  = FLCN_ERR_ILWALID_INDEX,
    },
    {
        .bVoltLoaded                     = LW_TRUE,
        .voltRailsDynamilwpdateStatus    = FLCN_OK,
        .voltPoliciesDynamilwpdateStatus = FLCN_ERR_ILWALID_INDEX,
        .expectedResult                  = FLCN_ERR_ILWALID_INDEX,
    },
};

UT_SUITE_DEFINE(PMU_OBJVOLT,
                UT_SUITE_SET_COMPONENT("Unit Volt Core")
                UT_SUITE_SET_DESCRIPTION("Unit tests for Unit Volt Core")
                UT_SUITE_SET_OWNER("ngodbole"))

UT_CASE_DEFINE(PMU_OBJVOLT, voltVoltSanityCheckPositive,
  UT_CASE_SET_DESCRIPTION("Test voltVoltSanityCheckPositive")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Pre-test setup for testing the sanity check interface for Unit.
 *             Volt Core with different set of inputs.
 *
 * @details    Before the test is run, set the input vales for mocked interfaces.
 */
PRE_TEST_METHOD(voltVoltSanityCheck)
{
    Volt.bLoaded                             = LW_TRUE;
    voltPolicySanityCheck_MOCK_CONFIG.status = FLCN_OK;

    // Mock up the VOLT_RAIL board object group.
    railObj.super.type      = LW2080_CTRL_VOLT_VOLT_DOMAIN_LOGIC;
    railObj.super.grpIdx    = 0U;
    railObjects[0]          = &railObj.super;

    Volt.railMetadata.super.super.type        = LW2080_CTRL_BOARDOBJGRP_TYPE_E32;
    Volt.railMetadata.super.super.objSlots    = 1U;
    Volt.railMetadata.super.super.ppObjects   = railObjects;
    boardObjGrpMaskInit_E32(&Volt.railMetadata.super.objMask);
    boardObjGrpMaskBitSet(&Volt.railMetadata.super.objMask, BOARDOBJ_GET_GRP_IDX(&railObj.super));

    // Mock up the VOLT_POLICY board object group.
    policyObj.super.type      = LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SINGLE_RAIL;
    policyObj.super.grpIdx    = 0U;
    policyObjects[0]          = &policyObj.super;

    Volt.policies.super.type        = LW2080_CTRL_BOARDOBJGRP_TYPE_E32;
    Volt.policies.super.objSlots    = 1U;
    Volt.policies.super.ppObjects   = policyObjects;
    boardObjGrpMaskInit_E32(&Volt.policies.objMask);
    boardObjGrpMaskBitSet(&Volt.policies.objMask, BOARDOBJ_GET_GRP_IDX(&policyObj.super));
}

/*!
 * @brief      Post-test teardown after testing sanity check interface for Unit
 *             Volt Core.
 *
 * @details    Ensure to set clean values for inputs to mocked interfaces.
 */
POST_TEST_METHOD(voltVoltSanityCheck)
{
    voltPolicyClientColwertToIdx_MOCK_CONFIG.seqPolicyIndex = 0;
    voltPolicySanityCheck_MOCK_CONFIG.status                = FLCN_OK;

    memset(&Volt, 0U, sizeof(Volt));
}

/*!
 * @brief      Test that the Unit Volt Core handles all negative inputs correctly.
 *
 * @details    The test shall ilwoke the Unit Volt Core method that sanitizes
               the voltage parameters which are provided by clients.
 *
 *             Parameters: client ID, list of voltage rails with rail parameters.
 *
 *             The test passes if the expected result in the dataset matches with
 *             the actual result received after exelwting the test.
 *
 *             Any other return value, constitutes failure.
 */
UT_CASE_RUN(PMU_OBJVOLT, voltVoltSanityCheckPositive)
{
    LwU8                            numEntriesPositive =
        sizeof(voltVoltSanityCheckPositive_inout) / sizeof(VOLT_VOLT_SANITY_CHECK_INPUT_OUTPUT);
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST railList;
    FLCN_STATUS                     status;
    LwU8                            i;

    memset(&railList, 0, sizeof(railList));

    PRE_TEST_NAME(voltVoltSanityCheck)();

    for (i = 0; i < numEntriesPositive; i++)
    {
        // Copy the data set.
        memcpy(&railList, &voltVoltSanityCheckPositive_inout[i].railList, sizeof(railList));
        Volt.bLoaded                                            = 
            voltVoltSanityCheckPositive_inout[i].bVoltLoaded;
        voltPolicyClientColwertToIdx_MOCK_CONFIG.seqPolicyIndex =
            voltVoltSanityCheckPositive_inout[i].clientID;
        voltPolicySanityCheck_MOCK_CONFIG.status                =
            voltVoltSanityCheckPositive_inout[i].voltPolicySanityCheckStatus;

        // Run the function under test.
        status = voltVoltSanityCheck_IMPL(0, &railList, LW_FALSE);

        // Verify result.
        UT_ASSERT_EQUAL_UINT(status, voltVoltSanityCheckPositive_inout[i].expectedResult);
        UT_ASSERT_EQUAL_UINT(0U, UTF_INTRINSICS_MOCK_GET_HALT_COUNT());
    }

    POST_TEST_NAME(voltVoltSanityCheck)();
}

UT_CASE_DEFINE(PMU_OBJVOLT, voltVoltSanityCheckNegative,
  UT_CASE_SET_DESCRIPTION("Test voltVoltSanityCheckNegative")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))
/*!
 * @brief      Test that the Unit Volt Core handles all negative inputs correctly
 *             and graciously.
 *
 * @details    The test shall ilwoke the Unit Volt Core method that sanitizes
               the voltage parameters which are provided by clients.
 *
 *             Parameters: client ID, list of voltage rails with rail parameters.
 *
 *             The test passes if the expected result in the dataset matches with
 *             the actual result received after exelwting the test.
 *
 *             Any other return value, constitutes failure.
 */
UT_CASE_RUN(PMU_OBJVOLT, voltVoltSanityCheckNegative)
{
    LwU8                            numEntriesNegative =
        sizeof(voltVoltSanityCheckNegative_inout) / sizeof(VOLT_VOLT_SANITY_CHECK_INPUT_OUTPUT);
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST railList;
    FLCN_STATUS                     status;
    LwU8                            i;

    memset(&railList, 0, sizeof(railList));

    PRE_TEST_NAME(voltVoltSanityCheck)();

    for (i = 0; i < numEntriesNegative; i++)
    {
        // Copy the data set.
        memcpy(&railList, &voltVoltSanityCheckNegative_inout[i].railList, sizeof(railList));
        Volt.bLoaded                                            = 
            voltVoltSanityCheckNegative_inout[i].bVoltLoaded;
        voltPolicyClientColwertToIdx_MOCK_CONFIG.seqPolicyIndex =
            voltVoltSanityCheckNegative_inout[i].clientID;
        voltPolicySanityCheck_MOCK_CONFIG.status                =
            voltVoltSanityCheckNegative_inout[i].voltPolicySanityCheckStatus;

        // Run the function under test.
        status = voltVoltSanityCheck_IMPL(0, &railList, LW_FALSE);

        // Verify result.
        UT_ASSERT_EQUAL_UINT(status, voltVoltSanityCheckNegative_inout[i].expectedResult);
    }

    POST_TEST_NAME(voltVoltSanityCheck)();
}

UT_CASE_DEFINE(PMU_OBJVOLT, voltVoltSetVoltagePositive,
  UT_CASE_SET_DESCRIPTION("Test voltVoltSetVoltagePositive")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test that the Unit Volt Core handles all positive inputs correctly.
 *
 * @details    The test shall ilwoke the Unit Volt Core method that sets voltage
 *             on a set of rails.
 *
 *             Parameters: client ID, list of voltage rails with rail parameters.
 *
 *             The test passes if the expected result in the dataset matches with
 *             the actual result received after exelwting the test.
 *
 *             Any other return value, constitutes failure.
 */
UT_CASE_RUN(PMU_OBJVOLT, voltVoltSetVoltagePositive)
{
    LwU8                            numEntriesPositive =
        sizeof(voltVoltSetVoltagePositive_inout) / sizeof(VOLT_VOLT_SET_VOLTAGE_INPUT_OUTPUT);
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST railList;
    FLCN_STATUS                     status;
    LwU8                            i;

    memset(&railList, 0, sizeof(railList));

    // Test for positive test cases.
    for (i = 0; i < numEntriesPositive; i++)
    {
        // Copy the data set.
        voltVoltSanityCheck_MOCK_CONFIG.status =
            voltVoltSetVoltagePositive_inout[i].voltVoltSanityCheckStatus;
        voltRailSetNoiseUnawareVmin_MOCK_CONFIG.status =
            voltVoltSetVoltagePositive_inout[i].voltRailSetNoiseUnawareVminStatus;
        voltPolicySetVoltage_MOCK_CONFIG.status =
            voltVoltSetVoltagePositive_inout[i].voltPolicySetVoltageStatus;

        // Run the function under test.
        status = voltVoltSetVoltage(0, &railList);

        // Verify result.
        UT_ASSERT_EQUAL_UINT(status, voltVoltSetVoltagePositive_inout[i].expectedResult);
        UT_ASSERT_EQUAL_UINT(0U, UTF_INTRINSICS_MOCK_GET_HALT_COUNT());
    }
}

UT_CASE_DEFINE(PMU_OBJVOLT, voltVoltSetVoltageNegative,
  UT_CASE_SET_DESCRIPTION("Test voltVoltSetVoltageNegative")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test that the Unit Volt Core handles all invalid inputs correctly
 *             and graciously.
 *
 * @details    The test shall ilwoke the Unit Volt Core method that sets voltage
 *             on a set of rails.
 *
 *             Parameters: client ID, list of voltage rails with rail parameters.
 *
 *             The test passes if the expected result in the dataset matches with
 *             the actual result received after exelwting the test.
 *
 *             Any other return value, constitutes failure.
 */
UT_CASE_RUN(PMU_OBJVOLT, voltVoltSetVoltageNegative)
{
    LwU8                            numEntriesNegative =
        sizeof(voltVoltSetVoltageNegative_inout) / sizeof(VOLT_VOLT_SET_VOLTAGE_INPUT_OUTPUT);
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST railList;
    FLCN_STATUS                     status;
    LwU8                            i;

    memset(&railList, 0, sizeof(railList));

    // Test for negative test cases.
    for (i = 0; i < numEntriesNegative; i++)
    {
        // Copy the data set.
        voltVoltSanityCheck_MOCK_CONFIG.status =
            voltVoltSetVoltageNegative_inout[i].voltVoltSanityCheckStatus;
        voltRailSetNoiseUnawareVmin_MOCK_CONFIG.status =
            voltVoltSetVoltageNegative_inout[i].voltRailSetNoiseUnawareVminStatus;
        voltPolicySetVoltage_MOCK_CONFIG.status =
            voltVoltSetVoltageNegative_inout[i].voltPolicySetVoltageStatus;

        // Run the function under test.
        status = voltVoltSetVoltage(0, &railList);

        // Verify result.
        UT_ASSERT_EQUAL_UINT(status, voltVoltSetVoltageNegative_inout[i].expectedResult);
    }
}

UT_CASE_DEFINE(PMU_OBJVOLT, voltProcessDynamilwpdatePositive,
  UT_CASE_SET_DESCRIPTION("Test voltProcessDynamilwpdatePositive")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test that the Unit Volt Core handles all positive inputs correctly.
 *
 * @details    The test shall ilwoke the Unit Volt Core method that does dynamic
 *             of VFE based limits.
 *
 *             Parameters: client ID, list of voltage rails with rail parameters.
 *
 *             The test passes if the expected result in the dataset matches with
 *             the actual result received after exelwting the test.
 *
 *             Any other return value, constitutes failure.
 */
UT_CASE_RUN(PMU_OBJVOLT, voltProcessDynamilwpdatePositive)
{
    LwU8                            numEntriesPositive =
        sizeof(voltProcessDynamilwpdatePositive_inout) / sizeof(VOLT_PROCESS_DYNAMIC_UPDATE_INPUT_OUTPUT);
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST railList;
    FLCN_STATUS                     status;
    LwU8                            i;

    // Test for positive test cases.
    for (i = 0; i < numEntriesPositive; i++)
    {
        // Copy the data set.
        Volt.bLoaded                                 = 
            voltProcessDynamilwpdatePositive_inout[i].bVoltLoaded;
        voltRailsDynamilwpdate_MOCK_CONFIG.status    =
            voltProcessDynamilwpdatePositive_inout[i].voltRailsDynamilwpdateStatus;
        voltPoliciesDynamilwpdate_MOCK_CONFIG.status =
            voltProcessDynamilwpdatePositive_inout[i].voltPoliciesDynamilwpdateStatus;

        // Run the function under test.
        status = voltProcessDynamilwpdate();

        // Verify result.
        UT_ASSERT_EQUAL_UINT(status, voltProcessDynamilwpdatePositive_inout[i].expectedResult);
        UT_ASSERT_EQUAL_UINT(0U, UTF_INTRINSICS_MOCK_GET_HALT_COUNT());
    }
}

UT_CASE_DEFINE(PMU_OBJVOLT, voltProcessDynamilwpdateNegative,
  UT_CASE_SET_DESCRIPTION("Test voltProcessDynamilwpdateNegative")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test that the Unit Volt Core handles all negative inputs correctly.
 *
 * @details    The test shall ilwoke the Unit Volt Core method that does dynamic
 *             of VFE based limits.
 *
 *             Parameters: client ID, list of voltage rails with rail parameters.
 *
 *             The test passes if the expected result in the dataset matches with
 *             the actual result received after exelwting the test.
 *
 *             Any other return value, constitutes failure.
 */
UT_CASE_RUN(PMU_OBJVOLT, voltProcessDynamilwpdateNegative)
{
    LwU8                            numEntriesNegative =
        sizeof(voltProcessDynamilwpdateNegative_inout) / sizeof(VOLT_PROCESS_DYNAMIC_UPDATE_INPUT_OUTPUT);
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST railList;
    FLCN_STATUS                     status;
    LwU8                            i;

    // Test for positive test cases.
    for (i = 0; i < numEntriesNegative; i++)
    {
        // Copy the data set.
        Volt.bLoaded                                 = 
            voltProcessDynamilwpdateNegative_inout[i].bVoltLoaded;
        voltRailsDynamilwpdate_MOCK_CONFIG.status    =
            voltProcessDynamilwpdateNegative_inout[i].voltRailsDynamilwpdateStatus;
        voltPoliciesDynamilwpdate_MOCK_CONFIG.status =
            voltProcessDynamilwpdateNegative_inout[i].voltPoliciesDynamilwpdateStatus;

        // Run the function under test.
        status = voltProcessDynamilwpdate();

        // Verify result.
        UT_ASSERT_EQUAL_UINT(status, voltProcessDynamilwpdateNegative_inout[i].expectedResult);
    }
}

UT_CASE_DEFINE(PMU_OBJVOLT, voltPostInitPositive,
  UT_CASE_SET_DESCRIPTION("Test voltPostInitPositive")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test that the Unit Volt Core handles post initialization correctly.
 *
 * @details    The test shall ilwoke the Unit Volt Core method that does VOLT post-initialization.
 *
 *             Parameters: None.
 *
 *             The test passes if the expected result in the dataset matches with
 *             the actual result received after exelwting the test.
 *
 *             Any other return value, constitutes failure.
 */
UT_CASE_RUN(PMU_OBJVOLT, voltPostInitPositive)
{
    FLCN_STATUS status;

    if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_SEMAPHORE))
    {
        // Report NO_FREE_MEM when perfSemaphoreRWInitialize returns NULL
        osSemaphoreCreateRW_MOCK_fake.return_val = NULL;
        status = voltPostInit_IMPL();
        UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_NO_FREE_MEM);
    }
}

UT_CASE_DEFINE(PMU_OBJVOLT, voltPostInitNegative,
  UT_CASE_SET_DESCRIPTION("Test voltPostInitNegative")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test that the Unit Volt Core handles post initialization with negative inputs correctly.
 *
 * @details    The test shall ilwoke the Unit Volt Core method that does VOLT post-initialization.
 *
 *             Parameters: None.
 *
 *             The test passes if the expected result in the dataset matches with
 *             the actual result received after exelwting the test.
 *
 *             Any other return value, constitutes failure.
 */
UT_CASE_RUN(PMU_OBJVOLT, voltPostInitNegative)
{
    FLCN_STATUS     status;
    SEMAPHORE_RW    semaphore;

    // Report FLCN_OK if all goes well
    osSemaphoreCreateRW_MOCK_fake.return_val = &semaphore;
    status = voltPostInit_IMPL();
    UT_ASSERT_EQUAL_UINT(status, FLCN_OK);
}


