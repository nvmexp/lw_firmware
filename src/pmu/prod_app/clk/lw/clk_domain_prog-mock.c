/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file clk_domain_prog-mock.c
 * @brief Mock implementation for clk_domain_prog public interface.
 */

#include "test-macros.h"
#include "pmu_objclk.h"
#include "clk/clk_domain_prog.h"
#include "clk/clk_domain_prog-mock.h"

#define DOMAIN_PROG_IS_NOISE_AWARE_MAX_ENTRIES                    (1U)
typedef struct
{
    LwU8 numCalls;
    struct
    {
        FLCN_STATUS status;
        clkDomainProgIsNoiseAware_MOCK_CALLBACK *pCallback;
    } entries[DOMAIN_PROG_IS_NOISE_AWARE_MAX_ENTRIES];
} DOMAIN_PROG_IS_NOISE_AWARE_MOCK_CONFIG;
static DOMAIN_PROG_IS_NOISE_AWARE_MOCK_CONFIG domain_prog_is_noise_aware_MOCK_CONFIG;

/*!
 * @brief Initializes the DOMAIN_PROG_IS_NOISE_AWARE_MOCK_CONFIG data.
 */
void clkDomainProgIsNoiseAwareMockInit(void)
{
    memset(&domain_prog_is_noise_aware_MOCK_CONFIG, 0x00, sizeof(domain_prog_is_noise_aware_MOCK_CONFIG));
    for (LwU8 i = 0; i < DOMAIN_PROG_IS_NOISE_AWARE_MAX_ENTRIES; ++i)
    {
        domain_prog_is_noise_aware_MOCK_CONFIG.entries[i].status = FLCN_ERROR;
        domain_prog_is_noise_aware_MOCK_CONFIG.entries[i].pCallback = NULL;
    }
}

/*!
 * @brief Add entry into the mock configuration data.
 *
 * @param[in]  entry      The entry (call number) of the entry to populate.
 * @param[in]  status     The return value.
 */
void clkDomainProgIsNoiseAwareMockAddEntry(LwU8 entry, FLCN_STATUS status)
{
    UT_ASSERT_TRUE(entry < DOMAIN_PROG_IS_NOISE_AWARE_MAX_ENTRIES);
    domain_prog_is_noise_aware_MOCK_CONFIG.entries[entry].status = status;
}

/*!
 * @brief Adds an entry to the mock data for clkDomainProgIsNoiseAware_StubWithCallback().
 *
 * This is the approach to use when the tests wants to either use the production
 * function or have a function return a value via a parameter.
 *
 * @param entry      The entry (or call number) for the test.
 * @param pCallback  Function to call.
 */
void clkDomainProgIsNoiseAware_StubWithCallback(LwU8 entry, clkDomainProgIsNoiseAware_MOCK_CALLBACK *pCallback)
{
    UT_ASSERT_TRUE(entry < DOMAIN_PROG_IS_NOISE_AWARE_MAX_ENTRIES);
    domain_prog_is_noise_aware_MOCK_CONFIG.entries[entry].pCallback = pCallback;
}

/*!
 * @brief Mock implementation of clkDomainProgIsNoiseAware().
 *
 * The return type and values are dictated by domain_prog_is_noise_aware_MOCK_CONFIG.
 */
FLCN_STATUS
clkDomainProgIsNoiseAware_MOCK
(
    CLK_DOMAIN_PROG  *pDomainProg,
    LwBool           *pbIsNoiseAware
)
{
    LwU8 entry = domain_prog_is_noise_aware_MOCK_CONFIG.numCalls;
    ++domain_prog_is_noise_aware_MOCK_CONFIG.numCalls;

    if (domain_prog_is_noise_aware_MOCK_CONFIG.entries[entry].pCallback != NULL)
    {
        return domain_prog_is_noise_aware_MOCK_CONFIG.entries[entry].pCallback(pDomainProg, pbIsNoiseAware);
    }
    else
    {
        return domain_prog_is_noise_aware_MOCK_CONFIG.entries[entry].status;
    }
}
