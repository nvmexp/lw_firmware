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
 * @file    voltdev-mock.c
 * @brief   Mock implementations of VOLT_DEV public interfaces.
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

#include "volt/voltdev-mock.h"
#include "volt/voltdev.h"

/* ------------------------ Application Includes ---------------------------- */
/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
VOLT_DEVICE_ROUND_VOLTAGE_MOCK_CONFIG voltDeviceRoundVoltage_MOCK_CONFIG;
VOLT_DEVICE_GET_VOLTAGE_MOCK_CONFIG   voltDeviceGetVoltage_MOCK_CONFIG;
VOLT_DEVICE_SET_VOLTAGE_MOCK_CONFIG   voltDeviceSetVoltage_MOCK_CONFIG;

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief   MOCK implementation of voltDeviceRoundVoltage.
 *
 * @details Configuration for this mock interface is controlled via values in
 *          @ref voltDeviceRoundVoltage_MOCK_CONFIG. See
 *          @ref voltDeviceRoundVoltage_IMPL() for original interface.
 *
 * @param[in]     pDevice       VOLT_DEVICE object pointer
 * @param[in/out] pVoltageuV    Rounded value
 * @param[in]     bRoundUp      Boolean to round up or down
 * @param[in]     bBound
 *      Boolean flag indicating whether the rounded value should be bound to
 *      the range of voltages supported on the regulator.  If this flag is
 *      LW_FALSE and the provided value is outside the range, the value will
 *      be rounded (if possible) but outside the range of supported voltages.
 *
 * @return  voltDeviceRoundVoltage_MOCK_CONFIG.status
 */
FLCN_STATUS
voltDeviceRoundVoltage_MOCK
(
    VOLT_DEVICE    *pDevice,
    LwS32          *pVoltageuV,
    LwBool          bRoundUp,
    LwBool          bBound
)
{
    *pVoltageuV = voltDeviceRoundVoltage_MOCK_CONFIG.roundedVoltageuV;
    return voltDeviceRoundVoltage_MOCK_CONFIG.status;
}

/*!
 * @brief   MOCK implementation of voltDeviceGetVoltage.
 *
 * @details Configuration for this mock interface is controlled via values in
 *          @ref voltDeviceGetVoltage_MOCK_CONFIG. See
 *          @ref voltDeviceGetVoltage_IMPL() for original interface.
 *
 * @param[in]  pDevice      VOLT_DEVICE object pointer
 * @param[out] pVoltageuV   Current voltage
 *
 * @return  voltDeviceGetVoltage_MOCK_CONFIG.status
 */
FLCN_STATUS
voltDeviceGetVoltage_MOCK
(
    VOLT_DEVICE    *pDevice,
    LwU32          *pVoltageuV
)
{
    return voltDeviceGetVoltage_MOCK_CONFIG.status;
}

/*!
 * @brief   MOCK implementation of voltDeviceSetVoltage.
 *
 * @details Configuration for this mock interface is controlled via values in
 *          @ref voltDeviceSetVoltage_MOCK_CONFIG. See
 *          @ref voltDeviceSetVoltage_IMPL() for original interface.
 *
 * @param[in]  pDevice      VOLT_DEVICE object pointer
 * @param[out] pVoltageuV   Current voltage
 *
 * @return  voltDeviceGetVoltage_MOCK_CONFIG.status
 */
FLCN_STATUS
voltDeviceSetVoltage_MOCK
(
    VOLT_DEVICE    *pDevice,
    LwU32           voltageuV,
    LwBool          bTrigger,
    LwBool          bWait,
    LwU8            railAction
)
{
    return voltDeviceSetVoltage_MOCK_CONFIG.status;
}

/* ------------------------ Private Functions ------------------------------- */
