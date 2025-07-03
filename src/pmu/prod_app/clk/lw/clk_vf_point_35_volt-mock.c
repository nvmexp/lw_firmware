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
 * @file clk_vf_point_35_volt-mock.c
 * @brief Mock implementation for clk_vf_point_35_volt public interface.
 */

#include "test-macros.h"
#include "pmu_objclk.h"
#include "clk/clk_vf_point_35_volt.h"
#include "clk/clk_vf_point_35_volt-mock.h"

#define LOAD_MAX_ENTRIES                                                    (1U)
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
void clkVfPointLoad_35_VOLTMockInit()
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
 * @param[in]  entry      The entry (call number) of the entry to populate.
 * @param[in]  status     The return value.
 */
void clkVfPointLoad_35_VOLTMockAddEntry(LwU8 entry, FLCN_STATUS status)
{
    UT_ASSERT_TRUE(entry < LOAD_MAX_ENTRIES);
    load_MOCK_CONFIG.entries[entry].status = status;
}

/*!
 * @brief Mock implementation of clkVfPointLoad_35_VOLT().
 *
 * The return type and values are dictated by getVoltage_MOCK_CONFIG.
 */
FLCN_STATUS
clkVfPointLoad_35_VOLT_MOCK
(
    CLK_VF_POINT            *pVfPoint,
    CLK_PROG_3X_PRIMARY      *pProg3XPrimary,
    CLK_DOMAIN_3X_PRIMARY    *pDomain3XPrimary,
    LwU8                     voltRailIdx,
    LwU8                     lwrveIdx
)
{
    LwU8 entry = load_MOCK_CONFIG.numCalls;
    ++load_MOCK_CONFIG.numCalls;
    return load_MOCK_CONFIG.entries[entry].status;
}


#define GET_VOLTAGE_MAX_ENTRIES                                             (1U)
typedef struct
{
    LwU8 numCalls;
    struct
    {
        FLCN_STATUS status;
        LwU32 voltageuV;
    } entries[GET_VOLTAGE_MAX_ENTRIES];
} GET_VOLTAGE_MOCK_CONFIG;
static GET_VOLTAGE_MOCK_CONFIG getVoltage_MOCK_CONFIG;

/*!
 * @brief Initializes the GET_VOLTAGE_MOCK_CONFIG data.
 */
void clkVfPointVoltageuVGet_35_VOLTMockInit()
{
    memset(&getVoltage_MOCK_CONFIG, 0x00, sizeof(getVoltage_MOCK_CONFIG));
    for (LwU8 i = 0; i < GET_VOLTAGE_MAX_ENTRIES; ++i)
    {
        getVoltage_MOCK_CONFIG.entries[i].status = FLCN_ERROR;
    }
}

/*!
 * @brief Add entry into the mock configuration data.
 *
 * @param[in]  entry      The entry (call number) of the entry to populate.
 * @param[in]  status     The return value.
 * @param[in]  voltageuV  The value to populate in the output parameter.
 */
void clkVfPointVoltageuVGet_35_VOLTMockAddEntry(LwU8 entry, FLCN_STATUS status, LwU32 voltageuV)
{
    UT_ASSERT_TRUE(entry < GET_VOLTAGE_MAX_ENTRIES);
    getVoltage_MOCK_CONFIG.entries[entry].status = status;
    getVoltage_MOCK_CONFIG.entries[entry].voltageuV = voltageuV;
}

/*!
 * @brief Mock implementation of clkVfPointVoltageuVGet_35_VOLT().
 *
 * The return type and values are dictated by getVoltage_MOCK_CONFIG.
 */
FLCN_STATUS
clkVfPointVoltageuVGet_35_VOLT_MOCK
(
    CLK_VF_POINT  *pVfPoint,
    LwU8           voltageType,
    LwU32         *pVoltageuV
)
{
    LwU8 entry = getVoltage_MOCK_CONFIG.numCalls;
    ++getVoltage_MOCK_CONFIG.numCalls;

    *pVoltageuV = getVoltage_MOCK_CONFIG.entries[entry].voltageuV;
    return getVoltage_MOCK_CONFIG.entries[entry].status;
}
