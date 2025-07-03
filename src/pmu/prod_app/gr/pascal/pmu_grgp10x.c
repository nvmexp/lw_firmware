/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_grgp10x.c
 * @brief  HAL-interface for the GP10X.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_pwr_csb.h"
#include "dev_graphics_nobundle.h"
#include "dev_master.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "pmu_objpg.h"
#include "pmu_objlpwr.h"
#include "pmu_objpmu.h"
#include "pmu_objgr.h"
#include "pmu_objfifo.h"
#include "pmu_bar0.h"

#include "dbgprintf.h"
#include "config/g_gr_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------  Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
LwU32 grLwrrentCtx;
/* ------------------------- Prototypes --------------------------------------*/
static LwBool s_grIsIdle_GP10X(void);

/* ------------------------- Public Functions  ------------------------------ */

/*!
 * @brief GR coupled RPPG entry sequence.
 *
 * This is the sequence:
 * 1) Make sure that GR is idle.
 * 2) Enable method holdoff for GR.
 * 3) Make sure that GR is idle.
 * 4) If GR-unbind WAR enabled then unbind GR
 * 5) Disable GPC priv ring
 * 6) Engage GR coupled RPPG.
 *
 * @return FLCN_OK      On success.
 * @return FLCN_ERROR   Otherwise.
 */
FLCN_STATUS
grRppgEntry_GP10X(void)
{
    OBJPGSTATE *pGrState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_PG);

    // Step 1: Check for GR idleness
    if (!s_grIsIdle_GP10X())
    {
        // TODO RPPG: Update abort reason mask
        return FLCN_ERROR;
    }

    // Step 2: Engage method holdoff.
    if (FLCN_OK != lpwrHoldoffMaskSet_HAL(&Lpwr, LPWR_HOLDOFF_CLIENT_ID_GR_PG,
                                                 pGrState->holdoffMask))
    {
        // TODO RPPG: Update abort reason mask for holdoff.

        // Disable Holdoff
        lpwrHoldoffMaskSet_HAL(&Lpwr, LPWR_HOLDOFF_CLIENT_ID_GR_PG,
                                      LPWR_HOLDOFF_MASK_NONE);
        return FLCN_ERROR;
    }

    // Step 3: Check for GR idleness
    if (!s_grIsIdle_GP10X())
    {
        // TODO RPPG: Update abort reason mask for GR INTR.

        // Disable Holdoff
        lpwrHoldoffMaskSet_HAL(&Lpwr, LPWR_HOLDOFF_CLIENT_ID_GR_PG,
                                      LPWR_HOLDOFF_MASK_NONE);
        return FLCN_ERROR;
    }

    //
    // Step 4: Unbind Graphics engine (removed with PMU_GR_CG_WAR200124305_UNBIND)
    //

    // Step 5: Disable GPC priv ring
    if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, PRIV_RING))
    {
        grPgPrivErrHandlingActivate_HAL(&Gr, LW_TRUE);
    }

    // Step 6: Engage GR coupled RPPG
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_SEQ)                      &&
        LPWR_SEQ_IS_SRAM_CTRL_SUPPORTED(LPWR_SEQ_SRAM_CTRL_ID_GR) &&
        LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, HW_SEQUENCE))
    {
        lpwrSeqSramStateSet(LPWR_SEQ_SRAM_CTRL_ID_GR, LPWR_SEQ_STATE_SRAM_RPPG,
                            LW_FALSE);
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_PG_GR_WAR1696192_ELCG_FORCE_OFF))
    {
        //
        // Bug 1696192 ELCG needs to be forced off before clearing the PG_ON
        // interrupt. It will be turned back on when POWERED_DOWN event is
        // received
        //
        lpwrCgElcgDisable(PMU_ENGINE_GR, LPWR_CG_ELCG_CTRL_REASON_GR_PG);
    }

    return FLCN_OK;
}

/*!
 * @brief GR coupled RPPG exit sequence.
 *
 * This is the sequence:
 * 1) Disengage GR coupled RPPG
 * 2) Enable GPC priv ring
 * 3) If GR unbind WAR enabled then rebind GR
 * 4) Disable method holdoff for GR.
 *
 * @return FLCN_OK      On success.
 * @return FLCN_ERROR   Otherwise.
 */
FLCN_STATUS
grRppgExit_GP10X(void)
{
    OBJPGSTATE *pGrState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_PG);

    // Step1: Disengage GR coupled RPPG
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_SEQ)                      &&
        LPWR_SEQ_IS_SRAM_CTRL_SUPPORTED(LPWR_SEQ_SRAM_CTRL_ID_GR) &&
        LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, HW_SEQUENCE))
    {
        lpwrSeqSramStateSet(LPWR_SEQ_SRAM_CTRL_ID_GR, LPWR_SEQ_STATE_PWR_ON,
                            LW_TRUE);
    }

    // Step2: Enable GPC priv ring
    if (LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, PRIV_RING))
    {
        grPgPrivErrHandlingActivate_HAL(&Gr, LW_FALSE);
    }

    // Step3: Rebind Graphics enigne (removed with PMU_GR_CG_WAR200124305_UNBIND)

    // Step4: Disable method holdoffs
    lpwrHoldoffMaskSet_HAL(&Lpwr, LPWR_HOLDOFF_CLIENT_ID_GR_PG,
                                  LPWR_HOLDOFF_MASK_NONE);

    return FLCN_OK;
}

/*!
 * @brief Initializes holdoff mask for GR.
 */
void
grInitSetHoldoffMask_GP10X(void)
{
    OBJPGSTATE *pGrState    = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_PG);
    LwU32       holdoffMask = 0;

    if (FIFO_IS_ENGINE_PRESENT(GR))
    {
        holdoffMask |= BIT(GET_FIFO_ENG(PMU_ENGINE_GR));
    }
    if (FIFO_IS_ENGINE_PRESENT(CE0))
    {
        holdoffMask |= BIT(GET_FIFO_ENG(PMU_ENGINE_CE0));
    }
    if (FIFO_IS_ENGINE_PRESENT(CE1))
    {
        holdoffMask |= BIT(GET_FIFO_ENG(PMU_ENGINE_CE1));
    }

    pGrState->holdoffMask = holdoffMask;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief Helper function for checking GR idleness
 *
 * @return LW_TRUE  if GPU is idle
 *         LW_FALSE otherwise
 */
static LwBool
s_grIsIdle_GP10X(void)
{
    LwU32         val;
    FLCN_STATUS   status;
    LwU32         numItems;

    // Report non-idle if there are pending interrupts on GR
    if (grIsIntrPending_HAL(&Gr))
    {
        // TODO: Add logging mechanism.
        return LW_FALSE;
    }

    // Abort if GR IDLE_FLIP asserted
    if (pgCtrlIdleFlipped_HAL(&Pg, RM_PMU_LPWR_CTRL_ID_GR_PG))
    {
        // TODO RPPG: Update abort reason mask for idle flip.
        return LW_FALSE;
    }

    status = lwrtosQueueGetLwrrentNumItems(LWOS_QUEUE(PMU, LPWR), &numItems);

    if (status != FLCN_OK)
    {
        PMU_HALT();
    }

    // Report non-idle if PG task queue is not empty
    if (numItems > 0)
    {
        // TODO: Add logging mechanism.
        return LW_FALSE;
    }

    // Check for SEC2 Wake-up
    if (PMUCFG_FEATURE_ENABLED(PMU_PG_SEC2_RTOS_WAR_200089154))
    {
        val = REG_RD32(CSB, LW_CPWR_PMU_IDLE_STATUS_1);

        if (FLD_TEST_DRF(_CPWR_PMU, _IDLE_STATUS_1, _SW0, _BUSY, val))
        {
            // TODO: Add logging mechanism.
            return LW_FALSE;
        }
    }

    return LW_TRUE;
}
