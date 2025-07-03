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
 * @file    thrmchannel-test.c
 * @brief   Unit tests for logic in thrmchannel.c
 */

/* ------------------------ System Includes --------------------------------- */
#include "test-macros.h"

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
 *          to interface thermChannelTempValueGet().
 */
typedef struct THERM_CHANNLE_TEMP_VALUE_GET_INPUT_OUTPUT
{
    // Input Parameters
    LwU8                            channelType;
    LwSFXP8_8                       scaling;

    // Mocked Parameters
    FLCN_STATUS                     thermDeviceTempValueGet_MOCK_Status;
    LwTemp                          thermDeviceTempValueGet_MOCK_Temp;

    // Expected Parameters
    LwTemp                          expectedTemp;
    FLCN_STATUS                     expectedStatus;
} THERM_CHANNEL_TEMP_VALUE_GET_INPUT_OUTPUT;

/* ------------------------ Global Data-------------------------------------- */
/* ------------------------ Local Data -------------------------------------- */
/*!
 * @brief   Table of negative test conditions for thermChannelTempValueGet.
 */
static THERM_CHANNEL_TEMP_VALUE_GET_INPUT_OUTPUT thermChannelTempValueGet_negativeInOut[] =
{
    {
        .channelType    = LW2080_CTRL_THERMAL_THERM_CHANNEL_CLASS_ILWALID,
        .thermDeviceTempValueGet_MOCK_Status = FLCN_OK,
        .expectedStatus                      = FLCN_ERR_NOT_SUPPORTED,
    },

    {
        .channelType = LW2080_CTRL_THERMAL_THERM_CHANNEL_CLASS_DEVICE,
        .thermDeviceTempValueGet_MOCK_Status = FLCN_ERROR,
        .expectedStatus                      = FLCN_ERROR,
    }
};

/*!
 * @brief   Table of positive test conditions for thermChannelTempValueGet.
 */
static THERM_CHANNEL_TEMP_VALUE_GET_INPUT_OUTPUT thermChannelTempValueGet_positiveInOut[] =
{
    // Scaling = 0
    {
        .channelType = LW2080_CTRL_THERMAL_THERM_CHANNEL_CLASS_DEVICE,
        .scaling     = 0,
        .thermDeviceTempValueGet_MOCK_Status = FLCN_OK,
        .thermDeviceTempValueGet_MOCK_Temp   = LW_TEMP_MIN_UT,
        .expectedStatus = FLCN_OK,
        .expectedTemp   = 0,
    },

    // Scaling = 1
    {
        .channelType = LW2080_CTRL_THERMAL_THERM_CHANNEL_CLASS_DEVICE,
        .scaling     = (0x100),
        .thermDeviceTempValueGet_MOCK_Status = FLCN_OK,
        .thermDeviceTempValueGet_MOCK_Temp   = LW_TEMP_MAX_UT,
        .expectedStatus = FLCN_OK,
        .expectedTemp   = LW_TEMP_MAX_UT,
    },

    {
        .channelType = LW2080_CTRL_THERMAL_THERM_CHANNEL_CLASS_DEVICE,
        .scaling     = (0x100),
        .thermDeviceTempValueGet_MOCK_Status = FLCN_OK,
        .thermDeviceTempValueGet_MOCK_Temp   = LW_TEMP_109_C,
        .expectedStatus = FLCN_OK,
        .expectedTemp   = LW_TEMP_109_C,
    },

    {
        .channelType = LW2080_CTRL_THERMAL_THERM_CHANNEL_CLASS_DEVICE,
        .scaling     = (0x100),
        .thermDeviceTempValueGet_MOCK_Status = FLCN_OK,
        .thermDeviceTempValueGet_MOCK_Temp   = LW_TEMP_111_C,
        .expectedStatus = FLCN_OK,
        .expectedTemp   = LW_TEMP_MAX_UT,
    },

    {
        .channelType = LW2080_CTRL_THERMAL_THERM_CHANNEL_CLASS_DEVICE,
        .scaling     = (0x100),
        .thermDeviceTempValueGet_MOCK_Status = FLCN_OK,
        .thermDeviceTempValueGet_MOCK_Temp   = LW_TEMP_MIN_UT,
        .expectedStatus = FLCN_OK,
        .expectedTemp   = LW_TEMP_MIN_UT,
    },

    {
        .channelType = LW2080_CTRL_THERMAL_THERM_CHANNEL_CLASS_DEVICE,
        .scaling     = (0x100),
        .thermDeviceTempValueGet_MOCK_Status = FLCN_OK,
        .thermDeviceTempValueGet_MOCK_Temp   = LW_TEMP_NEG_9_C,
        .expectedStatus = FLCN_OK,
        .expectedTemp   = LW_TEMP_NEG_9_C,
    },

    {
        .channelType = LW2080_CTRL_THERMAL_THERM_CHANNEL_CLASS_DEVICE,
        .scaling     = (0x100),
        .thermDeviceTempValueGet_MOCK_Status = FLCN_OK,
        .thermDeviceTempValueGet_MOCK_Temp   = LW_TEMP_NEG_11_C,
        .expectedStatus = FLCN_OK,
        .expectedTemp   = LW_TEMP_MIN_UT,
    },

    // Scaling = 1.00390625
    {
        .channelType = LW2080_CTRL_THERMAL_THERM_CHANNEL_CLASS_DEVICE,
        .scaling     = (0x101),
        .thermDeviceTempValueGet_MOCK_Status = FLCN_OK,
        .thermDeviceTempValueGet_MOCK_Temp   = LW_TEMP_109_5_C,
        .expectedStatus = FLCN_OK,
    },
};

/* ------------------------ Functions -------------------------------- */
UT_SUITE_DEFINE(PMU_THERM_THRMCHANNEL,
                UT_SUITE_SET_COMPONENT("Unit Therm")
                UT_SUITE_SET_DESCRIPTION("Unit tests for Therm Component group")
                UT_SUITE_SET_OWNER("sajjank"))

UT_CASE_DEFINE(PMU_THERM_THRMCHANNEL, thermChannelTempValueGetNegative,
  UT_CASE_SET_DESCRIPTION("Test thermChannelTempValueGetNegative")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Unit Component Therm Core Test handles all Positive inputs.
 *
 * @details    The test shall ilwoke the Unit Therm Component method to get channel temperature.
 *
 */
UT_CASE_RUN(PMU_THERM_THRMCHANNEL, thermChannelTempValueGetNegative)
{
    LwU8            numEntries;
    LwU8            i;
    FLCN_STATUS     status;
    LwTemp          temp;
    THERM_CHANNEL   channel;

    numEntries  = (sizeof (thermChannelTempValueGet_negativeInOut) /
                            sizeof (THERM_CHANNEL_TEMP_VALUE_GET_INPUT_OUTPUT));

    THERM_CHANNEL_TEMP_VALUE_GET_INPUT_OUTPUT *ptrTestCase = thermChannelTempValueGet_negativeInOut;
    THERM_DEVICE_TEMP_VALUE_GET_MOCK_CONFIG   *ptrMock     = &thermDeviceTempValueGet_MOCK_CONFIG;

    for (i = 0; i < numEntries; i++)
    {
        // Copy the Input data set.
        memset(&channel, 0, sizeof (THERM_CHANNEL));
        (((BOARDOBJ *)(&channel))->type) = ptrTestCase[i].channelType;

        // Copy the Mock data set.
        memset(&thermDeviceTempValueGet_MOCK_CONFIG, 0, sizeof (THERM_DEVICE_TEMP_VALUE_GET_MOCK_CONFIG));
        ptrMock->status = ptrTestCase[i].thermDeviceTempValueGet_MOCK_Status;

        // Run the function under test.
        status = thermChannelTempValueGet(&channel, &temp);

        // Verify result.
        UT_ASSERT_EQUAL_UINT(status, ptrTestCase[i].expectedStatus);
    }

}


UT_CASE_DEFINE(PMU_THERM_THRMCHANNEL, thermChannelTempValueGetPositive,
  UT_CASE_SET_DESCRIPTION("Test thermChannelTempValueGetPositive")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Unit Component Therm Core Test handles all Negative inputs.
 *
 * @details    The test shall ilwoke the Unit Therm Component method to get channel temperature.
 *
 */
UT_CASE_RUN(PMU_THERM_THRMCHANNEL, thermChannelTempValueGetPositive)
{
    LwU8            numEntries;
    LwU8            i;
    FLCN_STATUS     status;
    LwTemp          temp;
    THERM_CHANNEL   channel;

    numEntries  = (sizeof (thermChannelTempValueGet_positiveInOut) /
                            sizeof (THERM_CHANNEL_TEMP_VALUE_GET_INPUT_OUTPUT));

    THERM_CHANNEL_TEMP_VALUE_GET_INPUT_OUTPUT *ptrTestCase = thermChannelTempValueGet_positiveInOut;
    THERM_DEVICE_TEMP_VALUE_GET_MOCK_CONFIG   *ptrMock     = &thermDeviceTempValueGet_MOCK_CONFIG;

    for (i = 0; i < numEntries; i++)
    {
        // Copy the Input data set.
        memset(&channel, 0, sizeof (THERM_CHANNEL));
        (((BOARDOBJ *)(&channel))->type) = ptrTestCase[i].channelType;
        THERM_CHANNEL_DEVICE *pChDev = (THERM_CHANNEL_DEVICE *)&channel;
        pChDev->super.scaling        = ptrTestCase[i].scaling;
        pChDev->super.offsetSw       = 0;
        pChDev->super.lwTempMin      = LW_TEMP_MIN_UT;
        pChDev->super.lwTempMax      = LW_TEMP_MAX_UT;

        // Copy the Mock data set.
        memset(&thermDeviceTempValueGet_MOCK_CONFIG, 0, sizeof (THERM_DEVICE_TEMP_VALUE_GET_MOCK_CONFIG));
        ptrMock->status = ptrTestCase[i].thermDeviceTempValueGet_MOCK_Status;
        ptrMock->temp   = ptrTestCase[i].thermDeviceTempValueGet_MOCK_Temp;

        // Run the function under test.
        status = thermChannelTempValueGet(&channel, &temp);

        // Verify result.
        UT_ASSERT_EQUAL_UINT(status, ptrTestCase[i].expectedStatus);
        UT_ASSERT_EQUAL_UINT(0U, UTF_INTRINSICS_MOCK_GET_HALT_COUNT());
        if (pChDev->super.scaling & 0xFF)
        {
            // Scale factor is in fraction, so, cannot just compare fraction values.
            LwS32 tempRounded = LW_TYPES_SFXP_X_Y_TO_S32_ROUNDED(24, 8, temp);
            LwS32 calcTemp = LW_TYPES_SFXP_X_Y_TO_S32_ROUNDED(16, 16, ptrMock->temp * pChDev->super.scaling + pChDev->super.offsetSw);

            UT_ASSERT_IN_RANGE_INT(tempRounded,
                                LW_MAX(calcTemp - 1, LW_TYPES_SFXP_X_Y_TO_S32_ROUNDED(24, 8, pChDev->super.lwTempMin)),
                                LW_MIN(calcTemp + 1, LW_TYPES_SFXP_X_Y_TO_S32_ROUNDED(24, 8, pChDev->super.lwTempMax)));
        }
        else
        {
            // Scale factor is integer.
            UT_ASSERT_EQUAL_UINT(temp, ptrTestCase[i].expectedTemp);
        }
    }
}
