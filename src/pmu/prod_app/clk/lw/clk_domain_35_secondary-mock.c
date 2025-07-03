/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file clk_domain_35_secondary_base-mock.c
 * @brief Mock implementation for clk_domain_35_secondary public interface.
 */

#include "test-macros.h"
#include "pmu_objclk.h"
#include "clk/clk_domain_35_secondary.h"
#include "clk/clk_domain_35_secondary-mock.h"

#define VOLT_TO_FREQ_TUPLE_MAX_ENTRIES                                      (1U)

typedef struct
{
    LwU8 numCalls;
    struct
    {
        FLCN_STATUS status;
    } entries[VOLT_TO_FREQ_TUPLE_MAX_ENTRIES];
} VOLT_TO_FREQ_TUPLE_MOCK_CONFIG;

static VOLT_TO_FREQ_TUPLE_MOCK_CONFIG voltToFreqTuple_MOCK_CONFIG;


/*!
 * @brief Initializes the VOLT_TO_FREQ_TUPLE_MOCK_CONFIG_DATA.
 */
void clkDomainProgVoltToFreqTuple_35_SECONDARYMockInit(void)
{
    memset(&voltToFreqTuple_MOCK_CONFIG, 0x00, sizeof(voltToFreqTuple_MOCK_CONFIG));
    for (LwU8 i = 0; i < VOLT_TO_FREQ_TUPLE_MAX_ENTRIES; ++i)
    {
        voltToFreqTuple_MOCK_CONFIG.entries[i].status = FLCN_ERROR;
    }
}

/*!
 * @brief Add entry into the mock configuration data.
 *
 * @param[in]  entry      The entry (call number) of the entry to populate.
 * @param[in]  status     The return value.
 */
void clkDomainProgVoltToFreqTuple_35_SECONDARYMockAddEntry(LwU8 entry, FLCN_STATUS status)
{
    UT_ASSERT_TRUE(entry < VOLT_TO_FREQ_TUPLE_MAX_ENTRIES);
    voltToFreqTuple_MOCK_CONFIG.entries[entry].status = status;
}

/*!
 * @brief Mock implementation of clkDomainProgVoltToFreqTuple_35_PRIMARY().
 *
 * The return type and values are dictated by voltToFreqTuple_MOCK_CONFIG.
 */
FLCN_STATUS
clkDomainProgVoltToFreqTuple_35_SECONDARY_MOCK
(
    CLK_DOMAIN_PROG                                *pDomainProg,
    LwU8                                            voltRailIdx,
    LwU8                                            voltageType,
    LW2080_CTRL_CLK_VF_INPUT                       *pInput,
    LW2080_CTRL_CLK_CLK_VF_POINT_LUT_FREQ_TUPLE    *pOutput,
    LW2080_CTRL_CLK_VF_ITERATION_STATE             *pVfIterState
)
{
    LwU8 entry = voltToFreqTuple_MOCK_CONFIG.numCalls;
    ++voltToFreqTuple_MOCK_CONFIG.numCalls;

    return voltToFreqTuple_MOCK_CONFIG.entries[entry].status;
}
