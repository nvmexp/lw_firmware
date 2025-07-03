/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_icghxxx.c
 * @brief  Contains interrupt setup for new LW_CTRL tree in PMU
 *
 * Implementation Notes:
 *    # Within ISRs only use macro DBG_PRINTF_ISR (never DBG_PRINTF).
 *
 *    # Avoid access to any BAR0/PRIV registers while in the ISR. This is to
 *      avoid inlwrring the penalty of accessing the PRIV bus due to its
 *      relatively high-latency.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmu_bar0.h"

#include "pmu_objlowlatency.h"
#include "pmu/pmumailboxid.h"
#include "dev_pwr_csb.h"
#include "dev_ctrl.h"
#include "dev_xtl_ep_pri.h"
#include "dev_vm.h"

#include "dev_vm_addendum.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objic.h"
#include "dev_ctrl_defines.h"

#include "config/g_ic_private.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief   Initializes the top-level interrupt sources
 *
 * @details The PMU's interrupt sources are organized into a tree. This function
 *          configures and enables the interrupts at the top-level, as required.
 */
void
icPreInitTopLevel_GHXXX(void)
{
    LwU32 irqMode;
    LwU32 irqDest;
    LwU32 irqMset = 0U;

    icPreInitTopLevel_GA10X();

    if (PMUCFG_FEATURE_ENABLED(PMU_BIF_LANE_MARGINING))
    {
        //
        // To configure LM interrupt:
        // a. Set interrupt mode to level triggered.
        //
        irqMode = REG_RD32(CSB, LW_CPWR_FALCON_IRQMODE);
        irqMode = FLD_SET_DRF(_CPWR, _FALCON_IRQMODE, _LVL_EXT_RSVD8,
                              _TRUE, irqMode);
        REG_WR32(CSB, LW_CPWR_FALCON_IRQMODE, irqMode);

        // b. Set destination to RISCV core.
        irqDest = REG_RD32(CSB, LW_CPWR_RISCV_IRQDEST);
        irqDest = FLD_SET_DRF(_CPWR, _RISCV_IRQDEST, _EXT_RSVD8,
                              _RISCV, irqDest);
        REG_WR32(CSB, LW_CPWR_RISCV_IRQDEST, irqDest);

        // c. Enable LM interrupt
        irqMset = FLD_SET_DRF(_CPWR, _RISCV_IRQMSET, _EXT_RSVD8,
                              _SET, irqMset);
        REG_WR32(CSB, LW_CPWR_RISCV_IRQMSET, irqMset);
    }

    //
    // XTL implements 4 INTR_CTRL interrupt sources (one for each of
    // CPU, GSP, PMU, system firmware) to allow more secure clients
    // such as PMU to have independent control of their interrupts.
    // ct_assert for now that the init vector is in the same spot as
    // when this code was written because we will hardcode enable
    // only INTR_TOP bit 2 and INTR_LEAF(4) bit 8 to enable only that
    // one. Then lock it down to ensure PMU's interrupts cannot be
    // changed by other agents like RM.
    //
    ct_assert(DRF_VAL(_CTRL, _INTR_CTRL_ACCESS_DEFINES, _PMU,
                      LW_XTL_EP_PRI_INTR_CTRL_2_MESSAGE_INIT) ==
              LW_CTRL_INTR_CTRL_ACCESS_DEFINES_PMU_ENABLE);

    const LwU32 vector  = DRF_VAL(_CTRL, _INTR_CTRL_ACCESS_DEFINES, _VECTOR,
                                  LW_XTL_EP_PRI_INTR_CTRL_2_MESSAGE_INIT);
    const LwU32 subTree = LW_CTRL_INTR_GPU_VECTOR_TO_SUBTREE(vector);
    // Ensure that we're in line with RM
    ct_assert(subTree  == LW_CPU_INTR_UVM_SHARED_SUBTREE_START);
    const LwU32 topEn   = LWBIT32(LW_CTRL_INTR_SUBTREE_TO_TOP_BIT(subTree));
    const LwU32 leafReg = LW_CTRL_INTR_GPU_VECTOR_TO_LEAF_REG(vector);
    const LwU32 leafVal = LWBIT32(LW_CTRL_INTR_GPU_VECTOR_TO_LEAF_BIT(vector));

    // Write back the default value in case something else overwrote it
    XTL_REG_WR32(LW_XTL_EP_PRI_INTR_CTRL(2), LW_XTL_EP_PRI_INTR_CTRL_2_MESSAGE_INIT);

    REG_WR32(CSB, LW_CPWR_PMU_INTR_TOP_EN, topEn);
    REG_WR32(CSB, LW_CPWR_PMU_INTR_LEAF_EN(leafReg), leafVal);
}

/*!
 * @copydoc icService_GMXXX
 */
LwBool
icService_GHXXX(LwU32 pendingIrqMask)
{
    const LwU32 vector  = DRF_VAL(_CTRL, _INTR_CTRL_ACCESS_DEFINES, _VECTOR,
                                  LW_XTL_EP_PRI_INTR_CTRL_2_MESSAGE_INIT);
    const LwU32 leafReg = LW_CTRL_INTR_GPU_VECTOR_TO_LEAF_REG(vector);
    const LwU32 leafVal = LWBIT32(LW_CTRL_INTR_GPU_VECTOR_TO_LEAF_BIT(vector));
    LwU32 val = 0;
    LwU32 irqPending;

    if (PMUCFG_FEATURE_ENABLED(PMU_BIF_LANE_MARGINING))
    {
        irqPending = REG_RD32(CSB, LW_CPWR_FALCON_IRQSTAT);

        if (PENDING_LANE_MARGINING(irqPending))
        {
            icServiceLaneMarginingInterrupts_HAL(&Ic);
        }
    }

    //
    // XTL is the only unit in the LW_CTRL tree, so it's sufficient to
    // just check for bit 7 in FIRST_LEVEL_INTR
    //
    // TODO: hook up to XTL isr
    // 
    REG_WR32(CSB, LW_CPWR_PMU_INTR_LEAF(leafReg), leafVal);
    val = FLD_SET_DRF(_XTL_EP_PRI, _INTR_RETRIGGER, _TRIGGER, _TRUE, val);
    XTL_REG_WR32(LW_XTL_EP_PRI_INTR_RETRIGGER(2), val);

    return icService_GA10X(pendingIrqMask);
}

/*!
 * @brief This is the interrupt handler for Lane Margining interrupts.
 */
void
icServiceLaneMarginingInterrupts_GHXXX(void)
{
    LwU32               irqDisable = 0U;
    FLCN_STATUS         status;
    DISPATCH_LOWLATENCY dispatchLowlatency;

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
        // Disable from XTL level itself. Once we disable this
        // even after re-trigger message won't get sent out
        //
        irqDisable = XTL_REG_RD32(LW_XTL_EP_PRI_INTR_ENABLE(2));
        irqDisable = FLD_SET_DRF(_XTL, _EP_PRI_INTR_ENABLE, _XPL_LANE_MARGINING,
                                 _DISABLED, irqDisable);
        XTL_REG_WR32(LW_XTL_EP_PRI_INTR_ENABLE(2), irqDisable);
    }
}
