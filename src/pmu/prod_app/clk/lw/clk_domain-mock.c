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
 * @file    clk_domain-mock.c
 * @brief   Mock implementations of clk_domain public interfaces.
 */

/* ------------------------ Application Includes ---------------------------- */
#include "test-macros.h"
#include "pmu_objclk.h"
#include "clk/clk_domain.h"
#include "clk/clk_domain-mock.h"

/* ------------------------ clkDomainLoad() --------------------------------- */
/*!
 * The maximum number of times the mock version of clkDomainLoad()
 * will be called in a single unit test case. Increase the value if more calls
 * are needed.
 */
#define LOAD_MAX_ENTRIES                                                    (4U)

/*!
 * Mock configuration data for clkDomainIsFixed().
 */
typedef struct
{
    LwU8 numCalls;
    struct
    {
        FLCN_STATUS status;
    } entries[LOAD_MAX_ENTRIES];
} LOAD_MOCK_CONFIG;
static LOAD_MOCK_CONFIG load_MOCK_CONFIG;

/*!
 * @brief Initializes the LOAD_MOCK_CONFIG data.
 */
void clkDomainLoadMockInit(void)
{
    memset(&load_MOCK_CONFIG, 0x00, sizeof(load_MOCK_CONFIG));
    for (LwU8 i = 0; i < LOAD_MAX_ENTRIES; ++i)
    {
        load_MOCK_CONFIG.entries[i].status = FLCN_ERROR;
    }
}

/*!
 * @brief Add entry into the mock configuration data.
 *
 * @param   entry   The entry (call number) of the entry to populate.
 * @param   status  The return value.
 */
void clkDomainLoadMockAddEntry(LwU8 entry, FLCN_STATUS status)
{
    UT_ASSERT_TRUE(entry < LOAD_MAX_ENTRIES);
    load_MOCK_CONFIG.entries[entry].status = status;
}

/*!
 * @brief Mock implementation of clkDomainLoad().
 *
 * The return type and values are dictated by load_MOCK_CONFIG.
 */
FLCN_STATUS
clkDomainLoad_MOCK(CLK_DOMAIN *pDomain)
{
    LwU8 entry = load_MOCK_CONFIG.numCalls;
    ++load_MOCK_CONFIG.numCalls;

    return load_MOCK_CONFIG.entries[entry].status;
}


/* ------------------------ clkDomainIsFixed() ------------------------------ */
/*!
 * The maximum number of times the mock version of clkDomainIsFixed()
 * will be called in a single unit test case. Increase the value if more calls
 * are needed.
 */
#define CLK_DOMAIN_IS_FIXED_MOCK_MAX_ENTRIES                            (32U)

/*!
 * Mock configuration data for clkDomainIsFixed().
 */
typedef struct
{
    LwU8 numCalls;
    struct
    {
        LwBool bIsFixed;
    } entries[CLK_DOMAIN_IS_FIXED_MOCK_MAX_ENTRIES];
} CLK_DOMAIN_IS_FIXED_MOCK_CONFIG;
static CLK_DOMAIN_IS_FIXED_MOCK_CONFIG clkDomainIsFixed_MOCK_CONFIG;

/*!
 * Helper function to initialize the configuration data for the mock version of
 * clkDomainIsFixed(). The configuration data is setup to return LW_FALSE on
 * each call to the mocked function.
 *
 * Make calls to @ref clkDomainIsFixedMockAddEntry() to properly setup the
 * expected return values from the mocked functions.
 */
void clkDomainIsFixedMockInit(void)
{
    memset(&clkDomainIsFixed_MOCK_CONFIG, 0x00, sizeof(clkDomainIsFixed_MOCK_CONFIG));
    for (LwU8 i = 0; i < CLK_DOMAIN_IS_FIXED_MOCK_MAX_ENTRIES; ++i)
    {
        clkDomainIsFixed_MOCK_CONFIG.entries[i].bIsFixed = LW_FALSE;
    }
}

/*!
 * Helper function to configure individual calls to the mocked function.
 *
 * @param[in]  entry     The zero-based function call index. It must be less than
 *                       @ref CLK_CLOCK_MONS_THRESHOLD_EVALUATE_MOCK_MAX_ENTRIES
 *                       or it will throw an assert during run-time.
 * @param[in]  bIsFixed  The value to return when the function is called on the
 *                       'entry' (zero-based) call.
 */
void clkDomainIsFixedMockAddEntry(LwU8 entry, LwBool bIsFixed)
{
    UT_ASSERT_TRUE(entry < CLK_DOMAIN_IS_FIXED_MOCK_MAX_ENTRIES);
    clkDomainIsFixed_MOCK_CONFIG.entries[entry].bIsFixed = bIsFixed;
}

/* ------------------------ Mocked Functions -------------------------------- */
/*!
 * @brief MOCK implementation of clkDomainIsFixed().
 *
 * @details Configuration for this mock interface is controlled via values in
 *          @ref CLK_DOMAIN_IS_FIXED_MOCK_CONFIG.
 *
 * @param[in]   pClkDomainList  Pointer to the clock domain list
 * @param[in]   pVoltRailList   Pointer to the voltage rail list
 * @param[out]  pStepClkMon     Pointer to the clock monitor step
 *
 * @return clkDomainIsFixed_MOCK_CONFIG.status
 */
LwBool clkDomainIsFixed_MOCK(CLK_DOMAIN *pDomain)
{
    LwU8 entry = clkDomainIsFixed_MOCK_CONFIG.numCalls;
    ++clkDomainIsFixed_MOCK_CONFIG.numCalls;

    return clkDomainIsFixed_MOCK_CONFIG.entries[entry].bIsFixed;
}
