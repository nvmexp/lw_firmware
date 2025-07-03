/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pmu_clknafll_regime_20-mock.c
 * @brief   Mock implementations of pmu_clknafll_regime_20 public interfaces.
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "pmu_objclk.h"
#include "clk/pmu_clknafll_regime_20.h"
#include "clk/pmu_clknafll_regime_10-mock.h"

/* ------------------------ Macros and Defines ------------------------------ */
#define CLK_NAFLL_GRP_PROGRAM_MOCK_MAX_ENTRIES                              (4U)

/* ------------------------ Types Definitions ------------------------------- */
/*!
 * Configuration data for the mocked version of clkNafllGrpProgram().
 */
typedef struct
{
    LwU8 numCalls;
    struct
    {
        FLCN_STATUS status;
    } entries[CLK_NAFLL_GRP_PROGRAM_MOCK_MAX_ENTRIES];
} CLK_NAFLL_GRP_PROGRAM_MOCK_CONFIG;

/* ------------------------ Static Variables -------------------------------- */
static CLK_NAFLL_GRP_PROGRAM_MOCK_CONFIG clkNafllGrpProgram_MOCK_CONFIG;

/* ------------------------ Utility Functions ------------------------------- */
/*!
 * @brief Initializes the mock configuration data for clkNafllGrpProgram().
 *
 * Zeros out the structure and sets the default return value as FLCN_ERROR.
 * It's the responsibility of the unit tests to provide expected & return
 * values prior to running the test case.
 */
void clkNafllGrpProgramMockInit(void)
{
    memset(&clkNafllGrpProgram_MOCK_CONFIG, 0x00, sizeof(clkNafllGrpProgram_MOCK_CONFIG));
    for (LwU8 i = 0; i < CLK_NAFLL_GRP_PROGRAM_MOCK_MAX_ENTRIES; ++i)
    {
        clkNafllGrpProgram_MOCK_CONFIG.entries[i].status = FLCN_ERROR;
    }
}

/*!
 * @brief Adds an entry to the clkNafllGrpProgram_MOCK_CONFIG data.
 *
 * @param[in]  entry   The entry (or call number) for the test.
 * @param[in]  status  Value to return from the mock function.
 */
void clkNafllGrpProgramMockAddEntry(LwU8 entry, FLCN_STATUS status)
{
    UT_ASSERT_TRUE(entry < CLK_NAFLL_GRP_PROGRAM_MOCK_MAX_ENTRIES);
    clkNafllGrpProgram_MOCK_CONFIG.entries[entry].status = status;
}

/* ------------------------ Mocked Functions -------------------------------- */
/*!
 * @brief MOCK implementation of clkNafllGrpProgram().
 *
 * @details Configuration for this mock interface is controlled via values in
 *          @ref clkNafllGrpProgram_MOCK_CONFIG.
 *
 */
FLCN_STATUS clkNafllGrpProgram_MOCK(LW2080_CTRL_CLK_CLK_DOMAIN_LIST *pDomainList)
{
    LwU8 entry = clkNafllGrpProgram_MOCK_CONFIG.numCalls;
    ++clkNafllGrpProgram_MOCK_CONFIG.numCalls;

    return clkNafllGrpProgram_MOCK_CONFIG.entries[entry].status;
}
