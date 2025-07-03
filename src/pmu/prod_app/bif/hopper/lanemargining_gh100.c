/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
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
#include "pmu_bar0.h"
#include "pmu_objpmu.h"
/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "config/g_bif_private.h"
#include "pmu_objbif.h"
#include "pmu_objfuse.h"
#include "pmu/pmumailboxid.h"

#include "dev_lw_xpl.h"
#include "dev_xtl_ep_pri.h"
#include "dev_xtl_ep_pcfg_gpu.h"
#include "dev_pwr_csb.h"
#include "dev_lw_xpl_addendum.h"
#include "dev_lw_xtl_addendum.h"
#include "dev_lw_xtl_ep_pcfg_gpu_addendum.h"
#include "dev_lw_uphy_dlm_addendum.h"
#include "dev_lw_uxl_addendum.h"
#include "dev_lw_uxl_pri_ctx.h"
#include "dev_ctrl.h"
#include "dev_ctrl_defines.h"
#include "dev_fuse.h"
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
static FLCN_STATUS s_bifHandleMarginCommand_GH100(LwU8 laneNum, LwU32 laneResetMask)
    GCC_ATTRIB_SECTION("imem_libBifMargining", "s_bifHandleMarginCommand_GH100");
static FLCN_STATUS s_bifScheduleNextStepMarginCmd_GH100(LwU32 marginRequestMask, LwU8 laneNum, LwBool *bQueuedStepMarginCmd)
    GCC_ATTRIB_SECTION("imem_libBifMargining", "s_bifScheduleNextStepMarginCmd_GH100");
static FLCN_STATUS s_bifHandleStepMarginCommand_GH100(LwU8 laneNum, LwU8 rcvrNum, LwU8 marginType, LwU8 usageModel)
    GCC_ATTRIB_SECTION("imem_libBifMargining", "s_bifHandleStepMarginCommand_GH100");
static FLCN_STATUS s_bifGetLaneNum(LwU32 laneSelectMask, LwU8 *pLaneNum)
    GCC_ATTRIB_SECTION("imem_libBifMargining", "s_bifGetLaneNum");
static FLCN_STATUS s_bifDoLaneMargining_GH100(LwU8 laneNum, LwU32 initialMarginCmd, LwU8 marginPayload)
    GCC_ATTRIB_SECTION("imem_libBifMargining", "s_bifDoLaneMargining_GH100");
static void s_bifEnableUphyMarginingGenspeed_GH100(LwU32 regAddress, LwBool bStartStepMargining)
    GCC_ATTRIB_SECTION("imem_libBifMargining", "s_bifEnableUphyMarginingGenspeed_GH100");
static void s_bifUpdateUphyCdrOffsetScale_GH100(LwU32 regAddress, LwBool bStartStepMargining, LwU8 cdrOffsetScale)
    GCC_ATTRIB_SECTION("imem_libBifMargining", "s_bifUpdateUphyCdrOffsetScale_GH100");
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief This function enables Lane Margining interrupts to PMU only when
 * Gen4 is supported and enabled.
 */
void
bifInitMarginingIntr_GH100(void)
{
    LwU32 irqMclr = 0U;
    LwU32 tempRegVal;
    LwU8  laneNo;

    // Lane Margining is supported only for Gen4 and above.
    if (!FLD_TEST_DRF(_RM_PMU, _BIF_CAPS, _GEN4PCIE, _SUPPORTED, Bif.bifCaps) ||
        (!PMUCFG_FEATURE_ENABLED(PMU_BIF_LANE_MARGINING)) ||
        (Bif.bMarginingRegkeyEnabled == LW_FALSE))
    {
        //
        // Disable LM interrupt
        // We are NOT clearing the LW_CPWR_FALCON_IRQMODE and
        // LW_CPWR_RISCV_IRQDEST interrupts set in icPreInitTopLevel_GHXXX
        //
        irqMclr = FLD_SET_DRF(_CPWR, _RISCV_IRQMCLR, _EXT_RSVD8, _SET, irqMclr);
        REG_WR32(CSB, LW_CPWR_RISCV_IRQMCLR, irqMclr);
        return;
    }

    // Enable LM at XPL level
    for (laneNo = 0; laneNo < LW_XPL_PL_LM_LANE_INTERRUPT__SIZE_1; laneNo++)
    {
        tempRegVal = XPL_REG_RD32(BAR0, LW_XPL_PL_LM_LANE_INTERRUPT(laneNo));
        tempRegVal = FLD_SET_DRF_NUM(_XPL, _PL_LM_LANE_INTERRUPT,
                                     _MARGIN_CMD_INT_EN, 0x1, tempRegVal);
        tempRegVal = FLD_SET_DRF_NUM(_XPL, _PL_LM_LANE_INTERRUPT,
                                     _HW_STATUS_INT_EN, 0x1, tempRegVal);
        tempRegVal = FLD_SET_DRF_NUM(_XPL, _PL_LM_LANE_INTERRUPT,
                                     _ERR_LIMIT_EXCEED_INT_EN, 0x1, tempRegVal);
        XPL_REG_WR32(BAR0, LW_XPL_PL_LM_LANE_INTERRUPT(laneNo), tempRegVal);
    }

    XPL_REG_WR32(BAR0, LW_XPL_PL_LM_INTERRUPT_EN,
                 LW_XPL_PL_LM_INTERRUPT_EN_ENABLE_ALL);

    // Without this enable bit HW_MARGIN_READY interrupt is never pending
    tempRegVal = XPL_REG_RD32(BAR0, LW_XPL_PL_LM_EN);
    tempRegVal = FLD_SET_DRF_NUM(_XPL, _PL_LM_EN,
                                 _LANE_MARGIN_ENABLE, 0x1, tempRegVal);
    XPL_REG_WR32(BAR0, LW_XPL_PL_LM_EN, tempRegVal);

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
bifServiceMarginingIntr_GH100(void)
{
    LwU32       regVal;
    FLCN_STATUS status    = FLCN_OK;
    LwU32       val       = 0;
    const LwU32 vector    = DRF_VAL(_CTRL, _INTR_CTRL_ACCESS_DEFINES, _VECTOR,
                                    LW_XTL_EP_PRI_INTR_CTRL_2_MESSAGE_INIT);
    const LwU32 leafReg   = LW_CTRL_INTR_GPU_VECTOR_TO_LEAF_REG(vector);
    const LwU32 leafVal   = LWBIT32(LW_CTRL_INTR_GPU_VECTOR_TO_LEAF_BIT(vector));

    regVal = XPL_REG_RD32(BAR0, LW_XPL_PL_LM_INTERRUPT_STATUS);
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
    REG_WR32(CSB, LW_CPWR_PMU_INTR_LEAF(leafReg), leafVal);
    val = FLD_SET_DRF(_XTL_EP_PRI, _INTR_RETRIGGER, _TRIGGER, _TRUE, val);
    XTL_REG_WR32(LW_XTL_EP_PRI_INTR_RETRIGGER(2), val);

    //
    // Re-enable margining interrupt. These were disabled from
    // icServiceLaneMarginingInterrupts_GHXXX as soon as margining interrupt
    // on PMU was detected
    //
    regVal = XTL_REG_RD32(LW_XTL_EP_PRI_INTR_ENABLE(2));
    regVal = FLD_SET_DRF(_XTL, _EP_PRI_INTR_ENABLE, _XPL_LANE_MARGINING,
                         _ENABLED, regVal);
    XTL_REG_WR32(LW_XTL_EP_PRI_INTR_ENABLE(2), regVal);

    return status;
}

/*!
 * @brief Handle Margining Ready interrupt
 *
 * @param[in] regVal Current value of LW_XPL_PL_LM_INTERRUPT_STATUS register
 */
void
bifHandleMarginingReadyIntr_GH100
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
    if (PMUCFG_FEATURE_ENABLED(PMU_BIF_LANE_MARGINING)
        && bifMarginingEnabled_HAL(&Bif))
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
    XPL_REG_WR32(BAR0, LW_XPL_PL_LM_INTERRUPT_STATUS, regVal);

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
bifMarginingEnabled_GH100(void)
{
    LwU32 fuseVal;
    //
    // Please do not change the default value of this. Rest of the code
    // assumes default value to be false
    //
    LwBool bMarginingEnabled = LW_FALSE;

    fuseVal = fuseRead(LW_FUSE_OPT_LANE_MARGINING_CONTROL);

    if (FLD_TEST_DRF(_FUSE, _OPT_LANE_MARGINING_CONTROL, _DATA, _DISABLE,
                     fuseVal))
    {
        if (Bif.bMarginingRegkeyEnabled)
        {
            bMarginingEnabled = LW_TRUE;
        }
    }

    return bMarginingEnabled;
}

/*!
 * @brief Handle Margining Abort interrupt
 *
 * @param[in] regVal Current value of LW_XPL_PL_LM_INTERRUPT_STATUS register
 */
void
bifHandleMarginingAbortIntr_GH100
(
    LwU32 regVal
)
{
    LwU32 tempRegVal;
    LwU8  laneNum;
    MARGINING_DATA_GET()->marginingScheduledMask = 0U;

    //
    // Stop margining for all lanes
    // In the UXL interface, multi cast read is not supported
    // hence we have to loop through each lane
    //
    for (laneNum = 0; laneNum < BIF_MAX_PCIE_LANES; laneNum++)
    {
        bifStopStepMargining_HAL(&Bif, BIT(laneNum), LW_TRUE);
    }

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
    XPL_REG_WR32(BAR0, LW_XPL_PL_LM_INTERRUPT_STATUS, regVal);
}

/*!
 * @brief Stop margining for the specified lanes(using laneSelectMask).
 *
 * Steps to stop margining:
 * 1. Select Lane
 * 2. Set margin_en to 0
 * 3. Disable margining for Gen4
 * 4. Disable margining for Gen5
 * 5. Reset CDR_offset_scale to default
 *
 * @param[in] laneSelectMask   Lane number for which Step Margin needs to be
 *                             stopped. For 'i'th lane to be selected,
 *                             bit 'i' needs to be set in the laneSelectMask
 * @param[in] bResetErrorCount For Hopper, this has no effect. Added for HAL
 *                             compatibility purpose.
 */
void
bifStopStepMargining_GH100
(
    LwU32  laneSelectMask,
    LwBool bResetErrorCount
)
{
    LwU32 regVal;
    LwU8  laneNum;
    LwU8  *pLastStepMarginOffset;
    LwU8  *pLastStepMarginType;

    // Step 1. Select the desired lane(s) in PHY
    regVal = BAR0_REG_RD32(LW_UXL_LANE_COMMON_MCAST_CTL);
    regVal = FLD_SET_DRF_NUM(_UXL, _LANE_COMMON_MCAST_CTL, _LANE_MASK,
                             laneSelectMask, regVal);
    BAR0_REG_WR32(LW_UXL_LANE_COMMON_MCAST_CTL, regVal);

    // Step 2. Stop the margining by setting margin_en to 0
    regVal = 0U;
    regVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_RDS, 0x0U,
                             regVal);
    regVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_WDS, 0x1U,
                             regVal);
    regVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_ADDR,
                             LW_UPHY_DLM_AE_MARGIN_CTRL0, regVal);
    regVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_WDATA_MARGIN_EN,
                             0x0U, regVal);
    BAR0_REG_WR32(LW_UXL_LANE_REG_DIRECT_CTL_1(0), regVal);

    // Step 3. Disable margining for Gen4 speed
    s_bifEnableUphyMarginingGenspeed_GH100(LW_UPHY_DLM_MGMT_RX_RATE_CTRL_1_GEN4, LW_FALSE);

    // Step 4. Disable margining for Gen5 speed
    s_bifEnableUphyMarginingGenspeed_GH100(LW_UPHY_DLM_MGMT_RX_RATE_CTRL_1_GEN5, LW_FALSE);

    // Step 5. CDR_offset_scale reset to default
    s_bifUpdateUphyCdrOffsetScale_GH100(LW_UPHY_DLM_AE_EOM_FOM_CDR_OFFSET_CTRL, LW_FALSE,
                                        LW_UXL_LANE_REG_DIRECT_CTL_2_CFG_RDATA_CDR_OFFSET_SCALE_DEFAULT);

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
bifHandleMarginCommandIntr_GH100
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
        goto bifHandleMarginCommandIntr_GH100_exit;
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

    tempRegVal = XPL_REG_RD32(BAR0, LW_XPL_PL_LM_LANE_INTERRUPT(laneNum));
    if (FLD_TEST_DRF(_XPL, _PL_LM_LANE_INTERRUPT, _ERR_LIMIT_EXCEED_INT_STATUS,
                     _PENDING, tempRegVal))
    {
        // Handler for _ERR_LIMIT_EXCEED_INT_STATUS
        status = bifHandleErrorCountExcdIntr_HAL(&Bif, laneResetMask);
        // Step 2: RESET only the PENDING bit of interrupt being handled here
        laneInterruptMask = FLD_SET_DRF(_XPL, _PL_LM_LANE_INTERRUPT,
                                        _ERR_LIMIT_EXCEED_INT_STATUS, _RESET,
                                        laneInterruptMask);
    }

    if (FLD_TEST_DRF(_XPL, _PL_LM_LANE_INTERRUPT, _MARGIN_CMD_INT_STATUS,
                          _PENDING, tempRegVal))
    {
        // Now handle the actual margining command for the identified lane
        status = s_bifHandleMarginCommand_GH100(laneNum, laneResetMask);
        // Step 2: RESET only the PENDING bit of interrupt being handled here
        laneInterruptMask = FLD_SET_DRF(_XPL, _PL_LM_LANE_INTERRUPT,
                                        _MARGIN_CMD_INT_STATUS, _RESET,
                                        laneInterruptMask);
    }

    if (FLD_TEST_DRF(_XPL, _PL_LM_LANE_INTERRUPT, _HW_STATUS_INT_STATUS,
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
    XPL_REG_WR32(BAR0, LW_XPL_PL_LM_LANE_INTERRUPT(laneNum), laneInterruptMask);
    XPL_REG_WR32(BAR0, LW_XPL_PL_LM_INTERRUPT_STATUS, laneResetMask);

bifHandleMarginCommandIntr_GH100_exit:
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
bifHandleErrorCountExcdIntr_GH100
(
    LwU32 laneResetMask
)
{
    LwU8        laneNum;
    FLCN_STATUS status = FLCN_OK;

    //
    // We could not find any lane with the pending interrupt bit
    // Maybe a false interrupt. Ignoring is safe.
    //
    status = s_bifGetLaneNum(laneResetMask, &laneNum);
    if (status != FLCN_OK ||
        laneNum == BIF_MAX_PCIE_LANES)
    {
        goto bifHandleErrorCountExcdIntr_GH100_exit;
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
        goto bifHandleErrorCountExcdIntr_GH100_exit;
    }

    bifEnableMarginErrorCounting_HAL(&Bif, laneNum, LW_FALSE);

    //
    // For task LOWLATENCY, following statement is as good as atomic since
    // this task runs at priority of 4 and task BIF won't be able to preempt it
    //
    MARGINING_DATA_GET()->marginingScheduledMask &= ~(laneResetMask);

bifHandleErrorCountExcdIntr_GH100_exit:
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
bifUpdateStepMarginingExecStatus_GH100
(
    LwU8 laneNum,
    LwU8 execStatus
)
{
    LwU32       regVal;
    LwU8        rcvrNum;
    LwU8        marginType;
    LwU8        usageModel;
    LwU8        errorCount = 0;
    FLCN_STATUS status = FLCN_OK;

    regVal     = bifBar0ConfigSpaceRead_HAL(&Bif, LW_FALSE,
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
            status = bifGetMarginingErrorCount_HAL(&Bif, BIT(laneNum),
                                                   &errorCount);
            regVal = FLD_SET_DRF_NUM(_XTL, _EP_PRI_MARGINING_SHADOW_LANE_STATUS,
                                     _MARGIN_PAYLOAD_ERROR_COUNT,
                                     errorCount, regVal);
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
            REG_WR32(CSB,
                     LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_BIF_LANE_MARGINING),
                     status);
            PMU_BREAKPOINT();
            goto bifUpdateStepMarginingExecStatus_GH100_exit;
        }
    }

    XTL_REG_WR32(LW_XTL_EP_PRI_MARGINING_SHADOW_LANE_STATUS, regVal);

bifUpdateStepMarginingExecStatus_GH100_exit:
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
s_bifHandleMarginCommand_GH100
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
    LwU32 errorCountLimit;

    FLCN_STATUS status = FLCN_OK;

    // Read margin ctrl register and decide what margining command is issued
    regVal = bifBar0ConfigSpaceRead_HAL(&Bif, LW_FALSE,
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
                REG_WR32(CSB,
                    LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_BIF_LANE_MARGINING),
                    status);
                goto s_bifHandleMarginCommand_GH100_exit;
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
                    REG_WR32(CSB,
                        LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_BIF_LANE_MARGINING),
                        status);
                    goto s_bifHandleMarginCommand_GH100_exit;
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
                    REG_WR32(CSB,
                        LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_BIF_LANE_MARGINING),
                        status);
                    goto s_bifHandleMarginCommand_GH100_exit;
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
                    REG_WR32(CSB,
                        LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_BIF_LANE_MARGINING),
                        status);
                    goto s_bifHandleMarginCommand_GH100_exit;
                }

                //
                // For Set Error Count Limit, MarginPayload[7:6] => 11b,
                // MarginPayload[5:0] => Error Count Limit
                //
                regVal = XPL_REG_RD32(BAR0, LW_XPL_PL_LM_LANE_CONTROL(laneNum));
                errorCountLimit = marginPayload &
                                  PCIE_LANE_MARGIN_PAYLOAD_SET_ERROR_COUNT_VALUE_MASK;
                regVal = FLD_SET_DRF_NUM(_XPL, _PL_LM_LANE_CONTROL,
                                         _LIMIT_ERROR_COUNT, errorCountLimit,
                                         regVal);
                XPL_REG_WR32(BAR0, LW_XPL_PL_LM_LANE_CONTROL(laneNum), regVal);
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
                REG_WR32(CSB,
                    LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_BIF_LANE_MARGINING),
                    status);
                goto s_bifHandleMarginCommand_GH100_exit;
            }

            status = s_bifHandleStepMarginCommand_GH100(laneNum, rcvrNum,
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
                PMU_BREAKPOINT();
                goto s_bifHandleMarginCommand_GH100_exit;
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

s_bifHandleMarginCommand_GH100_exit:
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
bifUpdateMarginingStatus_GH100
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
bifEnableMarginErrorCounting_GH100
(
    LwU8   laneNum,
    LwBool bEnableErrorCounting
)
{
    LwU32       regVal;
    FLCN_STATUS status = FLCN_OK;

    regVal = XPL_REG_RD32(BAR0, LW_XPL_PL_LM_LANE_CONTROL(laneNum));
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
    XPL_REG_WR32(BAR0, LW_XPL_PL_LM_LANE_CONTROL(laneNum), regVal);

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
bifClearErrorLog_GH100
(
    LwU8 laneNum
)
{
    LwU32       regVal;
    FLCN_STATUS status = FLCN_OK;

    if (laneNum >= BIF_MAX_PCIE_LANES)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        REG_WR32(CSB,
                 LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_BIF_LANE_MARGINING),
                 status);
        PMU_BREAKPOINT();
        goto bifClearErrorLog_GH100_exit;
    }

    regVal = XPL_REG_RD32(BAR0, LW_XPL_PL_LM_LANE_CONTROL(laneNum));
    regVal = FLD_SET_DRF_NUM(_XPL, _PL_LM_LANE_CONTROL, _CLEAR_ERR_COUNT,
                             0x1U, regVal);
    XPL_REG_WR32(BAR0, LW_XPL_PL_LM_LANE_CONTROL(laneNum), regVal);

bifClearErrorLog_GH100_exit:
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
s_bifHandleStepMarginCommand_GH100
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
    if (PMUCFG_FEATURE_ENABLED(PMUTASK_BIF))
    {
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
            DISPATCH_BIF dispatchBif;

            dispatchBif.hdr.eventType = BIF_EVENT_ID_LANE_MARGIN;
            dispatchBif.signalLaneMargining.laneNum = laneNum;
            status = lwrtosQueueIdSend(LWOS_QUEUE_ID(PMU, BIF),
                                       &dispatchBif,
                                       sizeof(dispatchBif),
                                       0U);
            if (status == FLCN_OK)
            {
                MARGINING_DATA_GET()->marginingScheduledMask |= BIT(laneNum);
                MARGINING_DATA_GET()->bStepMarginCmdQueuedOrInProgress = LW_TRUE;
            }
            else
            {
                PMU_BREAKPOINT();
                goto s_bifHandleStepMarginCommand_GH100_exit;
            }
        }
    }
    else
    {
        //
        // Control shouldn't be here because if bif task feature is
        // disabled we are going to indicate 'no support' for lane
        // margining. If control is here it means application has
        // ignored no support and issued further margining commands
        // anyway
        //
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto s_bifHandleStepMarginCommand_GH100_exit;
    }

s_bifHandleStepMarginCommand_GH100_exit:
    return status;
}

/*!
 * @brief Helper function for bifDoLaneMargining_GH100
 *
 * @param[in] laneNum          Lane number
 * @param[in] initialMarginCmd Margining command stored for later reference
 * @param[in] marginOffset     Margin payload offset
 */
static FLCN_STATUS
s_bifDoLaneMargining_GH100
(
    LwU8  laneNum,
    LwU32 initialMarginCmd,
    LwU8  marginOffset
)
{
    LwU32       regVal;
    LwU8        errorCount;
    LwBool      bMarginingInProgress;
    LwU8        *pLastStepMarginOffset;
    LwU8        *pLastStepMarginType;
    FLCN_STATUS status = FLCN_OK;
    LwU8        rcvrNum;
    LwU8        marginType;
    LwU8        usageModel;

    pLastStepMarginType   = MARGINING_DATA_GET()->lastStepMarginType;
    pLastStepMarginOffset = MARGINING_DATA_GET()->lastStepMarginOffset;

    //
    // For rcvrNum, marginType and usageModel, just need to reflect
    // back what is there in ctrl register's payload
    //
    rcvrNum      = DRF_VAL(_EP_PCFG_GPU, _MARGINING_LANE_CTRL_STATUS_REGISTER,
                           _RECEIVER_NUMBER, initialMarginCmd);
    marginType   = DRF_VAL(_EP_PCFG_GPU, _MARGINING_LANE_CTRL_STATUS_REGISTER,
                           _MARGIN_TYPE, initialMarginCmd);
    usageModel   = DRF_VAL(_EP_PCFG_GPU, _MARGINING_LANE_CTRL_STATUS_REGISTER,
                           _USAGE_MODEL, initialMarginCmd);

    //
    // If this is not the same margining command as that of last
    // margining command then we need to initiate a new margining
    // request in UPHY
    //
    appTaskCriticalEnter();
    if ((marginOffset != pLastStepMarginOffset[laneNum])||
        (marginType   != pLastStepMarginType[laneNum]))
    {
        appTaskCriticalExit();
        status = bifStartStepMargining_HAL(&Bif, marginType,
                                           marginOffset, BIT(laneNum));
        if (status != FLCN_OK)
        {
            return status;
        }
    }
    else
    {
        appTaskCriticalExit();
    }

    status = bifGetMarginingErrorCount_HAL(&Bif, BIT(laneNum),
                                           &errorCount);
    if (status != FLCN_OK)
    {
        return status;
    }

    regVal = 0U;
    regVal = FLD_SET_DRF_NUM(_XTL, _EP_PRI_MARGINING_SHADOW_LANE_STATUS,
                             _RCVR_NUM, rcvrNum, regVal);
    regVal = FLD_SET_DRF_NUM(_XTL, _EP_PRI_MARGINING_SHADOW_LANE_STATUS,
                             _MARGIN_TYPE, marginType, regVal);
    regVal = FLD_SET_DRF_NUM(_XTL, _EP_PRI_MARGINING_SHADOW_LANE_STATUS,
                             _USAGE_MODEL, usageModel, regVal);
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
    appTaskCriticalEnter();
    {
        //
        // task LOWLATENCY can stop margininig without this thread's
        // knowledge. If that is the case, do not overwrite margining
        // status for this lane.
        //
        bMarginingInProgress =
            ((BIT(laneNum) & MARGINING_DATA_GET()->marginingScheduledMask) != 0U);

        if (bMarginingInProgress &&
            (initialMarginCmd == bifBar0ConfigSpaceRead_HAL(&Bif, LW_FALSE,
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
    appTaskCriticalExit();

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
bifDoLaneMargining_GH100
(
    LwU8 laneNum
)
{
    LwU32       regVal;
    LwU8        tempRegVal;
    LwU32       initialMarginCmd;
    LwU32       tempMarginMask;
    LwU8        marginOffset;
    LwU8        marginPayload;
    LwU8        marginType;
    LwBool      bMarginDirectionLeft = LW_FALSE;
    LwBool      bMarginDirectionDown = LW_FALSE;
    LwBool      bQueuedStepMarginCmd = LW_FALSE;
    FLCN_STATUS status               = FLCN_OK;

    // Read margin ctrl register and decide what margining command is issued
    regVal = bifBar0ConfigSpaceRead_HAL(&Bif, LW_FALSE,
                 LW_EP_PCFG_GPU_MARGINING_LANE_CTRL_STATUS_REGISTER(laneNum));

    // Store the margining command seen for later reference
    initialMarginCmd = regVal;

    marginType   = DRF_VAL(_EP_PCFG_GPU, _MARGINING_LANE_CTRL_STATUS_REGISTER,
                           _MARGIN_TYPE, regVal);
    marginOffset = DRF_VAL(_EP_PCFG_GPU, _MARGINING_LANE_CTRL_STATUS_REGISTER,
                           _MARGIN_PAYLOAD, regVal);

    switch (marginType)
    {
        case PCIE_LANE_MARGIN_TYPE_3:
        {
            //
            // For Hopper, MIndLeftRightTiming is 1 and therefore
            // Margin Payload [6]   : Indicates go to left or right.
            // A 0b indicates to move the receiver to the right of the normal
            // setting whereas a 1b indicates to move the receiver to the left
            // of the normal setting.
            // Margin Payload [5:0] : Indicates the number of steps beyond
            // the normal setting.
            //
            bMarginDirectionLeft = FLD_TEST_REF(BIF_MARGINING_MARGIN_PAYLOAD_OFFSET_TIME_DIRECTION,
                                                _LEFT, marginOffset);

            if (bMarginDirectionLeft == LW_TRUE)
            {
                //
                // For UPHY layer to understand that the direction is changed
                // we need to set the MSB of the payload.
                // UPHY's SW logic will then take 2's complement of the payload
                // value to find the number of steps and move the data sampler
                // in the opposite direction.
                // Same thing applies for voltage margining too.
                //
                tempRegVal = REF_VAL(BIF_MARGINING_MARGIN_PAYLOAD_OFFSET_TIME_PAYLOAD,
                                     marginOffset);
                // Maximum steps allowed in left direction is 16
                if (tempRegVal > 0x10)
                {
                    goto bifDoLaneMargining_GH100_exit;
                }
                tempRegVal    = BIF_TWOS_COMPLEMENT_GET(tempRegVal);
                marginPayload = 0;
                marginPayload = REF_VAL(BIF_MARGINING_MARGIN_PAYLOAD_OFFSET_TIME_PAYLOAD,
                                        tempRegVal);
            }
            else
            {
                marginPayload = marginOffset;
                // Maximum steps allowed in right direction is 15
                if (marginPayload > 0xF)
                {
                    goto bifDoLaneMargining_GH100_exit;
                }
            }

            status = s_bifDoLaneMargining_GH100(laneNum, initialMarginCmd, marginPayload);
            if (status != FLCN_OK)
            {
                goto bifDoLaneMargining_GH100_exit;
            }

            break;
        }
        case PCIE_LANE_MARGIN_TYPE_4:
        {
            //
            // For Hopper, MIndUpDowlwoltage is 1 and therefore
            // Margin Payload [7]   : Indicates go to up or down.
            // A 0b indicates to move the receiver up from the normal
            // setting whereas a 1b indicates to move the receiver down from
            // the normal setting.
            // Margin Payload [6:0] : Indicates the number of steps beyond
            // the normal setting.
            //
            bMarginDirectionDown = FLD_TEST_REF(BIF_MARGINING_MARGIN_PAYLOAD_OFFSET_VOLTAGE_DIRECTION,
                                                _DOWN, marginOffset);

            if (bMarginDirectionDown == LW_TRUE)
            {
                tempRegVal = REF_VAL(BIF_MARGINING_MARGIN_PAYLOAD_OFFSET_VOLTAGE_PAYLOAD,
                                     marginOffset);
                // Maximum steps allowed in down direction is 64
                if (tempRegVal > 0x40)
                {
                    goto bifDoLaneMargining_GH100_exit;
                }
                tempRegVal    = BIF_TWOS_COMPLEMENT_GET(tempRegVal);
                // 2's complement 
                marginPayload = 0;
                marginPayload = REF_VAL(BIF_MARGINING_MARGIN_PAYLOAD_OFFSET_VOLTAGE_PAYLOAD,
                                        tempRegVal);
            }
            else
            {
                marginPayload = marginOffset;
                // Maximum steps allowed in up direction is 63
                if (marginPayload > 0x3F)
                {
                    goto bifDoLaneMargining_GH100_exit;
                }
            }

            status = s_bifDoLaneMargining_GH100(laneNum, initialMarginCmd, marginPayload);
            if (status != FLCN_OK)
            {
                goto bifDoLaneMargining_GH100_exit;
            }

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
        goto bifDoLaneMargining_GH100_exit;
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
        status = s_bifScheduleNextStepMarginCmd_GH100(tempMarginMask, laneNum,
                                                      &bQueuedStepMarginCmd);
        if (status != FLCN_OK)
        {
            goto bifDoLaneMargining_GH100_exit;
        }
    }

bifDoLaneMargining_GH100_exit:
    //
    // We want to update bStepMarginCmdQueuedOrInProgress to False when
    // queue slot for Step Margin command is going to be free that is this
    // function is going to exit without adding another command to task
    // BIF's queue for Step Margining
    //
    if (!bQueuedStepMarginCmd)
    {
        appTaskCriticalEnter();
        //
        // Before exiting the Step Margin loop, make sure that task Lowlatency
        // is not requesting Step Margin from task BIF. Before this point,
        // task LL thinks that task BIF will handle Step Margin command,
        // so let's honor that assumption here
        //
        if (MARGINING_DATA_GET()->marginingScheduledMask == 0U)
        {
            MARGINING_DATA_GET()->bStepMarginCmdQueuedOrInProgress = LW_FALSE;
            appTaskCriticalExit();
            return status;
        }
        else
        {
            appTaskCriticalExit();
            tempMarginMask = MARGINING_DATA_GET()->marginingScheduledMask;
            status = s_bifScheduleNextStepMarginCmd_GH100(tempMarginMask,
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
                appTaskCriticalEnter();
                MARGINING_DATA_GET()->bStepMarginCmdQueuedOrInProgress = LW_FALSE;
                appTaskCriticalExit();
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
s_bifScheduleNextStepMarginCmd_GH100
(
    LwU32  marginRequestMask,
    LwU8   laneNum,
    LwBool *pbQueuedStepMarginCmd
)
{
    DISPATCH_BIF dispatchBif;
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
        goto s_bifScheduleNextStepMarginCmd_GH100_exit;
    }

    do
    {
        laneNum++;
        if (laneNum >= BIF_MAX_PCIE_LANES)
        {
            laneNum = 0U;
        }
    } while ((BIT(laneNum) & marginRequestMask) == 0U);

    dispatchBif.hdr.eventType = BIF_EVENT_ID_LANE_MARGIN;
    dispatchBif.signalLaneMargining.laneNum = laneNum;
    status = lwrtosQueueIdSend(LWOS_QUEUE_ID(PMU, BIF), &dispatchBif,
                               sizeof(dispatchBif),
                               BIF_STEP_MARGINING_TIMEOUT_US);
    if (status != FLCN_OK)
    {
        //
        // Step margining command is not going to be handled for this lane.
        // Indicate the reason for failure in the mailbox register.
        //
        REG_WR32(CSB,
                 LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_BIF_LANE_MARGINING),
                 status);
        PMU_BREAKPOINT();
    }
    else
    {
        *pbQueuedStepMarginCmd = LW_TRUE;
    }

s_bifScheduleNextStepMarginCmd_GH100_exit:
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
        REG_WR32(CSB,
                 LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_BIF_LANE_MARGINING),
                 status);
        PMU_BREAKPOINT();
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
        REG_WR32(CSB,
                 LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_BIF_LANE_MARGINING),
                 status);
        PMU_BREAKPOINT();
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
bifGetMarginingErrorCount_GH100
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
        REG_WR32(CSB,
                 LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_BIF_LANE_MARGINING),
                 status);
        PMU_BREAKPOINT();
        goto bifGetMarginingErrorCount_GH100_exit;
    }

    status = s_bifGetLaneNum(laneSelectMask, &laneNum);
    if (status != FLCN_OK)
    {
        goto bifGetMarginingErrorCount_GH100_exit;
    }

    tempRegVal = XPL_REG_RD32(BAR0, LW_XPL_PL_LM_LANE_CONTROL(laneNum));
    *pErrorCount = DRF_VAL(_XPL, _PL_LM_LANE_CONTROL, _ERROR_COUNT, tempRegVal);

bifGetMarginingErrorCount_GH100_exit:
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
 * Steps to start margining:
 * 1. Select lane
 * 2. Enable margining for Gen4
 * 3. Enable margining for Gen5
 * 4. Set CDR_offset_scale
 * 5. Set margin_en to 0
 *
 * @param[in] marginType        Margin type specified in the margining command
 * @param[in] marginOffset      Margin offset specified in the margining command
 * @param[in] laneSelectMask    Lane number for which margining has to be
 *                              started. For 'i'th lane to be selected, bit 'i'
 *                              needs to be set in the laneSelectMask
 */
FLCN_STATUS
bifStartStepMargining_GH100
(
    LwU8  marginType,
    LwU8  marginOffset,
    LwU32 laneSelectMask
)
{
    LwU8        laneNum;
    LwU32       regVal;
    FLCN_STATUS status = FLCN_OK;

    // Make sure that margining params are valid.
    if ((marginType == BIF_ILWALID_STEP_MARGIN_TYPE) ||
        (marginOffset == BIF_ILWALID_STEP_MARGIN_OFFSET) ||
        (laneSelectMask == 0U))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        REG_WR32(CSB,
                 LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_BIF_LANE_MARGINING),
                 status);
        PMU_BREAKPOINT();
        goto bifStartStepMargining_GH100_exit;
    }

    // Step 1. Select lane
    regVal = BAR0_REG_RD32(LW_UXL_LANE_COMMON_MCAST_CTL);
    regVal = FLD_SET_DRF_NUM(_UXL, _LANE_COMMON_MCAST_CTL, _LANE_MASK,
                             laneSelectMask, regVal);
    //
    // Todo by anaikwade: Pre-Hopper we assume that task LOWLATENCY won't
    //                    change laneSelectMask(value in *_MCAST_CTL).
    //                    Need to remove that assumption. Bug 200753824.
    //
    appTaskCriticalEnter();
    BAR0_REG_WR32(LW_UXL_LANE_COMMON_MCAST_CTL, regVal);
    appTaskCriticalExit();

    // Step 2. Enable margining for Gen4 speed
    s_bifEnableUphyMarginingGenspeed_GH100(LW_UPHY_DLM_MGMT_RX_RATE_CTRL_1_GEN4, LW_TRUE);

    // Step 3. Enable margining for Gen5 speed
    s_bifEnableUphyMarginingGenspeed_GH100(LW_UPHY_DLM_MGMT_RX_RATE_CTRL_1_GEN5, LW_TRUE);

    //
    // Step 4. Set CDR_offset_scale
    // Todo by anaikwade: This value should be made regkey programmable
    //                    as this could vary depending on the system
    //
    s_bifUpdateUphyCdrOffsetScale_GH100(LW_UPHY_DLM_AE_EOM_FOM_CDR_OFFSET_CTRL, LW_TRUE,
                                        LW_UXL_LANE_REG_DIRECT_CTL_2_CFG_RDATA_CDR_OFFSET_SCALE_DEFAULT);

    // Step 5. Start step margining at UPHY level
    regVal = 0U;
    regVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_RDS, 0x0U,
                             regVal);
    regVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_WDS, 0x1U,
                             regVal);
    regVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_ADDR,
                             LW_UPHY_DLM_AE_MARGIN_CTRL0, regVal);
    regVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_WDATA_MARGIN_EN,
                             0x1, regVal);

    // One step WRITE in PHY control register for margin offset + dir
    if (marginType == PCIE_LANE_MARGIN_TYPE_3)
    {
        //
        // For time-offset bits 12:8 are used. See following file for details
        // //dev/mixsig/uphy/6.1/rel/5.1/vmod/dlm/LW_UPHY_DLM_init_pkg.h
        //
        regVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_WDATA_MARGIN_T,
                                 marginOffset, regVal);
    }
    else
    {
        //
        // For voltage-offset bits 6:0 are used. See following file for details
        // //dev/mixsig/uphy/6.1/rel/5.1/vmod/dlm/LW_UPHY_DLM_init_pkg.h
        //
        regVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_WDATA_MARGIN_V,
                                 marginOffset, regVal);
    }
    //
    // Todo by anaikwade: Pre-Hopper we assume that task LOWLATENCY won't
    //                    change laneSelectMask(value in *_MCAST_CTL).
    //                    Need to remove that assumption. Bug 200753824.
    //
    appTaskCriticalEnter();
    BAR0_REG_WR32(LW_UXL_LANE_REG_DIRECT_CTL_1(0), regVal);
    appTaskCriticalExit();

    status = s_bifGetLaneNum(laneSelectMask, &laneNum);
    if (status != FLCN_OK)
    {
        REG_WR32(CSB,
                 LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_BIF_LANE_MARGINING),
                 status);
        PMU_BREAKPOINT();
        goto bifStartStepMargining_GH100_exit;
    }

    // Enable error counting for the requested lane
    bifEnableMarginErrorCounting_HAL(&Bif, laneNum, LW_TRUE);

bifStartStepMargining_GH100_exit:
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
bifHandleReportMarginingCommand_GH100
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
            regVal = XPL_REG_RD32(BAR0, LW_XPL_PL_LM_CAPABILITY0);
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
            regVal = XPL_REG_RD32(BAR0, LW_XPL_PL_LM_CAPABILITY0);
            regVal = DRF_VAL(_XPL, _PL_LM_CAPABILITY0, _M_NUM_VOLTAGE_STEPS,
                             regVal);

            marginPayload = 0x0U;
            marginPayload = FLD_SET_REF_NUM(LW_XPL_LM_CAPABILITY_MARGIN_PAYLOAD_M_NUM_VOLTAGE_STEPS,
                                            regVal, marginPayload);
            break;
        }
        case PCIE_LANE_MARGIN_REPORT_NUM_TIMING_STEPS:
        {
            regVal = XPL_REG_RD32(BAR0, LW_XPL_PL_LM_CAPABILITY0);
            regVal = DRF_VAL(_XPL, _PL_LM_CAPABILITY0, _M_NUM_TIMING_STEPS,
                             regVal);

            marginPayload = 0x0U;
            marginPayload = FLD_SET_REF_NUM(LW_XPL_LM_CAPABILITY_MARGIN_PAYLOAD_M_NUM_TIMING_STEPS,
                                            regVal, marginPayload);
            break;
        }
        case PCIE_LANE_MARGIN_REPORT_MAX_TIMING_OFFSET:
        {
            regVal = XPL_REG_RD32(BAR0, LW_XPL_PL_LM_CAPABILITY0);
            regVal = DRF_VAL(_XPL, _PL_LM_CAPABILITY0, _M_MAX_TIMING_OFFSET, regVal);

            marginPayload = 0x0U;
            marginPayload = FLD_SET_REF_NUM(LW_XPL_LM_CAPABILITY_MARGIN_PAYLOAD_M_MAX_TIMING_OFFSET,
                                            regVal, marginPayload);
            break;
        }
        case PCIE_LANE_MARGIN_REPORT_MAX_VOLTAGE_OFFSET:
        {
            regVal = XPL_REG_RD32(BAR0, LW_XPL_PL_LM_CAPABILITY1);
            regVal = DRF_VAL(_XPL, _PL_LM_CAPABILITY1, _M_MAX_VOLTAGE_OFFSET, regVal);

            marginPayload = 0x0U;
            marginPayload = FLD_SET_REF_NUM(LW_XPL_LM_CAPABILITY_MARGIN_PAYLOAD_M_MAX_VOLTAGE_OFFSET,
                                            regVal, marginPayload);
            break;
        }
        case PCIE_LANE_MARGIN_REPORT_SAMPLING_RATE_VOLTAGE:
        {
            regVal = XPL_REG_RD32(BAR0, LW_XPL_PL_LM_CAPABILITY1);
            regVal = DRF_VAL(_XPL, _PL_LM_CAPABILITY1, _M_SAMPLING_RATE_VOLTAGE, regVal);

            marginPayload = 0x0U;
            marginPayload = FLD_SET_REF_NUM(LW_XPL_LM_CAPABILITY_MARGIN_PAYLOAD_M_SAMPLING_RATE_VOLTAGE,
                                            regVal, marginPayload);
            break;
        }
        case PCIE_LANE_MARGIN_REPORT_SAMPLING_RATE_TIMING:
        {
            regVal = XPL_REG_RD32(BAR0, LW_XPL_PL_LM_CAPABILITY1);
            regVal = DRF_VAL(_XPL, _PL_LM_CAPABILITY1, _M_SAMPLING_RATE_TIMING, regVal);

            marginPayload = 0x0U;
            marginPayload = FLD_SET_REF_NUM(LW_XPL_LM_CAPABILITY_MARGIN_PAYLOAD_M_SAMPLING_RATE_TIMING,
                                            regVal, marginPayload);
            break;
        }
        case PCIE_LANE_MARGIN_REPORT_SAMPLE_COUNT:
        {
            regVal = XPL_REG_RD32(BAR0, LW_XPL_PL_LM_CAPABILITY1);
            regVal = DRF_VAL(_XPL, _PL_LM_CAPABILITY1, _M_SAMPLE_COUNT, regVal);

            marginPayload = 0x0U;
            marginPayload = FLD_SET_REF_NUM(LW_XPL_LM_CAPABILITY_MARGIN_PAYLOAD_M_SAMPLE_COUNT,
                                            regVal, marginPayload);
            break;
        }
        case PCIE_LANE_MARGIN_REPORT_MAX_LANES:
        {
            regVal = XPL_REG_RD32(BAR0, LW_XPL_PL_LM_CAPABILITY2);
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
 * @brief Enable margining for particular gen speed
 *
 * Steps to update UPHY config register:
 * 1. Program UXL interface (*CTL_1) to read the register address
 * 2. Actually read the value using UXL interface (*CTL_2)
 * 3. Modify the required bits
 * 4. Write back to the register address through UXL interface (*CTL_1)
 *
 * Todo by anaikwade: Evaluate all the critical sections mentioned in this file
 *
 * @param[in] regAddress          register address
 * @param[in] bStartStepMargining True if in start step margining code path
 */
static void
s_bifEnableUphyMarginingGenspeed_GH100
(
    LwU32  regAddress,
    LwBool bStartStepMargining
)
{
    LwU32 regVal;
    LwU32 rData;

    // Step 1. Prepare UXL interface to read the existing value in regAddress
    regVal = 0U;
    regVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_RDS, 0x1U,
                             regVal);
    regVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_WDS, 0x0U,
                             regVal);
    regVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_ADDR, regAddress,
                             regVal);

    if (bStartStepMargining)
    {
        appTaskCriticalEnter();
        BAR0_REG_WR32(LW_UXL_LANE_REG_DIRECT_CTL_1(0), regVal);

        // Step 2. Actually read the existing value in regAddress
        rData = BAR0_REG_RD32(LW_UXL_LANE_REG_DIRECT_CTL_2(0));
        appTaskCriticalExit();
    }
    else
    {
        BAR0_REG_WR32(LW_UXL_LANE_REG_DIRECT_CTL_1(0), regVal);

        // Step 2. Actually read the existing value in regAddress
        rData = BAR0_REG_RD32(LW_UXL_LANE_REG_DIRECT_CTL_2(0));
    }

    rData = DRF_VAL(_UXL, _LANE_REG_DIRECT_CTL_2, _CFG_RDATA, rData);

    // Step 3. Modify only the required aeintf_en bit
    if (bStartStepMargining)
    {
        rData = FLD_SET_DRF(_UXL, _LANE_REG_DIRECT_CTL_2, _CFG_RDATA_MARGIN_AEINTF_EN,
                            _ENABLE, rData);
    }
    else
    {
        rData = FLD_SET_DRF(_UXL, _LANE_REG_DIRECT_CTL_2, _CFG_RDATA_MARGIN_AEINTF_EN,
                            _DISABLE, rData);
    }

    // Step 4. Write the updated value
    regVal = 0U;
    regVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_RDS, 0x0U,
                             regVal);
    regVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_WDS, 0x1U,
                             regVal);
    regVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_ADDR, regAddress,
                             regVal);
    regVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_WDATA, rData,
                             regVal);
    
    if (bStartStepMargining)
    {
        appTaskCriticalEnter();
        BAR0_REG_WR32(LW_UXL_LANE_REG_DIRECT_CTL_1(0), regVal);
        appTaskCriticalExit();
    }
    else
    {
        BAR0_REG_WR32(LW_UXL_LANE_REG_DIRECT_CTL_1(0), regVal);
    }
}

/*!
 * @brief Update CDR_offset_scale
 *
 * Steps to update UPHY config register:
 * 1. Program UXL interface (*CTL_1) to read the register address
 * 2. Actually read the value using UXL interface (*CTL_2)
 * 3. Modify the required bits
 * 4. Write back to the register address through UXL interface (*CTL_1)
 *
 * @param[in] regAddress          register address
 * @param[in] bStartStepMargining True if in start step margining code path
 * @param[in] cdrOffsetScale      value to disable margining
 */
static void
s_bifUpdateUphyCdrOffsetScale_GH100
(
    LwU32  regAddress,
    LwBool bStartStepMargining,
    LwU8   cdrOffsetScale
)
{
    LwU32 regVal;
    LwU32 rData;

    // Step 1. Prepare UXL interface to read the existing value in regAddress
    regVal = 0U;
    regVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_RDS, 0x1U,
                             regVal);
    regVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_WDS, 0x0U,
                             regVal);
    regVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_ADDR, regAddress,
                             regVal);

    if (bStartStepMargining)
    {
        appTaskCriticalEnter();
        BAR0_REG_WR32(LW_UXL_LANE_REG_DIRECT_CTL_1(0), regVal);

        // Step 2. Actually read the existing value in regAddress
        rData = BAR0_REG_RD32(LW_UXL_LANE_REG_DIRECT_CTL_2(0));
        appTaskCriticalExit();
    }
    else
    {
        BAR0_REG_WR32(LW_UXL_LANE_REG_DIRECT_CTL_1(0), regVal);

        // Step 2. Actually read the existing value in regAddress
        rData = BAR0_REG_RD32(LW_UXL_LANE_REG_DIRECT_CTL_2(0));
    }

    rData = DRF_VAL(_UXL, _LANE_REG_DIRECT_CTL_2, _CFG_RDATA, rData);

    // Step 3. Modify only the required CDR_OFFSET_SCALE bits
    rData = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_2, _CFG_RDATA_CDR_OFFSET_SCALE,
                            cdrOffsetScale, rData);

    // Step 4. Write the updated value
    regVal = 0U;
    regVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_RDS, 0x0U,
                             regVal);
    regVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_WDS, 0x1U,
                             regVal);
    regVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_ADDR, regAddress,
                             regVal);
    regVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_WDATA, rData,
                             regVal);

    if (bStartStepMargining)
    {
        appTaskCriticalEnter();
        BAR0_REG_WR32(LW_UXL_LANE_REG_DIRECT_CTL_1(0), regVal);
        appTaskCriticalExit();
    }
    else
    {
        BAR0_REG_WR32(LW_UXL_LANE_REG_DIRECT_CTL_1(0), regVal);
    }
}
