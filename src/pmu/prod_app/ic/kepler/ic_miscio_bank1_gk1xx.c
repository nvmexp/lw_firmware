/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2011-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   ic_miscio_bank1_gk1xx.c
 * @brief  Contains all Interrupt Control routines for MISCIO bank 1 specific to
 *         GK1XX.
  */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"

#if (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))
#include "dev_lw_xve.h"
#endif // (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))

#include "dev_pmgr.h"
#include "dev_pmgr_addendum.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "dbgprintf.h"
#include "pmu_disp.h"
#include "pmu_objic.h"
#include "pmu_objpg.h"
#include "pmu_objms.h"
#include "pmu_objdisp.h"
#include "pmu_objgcx.h"
#include "config/g_ic_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
#if (!PMUCFG_FEATURE_ENABLED(PMU_DISPLAYLESS))
static void s_icServiceMsWakeEvents_GMXXX(LwU32 intrstat)
    GCC_ATTRIB_SECTION("imem_resident", "s_icServiceMsWakeEvents_GMXXX");
#endif

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
#if (!PMUCFG_FEATURE_ENABLED(PMU_DISPLAYLESS))
/*!
 * This function is responsible for handling interrupts caused by MSCG/SW-ASR
 * events.
 */
static void
s_icServiceMsWakeEvents_GMXXX
(
    LwU32 intrstat
)
{
    DISPATCH_LPWR lpwrEvt;
    LwU32         val      = 0;
    LwBool        bSendEvt = LW_TRUE;

    //
    // When in BLOCK EVERYTHING mode, an Allow-list access can trigger the
    // XVE bar blocker interrupt and set INTR_STAT to PENDING. But PMU
    // never sees those interrupts as the blocking function explicitly
    // disables the rising edge GPIO_1 XVE interrupt at the first step.
    // When the ucode switches to BLOCK_EQN mode, the XVE HW drives the
    // INTR_STAT to NOT_PENDING for such an Allow-list access.
    //
    // But as soon as the blocking function enables the rising edge GPIO_1
    // XVE interrupt in Step 11, PMU receives the interrupt. To prevent
    // such an interrupt from an allow-list from waking MSCG, we do not
    // forward the event to PG queue, if the following conditions are true
    //
    // 1. If GPIO_1 XVE Rising Interrupt was the only interrupt set and
    // 2. XVE_BAR_BLOCKER_INTR_STAT was not pending
    //
    // This optimization helps us to increase the MSCG efficiency by 2-4%
    // by preventing MSCG wakeups when an allow-list access oclwrs in the
    // initial portion of blocking function (from BLOCK_EVERYTHING to
    // BLOCK EQUATION)

#if (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))
    // 1. Check if XVE BAR BLOCKER GPIO is the only interrupt rising
    if (intrstat == DRF_DEF(_CPWR_PMU, _GPIO_1_INTR_RISING, _XVE_BAR_BLOCKER, _PENDING))
    {
        // 2. Check if XVE_BAR_BLOCKER INTR_STAT is NOT_PENDING
        val = REG_RD32(BAR0, DEVICE_BASE(LW_PCFG) + LW_XVE_BAR_BLOCKER);
        if ((FLD_TEST_DRF(_XVE, _BAR_BLOCKER, _INTR_STAT, _NOT_PENDING, val)))
        {
            // Do not send the event since both 1 and 2 are TRUE
            bSendEvt = LW_FALSE;
        }
    }
#endif // (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))

    if (bSendEvt)
    {
        FLCN_STATUS status;
        //
        // Disable all wake-up interrupts for MS related features.
        // PG task will re-enable them while processing MS_WAKEUP_INTR.
        //
        val = REG_RD32(CSB, LW_CPWR_PMU_GPIO_1_INTR_RISING_EN) &
              (~Ms.intrRiseEnMask);
        REG_WR32_STALL(CSB, LW_CPWR_PMU_GPIO_1_INTR_RISING_EN, val);

        lpwrEvt.hdr.eventType = LPWR_EVENT_ID_MS_WAKEUP_INTR;
        lpwrEvt.intr.intrStat = intrstat;
        lpwrEvt.intr.ctrlId   = RM_PMU_LPWR_CTRL_ID_ILWALID_ENGINE;

        status = lwrtosQueueIdSendFromISRWithStatus(LWOS_QUEUE_ID(PMU, LPWR),
                                                    &lpwrEvt, sizeof(lpwrEvt));

        //
        // Make sure we had a free slot in PG queue to wake MS features.
        // We can't delay MS wake events.
        //
        if (status != FLCN_OK)
        {
            PMU_HALT();
        }
    }
}
#endif

/* ------------------------- Public Functions ------------------------------- */
/*!
 * This is the interrupt handler for MISCIO (GPIO) bank 1 interrupts.  Both
 * rising- and falling- edge interrupts are detected and handled here.
 */
void
icServiceMiscIOBank1_GPXXX(void)
{
    LwU32         intrRising;
    LwU32         riseEn;
#if (!PMUCFG_FEATURE_ENABLED(PMU_DISPLAYLESS))
    DISPATCH_LPWR lpwrEvt;
#endif

    intrRising   = REG_RD32(CSB, LW_CPWR_PMU_GPIO_1_INTR_RISING );
    riseEn       = REG_RD32(CSB, LW_CPWR_PMU_GPIO_1_INTR_RISING_EN);

    // be sure to mask off the disabled interrupts
    intrRising   = intrRising  & riseEn;

    //
    // Clear rising edge GPIO interrupts before calling
    // s_icServiceMsWakeEvents_GK1XX
    //
    // Bug 898019. Do the clear before trying to send the events to PG Queue.
    // There is a very tight race condition where PMU can drop a valid wakeup
    // interrupt after it checks for XVE_BAR_BLOCKER_INTR_STAT, sees it as
    // NOT_PENDING and XVE HW then raises a new BAR_BLOCKER interrupt.
    // Clearing the rising edge GPIO interrupt after doing that check creates
    // this condition where we would exit this function with clearing a valid
    // wakeup interrupt without even acting on it.
    //
    REG_WR32_STALL(CSB, LW_CPWR_PMU_GPIO_1_INTR_RISING , intrRising);

#if (!PMUCFG_FEATURE_ENABLED(PMU_DISPLAYLESS))
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_GRP_MS) &&
        lpwrGrpIsCtrlSupported(RM_PMU_LPWR_GRP_CTRL_ID_MS))
    {
        if ((intrRising & Ms.intrRiseEnMask) != 0U)
        {
            //
            // First send MS Aborted event to DI-OS/DI-SCC.
            //
            // If the GCX task is in imem and the lwrrently entring DI-OS/DI-SSC
            // then abort should first be sent to the GCX task since we may need
            // to undo some steps before being able to abort/exit mscg.
            //
            gcxDidleSendMsAbortedEvent_HAL(&Gcx);

            //
            // Now send the wakeup event to LPWR Task, so that we can wakeup
            // MS Group Features.
            //
            s_icServiceMsWakeEvents_GMXXX(intrRising);
        }

        if (PENDING_IO_BANK1(_FBH_DWCF, _RISING, intrRising))
        {
            //
            // Process DWCF interrupt for RM_ONESHOT:
            // Step1) Set bNisoMsActive flag
            // Step2) Send DWCF interrupt to PG task for further processing
            //
            if ((PMUCFG_FEATURE_ENABLED(PMU_MS_OSM)) &&
                (DispContext.pRmOneshotCtx != NULL))
            {
                // Step1) Set bNisoMsActive flag.
                DispContext.pRmOneshotCtx->bNisoMsActive = LW_TRUE;

                //
                // Step2) Send DWCF interrupt/NISO MSCG Start event to
                // PG task for further processing
                //
                lpwrEvt.hdr.eventType = LPWR_EVENT_ID_MS_OSM_NISO_START;
                lpwrEvt.intr.ctrlId = RM_PMU_LPWR_CTRL_ID_MS_MSCG;
                lwrtosQueueIdSendFromISR(LWOS_QUEUE_ID(PMU, LPWR),
                                         &lpwrEvt, sizeof(lpwrEvt));
            }
        }
    }
#endif

    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_UNIFIED_PRIV_BLOCKER_CTRL))
    {
        // Service the Priv Blocker interrupts
        icServicePrivBlockerIrq_HAL(&Ic, intrRising);
    }

    DBG_PRINTF_ISR(("IOK intr r=0x%08x,f=0x%08x,in=0x%08x\n",
                    intrRising,
                    0,
                    REG_RD32(CSB, LW_CPWR_PMU_GPIO_1_INPUT),
                    0));
}
