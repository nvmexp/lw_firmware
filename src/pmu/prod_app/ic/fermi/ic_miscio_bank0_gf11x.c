/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2009-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   ic_miscio_bank0_gf11x.c
 *
 * @brief  Contains all Interrupt Control routines for MISCIO bank 0 specific to
 *         GF1XX.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"
#include "dev_pwr_csb_addendum.h"
#include "dev_pwr_falcon_csb.h"
#include "dev_pwr_falcon_csb_addendum.h"

#include "dev_bus.h"

#if PMUCFG_FEATURE_ENABLED(PMU_MS_OSM_1)
#include "dev_lw_xve.h"
#endif // PMUCFG_FEATURE_ENABLED(PMU_MS_OSM_1)

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "pmu_objic.h"
#include "pmu_objdisp.h"
#include "pmu_objgcx.h"
#include "pmu_objpg.h"
#include "pmu_objpmu.h"
#include "pmu_objdi.h"
#include "pmu_disp.h"
#include "pmu_objsmbpbi.h"

#include "cmdmgmt/cmdmgmt.h"
#include "unit_api.h"
#include "dbgprintf.h"
#include "rmpbicmdif.h"

#include "config/g_ic_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
static void s_icServiceMiscIOPBI_GMXXX(LwU32 *pIrqEn)
    GCC_ATTRIB_SECTION("imem_resident", "s_icServiceMiscIOPBI_GMXXX");

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * This is the interrupt handler for MISCIO (GPIO) bank 0 interrupts.  Both
 * rising- and falling- edge interrupts are detected and handled here.
 */
void
icServiceMiscIOBank0_GMXXX(void)
{
    LwU32  intrRising;
    LwU32  intrFalling;
    LwU32  riseEn;
    LwU32  fallEn;

    intrRising   = REG_RD32(CSB, LW_CPWR_PMU_GPIO_INTR_RISING );
    intrFalling  = REG_RD32(CSB, LW_CPWR_PMU_GPIO_INTR_FALLING);
    riseEn       = REG_RD32(CSB, LW_CPWR_PMU_GPIO_INTR_RISING_EN);
    fallEn       = REG_RD32(CSB, LW_CPWR_PMU_GPIO_INTR_FALLING_EN);

    // be sure to mask off the disabled interrupts
    intrRising   = intrRising  & riseEn;
    intrFalling  = intrFalling & fallEn;

    if (PENDING_IO_BANK0(_XVE_INTR, _FALLING, intrFalling))
    {
        s_icServiceMiscIOPBI_GMXXX(&fallEn);
    }
    if (PENDING_IO_BANK0(_XVXP_DEEP_IDLE , _RISING, intrRising))
    {
        icServiceMiscIODeepL1_HAL(&Ic, &riseEn, LW_TRUE, DI_PEX_SLEEP_STATE_DEEP_L1);
    }
    if (PENDING_IO_BANK0(_XVXP_DEEP_IDLE, _FALLING, intrFalling))
    {
        icServiceMiscIODeepL1_HAL(&Ic, &fallEn, LW_FALSE, DI_PEX_SLEEP_STATE_DEEP_L1);
    }
#if (!PMUCFG_FEATURE_ENABLED(PMU_DISPLAYLESS))
    if (PENDING_IO_BANK0(_DISP_2PMU_INTR, _RISING, intrRising))
    {
        // We do not disable DISP interrupts, just clear them.
        dispService_HAL(&Disp);
    }
#endif

    REG_WR32(CSB, LW_CPWR_PMU_GPIO_INTR_RISING   , intrRising);
    REG_WR32(CSB, LW_CPWR_PMU_GPIO_INTR_FALLING  , intrFalling);
    REG_WR32(CSB, LW_CPWR_PMU_GPIO_INTR_RISING_EN, riseEn);
    REG_WR32_STALL(CSB, LW_CPWR_PMU_GPIO_INTR_FALLING_EN, fallEn);

#if (!PMUCFG_FEATURE_ENABLED(PMU_DISPLAYLESS))
    //
    // Its possible that a display interrupt is raised 'after' we exit the
    // dispService_HAL interrupt the first time we service _DISP_2PMU_INTR
    // _RISING interrupt and 'before' we clear the _RISING interrupts. In such
    // a race condition, we would clear the _RISING interrupt with display
    // interrupts pending which would block any further display interrupts to
    // be raised and serviced by the PMU.
    // Check the _DISP_2PMU_INTR one more time after clearing the _INTR_RISING
    // If a DISP_2PMU_INTR is _TRUE and there is no RISING interrupt raised for
    // a new interrupt, call dispService_HAL to service the interrupt.
    //
    intrRising = REG_RD32(CSB, LW_CPWR_PMU_GPIO_INTR_RISING);
    if (!PENDING_IO_BANK0(_DISP_2PMU_INTR, _RISING, intrRising) &&
        REG_FLD_TEST_DRF_DEF(CSB, _CPWR, _PMU_GPIO_INPUT, _DISP_2PMU_INTR, _TRUE))

    {
        dispService_HAL(&Disp);
    }
#endif

    DBG_PRINTF_ISR(("IO intr r=0x%08x,f=0x%08x,in=0x%08x\n",
                    intrRising,
                    intrFalling,
                    REG_RD32(CSB, LW_CPWR_PMU_GPIO_INPUT),
                    0));
}

/*!
 * This function is responsible for notifying the Deepidle task that a Deep-L1
 * GPIO interrupt has oclwrred and requires servicing.
 *
 * @param[in]  pIrqEn
 *     Pointer to the current value of the rising-edge interrupt enable
 *     register where the DeepL1 interrupts are reported when 'bRising' is
 *     'LW_TRUE'; otherwise a pointer to the current falling-edge interrupt
 *     enable register.
 *
 * @param[in]  bRising
 *     'LW_TRUE'  if the interrupt was on the rising-edge; 'LW_FALSE' if on the
 *     falling-edge.
 *
 * @param[in]  pexSleepState
 *      PEX sleep state - DI_PEX_SLEEP_STATE_xyz
 */
void
icServiceMiscIODeepL1_GMXXX
(
    LwU32  *pIrqEn,
    LwBool  bRising,
    LwU32   pexSleepState
)
{
    if (PMUCFG_FEATURE_ENABLED(PMU_GCX_DIDLE))
    {
        DISPATCH_DIDLE dispatch;

        dispatch.hdr.eventType           = DIDLE_SIGNAL_EVTID;
        dispatch.sigDeepL1.pexSleepState = pexSleepState;
        //
        // Set the signal-type based on whether this is a rising- or falling-
        // edge interrupt and disable the interrupt source (to be re-enabled by
        // Deepidle task).
        //
        if (bRising)
        {
            dispatch.sigDeepL1.signalType = DIDLE_SIGNAL_DEEP_L1_ENTRY;
        }
        else
        {
            dispatch.sigDeepL1.signalType = DIDLE_SIGNAL_DEEP_L1_EXIT;
        }

        gcxDidleSetGpio_HAL(&Gcx, pIrqEn, pexSleepState, LW_FALSE);

        lwrtosQueueIdSendFromISR(LWOS_QUEUE_ID(PMU, GCX),
                                 &dispatch, sizeof(dispatch));
    }
    else
    {
        // Nothing to do.
    }
}

/*!
 * This function enables PBI interrupts that are handled in task_pbi.
 */
void
icEnablePBIInterrupt_GMXXX(void)
{
    LwU32 gpioEn;

    // Re-enable the falling-edge PBI interrupt
    appTaskCriticalEnter();
    {
        gpioEn = REG_RD32(CSB, LW_CPWR_PMU_GPIO_INTR_FALLING_EN);
        gpioEn = FLD_SET_DRF(_CPWR_PMU, _GPIO_INTR_FALLING_EN, _XVE_INTR,
                             _ENABLED, gpioEn);
        REG_WR32_STALL(CSB, LW_CPWR_PMU_GPIO_INTR_FALLING_EN, gpioEn);
    }
    appTaskCriticalExit();
}


/* ------------------------- Private Functions ------------------------------ */

/*!
 * This function is responsible for notifying command dispatcher task that
 * there is a PBI GPIO interrupt and require servicing.
 *
 * @param[in,out]  pIrqEn
 *     Pointer to the current value of the rising-edge interrupt enable
 *     register where the PBI interrupt is reported.
 */
static void
s_icServiceMiscIOPBI_GMXXX
(
    LwU32 *pIrqEn
)
{
    DISPATCH_CMDMGMT  dispatch;
    dispatch.disp2unitEvt = DISP2UNIT_EVT_SIGNAL;
    dispatch.signal.gpioSignal = DISPATCH_CMDMGMT_SIGNAL_GPIO_PBI;

    if (PMUCFG_FEATURE_ENABLED(PMU_PBI_SUPPORT))
    {
#if PMUCFG_FEATURE_ENABLED(PMU_MS_OSM_1)
        LwU32 data = REG_RD32(BAR0_CFG, LW_XVE_VENDOR_SPECIFIC_MSGBOX_COMMAND);

        if (pgIsCtrlSupported(RM_PMU_LPWR_CTRL_ID_MS_MSCG) &&
            FLD_TEST_DRF(_PBI, _COMMAND, _FUNC_ID, _ATTACH_DISPLAY_TO_MSCG, data))
        {
            DISPATCH_LPWR           lpwrEvt;
            DISP_RM_ONESHOT_CONTEXT *pRmOneshotCtx = DispContext.pRmOneshotCtx;

            if (pRmOneshotCtx != NULL && pRmOneshotCtx->bActive)
            {
                // Reset NisoMscg Flag.
                pRmOneshotCtx->bNisoMsActive = LW_FALSE;

                // Send PBI interrupt to PG task for attaching display to MSCG pre-condition
                lpwrEvt.hdr.eventType = LPWR_EVENT_ID_MS_OSM_NISO_END;
                lpwrEvt.intr.ctrlId = RM_PMU_LPWR_CTRL_ID_MS_MSCG;
                lwrtosQueueIdSendFromISR(LWOS_QUEUE_ID(PMU, LPWR),
                                         &lpwrEvt, sizeof(lpwrEvt));

                // Set the ACK pending variable, to be handled by Pg Task
                pRmOneshotCtx->bPbiAckPending = LW_TRUE;
            }
        }
        else
#endif // PMUCFG_FEATURE_ENABLED(PMU_MS_OSM_1)
        {
            //
            // Disable the interrupt source (to be re enabled by command
            // dispatcher task)
            //
            *pIrqEn = FLD_SET_DRF(_CPWR_PMU, _GPIO_INTR_FALLING_EN, _XVE_INTR,
                                  _DISABLED, *pIrqEn);

            lwrtosQueueIdSendFromISR(LWOS_QUEUE_ID(PMU, CMDMGMT),
                                     &dispatch, sizeof(dispatch));
        }
    }
}
