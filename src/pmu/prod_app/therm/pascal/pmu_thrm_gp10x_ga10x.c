/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pmu_thrm_gp10x_ga10x.c
 * @brief   Thermal PMU HAL functions for GP10X through GA10X.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmu_oslayer.h"

#include "dev_pwr_csb.h"
#include "dev_pwr_csb_addendum.h"

/* ------------------------- Application Includes --------------------------- */
#include "therm/objtherm.h"

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
 * @return  FLCN_ERR_ILWALID_STATE  Counter has overflown.
 */
FLCN_STATUS
thermMonCountGet_GP10X
(
    THRM_MON          *pThrmMon,
    LwU32             *pCount
)
{
    FLCN_STATUS status      = FLCN_OK;
    LwU8        monIdx      = pThrmMon->phyInstIdx;
    LwU32       ctrlVal;
    LwU32       stateVal;

    ctrlVal = REG_RD32(CSB, LW_CPWR_THERM_INTR_MONITOR_CTRL((LwU32)monIdx));
    ctrlVal =
        FLD_SET_DRF(_CPWR_THERM, _INTR_MONITOR_CTRL, _CLEAR, _TRIGGER, ctrlVal);

    //
    // If any context switch occur between reading the last counter value
    // and clearing it, will lead to some error (eg lost edges) in the next
    // counter read. So cache counter value and clear the counter atomically
    //
    appTaskCriticalEnter();
    {
        stateVal = REG_RD32(CSB, LW_CPWR_THERM_INTR_MONITOR_STATE((LwU32)monIdx));

        // CLEAR the counter.
        REG_WR32(CSB, LW_CPWR_THERM_INTR_MONITOR_CTRL((LwU32)monIdx), ctrlVal);
    }
    appTaskCriticalExit();

#if !PMU_PROFILE_AD10B_RISCV
    //
    // TODO: New HAL needs to be forked for AD10B after manual are updated
    // Counter might have overflowed in case client wasn't sampling fast enough
    //
    if (FLD_TEST_DRF(_CPWR_THERM, _INTR_MONITOR_STATE, _OVERFLOW, _YES, stateVal))
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
    }
    else
#endif
    {
        // Interpret the counter value.
        *pCount = DRF_VAL(_CPWR_THERM, _INTR_MONITOR_STATE, _VALUE, stateVal);
    }

    return status;
}
