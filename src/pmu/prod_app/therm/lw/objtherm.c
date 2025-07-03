/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2009-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_objtherm.c
 * @brief  Container-object for PMU Thermal routines. Contains generic non-HAL
 *         interrupt-routines plus logic required to hook-up chip-specific
 *         interrupt HAL-routines.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "main_init_api.h"
#include "pmu_objhal.h"
#include "therm/objtherm.h"
#include "pmu_objpmu.h"

#include "config/g_therm_private.h"
#if(THERM_MOCK_FUNCTIONS_GENERATED)
#include "config/g_therm_mock.c"
#endif

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/*!
 * @brief Main structure for all THERM data.
 */
OBJTHERM Therm;

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief Construct the THERM object. This includes the HAL interface as well
 * as all software objects (ex. semaphores) used by the PMGR module.
 *
 * @return  FLCN_OK     On success
 */
FLCN_STATUS
constructTherm_IMPL(void)
{
    memset(&Therm, 0, sizeof(OBJTHERM));
    return FLCN_OK;
}

#if ((PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_POLICY)) || \
     (PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_MULTI_FAN)))
/*!
 * @brief This function returns the current time in microseconds
 *
 * This function gets the current time in ns and approximates the
 * current time in us. We do not have support for 64 bit math in
 * the PMU and this gives a close approximation of the current time
 * in us(approx 2% error). Might revisit this in the future
 *
 * @return current time in us
 */
LwU32 thermGetLwrrentTimeInUs(void)
{
    FLCN_TIMESTAMP  time;
    osPTimerTimeNsLwrrentGet(&time);
    return TIMER_TO_1024_NS(time);
}
#endif
