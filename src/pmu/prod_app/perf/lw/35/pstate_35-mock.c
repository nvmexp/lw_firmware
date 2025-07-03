/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pstate_35-mock.c
 * @brief   Mock implementations of pstate_35 public interfaces.
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "perf/35/pstate_35.h"
#include "perf/35/pstate_35-mock.h"

/* ------------------------ boardObjDynamicCast() --------------------------- */
#define CLK_FREQ_GET_35_MOCK_MAX_ENTRIES                                   (16U)

typedef struct
{
    LwU8 numCalls;
    struct
    {
        FLCN_STATUS status;
        LwBool      bSetup;
    } entries[CLK_FREQ_GET_35_MOCK_MAX_ENTRIES];
} CLK_FREQ_GET_35_MOCK_CONFIG;

static CLK_FREQ_GET_35_MOCK_CONFIG clk_freq_get_35_MOCK_CONFIG = { 0 };

/*!
 * @brief Initializes the mock configuration data for perfPstateClkFreqGet_35().
 *
 * Sets the return value to FLCN_ERROR for each entry. It is the responsibility
 * of the test to provide the mock return values prior to running the test.
 */
void perfPstateClkFreqGet35MockInit()
{
    memset(&clk_freq_get_35_MOCK_CONFIG, 0x00, sizeof(clk_freq_get_35_MOCK_CONFIG));
    for (LwU8 i = 0; i < CLK_FREQ_GET_35_MOCK_MAX_ENTRIES; ++i)
    {
        clk_freq_get_35_MOCK_CONFIG.entries[i].status   = FLCN_ERROR;
        clk_freq_get_35_MOCK_CONFIG.entries[i].bSetup   = LW_FALSE;
    }
}

/*!
 * @brief Adds an entry to the mock data for boardObjDynamicCast().
 *
 * @param[in]  entry    The entry (or call number) for the test.
 * @param[in]  status   Value to return from the mock function.
 */
void perfPstateClkFreqGet35MockAddEntry
(
    LwU8        entry,
    FLCN_STATUS status
)
{
    UT_ASSERT_TRUE(entry < CLK_FREQ_GET_35_MOCK_MAX_ENTRIES);
    clk_freq_get_35_MOCK_CONFIG.entries[entry].status   = status;
    clk_freq_get_35_MOCK_CONFIG.entries[entry].bSetup   = LW_TRUE;
}

LwU8 perfPstateClkFreqGet35MockNumCalls()
{
    return clk_freq_get_35_MOCK_CONFIG.numCalls;
}

/*!
 * @brief Mock implementation of perfPstateClkFreqGet_35().
 *
 * @details Configuration for this mock interface is controlled via values in
 *          @ref clk_freq_get_35_MOCK_CONFIG.
 */
FLCN_STATUS
perfPstateClkFreqGet_35_MOCK
(
    PSTATE                                 *pPstate,
    LwU8                                    clkDomainIdx,
    LW2080_CTRL_PERF_PSTATE_CLOCK_ENTRY    *pPstateClkEntry
)
{
    LwU8 entry = clk_freq_get_35_MOCK_CONFIG.numCalls;
    ++clk_freq_get_35_MOCK_CONFIG.numCalls;
    if (clk_freq_get_35_MOCK_CONFIG.entries[entry].bSetup)
    {
        return clk_freq_get_35_MOCK_CONFIG.entries[entry].status;
    }
    else
    {
        return perfPstateClkFreqGet_35_IMPL(pPstate,
                                            clkDomainIdx,
                                            pPstateClkEntry);
    }
}
