/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_thrmsensor.c
 * @brief  Container-object for PMU Thermal sensor routines. Contains generic
 *         non-HAL temperature sensor-routines.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objhal.h"
#include "therm/objtherm.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/*!
 * GPU's thermal sensor calibration values in normalized form (F0.16).
 * Normalized to hide differencies between various GPUs (GF10X -> GF11X).
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_THERM_SENSOR))
LwS32 ThermSensorA = 0;
LwS32 ThermSensorB = 0;
#endif

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

