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
 * @file   pmu_icbank2tuxxx.c
 * @brief  Contains Interrupt Control routine for handling GPIO bank2 interrupts for TUxxx.
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
#include "pmu/pmumailboxid.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objic.h"
#include "pmu_objlowlatency.h"

#include "config/g_ic_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */

/*!
 * @brief This is the interrupt handler for MISCIO(GPIO) bank 2 interrupts.
 */
void
icServiceMiscIOBank2_TUXXX(void)
{
    LwU32               intrHigh;
    LwU32               intrHighEn;
    FLCN_STATUS         status;
    DISPATCH_LOWLATENCY dispatchLowlatency;

    intrHigh   = REG_RD32(CSB, LW_CPWR_PMU_GPIO_2_INTR_HIGH);
    intrHighEn = REG_RD32(CSB, LW_CPWR_PMU_GPIO_2_INTR_HIGH_EN);

    // Be sure to mask off the disabled interrupts
    intrHigh = intrHigh & intrHighEn;

    if (PENDING_IO_BANK2(_XVE_MARGINING_INTR, _HIGH, intrHigh))
    {
        if (PMUCFG_FEATURE_ENABLED(PMU_BIF_LANE_MARGINING))
        {
            dispatchLowlatency.hdr.eventType = LOWLATENCY_EVENT_ID_LANE_MARGINING_INTERRUPT;

            status = lwrtosQueueIdSendFromISRWithStatus(LWOS_QUEUE_ID(PMU, LOWLATENCY),
                                                        &dispatchLowlatency,
                                                        sizeof(dispatchLowlatency));
            if (status != FLCN_OK)
            {
                //
                // Margining command is not going to be handled.
                // Indicate the reason for failure in the mailbox register.
                //
                REG_WR32(CSB,
                         LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_BIF_LANE_MARGINING),
                         status);
                PMU_BREAKPOINT();
            }
            else
            {
                //
                // Keep the margining interrupts to PMU disabled until we
                // handle the current margining interrupt. Margining interrupts
                // will be re-enabled in bifServiceMarginingInterrupts_*
                //
                intrHighEn = FLD_SET_DRF(_CPWR_PMU, _GPIO_2_INTR_HIGH_EN,
                                         _XVE_MARGINING_INTR, _DISABLED,
                                         intrHighEn);
                REG_WR32_STALL(CSB, LW_CPWR_PMU_GPIO_2_INTR_HIGH_EN, intrHighEn);
            }
        }
    }

    // Clear the interrupt if it was asserted
    REG_WR32(CSB, LW_CPWR_PMU_GPIO_2_INTR_HIGH, intrHigh);
}
