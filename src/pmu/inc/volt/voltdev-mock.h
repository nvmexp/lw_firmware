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
 * @file    voltdev-mock.h
 * @brief   Data required for configuring mock VOLT_DEVICE interfaces.
 */

#ifndef VOLTDEV_MOCK_H
#define VOLTDEV_MOCK_H

/* ------------------------ Types Definitions ------------------------------- */
/*!
 * Configuration variables for @ref voltDeviceRoundVoltage_MOCK.
 */
typedef struct VOLT_DEVICE_ROUND_VOLTAGE_MOCK_CONFIG
{
    /*!
     * Mocked value of rounded voltage in uV.
     */
    LwS32       roundedVoltageuV;

    /*!
     * Mock returned status.
     */
    FLCN_STATUS status;
} VOLT_DEVICE_ROUND_VOLTAGE_MOCK_CONFIG;

/*!
 * Configuration variables for @ref voltDeviceGetVoltage_MOCK.
 */
typedef struct VOLT_DEVICE_GET_VOLTAGE_MOCK_CONFIG
{
    /*!
     * Mock returned status.
     */
    FLCN_STATUS status;
} VOLT_DEVICE_GET_VOLTAGE_MOCK_CONFIG;

/*!
 * Configuration variables for @ref voltDeviceSetVoltage_MOCK.
 */
typedef struct VOLT_DEVICE_SET_VOLTAGE_MOCK_CONFIG
{
    /*!
     * Mock returned status.
     */
    FLCN_STATUS status;
} VOLT_DEVICE_SET_VOLTAGE_MOCK_CONFIG;

/* ------------------------ Structures ---------------------------- */
extern VOLT_DEVICE_ROUND_VOLTAGE_MOCK_CONFIG voltDeviceRoundVoltage_MOCK_CONFIG;
extern VOLT_DEVICE_GET_VOLTAGE_MOCK_CONFIG   voltDeviceGetVoltage_MOCK_CONFIG;
extern VOLT_DEVICE_SET_VOLTAGE_MOCK_CONFIG   voltDeviceSetVoltage_MOCK_CONFIG;

#endif // VOLTDEV_MOCK_H
