/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    voltrail-mock.c
 * @brief   Mock implementations of voltrail public interfaces.
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "volt/objvolt.h"       // only included to have dependencies resolved
#include "volt/voltrail.h"
#include "volt/voltrail-mock.h"

/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
VOLT_RAIL_GET_VOLTAGE_MOCK_CONFIG            voltRailGetVoltage_MOCK_CONFIG;
VOLT_RAIL_GET_VOLTAGE_MOCK_CONFIG            voltRailGetVoltage_MOCK_CONFIG;
VOLT_RAIL_SET_NOISE_UNAWARE_VMIN_MOCK_CONFIG voltRailSetNoiseUnawareVmin_MOCK_CONFIG;
VOLT_RAILS_DYNAMIC_UPDATE_MOCK_CONFIG        voltRailsDynamilwpdate_MOCK_CONFIG;
VOLT_RAIL_DYNAMIC_UPDATE_MOCK_CONFIG         voltRailDynamilwpdate_MOCK_CONFIG;
VOLT_RAIL_ROUND_VOLTAGE_MOCK_CONFIG          voltRailRoundVoltage_MOCK_CONFIG;
VOLT_RAIL_GET_VOLTAGE_MAX_MOCK_CONFIG        voltRailGetVoltageMax_MOCK_CONFIG;

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief Initializes the mock configuration data for voltRailGetVoltage().
 *
 * Zeros out the structure. Responsibility of the test to provide expected
 * values and return values prior to running tests.
 */
void voltRailGetVoltageMockInit()
{
    memset(&voltRailGetVoltage_MOCK_CONFIG, 0x00, sizeof(voltRailGetVoltage_MOCK_CONFIG));
}

/*!
 * @brief Adds an entry to the voltRailGetVoltage_MOCK_CONFIG data.
 *
 * If the pointer to the expected values is null, the mock function will not
 * check the expected values when the mock function is called.
 *
 * @param[in]  entry            The entry (or call number) for the test.
 * @param[in]  status           Value to return from the mock function
 * @param[in]  pExpectedValues  Expected values to compare against when the mock
 *                              function is called. If null, the mock function
 *                              will skip the comparisons.
 */
void voltRailGetVoltageMockAddEntry
(
    LwU8                                        entry,
    FLCN_STATUS                                 status,
    VOLT_RAIL_GET_VOLTAGE_MOCK_EXPECTED_VALUE  *pExpectedValues
)
{
    UT_ASSERT_TRUE(entry < VOLT_RAIL_GET_VOLTAGE_MOCK_MAX_ENTRIES);
    voltRailGetVoltage_MOCK_CONFIG.entries[entry].status = status;

    if (pExpectedValues != NULL)
    {
        voltRailGetVoltage_MOCK_CONFIG.entries[entry].bCheckExpectedValues = LW_TRUE;
        voltRailGetVoltage_MOCK_CONFIG.entries[entry].expected = *pExpectedValues;
    }
    else
    {
        voltRailGetVoltage_MOCK_CONFIG.entries[entry].bCheckExpectedValues = LW_FALSE;
    }

}

/*!
 * @brief   MOCK implementation of voltRailGetVoltage
 *
 * @details Configuration for this mock interface is controlled via values in
 *          @ref voltRailGetVoltage_MOCK_CONFIG.
 *
 * @param[in]   pRail       Value passed in by function under test.
 * @param[in]   pVoltageuV  Value passed in by function under test.
 *
 * @return      voltRailGetVoltage_MOCK_CONFIG.status.
 */
FLCN_STATUS
voltRailGetVoltage_MOCK
(
    VOLT_RAIL  *pRail,
    LwU32      *pVoltageuV
)
{
    LwU8 entry = voltRailGetVoltage_MOCK_CONFIG.numCalled;
    voltRailGetVoltage_MOCK_CONFIG.numCalled++;

    if (voltRailGetVoltage_MOCK_CONFIG.entries[entry].bCheckExpectedValues)
    {
        UT_ASSERT_EQUAL_PTR(pRail, voltRailGetVoltage_MOCK_CONFIG.entries[entry].expected.pRail);
    }

    return voltRailGetVoltage_MOCK_CONFIG.entries[entry].status;
}

/*!
 * @brief   MOCK implementation of voltRailSetNoiseUnawareVmin.
 *
 * @details Configuration for this mock interface is controlled via values in
 *          @ref voltRailSetNoiseUnawareVmin_MOCK_CONFIG. See
 *          @ref voltRailSetNoiseUnawareVmin_IMPL() for original interface.
 *
 * @param[in]  pList    List containing target voltage for the rails
 *
 * @return     voltRailSetNoiseUnawareVmin_MOCK_CONFIG.status
 */
FLCN_STATUS
voltRailSetNoiseUnawareVmin_MOCK
(
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST    *pRailList
)
{
    return voltRailSetNoiseUnawareVmin_MOCK_CONFIG.status;
}

/*!
 * @brief   MOCK implementation of voltRailsDynamilwpdate.
 *
 * @details Configuration for this mock interface is controlled via values in
 *          @ref voltRailsDynamilwpdate_MOCK_CONFIG. See
 *          @ref voltRailsDynamilwpdate_IMPL() for original interface.
 *
 * @return     voltRailsDynamilwpdate_MOCK_CONFIG.status
 */
FLCN_STATUS
voltRailsDynamilwpdate_MOCK(LwBool bVfeEvaluate)
{
    return voltRailsDynamilwpdate_MOCK_CONFIG.status;
}

/*!
 * @brief   MOCK implementation of voltRailDynamilwpdate.
 *
 * @details Configuration for this mock interface is controlled via values in
 *          @ref voltRailDynamilwpdate_MOCK_CONFIG. See
 *          @ref voltRailDynamilwpdate_IMPL() for original interface.
 *
 * @return     voltRailDynamilwpdate_MOCK_CONFIG.status
 */
FLCN_STATUS
voltRailDynamilwpdate_MOCK
(
    VOLT_RAIL  *pRail,
    LwBool      bVfeEvaluate
)
{
    return voltRailDynamilwpdate_MOCK_CONFIG.status;
}

/*!
 * @brief MOCK implementation of round voltage interface.
 *
 * @param[in]     pRail         VOLT_RAIL object pointer
 * @param[in/out] pVoltageuV    Rounded value
 * @param[in]     bRoundUp      Boolean to round up or down
 * @param[in]     bBound
 *      Boolean flag indicating whether the rounded value should be bound to
 *      the range of voltages supported on the regulator.  If this flag is
 *      LW_FALSE and the provided value is outside the range, the value will
 *      be rounded (if possible) but outside the range of supported voltages.
 *
 * @return FLCN_OK
 *     Voltage successfully rounded to a supported value.
 */
FLCN_STATUS
voltRailRoundVoltage_MOCK
(
    VOLT_RAIL  *pRail,
    LwS32      *pVoltageuV,
    LwBool      bRoundUp,
    LwBool      bBound
)
{
    *pVoltageuV = voltRailRoundVoltage_MOCK_CONFIG.roundedVoltageuV;
    return voltRailRoundVoltage_MOCK_CONFIG.status;
}

/*!
 * @brief MOCK implementation of get max voltage limit interface.
 *
 * @param[in]     pRail         VOLT_RAIL object pointer
 * @param[in/out] pVoltageuV    Rounded value
 * @param[in]     bRoundUp      Boolean to round up or down
 * @param[in]     bBound
 *      Boolean flag indicating whether the rounded value should be bound to
 *      the range of voltages supported on the regulator.  If this flag is
 *      LW_FALSE and the provided value is outside the range, the value will
 *      be rounded (if possible) but outside the range of supported voltages.
 *
 * @return FLCN_OK
 *     Voltage successfully rounded to a supported value.
 */
FLCN_STATUS
voltRailGetVoltageMax_MOCK
(
    VOLT_RAIL  *pRail,
    LwU32     *pMaxLimituV
)
{
    *pMaxLimituV = voltRailGetVoltageMax_MOCK_CONFIG.maxLimituV;
    return voltRailGetVoltageMax_MOCK_CONFIG.status;
}

DEFINE_FAKE_VALUE_FUNC(FLCN_STATUS, voltRailGetVoltageInternal_MOCK, VOLT_RAIL *, LwU32 *);
DEFINE_FAKE_VALUE_FUNC(FLCN_STATUS, voltRailSetVoltage_MOCK, VOLT_RAIL *, LwU32, LwBool, LwBool, LwU8);
