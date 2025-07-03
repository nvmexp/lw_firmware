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
 * @file    thrm-mock.h
 * @brief   Data required for configuring mock thrmchannel.c interfaces.
 */

#ifndef THRM_MOCK_H
#define THRM_MOCK_H

/* ------------------------ Structures ---------------------------- */
/*!
 * Configuration variables for @ref thermDeviceTempValueGet_MOCK.
 */
typedef struct THERM_DEVICE_TEMP_VALUE_GET_MOCK_CONFIG
{
    // Mocked Temperature value
    LwTemp      temp;

    // Mocked Status
    FLCN_STATUS status;
} THERM_DEVICE_TEMP_VALUE_GET_MOCK_CONFIG;

/* ------------------------ Types Definitions ------------------------------- */
/*!
 * Declaration of the Structure Allocated for the Mocked Interface Parameters.
 */
extern THERM_DEVICE_TEMP_VALUE_GET_MOCK_CONFIG thermDeviceTempValueGet_MOCK_CONFIG;
#endif // THRM_MOCK_H
