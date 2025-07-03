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
 * @file    pmu_clknafll-test.c
 * @brief   Unit tests for logic in Clocks-HW NAFLL devices.
 */

/* ------------------------ System Includes --------------------------------- */
#include "test-macros.h"

/* ------------------------ Application Includes ---------------------------- */
#include "pmu_objclk.h"
#include "clk/pmu_clknafll.h"
#include "clk/pmu_clk-test.h"
#include "dev_trim.h"

/* ------------------------ Globals ---------------------------------------- */
CLK_NAFLL_DEVICE  nafllDev1    = { 0U };
CLK_NAFLL_DEVICE  nafllDev2    = { 0U };
CLK_ADC_DEVICE    logicAdcDev1 = { 0U };
CLK_ADC_DEVICE    logicAdcDev2 = { 0U };

BOARDOBJ         *nafllObjects[2U];

LW2080_CTRL_CLK_FREQ_SOURCE_STATUS_NAFLL nafllStatus1;
LW2080_CTRL_CLK_FREQ_SOURCE_STATUS_NAFLL nafllStatus2;
LW2080_CTRL_CLK_CLK_DOMAIN_LIST          clkDomainList;

const CLK_NAFLL_ADDRESS_MAP ClkNafllRegMap_HAL[] =
{
    {
        LW2080_CTRL_CLK_NAFLL_ID_SYS,
        {
            LW_PTRIM_SYS_NAFLL_SYSLUT_WRITE_ADDR,
            LW_PTRIM_SYS_NAFLL_SYSLUT_WRITE_DATA,
            LW_PTRIM_SYS_NAFLL_SYSLUT_CFG,
            LW_PTRIM_SYS_NAFLL_SYSLUT_SW_FREQ_REQ,
            LW_PTRIM_SYS_NAFLL_SYSNAFLL_COEFF,
        }
    },
    {
        LW2080_CTRL_CLK_NAFLL_ID_XBAR,
        {
            LW_PTRIM_SYS_NAFLL_XBARLUT_WRITE_ADDR,
            LW_PTRIM_SYS_NAFLL_XBARLUT_WRITE_DATA,
            LW_PTRIM_SYS_NAFLL_XBARLUT_CFG,
            LW_PTRIM_SYS_NAFLL_XBARLUT_SW_FREQ_REQ,
            LW_PTRIM_SYS_NAFLL_XBARNAFLL_COEFF,
        }
    },
    {
        LW2080_CTRL_CLK_NAFLL_ID_GPC0,
        {
            LW_PTRIM_GPC_GPCLUT_WRITE_ADDR(0),
            LW_PTRIM_GPC_GPCLUT_WRITE_DATA(0),
            LW_PTRIM_GPC_GPCLUT_CFG(0),
            LW_PTRIM_GPC_GPCLUT_SW_FREQ_REQ(0),
            LW_PTRIM_GPC_GPCNAFLL_COEFF(0),
        }
    },
    {
        LW2080_CTRL_CLK_NAFLL_ID_GPC1,
        {
            LW_PTRIM_GPC_GPCLUT_WRITE_ADDR(1),
            LW_PTRIM_GPC_GPCLUT_WRITE_DATA(1),
            LW_PTRIM_GPC_GPCLUT_CFG(1),
            LW_PTRIM_GPC_GPCLUT_SW_FREQ_REQ(1),
            LW_PTRIM_GPC_GPCNAFLL_COEFF(1),
        }
    },
    {
        LW2080_CTRL_CLK_NAFLL_ID_GPC2,
        {
            LW_PTRIM_GPC_GPCLUT_WRITE_ADDR(2),
            LW_PTRIM_GPC_GPCLUT_WRITE_DATA(2),
            LW_PTRIM_GPC_GPCLUT_CFG(2),
            LW_PTRIM_GPC_GPCLUT_SW_FREQ_REQ(2),
            LW_PTRIM_GPC_GPCNAFLL_COEFF(2),
        }
    },
    {
        LW2080_CTRL_CLK_NAFLL_ID_GPC3,
        {
            LW_PTRIM_GPC_GPCLUT_WRITE_ADDR(3),
            LW_PTRIM_GPC_GPCLUT_WRITE_DATA(3),
            LW_PTRIM_GPC_GPCLUT_CFG(3),
            LW_PTRIM_GPC_GPCLUT_SW_FREQ_REQ(3),
            LW_PTRIM_GPC_GPCNAFLL_COEFF(3),
        }
    },
    {
        LW2080_CTRL_CLK_NAFLL_ID_GPC4,
        {
            LW_PTRIM_GPC_GPCLUT_WRITE_ADDR(4),
            LW_PTRIM_GPC_GPCLUT_WRITE_DATA(4),
            LW_PTRIM_GPC_GPCLUT_CFG(4),
            LW_PTRIM_GPC_GPCLUT_SW_FREQ_REQ(4),
            LW_PTRIM_GPC_GPCNAFLL_COEFF(4),
        }
    },
    {
        LW2080_CTRL_CLK_NAFLL_ID_GPC5,
        {
            LW_PTRIM_GPC_GPCLUT_WRITE_ADDR(5),
            LW_PTRIM_GPC_GPCLUT_WRITE_DATA(5),
            LW_PTRIM_GPC_GPCLUT_CFG(5),
            LW_PTRIM_GPC_GPCLUT_SW_FREQ_REQ(5),
            LW_PTRIM_GPC_GPCNAFLL_COEFF(5),
        }
    },
    {
        LW2080_CTRL_CLK_NAFLL_ID_GPCS,
        {
            LW_PTRIM_GPC_BCAST_GPCLUT_WRITE_ADDR,
            LW_PTRIM_GPC_BCAST_GPCLUT_WRITE_DATA,
            LW_PTRIM_GPC_BCAST_GPCLUT_CFG,
            LW_PTRIM_GPC_BCAST_GPCLUT_SW_FREQ_REQ,
            LW_PTRIM_GPC_BCAST_GPCNAFLL_COEFF,
        }
    },
    {
        LW2080_CTRL_CLK_NAFLL_ID_LWD,
        {
            LW_PTRIM_SYS_NAFLL_LWDLUT_WRITE_ADDR,
            LW_PTRIM_SYS_NAFLL_LWDLUT_WRITE_DATA,
            LW_PTRIM_SYS_NAFLL_LWDLUT_CFG,
            LW_PTRIM_SYS_NAFLL_LWDLUT_SW_FREQ_REQ,
            LW_PTRIM_SYS_NAFLL_LWDNAFLL_COEFF,
        }
    },
    {
        LW2080_CTRL_CLK_NAFLL_ID_HOST,
        {
            LW_PTRIM_SYS_NAFLL_HOSTLUT_WRITE_ADDR,
            LW_PTRIM_SYS_NAFLL_HOSTLUT_WRITE_DATA,
            LW_PTRIM_SYS_NAFLL_HOSTLUT_CFG,
            LW_PTRIM_SYS_NAFLL_HOSTLUT_SW_FREQ_REQ,
            LW_PTRIM_SYS_NAFLL_HOSTNAFLL_COEFF,
        }
    },
    // MUST be the last to find the end of array.
    {
        LW2080_CTRL_CLK_NAFLL_ID_MAX,
        {
            LW2080_CTRL_CLK_NAFLL_ID_UNDEFINED,
            LW2080_CTRL_CLK_NAFLL_ID_UNDEFINED,
            LW2080_CTRL_CLK_NAFLL_ID_UNDEFINED,
            LW2080_CTRL_CLK_NAFLL_ID_UNDEFINED,
            LW2080_CTRL_CLK_NAFLL_ID_UNDEFINED,
        }
    },
};

/* ------------------------ Type Definitions -------------------------------- */
/*!
 * @brief      Static helper to initialize all the NAFLL devices before
 *             individual tests can use them.
 */
static void nafllSetup(void);

/*!
 * @brief      Static helper to clean-up the NAFLL devices after
 *             the individual tests are done.
 */
static void nafllTeardown(void);

/* ------------------------ Local Data -------------------------------------- */
/* ------------------------ Defines and Macros ------------------------------ */
/* ------------------------ Test Suite Declaration--------------------------- */
/*!
 * @brief      Definition of the Unit Clocks-HW NAFLLs test suite.
 */
UT_SUITE_DEFINE(PMU_CLKNAFLL,
                UT_SUITE_SET_COMPONENT("Unit Clock NAFLLs")
                UT_SUITE_SET_DESCRIPTION("Test suite for the Clocks-HW Nafll unit")
                UT_SUITE_SET_OWNER("kwadhwa")
                UT_SUITE_SETUP_HOOK((void *)&nafllSetup)
                UT_SUITE_TEARDOWN_HOOK((void *)&nafllTeardown))

/*!
 * @brief      Pre-test setup for the NAFLL test suite.
 *
 * @details    Before any of the individual test case can run, we need to have
 *             the global 'Clk' variable initialized with the required Nafll
 *             devices added to the boardobjgrp 'clkNafll'. This method takes
 *             care of setting up the 'Clk.clkNafll' for all the tests to use.
 */
static void nafllSetup(void)
{
    nafllDev1.super.type                 = LW2080_CTRL_CLK_NAFLL_DEVICE_TYPE_V20;
    nafllDev1.super.grpIdx               = 0U;
    nafllDev1.id                         = LW2080_CTRL_CLK_NAFLL_ID_GPC0;
    nafllDev1.clkDomain                  = LW2080_CTRL_CLK_DOMAIN_GPCCLK;
    nafllDev1.mdiv                       = 27U;
    nafllDev1.inputRefClkFreqMHz         = 405U;
    nafllDev1.inputRefClkDivVal          = 1U;
    nafllDev1.dvco.bEnabled              = LW_TRUE;
    nafllDev1.dvco.bDvco1x               = LW_TRUE;
    nafllDev1.regimeDesc.fixedFreqRegimeLimitMHz = 405U;
    nafllDev1.regimeDesc.nafllId         = LW2080_CTRL_CLK_NAFLL_ID_GPC0;
    nafllDev1.bSkipPldivBelowDvcoMin     = LW_TRUE;
    nafllDev1.pStatus                    = &nafllStatus1;
    nafllDev1.lutDevice.id               = LW2080_CTRL_CLK_NAFLL_ID_GPC0;
    nafllDev1.lutDevice.lwrrentTempIndex = 0;
    nafllDev1.lutDevice.bInitialized     = LW_TRUE;
    logicAdcDev1.id                      = LW2080_CTRL_CLK_ADC_ID_GPC0;
    logicAdcDev1.bPoweredOn              = LW_TRUE;
    logicAdcDev1.disableClientsMask      = 0U;
    logicAdcDev1.nafllsSharedMask        = BIT32(LW2080_CTRL_CLK_NAFLL_ID_GPC0);
    logicAdcDev1.bHwCalEnabled           = LW_TRUE;
    nafllDev1.pLogicAdcDevice            = &logicAdcDev1;

    nafllDev2.super.type                 = LW2080_CTRL_CLK_NAFLL_DEVICE_TYPE_V20;
    nafllDev2.super.grpIdx               = 1U;
    nafllDev2.id                         = LW2080_CTRL_CLK_NAFLL_ID_GPC1;
    nafllDev2.clkDomain                  = LW2080_CTRL_CLK_DOMAIN_GPCCLK;
    nafllDev2.mdiv                       = 27U;
    nafllDev2.inputRefClkFreqMHz         = 405U;
    nafllDev2.inputRefClkDivVal          = 1U;
    nafllDev2.dvco.bEnabled              = LW_TRUE;
    nafllDev2.dvco.bDvco1x               = LW_TRUE;
    nafllDev2.regimeDesc.fixedFreqRegimeLimitMHz = 405U;
    nafllDev2.regimeDesc.nafllId         = LW2080_CTRL_CLK_NAFLL_ID_GPC1;
    nafllDev2.bSkipPldivBelowDvcoMin     = LW_TRUE;
    nafllDev2.pStatus                    = &nafllStatus2;
    nafllDev2.lutDevice.id               = LW2080_CTRL_CLK_NAFLL_ID_GPC1;
    nafllDev2.lutDevice.lwrrentTempIndex = 0;
    nafllDev2.lutDevice.bInitialized     = LW_TRUE;
    logicAdcDev2.id                      = LW2080_CTRL_CLK_ADC_ID_GPC1;
    logicAdcDev2.bPoweredOn              = LW_TRUE;
    logicAdcDev2.disableClientsMask      = 0U;
    logicAdcDev2.nafllsSharedMask        = BIT32(LW2080_CTRL_CLK_NAFLL_ID_GPC1);
    logicAdcDev2.bHwCalEnabled           = LW_TRUE;
    nafllDev2.pLogicAdcDevice            = &logicAdcDev2;

    // Setup the Nafll status structures
    nafllStatus1.pldivEngage             = LW_TRISTATE_INDETERMINATE;
    nafllStatus1.regime.targetRegimeId   = LW2080_CTRL_CLK_NAFLL_REGIME_ID_ILWALID;
    nafllStatus1.regime.targetFreqMHz    = 0U;
    nafllStatus1.regime.lwrrentRegimeId  = LW2080_CTRL_CLK_NAFLL_REGIME_ID_ILWALID;
    nafllStatus1.regime.lwrrentFreqMHz   = 0U;
    nafllStatus2.pldivEngage             = LW_TRISTATE_INDETERMINATE;
    nafllStatus2.regime.targetRegimeId   = LW2080_CTRL_CLK_NAFLL_REGIME_ID_ILWALID;
    nafllStatus2.regime.targetFreqMHz    = 0U;
    nafllStatus2.regime.lwrrentRegimeId  = LW2080_CTRL_CLK_NAFLL_REGIME_ID_ILWALID;
    nafllStatus2.regime.lwrrentFreqMHz   = 0U;

    // Setup OBJCLK with valid CLK_NAFLL_DEVICES
    Clk.clkNafll.super.super.type        = LW2080_CTRL_BOARDOBJGRP_TYPE_E32;
    Clk.clkNafll.super.super.objSlots    = 2U;
    Clk.clkNafll.super.super.ppObjects   = nafllObjects;
    boardObjGrpMaskInit_E32(&(Clk.clkNafll.super.objMask));
    boardObjGrpMaskBitSet(&(Clk.clkNafll.super.objMask), BOARDOBJ_GET_GRP_IDX(&nafllDev1.super));
    boardObjGrpMaskBitSet(&(Clk.clkNafll.super.objMask), BOARDOBJ_GET_GRP_IDX(&nafllDev2.super));
    boardObjGrpMaskInit_E32(&(Clk.clkNafll.lutProgPrimaryMask));
    boardObjGrpMaskBitSet(&(Clk.clkNafll.lutProgPrimaryMask), BOARDOBJ_GET_GRP_IDX(&nafllDev1.super));
    boardObjGrpMaskBitSet(&(Clk.clkNafll.lutProgPrimaryMask), BOARDOBJ_GET_GRP_IDX(&nafllDev2.super));

    Clk.clkNafll.nafllSupportedMask = BIT32(LW2080_CTRL_CLK_NAFLL_ID_GPC0)|
                                      BIT32(LW2080_CTRL_CLK_NAFLL_ID_GPC1);
    Clk.clkNafll.dvcoMinFreqMHz[0]  = 350;


    // Insert nafll device into group
    nafllObjects[0U] = &nafllDev1.super;
    nafllObjects[1U] = &nafllDev2.super;

    extern FLCN_STATUS clkNafllRegMapInit_GP10X(CLK_NAFLL_DEVICE *pNafllDev);
    (void)clkNafllRegMapInit_GP10X(&nafllDev1);
    (void)clkNafllRegMapInit_GP10X(&nafllDev2);
}

/*!
 * @brief      Post-test clean-up for the NAFLL test suite.
 *
 * @details    Once the individual test case is done, we need to clean up the
 *             global 'Clk' variable and reset it to all zeroes.
 */
static void nafllTeardown(void)
{
    (void)memset(&Clk, 0U, sizeof(Clk));
}

/*!
 * @brief      Test the method to compute ndiv from a given frequency value.
 */
UT_CASE_DEFINE(PMU_CLKNAFLL, clkNafllNdivFromFreqCompute_test1,
  UT_CASE_SET_DESCRIPTION("Test case for computing ndiv from a given frequency value")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Case 1 - dvco set to 1x
 */
PRE_TEST_METHOD(clkNafllNdivFromFreqCompute_test1)
{
    nafllDev1.dvco.bDvco1x = LW_TRUE;
}

/*!
 * @brief      Reset the dvco to default
 */
POST_TEST_METHOD(clkNafllNdivFromFreqCompute_test1)
{
    nafllDev1.dvco.bDvco1x = LW_TRUE;
}

/*!
 * @brief      Test that the Unit Clock-NAFLLs successfully computes ndiv from
 *             the given frequency value
 *
 * @details    This particular test targets use case 1 with the dvco set to 1X
 *             and requesting a rounding down.
 */
UT_CASE_RUN(PMU_CLKNAFLL, clkNafllNdivFromFreqCompute_test1)
{
    CLK_NAFLL_DEVICE *pNafllDev = CLK_NAFLL_DEVICE_GET(0U);
    LwU16             nDiv;
    FLCN_STATUS       status    = FLCN_OK;

    PRE_TEST_NAME(clkNafllNdivFromFreqCompute_test1)();

    status = clkNafllNdivFromFreqCompute(pNafllDev, 720U, &nDiv, LW_TRUE);

    UT_ASSERT_EQUAL_UINT(FLCN_OK, status);
    UT_ASSERT_EQUAL_UINT(nDiv, 48U);

    POST_TEST_NAME(clkNafllNdivFromFreqCompute_test1)();
}

/*!
 * @brief      Test the method to compute ndiv from a given frequency value.
 */
UT_CASE_DEFINE(PMU_CLKNAFLL, clkNafllNdivFromFreqCompute_test2,
  UT_CASE_SET_DESCRIPTION("Test case for computing frequency from a given ndiv value")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Case 2 - dvco set to 2x
 */
PRE_TEST_METHOD(clkNafllNdivFromFreqCompute_test2)
{
    nafllDev1.dvco.bDvco1x = LW_FALSE;
}

/*!
 * @brief      Reset the dvco to default
 */
POST_TEST_METHOD(clkNafllNdivFromFreqCompute_test2)
{
    nafllDev1.dvco.bDvco1x = LW_TRUE;
}

/*!
 * @brief      Test that the Unit Clock-NAFLLs successfully computes ndiv from
 *             the given frequency value
 *
 * @details    This particular test targets use case 2 with the dvco set to 2X
 *             and requesting a rounding down.
 */
UT_CASE_RUN(PMU_CLKNAFLL, clkNafllNdivFromFreqCompute_test2)
{
    CLK_NAFLL_DEVICE *pNafllDev = CLK_NAFLL_DEVICE_GET(0U);
    LwU16             nDiv;
    FLCN_STATUS       status    = FLCN_OK;

    PRE_TEST_NAME(clkNafllNdivFromFreqCompute_test2)();

    // Case 2 - dvco 2x + bFloor
    status = clkNafllNdivFromFreqCompute(pNafllDev, 734U, &nDiv, LW_TRUE);

    UT_ASSERT_EQUAL_UINT(FLCN_OK, status);
    UT_ASSERT_EQUAL_UINT(nDiv, 97U);

    POST_TEST_NAME(clkNafllNdivFromFreqCompute_test2)();
}

/*!
 * @brief      Test the method to compute ndiv from a given frequency value.
 */
UT_CASE_DEFINE(PMU_CLKNAFLL, clkNafllNdivFromFreqCompute_test3,
  UT_CASE_SET_DESCRIPTION("Test case for computing frequency from a given ndiv value")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Case 3 - dvco set to 1x
 */
PRE_TEST_METHOD(clkNafllNdivFromFreqCompute_test3)
{
    nafllDev1.dvco.bDvco1x = LW_TRUE;
}

/*!
 * @brief      Reset the dvco to default
 */
POST_TEST_METHOD(clkNafllNdivFromFreqCompute_test3)
{
    nafllDev1.dvco.bDvco1x = LW_TRUE;
}

/*!
 * @brief      Test that the Unit Clock-NAFLLs successfully computes ndiv from
 *             the given frequency value
 *
 * @details    This particular test targets use case 3 with the dvco set to 1X
 *             and *not* requesting a rounding down.
 */
UT_CASE_RUN(PMU_CLKNAFLL, clkNafllNdivFromFreqCompute_test3)
{
    CLK_NAFLL_DEVICE *pNafllDev = CLK_NAFLL_DEVICE_GET(0U);
    LwU16             nDiv;
    FLCN_STATUS       status    = FLCN_OK;

    PRE_TEST_NAME(clkNafllNdivFromFreqCompute_test3)();

    // Case 3 - dvco 1x + !bFloor
    status = clkNafllNdivFromFreqCompute(pNafllDev, 734U, &nDiv, LW_FALSE);

    UT_ASSERT_EQUAL_UINT(FLCN_OK, status);
    UT_ASSERT_EQUAL_UINT(nDiv, 49U);

    POST_TEST_NAME(clkNafllNdivFromFreqCompute_test3)();
}

/*!
 * @brief      Test the method to compute ndiv from a given frequency value.
 */
UT_CASE_DEFINE(PMU_CLKNAFLL, clkNafllNdivFromFreqCompute_test4,
  UT_CASE_SET_DESCRIPTION("Test case for computing frequency from a given ndiv value")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Case 4 - dvco set to 2x
 */
PRE_TEST_METHOD(clkNafllNdivFromFreqCompute_test4)
{
    nafllDev1.dvco.bDvco1x = LW_FALSE;
}

/*!
 * @brief      Reset the dvco to default
 */
POST_TEST_METHOD(clkNafllNdivFromFreqCompute_test4)
{
    nafllDev1.dvco.bDvco1x = LW_TRUE;
}

/*!
 * @brief      Test that the Unit Clock-NAFLLs successfully computes ndiv from
 *             the given frequency value
 *
 * @details    This particular test targets use case 4 with the dvco set to 2X
 *             and *not* requesting a rounding down.
 */
UT_CASE_RUN(PMU_CLKNAFLL, clkNafllNdivFromFreqCompute_test4)
{
    CLK_NAFLL_DEVICE *pNafllDev = CLK_NAFLL_DEVICE_GET(0U);
    LwU16             nDiv;
    FLCN_STATUS       status    = FLCN_OK;

    PRE_TEST_NAME(clkNafllNdivFromFreqCompute_test4)();

    // Case 4 - dvco 2x + !bFloor
    status = clkNafllNdivFromFreqCompute(pNafllDev, 721U, &nDiv, LW_FALSE);

    UT_ASSERT_EQUAL_UINT(FLCN_OK, status);
    UT_ASSERT_EQUAL_UINT(nDiv, 97U);

    POST_TEST_NAME(clkNafllNdivFromFreqCompute_test4)();
}

/*!
 * @brief      Test the method to compute frequency from a given ndiv value.
 */
UT_CASE_DEFINE(PMU_CLKNAFLL, clkNafllFreqFromNdivCompute_test1,
  UT_CASE_SET_DESCRIPTION("Test case for computing ndiv from a given frequency value")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Case 1 - dvco set to 1x
 */
PRE_TEST_METHOD(clkNafllFreqFromNdivCompute_test1)
{
    nafllDev1.dvco.bDvco1x = LW_TRUE;
}

/*!
 * @brief      Reset the dvco to default
 */
POST_TEST_METHOD(clkNafllFreqFromNdivCompute_test1)
{
    nafllDev1.dvco.bDvco1x = LW_TRUE;
}

/*!
 * @brief      Test that the Unit Clock-NAFLLs successfully computes frequency
 *             from the given ndiv value
 *
 * @details    This particular test targets use case 1 with the dvco set to 1X
 */
UT_CASE_RUN(PMU_CLKNAFLL, clkNafllFreqFromNdivCompute_test1)
{
    CLK_NAFLL_DEVICE  *pNafllDev = CLK_NAFLL_DEVICE_GET(0U);

    // Case 1 - dvco 1x
    PRE_TEST_NAME(clkNafllFreqFromNdivCompute_test1)();

    UT_ASSERT_EQUAL_UINT(720U, clkNafllFreqFromNdivCompute(pNafllDev, 48U));

    POST_TEST_NAME(clkNafllFreqFromNdivCompute_test1)();
}

/*!
 * @brief      Test the method to compute frequency from a given ndiv value.
 */
UT_CASE_DEFINE(PMU_CLKNAFLL, clkNafllFreqFromNdivCompute_test2,
  UT_CASE_SET_DESCRIPTION("Test case for computing ndiv from a given frequency value")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Case 2 - dvco set to 2x
 */
PRE_TEST_METHOD(clkNafllFreqFromNdivCompute_test2)
{
    nafllDev1.dvco.bDvco1x = LW_FALSE;
}

/*!
 * @brief      Reset the dvco to default
 */
POST_TEST_METHOD(clkNafllFreqFromNdivCompute_test2)
{
    nafllDev1.dvco.bDvco1x = LW_TRUE;
}

/*!
 * @brief      Test that the Unit Clock-NAFLLs successfully computes frequency
 *             from the given ndiv value
 *
 * @details    This particular test targets use case 2 with the dvco set to 2X
 */
UT_CASE_RUN(PMU_CLKNAFLL, clkNafllFreqFromNdivCompute_test2)
{
    CLK_NAFLL_DEVICE  *pNafllDev = CLK_NAFLL_DEVICE_GET(0U);

    // Case 2 - dvco 2x
    PRE_TEST_NAME(clkNafllFreqFromNdivCompute_test2)();

    UT_ASSERT_EQUAL_UINT(720U, clkNafllFreqFromNdivCompute(pNafllDev, 96U));

    POST_TEST_NAME(clkNafllFreqFromNdivCompute_test2)();
}

/*!
 * @brief      Test the method to quantize a given frequency value.
 */
UT_CASE_DEFINE(PMU_CLKNAFLL, clkNafllFreqMHzQuantize_test1,
  UT_CASE_SET_DESCRIPTION("Test case for quantizing a given raw frequency value to the nearest HW achievable value")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test that the Unit Clock-NAFLLs successfully quantizes a given
 *             raw frequency value to the nearest achievable HW frequency
 *
 * @details    This particular test targets use case 1 with the input raw
 *             frequency is one less than the multiple of step-size for the
 *             Nafll device
 */
UT_CASE_RUN(PMU_CLKNAFLL, clkNafllFreqMHzQuantize_test1)
{
    // 734 is a *not* multiple of 15 - step size for the NAFLL
    LwU16       freqMHz = 734U;
    FLCN_STATUS status  = FLCN_OK;

    // Case 1 - bFloor + frequency 1 less than the multiple of step size
    status  = clkNafllFreqMHzQuantize_IMPL(LW2080_CTRL_CLK_DOMAIN_GPCCLK, &freqMHz, LW_TRUE);
    UT_ASSERT_EQUAL_UINT(FLCN_OK, status);
    UT_ASSERT_EQUAL_UINT(720U, freqMHz);

    // Case 2 - !bFloor + frequency 1 less than the multiple of step size
    freqMHz = 734U;
    status  = clkNafllFreqMHzQuantize_IMPL(LW2080_CTRL_CLK_DOMAIN_GPCCLK, &freqMHz, LW_FALSE);
    UT_ASSERT_EQUAL_UINT(FLCN_OK, status);
    UT_ASSERT_EQUAL_UINT(735U, freqMHz);
}

/*!
 * @brief      Test the method to quantize a given frequency value.
 */
UT_CASE_DEFINE(PMU_CLKNAFLL, clkNafllFreqMHzQuantize_test2,
  UT_CASE_SET_DESCRIPTION("Test case for quantizing a given raw frequency value to the nearest HW achievable value")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test that the Unit Clock-NAFLLs successfully quantizes a given
 *             raw frequency value to the nearest achievable HW frequency
 *
 * @details    This particular test targets use case 2 with the input raw
 *             frequency being a multiple of step-size for the Nafll device
 */
UT_CASE_RUN(PMU_CLKNAFLL, clkNafllFreqMHzQuantize_test2)
{
    // 735 is a multiple of 15 - step size for the NAFLL
    LwU16       freqMHz = 735U;
    FLCN_STATUS status  = FLCN_OK;

    // Case 3 - bFloor + frequency a multiple of step size
    status  = clkNafllFreqMHzQuantize_IMPL(LW2080_CTRL_CLK_DOMAIN_GPCCLK, &freqMHz, LW_TRUE);
    UT_ASSERT_EQUAL_UINT(FLCN_OK, status);
    UT_ASSERT_EQUAL_UINT(735U, freqMHz);

    // Case 4 - !bFloor + frequency a multiple of step size
    status  = clkNafllFreqMHzQuantize_IMPL(LW2080_CTRL_CLK_DOMAIN_GPCCLK, &freqMHz, LW_FALSE);
    UT_ASSERT_EQUAL_UINT(FLCN_OK, status);
    UT_ASSERT_EQUAL_UINT(735U, freqMHz);
}

/*!
 * @brief      Test the method to quantize a given frequency value.
 */
UT_CASE_DEFINE(PMU_CLKNAFLL, clkNafllFreqMHzQuantize_test3,
  UT_CASE_SET_DESCRIPTION("Test case for quantizing a given raw frequency value to the nearest HW achievable value")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test that the Unit Clock-NAFLLs successfully quantizes a given
 *             raw frequency value to the nearest achievable HW frequency
 *
 * @details    This particular test targets use case 3 with the input raw
 *             frequency is one more than the multiple of step-size for the
 *             Nafll device
 */
UT_CASE_RUN(PMU_CLKNAFLL, clkNafllFreqMHzQuantize_test3)
{
    // 734 is a *not* multiple of 15 - step size for the NAFLL
    LwU16       freqMHz = 736U;
    FLCN_STATUS status  = FLCN_OK;

    // Case 5 - bFloor + frequency 1 more than the multiple of step size
    status  = clkNafllFreqMHzQuantize_IMPL(LW2080_CTRL_CLK_DOMAIN_GPCCLK, &freqMHz, LW_TRUE);
    UT_ASSERT_EQUAL_UINT(FLCN_OK, status);
    UT_ASSERT_EQUAL_UINT(735U, freqMHz);

    // Case 6 - !bFloor + frequency 1 more than the multiple of step size
    freqMHz = 736U;
    status  = clkNafllFreqMHzQuantize_IMPL(LW2080_CTRL_CLK_DOMAIN_GPCCLK, &freqMHz, LW_FALSE);
    UT_ASSERT_EQUAL_UINT(FLCN_OK, status);
    UT_ASSERT_EQUAL_UINT(750U, freqMHz);
}

/*!
 * @brief      Test the method to map a given clock domain to Nafll-ids mask.
 */
UT_CASE_DEFINE(PMU_CLKNAFLL, clkNafllDomainToIdMask_test1,
  UT_CASE_SET_DESCRIPTION("Test case to map a given clock domain to its corresponding NAFLL device(s)")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test that the Unit Clock-NAFLLs successfully maps a given clock
 *             domain to the corresponding mask of Nafll device(s)
 */
UT_CASE_RUN(PMU_CLKNAFLL, clkNafllDomainToIdMask_test1)
{
    LwU32       nafllIdMask;
    FLCN_STATUS status;

    // Execute unit under test
    // GPC0 & 1 existing
    status = clkNafllDomainToIdMask(LW2080_CTRL_CLK_DOMAIN_GPCCLK, &nafllIdMask);
    UT_ASSERT_EQUAL_UINT(FLCN_OK, status);
    UT_ASSERT_EQUAL_UINT((BIT32(LW2080_CTRL_CLK_NAFLL_ID_GPC0)|
                          BIT32(LW2080_CTRL_CLK_NAFLL_ID_GPC1)),
                          nafllIdMask);
}

/*!
 * @brief      Test the error case for the method to map a given clock domain to Nafll-ids mask.
 */
UT_CASE_DEFINE(PMU_CLKNAFLL, clkNafllDomainToIdMask_test2,
  UT_CASE_SET_DESCRIPTION("Test case for error condition while trying to map a given clock domain to its corresponding NAFLL device(s)")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test that the Unit Clock-NAFLLs fails gracefully for a clock
 *             domain that does not exist in the Nafll devices table
 */
UT_CASE_RUN(PMU_CLKNAFLL, clkNafllDomainToIdMask_test2)
{
    LwU32       nafllIdMask;
    FLCN_STATUS status;

    // Execute unit under test
    // SYS not existing
    status = clkNafllDomainToIdMask(LW2080_CTRL_CLK_DOMAIN_SYSCLK, &nafllIdMask);
    UT_ASSERT_EQUAL_UINT(FLCN_ERROR, status);
    UT_ASSERT_EQUAL_UINT(LW2080_CTRL_CLK_NAFLL_MASK_UNDEFINED, nafllIdMask);
}

/*!
 * @brief      Test the method to get the pointer to an NAFLL device for a given NAFLL-id.
 */
UT_CASE_DEFINE(PMU_CLKNAFLL, clkNafllDeviceGet_test1,
  UT_CASE_SET_DESCRIPTION("Test case for getting an NAFLL device for the given NAFLL id.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test that the Unit Clock-NAFLLs successfully returns NULL for
 *             a non-existent Nafll-id
 */
UT_CASE_RUN(PMU_CLKNAFLL, clkNafllDeviceGet_test1)
{
    // Case 1 - LTC not existing - should be NULL
    UT_ASSERT_EQUAL_PTR(clkNafllDeviceGet(LW2080_CTRL_CLK_NAFLL_ID_LTC), NULL);
}

/*!
 * @brief      Test the method to get the pointer to an NAFLL device for a given NAFLL-id.
 */
UT_CASE_DEFINE(PMU_CLKNAFLL, clkNafllDeviceGet_test2,
  UT_CASE_SET_DESCRIPTION("Test case for getting an NAFLL device for the given NAFLL id.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test that the Unit Clock-NAFLLs successfully gets the correct
 *             Nafll device for a given Nafll-id
 */
UT_CASE_RUN(PMU_CLKNAFLL, clkNafllDeviceGet_test2)
{
    // Case 2 - GPC0 existing in nafllDev1
    UT_ASSERT_EQUAL_PTR(clkNafllDeviceGet(LW2080_CTRL_CLK_NAFLL_ID_GPC0), &nafllDev1);

    // Case 3 - GPC1 existing in nafllDev2
    UT_ASSERT_EQUAL_PTR(clkNafllDeviceGet(LW2080_CTRL_CLK_NAFLL_ID_GPC1), &nafllDev2);
}

/*!
 * @brief      Test the method to return DVCO min related parameters for a given clock domain
 */
UT_CASE_DEFINE(PMU_CLKNAFLL, clkNafllDvcoMinFreqMHzGet_test,
  UT_CASE_SET_DESCRIPTION("Test case for querying Dvco min frequency for a given clock domain")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test that the Unit Clock-NAFLLs successfully returns the cached
 *             DVCO min frequency and the boolean that skips pldiv below DVCO-Min
 */
UT_CASE_RUN(PMU_CLKNAFLL, clkNafllDvcoMinFreqMHzGet_test)
{
    CLK_NAFLL_DEVICE *pNafllDev = CLK_NAFLL_DEVICE_GET(0U);
    FLCN_STATUS       status    = FLCN_OK;
    LwU16             dvcoMinFreqMHz;
    LwBool            bSkipPldivBelowDvcoMin;

    status = clkNafllDvcoMinFreqMHzGet(LW2080_CTRL_CLK_DOMAIN_GPCCLK,
                                       &dvcoMinFreqMHz, &bSkipPldivBelowDvcoMin);
    UT_ASSERT_EQUAL_UINT(FLCN_OK, status);
    UT_ASSERT_EQUAL_UINT(pNafllDev->bSkipPldivBelowDvcoMin, bSkipPldivBelowDvcoMin);
    UT_ASSERT_EQUAL_UINT(Clk.clkNafll.dvcoMinFreqMHz[0], dvcoMinFreqMHz);
}

/*!
 * @brief      Test the method to compute frequency from a given ndiv value.
 */
UT_CASE_DEFINE(PMU_CLKNAFLL, clkNafllGetFreqMHzByIndex_test,
  UT_CASE_SET_DESCRIPTION("Test case for computing ndiv from a given frequency value")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test that the Unit Clock-NAFLLs successfully computes frequency
 *             from the given ndiv value
 */
UT_CASE_RUN(PMU_CLKNAFLL, clkNafllGetFreqMHzByIndex_test)
{
    LwU16 freqMHz;

    UT_ASSERT_EQUAL_UINT(FLCN_OK, clkNafllGetFreqMHzByIndex_IMPL(LW2080_CTRL_CLK_DOMAIN_GPCCLK, 48U, &freqMHz));
    UT_ASSERT_EQUAL_UINT(720U, freqMHz);
}

/*!
 * @brief      Test the method to configure a given clock domain
 */
UT_CASE_DEFINE(PMU_CLKNAFLL, clkNafllConfig_test,
  UT_CASE_SET_DESCRIPTION("Test case for configuring given Nafll device")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test that the Unit Clock-NAFLLs successfully configures the given
 *             Nafll device with the target parameters
 */
UT_CASE_RUN(PMU_CLKNAFLL, clkNafllConfig_test)
{
    FLCN_STATUS       status;
    CLK_NAFLL_DEVICE *pNafllDev = CLK_NAFLL_DEVICE_GET(0U);
    LW2080_CTRL_CLK_FREQ_SOURCE_STATUS_NAFLL *pStatus = &nafllStatus1;
    RM_PMU_CLK_FREQ_TARGET_SIGNAL target;

    target.super.freqKHz        = 720U * 1000U;
    target.super.regimeId       = LW2080_CTRL_CLK_NAFLL_REGIME_ID_FFR;
    target.super.source         = LW2080_CTRL_CLK_PROG_1X_SOURCE_NAFLL;
    target.super.dvcoMinFreqMHz = 450U;

    status = clkNafllConfig(pNafllDev, pStatus, &target);
    UT_ASSERT_EQUAL_UINT(FLCN_OK, status);
    UT_ASSERT_EQUAL_UINT(pStatus->regime.targetFreqMHz,  720U);
    UT_ASSERT_EQUAL_UINT(pStatus->regime.targetRegimeId, LW2080_CTRL_CLK_NAFLL_REGIME_ID_FFR);
    UT_ASSERT_EQUAL_UINT(pStatus->dvco.minFreqMHz, 450U);
}

/*!
 * @brief      Test the method to program a given clock domain - FFR case
 */
UT_CASE_DEFINE(PMU_CLKNAFLL, clkNafllProgram_test1,
  UT_CASE_SET_DESCRIPTION("Test case for programming given Nafll device")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Set up the register mocking
 */
PRE_TEST_METHOD(clkNafllProgram_test1)
{
    LwU32 nafllSwFreqReqReg = CLK_NAFLL_REG_GET(&nafllDev1, LUT_SW_FREQ_REQ);
    UTF_IO_MOCK(nafllSwFreqReqReg,  nafllSwFreqReqReg,  NULL, NULL);

    // Test value - FFR regime
    nafllStatus1.pldivEngage             = LW_TRISTATE_INDETERMINATE;
    nafllStatus1.regime.targetRegimeId   = LW2080_CTRL_CLK_NAFLL_REGIME_ID_FFR;
    nafllStatus1.regime.targetFreqMHz    = 720U;

    // Initialize to _INIT values
    REG_WR32(FECS, nafllSwFreqReqReg,  0x00000000U);
}

/*!
 * @brief      Restore defaults
 */
POST_TEST_METHOD(clkNafllProgram_test1)
{
    nafllStatus1.pldivEngage             = LW_TRISTATE_INDETERMINATE;
    nafllStatus1.regime.targetRegimeId   = LW2080_CTRL_CLK_NAFLL_REGIME_ID_ILWALID;
    nafllStatus1.regime.targetFreqMHz    = 0U;
    nafllStatus1.regime.lwrrentRegimeId  = LW2080_CTRL_CLK_NAFLL_REGIME_ID_ILWALID;
    nafllStatus1.regime.lwrrentFreqMHz   = 0U;

    REG_WR32(FECS, CLK_NAFLL_REG_GET(&nafllDev1, LUT_SW_FREQ_REQ),  0x00000000U);
}

/*!
 * @brief      Test that the Unit Clock-NAFLLs successfully programs a given
 *             Nafll device with a set of configured parameters
 */
UT_CASE_RUN(PMU_CLKNAFLL, clkNafllProgram_test1)
{
    FLCN_STATUS                    status;
    CLK_NAFLL_DEVICE              *pNafllDev = CLK_NAFLL_DEVICE_GET(0U);
    LwU32                          nafllSwFreqReqReg = CLK_NAFLL_REG_GET(pNafllDev, LUT_SW_FREQ_REQ);
    RM_PMU_CLK_FREQ_TARGET_SIGNAL  target;

    PRE_TEST_NAME(clkNafllProgram_test1)();

    status = clkNafllProgram(pNafllDev, &nafllStatus1);
    UT_ASSERT_EQUAL_UINT(FLCN_OK, status);
    UT_ASSERT_EQUAL_UINT(REG_RD32(FECS, nafllSwFreqReqReg), 0x3000030LW);

    POST_TEST_NAME(clkNafllProgram_test1)();
}

/*!
 * @brief      Test the method to program a given clock domain - FR case
 */
UT_CASE_DEFINE(PMU_CLKNAFLL, clkNafllProgram_test2,
  UT_CASE_SET_DESCRIPTION("Test case for programming given Nafll device")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Set up the register mocking
 */
PRE_TEST_METHOD(clkNafllProgram_test2)
{
    LwU32 nafllSwFreqReqReg = CLK_NAFLL_REG_GET(&nafllDev1, LUT_SW_FREQ_REQ);
    UTF_IO_MOCK(nafllSwFreqReqReg,  nafllSwFreqReqReg,  NULL, NULL);

    // Test value - FR regime
    nafllStatus1.pldivEngage             = LW_TRISTATE_INDETERMINATE;
    nafllStatus1.regime.targetRegimeId   = LW2080_CTRL_CLK_NAFLL_REGIME_ID_FR;
    nafllStatus1.regime.targetFreqMHz    = 720U;

    // Initialize to _INIT values
    REG_WR32(FECS, nafllSwFreqReqReg,  0x00000000U);
}

/*!
 * @brief      Restore defaults
 */
POST_TEST_METHOD(clkNafllProgram_test2)
{
    nafllStatus1.pldivEngage             = LW_TRISTATE_INDETERMINATE;
    nafllStatus1.regime.targetRegimeId   = LW2080_CTRL_CLK_NAFLL_REGIME_ID_ILWALID;
    nafllStatus1.regime.targetFreqMHz    = 0U;
    nafllStatus1.regime.lwrrentRegimeId  = LW2080_CTRL_CLK_NAFLL_REGIME_ID_ILWALID;
    nafllStatus1.regime.lwrrentFreqMHz   = 0U;

    REG_WR32(FECS, CLK_NAFLL_REG_GET(&nafllDev1, LUT_SW_FREQ_REQ),  0x00000000U);
}

/*!
 * @brief      Test that the Unit Clock-NAFLLs successfully programs a given
 *             Nafll device with a set of configured parameters
 */
UT_CASE_RUN(PMU_CLKNAFLL, clkNafllProgram_test2)
{
    FLCN_STATUS                    status;
    CLK_NAFLL_DEVICE              *pNafllDev = CLK_NAFLL_DEVICE_GET(0U);
    LwU32                          nafllSwFreqReqReg = CLK_NAFLL_REG_GET(pNafllDev, LUT_SW_FREQ_REQ);
    RM_PMU_CLK_FREQ_TARGET_SIGNAL  target;

    PRE_TEST_NAME(clkNafllProgram_test2)();

    status = clkNafllProgram(pNafllDev, &nafllStatus1);
    UT_ASSERT_EQUAL_UINT(FLCN_OK, status);
    UT_ASSERT_EQUAL_UINT(REG_RD32(FECS, nafllSwFreqReqReg), 0x10000304U);

    POST_TEST_NAME(clkNafllProgram_test2)();
}

/*!
 * @brief      Test the method to program a given clock domain - VR case
 */
UT_CASE_DEFINE(PMU_CLKNAFLL, clkNafllProgram_test3,
  UT_CASE_SET_DESCRIPTION("Test case for programming given Nafll device")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Set up the register mocking
 */
PRE_TEST_METHOD(clkNafllProgram_test3)
{
    LwU32 nafllSwFreqReqReg = CLK_NAFLL_REG_GET(&nafllDev1, LUT_SW_FREQ_REQ);
    UTF_IO_MOCK(nafllSwFreqReqReg,  nafllSwFreqReqReg,  NULL, NULL);

    // Test value - VR regime
    nafllStatus1.pldivEngage             = LW_TRISTATE_INDETERMINATE;
    nafllStatus1.regime.targetRegimeId   = LW2080_CTRL_CLK_NAFLL_REGIME_ID_VR;
    nafllStatus1.regime.targetFreqMHz    = 720U;

    // Initialize to FR regime values to be able to compare for VR values
    REG_WR32(FECS, nafllSwFreqReqReg,  0x10000004U);
}

/*!
 * @brief      Restore defaults
 */
POST_TEST_METHOD(clkNafllProgram_test3)
{
    nafllStatus1.pldivEngage             = LW_TRISTATE_INDETERMINATE;
    nafllStatus1.regime.targetRegimeId   = LW2080_CTRL_CLK_NAFLL_REGIME_ID_ILWALID;
    nafllStatus1.regime.targetFreqMHz    = 0U;
    nafllStatus1.regime.lwrrentRegimeId  = LW2080_CTRL_CLK_NAFLL_REGIME_ID_ILWALID;
    nafllStatus1.regime.lwrrentFreqMHz   = 0U;

    REG_WR32(FECS, CLK_NAFLL_REG_GET(&nafllDev1, LUT_SW_FREQ_REQ),  0x00000000U);
}

/*!
 * @brief      Test that the Unit Clock-NAFLLs successfully programs a given
 *             Nafll device with a set of configured parameters
 */
UT_CASE_RUN(PMU_CLKNAFLL, clkNafllProgram_test3)
{
    FLCN_STATUS                    status;
    CLK_NAFLL_DEVICE              *pNafllDev = CLK_NAFLL_DEVICE_GET(0U);
    LwU32                          nafllSwFreqReqReg = CLK_NAFLL_REG_GET(pNafllDev, LUT_SW_FREQ_REQ);
    RM_PMU_CLK_FREQ_TARGET_SIGNAL  target;

    PRE_TEST_NAME(clkNafllProgram_test3)();

    status = clkNafllProgram(pNafllDev, &nafllStatus1);
    UT_ASSERT_EQUAL_UINT(FLCN_OK, status);
    UT_ASSERT_EQUAL_UINT(REG_RD32(FECS, nafllSwFreqReqReg), 0x00000000U);

    POST_TEST_NAME(clkNafllProgram_test3)();
}

/*!
 * @brief      Test the method to program a given clock domain - Error case
 */
UT_CASE_DEFINE(PMU_CLKNAFLL, clkNafllProgram_test4,
  UT_CASE_SET_DESCRIPTION("Test error case for programming given Nafll device")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Set up the register mocking
 */
PRE_TEST_METHOD(clkNafllProgram_test4)
{
    // Lut not initialized - error ILWALID_STATE
    nafllDev1.lutDevice.bInitialized = LW_FALSE;
}

/*!
 * @brief      Restore defaults
 */
POST_TEST_METHOD(clkNafllProgram_test4)
{
    // Defaults
    nafllDev1.lutDevice.bInitialized = LW_TRUE;
}

/*!
 * @brief      Test that the Unit Clock-NAFLLs handles error cases gracefully:
 *               - Lut not initialized - error ILWALID_STATE
 */
UT_CASE_RUN(PMU_CLKNAFLL, clkNafllProgram_test4)
{
    CLK_NAFLL_DEVICE *pNafllDev = CLK_NAFLL_DEVICE_GET(0U);

    PRE_TEST_NAME(clkNafllProgram_test4)();

    UT_ASSERT_EQUAL_UINT(FLCN_ERR_ILWALID_STATE,
                         clkNafllProgram(pNafllDev, &nafllStatus1));

    POST_TEST_NAME(clkNafllProgram_test4)();
}

/*!
 * @brief      Test the method to program a given clock domain - Error case
 */
UT_CASE_DEFINE(PMU_CLKNAFLL, clkNafllProgram_test5,
  UT_CASE_SET_DESCRIPTION("Test error case for programming given Nafll device")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Set up the register mocking
 */
PRE_TEST_METHOD(clkNafllProgram_test5)
{
    // Invalid target regime - error ILWALID_ARG
    nafllStatus1.regime.targetRegimeId   = LW2080_CTRL_CLK_NAFLL_REGIME_ID_ILWALID;
}

/*!
 * @brief      Test that the Unit Clock-NAFLLs handles error cases gracefully:
 *               - Invalid target regime - error ILWALID_ARG
 */
UT_CASE_RUN(PMU_CLKNAFLL, clkNafllProgram_test5)
{
    CLK_NAFLL_DEVICE *pNafllDev = CLK_NAFLL_DEVICE_GET(0U);

    PRE_TEST_NAME(clkNafllProgram_test5)();

    UT_ASSERT_EQUAL_UINT(FLCN_ERR_ILWALID_ARGUMENT,
                         clkNafllProgram(pNafllDev, &nafllStatus1));
}

/*!
 * @brief      Test the method to program a non-empty list of clock domains.
 */
UT_CASE_DEFINE(PMU_CLKNAFLL, clkNafllGrpProgram_test,
  UT_CASE_SET_DESCRIPTION("Test case for programing non-empty list of clocks domains")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Initialize the clkDomainList to program GPC/720MHz/FFR
 */
PRE_TEST_METHOD(clkNafllGrpProgram_test)
{
    LwU32       nafllSwFreqReqReg1 = CLK_NAFLL_REG_GET(&nafllDev1, LUT_SW_FREQ_REQ);
    LwU32       nafllSwFreqReqReg2 = CLK_NAFLL_REG_GET(&nafllDev2, LUT_SW_FREQ_REQ);

    clkDomainList.numDomains                   = 1;
    clkDomainList.clkDomains[0].clkDomain      = LW2080_CTRL_CLK_DOMAIN_GPCCLK;
    clkDomainList.clkDomains[0].clkFreqKHz     = 720U * 1000U;
    clkDomainList.clkDomains[0].regimeId       = LW2080_CTRL_CLK_NAFLL_REGIME_ID_FFR;
    clkDomainList.clkDomains[0].source         = LW2080_CTRL_CLK_PROG_1X_SOURCE_NAFLL;
    clkDomainList.clkDomains[0].dvcoMinFreqMHz = 450U * 1000U;

    UTF_IO_MOCK(nafllSwFreqReqReg1,  nafllSwFreqReqReg1,  NULL, NULL);
    UTF_IO_MOCK(nafllSwFreqReqReg2,  nafllSwFreqReqReg2,  NULL, NULL);
    UTF_IO_MOCK(LW_PTRIM_GPC_GPCNAFLL_ADC_CTRL(0),  LW_PTRIM_GPC_GPCNAFLL_ADC_CTRL(0),  NULL, NULL);
    UTF_IO_MOCK(LW_PTRIM_GPC_GPCNAFLL_ADC_CTRL(1),  LW_PTRIM_GPC_GPCNAFLL_ADC_CTRL(1),  NULL, NULL);

    // default values
    nafllStatus1.pldivEngage             = LW_TRISTATE_INDETERMINATE;
    nafllStatus1.regime.targetRegimeId   = LW2080_CTRL_CLK_NAFLL_REGIME_ID_ILWALID;
    nafllStatus1.regime.targetFreqMHz    = 0U;
    nafllStatus1.regime.lwrrentRegimeId  = LW2080_CTRL_CLK_NAFLL_REGIME_ID_ILWALID;
    nafllStatus1.regime.lwrrentFreqMHz   = 0U;
    nafllStatus2.pldivEngage             = LW_TRISTATE_INDETERMINATE;
    nafllStatus2.regime.targetRegimeId   = LW2080_CTRL_CLK_NAFLL_REGIME_ID_ILWALID;
    nafllStatus2.regime.targetFreqMHz    = 0U;
    nafllStatus2.regime.lwrrentRegimeId  = LW2080_CTRL_CLK_NAFLL_REGIME_ID_ILWALID;
    nafllStatus2.regime.lwrrentFreqMHz   = 0U;

    // Initialize to _INIT values
    REG_WR32(FECS, nafllSwFreqReqReg1,  0x00000000U);
    REG_WR32(FECS, nafllSwFreqReqReg2,  0x00000000U);
    REG_WR32(FECS, LW_PTRIM_GPC_GPCNAFLL_ADC_CTRL(0),  0x00000002U);
    REG_WR32(FECS, LW_PTRIM_GPC_GPCNAFLL_ADC_CTRL(1),  0x00000002U);
}

/*!
 * @brief      Reset the clkDomainList to zeroes and restore defaults
 */
POST_TEST_METHOD(clkNafllGrpProgram_test)
{
    (void)memset(&clkDomainList, 0U, sizeof(clkDomainList));

    nafllStatus1.pldivEngage             = LW_TRISTATE_INDETERMINATE;
    nafllStatus1.regime.targetRegimeId   = LW2080_CTRL_CLK_NAFLL_REGIME_ID_ILWALID;
    nafllStatus1.regime.targetFreqMHz    = 0U;
    nafllStatus1.regime.lwrrentRegimeId  = LW2080_CTRL_CLK_NAFLL_REGIME_ID_ILWALID;
    nafllStatus1.regime.lwrrentFreqMHz   = 0U;
    nafllStatus2.pldivEngage             = LW_TRISTATE_INDETERMINATE;
    nafllStatus2.regime.targetRegimeId   = LW2080_CTRL_CLK_NAFLL_REGIME_ID_ILWALID;
    nafllStatus2.regime.targetFreqMHz    = 0U;
    nafllStatus2.regime.lwrrentRegimeId  = LW2080_CTRL_CLK_NAFLL_REGIME_ID_ILWALID;
    nafllStatus2.regime.lwrrentFreqMHz   = 0U;

    REG_WR32(FECS, CLK_NAFLL_REG_GET(&nafllDev1, LUT_SW_FREQ_REQ),  0x00000000U);
    REG_WR32(FECS, CLK_NAFLL_REG_GET(&nafllDev2, LUT_SW_FREQ_REQ),  0x00000000U);
}

/*!
 * @brief      Test that the Unit Clock-NAFLLs successfully programs a non-empty
 *             list of clock domains
 */
UT_CASE_RUN(PMU_CLKNAFLL, clkNafllGrpProgram_test)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       nafllSwFreqReqReg1 = CLK_NAFLL_REG_GET(&nafllDev1, LUT_SW_FREQ_REQ);
    LwU32       nafllSwFreqReqReg2 = CLK_NAFLL_REG_GET(&nafllDev2, LUT_SW_FREQ_REQ);
    LW2080_CTRL_CLK_FREQ_SOURCE_STATUS_NAFLL *pStatus1 = &nafllStatus1;
    LW2080_CTRL_CLK_FREQ_SOURCE_STATUS_NAFLL *pStatus2 = &nafllStatus2;

    PRE_TEST_NAME(clkNafllGrpProgram_test)();

    status = clkNafllGrpProgram_IMPL(&clkDomainList);

    // Config parameters
    UT_ASSERT_EQUAL_UINT(pStatus1->regime.targetFreqMHz,  720U);
    UT_ASSERT_EQUAL_UINT(pStatus1->regime.targetRegimeId, LW2080_CTRL_CLK_NAFLL_REGIME_ID_FFR);
    UT_ASSERT_EQUAL_UINT(pStatus2->regime.targetFreqMHz,  720U);
    UT_ASSERT_EQUAL_UINT(pStatus2->regime.targetRegimeId, LW2080_CTRL_CLK_NAFLL_REGIME_ID_FFR);

    // Programmed registers
    UT_ASSERT_EQUAL_UINT(REG_RD32(FECS, nafllSwFreqReqReg1), 0x3000030LW);
    UT_ASSERT_EQUAL_UINT(REG_RD32(FECS, nafllSwFreqReqReg2), 0x3000030LW);

    // Cached values of the lwrrentRegimeId & lwrrentFreqMHz
    UT_ASSERT_EQUAL_UINT(nafllDev1.pStatus->regime.lwrrentRegimeId, LW2080_CTRL_CLK_NAFLL_REGIME_ID_FFR);
    UT_ASSERT_EQUAL_UINT(nafllDev1.pStatus->regime.lwrrentFreqMHz,  720U);
    UT_ASSERT_EQUAL_UINT(nafllDev2.pStatus->regime.lwrrentRegimeId, LW2080_CTRL_CLK_NAFLL_REGIME_ID_FFR);
    UT_ASSERT_EQUAL_UINT(nafllDev2.pStatus->regime.lwrrentFreqMHz,  720U);

    UT_ASSERT_EQUAL_UINT(FLCN_OK, status);

    POST_TEST_NAME(clkNafllGrpProgram_test)();
}

/*!
 * @brief      Test the SW regime-id to HW regime-id mapping - FFR case
 */
UT_CASE_DEFINE(PMU_CLKNAFLL, clkNafllGetHwRegimeBySwRegime_test1,
  UT_CASE_SET_DESCRIPTION("Test case for mapping the FFR SW regime-id to _SW_REQ HW regime-id.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test that the Unit Clock-NAFLLs successfully maps the SW-FFR
 *             regime to the HW-SW_REQ regime
 */
UT_CASE_RUN(PMU_CLKNAFLL, clkNafllGetHwRegimeBySwRegime_test1)
{
    LwU8        hwRegimeId;
    FLCN_STATUS status;

    // Execute unit under test
    status = clkNafllGetHwRegimeBySwRegime(LW2080_CTRL_CLK_NAFLL_REGIME_ID_FFR, &hwRegimeId);
    UT_ASSERT_EQUAL_UINT(FLCN_OK, status);
    UT_ASSERT_EQUAL_UINT(LW2080_CTRL_CLK_NAFLL_SW_OVERRIDE_LUT_USE_SW_REQ, hwRegimeId);
}

/*!
 * @brief      Test the SW regime-id to HW regime-id mapping - FR case
 */
UT_CASE_DEFINE(PMU_CLKNAFLL, clkNafllGetHwRegimeBySwRegime_test2,
  UT_CASE_SET_DESCRIPTION("Test case for mapping the FR SW regime-id to _MIN HW regime-id.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test that the Unit Clock-NAFLLs successfully maps the SW-FR
 *             regime to the HW-MIN regime
 */
UT_CASE_RUN(PMU_CLKNAFLL, clkNafllGetHwRegimeBySwRegime_test2)
{
    LwU8        hwRegimeId;
    FLCN_STATUS status;

    // Execute unit under test
    status = clkNafllGetHwRegimeBySwRegime(LW2080_CTRL_CLK_NAFLL_REGIME_ID_FR, &hwRegimeId);
    UT_ASSERT_EQUAL_UINT(FLCN_OK, status);
    UT_ASSERT_EQUAL_UINT(LW2080_CTRL_CLK_NAFLL_SW_OVERRIDE_LUT_USE_MIN, hwRegimeId);
}

/*!
 * @brief      Test the SW regime-id to HW regime-id mapping - VR case
 */
UT_CASE_DEFINE(PMU_CLKNAFLL, clkNafllGetHwRegimeBySwRegime_test3,
  UT_CASE_SET_DESCRIPTION("Test case for mapping the VR SW regime-id to _HW_REQ HW regime-id.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test that the Unit Clock-NAFLLs successfully maps the SW-VR
 *             regime to the HW-HW_REQ regime
 */
UT_CASE_RUN(PMU_CLKNAFLL, clkNafllGetHwRegimeBySwRegime_test3)
{
    LwU8        hwRegimeId;
    FLCN_STATUS status;

    // Execute unit under test
    status = clkNafllGetHwRegimeBySwRegime(LW2080_CTRL_CLK_NAFLL_REGIME_ID_VR, &hwRegimeId);
    UT_ASSERT_EQUAL_UINT(FLCN_OK, status);
    UT_ASSERT_EQUAL_UINT(LW2080_CTRL_CLK_NAFLL_SW_OVERRIDE_LUT_USE_HW_REQ, hwRegimeId);
}

/*!
 * @brief      Test the error case for the SW regime-id to HW regime-id mapping.
 */
UT_CASE_DEFINE(PMU_CLKNAFLL, clkNafllGetHwRegimeBySwRegime_test4,
  UT_CASE_SET_DESCRIPTION("Test case for mapping _ILWALID SW regime-id returning error status.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test that the Unit Clock-NAFLLs fails gracefully for an invalid
 *             SW regime-id
 */
UT_CASE_RUN(PMU_CLKNAFLL, clkNafllGetHwRegimeBySwRegime_test4)
{
    LwU8        hwRegimeId;
    FLCN_STATUS status;

    // Execute unit under test
    status = clkNafllGetHwRegimeBySwRegime(LW2080_CTRL_CLK_NAFLL_REGIME_ID_ILWALID, &hwRegimeId);
    UT_ASSERT_EQUAL_UINT(FLCN_ERR_ILWALID_ARGUMENT, status);
}

/*!
 * @brief      Test the target regime callwlation given a Voltage-Frequency pair
 */
UT_CASE_DEFINE(PMU_CLKNAFLL, clkNafllDevTargetRegimeGet_test1,
  UT_CASE_SET_DESCRIPTION("Test case for mapping freq>FFR-limit to FR SW regime-id")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test that the Unit Clock-NAFLLs successfully maps a frequency
 *             value greater than FFR-limit to the FR SW-regime
 */
UT_CASE_RUN(PMU_CLKNAFLL, clkNafllDevTargetRegimeGet_test1)
{
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST_ITEM voltList;
    CLK_NAFLL_DEVICE                    *pNafllDev = CLK_NAFLL_DEVICE_GET(0U);
    LwU8 targetRegime;

    pNafllDev->regimeDesc.targetRegimeIdOverride = LW2080_CTRL_CLK_NAFLL_REGIME_ID_ILWALID;

    // Execute unit under test
    targetRegime = clkNafllDevTargetRegimeGet(pNafllDev, 720U, 450U, &voltList);
    UT_ASSERT_EQUAL_UINT(LW2080_CTRL_CLK_NAFLL_REGIME_ID_FR, targetRegime);
}

/*!
 * @brief      Test the target regime callwlation given a Voltage-Frequency pair - FFR case
 */
UT_CASE_DEFINE(PMU_CLKNAFLL, clkNafllDevTargetRegimeGet_test2,
  UT_CASE_SET_DESCRIPTION("Test case for mapping freq<FFR-limit to FFR SW regime-id")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test that the Unit Clock-NAFLLs successfully maps a frequency
 *             value lesser than FFR-limit to the FFR SW-regime
 */
UT_CASE_RUN(PMU_CLKNAFLL, clkNafllDevTargetRegimeGet_test2)
{
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST_ITEM voltList;
    CLK_NAFLL_DEVICE                    *pNafllDev = CLK_NAFLL_DEVICE_GET(0U);
    LwU8 targetRegime;

    pNafllDev->regimeDesc.targetRegimeIdOverride = LW2080_CTRL_CLK_NAFLL_REGIME_ID_ILWALID;

    // Execute unit under test
    targetRegime = clkNafllDevTargetRegimeGet(pNafllDev, 390U, 450U, &voltList);
    UT_ASSERT_EQUAL_UINT(LW2080_CTRL_CLK_NAFLL_REGIME_ID_FFR, targetRegime);
}

/*!
 * @brief      Test the target regime callwlation given a Voltage-Frequency pair - VR case
 */
UT_CASE_DEFINE(PMU_CLKNAFLL, clkNafllDevTargetRegimeGet_test3,
  UT_CASE_SET_DESCRIPTION("Test case for mapping volt>minNoiseAwareVolt to VR SW regime-id")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test that the Unit Clock-NAFLLs successfully allows a regime-id
 *             override to FR given the voltage > min Noise-Aware voltage value
 */
UT_CASE_RUN(PMU_CLKNAFLL, clkNafllDevTargetRegimeGet_test3)
{
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST_ITEM voltList;
    CLK_NAFLL_DEVICE                    *pNafllDev = CLK_NAFLL_DEVICE_GET(0U);
    LwU8 targetRegime;

    voltList.voltageuV = 810000U;
    voltList.voltageMinNoiseUnawareuV = 700000U;
    pNafllDev->regimeDesc.targetRegimeIdOverride = LW2080_CTRL_CLK_NAFLL_REGIME_ID_VR_ABOVE_NOISE_UNAWARE_VMIN;

    // Execute unit under test
    targetRegime = clkNafllDevTargetRegimeGet(pNafllDev, 1000U, 450U, &voltList);
    UT_ASSERT_EQUAL_UINT(LW2080_CTRL_CLK_NAFLL_REGIME_ID_VR, targetRegime);
}

/*!
 * @brief      Test the target regime callwlation given a Voltage-Frequency pair - Error case
 */
UT_CASE_DEFINE(PMU_CLKNAFLL, clkNafllDevTargetRegimeGet_test4,
  UT_CASE_SET_DESCRIPTION("Test case for gracefully handling invalid parameters")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test that the Unit Clock-NAFLLs gracefully handles invalid parameters
 */
UT_CASE_RUN(PMU_CLKNAFLL, clkNafllDevTargetRegimeGet_test4)
{
    LwU8 targetRegime;
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST_ITEM voltList;

    // Execute unit under test
    targetRegime = clkNafllDevTargetRegimeGet(CLK_NAFLL_DEVICE_GET(0U), 0U, 450U, NULL);
    UT_ASSERT_EQUAL_UINT(LW2080_CTRL_CLK_NAFLL_REGIME_ID_ILWALID, targetRegime);

    targetRegime = clkNafllDevTargetRegimeGet(NULL, 0U, 450U, &voltList);
    UT_ASSERT_EQUAL_UINT(LW2080_CTRL_CLK_NAFLL_REGIME_ID_ILWALID, targetRegime);
}
