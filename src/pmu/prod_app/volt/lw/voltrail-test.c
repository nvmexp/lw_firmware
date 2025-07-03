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
 * @file    voltrail-test.c
 * @brief   Unit tests for logic in VOLT_RAIL.
 */

/* ------------------------ System Includes --------------------------------- */
#include "test-macros.h"

/* ------------------------ Application Includes ---------------------------- */
#include "lwstatus.h"
#include "volt/objvolt.h"
#include "volt/objvolt-test.h"
#include "volt/voltrail.h"
#include "volt/voltrail-mock.h"
#include "volt/voltdev-mock.h"
#include "perf/3x/vfe_equ-mock.h"

/* ------------------------ Defines ----------------------------------------- */
/* ------------------------ Mocked Functions -------------------------------- */
/*!
/* ------------------------ Type Definitions -------------------------------- */
/*!
 * @brief   Structure describing a set of inputs to get voltage max interface
 *          and the set of expected results.
 */
typedef struct VOLT_RAIL_GET_VOLTAGE_MAX_INPUT_OUTPUT
{
    VOLT_RAIL       voltRail;
    LwU32           expectedMaxLimituV;
    FLCN_STATUS     expectedResult;
}VOLT_RAIL_GET_VOLTAGE_MAX_INPUT_OUTPUT;

/*!
 * @brief   Structure describing a set of inputs to get voltage internal interface
 *          and the set of expected results.
 */
typedef struct VOLT_RAIL_GET_VOLTAGE_INTERNAL_INPUT_OUTPUT
{
    VOLT_RAIL       voltRail;
    LwU32           expectedLwrrVoltDefaultuV;
    FLCN_STATUS     expectedResult;
}VOLT_RAIL_GET_VOLTAGE_INTERNAL_INPUT_OUTPUT;

/*!
 * @brief   Structure describing a set of inputs to voltage load interface
 *          and the set of expected results.
 */
typedef struct VOLT_RAILS_LOAD_INPUT_OUTPUT
{
    FLCN_STATUS     voltDeviceGetVoltageMockStatus;
    FLCN_STATUS     voltDeviceRoundVoltageMockStatus;
    FLCN_STATUS     expectedResult;
}VOLT_RAILS_LOAD_INPUT_OUTPUT;

/*!
 * @brief   Structure describing a set of inputs to voltage dynamic update interface
 *          and the set of expected results.
 */
typedef struct VOLT_RAILS_DYNAMIC_UPDATE_INPUT_OUTPUT
{
    FLCN_STATUS     voltRailDynamilwpdateMockStatus;
    FLCN_STATUS     expectedResult;
}VOLT_RAILS_DYNAMIC_UPDATE_INPUT_OUTPUT;

/*!
 * @brief   Structure describing a set of inputs to get voltage interface
 *          and the set of expected results.
 */
typedef struct VOLT_RAIL_GET_VOLTAGE_INPUT_OUTPUT
{
    VOLT_RAIL       voltRail;
    LwU32           expectedLwrrVoltuV;
    FLCN_STATUS     expectedResult;
}VOLT_RAIL_GET_VOLTAGE_INPUT_OUTPUT;

/*!
 * @brief   Structure describing a set of inputs to round voltage interface
 *          and the set of expected results.
 */
typedef struct VOLT_RAIL_ROUND_VOLTAGE_INPUT_OUTPUT
{
    LwS32           mockedRoundedVoltuV;
    FLCN_STATUS     mockedVoltDeviceRoundVoltageStatus;
    FLCN_STATUS     expectedResult;
}VOLT_RAIL_ROUND_VOLTAGE_INPUT_OUTPUT;

/*!
 * @brief   Structure describing a set of inputs to voltage rail dynamic update
 *          interface and the set of expected results.
 */
typedef struct VOLT_RAIL_DYNAMIC_UPDATE_INPUT_OUTPUT
{
    VOLT_RAIL       voltRail;
    LwBool          bVfeEvaluate;
    LwU32           voltuVResult;
    LwS32           roundedVoltuV;
    LwU32           expectedRelLimituV;
    LwU32           expectedAltRelLimituV;
    LwU32           expectedOvLimituV;
    LwU32           expectedMaxLimituV;
    LwU32           expectedVminLimituV;
    LwU32           expectedVoltMarginLimituV;
    FLCN_STATUS     vfeEquEvaluateStatus;
    FLCN_STATUS     voltRailRoundVoltageStatus;
    FLCN_STATUS     expectedStatus;
}VOLT_RAIL_DYNAMIC_UPDATE_INPUT_OUTPUT;

/*!
 * @brief   Structure describing a set of inputs to set voltage interface
 *          and the set of expected results.
 */
typedef struct VOLT_RAIL_SET_VOLTAGE_INPUT_OUTPUT
{
    LwU32           reqLwrrVoltuV;
    LwU32           mockedRoundedVoltuV;
    FLCN_STATUS     mockedVoltDeviceRoundVoltageStatus;
    FLCN_STATUS     mockedVoltDeviceSetVoltageStatus;
    FLCN_STATUS     expectedResult;
}VOLT_RAIL_SET_VOLTAGE_INPUT_OUTPUT;

/* ------------------------ Global Data-------------------------------------- */
static VOLT_RAIL   railObj1     = { 0U };
static BOARDOBJ   *railObjects1[1];

static VOLT_DEVICE deviceObj1     = { 0U };
static BOARDOBJ   *deviceObjects1[1];
/* ------------------------ Local Data-------------------------------------- */
/*!
 * @brief   Table of positive test conditions for voltRailGetVoltageMax.
 */
static  VOLT_RAIL_GET_VOLTAGE_MAX_INPUT_OUTPUT voltRailGetVoltageMaxPositive_inout[] =
{
    {
        .voltRail =
        {
            .maxLimituV = 1125000,
        },
        .expectedMaxLimituV = 1125000,
        .expectedResult     = FLCN_OK,
    },
};

/*!
 * @brief   Table of positive test conditions for voltRailGetVoltageInternal.
 */
static  VOLT_RAIL_GET_VOLTAGE_INTERNAL_INPUT_OUTPUT voltRailGetVoltageInternalPositive_inout[] =
{
    {
        .voltRail =
        {
            .lwrrVoltDefaultuV = 625000,
        },
        .expectedLwrrVoltDefaultuV = 625000,
        .expectedResult     = FLCN_OK,
    },
};

/*!
 * @brief   Table of positive test conditions for voltRailGetVoltageMax.
 */
static  VOLT_RAILS_LOAD_INPUT_OUTPUT voltRailsLoadPositive_inout[] =
{
    {
        .voltDeviceGetVoltageMockStatus     = FLCN_OK,
        .voltDeviceRoundVoltageMockStatus   = FLCN_OK,
        .expectedResult                     = FLCN_OK,
    },
};

static  VOLT_RAILS_LOAD_INPUT_OUTPUT voltRailsLoadNegative_inout[] =
{
    {
        .voltDeviceGetVoltageMockStatus     = FLCN_ERR_NOT_SUPPORTED,
        .voltDeviceRoundVoltageMockStatus   = FLCN_OK,
        .expectedResult                     = FLCN_ERR_NOT_SUPPORTED,
    },
    {
        .voltDeviceGetVoltageMockStatus     = FLCN_OK,
        .voltDeviceRoundVoltageMockStatus   = FLCN_ERR_ILWALID_ARGUMENT,
        .expectedResult                     = FLCN_ERR_ILWALID_ARGUMENT,
    },
};

/*!
 * @brief   Table of positive test conditions for voltRailGetVoltageMax.
 */
static  VOLT_RAILS_DYNAMIC_UPDATE_INPUT_OUTPUT voltRailsDynamilwpdatePositive_inout[] =
{
    {
        .voltRailDynamilwpdateMockStatus     = FLCN_OK,
        .expectedResult                      = FLCN_OK,
    },
};

static  VOLT_RAILS_DYNAMIC_UPDATE_INPUT_OUTPUT voltRailsDynamilwpdateNegative_inout[] =
{
    {
        .voltRailDynamilwpdateMockStatus     = FLCN_ERR_ILWALID_ARGUMENT,
        .expectedResult                      = FLCN_ERR_ILWALID_ARGUMENT,
    },
};

/*!
 * @brief   Table of positive test conditions for voltRailGetVoltage.
 */
static  VOLT_RAIL_GET_VOLTAGE_INPUT_OUTPUT voltRailGetVoltagePositive_inout[] =
{
    {
        .voltRail =
        {
            .lwrrVoltDefaultuV = 625000,
        },
        .expectedLwrrVoltuV = 625000,
        .expectedResult     = FLCN_OK,
    },
};

/*!
 * @brief   Table of positive test conditions for voltRailGetVoltage.
 */
static  VOLT_RAIL_ROUND_VOLTAGE_INPUT_OUTPUT voltRailRoundVoltagePositive_inout[] =
{
    {
        .mockedRoundedVoltuV                = 625000,
        .mockedVoltDeviceRoundVoltageStatus = FLCN_OK,
        .expectedResult                     = FLCN_OK,
    },
};

/*!
 * @brief   Table of positive test conditions for voltRailRoundVoltage.
 */
static  VOLT_RAIL_ROUND_VOLTAGE_INPUT_OUTPUT voltRailRoundVoltageNegative_inout[] =
{
    {
        .mockedRoundedVoltuV                = 625000,
        .mockedVoltDeviceRoundVoltageStatus = FLCN_ERR_ILWALID_ARGUMENT,
        .expectedResult                     = FLCN_ERR_ILWALID_ARGUMENT,
    },
};

/*!
 * @brief   Table of positive test conditions for voltRailRoundVoltage.
 */
static  VOLT_RAIL_DYNAMIC_UPDATE_INPUT_OUTPUT voltRailDynamilwpdatePositive_inout[] =
{
    {
        .voltRail = 
        {
            .relLimitVfeEquIdx          = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .altRelLimitVfeEquIdx       = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .ovLimitVfeEquIdx           = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .vminLimitVfeEquIdx         = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .voltMarginLimitVfeEquIdx   = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .voltDeltauV =
                {
                    125000,
                    0,
                    0,
                    0,
                },
        },
        .bVfeEvaluate                   = LW_FALSE,
        .voltuVResult                   = 625000,
        .roundedVoltuV                  = 750000,
        .expectedRelLimituV             = 0,
        .expectedAltRelLimituV          = 0,
        .expectedOvLimituV              = 0,
        .expectedMaxLimituV             = LW_U32_MAX,
        .expectedVminLimituV            = 0,
        .expectedVoltMarginLimituV      = 0,
        .vfeEquEvaluateStatus           = FLCN_OK,
        .voltRailRoundVoltageStatus     = FLCN_OK,
        .expectedStatus                 = FLCN_OK,
    },
    {
        .voltRail = 
        {
            .relLimitVfeEquIdx          = 0,
            .altRelLimitVfeEquIdx       = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .ovLimitVfeEquIdx           = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .vminLimitVfeEquIdx         = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .voltMarginLimitVfeEquIdx   = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .voltDeltauV =
                {
                    125000,
                    0,
                    0,
                    0,
                },
        },
        .bVfeEvaluate                   = LW_TRUE,
        .voltuVResult                   = 625000,
        .roundedVoltuV                  = 750000,
        .expectedRelLimituV             = 750000,
        .expectedAltRelLimituV          = 0,
        .expectedOvLimituV              = 0,
        .expectedMaxLimituV             = 750000,
        .expectedVminLimituV            = 0,
        .expectedVoltMarginLimituV      = 0,
        .vfeEquEvaluateStatus           = FLCN_OK,
        .voltRailRoundVoltageStatus     = FLCN_OK,
        .expectedStatus                 = FLCN_OK,
    },
    {
        .voltRail = 
        {
            .relLimitVfeEquIdx          = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .altRelLimitVfeEquIdx       = 0,
            .ovLimitVfeEquIdx           = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .vminLimitVfeEquIdx         = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .voltMarginLimitVfeEquIdx   = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .voltDeltauV =
                {
                    0,
                    125000,
                    0,
                    0,
                },
        },
        .bVfeEvaluate                   = LW_TRUE,
        .voltuVResult                   = 625000,
        .roundedVoltuV                  = 750000,
        .expectedRelLimituV             = 0,
        .expectedAltRelLimituV          = 750000,
        .expectedOvLimituV              = 0,
        .expectedMaxLimituV             = 750000,
        .expectedVminLimituV            = 0,
        .expectedVoltMarginLimituV      = 0,
        .vfeEquEvaluateStatus           = FLCN_OK,
        .voltRailRoundVoltageStatus     = FLCN_OK,
        .expectedStatus                 = FLCN_OK,
    },
    {
        .voltRail = 
        {
            .relLimitVfeEquIdx          = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .altRelLimitVfeEquIdx       = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .ovLimitVfeEquIdx           = 0,
            .vminLimitVfeEquIdx         = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .voltMarginLimitVfeEquIdx   = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .voltDeltauV =
                {
                    0,
                    0,
                    125000,
                    0,
                },
        },
        .bVfeEvaluate                   = LW_TRUE,
        .voltuVResult                   = 625000,
        .roundedVoltuV                  = 750000,
        .expectedRelLimituV             = 0,
        .expectedAltRelLimituV          = 0,
        .expectedOvLimituV              = 750000,
        .expectedMaxLimituV             = 750000,
        .expectedVminLimituV            = 0,
        .expectedVoltMarginLimituV      = 0,
        .vfeEquEvaluateStatus           = FLCN_OK,
        .voltRailRoundVoltageStatus     = FLCN_OK,
        .expectedStatus                 = FLCN_OK,
    },
    {
        .voltRail = 
        {
            .relLimitVfeEquIdx          = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .altRelLimitVfeEquIdx       = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .ovLimitVfeEquIdx           = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .vminLimitVfeEquIdx         = 0,
            .voltMarginLimitVfeEquIdx   = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .voltDeltauV =
                {
                    0,
                    0,
                    0,
                    125000,
                },
        },
        .bVfeEvaluate                   = LW_TRUE,
        .voltuVResult                   = 625000,
        .roundedVoltuV                  = 750000,
        .expectedRelLimituV             = 0,
        .expectedAltRelLimituV          = 0,
        .expectedOvLimituV              = 0,
        .expectedMaxLimituV             = LW_U32_MAX,
        .expectedVminLimituV            = 750000,
        .expectedVoltMarginLimituV      = 0,
        .vfeEquEvaluateStatus           = FLCN_OK,
        .voltRailRoundVoltageStatus     = FLCN_OK,
        .expectedStatus                 = FLCN_OK,
    },
    {
        .voltRail = 
        {
            .relLimitVfeEquIdx          = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .altRelLimitVfeEquIdx       = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .ovLimitVfeEquIdx           = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .vminLimitVfeEquIdx         = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .voltMarginLimitVfeEquIdx   = 0,
            .voltDeltauV =
                {
                    0,
                    0,
                    0,
                    0,
                },
        },
        .bVfeEvaluate                   = LW_TRUE,
        .voltuVResult                   = 625000,
        .roundedVoltuV                  = 625000,
        .expectedRelLimituV             = 0,
        .expectedAltRelLimituV          = 0,
        .expectedOvLimituV              = 0,
        .expectedMaxLimituV             = LW_U32_MAX,
        .expectedVminLimituV            = 0,
        .expectedVoltMarginLimituV      = 625000,
        .vfeEquEvaluateStatus           = FLCN_OK,
        .voltRailRoundVoltageStatus     = FLCN_OK,
        .expectedStatus                 = FLCN_OK,
    },
};

/*!
 * @brief   Table of negative test conditions for voltRailDynamilwpdate.
 */
static  VOLT_RAIL_DYNAMIC_UPDATE_INPUT_OUTPUT voltRailDynamilwpdateNegative_inout[] =
{
    {
        .voltRail = 
        {
            .relLimitVfeEquIdx          = 0,
            .altRelLimitVfeEquIdx       = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .ovLimitVfeEquIdx           = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .vminLimitVfeEquIdx         = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .voltMarginLimitVfeEquIdx   = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .voltDeltauV =
                {
                    125000,
                    0,
                    0,
                    0,
                },
        },
        .bVfeEvaluate                   = LW_TRUE,
        .voltuVResult                   = 625000,
        .roundedVoltuV                  = 750000,
        .expectedRelLimituV             = 750000,
        .expectedAltRelLimituV          = 0,
        .expectedOvLimituV              = 0,
        .expectedMaxLimituV             = 750000,
        .expectedVminLimituV            = 0,
        .expectedVoltMarginLimituV      = 0,
        .vfeEquEvaluateStatus           = FLCN_ERR_ILWALID_STATE,
        .voltRailRoundVoltageStatus     = FLCN_OK,
        .expectedStatus                 = FLCN_ERR_ILWALID_STATE,
    },
    {
        .voltRail = 
        {
            .relLimitVfeEquIdx          = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .altRelLimitVfeEquIdx       = 0,
            .ovLimitVfeEquIdx           = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .vminLimitVfeEquIdx         = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .voltMarginLimitVfeEquIdx   = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .voltDeltauV =
                {
                    0,
                    125000,
                    0,
                    0,
                },
        },
        .bVfeEvaluate                   = LW_TRUE,
        .voltuVResult                   = 625000,
        .roundedVoltuV                  = 750000,
        .expectedRelLimituV             = 0,
        .expectedAltRelLimituV          = 750000,
        .expectedOvLimituV              = 0,
        .expectedMaxLimituV             = 750000,
        .expectedVminLimituV            = 0,
        .expectedVoltMarginLimituV      = 0,
        .vfeEquEvaluateStatus           = FLCN_ERR_ILWALID_STATE,
        .voltRailRoundVoltageStatus     = FLCN_OK,
        .expectedStatus                 = FLCN_ERR_ILWALID_STATE,
    },
    {
        .voltRail = 
        {
            .relLimitVfeEquIdx          = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .altRelLimitVfeEquIdx       = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .ovLimitVfeEquIdx           = 0,
            .vminLimitVfeEquIdx         = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .voltMarginLimitVfeEquIdx   = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .voltDeltauV =
                {
                    0,
                    0,
                    125000,
                    0,
                },
        },
        .bVfeEvaluate                   = LW_TRUE,
        .voltuVResult                   = 625000,
        .roundedVoltuV                  = 750000,
        .expectedRelLimituV             = 0,
        .expectedAltRelLimituV          = 0,
        .expectedOvLimituV              = 750000,
        .expectedMaxLimituV             = 750000,
        .expectedVminLimituV            = 0,
        .expectedVoltMarginLimituV      = 0,
        .vfeEquEvaluateStatus           = FLCN_ERR_ILWALID_STATE,
        .voltRailRoundVoltageStatus     = FLCN_OK,
        .expectedStatus                 = FLCN_ERR_ILWALID_STATE,
    },
    {
        .voltRail = 
        {
            .relLimitVfeEquIdx          = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .altRelLimitVfeEquIdx       = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .ovLimitVfeEquIdx           = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .vminLimitVfeEquIdx         = 0,
            .voltMarginLimitVfeEquIdx   = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .voltDeltauV =
                {
                    0,
                    0,
                    0,
                    125000,
                },
        },
        .bVfeEvaluate                   = LW_TRUE,
        .voltuVResult                   = 625000,
        .roundedVoltuV                  = 750000,
        .expectedRelLimituV             = 0,
        .expectedAltRelLimituV          = 0,
        .expectedOvLimituV              = 0,
        .expectedMaxLimituV             = LW_U32_MAX,
        .expectedVminLimituV            = 750000,
        .expectedVoltMarginLimituV      = 0,
        .vfeEquEvaluateStatus           = FLCN_ERR_ILWALID_STATE,
        .voltRailRoundVoltageStatus     = FLCN_OK,
        .expectedStatus                 = FLCN_ERR_ILWALID_STATE,
    },
    {
        .voltRail = 
        {
            .relLimitVfeEquIdx          = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .altRelLimitVfeEquIdx       = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .ovLimitVfeEquIdx           = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .vminLimitVfeEquIdx         = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .voltMarginLimitVfeEquIdx   = 0,
            .voltDeltauV =
                {
                    0,
                    0,
                    0,
                    0,
                },
        },
        .bVfeEvaluate                   = LW_TRUE,
        .voltuVResult                   = 625000,
        .roundedVoltuV                  = 625000,
        .expectedRelLimituV             = 0,
        .expectedAltRelLimituV          = 0,
        .expectedOvLimituV              = 0,
        .expectedMaxLimituV             = LW_U32_MAX,
        .expectedVminLimituV            = 0,
        .expectedVoltMarginLimituV      = 625000,
        .vfeEquEvaluateStatus           = FLCN_ERR_ILWALID_STATE,
        .voltRailRoundVoltageStatus     = FLCN_OK,
        .expectedStatus                 = FLCN_ERR_ILWALID_STATE,
    },
    {
        .voltRail = 
        {
            .relLimitVfeEquIdx          = 0,
            .altRelLimitVfeEquIdx       = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .ovLimitVfeEquIdx           = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .vminLimitVfeEquIdx         = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .voltMarginLimitVfeEquIdx   = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .voltDeltauV =
                {
                    125000,
                    0,
                    0,
                    0,
                },
        },
        .bVfeEvaluate                   = LW_TRUE,
        .voltuVResult                   = 625000,
        .roundedVoltuV                  = 750000,
        .expectedRelLimituV             = 750000,
        .expectedAltRelLimituV          = 0,
        .expectedOvLimituV              = 0,
        .expectedMaxLimituV             = 750000,
        .expectedVminLimituV            = 0,
        .expectedVoltMarginLimituV      = 0,
        .vfeEquEvaluateStatus           = FLCN_OK,
        .voltRailRoundVoltageStatus     = FLCN_ERR_ILWALID_INDEX,
        .expectedStatus                 = FLCN_ERR_ILWALID_INDEX,
    },
    {
        .voltRail = 
        {
            .relLimitVfeEquIdx          = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .altRelLimitVfeEquIdx       = 0,
            .ovLimitVfeEquIdx           = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .vminLimitVfeEquIdx         = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .voltMarginLimitVfeEquIdx   = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .voltDeltauV =
                {
                    0,
                    125000,
                    0,
                    0,
                },
        },
        .bVfeEvaluate                   = LW_TRUE,
        .voltuVResult                   = 625000,
        .roundedVoltuV                  = 750000,
        .expectedRelLimituV             = 0,
        .expectedAltRelLimituV          = 750000,
        .expectedOvLimituV              = 0,
        .expectedMaxLimituV             = 750000,
        .expectedVminLimituV            = 0,
        .expectedVoltMarginLimituV      = 0,
        .vfeEquEvaluateStatus           = FLCN_OK,
        .voltRailRoundVoltageStatus     = FLCN_ERR_ILWALID_INDEX,
        .expectedStatus                 = FLCN_ERR_ILWALID_INDEX,
    },
    {
        .voltRail = 
        {
            .relLimitVfeEquIdx          = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .altRelLimitVfeEquIdx       = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .ovLimitVfeEquIdx           = 0,
            .vminLimitVfeEquIdx         = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .voltMarginLimitVfeEquIdx   = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .voltDeltauV =
                {
                    0,
                    0,
                    125000,
                    0,
                },
        },
        .bVfeEvaluate                   = LW_TRUE,
        .voltuVResult                   = 625000,
        .roundedVoltuV                  = 750000,
        .expectedRelLimituV             = 0,
        .expectedAltRelLimituV          = 0,
        .expectedOvLimituV              = 750000,
        .expectedMaxLimituV             = 750000,
        .expectedVminLimituV            = 0,
        .expectedVoltMarginLimituV      = 0,
        .vfeEquEvaluateStatus           = FLCN_OK,
        .voltRailRoundVoltageStatus     = FLCN_ERR_ILWALID_INDEX,
        .expectedStatus                 = FLCN_ERR_ILWALID_INDEX,
    },
    {
        .voltRail = 
        {
            .relLimitVfeEquIdx          = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .altRelLimitVfeEquIdx       = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .ovLimitVfeEquIdx           = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .vminLimitVfeEquIdx         = 0,
            .voltMarginLimitVfeEquIdx   = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .voltDeltauV =
                {
                    0,
                    0,
                    0,
                    125000,
                },
        },
        .bVfeEvaluate                   = LW_TRUE,
        .voltuVResult                   = 625000,
        .roundedVoltuV                  = 750000,
        .expectedRelLimituV             = 0,
        .expectedAltRelLimituV          = 0,
        .expectedOvLimituV              = 0,
        .expectedMaxLimituV             = LW_U32_MAX,
        .expectedVminLimituV            = 750000,
        .expectedVoltMarginLimituV      = 0,
        .vfeEquEvaluateStatus           = FLCN_OK,
        .voltRailRoundVoltageStatus     = FLCN_ERR_ILWALID_INDEX,
        .expectedStatus                 = FLCN_ERR_ILWALID_INDEX,
    },
    {
        .voltRail = 
        {
            .relLimitVfeEquIdx          = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .altRelLimitVfeEquIdx       = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .ovLimitVfeEquIdx           = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .vminLimitVfeEquIdx         = LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
            .voltMarginLimitVfeEquIdx   = 0,
            .voltDeltauV =
                {
                    0,
                    0,
                    0,
                    0,
                },
        },
        .bVfeEvaluate                   = LW_TRUE,
        .voltuVResult                   = 625000,
        .roundedVoltuV                  = 625000,
        .expectedRelLimituV             = 0,
        .expectedAltRelLimituV          = 0,
        .expectedOvLimituV              = 0,
        .expectedMaxLimituV             = LW_U32_MAX,
        .expectedVminLimituV            = 0,
        .expectedVoltMarginLimituV      = 625000,
        .vfeEquEvaluateStatus           = FLCN_OK,
        .voltRailRoundVoltageStatus     = FLCN_ERR_ILWALID_INDEX,
        .expectedStatus                 = FLCN_ERR_ILWALID_INDEX,
    },
};

/*!
 * @brief   Table of positive test conditions for voltRailSetVoltage.
 */
static  VOLT_RAIL_SET_VOLTAGE_INPUT_OUTPUT voltRailSetVoltagePositive_inout[] =
{
    {
        .reqLwrrVoltuV                      = 625000,
        .mockedRoundedVoltuV                = 625000,
        .mockedVoltDeviceRoundVoltageStatus = FLCN_OK,
        .mockedVoltDeviceSetVoltageStatus = FLCN_OK,
        .expectedResult                     = FLCN_OK,
    },
};

/*!
 * @brief   Table of negative test conditions for voltRailSetVoltage.
 */
static  VOLT_RAIL_SET_VOLTAGE_INPUT_OUTPUT voltRailSetVoltageNegative_inout[] =
{
    {
        .reqLwrrVoltuV                      = 625000,
        .mockedRoundedVoltuV                = 625000,
        .mockedVoltDeviceRoundVoltageStatus = FLCN_ERR_ILWALID_STATE,
        .mockedVoltDeviceSetVoltageStatus = FLCN_OK,
        .expectedResult                     = FLCN_ERR_ILWALID_STATE,
    },
    {
        .reqLwrrVoltuV                      = 625000,
        .mockedRoundedVoltuV                = 625000,
        .mockedVoltDeviceRoundVoltageStatus = FLCN_OK,
        .mockedVoltDeviceSetVoltageStatus = FLCN_ERR_ILWALID_STATE,
        .expectedResult                     = FLCN_ERR_ILWALID_STATE,
    },
};

UT_SUITE_DEFINE(PMU_VOLTRAIL,
                UT_SUITE_SET_COMPONENT("Unit Volt Rail")
                UT_SUITE_SET_DESCRIPTION("Unit tests for Unit Volt Rail")
                UT_SUITE_SET_OWNER("ngodbole"))

UT_CASE_DEFINE(PMU_VOLTRAIL, voltRailGetVoltageMaxPositive,
  UT_CASE_SET_DESCRIPTION("Test voltRailGetVoltageMaxPositive")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test that the Unit Volt Rail handles all positive inputs correctly.
 *
 * @details    The test shall ilwoke the Unit Volt Rail method that does retrieves
 *             maximum voltage limit.
 *
 *             Parameters: Pointer to VOLT_RAIL, Pointer to store voltage max limit.
 *
 *             The test passes if the expected result in the dataset matches with
 *             the actual result received after exelwting the test.
 *
 *             Any other return value, constitutes failure.
 */

UT_CASE_RUN(PMU_VOLTRAIL, voltRailGetVoltageMaxPositive)
{
    LwU8    numEntriesPositive =
        sizeof(voltRailGetVoltageMaxPositive_inout) / sizeof(VOLT_RAIL_GET_VOLTAGE_MAX_INPUT_OUTPUT);
    LwU32           maxLimituV;
    FLCN_STATUS     status;
    LwU8            i;

    for (i = 0; i < numEntriesPositive; i++)
    {
        // Copy the data set.
        // Run the function under test.
        status = voltRailGetVoltageMax_IMPL(&voltRailGetVoltageMaxPositive_inout[i].voltRail, &voltRailGetVoltageMaxPositive_inout[i].expectedMaxLimituV);

        // Verify result.
        UT_ASSERT_EQUAL_UINT(status, voltRailGetVoltageMaxPositive_inout[i].expectedResult);
        UT_ASSERT_EQUAL_UINT(voltRailGetVoltageMaxPositive_inout[i].voltRail.maxLimituV, voltRailGetVoltageMaxPositive_inout[i].expectedMaxLimituV);
        UT_ASSERT_EQUAL_UINT(0U, UTF_INTRINSICS_MOCK_GET_HALT_COUNT());
    }
}

UT_CASE_DEFINE(PMU_VOLTRAIL, voltRailGetVoltageMaxNegative,
  UT_CASE_SET_DESCRIPTION("Test voltRailGetVoltageMaxNegative")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test that the Unit Volt Rail handles all negative inputs correctly.
 *
 * @details    The test shall ilwoke the Unit Volt Rail method that does retrieves
 *             maximum voltage limit.
 *
 *             Parameters: Pointer to VOLT_RAIL, Pointer to store voltage max limit.
 *
 *             The test passes if the expected result in the dataset matches with
 *             the actual result received after exelwting the test.
 *
 *             Any other return value, constitutes failure.
 */

UT_CASE_RUN(PMU_VOLTRAIL, voltRailGetVoltageMaxNegative)
{
    VOLT_RAIL      *pRail = NULL;
    LwU32           maxLimituV;
    FLCN_STATUS     status;

    // Pass VOLT_RAIL as NULL.
    status = voltRailGetVoltageMax_IMPL(pRail, &maxLimituV);

    // Verify result.
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_ILWALID_ARGUMENT);
}

UT_CASE_DEFINE(PMU_VOLTRAIL, voltRailGetVoltageInternalPositive,
  UT_CASE_SET_DESCRIPTION("Test voltRailGetVoltageInternalPositive")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test that the Unit Volt Rail handles all positive inputs correctly.
 *
 * @details    The test shall ilwoke the Unit Volt Rail method that does retrieves
 *             current voltage.
 *
 *             Parameters: Pointer to VOLT_RAIL, Pointer to store current voltage.
 *
 *             The test passes if the expected result in the dataset matches with
 *             the actual result received after exelwting the test.
 *
 *             Any other return value, constitutes failure.
 */

UT_CASE_RUN(PMU_VOLTRAIL, voltRailGetVoltageInternalPositive)
{
    LwU8    numEntriesPositive =
        sizeof(voltRailGetVoltageInternalPositive_inout) / sizeof(VOLT_RAIL_GET_VOLTAGE_INTERNAL_INPUT_OUTPUT);
    FLCN_STATUS     status;
    LwU8            i;

    for (i = 0; i < numEntriesPositive; i++)
    {
        // Copy the data set.
        // Run the function under test.
        status = voltRailGetVoltageInternal(&voltRailGetVoltageInternalPositive_inout[i].voltRail, &voltRailGetVoltageInternalPositive_inout[i].expectedLwrrVoltDefaultuV);

        // Verify result.
        UT_ASSERT_EQUAL_UINT(status, voltRailGetVoltageInternalPositive_inout[i].expectedResult);
        UT_ASSERT_EQUAL_UINT(voltRailGetVoltageInternalPositive_inout[i].voltRail.lwrrVoltDefaultuV, voltRailGetVoltageInternalPositive_inout[i].expectedLwrrVoltDefaultuV);
        UT_ASSERT_EQUAL_UINT(0U, UTF_INTRINSICS_MOCK_GET_HALT_COUNT());
    }
}

UT_CASE_DEFINE(PMU_VOLTRAIL, voltRailGetVoltageInternalNegative,
  UT_CASE_SET_DESCRIPTION("Test voltRailGetVoltageInternalNegative")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test that the Unit Volt Rail handles all negative inputs correctly.
 *
 * @details    The test shall ilwoke the Unit Volt Rail method that does retrieves
 *             current voltage..
 *
 *             Parameters: Pointer to VOLT_RAIL, Pointer to store current voltage.
 *
 *             The test passes if the expected result in the dataset matches with
 *             the actual result received after exelwting the test.
 *
 *             Any other return value, constitutes failure.
 */

UT_CASE_RUN(PMU_VOLTRAIL, voltRailGetVoltageInternalNegative)
{
    LwU32           lwrrVoltDefaultuV;
    FLCN_STATUS     status;

    // Pass VOLT_RAIL as NULL.
    status = voltRailGetVoltageInternal_IMPL(NULL, &lwrrVoltDefaultuV);

    // Verify result.
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_ILWALID_ARGUMENT);
}

UT_CASE_DEFINE(PMU_VOLTRAIL, voltRailsLoadPositive,
  UT_CASE_SET_DESCRIPTION("Test voltRailsLoadPositive")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Pre-test setup for testing the load interface for Unit Volt Rail
 *             with different set of inputs.
 *
 * @details    Before the test is run, set the input vales for mocked interfaces.
 */
PRE_TEST_METHOD(voltRailsLoadPositive)
{
    // Mock up the VOLT_RAIL board object group.
    railObj1.super.type      = LW2080_CTRL_VOLT_VOLT_DOMAIN_LOGIC;
    railObj1.super.grpIdx    = 0U;
    railObjects1[0]          = &railObj1.super;

    Volt.railMetadata.super.super.type        = LW2080_CTRL_BOARDOBJGRP_TYPE_E32;
    Volt.railMetadata.super.super.objSlots    = 1U;
    Volt.railMetadata.super.super.ppObjects   = railObjects1;
    boardObjGrpMaskInit_E32(&Volt.railMetadata.super.objMask);
    boardObjGrpMaskBitSet(&Volt.railMetadata.super.objMask, BOARDOBJ_GET_GRP_IDX(&railObj1));
}

/*!
 * @brief      Post-test teardown after testing load interface for Unit Volt Rail.
 *
 * @details    Ensure to set clean values for inputs to mocked interfaces.
 */
POST_TEST_METHOD(voltRailsLoadPositive)
{
    memset(&Volt, 0U, sizeof(Volt));
}

/*!
 * @brief      Test that the Unit Volt Rail Load interface handles all positive
 *             inputs correctly.
 *
 * @details    The test shall ilwoke the Unit Volt Rail method that loads VOLT_RAIL
 *             SW state.
 *
 *             Parameters: void
 *
 *             The test passes if the expected result in the dataset matches with
 *             the actual result received after exelwting the test.
 *
 *             Any other return value, constitutes failure.
 */
UT_CASE_RUN(PMU_VOLTRAIL, voltRailsLoadPositive)
{
    LwU8    numEntriesPositive =
        sizeof(voltRailsLoadPositive_inout) / sizeof(VOLT_RAILS_LOAD_INPUT_OUTPUT);
    FLCN_STATUS     status;
    LwU8            i;

    PRE_TEST_NAME(voltRailsLoadPositive)();

    for (i = 0; i < numEntriesPositive; i++)
    {
        // Copy the data set.
        // Run the function under test.
        voltDeviceGetVoltage_MOCK_CONFIG.status     = voltRailsLoadPositive_inout[i].voltDeviceGetVoltageMockStatus;
        voltDeviceRoundVoltage_MOCK_CONFIG.status   = voltRailsLoadPositive_inout[i].voltDeviceRoundVoltageMockStatus;
        status = voltRailsLoad();

        // Verify result.
        UT_ASSERT_EQUAL_UINT(status, voltRailsLoadPositive_inout[i].expectedResult);
        UT_ASSERT_EQUAL_UINT(0U, UTF_INTRINSICS_MOCK_GET_HALT_COUNT());
    }

    POST_TEST_NAME(voltRailsLoadPositive)();
}

UT_CASE_DEFINE(PMU_VOLTRAIL, voltRailsLoadNegative,
  UT_CASE_SET_DESCRIPTION("Test voltRailsLoadNegative")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Pre-test setup for testing the load interface for Unit Volt Rail
 *             with different set of inputs.
 *
 * @details    Before the test is run, set the input values for mocked interfaces.
 */
PRE_TEST_METHOD(voltRailsLoadNegative)
{
    // Mock up the VOLT_RAIL board object group.
    railObj1.super.type      = LW2080_CTRL_VOLT_VOLT_DOMAIN_LOGIC;
    railObj1.super.grpIdx    = 0U;
    railObjects1[0]          = &railObj1.super;

    Volt.railMetadata.super.super.type        = LW2080_CTRL_BOARDOBJGRP_TYPE_E32;
    Volt.railMetadata.super.super.objSlots    = 1U;
    Volt.railMetadata.super.super.ppObjects   = railObjects1;
    boardObjGrpMaskInit_E32(&Volt.railMetadata.super.objMask);
    boardObjGrpMaskBitSet(&Volt.railMetadata.super.objMask, BOARDOBJ_GET_GRP_IDX(&railObj1));
}

/*!
 * @brief      Post-test teardown after testing load interface for Unit Volt Rail.
 *
 * @details    Ensure to set clean values for inputs to mocked interfaces.
 */
POST_TEST_METHOD(voltRailsLoadNegative)
{
    memset(&Volt, 0U, sizeof(Volt));
}

/*!
 * @brief      Test that the Unit Volt Rail handles all negative inputs correctly.
 *
 * @details    The test shall ilwoke the Unit Volt Rail method that loads VOLT
 *             SW state.
 *
 *             Parameters: void
 *
 *             The test passes if the expected result in the dataset matches with
 *             the actual result received after exelwting the test.
 *
 *             Any other return value, constitutes failure.
 */
UT_CASE_RUN(PMU_VOLTRAIL, voltRailsLoadNegative)
{
    LwU8    numEntriesNegative =
        sizeof(voltRailsLoadNegative_inout) / sizeof(VOLT_RAILS_LOAD_INPUT_OUTPUT);
    FLCN_STATUS     status;
    LwU8            i;

    PRE_TEST_NAME(voltRailsLoadNegative)();

    for (i = 0; i < numEntriesNegative; i++)
    {
        // Copy the data set.
        voltDeviceGetVoltage_MOCK_CONFIG.status     = voltRailsLoadNegative_inout[i].voltDeviceGetVoltageMockStatus;
        voltDeviceRoundVoltage_MOCK_CONFIG.status   = voltRailsLoadNegative_inout[i].voltDeviceRoundVoltageMockStatus;

        // Run the function under test.
        status = voltRailsLoad();

        // Verify result.
        UT_ASSERT_EQUAL_UINT(status, voltRailsLoadNegative_inout[i].expectedResult);
    }
    POST_TEST_NAME(voltRailsLoadNegative)();
}


UT_CASE_DEFINE(PMU_VOLTRAIL, voltRailsDynamilwpdatePositive,
  UT_CASE_SET_DESCRIPTION("Test voltRailsDynamilwpdatePositive")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Pre-test setup for testing the dynamic update interface for Unit.
 *             Volt Rail with different set of inputs.
 *
 * @details    Before the test is run, set the input values for mocked interfaces.
 */
PRE_TEST_METHOD(voltRailsDynamilwpdatePositive)
{
    // Mock up the VOLT_RAIL board object group.
    railObj1.super.type      = LW2080_CTRL_VOLT_VOLT_DOMAIN_LOGIC;
    railObj1.super.grpIdx    = 0U;
    railObjects1[0]          = &railObj1.super;

    Volt.railMetadata.super.super.type        = LW2080_CTRL_BOARDOBJGRP_TYPE_E32;
    Volt.railMetadata.super.super.objSlots    = 1U;
    Volt.railMetadata.super.super.ppObjects   = railObjects1;
    boardObjGrpMaskInit_E32(&Volt.railMetadata.super.objMask);
    boardObjGrpMaskBitSet(&Volt.railMetadata.super.objMask, BOARDOBJ_GET_GRP_IDX(&railObj1));
}

/*!
 * @brief      Post-test teardown after testing the dynamic update interface for Unit.
 *             Volt Rail with different set of inputs
 *
 * @details    Ensure to set clean values for inputs to mocked interfaces.
 */
POST_TEST_METHOD(voltRailsDynamilwpdatePositive)
{
    memset(&Volt, 0U, sizeof(Volt));
}

/*!
 * @brief      Test that the Unit Volt Rail handles all positive inputs correctly.
 *
 * @details    The test shall ilwoke the Unit Volt Rail method that dynamically
 *             updates all VFE dependent voltage parameters.
 *
 *             Parameters: void
 *
 *             The test passes if the expected result in the dataset matches with
 *             the actual result received after exelwting the test.
 *
 *             Any other return value, constitutes failure.
 */
UT_CASE_RUN(PMU_VOLTRAIL, voltRailsDynamilwpdatePositive)
{
    LwU8    numEntriesPositive =
        sizeof(voltRailsDynamilwpdatePositive_inout) / sizeof(VOLT_RAILS_DYNAMIC_UPDATE_INPUT_OUTPUT);
    FLCN_STATUS     status;
    LwU8            i;

    PRE_TEST_NAME(voltRailsDynamilwpdatePositive)();

    for (i = 0; i < numEntriesPositive; i++)
    {
        // Copy the data set.
        // Run the function under test.
        voltRailDynamilwpdate_MOCK_CONFIG.status   = voltRailsDynamilwpdatePositive_inout[i].voltRailDynamilwpdateMockStatus;
        status = voltRailsDynamilwpdate_IMPL(LW_TRUE);

        // Verify result.
        UT_ASSERT_EQUAL_UINT(status, voltRailsDynamilwpdatePositive_inout[i].expectedResult);
        UT_ASSERT_EQUAL_UINT(0U, UTF_INTRINSICS_MOCK_GET_HALT_COUNT());
    }

    POST_TEST_NAME(voltRailsDynamilwpdatePositive)();
}

UT_CASE_DEFINE(PMU_VOLTRAIL, voltRailsDynamilwpdateNegative,
  UT_CASE_SET_DESCRIPTION("Test voltRailsDynamilwpdateNegative")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Pre-test setup for testing the dynamic update interface for Unit.
 *             Volt Rail with different set of inputs.
 *
 * @details    Before the test is run, set the input values for mocked interfaces.
 */
PRE_TEST_METHOD(voltRailsDynamilwpdateNegative)
{
    // Mock up the VOLT_RAIL board object group.
    railObj1.super.type      = LW2080_CTRL_VOLT_VOLT_DOMAIN_LOGIC;
    railObj1.super.grpIdx    = 0U;
    railObjects1[0]          = &railObj1.super;

    Volt.railMetadata.super.super.type        = LW2080_CTRL_BOARDOBJGRP_TYPE_E32;
    Volt.railMetadata.super.super.objSlots    = 1U;
    Volt.railMetadata.super.super.ppObjects   = railObjects1;
    boardObjGrpMaskInit_E32(&Volt.railMetadata.super.objMask);
    boardObjGrpMaskBitSet(&Volt.railMetadata.super.objMask, BOARDOBJ_GET_GRP_IDX(&railObj1));
}

/*!
 * @brief      Post-test teardown after testing the dynamic update interface for Unit.
 *             Volt Rail with different set of inputs.
 *
 * @details    Ensure to set clean values for inputs to mocked interfaces.
 */
POST_TEST_METHOD(voltRailsDynamilwpdateNegative)
{
    memset(&Volt, 0U, sizeof(Volt));
}

/*!
 * @brief      Test that the Unit Volt Rail handles all negative inputs correctly.
 *
 * @details    The test shall ilwoke the Unit Volt Rail  method that dynamically
 *             updates all VFE dependent voltage parameters.
 *
 *             Parameters: void.
 *
 *             The test passes if the expected result in the dataset matches with
 *             the actual result received after exelwting the test.
 *
 *             Any other return value, constitutes failure.
 */
UT_CASE_RUN(PMU_VOLTRAIL, voltRailsDynamilwpdateNegative)
{
    LwU8    numEntriesNegative =
        sizeof(voltRailsDynamilwpdateNegative_inout) / sizeof(VOLT_RAILS_DYNAMIC_UPDATE_INPUT_OUTPUT);
    FLCN_STATUS     status;
    LwU8            i;

    PRE_TEST_NAME(voltRailsDynamilwpdateNegative)();

    for (i = 0; i < numEntriesNegative; i++)
    {
        // Copy the data set.
        voltRailDynamilwpdate_MOCK_CONFIG.status     = voltRailsDynamilwpdateNegative_inout[i].voltRailDynamilwpdateMockStatus;

        // Run the function under test.
        status = voltRailsDynamilwpdate_IMPL(LW_TRUE);

        // Verify result.
        UT_ASSERT_EQUAL_UINT(status, voltRailsDynamilwpdateNegative_inout[i].expectedResult);
    }
    POST_TEST_NAME(voltRailsDynamilwpdateNegative)();
}

UT_CASE_DEFINE(PMU_VOLTRAIL, voltRailGetVoltagePositive,
  UT_CASE_SET_DESCRIPTION("Test voltRailGetVoltagePositive")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test that the Unit Volt Rail handles all negative inputs correctly.
 *
 * @details    The test shall ilwoke the Unit Volt Rail  method that dynamically
 *             updates all VFE dependent voltage parameters.
 *
 *             Parameters: void.
 *
 *             The test passes if the expected result in the dataset matches with
 *             the actual result received after exelwting the test.
 *
 *             Any other return value, constitutes failure.
 */
UT_CASE_RUN(PMU_VOLTRAIL, voltRailGetVoltagePositive)
{
    LwU8    numEntriesPositive =
        sizeof(voltRailGetVoltagePositive_inout) / sizeof(VOLT_RAIL_GET_VOLTAGE_INPUT_OUTPUT);
    FLCN_STATUS     status;
    LwU8            i;

    for (i = 0; i < numEntriesPositive; i++)
    {
        // Copy the data set.
        // Run the function under test.
        status = voltRailGetVoltage_IMPL(&voltRailGetVoltagePositive_inout[i].voltRail, &voltRailGetVoltagePositive_inout[i].expectedLwrrVoltuV);

        // Verify result.
        UT_ASSERT_EQUAL_UINT(status, voltRailGetVoltagePositive_inout[i].expectedResult);
        UT_ASSERT_EQUAL_UINT(voltRailGetVoltagePositive_inout[i].voltRail.lwrrVoltDefaultuV, voltRailGetVoltagePositive_inout[i].expectedLwrrVoltuV);
        UT_ASSERT_EQUAL_UINT(0U, UTF_INTRINSICS_MOCK_GET_HALT_COUNT());
    }
}

UT_CASE_DEFINE(PMU_VOLTRAIL, voltRailGetVoltageNegative,
  UT_CASE_SET_DESCRIPTION("Test voltRailGetVoltageNegative")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test that the Unit Volt Rail handles all negative inputs correctly.
 *
 * @details    The test shall ilwoke the Unit Volt Rail  method that dynamically
 *             updates all VFE dependent voltage parameters.
 *
 *             Parameters: void.
 *
 *             The test passes if the expected result in the dataset matches with
 *             the actual result received after exelwting the test.
 *
 *             Any other return value, constitutes failure.
 */
UT_CASE_RUN(PMU_VOLTRAIL, voltRailGetVoltageNegative)
{
    LwU32           lwrrVoltDefaultuV;
    FLCN_STATUS     status;

    // Pass VOLT_RAIL as NULL.
    status = voltRailGetVoltage_IMPL(NULL, &lwrrVoltDefaultuV);

    // Verify result.
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_ILWALID_ARGUMENT);
}

UT_CASE_DEFINE(PMU_VOLTRAIL, voltRailRoundVoltagePositive,
  UT_CASE_SET_DESCRIPTION("Test voltRailRoundVoltagePositive")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test that the Unit Volt Rail handles all negative inputs correctly.
 *
 * @details    The test shall ilwoke the Unit Volt Rail  method that dynamically
 *             updates all VFE dependent voltage parameters.
 *
 *             Parameters: void.
 *
 *             The test passes if the expected result in the dataset matches with
 *             the actual result received after exelwting the test.
 *
 *             Any other return value, constitutes failure.
 */
UT_CASE_RUN(PMU_VOLTRAIL, voltRailRoundVoltagePositive)
{
    LwU8    numEntriesPositive =
        sizeof(voltRailRoundVoltagePositive_inout) / sizeof(VOLT_RAIL_ROUND_VOLTAGE_INPUT_OUTPUT);
    FLCN_STATUS     status;
    VOLT_RAIL       voltRail;
    LwS32           roundedVoltuV;
    LwU8            i;

    for (i = 0; i < numEntriesPositive; i++)
    {
        // Copy the data set.
        voltDeviceRoundVoltage_MOCK_CONFIG.roundedVoltageuV = voltRailRoundVoltagePositive_inout[i].mockedRoundedVoltuV;
        voltDeviceRoundVoltage_MOCK_CONFIG.status = voltRailRoundVoltagePositive_inout[i].mockedVoltDeviceRoundVoltageStatus;

        // Run the function under test.
        status = voltRailRoundVoltage_IMPL(&voltRail, &roundedVoltuV, LW_FALSE, LW_FALSE);

        // Verify result.
        UT_ASSERT_EQUAL_UINT(status, voltRailRoundVoltagePositive_inout[i].expectedResult);
        UT_ASSERT_EQUAL_UINT(roundedVoltuV, voltRailRoundVoltagePositive_inout[i].mockedRoundedVoltuV);
        UT_ASSERT_EQUAL_UINT(0U, UTF_INTRINSICS_MOCK_GET_HALT_COUNT());
    }
}

UT_CASE_DEFINE(PMU_VOLTRAIL, voltRailRoundVoltageNegative,
  UT_CASE_SET_DESCRIPTION("Test voltRailRoundVoltageNegative")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test that the Unit Volt Rail handles all negative inputs correctly.
 *
 * @details    The test shall ilwoke the Unit Volt Rail  method that dynamically
 *             updates all VFE dependent voltage parameters.
 *
 *             Parameters: void.
 *
 *             The test passes if the expected result in the dataset matches with
 *             the actual result received after exelwting the test.
 *
 *             Any other return value, constitutes failure.
 */
UT_CASE_RUN(PMU_VOLTRAIL, voltRailRoundVoltageNegative)
{
    LwU8    numEntriesNegative =
        sizeof(voltRailRoundVoltageNegative_inout) / sizeof(VOLT_RAIL_ROUND_VOLTAGE_INPUT_OUTPUT);
    FLCN_STATUS     status;
    VOLT_RAIL       voltRail;
    LwS32           roundedVoltuV;
    LwU8            i;

    for (i = 0; i < numEntriesNegative; i++)
    {
        // Copy the data set.
        voltDeviceRoundVoltage_MOCK_CONFIG.status = voltRailRoundVoltagePositive_inout[i].mockedVoltDeviceRoundVoltageStatus;

        // Run the function under test.
        status = voltRailRoundVoltage(&voltRail, &roundedVoltuV, LW_FALSE, LW_FALSE);

        // Verify result.
        UT_ASSERT_EQUAL_UINT(status, voltRailRoundVoltagePositive_inout[i].expectedResult);
    }
}

UT_CASE_DEFINE(PMU_VOLTRAIL, voltRailDynamilwpdatePositive,
  UT_CASE_SET_DESCRIPTION("Test voltRailDynamilwpdatePositive")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test that the Unit Volt Rail handles all positive inputs correctly.
 *
 * @details    The test shall ilwoke the Unit Volt Rail  method that dynamically
 *             updates all VFE dependent voltage parameters.
 *
 *             Parameters: void.
 *
 *             The test passes if the expected result in the dataset matches with
 *             the actual result received after exelwting the test.
 *
 *             Any other return value, constitutes failure.
 */
UT_CASE_RUN(PMU_VOLTRAIL, voltRailDynamilwpdatePositive)
{
    LwU8    numEntriesPositive =
        sizeof(voltRailDynamilwpdatePositive_inout) / sizeof(VOLT_RAIL_DYNAMIC_UPDATE_INPUT_OUTPUT);
    FLCN_STATUS     status;
    LwU8            i;

    for (i = 0; i < numEntriesPositive; i++)
    {
        // Copy the data set.
        vfeEquEvaluate_MOCK_CONFIG.result = voltRailDynamilwpdatePositive_inout[i].voltuVResult;
        vfeEquEvaluate_MOCK_CONFIG.status = voltRailDynamilwpdatePositive_inout[i].vfeEquEvaluateStatus;
        voltRailRoundVoltage_MOCK_CONFIG.status = voltRailDynamilwpdatePositive_inout[i].voltRailRoundVoltageStatus;
        voltRailRoundVoltage_MOCK_CONFIG.roundedVoltageuV = voltRailDynamilwpdatePositive_inout[i].roundedVoltuV;
        // Run the function under test.
        status = voltRailDynamilwpdate_IMPL(&voltRailDynamilwpdatePositive_inout[i].voltRail, voltRailDynamilwpdatePositive_inout[i].bVfeEvaluate);

        // Verify result.
        UT_ASSERT_EQUAL_UINT(voltRailDynamilwpdatePositive_inout[i].voltRail.relLimituV, voltRailDynamilwpdatePositive_inout[i].expectedRelLimituV);
        UT_ASSERT_EQUAL_UINT(voltRailDynamilwpdatePositive_inout[i].voltRail.altRelLimituV, voltRailDynamilwpdatePositive_inout[i].expectedAltRelLimituV);
        UT_ASSERT_EQUAL_UINT(voltRailDynamilwpdatePositive_inout[i].voltRail.ovLimituV, voltRailDynamilwpdatePositive_inout[i].expectedOvLimituV);
        UT_ASSERT_EQUAL_UINT(voltRailDynamilwpdatePositive_inout[i].voltRail.maxLimituV, voltRailDynamilwpdatePositive_inout[i].expectedMaxLimituV);
        UT_ASSERT_EQUAL_UINT(voltRailDynamilwpdatePositive_inout[i].voltRail.vminLimituV, voltRailDynamilwpdatePositive_inout[i].expectedVminLimituV);
        UT_ASSERT_EQUAL_UINT(voltRailDynamilwpdatePositive_inout[i].voltRail.voltMarginLimituV, voltRailDynamilwpdatePositive_inout[i].expectedVoltMarginLimituV);
        UT_ASSERT_EQUAL_UINT(status, voltRailDynamilwpdatePositive_inout[i].expectedStatus);
        UT_ASSERT_EQUAL_UINT(0U, UTF_INTRINSICS_MOCK_GET_HALT_COUNT());
    }
}

UT_CASE_DEFINE(PMU_VOLTRAIL, voltRailDynamilwpdateNegative,
  UT_CASE_SET_DESCRIPTION("Test voltRailDynamilwpdateNegative")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test that the Unit Volt Rail handles all negative inputs correctly.
 *
 * @details    The test shall ilwoke the Unit Volt Rail  method that dynamically
 *             updates all VFE dependent voltage parameters.
 *
 *             Parameters: void.
 *
 *             The test passes if the expected result in the dataset matches with
 *             the actual result received after exelwting the test.
 *
 *             Any other return value, constitutes failure.
 */
UT_CASE_RUN(PMU_VOLTRAIL, voltRailDynamilwpdateNegative)
{
    LwU8    numEntriesNegative =
        sizeof(voltRailDynamilwpdateNegative_inout) / sizeof(VOLT_RAIL_DYNAMIC_UPDATE_INPUT_OUTPUT);
    FLCN_STATUS     status;
    LwU8            i;

    for (i = 0; i < numEntriesNegative; i++)
    {
        // Copy the data set.
        vfeEquEvaluate_MOCK_CONFIG.status = voltRailDynamilwpdateNegative_inout[i].vfeEquEvaluateStatus;
        voltRailRoundVoltage_MOCK_CONFIG.status = voltRailDynamilwpdateNegative_inout[i].voltRailRoundVoltageStatus;

        // Run the function under test.
        status = voltRailDynamilwpdate_IMPL(&voltRailDynamilwpdateNegative_inout[i].voltRail, voltRailDynamilwpdateNegative_inout[i].bVfeEvaluate);

        // Verify result.
        UT_ASSERT_EQUAL_UINT(status, voltRailDynamilwpdateNegative_inout[i].expectedStatus);
    }
}

UT_CASE_DEFINE(PMU_VOLTRAIL, voltRailSetVoltagePositive,
  UT_CASE_SET_DESCRIPTION("Test voltRailSetVoltagePositive")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Pre-test setup for testing the dynamic update interface for Unit.
 *             Volt Rail with different set of inputs.
 *
 * @details    Before the test is run, set the input values for mocked interfaces.
 */
PRE_TEST_METHOD(voltRailSetVoltagePositive)
{
    // Mock up the VOLT_DEVICE board object group.
    deviceObj1.super.type      = LW2080_CTRL_VOLT_VOLT_DEVICE_TYPE_PWM;
    deviceObj1.super.grpIdx    = 0U;
    deviceObjects1[0]          = &deviceObj1.super;

    Volt.devices.super.type        = LW2080_CTRL_BOARDOBJGRP_TYPE_E32;
    Volt.devices.super.objSlots    = 1U;
    Volt.devices.super.ppObjects   = deviceObjects1;
    boardObjGrpMaskInit_E32(&Volt.devices.objMask);
    boardObjGrpMaskBitSet(&Volt.devices.objMask, BOARDOBJ_GET_GRP_IDX(&deviceObj1));

    // Mock up the VOLT_RAIL board object group.
    railObj1.super.type      = LW2080_CTRL_VOLT_VOLT_DOMAIN_LOGIC;
    railObj1.super.grpIdx    = 0U;
    boardObjGrpMaskInit_E32(&railObj1.voltDevMask);
    boardObjGrpMaskBitSet(&railObj1.voltDevMask, BOARDOBJ_GET_GRP_IDX(&deviceObj1));
    railObjects1[0]          = &railObj1.super;

    Volt.railMetadata.super.super.type        = LW2080_CTRL_BOARDOBJGRP_TYPE_E32;
    Volt.railMetadata.super.super.objSlots    = 1U;
    Volt.railMetadata.super.super.ppObjects   = railObjects1;
    boardObjGrpMaskInit_E32(&Volt.railMetadata.super.objMask);
    boardObjGrpMaskBitSet(&Volt.railMetadata.super.objMask, BOARDOBJ_GET_GRP_IDX(&railObj1));
}

/*!
 * @brief      Post-test teardown after testing the dynamic update interface for Unit.
 *             Volt Rail with different set of inputs.
 *
 * @details    Ensure to set clean values for inputs to mocked interfaces.
 */
POST_TEST_METHOD(voltRailSetVoltagePositive)
{
    memset(&Volt, 0U, sizeof(Volt));
    memset(&railObj1, 0U, sizeof(VOLT_RAIL));
    memset(&deviceObj1, 0U, sizeof(VOLT_DEVICE));
    memset(&railObjects1, 0U, sizeof(BOARDOBJ *));
    memset(&deviceObjects1, 0U, sizeof(BOARDOBJ *));
}

/*!
 * @brief      Test that the Unit Volt Rail handles all positive inputs correctly.
 *
 * @details    The test shall ilwoke the Unit Volt Rail  method that dynamically
 *             updates all VFE dependent voltage parameters.
 *
 *             Parameters: void.
 *
 *             The test passes if the expected result in the dataset matches with
 *             the actual result received after exelwting the test.
 *
 *             Any other return value, constitutes failure.
 */
UT_CASE_RUN(PMU_VOLTRAIL, voltRailSetVoltagePositive)
{
    LwU8    numEntriesPositive =
        sizeof(voltRailSetVoltagePositive_inout) / sizeof(VOLT_RAIL_SET_VOLTAGE_INPUT_OUTPUT);
    FLCN_STATUS     status;
    LwU8            i;

    PRE_TEST_NAME(voltRailSetVoltagePositive)();
    for (i = 0; i < numEntriesPositive; i++)
    {
        // Copy the data set.
        voltDeviceRoundVoltage_MOCK_CONFIG.roundedVoltageuV = voltRailSetVoltagePositive_inout[i].mockedRoundedVoltuV;
        voltDeviceRoundVoltage_MOCK_CONFIG.status = voltRailSetVoltagePositive_inout[i].mockedVoltDeviceRoundVoltageStatus;
        voltDeviceSetVoltage_MOCK_CONFIG.status = voltRailSetVoltagePositive_inout[i].mockedVoltDeviceSetVoltageStatus;

        // Run the function under test.
        status = voltRailSetVoltage_IMPL(&railObj1, voltRailSetVoltagePositive_inout[i].reqLwrrVoltuV, LW_FALSE, LW_FALSE, LW2080_CTRL_VOLT_VOLT_RAIL_ACTION_VF_SWITCH);

        // Verify result.
        UT_ASSERT_EQUAL_UINT(status, voltRailRoundVoltagePositive_inout[i].expectedResult);
        UT_ASSERT_EQUAL_UINT(railObj1.lwrrVoltDefaultuV, voltRailSetVoltagePositive_inout[i].reqLwrrVoltuV);
        UT_ASSERT_EQUAL_UINT(0U, UTF_INTRINSICS_MOCK_GET_HALT_COUNT());
    }
    POST_TEST_NAME(voltRailSetVoltagePositive)();
}

UT_CASE_DEFINE(PMU_VOLTRAIL, voltRailSetVoltageNegative,
  UT_CASE_SET_DESCRIPTION("Test voltRailSetVoltageNegative")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Pre-test setup for testing the dynamic update interface for Unit.
 *             Volt Rail with different set of inputs.
 *
 * @details    Before the test is run, set the input values for mocked interfaces.
 */
PRE_TEST_METHOD(voltRailSetVoltageNegative)
{
    // Mock up the VOLT_DEVICE board object group.
    deviceObj1.super.type      = LW2080_CTRL_VOLT_VOLT_DEVICE_TYPE_PWM;
    deviceObj1.super.grpIdx    = 0U;
    deviceObjects1[0]          = &deviceObj1.super;

    Volt.devices.super.type        = LW2080_CTRL_BOARDOBJGRP_TYPE_E32;
    Volt.devices.super.objSlots    = 1U;
    Volt.devices.super.ppObjects   = deviceObjects1;
    boardObjGrpMaskInit_E32(&Volt.devices.objMask);
    boardObjGrpMaskBitSet(&Volt.devices.objMask, BOARDOBJ_GET_GRP_IDX(&deviceObj1));

    // Mock up the VOLT_RAIL board object group.
    railObj1.super.type      = LW2080_CTRL_VOLT_VOLT_DOMAIN_LOGIC;
    railObj1.super.grpIdx    = 0U;
    boardObjGrpMaskInit_E32(&railObj1.voltDevMask);
    boardObjGrpMaskBitSet(&railObj1.voltDevMask, BOARDOBJ_GET_GRP_IDX(&deviceObj1));
    railObjects1[0]          = &railObj1.super;

    Volt.railMetadata.super.super.type        = LW2080_CTRL_BOARDOBJGRP_TYPE_E32;
    Volt.railMetadata.super.super.objSlots    = 1U;
    Volt.railMetadata.super.super.ppObjects   = railObjects1;
    boardObjGrpMaskInit_E32(&Volt.railMetadata.super.objMask);
    boardObjGrpMaskBitSet(&Volt.railMetadata.super.objMask, BOARDOBJ_GET_GRP_IDX(&railObj1));
}

/*!
 * @brief      Post-test teardown after testing the dynamic update interface for Unit.
 *             Volt Rail with different set of inputs.
 *
 * @details    Ensure to set clean values for inputs to mocked interfaces.
 */
POST_TEST_METHOD(voltRailSetVoltageNegative)
{
    memset(&Volt, 0U, sizeof(Volt));
    memset(&railObj1, 0U, sizeof(VOLT_RAIL));
    memset(&deviceObj1, 0U, sizeof(VOLT_DEVICE));
    memset(&railObjects1, 0U, sizeof(BOARDOBJ *));
    memset(&deviceObjects1, 0U, sizeof(BOARDOBJ *));
}

/*!
 * @brief      Test that the Unit Volt Rail handles all negative inputs correctly.
 *
 * @details    The test shall ilwoke the Unit Volt Rail  method that dynamically
 *             updates all VFE dependent voltage parameters.
 *
 *             Parameters: void.
 *
 *             The test passes if the expected result in the dataset matches with
 *             the actual result received after exelwting the test.
 *
 *             Any other return value, constitutes failure.
 */
UT_CASE_RUN(PMU_VOLTRAIL, voltRailSetVoltageNegative)
{
    LwU8    numEntriesNegative =
        sizeof(voltRailSetVoltageNegative_inout) / sizeof(VOLT_RAIL_SET_VOLTAGE_INPUT_OUTPUT);
    FLCN_STATUS     status;
    LwU8            i;

    PRE_TEST_NAME(voltRailSetVoltageNegative)();
    for (i = 0; i < numEntriesNegative; i++)
    {
        // Copy the data set.
        voltDeviceRoundVoltage_MOCK_CONFIG.roundedVoltageuV = voltRailSetVoltageNegative_inout[i].mockedRoundedVoltuV;
        voltDeviceRoundVoltage_MOCK_CONFIG.status = voltRailSetVoltageNegative_inout[i].mockedVoltDeviceRoundVoltageStatus;
        voltDeviceSetVoltage_MOCK_CONFIG.status = voltRailSetVoltageNegative_inout[i].mockedVoltDeviceSetVoltageStatus;

        // Run the function under test.
        status = voltRailSetVoltage_IMPL(&railObj1, voltRailSetVoltageNegative_inout[i].reqLwrrVoltuV, LW_FALSE, LW_FALSE, LW2080_CTRL_VOLT_VOLT_RAIL_ACTION_VF_SWITCH);

        // Verify result.
        UT_ASSERT_EQUAL_UINT(status, voltRailSetVoltageNegative_inout[i].expectedResult);
    }
    POST_TEST_NAME(voltRailSetVoltagePositive)();

    // Run with voltDevMask as 0.
    status = voltRailSetVoltage_IMPL(&railObj1, voltRailSetVoltageNegative_inout[i].reqLwrrVoltuV, LW_FALSE, LW_FALSE, LW2080_CTRL_VOLT_VOLT_RAIL_ACTION_VF_SWITCH);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_ILWALID_STATE);
}

UT_CASE_DEFINE(PMU_VOLTRAIL, voltRailSetNoiseUnawareVminRailListNull,
  UT_CASE_SET_DESCRIPTION("Test voltRailSetNoiseUnawareVminRailListNull")
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
UT_CASE_RUN(PMU_VOLTRAIL, voltRailSetNoiseUnawareVminRailListNull)
{
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST *pList= NULL;
    FLCN_STATUS                      status;

    status = voltRailSetNoiseUnawareVmin_IMPL(pList);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_ILWALID_ARGUMENT);
}

UT_CASE_DEFINE(PMU_VOLTRAIL, voltRailSetNoiseUnawareVminRailVoltNotLoaded,
  UT_CASE_SET_DESCRIPTION("Test voltRailSetNoiseUnawareVminRailVoltNotLoaded")
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
UT_CASE_RUN(PMU_VOLTRAIL, voltRailSetNoiseUnawareVminRailVoltNotLoaded)
{
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST list;
    FLCN_STATUS                     status;

    Volt.bLoaded = LW_FALSE;
    status = voltRailSetNoiseUnawareVmin_IMPL(&list);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_ILWALID_STATE);
}

UT_CASE_DEFINE(PMU_VOLTRAIL, voltRailSetNoiseUnawareVminRailIlwalidVoltRail,
  UT_CASE_SET_DESCRIPTION("Test voltRailSetNoiseUnawareVminRailIlwalidVoltRail")
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
UT_CASE_RUN(PMU_VOLTRAIL, voltRailSetNoiseUnawareVminRailIlwalidVoltRail)
{
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST list;
    FLCN_STATUS                     status;
    memset(&Volt, 0U, sizeof(Volt));
    Volt.bLoaded = LW_TRUE;
    list.numRails = 1;
    list.rails[0].railIdx = 0;
    status = voltRailSetNoiseUnawareVmin_IMPL(&list);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_ILWALID_INDEX);
}

UT_CASE_DEFINE(PMU_VOLTRAIL, voltRailSetNoiseUnawareVminSuccess,
  UT_CASE_SET_DESCRIPTION("Test voltRailSetNoiseUnawareVminSuccess")
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
UT_CASE_RUN(PMU_VOLTRAIL, voltRailSetNoiseUnawareVminSuccess)
{
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST list;
    FLCN_STATUS                     status;

    Volt.bLoaded = LW_TRUE;

    // Mock up the VOLT_RAIL board object group.
    railObj1.super.type      = LW2080_CTRL_VOLT_VOLT_DOMAIN_LOGIC;
    railObj1.super.grpIdx    = 0U;
    railObjects1[0]          = &railObj1.super;

    Volt.railMetadata.super.super.type        = LW2080_CTRL_BOARDOBJGRP_TYPE_E32;
    Volt.railMetadata.super.super.objSlots    = 1U;
    Volt.railMetadata.super.super.ppObjects   = railObjects1;
    boardObjGrpMaskInit_E32(&Volt.railMetadata.super.objMask);
    boardObjGrpMaskBitSet(&Volt.railMetadata.super.objMask, BOARDOBJ_GET_GRP_IDX(&railObj1));

    list.numRails = 1;
    list.rails[0].railIdx                  = 0;
    list.rails[0].voltageMinNoiseUnawareuV = 650000;

    status = voltRailSetNoiseUnawareVmin_IMPL(&list);
    UT_ASSERT_EQUAL_UINT(railObj1.voltMinNoiseUnawareuV , list.rails[0].voltageMinNoiseUnawareuV);
}

UT_CASE_DEFINE(PMU_VOLTRAIL, voltRailGetVoltageMinFail,
  UT_CASE_SET_DESCRIPTION("Test voltRailGetVoltageMinFail")
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
UT_CASE_RUN(PMU_VOLTRAIL, voltRailGetVoltageMinFail)
{
    VOLT_RAIL   voltRail;
    LwU32       voltuV;
    FLCN_STATUS status;

    voltRail.vminLimitVfeEquIdx = LW2080_CTRL_BOARDOBJ_IDX_ILWALID;
    voltRail.vminLimituV = 1234;

    status = voltRailGetVoltageMin(&voltRail, &voltuV);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_NOT_SUPPORTED);
}

UT_CASE_DEFINE(PMU_VOLTRAIL, voltRailGetVoltageMinSuccess,
  UT_CASE_SET_DESCRIPTION("Test voltRailGetVoltageMinSuccess")
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
UT_CASE_RUN(PMU_VOLTRAIL, voltRailGetVoltageMinSuccess)
{
    VOLT_RAIL   voltRail;
    LwU32       voltuV;
    FLCN_STATUS status;

    voltRail.vminLimitVfeEquIdx = 2;
    voltRail.vminLimituV = 1234;

    status = voltRailGetVoltageMin(&voltRail, &voltuV);
    UT_ASSERT_EQUAL_UINT(status, FLCN_OK);
    UT_ASSERT_EQUAL_UINT(voltuV, voltRail.vminLimituV);
}

UT_CASE_DEFINE(PMU_VOLTRAIL, voltRailGetNoiseUnawareVminSuccess,
  UT_CASE_SET_DESCRIPTION("Test voltRailGetNoiseUnawareVminSuccess")
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
UT_CASE_RUN(PMU_VOLTRAIL, voltRailGetNoiseUnawareVminSuccess)
{
    VOLT_RAIL   voltRail;
    LwU32       voltuV;
    FLCN_STATUS status;

    voltRail.voltMinNoiseUnawareuV = 1234;

    status = voltRailGetNoiseUnawareVmin(&voltRail, &voltuV);
    UT_ASSERT_EQUAL_UINT(status, FLCN_OK);
    UT_ASSERT_EQUAL_UINT(voltuV, voltRail.voltMinNoiseUnawareuV );
}
