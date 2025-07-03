/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pmu_thrm_gh10x.c
 * @brief   Thermal PMU HAL functions for GH10X and later.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmu_oslayer.h"

#include "dev_pwr_csb.h"
#include "dev_pwr_csb_addendum.h"

/* ------------------------- Application Includes --------------------------- */
#include "therm/objtherm.h"
#include "therm/thrmmon.h"

#include "config/g_therm_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief   Read the thermal monitor counter.
 *
 * @param[in]   monIdx      Physical instance of the monitor.
 * @param[out]  pCount      Counter value.
 *
 * @return  FLCN_OK                 Valid counter value.
 */
FLCN_STATUS
thermMonCountGet_GH10X
(
    THRM_MON          *pThrmMon,
    LwU32             *pCount
)
{
    FLCN_STATUS status   = FLCN_OK;
    LwU8        monIdx   = pThrmMon->phyInstIdx;
    LwU32       maxCount = thermMonMaxCountGet_HAL();
    LwU32       regVal;
    LwU32       count;


    regVal = REG_RD32(CSB, LW_CPWR_THERM_INTR_MONITOR_STATE((LwU32)monIdx));
    count = DRF_VAL(_CPWR_THERM, _INTR_MONITOR_STATE, _VALUE, regVal);
    //
    // Comparing with counter value previosly read to identify wrap around
    // If there was no wrap around
    //
    if (count > pThrmMon->prevCount)
    {
        *pCount = count - pThrmMon->prevCount;
    }
    // Else if there was a wrap around
    else
    {
        *pCount = (maxCount - pThrmMon->prevCount) + count;
    }

    return status;
}

/*!
 * @brief   Read the thermal monitor counter.
 *
 * @param[in]   void
 *
 * @return  LwU32                 Monitor Max Count.
 */

LwU32
thermMonMaxCountGet_GH10X
(
   void
)
{
    return LW_CPWR_THERM_INTR_MONITOR_STATE_VALUE_MAX;
}
