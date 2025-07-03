/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SOE_OBJTHERM_H
#define SOE_OBJTHERM_H

/*!
 * @file soe_objtherm.h
 * @brief  SOE Therm Library HAL header file. HAL therm functions can be called
 *         via the 'ThermHal' global variable.
 */


/* ------------------------ System Includes -------------------------------- */
#include "lwuproc.h"
#include "soesw.h"

/* ------------------------ Application Includes --------------------------- */
#include "unit_dispatch.h"
#include "config/g_therm_hal.h"
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * TSENSE thermal limits for GET Thresholds API
 */
#define TSENSE_WARN_THRESHOLD  0x0
#define TSENSE_OVERT_THRESHOLD 0x1

/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */

#endif // SOE_OBJTHERM_H
