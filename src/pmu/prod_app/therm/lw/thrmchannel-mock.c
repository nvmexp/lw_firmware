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
 * @file    thrmchannel-mock.c
 * @brief   Mock implementations of thrmchannel public interfaces.
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

#include "therm/thrm-mock.h"
#include "therm/thrmdevice.h"

/* ------------------------ Application Includes ---------------------------- */
/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/*!
 * Structure Allocation of the Mocked Interface Parameters.
 */
THERM_DEVICE_TEMP_VALUE_GET_MOCK_CONFIG thermDeviceTempValueGet_MOCK_CONFIG;

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief   MOCK implementation of thermDeviceTempValueGet.
 *
 * @details Configuration for this mock interface is controlled via values in
 *          @ref thermDeviceTempValueGet_MOCK_CONFIG.
 *          See @ref thermDeviceTempValueGet_IMPL() for original interface.
 *
 * @param[in]  See @ref thermDeviceTempValueGet_IMPL().
 * @param[in]  See @ref thermDeviceTempValueGet_IMPL().
 * @param[in]  See @ref thermDeviceTempValueGet_IMPL().
 *
 * @return      thermDeviceTempValueGet_MOCK_CONFIG.status
 */
FLCN_STATUS
thermDeviceTempValueGet_MOCK
(
    THERM_DEVICE  *pDev,
    LwU8           thermDevProvIdx,
    LwTemp        *pLwTemp
)
{
    // Populate the Mocked temperature.
    *pLwTemp = thermDeviceTempValueGet_MOCK_CONFIG.temp;
    
    // Return Status
    return thermDeviceTempValueGet_MOCK_CONFIG.status;
}
