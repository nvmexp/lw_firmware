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
 * @file   soe_timer.c
 * @brief  Routines to control the Timer (GP) on LR10
 */

/* ------------------------ System Includes -------------------------------- */
#include "soesw.h"
/* ------------------------ Application Includes --------------------------- */
#include "dev_soe_csb.h"

#include "soe_objsoe.h"
#include "soe_objtimer.h"
#include "soe_objtherm.h"
#include "soe_objcore.h"
#include "soe_timer.h"

/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------- Macros and Defines ---------------------------- */
/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Public Functions ------------------------------- */

/*!
 * @brief  This function will setup the Timer
 * @param[in]   mode            TIMER_MODE
 * @param[in]   event           Timer event
 * @param[in]   interval        Timer interval
 * @param[in]   bIsModeOneShot  Timer is one shot/continuous
 *
 * @returns    FLCN_STATUS      FLCN_OK on successful exelwtion
 *                              Appropriate error status on failure.
 *
 * TODO:  Add a Timer Mutex for sharing GPTIMER among different tasks. (Bug 200551627)
 */
FLCN_STATUS
timerSetup_LR10
(
    TIMER_MODE mode,
    TIMER_EVENT event,
    LwU32 intervalUs,
    LwBool bIsModeOneShot
)
{
    LwU32 data;
    FLCN_STATUS retval = FLCN_OK;

    switch (mode)
    {
        case TIMER_MODE_SETUP:

            // Set the source to PTIMER and disable the timer
            data = FLD_SET_DRF(_CSOE, _FALCON_GPTMRCTL, _GPTMREN, _DISABLE, 0);
            data = FLD_SET_DRF(_CSOE, _FALCON_GPTMRCTL, _GPTMR_SRC_MODE, _PTIMER, data);
            REG_WR32(CSB, LW_CSOE_FALCON_GPTMRCTL, data);

            // Set Timer values to 'zero'
            REG_FLD_WR_DRF_NUM(CSB, _CSOE, _FALCON_GPTMRINT, _VAL, 0);
            REG_FLD_WR_DRF_NUM(CSB, _CSOE, _FALCON_GPTMRVAL, _VAL, 0);

            Timer.event = TIMER_EVENT_NONE;
            break;

        case TIMER_MODE_START:

           // Disable the timer before changing it.
            REG_FLD_WR_DRF_DEF(CSB, _CSOE, _FALCON_GPTMRCTL, _GPTMREN, _DISABLE);

            // Set the timer interval
            if (bIsModeOneShot)
            {
                REG_FLD_WR_DRF_NUM(CSB, _CSOE, _FALCON_GPTMRINT, _VAL, 0);
                REG_FLD_WR_DRF_NUM(CSB, _CSOE, _FALCON_GPTMRVAL, _VAL,
                           INTERVALUS_TO_TIMER0_ONESHOT_VAL(intervalUs));
            }
            else
            {
                REG_FLD_WR_DRF_NUM(CSB, _CSOE, _FALCON_GPTMRINT, _VAL,
                           INTERVALUS_TO_TIMER0_CONTINUOUS_VAL(intervalUs));
                REG_FLD_WR_DRF_NUM(CSB, _CSOE, _FALCON_GPTMRVAL, _VAL,
                           INTERVALUS_TO_TIMER0_CONTINUOUS_VAL(intervalUs));
            }

            // Set event in the Timer object
            Timer.event = event;

            // Enable GP Timer.
            REG_FLD_WR_DRF_DEF(CSB, _CSOE, _FALCON_GPTMRCTL, _GPTMREN, _ENABLE);
            break;

        case TIMER_MODE_STOP:

            // Disable the timer
            REG_FLD_WR_DRF_DEF(CSB, _CSOE, _FALCON_GPTMRCTL, _GPTMREN, _DISABLE);

            // Set Timer values to '0'.
            REG_FLD_WR_DRF_NUM(CSB, _CSOE, _FALCON_GPTMRINT, _VAL, 0);
            REG_FLD_WR_DRF_NUM(CSB, _CSOE, _FALCON_GPTMRVAL, _VAL, 0);

            Timer.event = TIMER_EVENT_NONE;
            break;

        default :
            return FLCN_ERROR;
    }

    return retval;
}

void
timerService_HAL(void)
{
    switch (Timer.event)
    {
        case TIMER_EVENT_THERM:
            thermServiceAlertTimerCallback_HAL();
        break;

        case TIMER_EVENT_NONE:
        default :
            SOE_HALT();
    }

    return;
}
