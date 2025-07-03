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
 * @file    voltrail-mock.h
 * @brief   Data required for configuring mock VOLT_RAIL interfaces.
 */

#ifndef VOLTRAIL_MOCK_H
#define VOLTRAIL_MOCK_H

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

#include "fff.h"

/* ------------------------ Application Includes ---------------------------- */
/* ------------------------ Forward Definitions ----------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
#define VOLT_RAIL_GET_VOLTAGE_MOCK_MAX_ENTRIES                              32

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */

/* ------------------------ Types Definitions ------------------------------- */
/*!
 * Configuration variables for @ref voltRailSetNoiseUnawareVmin_MOCK.
 */
typedef struct VOLT_RAIL_SET_NOISE_UNAWARE_VMIN_MOCK_CONFIG
{
    /*!
     * Mock returned status.
     */
    FLCN_STATUS status;
} VOLT_RAIL_SET_NOISE_UNAWARE_VMIN_MOCK_CONFIG;

/*!
 * Expected values passed in by the function under test.
 */
typedef struct
{
    VOLT_RAIL  *pRail;
} VOLT_RAIL_GET_VOLTAGE_MOCK_EXPECTED_VALUE;

/*!
 * Configuration data for the mock version of boardObjGrpMaskImport_FUNC().
 */
typedef struct
{
    LwU8 numCalled;
    struct
    {
        LwBool                                    bCheckExpectedValues;
        FLCN_STATUS                               status;
        VOLT_RAIL_GET_VOLTAGE_MOCK_EXPECTED_VALUE expected;
    } entries[VOLT_RAIL_GET_VOLTAGE_MOCK_MAX_ENTRIES];
} VOLT_RAIL_GET_VOLTAGE_MOCK_CONFIG;

/*!
 * Configuration variables for @ref voltRailsDynamilwpdate_MOCK.
 */
typedef struct VOLT_RAILS_DYNAMIC_UPDATE_MOCK_CONFIG
{
    /*!
     * Mock return status
     */
    FLCN_STATUS status;
} VOLT_RAILS_DYNAMIC_UPDATE_MOCK_CONFIG;

/*!
 * Configuration variables for @ref voltRailDynamilwpdate_MOCK.
 */
typedef struct VOLT_RAIL_DYNAMIC_UPDATE_MOCK_CONFIG
{
    /*!
     * Mock return status
     */
    FLCN_STATUS status;
} VOLT_RAIL_DYNAMIC_UPDATE_MOCK_CONFIG;

/*!
 * Configuration variables for @ref voltRailRoundVoltage_MOCK.
 */
typedef struct VOLT_RAIL_ROUND_VOLTAGE_MOCK_CONFIG
{
    /*!
     * Mocked value of rounded voltage in uV.
     */
    LwS32       roundedVoltageuV;

    /*!
     * Mock return status
     */
    FLCN_STATUS status;
} VOLT_RAIL_ROUND_VOLTAGE_MOCK_CONFIG;

/*!
 * Configuration variables for @ref voltRailGetVoltageMax_MOCK.
 */
typedef struct VOLT_RAIL_GET_VOLTAGE_MAX_MOCK_CONFIG
{
    /*!
     * Mocked value of max voltage limit in uV.
     */
    LwU32       maxLimituV;

    /*!
     * Mock return status
     */
    FLCN_STATUS status;
} VOLT_RAIL_GET_VOLTAGE_MAX_MOCK_CONFIG;

/* ------------------------ Structures ---------------------------- */
/* ------------------------ Extern Variables -------------------------------- */
extern VOLT_RAIL_GET_VOLTAGE_MOCK_CONFIG            voltRailGetVoltage_MOCK_CONFIG;
extern VOLT_RAIL_SET_NOISE_UNAWARE_VMIN_MOCK_CONFIG voltRailSetNoiseUnawareVmin_MOCK_CONFIG;
extern VOLT_RAILS_DYNAMIC_UPDATE_MOCK_CONFIG        voltRailsDynamilwpdate_MOCK_CONFIG;
extern VOLT_RAIL_DYNAMIC_UPDATE_MOCK_CONFIG         voltRailDynamilwpdate_MOCK_CONFIG;
extern VOLT_RAIL_ROUND_VOLTAGE_MOCK_CONFIG          voltRailRoundVoltage_MOCK_CONFIG;
extern VOLT_RAIL_GET_VOLTAGE_MAX_MOCK_CONFIG        voltRailGetVoltageMax_MOCK_CONFIG;

/* ------------------------ Public Functions -------------------------------- */
void voltRailGetVoltageMockInit();
void voltRailGetVoltageMockAddEntry(LwU8 entry, FLCN_STATUS status, VOLT_RAIL_GET_VOLTAGE_MOCK_EXPECTED_VALUE *pExpectedValues);

DECLARE_FAKE_VALUE_FUNC(FLCN_STATUS, voltRailGetVoltageInternal_MOCK, VOLT_RAIL *, LwU32 *);
DECLARE_FAKE_VALUE_FUNC(FLCN_STATUS, voltRailSetVoltage_MOCK, VOLT_RAIL *, LwU32, LwBool, LwBool, LwU8);

#endif // VOLTRAIL_MOCK_H
