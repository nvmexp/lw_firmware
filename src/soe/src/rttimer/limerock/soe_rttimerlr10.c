/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   soe_rttimer.c
 * @brief  Routines to control the RTTimer on LR10x
 */

/* ------------------------ System Includes -------------------------------- */
#include "soesw.h"
/* ------------------------ Application Includes --------------------------- */
#include "dev_soe_csb.h"

#include "soe_objsoe.h"
#include "soe_objrttimer.h"
#include "soe_objtimer.h"
#include "config/g_ic_private.h"

/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------- Macros and Defines ---------------------------- */
#define MAX_POLL_WAIT       (10) // Wait for a max of 10usecs

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Public Functions ------------------------------- */

/*!
 * @brief  This function will setup the RT Timer
 * @param[in]   interval        Timer interval
 * @param[in]   bIsModeOneShot  Timer is one shot/continuous
 *
 * @returns    FLCN_STATUS      FLCN_OK on successful exelwtion
 *                              Appropriate error status on failure.
 */
FLCN_STATUS rttimerSetup_LR10
(
    LwU32 interval
)
{
    LwU32 data;
    FLCN_STATUS retval = FLCN_OK;

    // Grab the RT timer semaphore
    if (lwrtosSemaphoreTake(RttimerMutex, 100) != FLCN_OK)
    {
        return FLCN_ERR_MUTEX_ACQUIRED;
    }

    data = REG_RD32(CSB, LW_CSOE_TIMER_CTRL);

    // Let's reset the timer first
    data = FLD_SET_DRF(_CSOE, _TIMER_CTRL, _RESET, _TRIGGER, data);
    REG_WR32(CSB, LW_CSOE_TIMER_CTRL, data);

    // Poll for the LW_CSOE_TIMER_CTRL_RESET
    if (!SOE_REG32_POLL_US(LW_CSOE_TIMER_CTRL,
                           DRF_SHIFTMASK(LW_CSOE_TIMER_CTRL_RESET),
                           LW_CSOE_TIMER_CTRL_RESET_DONE,
                           MAX_POLL_WAIT,
                           SOE_REG_POLL_MODE_CSB_EQ))
    {
        // Couldn't start the timer.
        lwrtosSemaphoreGive(RttimerMutex);
        return FLCN_ERR_TIMEOUT;
    }

    // Write the interval first
    REG_WR32(CSB, LW_CSOE_TIMER_0_INTERVAL, INTERVALUS_TO_TIMER0_CONTINUOUS_VAL(interval));

    // Program the ctrl to start the timer
    data = FLD_SET_DRF(_CSOE, _TIMER_CTRL, _GATE, _RUN, 0);
    data = FLD_SET_DRF(_CSOE, _TIMER_CTRL, _SOURCE, _PTIMER_1US, data);
    data = FLD_SET_DRF(_CSOE, _TIMER_CTRL, _MODE, _CONTINUOUS, data);

    // Write Ctrl now
    REG_WR32(CSB, LW_CSOE_TIMER_CTRL, data);

    return retval;
}

