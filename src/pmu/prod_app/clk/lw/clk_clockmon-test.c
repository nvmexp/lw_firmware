/*
 * Copyright (c) 2020-2021, LWPU CORPORATION. All rights reserved.
 *
 * LWPU CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from LWPU CORPORATION is strictly prohibited.
 */

/*!
 * @file    clk_clockmon-test.c
 * @brief   Unit tests for logic in Clocks-HW Clock monitors.
 */

/* ------------------------ System Includes --------------------------------- */
#include "test-macros.h"
#include "regmock.h"

/* ------------------------ Application Includes ---------------------------- */
#include "pmu_objclk.h"
#include "clk/clk_clockmon.h"
#include "clk/pmu_clk-test.h"
#include "dev_trim.h"
#include "perf/3x/vfe_equ-mock.h"

/* ------------------------ Globals ---------------------------------------- */
CLK_DOMAIN clkDomain;
CHANGE_SEQ_SCRIPT script = { 0U };
LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_DATA_ALIGNED StepLwrr = { 0U };
BOARDOBJ *pClkDomains[1];
CHANGE_SEQ changeSeq;
LW2080_CTRL_CLK_CLK_DOMAIN_LIST clkDomainList;
LW2080_CTRL_VOLT_VOLT_RAIL_LIST voltRailList;

/*!
 * @brief Mapping between the clk api domain and the various clock monitor registers
 */
static CLK_CLOCK_MON_ADDRESS_MAP _clockMonMap_TU10X[] =
{
    {
        LW2080_CTRL_CLK_DOMAIN_GPCCLK,
        {
            LW_PTRIM_GPC_BCAST_FMON_THRESHOLD_HIGH_GPCCLK_DIV2,
            LW_PTRIM_GPC_BCAST_FMON_THRESHOLD_LOW_GPCCLK_DIV2,
            LW_PTRIM_GPC_BCAST_FMON_REF_WINDOW_COUNT_GPCCLK_DIV2,
            LW_PTRIM_GPC_BCAST_FMON_REF_WINDOW_DC_CHECK_COUNT_GPCCLK_DIV2,
            LW_PTRIM_GPC_BCAST_FMON_CONFIG_GPCCLK_DIV2,
            LW_PTRIM_GPC_BCAST_FMON_ENABLE_STATUS_GPCCLK_DIV2,
            LW_PTRIM_GPC_BCAST_FMON_FAULT_STATUS_GPCCLK_DIV2,
            LW_PTRIM_GPC_BCAST_FMON_CLEAR_COUNTER_GPCCLK_DIV2,
        }
    },
    {
        LW2080_CTRL_CLK_DOMAIN_SYSCLK,
        {
            LW_PTRIM_SYS_FMON_THRESHOLD_HIGH_SYSCLK,
            LW_PTRIM_SYS_FMON_THRESHOLD_LOW_SYSCLK,
            LW_PTRIM_SYS_FMON_REF_WINDOW_COUNT_SYSCLK,
            LW_PTRIM_SYS_FMON_REF_WINDOW_DC_CHECK_COUNT_SYSCLK,
            LW_PTRIM_SYS_FMON_CONFIG_SYSCLK,
            LW_PTRIM_SYS_FMON_ENABLE_STATUS_SYSCLK,
            LW_PTRIM_SYS_FMON_FAULT_STATUS_SYSCLK,
            LW_PTRIM_SYS_FMON_CLEAR_COUNTER_SYSCLK,
        }
    },
    {
        LW2080_CTRL_CLK_DOMAIN_HUBCLK,
        {
            LW_PTRIM_SYS_FMON_THRESHOLD_HIGH_HUBCLK,
            LW_PTRIM_SYS_FMON_THRESHOLD_LOW_HUBCLK,
            LW_PTRIM_SYS_FMON_REF_WINDOW_COUNT_HUBCLK,
            LW_PTRIM_SYS_FMON_REF_WINDOW_DC_CHECK_COUNT_HUBCLK,
            LW_PTRIM_SYS_FMON_CONFIG_HUBCLK,
            LW_PTRIM_SYS_FMON_ENABLE_STATUS_HUBCLK,
            LW_PTRIM_SYS_FMON_FAULT_STATUS_HUBCLK,
            LW_PTRIM_SYS_FMON_CLEAR_COUNTER_HUBCLK,
        }
    },
#if PMUCFG_FEATURE_ENABLED(PMU_HOSTCLK_PRESENT)
    {
        LW2080_CTRL_CLK_DOMAIN_HOSTCLK,
        {
            LW_PTRIM_SYS_FMON_THRESHOLD_HIGH_HOSTCLK,
            LW_PTRIM_SYS_FMON_THRESHOLD_LOW_HOSTCLK,
            LW_PTRIM_SYS_FMON_REF_WINDOW_COUNT_HOSTCLK,
            LW_PTRIM_SYS_FMON_REF_WINDOW_DC_CHECK_COUNT_HOSTCLK,
            LW_PTRIM_SYS_FMON_CONFIG_HOSTCLK,
            LW_PTRIM_SYS_FMON_ENABLE_STATUS_HOSTCLK,
            LW_PTRIM_SYS_FMON_FAULT_STATUS_HOSTCLK,
            LW_PTRIM_SYS_FMON_CLEAR_COUNTER_HOSTCLK,
        }
    },
#endif
    {
        LW2080_CTRL_CLK_DOMAIN_XBARCLK,
        {
            LW_PTRIM_SYS_FMON_THRESHOLD_HIGH_XBARCLK_DIV2,
            LW_PTRIM_SYS_FMON_THRESHOLD_LOW_XBARCLK_DIV2,
            LW_PTRIM_SYS_FMON_REF_WINDOW_COUNT_XBARCLK_DIV2,
            LW_PTRIM_SYS_FMON_REF_WINDOW_DC_CHECK_COUNT_XBARCLK_DIV2,
            LW_PTRIM_SYS_FMON_CONFIG_XBARCLK_DIV2,
            LW_PTRIM_SYS_FMON_ENABLE_STATUS_XBARCLK_DIV2,
            LW_PTRIM_SYS_FMON_FAULT_STATUS_XBARCLK_DIV2,
            LW_PTRIM_SYS_FMON_CLEAR_COUNTER_XBARCLK_DIV2,
        }
    },
    {
        LW2080_CTRL_CLK_DOMAIN_LWDCLK,
        {
            LW_PTRIM_SYS_FMON_THRESHOLD_HIGH_LWDCLK,
            LW_PTRIM_SYS_FMON_THRESHOLD_LOW_LWDCLK,
            LW_PTRIM_SYS_FMON_REF_WINDOW_COUNT_LWDCLK,
            LW_PTRIM_SYS_FMON_REF_WINDOW_DC_CHECK_COUNT_LWDCLK,
            LW_PTRIM_SYS_FMON_CONFIG_LWDCLK,
            LW_PTRIM_SYS_FMON_ENABLE_STATUS_LWDCLK,
            LW_PTRIM_SYS_FMON_FAULT_STATUS_LWDCLK,
            LW_PTRIM_SYS_FMON_CLEAR_COUNTER_LWDCLK,
        }
    },
    {
        LW2080_CTRL_CLK_DOMAIN_MCLK,
        {
            LW_PTRIM_FBPA_BCAST_FMON_THRESHOLD_HIGH_DRAMCLK,
            LW_PTRIM_FBPA_BCAST_FMON_THRESHOLD_LOW_DRAMCLK,
            LW_PTRIM_FBPA_BCAST_FMON_REF_WINDOW_COUNT_DRAMCLK,
            LW_PTRIM_FBPA_BCAST_FMON_REF_WINDOW_DC_CHECK_COUNT_DRAMCLK,
            LW_PTRIM_FBPA_BCAST_FMON_CONFIG_DRAMCLK,
            LW_PTRIM_FBPA_BCAST_FMON_ENABLE_STATUS_DRAMCLK,
            LW_PTRIM_FBPA_BCAST_FMON_FAULT_STATUS_DRAMCLK,
            LW_PTRIM_FBPA_BCAST_FMON_CLEAR_COUNTER_DRAMCLK,
        }
    },
};

/* ------------------------ Defines and Macros ------------------------------ */
#define CLK_CLOCK_MON_SUPPORTED_NUM  LW_ARRAY_ELEMENTS(_clockMonMap_TU10X)

#define CLK_CLOCK_MON_REG_GET_TU10X(idx,_type)                                \
    (_clockMonMap_TU10X[idx].regAddr[CLK_CLOCK_MON_REG_TYPE_##_type])

/* ------------------------ Type Definitions -------------------------------- */
/*!
 * @brief      Static helper to initialize all the CLKMON devices before
 *             individual tests can use them.
 */
static void clkMonSetup(void);

/*!
 * @brief      Static helper to clean-up the CLKMON devices after
 *             the individual tests are done.
 */
static void clkMonTeardown(void);

/* ------------------------ Local Data -------------------------------------- */
/* ------------------------ Test Suite Declaration--------------------------- */
/*!
 * @brief      Definition of the Unit Clocks-HW CLKMON test suite.
 */
UT_SUITE_DEFINE(PMU_CLKMON,
                UT_SUITE_SET_COMPONENT("Unit Clock Monitors")
                UT_SUITE_SET_DESCRIPTION("Test suite for the Clocks-HW Clock Monitors unit")
                UT_SUITE_SET_OWNER("kwadhwa")
                UT_SUITE_SETUP_HOOK((void *)&clkMonSetup)
                UT_SUITE_TEARDOWN_HOOK((void *)&clkMonTeardown))

/*!
 * @brief      Pre-test setup for the CLKMON test suite.
 *
 * @details    Before any of the individual test case can run, we need to have
 *             the global 'Clk' variable initialized with the required ClkMon
 *             config params set up correctly.
 */
static void clkMonSetup(void)
{
    // Initialize the Clock Domains
    clkDomain.domain = clkWhich_GpcClk;
    clkDomain.apiDomain = LW2080_CTRL_CLK_DOMAIN_GPCCLK;
    clkDomain.perfDomainGrpIdx = 0;
    clkDomain.super.super.grpIdx = 0;

    // Initialize the Clock Domain BoardObj pointers
    pClkDomains[0] = (BOARDOBJ*)&clkDomain;

    // Initialize the ObjClk
    memset(&Clk, 0x00U, sizeof(Clk));
    Clk.domains.super.super.type = LW2080_CTRL_BOARDOBJGRP_TYPE_E32;
    Clk.domains.super.super.classId = LW2080_CTRL_BOARDOBJGRP_CLASS_ID_CLK_CLK_DOMAIN;
    Clk.domains.super.super.objSlots = 1;
    Clk.domains.super.super.bConstructed = LW_TRUE;
    Clk.domains.super.super.ppObjects = pClkDomains;
    Clk.domains.clkMonRefWinUsec = 100U;

    boardObjGrpMaskInit_E32(&(Clk.domains.super.objMask));
    boardObjGrpMaskInit_E32(&(Clk.domains.clkMonDomainsMask));
    boardObjGrpMaskBitSet(&(Clk.domains.super.objMask), BOARDOBJ_GET_GRP_IDX(pClkDomains[0]));
    boardObjGrpMaskBitSet(&(Clk.domains.clkMonDomainsMask), BOARDOBJ_GET_GRP_IDX(pClkDomains[0]));

    // Initialize the Change Sequencer Script
    memset(&script, 0x00U, sizeof(script));
    memset(&StepLwrr, 0x00U, sizeof(StepLwrr));
    script.pStepLwrr = &StepLwrr;

    script.pStepLwrr->data.clkMon.super.stepId = LW2080_CTRL_PERF_CHANGE_SEQ_35_STEP_ID_CLK_MON;
    script.pStepLwrr->data.clkMon.clkMonList.numDomains = 1;
    script.pStepLwrr->data.clkMon.clkMonList.clkDomains[0].clkApiDomain = LW2080_CTRL_CLK_DOMAIN_GPCCLK;
    script.pStepLwrr->data.clkMon.clkMonList.clkDomains[0].clkFreqMHz = 1000U;
    script.pStepLwrr->data.clkMon.clkMonList.clkDomains[0].lowThresholdPercent = LW_TYPES_U32_TO_UFXP_X_Y(20, 12, 2);
    script.pStepLwrr->data.clkMon.clkMonList.clkDomains[0].highThresholdPercent = LW_TYPES_U32_TO_UFXP_X_Y(20, 12, 5);

    // Initialize changeSeq
    (void)memset(&changeSeq, 0x00U, sizeof(changeSeq));
    boardObjGrpMaskInit_E32(&(script.clkDomainsActiveMask));
    boardObjGrpMaskBitSet(&(script.clkDomainsActiveMask), 0);

    // Initialize clock domain list
    (void)memset(&clkDomainList, 0x00U, sizeof(clkDomainList));
    clkDomainList.numDomains = 1U;
    clkDomainList.clkDomains[0].clkDomain  = LW2080_CTRL_CLK_DOMAIN_GPCCLK;
    clkDomainList.clkDomains[0].clkFreqKHz = 1000000U;
    clkDomainList.clkDomains[0].regimeId   = LW2080_CTRL_CLK_NAFLL_REGIME_ID_FFR;
    clkDomainList.clkDomains[0].source     = LW2080_CTRL_CLK_PROG_1X_SOURCE_NAFLL;

    // Initialize volt rail list
    (void)memset(&voltRailList, 0x00U, sizeof(voltRailList));
    voltRailList.numRails = 1U;
    voltRailList.rails[0].railIdx   = 0U;
    voltRailList.rails[0].voltageuV = 800000U;
}

/*!
 * @brief      Post-test clean-up for the CLKMON test suite.
 *
 * @details    Once the ClkMon test suite run is complete, we can go ahead and
 *             clean-up all the global variables and reset them to zeroes.
 */
static void clkMonTeardown(void)
{
    (void)memset(&Clk, 0x00U, sizeof(Clk));
    (void)memset(&script, 0x00U, sizeof(script));
    (void)memset(&changeSeq, 0x00U, sizeof(changeSeq));
    (void)memset(&clkDomainList, 0x00U, sizeof(clkDomainList));
    (void)memset(&voltRailList, 0x00U, sizeof(voltRailList));
}

/*!
 * @brief      Test the method to check the clock monitor primary fault
 *             status - Success case
 */
UT_CASE_DEFINE(PMU_CLKMON, clkClockMonSanityCheckFaultStatusAll_test1,
  UT_CASE_SET_DESCRIPTION("Method to check the Primary FAULT STATUS")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Pre-test setup for clkClockMonSanityCheckFaultStatusAll_test1
 *
 * @details    Mock and initialize the required register.
 */
PRE_TEST_METHOD(clkClockMonSanityCheckFaultStatusAll_test1)
{
    LwU32 fmonPrimaryStatusReg = LW_PTRIM_SYS_FMON_MASTER_STATUS_REGISTER;
    UTF_IO_MOCK(fmonPrimaryStatusReg,  fmonPrimaryStatusReg,  NULL, NULL);

    // Initialize the '_FINAL_FMON_FAULT_OUT_STATUS' field to '_FALSE'
    REG_WR32(FECS, fmonPrimaryStatusReg, 0x0U);
}

/*!
 * @brief      Test that the Unit Clock-Monitors successfully checks if primary
 *             status register is cleared
 */
UT_CASE_RUN(PMU_CLKMON, clkClockMonSanityCheckFaultStatusAll_test1)
{
    FLCN_STATUS status;
    LwU32 fmonPrimaryStatusReg = LW_PTRIM_SYS_FMON_MASTER_STATUS_REGISTER;

    PRE_TEST_NAME(clkClockMonSanityCheckFaultStatusAll_test1)();

    // Execute unit under test
    status = clkClockMonSanityCheckFaultStatusAll();
    UT_ASSERT_EQUAL_UINT(status, FLCN_OK);
}

/*!
 * @brief      Test the method to check the clock monitor primary fault
 *             status - Failure case
 */
UT_CASE_DEFINE(PMU_CLKMON, clkClockMonSanityCheckFaultStatusAll_test2,
  UT_CASE_SET_DESCRIPTION("Method to check the Primary FAULT STATUS")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Pre-test setup for clkClockMonSanityCheckFaultStatusAll_test2
 *
 * @details    Mock and initialize the required register.
 */
PRE_TEST_METHOD(clkClockMonSanityCheckFaultStatusAll_test2)
{
    LwU32 fmonPrimaryStatusReg = LW_PTRIM_SYS_FMON_MASTER_STATUS_REGISTER;
    UTF_IO_MOCK(fmonPrimaryStatusReg,  fmonPrimaryStatusReg,  NULL, NULL);

    // Initialize the '_FINAL_FMON_FAULT_OUT_STATUS' field to '_TRUE'
    REG_WR32(FECS, fmonPrimaryStatusReg, 0x1U);
}

/*!
 * @brief      Test that the Unit Clock-Monitors successfully checks if primary
 *             status register is cleared
 */
UT_CASE_RUN(PMU_CLKMON, clkClockMonSanityCheckFaultStatusAll_test2)
{
    FLCN_STATUS status;
    LwU32 fmonPrimaryStatusReg = LW_PTRIM_SYS_FMON_MASTER_STATUS_REGISTER;

    PRE_TEST_NAME(clkClockMonSanityCheckFaultStatusAll_test2)();

    // Execute unit under test
    status = clkClockMonSanityCheckFaultStatusAll();
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_ILWALID_STATE);
}

/*!
 * @brief      Test the method to enable/disable the clock monitor around perf
 *             change - Enable case
 */
UT_CASE_DEFINE(PMU_CLKMON, clkClockMonsHandlePrePostPerfChangeNotification_test1,
  UT_CASE_SET_DESCRIPTION("Method to enable/disable the clock monitor")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Pre-test setup for clkClockMonsHandlePrePostPerfChangeNotification_test1
 *
 * @details    Mock and initialize the required register.
 */
PRE_TEST_METHOD(clkClockMonsHandlePrePostPerfChangeNotification_test1)
{
    LwU32 fmonConfigReg;
    LwU32 fmonStatusReg;

    fmonConfigReg = CLK_CLOCK_MON_REG_GET_TU10X((0), FMON_CONFIG);
    fmonStatusReg = CLK_CLOCK_MON_REG_GET_TU10X((0), FMON_ENABLE_STATUS);

    UTF_IO_MOCK(fmonConfigReg,  fmonConfigReg,  NULL, NULL);
    UTF_IO_MOCK(fmonStatusReg,  fmonStatusReg,  NULL, NULL);

    // Initialize all the '_CONFIG' fields to '_DISABLE'
    REG_WR32(FECS, fmonConfigReg, 0x0U);
}

/*!
 * @brief      Test that the Unit Clock-Monitors successfully enables the clock
 *             monitor for a given clock domain
 */
UT_CASE_RUN(PMU_CLKMON, clkClockMonsHandlePrePostPerfChangeNotification_test1)
{
    FLCN_STATUS status;
    LwU32 fmonConfigReg;
    CLK_DOMAIN *pDomain;
    LwU32 i;

    PRE_TEST_NAME(clkClockMonsHandlePrePostPerfChangeNotification_test1)();

    // Execute unit under test
    // PS: LW_FALSE to the API translates to clock monitor enable
    status = clkClockMonsHandlePrePostPerfChangeNotification(LW_FALSE);
    UT_ASSERT_EQUAL_UINT(status, FLCN_OK);
    fmonConfigReg = CLK_CLOCK_MON_REG_GET_TU10X((0), FMON_CONFIG);

    // Verify all the '_CONFIG' fields get set to '_ENABLE'
    UT_ASSERT_EQUAL_UINT(REG_RD32(FECS, fmonConfigReg), 0x7FU);
}

/*!
 * @brief      Test the method to enable/disable the clock monitor around perf
 *             change - Disable case
 */
UT_CASE_DEFINE(PMU_CLKMON, clkClockMonsHandlePrePostPerfChangeNotification_test2,
  UT_CASE_SET_DESCRIPTION("Method to enable/disable the clock monitor")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Pre-test setup for clkClockMonsHandlePrePostPerfChangeNotification_test2
 *
 * @details    Mock and initialize the required register.
 */
PRE_TEST_METHOD(clkClockMonsHandlePrePostPerfChangeNotification_test2)
{
    LwU32 fmonConfigReg;
    LwU32 fmonStatusReg;

    fmonConfigReg = CLK_CLOCK_MON_REG_GET_TU10X((0), FMON_CONFIG);
    fmonStatusReg = CLK_CLOCK_MON_REG_GET_TU10X((0), FMON_ENABLE_STATUS);

    UTF_IO_MOCK(fmonConfigReg,  fmonConfigReg,  NULL, NULL);
    UTF_IO_MOCK(fmonStatusReg,  fmonStatusReg,  NULL, NULL);

    // Initialize all the '_CONFIG' fields to '_ENABLE'
    REG_WR32(FECS, fmonConfigReg, 0x7FU);
}

/*!
 * @brief      Test that the Unit Clock-Monitors successfully enables the clock
 *             monitor for a given clock domain
 */
UT_CASE_RUN(PMU_CLKMON, clkClockMonsHandlePrePostPerfChangeNotification_test2)
{
    FLCN_STATUS status;
    LwU32 fmonConfigReg;

    PRE_TEST_NAME(clkClockMonsHandlePrePostPerfChangeNotification_test2)();

    // Execute unit under test
    // PS: LW_TRUE to the API translates to clock monitor disable
    status = clkClockMonsHandlePrePostPerfChangeNotification(LW_TRUE);
    UT_ASSERT_EQUAL_UINT(status, FLCN_OK);
    fmonConfigReg = CLK_CLOCK_MON_REG_GET_TU10X((0), FMON_CONFIG);

    // Verify all the '_CONFIG' fields get set to '_DISABLE'
    UT_ASSERT_EQUAL_UINT(REG_RD32(FECS, fmonConfigReg), 0x7EU);
}

/*!
 * @brief      Test the method to program the clock monitor thresholds
 */
UT_CASE_DEFINE(PMU_CLKMON, clkClockMonsProgram_test,
  UT_CASE_SET_DESCRIPTION("Method to program the clock monitor thresholds")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Pre-test setup for clkClockMonsProgram_test
 *
 * @details    Mock and initialize the required register.
 */
PRE_TEST_METHOD(clkClockMonsProgram_test)
{
    LwU32 fmonRefWindowReg;
    LwU32 fmonDcCheckReg;
    LwU32 fmonHighThreshReg;
    LwU32 fmonLowThreshReg;

    fmonRefWindowReg = CLK_CLOCK_MON_REG_GET_TU10X((0), FMON_REF_WINDOW_COUNT);
    fmonDcCheckReg = CLK_CLOCK_MON_REG_GET_TU10X((0), FMON_REF_WINDOW_DC_CHECK_COUNT);
    fmonHighThreshReg = CLK_CLOCK_MON_REG_GET_TU10X((0), FMON_THRESHOLD_HIGH);
    fmonLowThreshReg = CLK_CLOCK_MON_REG_GET_TU10X((0), FMON_THRESHOLD_LOW);

    UTF_IO_MOCK(fmonRefWindowReg,  fmonRefWindowReg,  NULL, NULL);
    UTF_IO_MOCK(fmonDcCheckReg,  fmonDcCheckReg,  NULL, NULL);
    UTF_IO_MOCK(fmonHighThreshReg,  fmonHighThreshReg,  NULL, NULL);
    UTF_IO_MOCK(fmonLowThreshReg,  fmonLowThreshReg,  NULL, NULL);

    // Initialize all the registers to '0'
    REG_WR32(FECS, fmonRefWindowReg, 0x0U);
    REG_WR32(FECS, fmonDcCheckReg, 0x0U);
    REG_WR32(FECS, fmonHighThreshReg, 0x0U);
    REG_WR32(FECS, fmonLowThreshReg, 0x0U);
}

/*!
 * @brief      Test that the Unit Clock-Monitors successfully programs the
               monitor threshold parameter for the given clock monitor
 */
UT_CASE_RUN(PMU_CLKMON, clkClockMonsProgram_test)
{
    FLCN_STATUS status;
    LwU32 fmonRefWindowReg;
    LwU32 fmonDcCheckReg;
    LwU32 fmonHighThreshReg;
    LwU32 fmonLowThreshReg;

    PRE_TEST_NAME(clkClockMonsProgram_test)();

    // Execute unit under test
    status = clkClockMonsProgram((LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_CLK_MON *)(&(script.pStepLwrr->data.super)));
    UT_ASSERT_EQUAL_UINT(status, FLCN_OK);

    // Verify all the thresholds are programmed correctly
    fmonRefWindowReg = CLK_CLOCK_MON_REG_GET_TU10X((0), FMON_REF_WINDOW_COUNT);
    fmonDcCheckReg = CLK_CLOCK_MON_REG_GET_TU10X((0), FMON_REF_WINDOW_DC_CHECK_COUNT);
    fmonHighThreshReg = CLK_CLOCK_MON_REG_GET_TU10X((0), FMON_THRESHOLD_HIGH);
    fmonLowThreshReg = CLK_CLOCK_MON_REG_GET_TU10X((0), FMON_THRESHOLD_LOW);

    UT_ASSERT_EQUAL_UINT(REG_RD32(FECS, fmonRefWindowReg), 0xA8LW);
    UT_ASSERT_EQUAL_UINT(REG_RD32(FECS, fmonDcCheckReg), 0x4BFU);
    UT_ASSERT_EQUAL_UINT(REG_RD32(FECS, fmonHighThreshReg), 0x927C0U);
    UT_ASSERT_EQUAL_UINT(REG_RD32(FECS, fmonLowThreshReg), 0x186A0U);
}

/*!
 * @brief      Test the method to evaluate the clock monitor threshold values
 */
UT_CASE_DEFINE(PMU_CLKMON, clkClockMonsThresholdEvaluate_test1,
  UT_CASE_SET_DESCRIPTION("Method to evaluate the clock monitor threshold parameters")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Pre-test setup for clkClockMonsThresholdEvaluate_test1
 *
 * @details    Initialize the vfe mock config
 */
PRE_TEST_METHOD(clkClockMonsThresholdEvaluate_test1)
{
    vfeEquEvaluate_MOCK_CONFIG.result = 10U;
    vfeEquEvaluate_MOCK_CONFIG.status = FLCN_OK;

    script.pStepLwrr->data.clkMon.clkMonList.clkDomains[0].lowThresholdPercent = 0U;
    script.pStepLwrr->data.clkMon.clkMonList.clkDomains[0].highThresholdPercent = 0U;
}

/*!
 * @brief      Test that the Unit Clock-Monitors successfully evaluates the
               monitor threshold parameter for the given clock monitor
 */
UT_CASE_RUN(PMU_CLKMON, clkClockMonsThresholdEvaluate_test1)
{
    FLCN_STATUS status;

    PRE_TEST_NAME(clkClockMonsThresholdEvaluate_test1)();

    // Execute unit under test
    status = clkClockMonsThresholdEvaluate_IMPL(&clkDomainList, &voltRailList,
                 (LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_CLK_MON *)(&(script.pStepLwrr->data.super)));
    UT_ASSERT_EQUAL_UINT(status, FLCN_OK);

    // Verify both the threshold values are evaluated correctly
    UT_ASSERT_EQUAL_UINT(script.pStepLwrr->data.clkMon.clkMonList.clkDomains[0].lowThresholdPercent, 10U);
    UT_ASSERT_EQUAL_UINT(script.pStepLwrr->data.clkMon.clkMonList.clkDomains[0].highThresholdPercent, 10U);
}

/*!
 * @brief      Test the method to evaluate the clock monitor threshold values
 */
UT_CASE_DEFINE(PMU_CLKMON, clkClockMonsThresholdEvaluate_test2,
  UT_CASE_SET_DESCRIPTION("Method to evaluate the clock monitor threshold parameters - error case")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Pre-test setup for clkClockMonsThresholdEvaluate_test2
 *
 * @details    Initialize the vfe mock config
 */
PRE_TEST_METHOD(clkClockMonsThresholdEvaluate_test2)
{
    vfeEquEvaluate_MOCK_CONFIG.result = 0U;
    vfeEquEvaluate_MOCK_CONFIG.status = FLCN_ERROR;
}

/*!
 * @brief      Test that the Unit Clock-Monitors gracefully handles the 
 *             error case while evaluating the monitor threshold parameters
 */
UT_CASE_RUN(PMU_CLKMON, clkClockMonsThresholdEvaluate_test2)
{
    FLCN_STATUS status;

    PRE_TEST_NAME(clkClockMonsThresholdEvaluate_test2)();

    // Execute unit under test
    status = clkClockMonsThresholdEvaluate_IMPL(&clkDomainList, &voltRailList,
                 (LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_CLK_MON *)(&(script.pStepLwrr->data.super)));
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERROR);
}
