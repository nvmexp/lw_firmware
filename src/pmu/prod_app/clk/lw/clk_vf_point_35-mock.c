/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file clk_vf_point_35-mock.c
 * @brief Mock implementation for clk_vf_point_35 public interface.
 */

#include "test-macros.h"
#include "pmu_objclk.h"
#include "clk/clk_vf_point_35.h"
#include "clk/clk_vf_point_35-mock.h"

#define CONSTRUCT_MAX_ENTRIES                                               (2U)
typedef struct
{
    LwU8 numCalls;
    struct
    {
        FLCN_STATUS status;
        BOARDOBJ *pBoardObj;
        clkVfPointConstruct_35_MOCK_CALLBACK *pCallback;
    } entries[CONSTRUCT_MAX_ENTRIES];
} CONSTRUCT_MOCK_CONFIG;
static CONSTRUCT_MOCK_CONFIG construct_MOCK_CONFIG;

/*!
 * @brief Initializes the CONSTRUCT_MOCK_CONFIG data.
 */
void clkVfPointConstruct_35MockInit()
{
    memset(&construct_MOCK_CONFIG, 0x00, sizeof(construct_MOCK_CONFIG));
    for (LwU8 i = 0; i < CONSTRUCT_MAX_ENTRIES; ++i)
    {
        construct_MOCK_CONFIG.entries[i].status = FLCN_ERROR;
        construct_MOCK_CONFIG.entries[i].pCallback = NULL;
    }
}

/*!
 * @brief Add entry into the mock configuration data.
 *
 * @param[in]  entry      The entry (call number) of the entry to populate.
 * @param[in]  status     The return value.
 * @param[in]  pBoardObj  Pointer to pre-allocated space to return as boardObj.
 */
void clkVfPointConstruct_35MockAddEntry(LwU8 entry, FLCN_STATUS status, BOARDOBJ *pBoardObj)
{
    UT_ASSERT_TRUE(entry < CONSTRUCT_MAX_ENTRIES);
    construct_MOCK_CONFIG.entries[entry].status = status;
    construct_MOCK_CONFIG.entries[entry].pBoardObj = pBoardObj;
}

/*!
 * @brief Adds an entry to the mock data for clkVfPointGrpIfaceModel10ObjSet_35().
 *
 * This is the approach to use when the tests wants to either use the production
 * function or have a function return a value via a parameter.
 *
 * @param entry      The entry (or call number) for the test.
 * @param pCallback  Function to call.
 */
void clkVfPointConstruct_35_StubWithCallback(LwU8 entry, clkVfPointConstruct_35_MOCK_CALLBACK *pCallback)
{
    UT_ASSERT_TRUE(entry < CONSTRUCT_MAX_ENTRIES);
    construct_MOCK_CONFIG.entries[entry].pCallback = pCallback;
}

/*!
 * @brief Mock implementation of clkVfPointGrpIfaceModel10ObjSet_35().
 *
 * The return type and values are dictated by construct_MOCK_CONFIG.
 */
FLCN_STATUS
clkVfPointGrpIfaceModel10ObjSet_35_MOCK
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    LwU8 entry = construct_MOCK_CONFIG.numCalls;
    ++construct_MOCK_CONFIG.numCalls;

    if (construct_MOCK_CONFIG.entries[entry].pCallback != NULL)
    {
        return construct_MOCK_CONFIG.entries[entry].pCallback(pBObjGrp, ppBoardObj, size, pBoardObjDesc);
    }
    else
    {
        *ppBoardObj = construct_MOCK_CONFIG.entries[entry].pBoardObj;
        return construct_MOCK_CONFIG.entries[entry].status;
    }
}


#define GET_VOLTAGE_MAX_ENTRIES                                             (1U)
typedef struct
{
    LwU8 numCalls;
    struct
    {
        FLCN_STATUS status;
        clkVfPointVoltageuVGet_35_MOCK_CALLBACK *pCallback;
    } entries[GET_VOLTAGE_MAX_ENTRIES];
} GET_VOLTAGE_MOCK_CONFIG;
static GET_VOLTAGE_MOCK_CONFIG getVoltage_MOCK_CONFIG;

/*!
 * @brief Initializes the GET_VOLTAGE_MOCK_CONFIG data.
 */
void clkVfPointVoltageuVGet_35_MockInit()
{
    memset(&getVoltage_MOCK_CONFIG, 0x00, sizeof(getVoltage_MOCK_CONFIG));
    for (LwU8 i = 0; i < GET_VOLTAGE_MAX_ENTRIES; ++i)
    {
        getVoltage_MOCK_CONFIG.entries[i].status = FLCN_ERROR;
        getVoltage_MOCK_CONFIG.entries[i].pCallback = NULL;
    }
}

/*!
 * @brief Add entry into the mock configuration data.
 *
 * @param   entry   The entry (call number) of the entry to populate.
 * @param   status  The return value.
 */
void clkVfPointVoltageuVGet_35_MockAddEntry(LwU8 entry, FLCN_STATUS status)
{
    UT_ASSERT_TRUE(entry < GET_VOLTAGE_MAX_ENTRIES);
    getVoltage_MOCK_CONFIG.entries[entry].status = status;
}

/*!
 * @brief Adds an entry to the mock data for clkVfPointVoltageuVGet_35().
 *
 * This is the approach to use when the tests wants to either use the production
 * function or have a function return a value via a parameter.
 *
 * @param   entry       The entry (or call number) for the test.
 * @param   pCallback   Function to call.
 */
void clkVfPointVoltageuVGet_35_StubWithCallback(LwU8 entry, clkVfPointVoltageuVGet_35_MOCK_CALLBACK *pCallback)
{
    UT_ASSERT_TRUE(entry < GET_VOLTAGE_MAX_ENTRIES);
    getVoltage_MOCK_CONFIG.entries[entry].pCallback = pCallback;
}

/*!
 * @brief Mock implementation of clkVfPointVoltageuVGet_35().
 *
 * The return type and values are dictated by getVoltage_MOCK_CONFIG.
 */
FLCN_STATUS
clkVfPointVoltageuVGet_35_MOCK
(
    CLK_VF_POINT  *pVfPoint,
    LwU8           voltageType,
    LwU32         *pVoltageuV
)
{
    LwU8 entry = getVoltage_MOCK_CONFIG.numCalls;
    ++getVoltage_MOCK_CONFIG.numCalls;

    if (getVoltage_MOCK_CONFIG.entries[entry].pCallback != NULL)
    {
        return getVoltage_MOCK_CONFIG.entries[entry].pCallback(pVfPoint, voltageType, pVoltageuV);
    }
    else
    {
        return getVoltage_MOCK_CONFIG.entries[entry].status;
    }
}


#define GET_STATUS_MAX_ENTRIES                                              (1U)
typedef struct
{
    LwU8 numCalls;
    struct
    {
        FLCN_STATUS status;
        clkVfPointGetStatus_35_MOCK_CALLBACK *pCallback;
    } entries[GET_STATUS_MAX_ENTRIES];
} GET_STATUS_MOCK_CONFIG;
static GET_STATUS_MOCK_CONFIG getStatus_MOCK_CONFIG;

/*!
 * @brief Initializes the GET_STATUS_MOCK_CONFIG data.
 */
void clkVfPointGetStatus_35_MockInit()
{
    memset(&getStatus_MOCK_CONFIG, 0x00, sizeof(getStatus_MOCK_CONFIG));
    for (LwU8 i = 0; i < GET_STATUS_MAX_ENTRIES; ++i)
    {
        getStatus_MOCK_CONFIG.entries[i].status = FLCN_ERROR;
        getStatus_MOCK_CONFIG.entries[i].pCallback = NULL;
    }
}

/*!
 * @brief Add entry into the mock configuration data.
 *
 * @param   entry   The entry (call number) of the entry to populate.
 * @param   status  The return value.
 */
void clkVfPointGetStatus_35_MockAddEntry(LwU8 entry, FLCN_STATUS status)
{
    UT_ASSERT_TRUE(entry < GET_VOLTAGE_MAX_ENTRIES);
    getStatus_MOCK_CONFIG.entries[entry].status = status;
}

/*!
 * @brief Adds an entry to the mock data for clkVfPointVoltageuVGet_35().
 *
 * This is the approach to use when the tests wants to either use the production
 * function or have a function return a value via a parameter.
 *
 * @param   entry       The entry (or call number) for the test.
 * @param   pCallback   Function to call.
 */
void clkVfPointGetStatus_35_StubWithCallback(LwU8 entry, clkVfPointGetStatus_35_MOCK_CALLBACK *pCallback)
{
    UT_ASSERT_TRUE(entry < GET_VOLTAGE_MAX_ENTRIES);
    getStatus_MOCK_CONFIG.entries[entry].pCallback = pCallback;
}

/*!
 * @brief Mock implementation of clkVfPointIfaceModel10GetStatus_35().
 *
 * The return type and values are dictated by getStatus_MOCK_CONFIG.
 */
FLCN_STATUS
clkVfPointIfaceModel10GetStatus_35_MOCK
(
    BOARDOBJ_IFACE_MODEL_10              *pModel10,
    RM_PMU_BOARDOBJ       *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    LwU8 entry = getStatus_MOCK_CONFIG.numCalls;
    ++getStatus_MOCK_CONFIG.numCalls;

    if (getStatus_MOCK_CONFIG.entries[entry].pCallback != NULL)
    {
        return getStatus_MOCK_CONFIG.entries[entry].pCallback(pBoardObj, pPayload);
    }
    else
    {
        return getStatus_MOCK_CONFIG.entries[entry].status;
    }
}

#define CACHE_MAX_ENTRIES                                                   (4U)
typedef struct
{
    LwU8 numCalls;
    struct
    {
        FLCN_STATUS status;
    } entries[CACHE_MAX_ENTRIES];
} CACHE_MOCK_CONFIG;
static CACHE_MOCK_CONFIG cache_MOCK_CONFIG;

/*!
 * @brief Initializes the CACHE_MOCK_CONFIG data.
 */
void clkVfPoint35CacheMockInit()
{
    memset(&cache_MOCK_CONFIG, 0x00, sizeof(cache_MOCK_CONFIG));
    for (LwU8 i = 0; i < CACHE_MAX_ENTRIES; ++i)
    {
        cache_MOCK_CONFIG.entries[i].status = FLCN_ERROR;
    }
}

/*!
 * @brief Add entry into the mock configuration data.
 *
 * @param  entry   The entry (call number) of the entry to populate.
 * @param  status  The return value.
 */
void clkVfPoint35CacheMockAddEntry(LwU8 entry, FLCN_STATUS status)
{
    UT_ASSERT_TRUE(entry < CACHE_MAX_ENTRIES);
    cache_MOCK_CONFIG.entries[entry].status = status;
}

/*!
 * @brief Mock implementation of clkVfPoint35Cache().
 *
 * The return type and values are dictated by cache_MOCK_CONFIG.
 */
FLCN_STATUS
clkVfPoint35Cache_MOCK
(
    CLK_VF_POINT_35            *pVfPoint35,
    CLK_VF_POINT_35            *pVfPoint35Last,
    CLK_DOMAIN_35_PRIMARY       *pDomain35Primary,
    CLK_PROG_35_PRIMARY         *pProg35Primary,
    LwU8                        voltRailIdx,
    LwU8                        lwrveIdx,
    LwBool                      bVFEEvalRequired
)
{
    LwU8 entry = cache_MOCK_CONFIG.numCalls;
    ++cache_MOCK_CONFIG.numCalls;

    return cache_MOCK_CONFIG.entries[entry].status;
}


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
void clkVfPointLoadMockInit()
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
 * @param  entry   The entry (call number) of the entry to populate.
 * @param  status  The return value.
 */
void clkVfPointLoadMockAddEntry(LwU8 entry, FLCN_STATUS status)
{
    UT_ASSERT_TRUE(entry < LOAD_MAX_ENTRIES);
    load_MOCK_CONFIG.entries[entry].status = status;
}

/*!
 * @brief Mock implementation of clkVfPointLoad().
 *
 * The return type and values are dictated by load_MOCK_CONFIG.
 */
FLCN_STATUS
clkVfPointLoad_MOCK
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
