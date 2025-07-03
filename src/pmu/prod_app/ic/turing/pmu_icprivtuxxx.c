/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_icprivtuxxx.c
 * @brief  Contains handler-routines for dealing with PRIV Blocker Interrupts
 *
 * Implementation Notes:
 *    # Within ISRs only use macro DBG_PRINTF_ISR (never DBG_PRINTF).
 *
 *    # Avoid access to any BAR0/PRIV registers while in the ISR. This is to
 *      avoid inlwrring the penalty of accessing the PRIV bus due to its
 *      relatively high-latency.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dbgprintf.h"

#include "dev_pwr_csb.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objic.h"
#include "pmu_objpg.h"
#include "pmu_objlpwr.h"

#include "config/g_ic_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * Specific handler-routine for dealing with PRIV Blocker interrupts
 */
void
icServicePrivBlockerIrq_TUXXX
(
    LwU32 intrRising
)
{
    DISPATCH_LPWR lpwrEvt;
    LwU32         val = 0;

    // Priv Blockers Interrupt Handling
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_UNIFIED_PRIV_BLOCKER_CTRL))
    {
        LPWR_PRIV_BLOCKER *pLpwrPrivBlocker = LPWR_GET_PRIV_BLOCKER();

        // Check if of the Priv blocker interrupt is pending
        if ((pLpwrPrivBlocker != NULL) &&
            ((intrRising & pLpwrPrivBlocker->gpio1IntrEnMask) != 0U))
        {
            //
            // Disable the blocker interrupts. Interrupts will be
            // enabled by LPWR Task when processing the wakup event.
            // Reason to disable these interrupt is to avoid
            // multiple wakeup event queueing to LPWR Task Queue.
            // So we want to optimize the usages of queue by queueing
            // single wakeup elwent for all the priv blocker interrupts.
            //
            val = REG_RD32(CSB, LW_CPWR_PMU_GPIO_1_INTR_RISING_EN) &
                  (~(pLpwrPrivBlocker->gpio1IntrEnMask));
            REG_WR32_STALL(CSB, LW_CPWR_PMU_GPIO_1_INTR_RISING_EN, val);

            //
            // We will send an event to LPWR Task with invalid ctrlId.
            // During the handling of this event, LPWR task will select
            // the required ctrlId.
            //
            lpwrEvt.intr.intrStat = intrRising;
            lpwrEvt.hdr.eventType = LPWR_EVENT_ID_PRIV_BLOCKER_WAKEUP_INTR;
            lpwrEvt.intr.ctrlId   = RM_PMU_LPWR_CTRL_ID_ILWALID_ENGINE;
            lwrtosQueueIdSendFromISR(LWOS_QUEUE_ID(PMU, LPWR),
                                     &lpwrEvt, sizeof(lpwrEvt));
        }
    }
}

