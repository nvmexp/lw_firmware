/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   lanemargining_gh100.c
 * @brief  Contains Lane Margining routines for GH100 and later chips
 */

/* ------------------------- System Includes -------------------------------- */
#include "soesw.h"
#include "soe_bar0.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "config/g_bif_private.h"
#include "soe_objbif.h"

#include "dev_lw_xpl.h"
#include "dev_xtl_ep_pri.h"
#include "dev_xtl_ep_pcfg_gpu.h"
#include "dev_lw_xpl_addendum.h"
#include "dev_lw_xtl_addendum.h"
#include "dev_lw_xtl_ep_pcfg_gpu_addendum.h"
#include "dev_lw_uxl_addendum.h"
#include "dev_lw_uxl_pri_ctx.h"
#include "dev_ctrl_ip.h"
#include "dev_ctrl_defines.h"
#include "dev_fuse.h"
#include "dev_soe_csb.h"
#include "soe_bar0.h"
#include "soe_cmdmgmt.h"
#include "soe_objgin.h"
#include "dev_ctrl_ip.h"
#include "dev_ctrl_ip_addendum.h"
#include "ptop_discovery_ip.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
static FLCN_STATUS s_bifHandleMarginCommand_LS10(LwU8 laneNum, LwU32 laneResetMask)
    GCC_ATTRIB_SECTION("imem_bif", "s_bifHandleMarginCommand_LS10");
static FLCN_STATUS s_bifScheduleNextStepMarginCmd_LS10(LwU32 marginRequestMask, LwU8 laneNum, LwBool *bQueuedStepMarginCmd)
    GCC_ATTRIB_SECTION("imem_bif", "s_bifScheduleNextStepMarginCmd_LS10");
static FLCN_STATUS s_bifHandleStepMarginCommand_LS10(LwU8 laneNum, LwU8 rcvrNum, LwU8 marginType, LwU8 usageModel)
    GCC_ATTRIB_SECTION("imem_bif", "s_bifHandleStepMarginCommand_LS10");
static FLCN_STATUS s_bifGetLaneNum(LwU32 laneSelectMask, LwU8 *pLaneNum)
    GCC_ATTRIB_SECTION("imem_bif", "s_bifGetLaneNum");


/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief This function enables Lane Margining interrupts to SOE.
 *  Only when Gen4 is supported and enabled.
 */
void
bifInitMarginingIntr_LS10(void)
{
    LwU32 tempRegVal;
    LwU8  laneNo;

    // Enable LM at XPL level
    for (laneNo = 0; laneNo < LW_XPL_PL_LM_LANE_INTERRUPT__SIZE_1; laneNo++)
    {
        tempRegVal = XPL_REG_RD32(LW_XPL_PL_LM_LANE_INTERRUPT(laneNo));
        tempRegVal = FLD_SET_DRF_NUM(_XPL, _PL_LM_LANE_INTERRUPT,
                                     _MARGIN_CMD_INT_EN, 0x1, tempRegVal);
        tempRegVal = FLD_SET_DRF_NUM(_XPL, _PL_LM_LANE_INTERRUPT,
                                     _HW_STATUS_INT_EN, 0x1, tempRegVal);
        tempRegVal = FLD_SET_DRF_NUM(_XPL, _PL_LM_LANE_INTERRUPT,
                                     _ERR_LIMIT_EXCEED_INT_EN, 0x1, tempRegVal);
        XPL_REG_WR32(LW_XPL_PL_LM_LANE_INTERRUPT(laneNo), tempRegVal);
    }

    XPL_REG_WR32(LW_XPL_PL_LM_INTERRUPT_EN,
                 LW_XPL_PL_LM_INTERRUPT_EN_ENABLE_ALL);

    // Without this enable bit HW_MARGIN_READY interrupt is never pending
    tempRegVal = XPL_REG_RD32(LW_XPL_PL_LM_EN);
    tempRegVal = FLD_SET_DRF_NUM(_XPL, _PL_LM_EN,
                                 _LANE_MARGIN_ENABLE, 0x1, tempRegVal);
    XPL_REG_WR32(LW_XPL_PL_LM_EN, tempRegVal);

    // Enable LM interrupts at XTL level
    tempRegVal = XTL_REG_RD32(LW_XTL_EP_PRI_INTR_ENABLE(2));
    tempRegVal = FLD_SET_DRF(_XTL, _EP_PRI_INTR_ENABLE,
                             _XPL_LANE_MARGINING, _ENABLED, tempRegVal);
    XTL_REG_WR32(LW_XTL_EP_PRI_INTR_ENABLE(2), tempRegVal);

    tempRegVal = XTL_REG_RD32(LW_XTL_EP_PRI_INTR_MASK_CLR(2));
    tempRegVal = FLD_SET_DRF_NUM(_XTL, _EP_PRI_INTR_MASK_CLR,
                                 _XPL_LANE_MARGINING, 0x1, tempRegVal);
    XTL_REG_WR32(LW_XTL_EP_PRI_INTR_MASK_CLR(2), tempRegVal);
}

/*!
 * @brief This function is called by task LOWLATENCY to handle the
 * margining interrupts. For step margining interrupts, this updates margining
 * status register and adds command in task BIF's queue if required
 *
 * @return FLCN_OK if interrupt is handled correctly
 */
FLCN_STATUS
bifServiceMarginingIntr_LS10(void)
{
    LwU32       regVal;
    FLCN_STATUS status    = FLCN_OK;

    regVal = XPL_REG_RD32(LW_XPL_PL_LM_INTERRUPT_STATUS);
    if (FLD_TEST_DRF(_XPL, _PL_LM_INTERRUPT_STATUS, _HW_MARGIN_READY,
                     _PENDING, regVal))
    {
        bifHandleMarginingReadyIntr_HAL(&Bif, regVal);
    }
    else if (FLD_TEST_DRF(_XPL, _PL_LM_INTERRUPT_STATUS, _HW_MARGIN_ABORT,
                          _PENDING, regVal))
    {
        bifHandleMarginingAbortIntr_HAL(&Bif, regVal);
    }
    // Lane specific interrupt is set
    else
    {
        status = bifHandleMarginCommandIntr_HAL(&Bif, regVal);
    }

    //
    // XPL interrupts are already cleared as part of the respective handling
    // functions. XTL interrupt need to be cleared explicitly
    //
    regVal = XTL_REG_RD32(LW_XTL_EP_PRI_INTR_STATUS(2));
    regVal = FLD_SET_DRF(_XTL, _EP_PRI_INTR_STATUS, _XPL_LANE_MARGINING,
                         _RESET, regVal);
    XTL_REG_WR32(LW_XTL_EP_PRI_INTR_STATUS(2), regVal);

    //
    // Now clear interrupt at GIN/LW_CTRL level. These are cleared as a part of
    // ISR itself, but simply clearing them in ISR won't work, since XPL, XTL
    // interrupts are pending, LW_CTRL interrupt bits will be set to pending
    // again. Now that they have been cleared, make sure to clear the LW_CTRL
    // interrupt here again
    //
    GIN_REG_WR32(LW_CTRL_SOE_INTR_UNITS,
        DRF_NUM(_CTRL, _SOE_INTR_UNITS, _XTL_SOE, 0x1));

    XTL_REG_WR32(LW_XTL_EP_PRI_INTR_RETRIGGER(2),
        DRF_DEF(_XTL_EP_PRI, _INTR_RETRIGGER, _TRIGGER, _TRUE));

    //
    // Re-enable margining interrupt. These were disabled from
    // bifServiceLaneMarginingInterrupts_LS10 as soon as margining interrupt
    // on SOE was detected
    //
    GIN_REG_WR32(LW_CTRL_SOE_INTR_UNITS_EN_SET,
        DRF_NUM(_CTRL, _SOE_INTR_UNITS, _XTL_SOE, 1));


    return status;
}

/*!
 * @brief Handle Margining Ready interrupt
 *
 * @param[in] regVal Current value of LW_XPL_PL_LM_INTERRUPT_STATUS register
 */
void
bifHandleMarginingReadyIntr_LS10
(
    LwU32 regVal
)
{
    LwU32 tempRegVal;
    LwU8  laneNum;
    LwU8  *pLastStepMarginOffset;
    LwU8  *pLastStepMarginType;

    // Handle the interrupt
    tempRegVal = XTL_REG_RD32(LW_XTL_EP_PRI_MARGINING_SHADOW_PORT_STATUS);

    //
    // Now the system is ready to take any margining commands from Root
    // port. Before declaring ourselves to be Margining ready, we check
    // if Margining is enabled at compile time.
    //
    if (bifMarginingEnabled_HAL(&Bif))
    {
        tempRegVal = FLD_SET_DRF_NUM(_XTL, _EP_PRI_MARGINING_SHADOW_PORT_STATUS,
                                     _MARGINING_RDY, 0x1U, tempRegVal);
        tempRegVal = FLD_SET_DRF_NUM(_XTL, _EP_PRI_MARGINING_SHADOW_PORT_STATUS,
                                     _MARGINING_SW_RDY, 0x1U, tempRegVal);
        //
        // This interrupt indicates start of margining requests.
        // Initialize margining scheduled mask
        //
        MARGINING_DATA_GET()->marginingScheduledMask = 0U;
    }
    else
    {
        tempRegVal = FLD_SET_DRF(_XTL, _EP_PRI_MARGINING_SHADOW_PORT_STATUS,
                                 _MARGINING_RDY, _INIT, tempRegVal);
        tempRegVal = FLD_SET_DRF(_XTL, _EP_PRI_MARGINING_SHADOW_PORT_STATUS,
                                 _MARGINING_SW_RDY, _INIT, tempRegVal);
    }
    XTL_REG_WR32(LW_XTL_EP_PRI_MARGINING_SHADOW_PORT_STATUS, tempRegVal);

    //
    // Ack the interrupt
    // Updating the existing value of LW_XPL_PL_LM_INTERRUPT_STATUS (received
    // by this function in the regVal param) could potentially clear other
    // PENDING bits than the one being handled here. Hence set RESET to only
    // that bit which we want to clear. Initializing regVal to 0, won't clear
    // the 'other' PENDING states in the register as HW simply ignores the
    // write of '0' as per the manuals. See below:
    // #define *_NO_PENDING     0x0 /* R-E-V */
    // Todo by anaikwade: Address this bug for prev. chips. Bug 200753824.
    //
    regVal = 0U;
    regVal = FLD_SET_DRF(_XPL, _PL_LM_INTERRUPT_STATUS, _HW_MARGIN_READY,
                         _RESET, regVal);
    XPL_REG_WR32(LW_XPL_PL_LM_INTERRUPT_STATUS, regVal);

    //
    // Initialize lastStepMarginType and lastStepMarginOffset arrays for all
    // lanes
    //
    pLastStepMarginType   = MARGINING_DATA_GET()->lastStepMarginType;
    pLastStepMarginOffset = MARGINING_DATA_GET()->lastStepMarginOffset;

    for (laneNum = 0; laneNum < BIF_MAX_PCIE_LANES; laneNum++)
    {
        pLastStepMarginType[laneNum]   = BIF_ILWALID_STEP_MARGIN_TYPE;
        pLastStepMarginOffset[laneNum] = BIF_ILWALID_STEP_MARGIN_OFFSET;
    }
}

/*!
 * @brief Checks if Margining is enabled from the fuse and from the regkey
 *
 * @return LW_TRUE  if Lane Margining is enabled
 *         LW_FALSE otherwise
 */
LwBool
bifMarginingEnabled_LS10(void)
{
/*
    LwU32 fuseVal = fuseRead(LW_FUSE_OPT_LANE_MARGINING_CONTROL);

    return FLD_TEST_DRF(_FUSE, _OPT_LANE_MARGINING_CONTROL, _DATA, _DISABLE,
                     fuseVal);
*/
    return LW_TRUE;
}

/*!
 * @brief Handle Margining Abort interrupt
 *
 * @param[in] regVal Current value of LW_XPL_PL_LM_INTERRUPT_STATUS register
 */
void
bifHandleMarginingAbortIntr_LS10
(
    LwU32 regVal
)
{
    LwU32 tempRegVal;
    MARGINING_DATA_GET()->marginingScheduledMask = 0U;

    // Stop margining for all lanes
    bifStopStepMargining_HAL(&Bif, LW_U16_MAX, LW_TRUE);

    // Update config space status to let client know that HW has aborted LM
    tempRegVal = XTL_REG_RD32(LW_XTL_EP_PRI_MARGINING_SHADOW_PORT_STATUS);
    tempRegVal = FLD_SET_DRF(_XTL, _EP_PRI_MARGINING_SHADOW_PORT_STATUS,
                             _MARGINING_RDY, _INIT, tempRegVal);
    tempRegVal = FLD_SET_DRF(_XTL, _EP_PRI_MARGINING_SHADOW_PORT_STATUS,
                             _MARGINING_SW_RDY, _INIT, tempRegVal);
    XTL_REG_WR32(LW_XTL_EP_PRI_MARGINING_SHADOW_PORT_STATUS, tempRegVal);

    //
    // Ack the interrupt
    // Updating the existing value of LW_XPL_PL_LM_INTERRUPT_STATUS (received
    // by this function in the regVal param) could potentially clear other
    // PENDING bits than the one being handled here. Hence set RESET to only
    // that bit which we want to clear. Initializing regVal to 0, won't clear
    // the 'other' PENDING states in the register as HW simply ignores the
    // write of '0' as per the manuals. See below:
    // #define *_NO_PENDING     0x0 /* R-E-V */
    // Todo by anaikwade: Address this bug for prev. chips. Bug 200753824.
    //
    regVal = 0U;
    regVal = FLD_SET_DRF(_XPL, _PL_LM_INTERRUPT_STATUS, _HW_MARGIN_ABORT,
                         _RESET, regVal);
    XPL_REG_WR32(LW_XPL_PL_LM_INTERRUPT_STATUS, regVal);
}

/*!
 * @brief Stop margining for the specified lanes(using laneSelectMask).
 *
 * @param[in] laneSelectMask   Lane number for which Step Margin needs to be
 *                             stopped. For 'i'th lane to be selected,
 *                             bit 'i' needs to be set in the laneSelectMask
 * @param[in] bResetErrorCount For Hopper, this has no effect. Added for HAL
 *                             compatibility purpose.
 */
void
bifStopStepMargining_LS10
(
    LwU32  laneSelectMask,
    LwBool bResetErrorCount
)
{
    LwU32 regVal;
    LwU8  laneNum;
    LwU8  *pLastStepMarginOffset;
    LwU8  *pLastStepMarginType;

    // Select the desired lane(s) in PHY
    regVal = BAR0_REG_RD32(LW_UXL_LANE_COMMON_MCAST_CTL);
    regVal = FLD_SET_DRF_NUM(_UXL, _LANE_COMMON_MCAST_CTL, _LANE_MASK,
                             laneSelectMask, regVal);
    BAR0_REG_WR32(LW_UXL_LANE_COMMON_MCAST_CTL, regVal);

    // Stop the margining by setting margin_en to 0
    regVal = 0U;
    regVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_RDS,
                             0x0U, regVal);
    regVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_WDS,
                             0x1U, regVal);
    regVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_ADDR,
                             LW_UPHY_DLM_AE_MARGIN_CTRL0, regVal);
    regVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_WDATA_MARGIN_EN,
                             0x0U, regVal);
    BAR0_REG_WR32(LW_UXL_LANE_REG_DIRECT_CTL_1(0), regVal);

    //
    // Loop through the laneSelectMask and reset lastMarginType and
    // lastMarginOffset for the Stop Margin requested lane
    //
    pLastStepMarginType   = MARGINING_DATA_GET()->lastStepMarginType;
    pLastStepMarginOffset = MARGINING_DATA_GET()->lastStepMarginOffset;

    for (laneNum = 0U; laneNum < BIF_MAX_PCIE_LANES; laneNum++)
    {
        if ((BIT(laneNum) & laneSelectMask) != 0U)
        {
            pLastStepMarginType[laneNum]   = BIF_ILWALID_STEP_MARGIN_TYPE;
            pLastStepMarginOffset[laneNum] = BIF_ILWALID_STEP_MARGIN_OFFSET;
        }
    }
}

/*!
 * @brief Handle interrupt for margining command for a specific lane
 *
 * @param[in] regVal Current value of LW_XPL_PL_LM_INTERRUPT_STATUS register
 *
 * @return    FLCN_OK If the interrupt is handled correctly
 */
FLCN_STATUS
bifHandleMarginCommandIntr_LS10
(
    LwU32 regVal
)
{
    LwU32       tempRegVal;
    LwU8        laneNum;
    LwU32       laneResetMask;
    LwU32       laneInterruptMask;
    FLCN_STATUS status = FLCN_OK;

    //
    // Check for which lane the interrrupt is pending.
    // We don't need to check if interrupt is pending for more than one lane.
    // If interrupts are pending for more than one lanes, remaining ones
    // would be taken care in the subsequent invocation of this function.
    //
    laneResetMask = 0x1U;
    for (laneNum = 0U; laneNum < BIF_MAX_PCIE_LANES; laneNum++)
    {
        //
        // regVal contains value of LW_XPL_PL_LM_INTERRUPT_STATUS register.
        // Check if the bit corresponding to lane #'laneNum' is set
        //
        if ((regVal & laneResetMask) != 0)
        {
            break;
        }
        // Keep left shifting the laneResetMask by 1 bit
        laneResetMask = BIT(laneNum + 1U);
    }

    //
    // We don't recognize what interrupt is this. We ignore this condition
    // since there is no perceivable harm because of an
    // unrecognized interrupt
    //
    if (laneNum == BIF_MAX_PCIE_LANES)
    {
        status = FLCN_OK;
        goto bifHandleMarginCommandIntr_LS10_exit;
    }

    //
    // Now find out which of the interrupts could be pending on the 'i'th lane
    // NOTE: DO NOT change the order in which the interrupts are handled.
    //       ERR_LIMIT_EXCEED_INT_STATUS : could cause link to go unstable
    //       MARGIN_CMD_INT_STATUS       : new command
    //       HW_STATUS_INT_STATUS        : HW reports status to SW to update
    //                                     error count
    // Given the above description we want to handle err limit excd intr first,
    // then a new margin cmd and lastly the hw status intr.
    //
    // Updating value of LW_XPL_PL_LM_LANE_INTERRUPT (stored in tempRegVal)
    // directly could potentially RESET other PENDING bits than the one being
    // handled in each condition. To RESET only the relevant PENDING bits,
    // Step 1: Keep the interrupts enabled
    //
    laneInterruptMask = 0x0U;
    laneInterruptMask = FLD_SET_DRF_NUM(_XPL, _PL_LM_LANE_INTERRUPT,
                            _MARGIN_CMD_INT_EN, 0x1U, laneInterruptMask);
    laneInterruptMask = FLD_SET_DRF_NUM(_XPL, _PL_LM_LANE_INTERRUPT,
                            _HW_STATUS_INT_EN, 0x1U, laneInterruptMask);
    laneInterruptMask = FLD_SET_DRF_NUM(_XPL, _PL_LM_LANE_INTERRUPT,
                            _ERR_LIMIT_EXCEED_INT_EN, 0x1U, laneInterruptMask);

    tempRegVal = XPL_REG_RD32(LW_XPL_PL_LM_LANE_INTERRUPT(laneNum));
    if (FLD_TEST_DRF(_XPL, _PL_LM_LANE_INTERRUPT, _ERR_LIMIT_EXCEED_INT_STATUS,
                     _PENDING, tempRegVal))
    {
        // Handler for _ERR_LIMIT_EXCEED_INT_STATUS
        status = bifHandleErrorCountExcdIntr_HAL(&Bif, tempRegVal);
        // Step 2: RESET only the PENDING bit of interrupt being handled here
        laneInterruptMask = FLD_SET_DRF(_XPL, _PL_LM_LANE_INTERRUPT,
                                        _ERR_LIMIT_EXCEED_INT_STATUS, _RESET,
                                        laneInterruptMask);
    }
    else if (FLD_TEST_DRF(_XPL, _PL_LM_LANE_INTERRUPT, _MARGIN_CMD_INT_STATUS,
                          _PENDING, tempRegVal))
    {
        // Now handle the actual margining command for the identified lane
        status = s_bifHandleMarginCommand_LS10(laneNum, laneResetMask);
        // Step 2: RESET only the PENDING bit of interrupt being handled here
        laneInterruptMask = FLD_SET_DRF(_XPL, _PL_LM_LANE_INTERRUPT,
                                        _MARGIN_CMD_INT_STATUS, _RESET,
                                        laneInterruptMask);
    }
    else if (FLD_TEST_DRF(_XPL, _PL_LM_LANE_INTERRUPT, _HW_STATUS_INT_STATUS,
                          _PENDING, tempRegVal))
    {
        //
        // Handler for _HW_STATUS_INT_STATUS
        // Step 2: RESET only the PENDING bit of interrupt being handled here
        //
        laneInterruptMask = FLD_SET_DRF(_XPL, _PL_LM_LANE_INTERRUPT,
                                        _HW_STATUS_INT_STATUS, _RESET,
                                        laneInterruptMask);
    }

    //
    // Ack the interrupts
    // Step 3: Actually write back to LW_XPL_PL_LM_LANE_INTERRUPT
    // Writing 0 to other PENDING bits won't clear their state in the register
    // as HW simply ignores the write of '0' as per the manuals. See below:
    // #define *_NO_PENDING     0x0 /* R-E-V */
    // Todo by anaikwade: Address this bug for prev. chips. Bug 200753824.
    //
    XPL_REG_WR32(LW_XPL_PL_LM_LANE_INTERRUPT(laneNum), laneInterruptMask);
    XPL_REG_WR32(LW_XPL_PL_LM_INTERRUPT_STATUS, laneResetMask);

bifHandleMarginCommandIntr_LS10_exit:
    return status;
}

/*!
 * @brief Handles error count exceeded interrupt during Margining
 *
 * @param[in] regVal  Current value of LW_XPL_PL_LM_LANE_INTERRUPT register
 *
 * return     FLCN_OK If the interrupt is handled successfully
 */
FLCN_STATUS
bifHandleErrorCountExcdIntr_LS10
(
    LwU32 regVal
)
{
    LwU8        laneNum;
    LwU32       laneResetMask;
    FLCN_STATUS status = FLCN_OK;

    //
    // Check for which particular lane the interrrupt is pending
    // We don't need to check if interrupts are pending for more than one lane.
    // If interrupts are pending for more than one lanes, SOE will be getting
    // subsequent interrupts for unhandled lanes
    //
    laneResetMask = 0x1U;
    for (laneNum = 0U; laneNum < BIF_MAX_PCIE_LANES; laneNum++)
    {
        if (regVal & laneResetMask)
        {
            break;
        }
        // Keep left shifting the laneResetMask by 1 bit
        laneResetMask = BIT(laneNum + 1U);
    }

    //
    // We could not find any lane with the pending interrupt bit
    // Maybe a false interrupt. Ignoring is safe.
    //
    if (laneNum == BIF_MAX_PCIE_LANES)
    {
        goto bifHandleErrorCountExcdIntr_LS10_exit;
    }

    //
    // Before acknowleding the interrupt, make sure to stop margining
    // and reset error count.
    // Here we are giving preference to stoping the margining since failing
    // to stop margining early enough with excess error count might result in
    // GPU falling off the bus/link going into unrecoverable state
    //
    bifStopStepMargining_HAL(&Bif, laneResetMask, LW_TRUE);

    status = bifUpdateStepMarginingExecStatus_HAL(&Bif, laneNum,
                 PCIE_LANE_MARGIN_TOO_MANY_ERRORS);
    if (status != FLCN_OK)
    {
        goto bifHandleErrorCountExcdIntr_LS10_exit;
    }

    //
    // For task LOWLATENCY, following statement is as good as atomic since
    // this task runs at priority of 4 and task BIF won't be able to preempt it
    //
    MARGINING_DATA_GET()->marginingScheduledMask &= ~(laneResetMask);

bifHandleErrorCountExcdIntr_LS10_exit:
    return status;
}

/*!
 * @brief Update margining status register during different stages of
 * Step Margining
 *
 * @param[in] laneNum    Lane Number for which Margining status needs
 *                       to be updated
 * @param[in] execStatus Current exelwtion status of Step Margining
 *
 * @return    FLCN_OK    If the status register is updated correctly
 */
FLCN_STATUS
bifUpdateStepMarginingExecStatus_LS10
(
    LwU8 laneNum,
    LwU8 execStatus
)
{
    LwU32       regVal;
    LwU8        rcvrNum;
    LwU8        marginType;
    LwU8        usageModel;
    FLCN_STATUS status = FLCN_OK;

    regVal     = BAR0_REG_RD32(DEVICE_BASE(LW_EP_PCFGM) +
                     LW_EP_PCFG_GPU_MARGINING_LANE_CTRL_STATUS_REGISTER(laneNum));
    rcvrNum    = DRF_VAL(_EP_PCFG_GPU, _MARGINING_LANE_CTRL_STATUS_REGISTER,
                         _RECEIVER_NUMBER, regVal);
    marginType = DRF_VAL(_EP_PCFG_GPU, _MARGINING_LANE_CTRL_STATUS_REGISTER,
                         _MARGIN_TYPE, regVal);
    usageModel = DRF_VAL(_EP_PCFG_GPU, _MARGINING_LANE_CTRL_STATUS_REGISTER,
                         _USAGE_MODEL, regVal);

    regVal = 0U;
    regVal = FLD_SET_DRF_NUM(_XTL, _EP_PRI_MARGINING_SHADOW_LANE_STATUS,
                             _LANE_ID, laneNum, regVal);
    regVal = FLD_SET_DRF_NUM(_XTL, _EP_PRI_MARGINING_SHADOW_LANE_STATUS,
                             _MARGIN_TYPE, marginType, regVal);
    regVal = FLD_SET_DRF_NUM(_XTL, _EP_PRI_MARGINING_SHADOW_LANE_STATUS,
                             _RCVR_NUM, rcvrNum, regVal);
    regVal = FLD_SET_DRF_NUM(_XTL, _EP_PRI_MARGINING_SHADOW_LANE_STATUS,
                             _USAGE_MODEL, usageModel, regVal);

    switch (execStatus)
    {
        case PCIE_LANE_MARGIN_TOO_MANY_ERRORS:
        {
            regVal = FLD_SET_DRF(_XTL, _EP_PRI_MARGINING_SHADOW_LANE_STATUS,
                                 _MARGIN_PAYLOAD_EXEC_STATUS,
                                 _TOO_MANY_ERRORS, regVal);
            break;
        }
        case PCIE_LANE_MARGIN_SET_UP_IN_PROGRESS:
        {
            regVal = FLD_SET_DRF(_XTL, _EP_PRI_MARGINING_SHADOW_LANE_STATUS,
                                 _MARGIN_PAYLOAD_EXEC_STATUS,
                                 _SET_UP_IN_PROGRESS, regVal);
            break;
        }
        case PCIE_LANE_MARGIN_MARGINING_IN_PROGRESS:
        {
            regVal = FLD_SET_DRF(_XTL, _EP_PRI_MARGINING_SHADOW_LANE_STATUS,
                                 _MARGIN_PAYLOAD_EXEC_STATUS,
                                 _MARGINING_IN_PROGRESS, regVal);
            break;
        }
        default:
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            SOE_BREAKPOINT();
            goto bifUpdateStepMarginingExecStatus_LS10_exit;
        }
    }

    XTL_REG_WR32(LW_XTL_EP_PRI_MARGINING_SHADOW_LANE_STATUS, regVal);

bifUpdateStepMarginingExecStatus_LS10_exit:
    return status;
}

/*!
 * @brief Handle margining command for a specific lane
 *
 * @param[in] laneNum       Lane number for which this margin command needs
 *                          to be handled
 * @param[in] laneResetMask Reset mask used to clear the pending interrupt
 *
 * @return    FLCN_OK    If margining command is handled correctly
 */
static FLCN_STATUS
s_bifHandleMarginCommand_LS10
(
    LwU8  laneNum,
    LwU32 laneResetMask
)
{
    LwU8  rcvrNum;
    LwU8  marginType;
    LwU8  usageModel;
    LwU8  marginPayload;
    LwU32 regVal;

    FLCN_STATUS status = FLCN_OK;

    // Read margin ctrl register and decide what margining command is issued
    regVal = BAR0_REG_RD32(DEVICE_BASE(LW_EP_PCFGM) +
                 LW_EP_PCFG_GPU_MARGINING_LANE_CTRL_STATUS_REGISTER(laneNum));

    rcvrNum       = DRF_VAL(_EP_PCFG_GPU, _MARGINING_LANE_CTRL_STATUS_REGISTER,
                            _RECEIVER_NUMBER, regVal);
    marginType    = DRF_VAL(_EP_PCFG_GPU, _MARGINING_LANE_CTRL_STATUS_REGISTER,
                            _MARGIN_TYPE, regVal);
    usageModel    = DRF_VAL(_EP_PCFG_GPU, _MARGINING_LANE_CTRL_STATUS_REGISTER,
                            _USAGE_MODEL, regVal);
    marginPayload = DRF_VAL(_EP_PCFG_GPU, _MARGINING_LANE_CTRL_STATUS_REGISTER,
                            _MARGIN_PAYLOAD, regVal);

    switch (marginType)
    {
        case PCIE_LANE_MARGIN_TYPE_1:
        {
            //
            // As per pcie spec, valid receiver numbers for margin type 1 are
            // 001b through 110b, however, for Margining commands targetting
            // an Upstream Port, only 000b and 110b receiver numbers are valid
            //
            if (rcvrNum != 0x6)
            {
                status = FLCN_ERR_LM_ILWALID_RECEIVER_NUMBER;
                goto s_bifHandleMarginCommand_LS10_exit;
            }
            // Handle report margining commands
            bifHandleReportMarginingCommand_HAL(&Bif, laneNum, marginType,
                rcvrNum, usageModel, marginPayload);
            break;
        }
        case PCIE_LANE_MARGIN_TYPE_2:
        {
            // Check payload to identify the margining command
            if (marginPayload == PCIE_LANE_MARGIN_PAYLOAD_GO_TO_NORMAL_SETTINGS)
            {
                //
                // As per pcie spec, valid receiver numbers for this command are
                // 000b through 110b, however, for Margining commands targetting
                // an Upstream Port, only 000b and 110b receiver numbers are valid
                //
                if (rcvrNum != 0x0 && rcvrNum != 0x6)
                {
                    status = FLCN_ERR_LM_ILWALID_RECEIVER_NUMBER;
                    goto s_bifHandleMarginCommand_LS10_exit;
                }

                bifStopStepMargining_HAL(&Bif, BIT(laneNum), LW_TRUE);
                bifUpdateMarginingStatus_HAL(&Bif, laneNum, marginType,
                    rcvrNum, usageModel,
                    PCIE_LANE_MARGIN_PAYLOAD_GO_TO_NORMAL_SETTINGS);
                // Disable error counting for this particular lane
                bifEnableMarginErrorCounting_HAL(&Bif, laneNum, LW_FALSE);

                MARGINING_DATA_GET()->marginingScheduledMask &= ~(laneResetMask);
            }
            else if (marginPayload == PCIE_LANE_MARGIN_PAYLOAD_CLEAR_ERROR_LOG)
            {
                //
                // As per pcie spec, valid receiver numbers for this command are
                // 000b through 110b, however, for Margining commands targetting
                // an Upstream Port, only 000b and 110b receiver numbers are valid
                //
                if (rcvrNum != 0x0 && rcvrNum != 0x6)
                {
                    status = FLCN_ERR_LM_ILWALID_RECEIVER_NUMBER;
                    goto s_bifHandleMarginCommand_LS10_exit;
                }

                //
                // Since there is a register to clear error log, we don't need
                // to stop margining before actually clearing the error log.
                // Todo by anaikwade: We need this change for GA100 as well.
                //                    Bug 200753824.
                //
                bifClearErrorLog_HAL(&Bif, laneNum);
                bifUpdateMarginingStatus_HAL(&Bif, laneNum, marginType,
                     rcvrNum, usageModel,
                     PCIE_LANE_MARGIN_PAYLOAD_CLEAR_ERROR_LOG);
            }
            else if ((marginPayload &
                 PCIE_LANE_MARGIN_PAYLOAD_SET_ERROR_COUNT_CMD_MASK) ==
                 PCIE_LANE_MARGIN_PAYLOAD_SET_ERROR_COUNT_LIMIT)
            {
                //
                // As per pcie spec, valid receiver numbers for this command are
                // 001b through 110b, however, for Margining commands targetting
                // an Upstream Port, only 000b and 110b receiver numbers are valid
                //
                if (rcvrNum != 0x6)
                {
                    status = FLCN_ERR_LM_ILWALID_RECEIVER_NUMBER;
                    goto s_bifHandleMarginCommand_LS10_exit;
                }

                //
                // For Set Error Count Limit, MarginPayload[7:6] => 11b,
                // MarginPayload[5:0] => Error Count Limit
                //
                regVal = XPL_REG_RD32(LW_XPL_PL_LM_LANE_CONTROL(laneNum));
                marginPayload = marginPayload &
                                PCIE_LANE_MARGIN_PAYLOAD_SET_ERROR_COUNT_VALUE_MASK;
                regVal = FLD_SET_DRF_NUM(_XPL, _PL_LM_LANE_CONTROL,
                                         _LIMIT_ERROR_COUNT, marginPayload,
                                         regVal);
                XPL_REG_WR32(LW_XPL_PL_LM_LANE_CONTROL(laneNum), regVal);
                bifUpdateMarginingStatus_HAL(&Bif, laneNum, marginType, rcvrNum,
                                             usageModel, marginPayload);
            }
            break;
        }
        // Step time/voltage margin command
        case PCIE_LANE_MARGIN_TYPE_3:
        case PCIE_LANE_MARGIN_TYPE_4:
        {
            //
            // As per pcie spec, valid receiver numbers for margin type 3/4 are
            // 001b through 110b, however, for Margining commands targetting
            // an Upstream Port, only 000b and 110b receiver numbers are valid
            //
            if (rcvrNum != 0x6)
            {
                status = FLCN_ERR_LM_ILWALID_RECEIVER_NUMBER;
                goto s_bifHandleMarginCommand_LS10_exit;
            }

            status = s_bifHandleStepMarginCommand_LS10(laneNum, rcvrNum,
                         marginType, usageModel);
            break;
        }
        // No command
        case PCIE_LANE_MARGIN_TYPE_7:
        {
            //
            // As per pcie spec, for type 7, receiver number must always be 0
            // and payload must be for the 'No Command'
            //
            if ((rcvrNum != 0U) ||
                (marginPayload != PCIE_LANE_MARGIN_PAYLOAD_NO_COMMAND))
            {
                status = FLCN_ERR_ILWALID_ARGUMENT;
                SOE_BREAKPOINT();
                goto s_bifHandleMarginCommand_LS10_exit;
            }
            else
            {
                //
                // Just need to reflect back what is there in ctrl register's
                // payload
                //
                bifUpdateMarginingStatus_HAL(&Bif, laneNum, marginType, rcvrNum,
                                             usageModel, marginPayload);
            }
            break;
        }
    } // switch (marginType)

s_bifHandleMarginCommand_LS10_exit:
    return status;
}

/*!
 * @brief Update margining status register in the config space
 *
 * @param[in]  laneNum       Lane number for which margining status update is
 *                           requested
 * @param[in]  marginType    Margin type as specified in the margining command
 * @param[in]  rcvrNum       Receiver number as specified in the margining
                             command
 * @param[in]  usageModel    Usage model as specified in the margining command
 * @param[in]  marginPayload Payload in response to the marginig command
 */
void
bifUpdateMarginingStatus_LS10
(
    LwU8  laneNum,
    LwU8  marginType,
    LwU8  rcvrNum,
    LwU8  usageModel,
    LwU8  marginPayload
)
{
    LwU32 regVal;

    regVal = XTL_REG_RD32(LW_XTL_EP_PRI_MARGINING_SHADOW_LANE_STATUS);

    regVal = FLD_SET_DRF_NUM(_XTL, _EP_PRI_MARGINING_SHADOW_LANE_STATUS,
                             _LANE_ID, laneNum, regVal);
    regVal = FLD_SET_DRF_NUM(_XTL, _EP_PRI_MARGINING_SHADOW_LANE_STATUS,
                             _MARGIN_TYPE, marginType, regVal);
    regVal = FLD_SET_DRF_NUM(_XTL, _EP_PRI_MARGINING_SHADOW_LANE_STATUS,
                             _RCVR_NUM, rcvrNum, regVal);
    regVal = FLD_SET_DRF_NUM(_XTL, _EP_PRI_MARGINING_SHADOW_LANE_STATUS,
                             _USAGE_MODEL, usageModel, regVal);
    regVal = FLD_SET_DRF_NUM(_XTL, _EP_PRI_MARGINING_SHADOW_LANE_STATUS,
                             _MARGIN_PAYLOAD, marginPayload, regVal);

    XTL_REG_WR32(LW_XTL_EP_PRI_MARGINING_SHADOW_LANE_STATUS, regVal);
}

/*!
 * @brief Enable Margin error counting in XPL for single lane.
 *
 * @param[in] laneNum              Lane number for which step margin has been
 *                                 enabled
 * @param[in] bEnableErrorCounting If this is 1, error count is enabled.
 *                                 If this is 0, error count is disabled.
 *
 * @return   FLCN_OK if Margin error count is enabled correctly
 */
FLCN_STATUS
bifEnableMarginErrorCounting_LS10
(
    LwU8   laneNum,
    LwBool bEnableErrorCounting
)
{
    LwU32       regVal;
    FLCN_STATUS status = FLCN_OK;

    regVal = XPL_REG_RD32(LW_XPL_PL_LM_LANE_CONTROL(laneNum));
    if (bEnableErrorCounting)
    {
        regVal = FLD_SET_DRF_NUM(_XPL, _PL_LM_LANE_CONTROL, _ENABLE_ERR_COUNT,
                                 0x1, regVal);
    }
    else
    {
        regVal = FLD_SET_DRF_NUM(_XPL, _PL_LM_LANE_CONTROL, _ENABLE_ERR_COUNT,
                                 0x0, regVal);
    }
    XPL_REG_WR32(LW_XPL_PL_LM_LANE_CONTROL(laneNum), regVal);

    return status;
}

/*!
 * @brief Resets error count for the requested lane
 *
 * @param[in] laneNum Requested lane number
 *
 * @return    FLCN_OK if error count cleared successfully
 */
FLCN_STATUS
bifClearErrorLog_LS10
(
    LwU8 laneNum
)
{
    LwU32       regVal;
    FLCN_STATUS status = FLCN_OK;

    if (laneNum >= BIF_MAX_PCIE_LANES)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        SOE_BREAKPOINT();
        goto bifClearErrorLog_LS10_exit;
    }

    regVal = XPL_REG_RD32(LW_XPL_PL_LM_LANE_CONTROL(laneNum));
    regVal = FLD_SET_DRF_NUM(_XPL, _PL_LM_LANE_CONTROL, _CLEAR_ERR_COUNT,
                             0x1U, regVal);
    XPL_REG_WR32(LW_XPL_PL_LM_LANE_CONTROL(laneNum), regVal);

bifClearErrorLog_LS10_exit:
    return status;
}

/*!
 * @brief Handle step margining command for a specific lane. Specifically,
 * this function updates the margining status register with
 * 'Set up in progres' and adds command in task BIF's queue for actual step
 * margining

 * @param[in] laneNum       Lane number for which this step margining command
 *                          needs to be handled
 * @param[in] rcvrNum       Receiver number as specified in the margining
                            command
 * @param[in] marginType    Margin type as specified in the margining command
 * @param[in] usageModel    Usage model as specified in the margining command

 * @return    FLCN_OK       If command for step margining is queued up correctly
 */
static FLCN_STATUS
s_bifHandleStepMarginCommand_LS10
(
    LwU8 laneNum,
    LwU8 rcvrNum,
    LwU8 marginType,
    LwU8 usageModel
)
{
    LwU32       regVal;
    FLCN_STATUS status = FLCN_OK;

    //
    // Set 'setup for margin in progress' and queue the bif task
    // for the remaining handling of step margin command
    //
    regVal = 0U;
    regVal = FLD_SET_DRF_NUM(_XTL, _EP_PRI_MARGINING_SHADOW_LANE_STATUS,
                             _RCVR_NUM, rcvrNum, regVal);
    regVal = FLD_SET_DRF_NUM(_XTL, _EP_PRI_MARGINING_SHADOW_LANE_STATUS,
                             _USAGE_MODEL, usageModel, regVal);
    regVal = FLD_SET_DRF_NUM(_XTL, _EP_PRI_MARGINING_SHADOW_LANE_STATUS,
                             _MARGIN_TYPE, marginType, regVal);
    regVal = FLD_SET_DRF(_XTL, _EP_PRI_MARGINING_SHADOW_LANE_STATUS,
                         _MARGIN_PAYLOAD_EXEC_STATUS,
                         _SET_UP_IN_PROGRESS, regVal);
    regVal = FLD_SET_DRF_NUM(_XTL, _EP_PRI_MARGINING_SHADOW_LANE_STATUS,
                             _LANE_ID, laneNum, regVal);
    XTL_REG_WR32(LW_XTL_EP_PRI_MARGINING_SHADOW_LANE_STATUS, regVal);

    //
    // First check if we already have a bif_task scheduled for this
    // laneNum. If there is already a task scheduled, we just skip
    // scheduling a new task. Otherwise, we schedule a bif task for
    // this lane and update the mask to reflect that bif task for
    // this lane is scheduled. We are not going to pass on the contents of
    // margin ctrl register to bif task since there is a realistic chance
    // of change of contents of margin ctrl register before bif task gets
    // a chance to execute
    //

    //
    // If margining command is already queued up or in progress(for the bif task),
    // we don't queue up another
    //
    if (MARGINING_DATA_GET()->bStepMarginCmdQueuedOrInProgress)
    {
        MARGINING_DATA_GET()->marginingScheduledMask |= BIT(laneNum);
    }
    else
    {
        DISP2UNIT_CMD disp2Bif = { 0 };
        RM_FLCN_CMD_SOE bifCmd;

        disp2Bif.eventType = DISP2UNIT_EVT_COMMAND;
        disp2Bif.cmdQueueId = 0;
        disp2Bif.pCmd = &bifCmd;

        memset(&bifCmd, 0, sizeof(bifCmd));
        bifCmd.hdr.unitId = RM_SOE_UNIT_BIF;
        bifCmd.hdr.size = sizeof(bifCmd);
        bifCmd.cmd.bif.cmdType = RM_SOE_BIF_CMD_SIGNAL_LANE_MARGINING;
        bifCmd.cmd.bif.laneMargining.laneNum = laneNum;

        status = lwrtosQueueSend(Disp2QBifThd,
                                   &disp2Bif,
                                   sizeof(disp2Bif),
                                   0U);
        if (status == FLCN_OK)
        {
            MARGINING_DATA_GET()->marginingScheduledMask |= BIT(laneNum);
            MARGINING_DATA_GET()->bStepMarginCmdQueuedOrInProgress = LW_TRUE;
        }
        else
        {
            SOE_BREAKPOINT();
            goto s_bifHandleStepMarginCommand_LS10_exit;
        }
    }

s_bifHandleStepMarginCommand_LS10_exit:
    return status;
}

/*!
 * @brief Deferred routine scheduled to execute step margining commands
 * This routine actually exelwtes margining for all lanes until margining for
 * all lanes is not stopped
 *
 * @param[in] laneNum Lane number for which step margin command is triggered
 *
 * @return    FLCN_OK If this margin request is handled correctly and the
 *                    command to handle next step margin request is added
 *                    in task BIF's queue
 */
FLCN_STATUS
bifDoLaneMargining_LS10
(
    LwU8 laneNum
)
{
    LwU32       regVal;
    LwU32       tempRegVal;
    LwU32       tempMarginMask;
    LwU8        rcvrNum;
    LwU8        marginType;
    LwU8        errorCount;
    LwU8        marginOffset;
    LwU8        usageModel;
    LwU8        *pLastStepMarginOffset;
    LwU8        *pLastStepMarginType;
    LwBool      bMarginingInProgress;
    LwBool      bQueuedStepMarginCmd = LW_FALSE;
    FLCN_STATUS status               = FLCN_OK;

    pLastStepMarginType   = MARGINING_DATA_GET()->lastStepMarginType;
    pLastStepMarginOffset = MARGINING_DATA_GET()->lastStepMarginOffset;

    // Read margin ctrl register and decide what margining command is issued
    regVal     = BAR0_REG_RD32(DEVICE_BASE(LW_EP_PCFGM) +
                 LW_EP_PCFG_GPU_MARGINING_LANE_CTRL_STATUS_REGISTER(laneNum));

    // Store the margining command seen for later reference
    tempRegVal = regVal;

    rcvrNum    = DRF_VAL(_EP_PCFG_GPU, _MARGINING_LANE_CTRL_STATUS_REGISTER,
                         _RECEIVER_NUMBER, regVal);
    marginType = DRF_VAL(_EP_PCFG_GPU, _MARGINING_LANE_CTRL_STATUS_REGISTER,
                         _MARGIN_TYPE, regVal);
    usageModel = DRF_VAL(_EP_PCFG_GPU, _MARGINING_LANE_CTRL_STATUS_REGISTER,
                         _USAGE_MODEL, regVal);

    switch (marginType)
    {
        case PCIE_LANE_MARGIN_TYPE_3:
        case PCIE_LANE_MARGIN_TYPE_4:
        {
            //
            // For Hopper, MIndLeftRightTiming is 0 and therefore
            // if MIndLeftRightTiming for the targeted Receiver is Clear
            // Margin Payload [6]: Reserved.
            // Margin Payload [5:0] indicates the number of steps beyond
            // the normal setting.
            //
            marginOffset = DRF_VAL(_EP_PCFG_GPU, _MARGINING_LANE_CTRL_STATUS_REGISTER,
                                   _MARGIN_PAYLOAD_OFFSET, regVal);

            //
            // For rcvrNum, marginType and usageModel, just need to reflect
            // back what is there in ctrl register's payload
            //
            regVal = 0U;
            regVal = FLD_SET_DRF_NUM(_XTL, _EP_PRI_MARGINING_SHADOW_LANE_STATUS,
                                     _RCVR_NUM, rcvrNum, regVal);
            regVal = FLD_SET_DRF_NUM(_XTL, _EP_PRI_MARGINING_SHADOW_LANE_STATUS,
                                     _MARGIN_TYPE, marginType, regVal);
            regVal = FLD_SET_DRF_NUM(_XTL, _EP_PRI_MARGINING_SHADOW_LANE_STATUS,
                                     _USAGE_MODEL, usageModel, regVal);

            //
            // If this is not the same margining command as that of last
            // margining command then we need to initiate a new margining
            // request in UPHY
            //
            lwrtosTaskCriticalEnter();
            if ((marginOffset != pLastStepMarginOffset[laneNum])||
                (marginType   != pLastStepMarginType[laneNum]))
            {
                lwrtosTaskCriticalExit();
                status = bifStartStepMargining_HAL(&Bif, marginType,
                                                   marginOffset, BIT(laneNum));
                if (status != FLCN_OK)
                {
                    goto bifDoLaneMargining_LS10_exit;
                }
            }
            else
            {
                lwrtosTaskCriticalExit();
            }

            status = bifGetMarginingErrorCount_HAL(&Bif, BIT(laneNum),
                                                   &errorCount);
            if (status != FLCN_OK)
            {
                goto bifDoLaneMargining_LS10_exit;
            }

            regVal = FLD_SET_DRF(_XTL, _EP_PRI_MARGINING_SHADOW_LANE_STATUS,
                                 _MARGIN_PAYLOAD_EXEC_STATUS,
                                 _MARGINING_IN_PROGRESS, regVal);
            regVal = FLD_SET_DRF_NUM(_XTL, _EP_PRI_MARGINING_SHADOW_LANE_STATUS,
                                     _MARGIN_PAYLOAD_ERROR_COUNT, errorCount,
                                     regVal);

            //
            // Update margin status register with the performed margining
            // result only if original command seen earlier hasn't changed.
            // If step margining was interrupted for this lane then we need to
            // reset lastMarginOffset and lastMarginType for this lane.
            // Keeping pLastStepMarginOffset and lastStepMarginType in critical
            // section in order to avoid overwriting of these by this function
            // It's correct if these are overwritten by task lowlatency since
            // that always represents latest margining request
            //
            lwrtosTaskCriticalEnter();
            {
                //
                // task LOWLATENCY can stop margininig without this thread's
                // knowledge. If that is the case, do not overwrite margining
                // status for this lane.
                //
                bMarginingInProgress =
                    ((BIT(laneNum) & MARGINING_DATA_GET()->marginingScheduledMask)
                     != 0U);
                if (bMarginingInProgress &&
                    (tempRegVal == BAR0_REG_RD32(DEVICE_BASE(LW_EP_PCFGM) +
                                   LW_EP_PCFG_GPU_MARGINING_LANE_CTRL_STATUS_REGISTER(laneNum))))
                {
                    regVal = FLD_SET_DRF_NUM(_XTL, _EP_PRI_MARGINING_SHADOW_LANE_STATUS,
                                             _LANE_ID, laneNum, regVal);
                    XTL_REG_WR32(LW_XTL_EP_PRI_MARGINING_SHADOW_LANE_STATUS, regVal);
                    pLastStepMarginType[laneNum]   = marginType;
                    pLastStepMarginOffset[laneNum] = marginOffset;
                }
                else
                {
                    pLastStepMarginType[laneNum]   = BIF_ILWALID_STEP_MARGIN_TYPE;
                    pLastStepMarginOffset[laneNum] = BIF_ILWALID_STEP_MARGIN_OFFSET;
                }
            }
            lwrtosTaskCriticalExit();

            break;
        }
        default:
        {
            //
            // Ignore any other command than step margining. Margin type being
            // different than type3/type4 is kind of acceptable since margin
            // ctrl register could get updated before task BIF gets a chance
            // to execute this function.
            // Do not exit this function at this point since we want to find
            // out if there is a valid step margin request for other lane
            // which needs to be handled.
            //
            break;
        }
    } // switch (marginType)

    //
    // Copying this in a local variable to avoid any sync issues which might
    // result in while loop exelwting infinitely(For example, after if block
    // establishes that marginingScheduledMask is not 0 and then before while
    // loop gets a chance to execute, marginingScheduledMask is modified to
    // 0 value by task lowlatency). Now one could argue that storing it
    // in a local variable also results in stale value as seen by task BIF.
    // While that is true, that will not really result in any unexpected
    // behavior since before actually starting step margining, this function
    // reads margin control register to be in sync with the latest margin
    // request. The case where tempMarginMask is seen as 0 by task BIF and
    // lowlatency task modifies it to non zero while tempMarginMask is still 0
    // is also taken care. In this case, tempMarginMask being 0 will cause
    // this function to return but task lowlatency will schedule a new command
    // in this case since last value of marginingScheduledMask was 0
    //
    tempMarginMask = MARGINING_DATA_GET()->marginingScheduledMask;

    //
    // Check if margining was aborted. If not, find the next lane for which
    // step margining needs to be performed.
    //
    if (tempMarginMask == 0U)
    {
        goto bifDoLaneMargining_LS10_exit;
    }
    else
    {
        //
        // There is no need to update bStepMarginCmdQueuedOrInProgress to TRUE
        // from task BIF since it must always be set to TRUE when task BIF
        // is running Step Margin command(in bifDoLaneMargining*). Task BIF
        // should only be responsible for setting it to FALSE if it is going
        // to exit from running Step Margin command.
        // If next Step Margin command is queued successfully, then we say that
        // task BIF is not going to exit from Step Margin command
        //
        status = s_bifScheduleNextStepMarginCmd_LS10(tempMarginMask, laneNum,
                                                      &bQueuedStepMarginCmd);
        if (status != FLCN_OK)
        {
            goto bifDoLaneMargining_LS10_exit;
        }
    }

bifDoLaneMargining_LS10_exit:
    //
    // We want to update bStepMarginCmdQueuedOrInProgress to False when
    // queue slot for Step Margin command is going to be free that is this
    // function is going to exit without adding another command to task
    // BIF's queue for Step Margining
    //
    if (!bQueuedStepMarginCmd)
    {
        lwrtosTaskCriticalEnter();
        //
        // Before exiting the Step Margin loop, make sure that task Lowlatency
        // is not requesting Step Margin from task BIF. Before this point,
        // task LL thinks that task BIF will handle Step Margin command,
        // so let's honor that assumption here
        //
        if (MARGINING_DATA_GET()->marginingScheduledMask == 0U)
        {
            MARGINING_DATA_GET()->bStepMarginCmdQueuedOrInProgress = LW_FALSE;
            lwrtosTaskCriticalExit();
            return status;
        }
        else
        {
            lwrtosTaskCriticalExit();
            tempMarginMask = MARGINING_DATA_GET()->marginingScheduledMask;
            status = s_bifScheduleNextStepMarginCmd_LS10(tempMarginMask,
                        laneNum, &bQueuedStepMarginCmd);
            if ((status != FLCN_OK) || (!bQueuedStepMarginCmd))
            {
                //
                // So here we tried to queue up a command but could not.
                // There is not much we can do at this point except setting
                // bStepMarginCmdQueuedOrInProgress to False without letting
                // task Lowlatency take control(and think that no need to add
                // to task BIF's queue). To avoid task Lowlatency to take over
                // we add critical section here
                //
                lwrtosTaskCriticalEnter();
                MARGINING_DATA_GET()->bStepMarginCmdQueuedOrInProgress = LW_FALSE;
                lwrtosTaskCriticalExit();
            }
        }
    }
    return status;
}

/*!
 * @brief Schedule command in task BIF's queue for handling Step margin
 * for the next requested lane
 *
 * @param[in] marginRequestMask      Specifies for which all lanes Step margin
 *                                   is requested
 * @param[in] laneNum                Lane number for which previous step margin
 *                                   was performed
 * @param[out] bQueuedStepMarginCmd  Set to true if cmd queued up successfully
 *
 * return FLCN_OK    If the command is added in task BIF's queue
 */
static FLCN_STATUS
s_bifScheduleNextStepMarginCmd_LS10
(
    LwU32  marginRequestMask,
    LwU8   laneNum,
    LwBool *pbQueuedStepMarginCmd
)
{
    DISP2UNIT_CMD disp2Bif;
    RM_FLCN_CMD_SOE bifCmd;
    FLCN_STATUS  status = FLCN_OK;

    //
    // Ideally caller should make sure that the marginRequestMask passed to
    // this function will always have at least one bit set in it. If the
    // value is 0, it means that there is no new margining request that needs
    // to be scheduled.
    //
    if (marginRequestMask == 0U)
    {
        // Do not rely on init value of status. Set it explicitly.
        status = FLCN_OK;
        goto s_bifScheduleNextStepMarginCmd_LS10_exit;
    }

    do
    {
        laneNum++;
        if (laneNum >= BIF_MAX_PCIE_LANES)
        {
            laneNum = 0U;
        }
    } while ((BIT(laneNum) & marginRequestMask) == 0U);

    disp2Bif.eventType = DISP2UNIT_EVT_COMMAND;
    disp2Bif.cmdQueueId = 0;
    disp2Bif.pCmd = &bifCmd;

    memset(&bifCmd, 0, sizeof(bifCmd));

    bifCmd.hdr.unitId = RM_SOE_UNIT_BIF;
    bifCmd.hdr.ctrlFlags = 0;
    bifCmd.hdr.seqNumId = 0;
    bifCmd.hdr.size = sizeof(bifCmd);
    bifCmd.cmd.bif.cmdType = RM_SOE_BIF_CMD_SIGNAL_LANE_MARGINING;
    bifCmd.cmd.bif.laneMargining.laneNum = laneNum;

    status = lwrtosQueueSend(Disp2QBifThd,
                               &disp2Bif,
                               sizeof(disp2Bif),
                               BIF_STEP_MARGINING_TIMEOUT_US);
    if (status != FLCN_OK)
    {
        SOE_BREAKPOINT();
    }
    else
    {
        *pbQueuedStepMarginCmd = LW_TRUE;
    }

s_bifScheduleNextStepMarginCmd_LS10_exit:
    return status;
}

/*!
 * @brief Get lane number from the given laneSelectMask. This function returns
 * only a single lane for which the corresponding bit is set. This does not
 * handle the case when multiple bits are set in the laneSelectMask.
 *
 * @param[in] laneSelectMask    Bit mask for selected lanes. 'i'th bit would be
 *                              set if lane #i needs to be selected
 * @param[out] pLaneNum         Pointer to the laneNum which'd be used by caller
 *
 * @return    FLCN_OK if the valid base register is found
 */
static FLCN_STATUS
s_bifGetLaneNum
(
    LwU32 laneSelectMask,
    LwU8  *pLaneNum
)
{
    FLCN_STATUS status = FLCN_OK;

    if (laneSelectMask == 0U)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        SOE_BREAKPOINT();
        goto s_bifGetLaneNum_exit;
    }

    for ((*pLaneNum) = 0U; (*pLaneNum) < BIF_MAX_PCIE_LANES; (*pLaneNum)++)
    {
        if ((BIT((*pLaneNum)) & laneSelectMask) != 0U)
        {
            break;
        }
    }

    // laneSelectMask does not have correct value.
    if ((*pLaneNum) == BIF_MAX_PCIE_LANES)
    {
        status = FLCN_ERR_OUT_OF_RANGE;
        SOE_BREAKPOINT();
        goto s_bifGetLaneNum_exit;
    }

s_bifGetLaneNum_exit:
    return status;
}

/*!
 * @brief Get error count for the specified lane.
 *
 * @param[in]  laneSelectMask Bit mask for selected lanes. 'i'th bit would
 *                            be set if lane #i needs to be selected
 * @param[out] pErrorCount    Margining error count for the requested lane
 *
 * @return    FLCN_OK if error count is returned correctly
 *
 * Todo by anaikwade: Send laneNum instead of laneSelectMask from the caller
 *                    function. Make sure that this change is reflected for
 *                    pre-Hopper as well. Bug 200753824.
 */
FLCN_STATUS
bifGetMarginingErrorCount_LS10
(
    LwU32 laneSelectMask,
    LwU8  *pErrorCount
)
{
    LwU32       tempRegVal;
    LwU8        laneNum;
    FLCN_STATUS status = FLCN_OK;

    if (laneSelectMask == 0U)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        SOE_BREAKPOINT();
        goto bifGetMarginingErrorCount_LS10_exit;
    }

    status = s_bifGetLaneNum(laneSelectMask, &laneNum);
    if (status != FLCN_OK)
    {
        goto bifGetMarginingErrorCount_LS10_exit;
    }

    tempRegVal = XPL_REG_RD32(LW_XPL_PL_LM_LANE_CONTROL(laneNum));
    *pErrorCount = DRF_VAL(_XPL, _PL_LM_LANE_CONTROL, _ERROR_COUNT, tempRegVal);

bifGetMarginingErrorCount_LS10_exit:
    return status;
}

/*!
 * @brief Start margining for the specified lane(using laneSelectMask).
 * Note that this function starts margining only for a single lane. If
 * laneSelectMask has more than one bits set, then this function just picks
 * up first set bit(starting from lsb) and starts margining for the
 * corresponding lane. This assumption is not a problem because interrupt
 * handling model for Lane Margining not multi-lane.
 *
 * @param[in] marginType        Margin type specified in the margining command
 * @param[in] marginOffset      Margin offset specified in the margining command
 * @param[in] laneSelectMask    Lane number for which margining has to be
 *                              started. For 'i'th lane to be selected, bit 'i'
 *                              needs to be set in the laneSelectMask
 */
FLCN_STATUS
bifStartStepMargining_LS10
(
    LwU8  marginType,
    LwU8  marginOffset,
    LwU32 laneSelectMask
)
{
    LwU8        laneNum;
    LwU32       tempRegVal;
    FLCN_STATUS status = FLCN_OK;

    // Make sure that margining params are valid.
    if ((marginType == BIF_ILWALID_STEP_MARGIN_TYPE) ||
        (marginOffset == BIF_ILWALID_STEP_MARGIN_OFFSET) ||
        (laneSelectMask == 0U))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        SOE_BREAKPOINT();
        goto bifStartStepMargining_LS10_exit;
    }

    tempRegVal = BAR0_REG_RD32(LW_UXL_LANE_COMMON_MCAST_CTL);
    tempRegVal = FLD_SET_DRF_NUM(_UXL, _LANE_COMMON_MCAST_CTL, _LANE_MASK,
                                 laneSelectMask, tempRegVal);
    //
    // Todo by anaikwade: Pre-Hopper we assume that task LOWLATENCY won't
    //                    change laneSelectMask(value in *_MCAST_CTL).
    //                    Need to remove that assumption. Bug 200753824.
    //
    lwrtosTaskCriticalEnter();
    BAR0_REG_WR32(LW_UXL_LANE_COMMON_MCAST_CTL, tempRegVal);
    lwrtosTaskCriticalExit();

    // Start step margining at UXL level
    tempRegVal = 0U;
    tempRegVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_RDS, 0x0U,
                                 tempRegVal);
    tempRegVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_WDS, 0x1U,
                                 tempRegVal);
    tempRegVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_ADDR,
                                 LW_UPHY_DLM_AE_MARGIN_CTRL0, tempRegVal);
    tempRegVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_WDATA_MARGIN_EN,
                                 0x1, tempRegVal);

    // 1 step WRITE in PHY control register for margin offset + dir
    if (marginType == PCIE_LANE_MARGIN_TYPE_3)
    {
        //
        // For time-offset bits 12:8 are used. See following file for details
        // //dev/mixsig/uphy/6.1/rel/5.1/vmod/dlm/LW_UPHY_DLM_init_pkg.h
        //
        tempRegVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_WDATA_MARGIN_T,
                                     marginOffset, tempRegVal);
    }
    else
    {
        //
        // For voltage-offset bits 6:0 are used. See following file for details
        // //dev/mixsig/uphy/6.1/rel/5.1/vmod/dlm/LW_UPHY_DLM_init_pkg.h
        //
        tempRegVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_WDATA_MARGIN_V,
                                     marginOffset, tempRegVal);
    }
    //
    // Todo by anaikwade: Pre-Hopper we assume that task LOWLATENCY won't
    //                    change laneSelectMask(value in *_MCAST_CTL).
    //                    Need to remove that assumption. Bug 200753824.
    //
    lwrtosTaskCriticalEnter();
    BAR0_REG_WR32(LW_UXL_LANE_REG_DIRECT_CTL_1(0), tempRegVal);
    lwrtosTaskCriticalExit();

    status = s_bifGetLaneNum(laneSelectMask, &laneNum);
    if (status != FLCN_OK)
    {
        SOE_BREAKPOINT();
        goto bifStartStepMargining_LS10_exit;
    }

    // Enable error counting for the requested lane
    bifEnableMarginErrorCounting_HAL(&Bif, laneNum, LW_TRUE);

bifStartStepMargining_LS10_exit:
    return status;
}

/*!
 *@brief Handle report margining command
 *
 * @param[in] laneNum       Lane number for which margining status update is
 *                          requested
 * @param[in] marginType    Margin type as specified in the margining command
 * @param[in] rcvrNum       Receiver number as specified in the margining
                            command
 * @param[in] usageModel    Usage model as specified in the margining command
 * @param[in] marginPayload Payload in response to the marginig command
 */
void
bifHandleReportMarginingCommand_LS10
(
    LwU8 laneNum,
    LwU8 marginType,
    LwU8 rcvrNum,
    LwU8 usageModel,
    LwU8 marginPayload
)
{
    LwU32 regVal;
    LwU32 tempRegVal;

    switch(marginPayload)
    {
        case PCIE_LANE_MARGIN_REPORT_CAPABILITIES:
        {
            //
            // Return the margining capabilities
            // Margin Type : 001b
            // Valid receiver num : 001b - 110b
            //
            // Margin Payload[7:5] = Reserved;
            // Margin Payload[4:0] = {MIndErrorSampler,
            // MSampleReportingMethod, MIndLeftRightTiming,
            // MIndUpDowlwoltage, MVoltageSupported}
            //
            marginPayload = 0x0U;
            regVal = XPL_REG_RD32(LW_XPL_PL_LM_CAPABILITY0);
            tempRegVal = DRF_VAL(_XPL, _PL_LM_CAPABILITY0,
                                 _M_IND_ERROR_SAMPLER, regVal);
            marginPayload = FLD_SET_REF_NUM(LW_XPL_LM_CAPABILITY_MARGIN_PAYLOAD_M_IND_ERROR_SAMPLER,
                                            tempRegVal, marginPayload);

            tempRegVal = DRF_VAL(_XPL, _PL_LM_CAPABILITY0,
                                 _M_SAMPLE_REPORTING_METHOD, regVal);
            marginPayload = FLD_SET_REF_NUM(LW_XPL_LM_CAPABILITY_MARGIN_PAYLOAD_M_SAMPLE_REPORTING_METHOD,
                                            tempRegVal, marginPayload);

            tempRegVal = DRF_VAL(_XPL, _PL_LM_CAPABILITY0,
                                 _M_IND_LEFT_RIGHT_TIMING, regVal);
            marginPayload = FLD_SET_REF_NUM(LW_XPL_LM_CAPABILITY_MARGIN_PAYLOAD_M_IND_LEFT_RIGHT_TIMING,
                                            tempRegVal, marginPayload);

            tempRegVal = DRF_VAL(_XPL, _PL_LM_CAPABILITY0,
                                 _M_IND_UP_DOWN_VOLTAGE, regVal);
            marginPayload = FLD_SET_REF_NUM(LW_XPL_LM_CAPABILITY_MARGIN_PAYLOAD_M_IND_UP_DOWN_VOLTAGE,
                                            tempRegVal, marginPayload);

            tempRegVal = DRF_VAL(_XPL, _PL_LM_CAPABILITY0,
                                 _M_VOLTAGES_SUPPORTED, regVal);
            marginPayload = FLD_SET_REF_NUM(LW_XPL_LM_CAPABILITY_MARGIN_PAYLOAD_M_VOLTAGES_SUPPORTED,
                                            tempRegVal, marginPayload);
            break;
        }
        case PCIE_LANE_MARGIN_REPORT_NUM_VOLTAGE_STEPS:
        {
            regVal = XPL_REG_RD32(LW_XPL_PL_LM_CAPABILITY0);
            regVal = DRF_VAL(_XPL, _PL_LM_CAPABILITY0, _M_NUM_VOLTAGE_STEPS,
                             regVal);

            marginPayload = 0x0U;
            marginPayload = FLD_SET_REF_NUM(LW_XPL_LM_CAPABILITY_MARGIN_PAYLOAD_M_NUM_VOLTAGE_STEPS,
                                            regVal, marginPayload);
            break;
        }
        case PCIE_LANE_MARGIN_REPORT_NUM_TIMING_STEPS:
        {
            regVal = XPL_REG_RD32(LW_XPL_PL_LM_CAPABILITY0);
            regVal = DRF_VAL(_XPL, _PL_LM_CAPABILITY0, _M_NUM_TIMING_STEPS,
                             regVal);

            marginPayload = 0x0U;
            marginPayload = FLD_SET_REF_NUM(LW_XPL_LM_CAPABILITY_MARGIN_PAYLOAD_M_NUM_TIMING_STEPS,
                                            regVal, marginPayload);
            break;
        }
        case PCIE_LANE_MARGIN_REPORT_MAX_TIMING_OFFSET:
        {
            regVal = XPL_REG_RD32(LW_XPL_PL_LM_CAPABILITY0);
            regVal = DRF_VAL(_XPL, _PL_LM_CAPABILITY0, _M_MAX_TIMING_OFFSET, regVal);

            marginPayload = 0x0U;
            marginPayload = FLD_SET_REF_NUM(LW_XPL_LM_CAPABILITY_MARGIN_PAYLOAD_M_MAX_TIMING_OFFSET,
                                            regVal, marginPayload);
            break;
        }
        case PCIE_LANE_MARGIN_REPORT_MAX_VOLTAGE_OFFSET:
        {
            regVal = XPL_REG_RD32(LW_XPL_PL_LM_CAPABILITY1);
            regVal = DRF_VAL(_XPL, _PL_LM_CAPABILITY1, _M_MAX_VOLTAGE_OFFSET, regVal);

            marginPayload = 0x0U;
            marginPayload = FLD_SET_REF_NUM(LW_XPL_LM_CAPABILITY_MARGIN_PAYLOAD_M_MAX_VOLTAGE_OFFSET,
                                            regVal, marginPayload);
            break;
        }
        case PCIE_LANE_MARGIN_REPORT_SAMPLING_RATE_VOLTAGE:
        {
            regVal = XPL_REG_RD32(LW_XPL_PL_LM_CAPABILITY1);
            regVal = DRF_VAL(_XPL, _PL_LM_CAPABILITY1, _M_SAMPLING_RATE_VOLTAGE, regVal);

            marginPayload = 0x0U;
            marginPayload = FLD_SET_REF_NUM(LW_XPL_LM_CAPABILITY_MARGIN_PAYLOAD_M_SAMPLING_RATE_VOLTAGE,
                                            regVal, marginPayload);
            break;
        }
        case PCIE_LANE_MARGIN_REPORT_SAMPLING_RATE_TIMING:
        {
            regVal = XPL_REG_RD32(LW_XPL_PL_LM_CAPABILITY1);
            regVal = DRF_VAL(_XPL, _PL_LM_CAPABILITY1, _M_SAMPLING_RATE_TIMING, regVal);

            marginPayload = 0x0U;
            marginPayload = FLD_SET_REF_NUM(LW_XPL_LM_CAPABILITY_MARGIN_PAYLOAD_M_SAMPLING_RATE_TIMING,
                                            regVal, marginPayload);
            break;
        }
        case PCIE_LANE_MARGIN_REPORT_SAMPLE_COUNT:
        {
            regVal = XPL_REG_RD32(LW_XPL_PL_LM_CAPABILITY1);
            regVal = DRF_VAL(_XPL, _PL_LM_CAPABILITY1, _M_SAMPLE_COUNT, regVal);

            marginPayload = 0x0U;
            marginPayload = FLD_SET_REF_NUM(LW_XPL_LM_CAPABILITY_MARGIN_PAYLOAD_M_SAMPLE_COUNT,
                                            regVal, marginPayload);
            break;
        }
        case PCIE_LANE_MARGIN_REPORT_MAX_LANES:
        {
            regVal = XPL_REG_RD32(LW_XPL_PL_LM_CAPABILITY2);
            regVal = DRF_VAL(_XPL, _PL_LM_CAPABILITY2, _M_MAX_LANES, regVal);

            marginPayload = 0x0U;
            marginPayload = FLD_SET_REF_NUM(LW_XPL_LM_CAPABILITY_MARGIN_PAYLOAD_M_MAX_LANES,
                                            regVal, marginPayload);
            break;
        }
    } // switch(marginPayload)

    bifUpdateMarginingStatus_HAL(&Bif, laneNum, marginType, rcvrNum,
                                 usageModel, marginPayload);
}

/*!
 * @brief Interrupt handler for Lane Margining interrupts.
 */
void
bifServiceLaneMarginingInterrupts_LS10(void)
{
    DISP2UNIT_CMD disp2Bif = { 0 };
    RM_FLCN_CMD_SOE bifCmd;
    FLCN_STATUS status = FLCN_OK;

    disp2Bif.eventType = DISP2UNIT_EVT_COMMAND;
    disp2Bif.cmdQueueId = 0;
    disp2Bif.pCmd = &bifCmd;

    memset(&bifCmd, 0, sizeof(bifCmd));
    bifCmd.hdr.unitId = RM_SOE_UNIT_BIF;
    bifCmd.hdr.size = sizeof(bifCmd);
    bifCmd.cmd.bif.cmdType = RM_SOE_BIF_CMD_SERVICE_MARGINING_INTERRUPTS;

    status = lwrtosQueueSendFromISRWithStatus(Disp2QBifThd,
                                                &disp2Bif,
                                                sizeof(disp2Bif));
    if (status != FLCN_OK)
    {
        //
        // Margining command is not going to be handled.
        //
        SOE_BREAKPOINT();
    }
    else
    {
        //
        // Keep the margining interrupts to SOE disabled until we
        // handle the current margining interrupt. Margining interrupts
        // will be re-enabled in bifServiceMarginingIntr_LS10
        //
        GIN_ISR_REG_WR32(LW_CTRL_SOE_INTR_UNITS_EN_CLEAR,
            DRF_NUM(_CTRL, _SOE_INTR_UNITS, _XTL_SOE, 1));
    }
}
