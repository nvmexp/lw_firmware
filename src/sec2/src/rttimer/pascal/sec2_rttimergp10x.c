/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   sec2_rttimer.c
 * @brief  Routines to control the RTTimer on GP10x
 */

/* ------------------------ System Includes -------------------------------- */
#include "sec2sw.h"
/* ------------------------ Application Includes --------------------------- */
#include "dev_sec_csb.h"

#include "sec2_objsec2.h"
#include "sec2_objrttimer.h"
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
 * @param[in]   mode            RTTIMER_MODE
 * @param[in]   interval        Timer interval
 * @param[in]   bIsModeOneShot  Timer is one shot/continuous
 *
 * @returns    FLCN_STATUS      FLCN_OK on successful exelwtion
 *                              Appropriate error status on failure.
 */
FLCN_STATUS rttimerSetup_GP10X
(
    RTTIMER_MODE mode,
    LwU32 interval,
    LwBool bIsModeOneShot
)
{
    LwU32 data;
    FLCN_STATUS retval = FLCN_OK;

    data = REG_RD32(CSB, LW_CSEC_TIMER_CTRL);
    switch (mode)
    {
        case RTTIMER_MODE_STOP:
            //
            // We should never hit this case in production.
            // Since RT Timer is enabled by default. It will only be
            // disabled in local build used for manual RT Timer test.
            //
            if (!SEC2CFG_FEATURE_ENABLED(SEC2RTTIMER_FOR_OS_TICKS))
            {
                data = FLD_SET_DRF(_CSEC, _TIMER_CTRL, _GATE, _STOP, data);
                REG_WR32(CSB, LW_CSEC_TIMER_CTRL, data);

                // Done with the RT timer. Give up the semaphore.
                lwrtosSemaphoreGive(RttimerMutex);
            }
            break;

        case RTTIMER_MODE_START:
            // Grab the RT timer semaphore
            if (lwrtosSemaphoreTake(RttimerMutex, 100) != FLCN_OK)
            {
                return FLCN_ERR_MUTEX_ACQUIRED;
            }

            // Let's reset the timer first
            data = FLD_SET_DRF(_CSEC, _TIMER_CTRL, _RESET, _TRIGGER, data);
            REG_WR32(CSB, LW_CSEC_TIMER_CTRL, data);

            // Poll for the LW_CSEC_TIMER_CTRL_RESET
            if (!SEC2_REG32_POLL_US(LW_CSEC_TIMER_CTRL,
                            DRF_SHIFTMASK(LW_CSEC_TIMER_CTRL_RESET),
                            LW_CSEC_TIMER_CTRL_RESET_DONE,
                            MAX_POLL_WAIT,
                            SEC2_REG_POLL_MODE_CSB_EQ))
            {
                // Couldn't start the timer.
                lwrtosSemaphoreGive(RttimerMutex);
                return FLCN_ERR_TIMEOUT;
            }

            // Write the interval first
            if (bIsModeOneShot)
            {
                REG_WR32(CSB, LW_CSEC_TIMER_0_INTERVAL, INTERVALUS_TO_TIMER0_ONESHOT_VAL(interval));
            }
            else
            {
                REG_WR32(CSB, LW_CSEC_TIMER_0_INTERVAL, INTERVALUS_TO_TIMER0_CONTINUOUS_VAL(interval));
            }

            // Program the ctrl to start the timer
            data = FLD_SET_DRF(_CSEC, _TIMER_CTRL, _GATE, _RUN, 0);
            
            data = FLD_SET_DRF(_CSEC, _TIMER_CTRL, _SOURCE, _PTIMER_1US, data);

            if (bIsModeOneShot)
            {
                data = FLD_SET_DRF(_CSEC, _TIMER_CTRL, _MODE, _ONE_SHOT, data);
            }
            else
            {
                data = FLD_SET_DRF(_CSEC, _TIMER_CTRL, _MODE, _CONTINUOUS, data);
            }

            // Write Ctrl now
            REG_WR32(CSB, LW_CSEC_TIMER_CTRL, data);

            break;
    }
    return retval;
}

