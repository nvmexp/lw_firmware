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
 * @file    clk_clockmon-mock.c
 * @brief   Mock implementations of clk_clkmon public interfaces.
 */

/* ------------------------ System Includes --------------------------------- */
#include "test-macros.h"
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "clk/clk_clockmon.h"
#include "clk/clk_clockmon-mock.h"

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * The maximum number of times the mock version of clkClockMonsThresholdEvaluate()
 * will be called in a single unit test case. Increase the value if more calls
 * are needed.
 */
#define CLK_CLOCK_MONS_THRESHOLD_EVALUATE_MOCK_MAX_ENTRIES                  1U

/* ------------------------ Types Definitions ------------------------------- */
/*!
 * Mock configuration data for clkClockMonsThresholdEvaluate.
 */
typedef struct
{
    LwU8 numCalls;
    struct
    {
        FLCN_STATUS status;
    } entries[CLK_CLOCK_MONS_THRESHOLD_EVALUATE_MOCK_MAX_ENTRIES];
} CLK_CLOCK_MONS_THRESHOLD_EVALUATE_MOCK_CONFIG;

/* ------------------------ Global Variables -------------------------------- */
static CLK_CLOCK_MONS_THRESHOLD_EVALUATE_MOCK_CONFIG clkClockMonsThresholdEvaluate_MOCK_CONFIG;

/* ------------------------ Helper Functions -------------------------------- */
/*!
 * Helper function to initialize the configuration data for the mock version of
 * clkClockMonsThresholdEvaluate(). The configuration data is setup to return
 * FLCN_ERROR on each call to the mocked function.
 *
 * Make calls to @ref clkClockMonsThresholdEvaluateMockAddEntry() to properly
 * setup the expected return values from the mocked functions.
 */
void clkClockMonsThresholdEvaluateMockInit(void)
{
    memset(&clkClockMonsThresholdEvaluate_MOCK_CONFIG, 0x00,
        sizeof(clkClockMonsThresholdEvaluate_MOCK_CONFIG));

    for (LwU8 i = 0; i < CLK_CLOCK_MONS_THRESHOLD_EVALUATE_MOCK_MAX_ENTRIES; ++i)
    {
        clkClockMonsThresholdEvaluate_MOCK_CONFIG.entries[i].status = FLCN_ERROR;
    }
}

/*!
 * Helper function to configure individual calls to the mocked function.
 *
 * @param[in]  entry   The zero-based function call index. It must be less than
 *                     @ref CLK_CLOCK_MONS_THRESHOLD_EVALUATE_MOCK_MAX_ENTRIES
 *                     or it will throw an assert during run-time.
 * @param[in]  status  The value to return when the function is called on the
 *                     'entry' (zero-based) call.
 */
void clkClockMonsThresholdEvaluateMockAddEntry(LwU8 entry, FLCN_STATUS status)
{
    UT_ASSERT_TRUE(entry < CLK_CLOCK_MONS_THRESHOLD_EVALUATE_MOCK_MAX_ENTRIES);
    clkClockMonsThresholdEvaluate_MOCK_CONFIG.entries[entry].status = status;
}

/* ------------------------ Mocked Functions -------------------------------- */
/*!
 * @brief MOCK implementation of clkClockMonsThresholdEvaluate
 *
 * @details Configuration for this mock interface is controlled via values in
 *          @ref CLK_CLOCK_MONS_THRESHOLD_EVALUATE_MOCK_CONFIG.
 *
 * @param[in]   pClkDomainList  Pointer to the clock domain list
 * @param[in]   pVoltRailList   Pointer to the voltage rail list
 * @param[out]  pStepClkMon     Pointer to the clock monitor step
 *
 * @return clkClockMonsThresholdEvaluate_MOCK_CONFIG.status
 */
FLCN_STATUS
clkClockMonsThresholdEvaluate_MOCK
(
    LW2080_CTRL_CLK_CLK_DOMAIN_LIST * const                 pClkDomainList,
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST * const                 pVoltRailList,
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_CLK_MON    *pStepClkMon
)
{
    LwU8 entry = clkClockMonsThresholdEvaluate_MOCK_CONFIG.numCalls;
    clkClockMonsThresholdEvaluate_MOCK_CONFIG.numCalls++;

    return clkClockMonsThresholdEvaluate_MOCK_CONFIG.entries[entry].status;
}
