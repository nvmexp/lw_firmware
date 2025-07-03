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
 * @file    voltdev-test.c
 * @brief   Unit tests for logic in VOLT_DEVICE.
 */

/* ------------------------ System Includes --------------------------------- */
#include "test-macros.h"

/* ------------------------ Application Includes ---------------------------- */
#include "pmusw.h"
#include "lwstatus.h"
#include "volt/objvolt.h"
#include "volt/objvolt-test.h"
#include "lib/lib_pwm.h"
#include "dev_pwr_csb.h"
#include "lib/lib_pwm-mock.h"
#include "lwosreg.h"
#include "lwrtos-mock.h"
#include "lwrtos.h"

/* ------------------------ Defines ----------------------------------------- */
/* ------------------------ Mocked Functions -------------------------------- */
/* ------------------------ Global Data-------------------------------------- */
PWM_SOURCE_DESCRIPTOR_TRIGGER  pwmSrcDescTrigger;
VOLT_DEVICE_PWM                voltDevPwm;

/* ------------------------ Type Definitions -------------------------------- */
/*!
 * @brief   Structure describing a set of inputs to get voltage internal interface
 *          and the set of expected results.
 */
typedef struct VOLT_DEVICE_GET_VOLTAGE_INPUT_OUTPUT
{
    VOLT_DEVICE_PWM             voltDevicePWM;
    PWM_PARAMS_GET_MOCK_CONFIG  pwmParamsMockConfig;
    LwU32                       expectedVoltageuV;
    FLCN_STATUS                 expectedResult;
} VOLT_DEVICE_GET_VOLTAGE_INPUT_OUTPUT;

/*!
 * @brief   Structure describing a set of inputs to set voltage internal interface
 *          and the set of expected results.
 */
typedef struct VOLT_DEVICE_SET_VOLTAGE_INPUT_OUTPUT
{
    VOLT_DEVICE_PWM             voltDevicePWM;
    LwU32                       targetVoltageuV;
    LwU32                       expectedDutyCycle;
    FLCN_STATUS                 expectedResult;
} VOLT_DEVICE_SET_VOLTAGE_INPUT_OUTPUT;

/*!
 * @brief   Structure describing a set of inputs to round voltage internal interface
 *          and the set of expected results.
 */
typedef struct VOLT_DEVICE_ROUND_VOLTAGE_INPUT_OUTPUT
{
    VOLT_DEVICE     voltDevice;
    LwS32          *pVoltageuV;
    LwBool          bRoundUp;
    LwBool          bBound;
    LwS32           expectedRoundedVoltage;
    FLCN_STATUS     expectedResult;
} VOLT_DEVICE_ROUND_VOLTAGE_INPUT_OUTPUT;

/* ------------------------ Local Data-------------------------------------- */
extern PWM_PARAMS_GET_MOCK_CONFIG pwmParamsGet_MOCK_CONFIG;

/*!
 * @brief   Table of positive test conditions for voltDeviceGetVoltage.
 */
static  VOLT_DEVICE_GET_VOLTAGE_INPUT_OUTPUT voltDeviceGetVoltagePositive_inout[] =
{
    {
        // 50% duty cycle
        .voltDevicePWM =
        {
            .super =
            {
                .super = 
                {
                    .type = LW2080_CTRL_VOLT_VOLT_DEVICE_TYPE_PWM,
                },
            },
            .voltageBaseuV = 300000,
            .voltageOffsetScaleuV = 1000000,
        },
        .pwmParamsMockConfig = 
        {
            .pwmDutyCycle = 80,
            .pwmPeriod = 160,
            .bDone = LW_TRUE,
        },
        .expectedVoltageuV = 800000,
        .expectedResult     = FLCN_OK,
    },
};

/*!
 * @brief   Table of negative test conditions for voltDeviceGetVoltage.
 * @note    voltDeviceGetVoltage_IMPL and voltDeviceGetVoltage_PWM is an internal
 *          API not expected to be used by any clients. So, no need to write test
 *          cases for checking input arguments for NULL since that would never
 *          happen since the VOLT code is fully aware of the internal functionality.
 */
static  VOLT_DEVICE_GET_VOLTAGE_INPUT_OUTPUT voltDeviceGetVoltageNegative_inout[] =
{
    {
        // default case
        .voltDevicePWM =
        {
            .super =
            {
                .super = 
                {
                    .type = 0x99,
                },
            },
            .voltageBaseuV = 300000,
            .voltageOffsetScaleuV = 1000000,
        },
        .pwmParamsMockConfig = 
        {
            .pwmDutyCycle = 80,
            .pwmPeriod = 160,
            .bDone = LW_TRUE,
            .status = FLCN_OK,
        },
        .expectedVoltageuV = 800000,
        .expectedResult     = FLCN_ERR_NOT_SUPPORTED,
    },
    {
        // unsupported VID feature
        .voltDevicePWM =
        {
            .super =
            {
                .super = 
                {
                    .type = LW2080_CTRL_VOLT_VOLT_DEVICE_TYPE_VID,
                },
            },
            .voltageBaseuV = 300000,
            .voltageOffsetScaleuV = 1000000,
        },
        .pwmParamsMockConfig = 
        {
            .pwmDutyCycle = 80,
            .pwmPeriod = 160,
            .bDone = LW_TRUE,
            .status = FLCN_OK,
        },
        .expectedVoltageuV = 800000,
        .expectedResult     = FLCN_ERR_NOT_SUPPORTED,
    },
    {
        // pwmParamsGet() failure
        .voltDevicePWM =
        {
            .super =
            {
                .super = 
                {
                    .type = LW2080_CTRL_VOLT_VOLT_DEVICE_TYPE_PWM,
                },
            },
            .voltageBaseuV = 300000,
            .voltageOffsetScaleuV = 1000000,
        },
        .pwmParamsMockConfig = 
        {
            .pwmDutyCycle = 80,
            .pwmPeriod = 160,
            .bDone = LW_TRUE,
            .status = FLCN_ERR_NOT_SUPPORTED,
        },
        .expectedVoltageuV = 800000,
        .expectedResult     = FLCN_ERR_NOT_SUPPORTED,
    },
    {
        // pwmParamsGet() failure
        .voltDevicePWM =
        {
            .super =
            {
                .super = 
                {
                    .type = LW2080_CTRL_VOLT_VOLT_DEVICE_TYPE_PWM,
                },
            },
            .voltageBaseuV = 300000,
            .voltageOffsetScaleuV = 1000000,
        },
        .pwmParamsMockConfig = 
        {
            .pwmDutyCycle = 80,
            .pwmPeriod = 0,
            .bDone = LW_TRUE,
            .status = FLCN_OK,
        },
        .expectedVoltageuV = 800000,
        .expectedResult     = FLCN_ERR_ILWALID_STATE,
    },
};

/*!
 * @brief   Table of positive test conditions for voltDeviceSetVoltage.
 */
static  VOLT_DEVICE_SET_VOLTAGE_INPUT_OUTPUT voltDeviceSetVoltagePositive_inout[] =
{
    {
        .voltDevicePWM =
        {
            .super =
            {
                .super = 
                {
                    .type = LW2080_CTRL_VOLT_VOLT_DEVICE_TYPE_PWM,
                },
            },
            .pPwmSrcDesc          = &pwmSrcDescTrigger,
            .voltageBaseuV        = 300000,
            .voltageOffsetScaleuV = 1000000,
            .rawPeriod            = 160,
        },
        .targetVoltageuV    = 600000,
        .expectedDutyCycle  = 48,
        .expectedResult     = FLCN_OK,
    },
};

/*!
 * @brief   Table of negative test conditions for voltDeviceSetVoltage.
 */
static  VOLT_DEVICE_SET_VOLTAGE_INPUT_OUTPUT voltDeviceSetVoltageNegative_inout[] =
{
    {
        .voltDevicePWM =
        {
            .super =
            {
                .super = 
                {
                    .type = LW2080_CTRL_VOLT_VOLT_DEVICE_TYPE_VID,
                },
            },
            .pPwmSrcDesc          = &pwmSrcDescTrigger,
            .voltageBaseuV        = 300000,
            .voltageOffsetScaleuV = 1000000,
            .rawPeriod            = 160,
        },
        .targetVoltageuV   = 600000,
        .expectedDutyCycle = 48,
        .expectedResult    = FLCN_ERR_NOT_SUPPORTED,
    },
};

LwS32 roundVoltagePositiveTestCase1 = 45;
LwS32 roundVoltagePositiveTestCase2 = 45;
LwS32 roundVoltagePositiveTestCase3 = -45;
LwS32 roundVoltagePositiveTestCase4 = -45;
LwS32 roundVoltagePositiveTestCase5 = 95;
LwS32 roundVoltagePositiveTestCase6 = 95;
LwS32 roundVoltagePositiveTestCase7 = 45;
LwS32 roundVoltagePositiveTestCase8 = 45;

/*!
 * @brief   Table of positive test conditions for voltDeviceRoundVoltage.
 */
static  VOLT_DEVICE_ROUND_VOLTAGE_INPUT_OUTPUT voltDeviceRoundVoltagePositive_inout[] =
{
        // Voltage step = 0 uV
    {
        .voltDevice =
        {
            .voltStepuV   = 0,
            .voltageMinuV = 40,
            .voltageMaxuV = 100,
        },
        .pVoltageuV              = &roundVoltagePositiveTestCase1,
        .bRoundUp                = LW_TRUE,
        .bBound                  = LW_TRUE,
        .expectedRoundedVoltage  = 40,
        .expectedResult          = FLCN_OK,
    },
    // Test round down
    {
        .voltDevice =
        {
            .voltStepuV   = 25,
            .voltageMinuV = 10,
            .voltageMaxuV = 100,
        },
        .pVoltageuV              = &roundVoltagePositiveTestCase2,
        .bRoundUp                = LW_FALSE,
        .bBound                  = LW_FALSE,
        .expectedRoundedVoltage  = 25,
        .expectedResult          = FLCN_OK,
    },
    // Test negative input voltage, round up
    {
        .voltDevice =
        {
            .voltStepuV   = 25,
            .voltageMinuV = 10,
            .voltageMaxuV = 100,
        },
        .pVoltageuV              = &roundVoltagePositiveTestCase3,
        .bRoundUp                = LW_TRUE,
        .bBound                  = LW_FALSE,
        .expectedRoundedVoltage  = -25,
        .expectedResult          = FLCN_OK,
    },
    // Test negative input voltage, round down
    {
        .voltDevice =
        {
            .voltStepuV   = 25,
            .voltageMinuV = 10,
            .voltageMaxuV = 100,
        },
        .pVoltageuV              = &roundVoltagePositiveTestCase4,
        .bRoundUp                = LW_FALSE,
        .bBound                  = LW_FALSE,
        .expectedRoundedVoltage  = -50,
        .expectedResult          = FLCN_OK,
    },
    // Test bound for vmin
    {
        .voltDevice =
        {
            .voltStepuV   = 25,
            .voltageMinuV = 90,
            .voltageMaxuV = 100,
        },
        .pVoltageuV              = &roundVoltagePositiveTestCase5,
        .bRoundUp                = LW_FALSE,
        .bBound                  = LW_TRUE,
        .expectedRoundedVoltage  = 90,
        .expectedResult          = FLCN_OK,
    },
    // Test bound for vmax
    {
        .voltDevice =
        {
            .voltStepuV     = 25,
            .voltageMinuV   = 10,
            .voltageMaxuV   = 98,
        },
        .pVoltageuV              = &roundVoltagePositiveTestCase6,
        .bRoundUp                = LW_TRUE,
        .bBound                  = LW_TRUE,
        .expectedRoundedVoltage  = 98,
        .expectedResult          = FLCN_OK,
    },
    // Test negative volt step
    {
        .voltDevice =
        {
            .voltStepuV   = -15,
            .voltageMinuV = 40,
            .voltageMaxuV = 100,
        },
        .pVoltageuV              = &roundVoltagePositiveTestCase7,
        .bRoundUp                = LW_TRUE,
        .bBound                  = LW_TRUE,
        .expectedRoundedVoltage  = 45,
        .expectedResult          = FLCN_OK,
    },
    // Test round up functionality
    {
        .voltDevice =
        {
            .voltStepuV   = 25,
            .voltageMinuV = 10,
            .voltageMaxuV = 100,
        },
        .pVoltageuV              = &roundVoltagePositiveTestCase8,
        .bRoundUp                = LW_TRUE,
        .bBound                  = LW_FALSE,
        .expectedRoundedVoltage  = 50,
        .expectedResult          = FLCN_OK,
    },
};

UT_SUITE_DEFINE(PMU_VOLTDEVICE,
                UT_SUITE_SET_COMPONENT("Unit Volt Device")
                UT_SUITE_SET_DESCRIPTION("Unit tests for Unit Volt Device")
                UT_SUITE_SET_OWNER("ngodbole"))

UT_CASE_DEFINE(PMU_VOLTDEVICE, voltDeviceGetVoltagePositive,
  UT_CASE_SET_DESCRIPTION("Test voltDeviceGetVoltagePositive")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test that the Unit Volt Device Get Voltage interface handles all positive
 *             inputs correctly.
 *
 * @details    The test shall ilwoke the Unit Volt Device method that gets current
 *             programmed voltage in uV.
 *
 *             Parameters: void
 *
 *             The test passes if the expected result in the dataset matches with
 *             the actual result received after exelwting the test.
 *
 *             Any other return value, constitutes failure.
 */
UT_CASE_RUN(PMU_VOLTDEVICE,voltDeviceGetVoltagePositive)
{
    LwU8    numEntriesPositive =
        sizeof(voltDeviceGetVoltagePositive_inout) / sizeof(VOLT_DEVICE_GET_VOLTAGE_INPUT_OUTPUT);
    FLCN_STATUS     status;
    VOLT_DEVICE_PWM voltDevPWM;
    LwU8            i;
    LwU32           lwrrVoltuV;


    for (i = 0; i < numEntriesPositive; i++)
    {
        memset(&voltDevPWM, 0, sizeof(VOLT_DEVICE_PWM));
        memset(&pwmParamsGet_MOCK_CONFIG, 0, sizeof(PWM_PARAMS_GET_MOCK_CONFIG));
        lwrrVoltuV = 0;

        // Copy the data set.
        memcpy(&voltDevPWM, &voltDeviceGetVoltagePositive_inout[i].voltDevicePWM, sizeof(VOLT_DEVICE_PWM));
        memcpy(&pwmParamsGet_MOCK_CONFIG, &voltDeviceGetVoltagePositive_inout[i].pwmParamsMockConfig, sizeof(PWM_PARAMS_GET_MOCK_CONFIG));

        boardObjDynamicCastMockInit();
        boardObjDynamicCastMockAddEntry(0, (void *)&voltDeviceGetVoltagePositive_inout[i].voltDevicePWM);

        // Run the function under test.
        status = voltDeviceGetVoltage_IMPL((VOLT_DEVICE *)&voltDevPWM, &lwrrVoltuV);

        // Verify result.
        UT_ASSERT_EQUAL_UINT(status, voltDeviceGetVoltagePositive_inout[i].expectedResult);
        UT_ASSERT_EQUAL_UINT(lwrrVoltuV, voltDeviceGetVoltagePositive_inout[i].expectedVoltageuV);
        UT_ASSERT_EQUAL_UINT(0U, UTF_INTRINSICS_MOCK_GET_HALT_COUNT());
    }
}

UT_CASE_DEFINE(PMU_VOLTDEVICE, voltDeviceGetVoltageNegative,
  UT_CASE_SET_DESCRIPTION("Test voltDeviceGetVoltageNegative")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test that the Unit Volt Device Get Voltage interface handles all negative
 *             inputs correctly.
 *
 * @details    The test shall ilwoke the Unit Volt Device method that gets current
 *             programmed voltage in uV.
 *
 *             Parameters: void
 *
 *             The test passes if the expected result in the dataset matches with
 *             the actual result received after exelwting the test.
 *
 *             Any other return value, constitutes failure.
 */
UT_CASE_RUN(PMU_VOLTDEVICE,voltDeviceGetVoltageNegative)
{
    LwU8    numEntriesNegative =
        sizeof(voltDeviceGetVoltageNegative_inout) / sizeof(VOLT_DEVICE_GET_VOLTAGE_INPUT_OUTPUT);
    FLCN_STATUS     status;
    VOLT_DEVICE_PWM voltDevPWM;
    LwU8            i;
    LwU32           lwrrVoltuV;


    for (i = 0; i < numEntriesNegative; i++)
    {
        memset(&voltDevPWM, 0, sizeof(VOLT_DEVICE_PWM));
        memset(&pwmParamsGet_MOCK_CONFIG, 0, sizeof(PWM_PARAMS_GET_MOCK_CONFIG));
        lwrrVoltuV = 0;

        // Copy the data set.
        memcpy(&voltDevPWM, &voltDeviceGetVoltageNegative_inout[i].voltDevicePWM, sizeof(VOLT_DEVICE_PWM));
        memcpy(&pwmParamsGet_MOCK_CONFIG, &voltDeviceGetVoltageNegative_inout[i].pwmParamsMockConfig, sizeof(PWM_PARAMS_GET_MOCK_CONFIG));

        boardObjDynamicCastMockInit();
        boardObjDynamicCastMockAddEntry(0, (void *)&voltDeviceGetVoltageNegative_inout[i].voltDevicePWM);

        // Run the function under test.
        status = voltDeviceGetVoltage_IMPL((VOLT_DEVICE *)&voltDevPWM, &lwrrVoltuV);

        // Verify result.
        UT_ASSERT_EQUAL_UINT(status, voltDeviceGetVoltageNegative_inout[i].expectedResult);
    }
}

/*!
 * @brief      Pre-test setup for testing the set voltage interface for Unit.
 *             Volt Device with different set of inputs.
 *
 * @details    Before the test is run, set the input vales for mocked interfaces.
 */
PRE_TEST_METHOD(voltDeviceSetVoltage)
{
    pwmSrcDescTrigger.super.type          = PWM_SOURCE_DESCRIPTOR_TYPE_TRIGGER;
    pwmSrcDescTrigger.super.addrPeriod    = LW_CPWR_THERM_VID0_PWM(0);
    pwmSrcDescTrigger.super.addrDutycycle = LW_CPWR_THERM_VID1_PWM(0);
    pwmSrcDescTrigger.super.mask          = DRF_MASK(LW_CPWR_THERM_VID0_PWM_PERIOD);
    pwmSrcDescTrigger.super.triggerIdx    = 0;
    pwmSrcDescTrigger.super.doneIdx       = 0;
    pwmSrcDescTrigger.super.bus           = REG_BUS_CSB;
    pwmSrcDescTrigger.super.bCancel       = LW_TRUE;
    pwmSrcDescTrigger.super.bTrigger      = LW_TRUE;
    pwmSrcDescTrigger.addrTrigger         = LW_CPWR_THERM_VID_PWM_TRIGGER_MASK;
    UTF_IO_MOCK(LW_CPWR_THERM_VID0_PWM(0), LW_CPWR_THERM_VID0_PWM(0),  NULL, NULL);
    REG_WR32(CSB, LW_CPWR_THERM_VID0_PWM(0), 0xA0);
    UTF_IO_MOCK(LW_CPWR_THERM_VID1_PWM(0),  LW_CPWR_THERM_VID1_PWM(0),  NULL, NULL);
    REG_WR32(CSB, LW_CPWR_THERM_VID1_PWM(0), 0x0);
    UTF_IO_MOCK(LW_CPWR_THERM_VID_PWM_TRIGGER_MASK, LW_CPWR_THERM_VID_PWM_TRIGGER_MASK, NULL, NULL);
    REG_WR32(CSB, LW_CPWR_THERM_VID_PWM_TRIGGER_MASK, 0x0);
}

/*!
 * @brief      Post-test setup for testing the set voltage interface for Unit.
 *             Volt Device with different set of inputs.
 *
 * @details    Before the test is run, set the input vales for mocked interfaces.
 */
POST_TEST_METHOD(voltDeviceSetVoltage)
{
    memset(&pwmSrcDescTrigger, 0, sizeof(PWM_SOURCE_DESCRIPTOR_TRIGGER));
}

UT_CASE_DEFINE(PMU_VOLTDEVICE, voltDeviceSetVoltagePositive,
  UT_CASE_SET_DESCRIPTION("Test voltDeviceSetVoltagePositive")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test that the Unit Volt Device Set Voltage interface handles all positive
 *             inputs correctly.
 *
 * @details    The test shall ilwoke the Unit Volt Device method that gets current
 *             programmed voltage in uV.
 *
 *             Parameters: void
 *
 *             The test passes if the expected result in the dataset matches with
 *             the actual result received after exelwting the test.
 *
 *             Any other return value, constitutes failure.
 */
UT_CASE_RUN(PMU_VOLTDEVICE,voltDeviceSetVoltagePositive)
{
    LwU8    numEntriesPositive =
        sizeof(voltDeviceSetVoltagePositive_inout) / sizeof(VOLT_DEVICE_SET_VOLTAGE_INPUT_OUTPUT);
    FLCN_STATUS     status;
    VOLT_DEVICE_PWM voltDevPWM;
    LwU8            i;

    PRE_TEST_NAME(voltDeviceSetVoltage)();
    for (i = 0; i < numEntriesPositive; i++)
    {
        memset(&voltDevPWM, 0, sizeof(VOLT_DEVICE_PWM));

        // Copy the data set.
        memcpy(&voltDevPWM, &voltDeviceSetVoltagePositive_inout[i].voltDevicePWM, sizeof(VOLT_DEVICE_PWM));

        // Run the function under test.
        status = voltDeviceSetVoltage_IMPL((VOLT_DEVICE *)&voltDevPWM, voltDeviceSetVoltagePositive_inout[i].targetVoltageuV, LW_FALSE, LW_FALSE, LW2080_CTRL_VOLT_VOLT_RAIL_ACTION_VF_SWITCH);

        // Verify result.
        UT_ASSERT_EQUAL_UINT(status, voltDeviceSetVoltagePositive_inout[i].expectedResult);

        UT_ASSERT_EQUAL_UINT(REG_RD32(CSB, LW_CPWR_THERM_VID1_PWM(0)), voltDeviceSetVoltagePositive_inout[i].expectedDutyCycle);
        UT_ASSERT_EQUAL_UINT(0U, UTF_INTRINSICS_MOCK_GET_HALT_COUNT());
    }
    POST_TEST_NAME(voltDeviceSetVoltage)();
}

UT_CASE_DEFINE(PMU_VOLTDEVICE, voltDeviceSetVoltageNegative,
  UT_CASE_SET_DESCRIPTION("Test voltDeviceSetVoltageNegative")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test that the Unit Volt Device Set Voltage interface handles all negative
 *             inputs correctly.
 *
 * @details    The test shall ilwoke the Unit Volt Device method that gets current
 *             programmed voltage in uV.
 *
 *             Parameters: void
 *
 *             The test passes if the expected result in the dataset matches with
 *             the actual result received after exelwting the test.
 *
 *             Any other return value, constitutes failure.
 */
UT_CASE_RUN(PMU_VOLTDEVICE, voltDeviceSetVoltageNegative)
{
    LwU8    numEntriesNegative =
        sizeof(voltDeviceSetVoltageNegative_inout) / sizeof(VOLT_DEVICE_SET_VOLTAGE_INPUT_OUTPUT);
    FLCN_STATUS     status;
    VOLT_DEVICE_PWM voltDevPWM;
    LwU8            i;

    PRE_TEST_NAME(voltDeviceSetVoltage)();
    for (i = 0; i < numEntriesNegative; i++)
    {
        memset(&voltDevPWM, 0, sizeof(VOLT_DEVICE_PWM));

        // Copy the data set.
        memcpy(&voltDevPWM, &voltDeviceSetVoltageNegative_inout[i].voltDevicePWM, sizeof(VOLT_DEVICE_PWM));

        // Run the function under test.
        status = voltDeviceSetVoltage_IMPL((VOLT_DEVICE *)&voltDevPWM, voltDeviceSetVoltageNegative_inout[i].targetVoltageuV, LW_FALSE, LW_FALSE, LW2080_CTRL_VOLT_VOLT_RAIL_ACTION_VF_SWITCH);

        // Verify result.
        UT_ASSERT_EQUAL_UINT(status, voltDeviceSetVoltageNegative_inout[i].expectedResult);
    }
    pwmSrcDescTrigger.super.type          = 0xFF;
    status = voltDeviceSetVoltage_IMPL((VOLT_DEVICE *)&voltDevPWM, voltDeviceSetVoltageNegative_inout[i].targetVoltageuV, LW_FALSE, LW_FALSE, LW2080_CTRL_VOLT_VOLT_RAIL_ACTION_VF_SWITCH);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_NOT_SUPPORTED);
    POST_TEST_NAME(voltDeviceSetVoltage)();
}

UT_CASE_DEFINE(PMU_VOLTDEVICE, voltDeviceRoundVoltagePositive,
  UT_CASE_SET_DESCRIPTION("Test voltDeviceRoundVoltagePositive")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test that the Unit Volt Device Round Voltage interface handles all positive
 *             inputs correctly.
 *
 * @details    The test shall ilwoke the Unit Volt Device method that gets current
 *             programmed voltage in uV.
 *
 *             Parameters: void
 *
 *             The test passes if the expected result in the dataset matches with
 *             the actual result received after exelwting the test.
 *
 *             Any other return value, constitutes failure.
 */
UT_CASE_RUN(PMU_VOLTDEVICE,voltDeviceRoundVoltagePositive)
{
    LwU8    numEntriesPositive =
        sizeof(voltDeviceRoundVoltagePositive_inout) / sizeof(VOLT_DEVICE_ROUND_VOLTAGE_INPUT_OUTPUT);
    FLCN_STATUS     status;
    VOLT_DEVICE     voltDev;
    LwU8            i;
    

    for (i = 0; i < numEntriesPositive; i++)
    {
        memset(&voltDev, 0, sizeof(VOLT_DEVICE));

        // Copy the data set.
        memcpy(&voltDev, &voltDeviceRoundVoltagePositive_inout[i].voltDevice, sizeof(VOLT_DEVICE));

        // Run the function under test.
        status = voltDeviceRoundVoltage_IMPL((VOLT_DEVICE *)&voltDev, voltDeviceRoundVoltagePositive_inout[i].pVoltageuV,
                        voltDeviceRoundVoltagePositive_inout[i].bRoundUp, voltDeviceRoundVoltagePositive_inout[i].bBound);
        utf_printf("Test exelwting %d", i);

        // Verify result.
        UT_ASSERT_EQUAL_UINT(status, voltDeviceRoundVoltagePositive_inout[i].expectedResult);
        UT_ASSERT_EQUAL_UINT(*(voltDeviceRoundVoltagePositive_inout[i].pVoltageuV), voltDeviceRoundVoltagePositive_inout[i].expectedRoundedVoltage);

        UT_ASSERT_EQUAL_UINT(0U, UTF_INTRINSICS_MOCK_GET_HALT_COUNT());
    }
}

UT_CASE_DEFINE(PMU_VOLTDEVICE, voltDevicesLoadPositive,
  UT_CASE_SET_DESCRIPTION("Test voltDevicesLoadPositive")
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
UT_CASE_RUN(PMU_VOLTDEVICE, voltDevicesLoadPositive)
{
    FLCN_STATUS    status;

    status = voltDevicesLoad();

    UT_ASSERT_EQUAL_UINT(status, FLCN_OK);
}

static LwU32 DelayTicks = 0;

static FLCN_STATUS
lwrtosLwrrentTaskDelayTicks_MOCK_Lwstom
(
    LwU32 ticksToDelay
)
{
    DelayTicks += ticksToDelay;
}

UT_CASE_DEFINE(PMU_VOLTDEVICE, voltDeviceSwitchDelayApplyPositive,
  UT_CASE_SET_DESCRIPTION("Test voltDeviceSwitchDelayApplyPositive")
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
UT_CASE_RUN(PMU_VOLTDEVICE, voltDeviceSwitchDelayApplyPositive)
{
    FLCN_STATUS    status;
    VOLT_DEVICE    voltDev;

    voltDev.switchDelayus = 128;
    LwU32 delayInTicks = LWRTOS_TIME_US_TO_TICKS(voltDev.switchDelayus);

    lwrtosMockInit();

    DelayTicks = 0;
    lwrtosLwrrentTaskDelayTicks_MOCK_fake.lwstom_fake =
        lwrtosLwrrentTaskDelayTicks_MOCK_Lwstom;

    voltDeviceSwitchDelayApply(&voltDev);

    UT_ASSERT_EQUAL_UINT(DelayTicks, delayInTicks);
}
