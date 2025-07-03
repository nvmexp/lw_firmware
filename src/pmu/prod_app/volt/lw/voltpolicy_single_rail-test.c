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
 * @file    voltpolicy_single_rail-test.c
 * @brief   Unit tests for logic in VOLT_POLICY_SINGLE_RAIL.
 */
/* ------------------------ System Includes --------------------------------- */
#include "test-macros.h"

/* ------------------------ Application Includes ---------------------------- */
#include "volt/objvolt.h"
#include "volt/objvolt-test.h"
#include "volt/voltpolicy-mock.h"
#include "volt/voltpolicy_single_rail-mock.h"
#include "volt/voltrail-mock.h"

UT_SUITE_DEFINE(PMU_VOLTPOLICYSINGLERAIL,
                UT_SUITE_SET_COMPONENT("Unit Volt Policy")
                UT_SUITE_SET_DESCRIPTION("Unit tests for Unit Volt Policy Single Rail class")
                UT_SUITE_SET_OWNER("ngodbole"))
                
UT_CASE_DEFINE(PMU_VOLTPOLICYSINGLERAIL, voltPolicyLoadSingleRailSuccess,
  UT_CASE_SET_DESCRIPTION("Test voltPolicyLoadSingleRailSuccess")
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
UT_CASE_RUN(PMU_VOLTPOLICYSINGLERAIL, voltPolicyLoadSingleRailSuccess)
{
    FLCN_STATUS status;
    VOLT_POLICY_SINGLE_RAIL voltPolicy;

    voltPolicy.super.super.type = LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SINGLE_RAIL;
    boardObjDynamicCastMockInit();
    boardObjDynamicCastMockAddEntry(0, (void *)&voltPolicy);

    RESET_FAKE(voltRailGetVoltageInternal_MOCK);
    voltRailGetVoltageInternal_MOCK_fake.return_val = FLCN_OK;
    status = voltPolicyLoad_SINGLE_RAIL_IMPL(&voltPolicy);
    UT_ASSERT_EQUAL_UINT(status, FLCN_OK);
}

UT_CASE_DEFINE(PMU_VOLTPOLICYSINGLERAIL, voltPolicyLoadSingleRailFail,
  UT_CASE_SET_DESCRIPTION("Test voltPolicyLoadSingleRailFail")
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
UT_CASE_RUN(PMU_VOLTPOLICYSINGLERAIL, voltPolicyLoadSingleRailFail)
{
    FLCN_STATUS status;
    VOLT_POLICY_SINGLE_RAIL voltPolicy;

    voltPolicy.super.super.type = LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SINGLE_RAIL;
    boardObjDynamicCastMockInit();
    boardObjDynamicCastMockAddEntry(0, (void *)&voltPolicy);

    RESET_FAKE(voltRailGetVoltageInternal_MOCK);
    voltRailGetVoltageInternal_MOCK_fake.return_val = FLCN_ERR_STATE_RESET_NEEDED;
    status = voltPolicyLoad_SINGLE_RAIL_IMPL(&voltPolicy);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_STATE_RESET_NEEDED);
}

UT_CASE_DEFINE(PMU_VOLTPOLICYSINGLERAIL, voltPolicySetVoltageSingleRailSuccess,
  UT_CASE_SET_DESCRIPTION("Test voltPolicySetVoltageSingleRailSuccess")
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
UT_CASE_RUN(PMU_VOLTPOLICYSINGLERAIL, voltPolicySetVoltageSingleRailSuccess)
{
    FLCN_STATUS                     status;
    VOLT_POLICY_SINGLE_RAIL         voltPolicy;
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST railList;


    memset(&railList, 0, sizeof(LW2080_CTRL_VOLT_VOLT_RAIL_LIST));
    voltPolicy.super.super.type = LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SINGLE_RAIL;
    boardObjDynamicCastMockInit();
    boardObjDynamicCastMockAddEntry(0, (void *)&voltPolicy);

    railList.rails[0].voltageuV                 = 400;
    railList.rails[0].voltOffsetuV[0]           = 200;
    voltRailRoundVoltage_MOCK_CONFIG.roundedVoltageuV = 400;
    voltRailRoundVoltage_MOCK_CONFIG.status     = FLCN_OK;
    voltRailGetVoltageMax_MOCK_CONFIG.maxLimituV = 500;
    voltRailGetVoltageMax_MOCK_CONFIG.status     = FLCN_OK;
    RESET_FAKE(voltRailSetVoltage_MOCK);
    voltRailSetVoltage_MOCK_fake.return_val = FLCN_OK;

    status = voltPolicySetVoltage_SINGLE_RAIL_IMPL(&voltPolicy, 1, &railList);
    UT_ASSERT_EQUAL_UINT(voltPolicy.lwrrVoltuV, 400);
    UT_ASSERT_EQUAL_UINT(status, FLCN_OK);
}

UT_CASE_DEFINE(PMU_VOLTPOLICYSINGLERAIL, voltPolicySetVoltageSingleRailRoundingFail,
  UT_CASE_SET_DESCRIPTION("Test voltPolicySetVoltageSingleRailRoundingFail")
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
UT_CASE_RUN(PMU_VOLTPOLICYSINGLERAIL, voltPolicySetVoltageSingleRailRoundingFail)
{
    FLCN_STATUS                     status;
    VOLT_POLICY_SINGLE_RAIL         voltPolicy;
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST railList;
    LwU32                           maxLimituV;

    memset(&railList, 0, sizeof(LW2080_CTRL_VOLT_VOLT_RAIL_LIST));
    voltPolicy.super.super.type = LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SINGLE_RAIL;
    boardObjDynamicCastMockInit();
    boardObjDynamicCastMockAddEntry(0, (void *)&voltPolicy);

    railList.rails[0].voltageuV                 = 400;
    railList.rails[0].voltOffsetuV[0]           = 200;
    voltRailRoundVoltage_MOCK_CONFIG.roundedVoltageuV = 400;
    voltRailRoundVoltage_MOCK_CONFIG.status     = FLCN_ERR_NOT_SUPPORTED;

    status = voltPolicySetVoltage_SINGLE_RAIL_IMPL(&voltPolicy, 1, &railList);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_NOT_SUPPORTED);
}

UT_CASE_DEFINE(PMU_VOLTPOLICYSINGLERAIL, voltPolicySetVoltageSingleRailMaxFail,
  UT_CASE_SET_DESCRIPTION("Test voltPolicySetVoltageSingleRailMaxFail")
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
UT_CASE_RUN(PMU_VOLTPOLICYSINGLERAIL, voltPolicySetVoltageSingleRailMaxFail)
{
    FLCN_STATUS                     status;
    VOLT_POLICY_SINGLE_RAIL         voltPolicy;
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST railList;
    LwU32                           maxLimituV;

    memset(&railList, 0, sizeof(LW2080_CTRL_VOLT_VOLT_RAIL_LIST));
    voltPolicy.super.super.type = LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SINGLE_RAIL;
    boardObjDynamicCastMockInit();
    boardObjDynamicCastMockAddEntry(0, (void *)&voltPolicy);

    railList.rails[0].voltageuV                 = 400;
    railList.rails[0].voltOffsetuV[0]           = 200;
    voltRailRoundVoltage_MOCK_CONFIG.roundedVoltageuV = 400;
    voltRailRoundVoltage_MOCK_CONFIG.status     = FLCN_OK;
    maxLimituV                                  = 500;
    voltRailGetVoltageMax_MOCK_CONFIG.maxLimituV = 500;
    voltRailGetVoltageMax_MOCK_CONFIG.status     = FLCN_ERR_NOT_SUPPORTED;
    RESET_FAKE(voltRailSetVoltage_MOCK);
    voltRailSetVoltage_MOCK_fake.return_val = FLCN_OK;

    status = voltPolicySetVoltage_SINGLE_RAIL_IMPL(&voltPolicy, 1, &railList);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_NOT_SUPPORTED);
}

UT_CASE_DEFINE(PMU_VOLTPOLICYSINGLERAIL, voltPolicySetVoltageSingleRailRailSetVoltageFail,
  UT_CASE_SET_DESCRIPTION("Test voltPolicySetVoltageSingleRailRailSetVoltageFail")
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
UT_CASE_RUN(PMU_VOLTPOLICYSINGLERAIL, voltPolicySetVoltageSingleRailRailSetVoltageFail)
{
    FLCN_STATUS                     status;
    VOLT_POLICY_SINGLE_RAIL         voltPolicy;
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST railList;
    LwU32                           maxLimituV;

    memset(&railList, 0, sizeof(LW2080_CTRL_VOLT_VOLT_RAIL_LIST));
    voltPolicy.super.super.type = LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SINGLE_RAIL;
    boardObjDynamicCastMockInit();
    boardObjDynamicCastMockAddEntry(0, (void *)&voltPolicy);

    railList.rails[0].voltageuV                 = 400;
    railList.rails[0].voltOffsetuV[0]           = 200;
    voltRailRoundVoltage_MOCK_CONFIG.roundedVoltageuV = 400;
    voltRailRoundVoltage_MOCK_CONFIG.status     = FLCN_OK;
    maxLimituV                                  = 500;
    voltRailGetVoltageMax_MOCK_CONFIG.maxLimituV = 500;
    voltRailGetVoltageMax_MOCK_CONFIG.status     = FLCN_OK;
    RESET_FAKE(voltRailSetVoltage_MOCK);
    voltRailSetVoltage_MOCK_fake.return_val = FLCN_ERR_NOT_SUPPORTED;

    status = voltPolicySetVoltage_SINGLE_RAIL_IMPL(&voltPolicy, 1, &railList);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_NOT_SUPPORTED);
}
