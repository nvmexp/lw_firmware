/*
 * Copyright (c) 2019-2020, LWPU CORPORATION. All rights reserved.
 *
 * LWPU CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from LWPU CORPORATION is strictly prohibited.
 */

/*!
 * @file    pmu_clkadc-test.c
 * @brief   Unit tests for logic in Clocks-HW ADC devices.
 */

/* ------------------------ System Includes --------------------------------- */
#include "test-macros.h"
#include "regmock.h"

/* ------------------------ Application Includes ---------------------------- */
#include "pmu_objclk.h"
#include "clk/pmu_clkadc.h"
#include "clk/pmu_clk-test.h"
#include "dev_trim.h"

/* ------------------------ Defines and Macros ------------------------------ */
#define CLK_ADC_REGISTER_UNDEFINED   (0U)

/* ------------------------ Globals ---------------------------------------- */
CLK_ADC_DEVICE  adcDev1 = { 0U };
CLK_ADC_DEVICE  adcDev2 = { 0U };
BOARDOBJ       *adcObjects[2U];
CLK_ADC         clkAdcs = { 0U };

/*!
 * @brief Mapping between the ADC ID and the various ADC registers
 */
const CLK_ADC_ADDRESS_MAP ClkAdcRegMap_HAL[] =
{
    {
        LW2080_CTRL_CLK_ADC_ID_SYS,
        {
            LW_PTRIM_SYS_NAFLL_SYSNAFLL_LWVDD_ADC_CTRL,
            LW_PTRIM_SYS_NAFLL_SYSNAFLL_LWVDD_ADC_OVERRIDE,
            LW_PTRIM_SYS_NAFLL_SYSNAFLL_LWVDD_ADC_ACC,
            LW_PTRIM_SYS_NAFLL_SYSNAFLL_LWVDD_ADC_NUM_SAMPLES,
            LW_PTRIM_SYS_NAFLL_SYSNAFLL_LWVDD_ADC_CAL,
            LW_PTRIM_SYS_NAFLL_SYSNAFLL_LWVDD_ADC_MONITOR,
            LW_PTRIM_SYS_NAFLL_SYSNAFLL_LWVDD_ADC_CTRL2,
            CLK_ADC_REGISTER_UNDEFINED,
        }
    },
    {
        LW2080_CTRL_CLK_ADC_ID_GPC0,
        {
            LW_PTRIM_GPC_GPCNAFLL_ADC_CTRL(0),
            LW_PTRIM_GPC_GPCNAFLL_ADC_OVERRIDE(0),
            LW_PTRIM_GPC_GPCNAFLL_ADC_ACC(0),
            LW_PTRIM_GPC_GPCNAFLL_ADC_NUM_SAMPLES(0),
            LW_PTRIM_GPC_GPCNAFLL_ADC_CAL(0),
            LW_PTRIM_GPC_GPCNAFLL_ADC_MONITOR(0),
            LW_PTRIM_GPC_GPCNAFLL_ADC_CTRL2(0),
            CLK_ADC_REGISTER_UNDEFINED,
        }
    },
    {
        LW2080_CTRL_CLK_ADC_ID_GPC1,
        {
            LW_PTRIM_GPC_GPCNAFLL_ADC_CTRL(1),
            LW_PTRIM_GPC_GPCNAFLL_ADC_OVERRIDE(1),
            LW_PTRIM_GPC_GPCNAFLL_ADC_ACC(1),
            LW_PTRIM_GPC_GPCNAFLL_ADC_NUM_SAMPLES(1),
            LW_PTRIM_GPC_GPCNAFLL_ADC_CAL(1),
            LW_PTRIM_GPC_GPCNAFLL_ADC_MONITOR(1),
            LW_PTRIM_GPC_GPCNAFLL_ADC_CTRL2(1),
            CLK_ADC_REGISTER_UNDEFINED,
        }
    },
    {
        LW2080_CTRL_CLK_ADC_ID_GPC2,
        {
            LW_PTRIM_GPC_GPCNAFLL_ADC_CTRL(2),
            LW_PTRIM_GPC_GPCNAFLL_ADC_OVERRIDE(2),
            LW_PTRIM_GPC_GPCNAFLL_ADC_ACC(2),
            LW_PTRIM_GPC_GPCNAFLL_ADC_NUM_SAMPLES(2),
            LW_PTRIM_GPC_GPCNAFLL_ADC_CAL(2),
            LW_PTRIM_GPC_GPCNAFLL_ADC_MONITOR(2),
            LW_PTRIM_GPC_GPCNAFLL_ADC_CTRL2(2),
            CLK_ADC_REGISTER_UNDEFINED,
        }
    },
    {
        LW2080_CTRL_CLK_ADC_ID_GPC3,
        {
            LW_PTRIM_GPC_GPCNAFLL_ADC_CTRL(3),
            LW_PTRIM_GPC_GPCNAFLL_ADC_OVERRIDE(3),
            LW_PTRIM_GPC_GPCNAFLL_ADC_ACC(3),
            LW_PTRIM_GPC_GPCNAFLL_ADC_NUM_SAMPLES(3),
            LW_PTRIM_GPC_GPCNAFLL_ADC_CAL(3),
            LW_PTRIM_GPC_GPCNAFLL_ADC_MONITOR(3),
            LW_PTRIM_GPC_GPCNAFLL_ADC_CTRL2(3),
            CLK_ADC_REGISTER_UNDEFINED,
        }
    },
    {
        LW2080_CTRL_CLK_ADC_ID_GPC4,
        {
            LW_PTRIM_GPC_GPCNAFLL_ADC_CTRL(4),
            LW_PTRIM_GPC_GPCNAFLL_ADC_OVERRIDE(4),
            LW_PTRIM_GPC_GPCNAFLL_ADC_ACC(4),
            LW_PTRIM_GPC_GPCNAFLL_ADC_NUM_SAMPLES(4),
            LW_PTRIM_GPC_GPCNAFLL_ADC_CAL(4),
            LW_PTRIM_GPC_GPCNAFLL_ADC_MONITOR(4),
            LW_PTRIM_GPC_GPCNAFLL_ADC_CTRL2(4),
            CLK_ADC_REGISTER_UNDEFINED,
        }
    },
    {
        LW2080_CTRL_CLK_ADC_ID_GPC5,
        {
            LW_PTRIM_GPC_GPCNAFLL_ADC_CTRL(5),
            LW_PTRIM_GPC_GPCNAFLL_ADC_OVERRIDE(5),
            LW_PTRIM_GPC_GPCNAFLL_ADC_ACC(5),
            LW_PTRIM_GPC_GPCNAFLL_ADC_NUM_SAMPLES(5),
            LW_PTRIM_GPC_GPCNAFLL_ADC_CAL(5),
            LW_PTRIM_GPC_GPCNAFLL_ADC_MONITOR(5),
            LW_PTRIM_GPC_GPCNAFLL_ADC_CTRL2(5),
            CLK_ADC_REGISTER_UNDEFINED,
        }
    },
    {
        LW2080_CTRL_CLK_ADC_ID_GPCS,
        {
            LW_PTRIM_GPC_BCAST_GPCNAFLL_ADC_CTRL,
            LW_PTRIM_GPC_BCAST_GPCNAFLL_ADC_OVERRIDE,
            LW_PTRIM_GPC_BCAST_GPCNAFLL_ADC_ACC,
            LW_PTRIM_GPC_BCAST_GPCNAFLL_ADC_NUM_SAMPLES,
            LW_PTRIM_GPC_BCAST_GPCNAFLL_ADC_CAL,
            LW_PTRIM_GPC_BCAST_GPCNAFLL_ADC_MONITOR,
            LW_PTRIM_GPC_BCAST_GPCNAFLL_ADC_CTRL2,
            CLK_ADC_REGISTER_UNDEFINED,
        }
    },
    // MUST be the last to find the end of array
    {
        LW2080_CTRL_CLK_ADC_ID_MAX,
        {
            CLK_ADC_REGISTER_UNDEFINED,
            CLK_ADC_REGISTER_UNDEFINED,
            CLK_ADC_REGISTER_UNDEFINED,
            CLK_ADC_REGISTER_UNDEFINED,
            CLK_ADC_REGISTER_UNDEFINED,
            CLK_ADC_REGISTER_UNDEFINED,
            CLK_ADC_REGISTER_UNDEFINED,
            CLK_ADC_REGISTER_UNDEFINED,
        }
    },
};

/* ------------------------ Type Definitions -------------------------------- */
/*!
 * @brief      Static helper to initialize all the ADC devices before
 *             individual tests can use them.
 */
static void adcSetup(void);

/*!
 * @brief      Static helper to clean-up the ADC devices after
 *             the individual tests are done.
 */
static void adcTeardown(void);

/* ------------------------ Local Data -------------------------------------- */
/* ------------------------ Test Suite Declaration--------------------------- */
/*!
 * @brief      Definition of the Unit Clocks-HW ADCs test suite.
 */
UT_SUITE_DEFINE(PMU_CLKADC,
                UT_SUITE_SET_COMPONENT("Unit Clock ADCs")
                UT_SUITE_SET_DESCRIPTION("Test suite for the Clocks-HW Adc unit")
                UT_SUITE_SET_OWNER("kwadhwa")
                UT_SUITE_SETUP_HOOK((void *)&adcSetup)
                UT_SUITE_TEARDOWN_HOOK((void *)&adcTeardown))

/*!
 * @brief      Pre-test setup for the ADC test suite.
 *
 * @details    Before any of the individual test case can run, we need to have
 *             the global 'Clk' variable initialized with the required Adc
 *             devices added to the boardobjgrp 'clkAdc'. This method takes care 
 *             of setting up the 'Clk.clkAdc' for all the tests to use.
 */
static void adcSetup(void)
{
    adcDev1.super.type         =  LW2080_CTRL_CLK_ADC_DEVICE_TYPE_V20;
    adcDev1.super.grpIdx       =  0U;
    adcDev1.id                 =  LW2080_CTRL_CLK_ADC_ID_GPC0;
    adcDev1.bPoweredOn         =  LW_TRUE;
    adcDev1.nafllsSharedMask   =  BIT32(LW2080_CTRL_CLK_NAFLL_ID_GPC0);
    adcDev1.bHwCalEnabled      =  LW_TRUE;
    adcDev1.disableClientsMask =  0U;

    adcDev2.super.type         =  LW2080_CTRL_CLK_ADC_DEVICE_TYPE_V20;
    adcDev2.super.grpIdx       =  1U;
    adcDev2.id                 =  LW2080_CTRL_CLK_ADC_ID_GPC1;
    adcDev2.bPoweredOn         =  LW_TRUE;
    adcDev2.nafllsSharedMask   =  BIT32(LW2080_CTRL_CLK_NAFLL_ID_GPC1);
    adcDev2.bHwCalEnabled      =  LW_TRUE;
    adcDev2.disableClientsMask =  0U;

    // Setup OBJCLK with valid CLK_NAFLL_DEVICES
    Clk.clkAdc.super.super.type      = LW2080_CTRL_BOARDOBJGRP_TYPE_E32;
    Clk.clkAdc.super.super.objSlots  = 2U;
    Clk.clkAdc.super.super.ppObjects = adcObjects;
    boardObjGrpMaskInit_E32(&(Clk.clkAdc.super.objMask));
    boardObjGrpMaskBitSet(&(Clk.clkAdc.super.objMask), BOARDOBJ_GET_GRP_IDX(&adcDev1.super));
    boardObjGrpMaskBitSet(&(Clk.clkAdc.super.objMask), BOARDOBJ_GET_GRP_IDX(&adcDev2.super));

    clkAdcs.adcSupportedMask = BIT32(LW2080_CTRL_CLK_ADC_ID_GPC0)|
                                         BIT32(LW2080_CTRL_CLK_ADC_ID_GPC1);
    clkAdcs.bAdcIsDisableAllowed = LW_TRUE;
    (Clk.clkAdc.pAdcs) = &clkAdcs;

    // Insert adc device into group
    adcObjects[0] = &adcDev1.super;
    adcObjects[1] = &adcDev2.super;

    extern FLCN_STATUS clkAdcRegMapInit_GP10X(CLK_ADC_DEVICE *pAdcDev);
    (void)clkAdcRegMapInit_GP10X(&adcDev1);
    (void)clkAdcRegMapInit_GP10X(&adcDev2);
}

/*!
 * @brief      Post-test clean-up for the ADC test suite.
 *
 * @details    Once the Adc test suite run is complete, we can go ahead and
 *             clean-up the global 'Clk' variable and reset it to all zeroes.
 */
static void adcTeardown(void)
{
    (void)memset(&Clk, 0U, sizeof(Clk));
}

/*!
 * @brief      Test the method to program the Adc calibration parameters
 */
UT_CASE_DEFINE(PMU_CLKADC, clkAdcCalProgram_test,
  UT_CASE_SET_DESCRIPTION("Method to test the ADC calibration")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Pre-test setup for clkAdcCalProgram_test
 *
 * @details    Mock the required registers. Initialize the gain and offset 
 *             fields in the mocked register.
 */
PRE_TEST_METHOD(clkAdcCalProgram_test)
{
    LwU32           adcCtrlReg;
    LwU32           adcCtrl2Reg;

    adcCtrlReg  = CLK_ADC_REG_GET(&adcDev1, CTRL);
    adcCtrl2Reg = CLK_ADC_REG_GET(&adcDev1, CTRL2);

    UTF_IO_MOCK(adcCtrlReg,  adcCtrlReg,  NULL, NULL);
    UTF_IO_MOCK(adcCtrl2Reg, adcCtrl2Reg, NULL, NULL);

    // Initialize the 'gain' to 10 and 'offset' to 18
    REG_WR32(FECS, adcCtrl2Reg, 0x000A0012U);

    // Initialize to Powered-On and Enabled
    REG_WR32(FECS, adcCtrlReg,  0x00000002U);
}

/*!
 * @brief      Test that the Unit Clock-ADCs successfully programs the Adc
 *             calibration parameters
 */
UT_CASE_RUN(PMU_CLKADC, clkAdcCalProgram_test)
{
    CLK_ADC_DEVICE     *pAdcDev1    = CLK_ADC_DEVICE_GET(0U);
    CLK_ADC_DEVICE_V20 *pAdcDevV20  = (CLK_ADC_DEVICE_V20 *)(pAdcDev1);
    FLCN_STATUS         status      = FLCN_OK;
    LwU32               adcCtrl2Reg = CLK_ADC_REG_GET(pAdcDev1, CTRL2);;

    PRE_TEST_NAME(clkAdcCalProgram_test)();

    // change 'gain' to 12 and 'offset' to 20
    pAdcDevV20->data.calType              = LW2080_CTRL_CLK_ADC_CAL_TYPE_V20;
    pAdcDevV20->data.adcCal.calV20.gain   = 12U;
    pAdcDevV20->data.adcCal.calV20.offset = 20U;

    // Execute unit under test
    status = clkAdcCalProgram(pAdcDev1, LW_TRUE);
    UT_ASSERT_EQUAL_UINT(status, FLCN_OK);
    UT_ASSERT_EQUAL_UINT(REG_RD32(FECS, adcCtrl2Reg), 0x000C0014U);
}

/*!
 * @brief      Test the method to enable and power-on the Adc device
 */
UT_CASE_DEFINE(PMU_CLKADC, clkAdcPowerOnAndEnable_test1,
  UT_CASE_SET_DESCRIPTION("Method to test the ADC Power-On and Enable")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Pre-test setup for clkAdcPowerOnAndEnable_test1
 *
 * @details    Mock the required registers. Initialize the bPoweredOn and the
 *             disableClientsMask parameters for the ADC device
 */
PRE_TEST_METHOD(clkAdcPowerOnAndEnable_test1)
{
    LwU32 adcCtrlReg = CLK_ADC_REG_GET(&adcDev1, CTRL);

    UTF_IO_MOCK(adcCtrlReg,  adcCtrlReg,  NULL, NULL);
    // Initialize to Powered-Off and Disabled
    REG_WR32(FECS, adcCtrlReg, 0x00000001U);

    adcDev1.bPoweredOn         = LW_FALSE;
    adcDev1.disableClientsMask = BIT(LW2080_CTRL_CLK_ADC_CLIENT_ID_PMU);
}

/*!
 * @brief      Test that the Unit Clock-ADCs successfully powers-on and enables
 *             the Adc device
 */
UT_CASE_RUN(PMU_CLKADC, clkAdcPowerOnAndEnable_test1)
{
    CLK_ADC_DEVICE *pAdcDev1    = CLK_ADC_DEVICE_GET(0U);
    FLCN_STATUS     status      = FLCN_OK;
    LwU32           adcCtrlReg  = CLK_ADC_REG_GET(pAdcDev1, CTRL);

    PRE_TEST_NAME(clkAdcPowerOnAndEnable_test1)();

    // Execute unit under test
    status = clkAdcPowerOnAndEnable(pAdcDev1, LW2080_CTRL_CLK_ADC_CLIENT_ID_PMU,
                                    LW2080_CTRL_CLK_NAFLL_ID_GPC0, LW_TRUE);
    UT_ASSERT_EQUAL_UINT(status, FLCN_OK);
    // Check if the register reflects Powered-On and Enabled
    UT_ASSERT_EQUAL_UINT(REG_RD32(FECS, adcCtrlReg), 0x00000002U);
    // Ensure the ADC is powered-on
    UT_ASSERT_EQUAL_UINT(pAdcDev1->bPoweredOn, LW_TRUE);
    // Ensure the ADC disableClientsMask is updated
    UT_ASSERT_EQUAL_UINT(pAdcDev1->disableClientsMask, 0);
}

/*!
 * @brief      Test the method to disable and power-off the Adc device
 */
UT_CASE_DEFINE(PMU_CLKADC, clkAdcPowerOnAndEnable_test2,
  UT_CASE_SET_DESCRIPTION("Method to test the ADC Power-Off and Disable")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Pre-test setup for clkAdcPowerOnAndEnable_test2
 *
 * @details    Mock the required registers and initialize the values to reflect
 *             the Adc device as disabled and powered-Off
 */
PRE_TEST_METHOD(clkAdcPowerOnAndEnable_test2)
{
    LwU32 adcCtrlReg = CLK_ADC_REG_GET(&adcDev1, CTRL);

    UTF_IO_MOCK(adcCtrlReg,  adcCtrlReg,  NULL, NULL);
    // Initialize to Powered-On and Enabled
    REG_WR32(FECS, adcCtrlReg, 0x00000002U);
}

/*!
 * @brief      Post-test clean-up for clkAdcPowerOnAndEnable_test2
 *
 * @details    Reset the Adc device state - disableClientsMask and bPoweredOn
 */
POST_TEST_METHOD(clkAdcPowerOnAndEnable_test2)
{
    adcDev1.bPoweredOn         = LW_TRUE;
    adcDev1.disableClientsMask = 0;
}

/*!
 * @brief      Test that the Unit Clock-ADCs successfully powers-off and disables
 *             the Adc device
 */
UT_CASE_RUN(PMU_CLKADC, clkAdcPowerOnAndEnable_test2)
{
    CLK_ADC_DEVICE *pAdcDev1    = CLK_ADC_DEVICE_GET(0U);
    FLCN_STATUS     status      = FLCN_OK;
    LwU32           adcCtrlReg  = CLK_ADC_REG_GET(pAdcDev1, CTRL);

    PRE_TEST_NAME(clkAdcPowerOnAndEnable_test2)();

    // Execute unit under test
    status = clkAdcPowerOnAndEnable(pAdcDev1, LW2080_CTRL_CLK_ADC_CLIENT_ID_PMU,
                                    LW2080_CTRL_CLK_NAFLL_ID_GPC0, LW_FALSE);
    UT_ASSERT_EQUAL_UINT(status, FLCN_OK);
    // Check if the register reflects Powered-Off and Disabled
    UT_ASSERT_EQUAL_UINT(REG_RD32(FECS, adcCtrlReg), 0x00000001U);
    // Ensure the ADC is powered-on
    UT_ASSERT_EQUAL_UINT(pAdcDev1->bPoweredOn, LW_FALSE);

    POST_TEST_NAME(clkAdcPowerOnAndEnable_test2)();
}

/*!
 * @brief      Test the method to load ADCs
 */
UT_CASE_DEFINE(PMU_CLKADC, clkAdcsLoad_test,
  UT_CASE_SET_DESCRIPTION("Method to test the ADCs Load")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Pre-test setup for clkAdcsLoad_test
 *
 * @details    Mock the required registers and initialize the values to reflect
 *             the Adc device as disabled and powered-Off
 */
PRE_TEST_METHOD(clkAdcsLoad_test)
{
    LwU32               adc1CtrlReg  = CLK_ADC_REG_GET(&adcDev1, CTRL);
    LwU32               adc1Ctrl2Reg = CLK_ADC_REG_GET(&adcDev1, CTRL2);
    LwU32               adc2CtrlReg  = CLK_ADC_REG_GET(&adcDev2, CTRL);
    LwU32               adc2Ctrl2Reg = CLK_ADC_REG_GET(&adcDev2, CTRL2);
    CLK_ADC_DEVICE_V20 *pAdcDev1V20  = (CLK_ADC_DEVICE_V20 *)(&adcDev1);
    CLK_ADC_DEVICE_V20 *pAdcDev2V20  = (CLK_ADC_DEVICE_V20 *)(&adcDev2);

    UTF_IO_MOCK(adc1CtrlReg,  adc1CtrlReg,  NULL, NULL);
    UTF_IO_MOCK(adc1Ctrl2Reg, adc1Ctrl2Reg, NULL, NULL);
    UTF_IO_MOCK(adc2CtrlReg,  adc2CtrlReg,  NULL, NULL);
    UTF_IO_MOCK(adc2Ctrl2Reg, adc2Ctrl2Reg, NULL, NULL);

    // Initialize the 'gain' to 10 and 'offset' to 18
    REG_WR32(FECS, adc1Ctrl2Reg, 0x000A0012U);
    // Initialize to Powered-On and Enabled
    REG_WR32(FECS, adc1CtrlReg,  0x00000002U);

    // Initialize the 'gain' to 12 and 'offset' to 20
    REG_WR32(FECS, adc2Ctrl2Reg, 0x000C0014U);
    // Initialize to Powered-On and Enabled
    REG_WR32(FECS, adc2CtrlReg,  0x00000002U);

    // change 'gain' to 12 and 'offset' to 20
    pAdcDev1V20->data.calType              = LW2080_CTRL_CLK_ADC_CAL_TYPE_V20;
    pAdcDev1V20->data.adcCal.calV20.gain   = 12U;
    pAdcDev1V20->data.adcCal.calV20.offset = 20U;
    // change 'gain' to 10 and 'offset' to 18
    pAdcDev2V20->data.calType              = LW2080_CTRL_CLK_ADC_CAL_TYPE_V20;
    pAdcDev2V20->data.adcCal.calV20.gain   = 10U;
    pAdcDev2V20->data.adcCal.calV20.offset = 18U;
}

/*!
 * @brief      Test that the Unit Clock-ADCs successfully powers-off and disables
 *             the Adc device
 */
UT_CASE_RUN(PMU_CLKADC, clkAdcsLoad_test)
{
    FLCN_STATUS status       = FLCN_OK;
    LwU32       adc1Ctrl2Reg = CLK_ADC_REG_GET(&adcDev1, CTRL2);;
    LwU32       adc2Ctrl2Reg = CLK_ADC_REG_GET(&adcDev2, CTRL2);;
    LwU32       actionMask   = 0x10;    // _ADC_HW_CAL_PROGRAM_YES

    PRE_TEST_NAME(clkAdcsLoad_test)();

    // Execute unit under test
    status = clkAdcsLoad(actionMask);
    UT_ASSERT_EQUAL_UINT(status, FLCN_OK);

    // Check if the register reflects the correct offset and gain values
    UT_ASSERT_EQUAL_UINT(REG_RD32(FECS, adc1Ctrl2Reg), 0x000C0014U);
    UT_ASSERT_EQUAL_UINT(REG_RD32(FECS, adc2Ctrl2Reg), 0x000A0012U);
}
