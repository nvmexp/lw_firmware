/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  objfan.c
 * @brief Container-object for PMU FAN Management routines.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "fan/objfan.h"
#include "therm/objtherm.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
OBJFAN Fan;

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * Construct the FAN object.
 */
void
fanConstruct(void)
{
    memset(&Fan, 0, sizeof(OBJFAN));

    //
    // Structure is cleared by default.
    //
    // Initially THERM task is not detached.
    // Fan.bTaskDetached = LW_FALSE;
    //
}

/*!
 * Loads all relevant FAN SW-state.
 */
FLCN_STATUS
fanLoad(void)
{
    FLCN_STATUS status = FLCN_OK;

#if PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_COOLER)
    if (Fan.coolers.super.super.ppObjects != NULL)
    {
        status = fanCoolersLoad();
    }
#endif

#if PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_POLICY)
    if ((status == FLCN_OK) &&
        (Fan.policies.super.super.ppObjects != NULL))
    {
        status = fanPoliciesLoad();
    }
#endif

#if PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_ARBITER)
    if ((status == FLCN_OK) &&
        (Fan.arbiters.super.super.ppObjects != NULL))
    {
        status = fanArbitersLoad();
    }
#endif

    // Assert/HALT on any errors during loading.
    PMU_HALT_COND(status == FLCN_OK);

    return status;
}

/* ------------------------- Private Functions ------------------------------ */
