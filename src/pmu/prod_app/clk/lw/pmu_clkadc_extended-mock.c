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
 * @file    pmu_clkadc_extended-mock.c
 * @brief   Mock implementations of pmu_clkadc_extended public interfaces.
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "pmu_objclk.h"
#include "clk/pmu_clkadc.h"
#include "clk/pmu_clkadc-mock.h"

/* ------------------------ Macros and Defines ------------------------------ */
#define CLKS_ADC_PROGRAM_MOCK_MAX_ENTRIES                                   (4U)

/* ------------------------ Types Definitions ------------------------------- */
/*!
 * Configuration data for the mocked version of clksAdcProgram().
 */
typedef struct
{
    LwU8 numCalls;
    struct
    {
        FLCN_STATUS status;
    } entries[CLKS_ADC_PROGRAM_MOCK_MAX_ENTRIES];
} CLKS_ADC_PROGRAM_MOCK_CONFIG;

/* ------------------------ Static Variables -------------------------------- */
static CLKS_ADC_PROGRAM_MOCK_CONFIG clksAdcProgram_MOCK_CONFIG;

/* ------------------------ Utility Functions ------------------------------- */
/*!
 * @brief Initializes the mock configuration data for clksAdcProgram().
 *
 * Zeros out the structure and sets the default return value as FLCN_ERROR.
 * It's the responsibility of the unit tests to provide expected & return
 * values prior to running the test case.
 */
void clksAdcProgramMockInit(void)
{
    memset(&clksAdcProgram_MOCK_CONFIG, 0x00, sizeof(clksAdcProgram_MOCK_CONFIG));
    for (LwU8 i = 0; i < CLKS_ADC_PROGRAM_MOCK_MAX_ENTRIES; ++i)
    {
        clksAdcProgram_MOCK_CONFIG.entries[i].status = FLCN_ERROR;
    }
}

/*!
 * @brief Adds an entry to the clksAdcProgramMockInit_MOCK_CONFIG data.
 *
 * @param[in]  entry   The entry (or call number) for the test.
 * @param[in]  status  Value to return from the mock function.
 */
void clksAdcProgramMockAddEntry(LwU8 entry, FLCN_STATUS status)
{
    UT_ASSERT_TRUE(entry < CLKS_ADC_PROGRAM_MOCK_MAX_ENTRIES);
    clksAdcProgram_MOCK_CONFIG.entries[entry].status = status;
}

/* ------------------------ Mocked Functions -------------------------------- */
/*!
 * @brief MOCK implementation of clksAdcProgram().
 *
 * @details Configuration for this mock interface is controlled via values in
 *          @ref clksAdcProgram_MOCK_CONFIG.
 *
 */
FLCN_STATUS
clkAdcsProgram_MOCK(LW2080_CTRL_CLK_ADC_SW_OVERRIDE_LIST *pAdcSwOverrideList)
{
    LwU8 entry = clksAdcProgram_MOCK_CONFIG.numCalls;
    ++clksAdcProgram_MOCK_CONFIG.numCalls;

    return clksAdcProgram_MOCK_CONFIG.entries[entry].status;
}
