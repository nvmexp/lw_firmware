/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    watchdog_tu10x.c
 * @brief   PMU HAL functions related to watchdog for TU10X+
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "main.h"
#include "dev_pwr_csb.h"

/* ------------------------- Application Includes --------------------------- */
#include "config/g_timer_private.h"
#include "pmu_objtimer.h"
#include "task_watchdog.h"
#include "pmu_objpmu.h"

/* ------------------------ Public Functions  ------------------------------- */
/*!
 * @brief      Initalizes the watchdog timer.
 *
 * @details    Program the hardware watchdog timer countdown value, have the
 *             timer source its clock from the engine clock, and enable the
 *             timer.
 */
void
timerWatchdogInit_TU10X(void)
{
    //
    // WDTMR has a configurable source timer on Turing+. The watchdog should
    // run off the engine clock rather than PTIMER.
    // Also, program the initial timeout value and enable the timer.
    //
    REG_FLD_WR_DRF_NUM_STALL(CSB, _CPWR, _FALCON_WDTMRVAL, _VAL, WATCHDOG_GET_HW_TIMER_TIMEOUT_ENGCLK());
    REG_WR32_STALL(CSB, LW_CPWR_FALCON_WDTMRCTL,
                   DRF_DEF(_CPWR, _FALCON_WDTMRCTL, _WDTMREN       , _ENABLE) |
                   DRF_DEF(_CPWR, _FALCON_WDTMRCTL, _WDTMR_SRC_MODE, _ENGCLK));
}

/*!
 * @brief      Disables the timer.
 *
 * @note       This should only be called as part of the PMU detach sequence.
 */
void
timerWatchdogStop_TU10X(void)
{
    REG_FLD_WR_DRF_DEF_STALL(CSB, _CPWR, _FALCON_WDTMRCTL, _WDTMREN, _DISABLE);
}

/*!
 * @brief      Resets the timer to the initial timeout value.
 */
void
timerWatchdogPet_TU10X(void)
{
    REG_FLD_WR_DRF_NUM_STALL(CSB, _CPWR, _FALCON_WDTMRVAL, _VAL, WATCHDOG_GET_HW_TIMER_TIMEOUT_ENGCLK());
}
