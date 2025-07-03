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
 * @file    thrmdevice-test.c
 * @brief   Unit tests for logic in thrmdevice.c
 */

/* ------------------------ System Includes --------------------------------- */
#include "test-macros.h"
#include "dev_pwr_csb.h"

/* ------------------------ Application Includes ---------------------------- */
#include "lwstatus.h"
#include "therm/thrmchannel.h"
#include "therm/thrmchannel-test.h"
#include "therm/thrm-mock.h"

/* ------------------------ Defines ----------------------------------------- */
#define LW_TEMP_MIN_UT      (LW_TEMP_NEG_10_C)  // -10 C
#define LW_TEMP_MAX_UT      (LW_TEMP_110_C)     // 110 C

/* ------------------------ Mocked Functions -------------------------------- */
/* ------------------------ Type Definitions -------------------------------- */
/* ------------------------ Structures -------------------------------------- */
/*!
 * @brief   Structure describing a set of inputs and the set of expected results
 *          to interface thermDeviceTempValueGet().
 */
typedef struct THERM_DEVICE_TEMP_VALUE_GET_INPUT_OUTPUT
{
    // Input Parameters
    LwU8                            deviceType;
    LwU8                            thermDevProvIdx;

    // Expected Parameters
    LwTemp                          expectedTemp;
    FLCN_STATUS                     expectedStatus;
} THERM_DEVICE_TEMP_VALUE_GET_INPUT_OUTPUT;

/* ------------------------ Global Data-------------------------------------- */
/* ------------------------ Local Data -------------------------------------- */
/*!
 * @brief   Table of negative test conditions for thermDeviceTempValueGet.
 */
static THERM_DEVICE_TEMP_VALUE_GET_INPUT_OUTPUT thermDeviceTempValueGet_negativeInOut[] =
{
    {
        .deviceType     = LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_GPU_GPC_TSOSC,
        .expectedStatus = FLCN_ERR_NOT_SUPPORTED,
    },

    {
        .deviceType     = LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_GPU_SCI,
        .expectedStatus = FLCN_ERR_NOT_SUPPORTED,
    },

    {
        .deviceType     = LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_GPU_GPC_COMBINED,
        .expectedStatus = FLCN_ERR_NOT_SUPPORTED,
    },

    {
        .deviceType     = LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_GDDR6_X_COMBINED,
        .expectedStatus = FLCN_ERR_NOT_SUPPORTED,
    },

    {
        .deviceType     = LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_I2C_TMP411,
        .expectedStatus = FLCN_ERR_NOT_SUPPORTED,
    },

    {
        .deviceType     = LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_I2C_ADM1032,
        .expectedStatus = FLCN_ERR_NOT_SUPPORTED,
    },

    {
        .deviceType     = LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_I2C_ADT7461,
        .expectedStatus = FLCN_ERR_NOT_SUPPORTED,
    },

    {
        .deviceType     = LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_I2C_MAX6649,
        .expectedStatus = FLCN_ERR_NOT_SUPPORTED,
    },

    {
        .deviceType     = LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_I2C_TMP451,
        .expectedStatus = FLCN_ERR_NOT_SUPPORTED,
    },

    {
        .deviceType     = LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_HBM2_SITE,
        .expectedStatus = FLCN_ERR_NOT_SUPPORTED,
    },

    {
        .deviceType     = LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_HBM2_COMBINED,
        .expectedStatus = FLCN_ERR_NOT_SUPPORTED,
    },

    {
        .deviceType     = LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_ILWALID,
        .expectedStatus = FLCN_ERR_NOT_SUPPORTED,
    },

    // TODO To enable once interface prototype is changed.
#if 0
    {
        .deviceType      = LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_GPU,
        .thermDevProvIdx = LW_CPWR_THERM_TEMP_SENSOR_ID_COUNT, 
        .expectedStatus  = FLCN_ERR_NOT_SUPPORTED,
    },
#endif
};

/*!
 * @brief   Table of positive test conditions for thermDeviceTempValueGet.
 */
static THERM_DEVICE_TEMP_VALUE_GET_INPUT_OUTPUT thermDeviceTempValueGet_positiveInOut[] =
{
    {
        .deviceType      = LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_GPU,
        .thermDevProvIdx = LW_CPWR_THERM_TEMP_SENSOR_ID_GPC_TSOSC_AVG,
        .expectedStatus  = FLCN_OK,
        .expectedTemp    = 0,
    },

    {
        .deviceType     = LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_GPU,
        .thermDevProvIdx = LW_CPWR_THERM_TEMP_SENSOR_ID_GPC_TSOSC_MAX,
        .expectedStatus = FLCN_OK,
        .expectedTemp   = 0,
    },

};

/* ------------------------ Functions -------------------------------- */
UT_SUITE_DEFINE(PMU_THERM_THRMDEVICE,
                UT_SUITE_SET_COMPONENT("Unit Therm")
                UT_SUITE_SET_DESCRIPTION("Unit tests for Therm Component group")
                UT_SUITE_SET_OWNER("sajjank"))

UT_CASE_DEFINE(PMU_THERM_THRMDEVICE, thermDeviceTempValueGetNegative,
  UT_CASE_SET_DESCRIPTION("Test thermDeviceTempValueGetNegative")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Unit Component Therm Core Test handles all Positive inputs.
 *
 * @details    The test shall ilwoke the Unit Therm Component method to get device temperature.
 *
 */
UT_CASE_RUN(PMU_THERM_THRMDEVICE, thermDeviceTempValueGetNegative)
{
    LwU8            numEntries;
    LwU8            i;
    FLCN_STATUS     status;
    LwU8            provIdx;
    LwTemp          temp;
    THERM_DEVICE    device;


    numEntries  = (sizeof (thermDeviceTempValueGet_negativeInOut) /
                            sizeof (THERM_DEVICE_TEMP_VALUE_GET_INPUT_OUTPUT));

    THERM_DEVICE_TEMP_VALUE_GET_INPUT_OUTPUT *ptrTestCase =
                                        thermDeviceTempValueGet_negativeInOut;
    THERM_DEVICE_TEMP_VALUE_GET_MOCK_CONFIG  *ptrMock     =
                                        &thermDeviceTempValueGet_MOCK_CONFIG;

    for (i = 0; i < numEntries; i++)
    {
        //
        // Initialize the stack variables.
        // Copy the Input data set.
        //

        memset(&device, 0, sizeof (THERM_DEVICE));

        // Populate Therm Device Type.
        (((BOARDOBJ *)(&device))->type) = ptrTestCase[i].deviceType;

        // Populate Therm Provider Index.
        provIdx = ptrTestCase[i].thermDevProvIdx;

        //
        // Skip mocking as its not required.
        // Execute the Function Under Test.
        // Verify the results.
        //

        // Run the function under test.
        status = thermDeviceTempValueGet_IMPL(&device, provIdx, &temp);

        // Verify result.
        UT_ASSERT_EQUAL_UINT(status, ptrTestCase[i].expectedStatus);
    }

}


UT_CASE_DEFINE(PMU_THERM_THRMDEVICE, thermDeviceTempValueGetPositive,
  UT_CASE_SET_DESCRIPTION("Test thermDeviceTempValueGetPositive")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Unit Component Therm Core Test handles all Negative inputs.
 *
 * @details    The test shall ilwoke the Unit Therm Component method to get device temperature.
 *
 */
UT_CASE_RUN(PMU_THERM_THRMDEVICE, thermDeviceTempValueGetPositive)
{
    LwU8            numEntries;
    LwU8            i;
    FLCN_STATUS     status;
    LwU8            provIdx;
    LwTemp          temp;
    THERM_CHANNEL   device;

    numEntries  = (sizeof (thermDeviceTempValueGet_positiveInOut) /
                            sizeof (THERM_DEVICE_TEMP_VALUE_GET_INPUT_OUTPUT));

    THERM_DEVICE_TEMP_VALUE_GET_INPUT_OUTPUT *ptrTestCase =
                                        thermDeviceTempValueGet_positiveInOut;
    THERM_DEVICE_TEMP_VALUE_GET_MOCK_CONFIG   *ptrMock    =
                                        &thermDeviceTempValueGet_MOCK_CONFIG;

    for (i = 0; i < numEntries; i++)
    {
        //
        // Initialize the stack variables.
        // Copy the Input data set.
        //

        memset(&device, 0, sizeof (THERM_DEVICE));

        // Populate Therm Device Type.
        (((BOARDOBJ *)(&device))->type) = ptrTestCase[i].deviceType;

        // Populate Therm Provider Index.
        provIdx = ptrTestCase[i].thermDevProvIdx;

        //
        // Skip mocking as its not required.
        // Execute the Function Under Test.
        // Verify the results.
        //

        // Run the function under test.
        status = thermDeviceTempValueGet_IMPL(&device, provIdx, &temp);

        // Verify result.
        UT_ASSERT_EQUAL_UINT(status, ptrTestCase[i].expectedStatus);
        UT_ASSERT_EQUAL_UINT(0U, UTF_INTRINSICS_MOCK_GET_HALT_COUNT());

        LwS32 lwrrTemp;   
        switch (ptrTestCase[i].thermDevProvIdx)
        {
            case LW_CPWR_THERM_TEMP_SENSOR_ID_GPC_TSOSC_MAX:
            {
                lwrrTemp = DRF_VAL_SIGNED(_CPWR_THERM, _TEMP_SENSOR, _INTEGER,
                        REG_RD32(CSB, LW_CPWR_THERM_TEMP_SENSOR_GPC_TSOSC_MAX));
                break;
            }

            case LW_CPWR_THERM_TEMP_SENSOR_ID_GPC_TSOSC_AVG:
            {
                lwrrTemp = DRF_VAL_SIGNED(_CPWR_THERM, _TEMP_SENSOR, _INTEGER,
                        REG_RD32(CSB, LW_CPWR_THERM_TEMP_SENSOR_GPC_TSOSC_AVG));
                break;
            }

            default:
            {
                lwrrTemp = 0;
                break;
            }
        }

        UT_ASSERT_IN_RANGE_INT(RM_PMU_LW_TEMP_TO_CELSIUS_TRUNCED(temp),
                        lwrrTemp + 5, lwrrTemp - 5);
    }
}
