/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   lanemargining_ga100.c
 * @brief  Contains GA100 based HAL implementations for Lane Margining
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmu_objpmu.h"

#include "dev_lw_xp.h"
#include "dev_lw_xve.h"
#include "dev_lw_xp_addendum.h"
#include "dev_lw_xve_addendum.h"
#include "dev_pwr_csb.h"
#include "dev_fuse.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "config/g_bif_private.h"
#include "pmu_objbif.h"
#include "pmu_objfuse.h"
#include "pmu/pmumailboxid.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
#if (PMU_PROFILE_GA100)
#define LW_UPHY_DLM_AE_MARGIN_CTRL0                                     0x238
#define LW_UPHY_DLM_AE_MARGIN_CTRL0_RESERVED1                           15:13
#define LW_UPHY_DLM_AE_MARGIN_CTRL0_MARGIN_EN                           12:12
#define LW_UPHY_DLM_AE_MARGIN_CTRL0_MARGIN_NBLKS_MAX                     11:8
#define LW_UPHY_DLM_AE_MARGIN_CTRL0_RESERVED0                             7:4
#define LW_UPHY_DLM_AE_MARGIN_CTRL0_MARGIN_X                              3:0

#define LW_XP_PEX_PAD_CTL_8_CFG_WDATA_MARGIN_EN                         12:12
#define LW_XP_PEX_PAD_CTL_8_CFG_WDATA_MARGIN_X                            3:0
#endif

static FLCN_STATUS s_bifGetMarginErrorCountBase_GA100(LwU8 laneNum, LwU32 *pBaseReg)
    GCC_ATTRIB_SECTION("imem_libBifMargining", "s_bifGetMarginErrorCountBase_GA100");

static FLCN_STATUS s_bifGetLaneNum(LwU32 laneSelectMask, LwU8 *pLaneNum)
    GCC_ATTRIB_SECTION("imem_libBifMargining", "s_bifGetLaneNum");

/*!
 * @brief This function which is called by lowlatency task handles the
 * margining interrupts. For step margining interrupts, this updates margining
 * status register and adds command in task BIF's queue if required
 *
 * @return FLCN_OK if interrupt is handled correctly
 */
FLCN_STATUS
bifServiceMarginingIntr_GA100(void)
{
    LwU32 regVal;
    LwU32 highEn;
    FLCN_STATUS status = FLCN_OK;

    regVal = BAR0_CFG_REG_RD32(LW_XVE_MARGINING_INTERRUPT);
    if (FLD_TEST_DRF(_XVE, _MARGINING_INTERRUPT, _READY, _PENDING, regVal))
    {
        bifHandleMarginingReadyIntr_HAL(&Bif, regVal);
    }
    else if (FLD_TEST_DRF(_XVE, _MARGINING_INTERRUPT, _ABORT, _PENDING, regVal))
    {
        bifHandleMarginingAbortIntr_HAL(&Bif, regVal);
    }
    else if (FLD_TEST_DRF(_XVE, _MARGINING_INTERRUPT, _ERROR_COUNT_LIMIT_EXCD,
                          _PENDING, regVal))
    {
        status = bifHandleErrorCountExcdIntr_HAL(&Bif, regVal);
        if (status != FLCN_OK)
        {
            goto bifServiceMarginingIntr_GA100_exit;
        }
    }
    // Lane specific interrupt is set
    else
    {
        status = bifHandleMarginCommandIntr_HAL(&Bif, regVal);
        if (status != FLCN_OK)
        {
            goto bifServiceMarginingIntr_GA100_exit;
        }
    }

bifServiceMarginingIntr_GA100_exit:
    //
    // Re-enable margining interrupts. These were disabled from
    // icServiceMiscIOBank2_TUXXX as soon as margining interrupt
    // on PMU was detected
    //
    highEn = REG_RD32(CSB, LW_CPWR_PMU_GPIO_2_INTR_HIGH_EN);
    highEn = FLD_SET_DRF(_CPWR_PMU, _GPIO_2_INTR_HIGH_EN, _XVE_MARGINING_INTR,
                         _ENABLED, highEn);
    REG_WR32_STALL(CSB, LW_CPWR_PMU_GPIO_2_INTR_HIGH_EN, highEn);

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
bifStartStepMargining_GA100
(
    LwU8  marginType,
    LwU8  marginOffset,
    LwU32 laneSelectMask
)
{
    LwU8        laneNum;
    LwU32       tempRegVal;
    LwU32       rData;
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
        goto bifStartStepMargining_GA100_exit;
    }

    tempRegVal = REG_RD32(FECS, LW_XP_PEX_PAD_CTL_SEL);
    tempRegVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_SEL, _LANE_SELECT,
                                 laneSelectMask, tempRegVal);
    REG_WR32(FECS, LW_XP_PEX_PAD_CTL_SEL, tempRegVal);

    //
    // First read the data from LW_UPHY_DLM_AE_MARGIN_CTRL0 and modify only
    // the margin_en and margin_x bits
    //
    tempRegVal = 0U;
    tempRegVal = FLD_SET_DRF(_XP, _PEX_PAD_CTL_8, _CFG_RESET, _INIT,
                             tempRegVal);
    tempRegVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_8, _CFG_RDS, 0x1U,
                                 tempRegVal);
    tempRegVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_8, _CFG_ADDR,
                                 LW_UPHY_DLM_AE_MARGIN_CTRL0, tempRegVal);
    REG_WR32(FECS, LW_XP_PEX_PAD_CTL_8, tempRegVal);
    // rData has value of LW_UPHY_DLM_AE_MARGIN_CTRL0
    rData = DRF_VAL(_XP, _PEX_PAD_CTL_9, _CFG_RDATA,
                    REG_RD32(FECS, LW_XP_PEX_PAD_CTL_9));

    //
    // tempRegVal has already been programmed with the PHY address and with
    // CFG_RESET value. RDS bit needs to be set to 0.
    //
    tempRegVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_8, _CFG_RDS, 0x0U,
                                 tempRegVal);
    tempRegVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_8, _CFG_WDS, 0x1U,
                                 tempRegVal);

    //
    // First set WDATA bits in tempRegVal to whatever we read from
    // LW_UPHY_DLM_AE_MARGIN_CTRL0
    //
    tempRegVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_8, _CFG_WDATA, rData,
                                 tempRegVal);
    tempRegVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_8, _CFG_WDATA_MARGIN_EN,
                                 0x1, tempRegVal);

    // 1 step WRITE in PHY control register for margin offset + dir
    if (marginType == PCIE_LANE_MARGIN_TYPE_3)
    {
        //
        // For X-offset bits 3:0 are used. See following file for details
        // //dev/mixsig/uphy/5.0/rel/ga100-fnl/vmod/dlm/LW_UPHY_DLM_init_pkg.h
        //
        tempRegVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_8, _CFG_WDATA_MARGIN_X,
                                     marginOffset, tempRegVal);
    }
    else
    {
        //
        // For GA100, HW does not have support for Y-offset
        //
    }
    REG_WR32(FECS, LW_XP_PEX_PAD_CTL_8, tempRegVal);

    status = s_bifGetLaneNum(laneSelectMask, &laneNum);
    if (status != FLCN_OK)
    {
        REG_WR32(CSB,
                 LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_BIF_LANE_MARGINING),
                 status);
        PMU_BREAKPOINT();
        goto bifStartStepMargining_GA100_exit;
    }

    // Enable error counting from XVE
    bifEnableMarginErrorCounting_HAL(&Bif, laneNum, LW_TRUE);

bifStartStepMargining_GA100_exit:
    return status;
}

/*!
 * @brief Stop margining for the specified lanes(using laneSelectMask).
 *
 * @param[in] laneSelectMask    Lane number for which Step Margin needs to be
 *                              stopped. For 'i'th lane to be selected,
 *                              bit 'i' needs to be set in the laneSelectMask
 * @param[in] bResetErrorCount  If set to true, reset the Margining error count
 *                              If set to false, do no reset this count
 */
void
bifStopStepMargining_GA100
(
    LwU32  laneSelectMask,
    LwBool bResetErrorCount
)
{
    LwU32 regVal;
    LwU32 rData;
    LwU8  laneNum;
    LwU8  *pLastStepMarginOffset;
    LwU8  *pLastStepMarginType;

    // Select the desired lane(s) in PHY
    regVal = REG_RD32(FECS, LW_XP_PEX_PAD_CTL_SEL);
    regVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_SEL,
                             _LANE_SELECT, laneSelectMask, regVal);
    REG_WR32(FECS, LW_XP_PEX_PAD_CTL_SEL, regVal);

    //
    // One step WRITE in PHY control register for Margin stop
    // First read the data from LW_UPHY_DLM_AE_MARGIN_CTRL0 and modify only
    // the margin_en and margin_x bits
    //
    regVal = 0U;
    regVal = FLD_SET_DRF(_XP, _PEX_PAD_CTL_8, _CFG_RESET, _INIT, regVal);
    regVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_8, _CFG_RDS, 0x1U, regVal);
    regVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_8, _CFG_ADDR,
                             LW_UPHY_DLM_AE_MARGIN_CTRL0, regVal);
    REG_WR32(FECS, LW_XP_PEX_PAD_CTL_8, regVal);
    // rData has value of LW_UPHY_DLM_AE_MARGIN_CTRL0
    rData = DRF_VAL(_XP, _PEX_PAD_CTL_9, _CFG_RDATA,
                    REG_RD32(FECS, LW_XP_PEX_PAD_CTL_9));

    //
    // RDS bit needs to be set to 0 while using WDS bit. Other bits are
    // already configured
    //
    regVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_8, _CFG_RDS, 0x0U, regVal);
    regVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_8, _CFG_WDS, 0x1U, regVal);
    regVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_8, _CFG_ADDR,
                             LW_UPHY_DLM_AE_MARGIN_CTRL0, regVal);

    // Program WDATA to set MARGIN_EN bit to 0
    regVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_8, _CFG_WDATA, rData, regVal);
    regVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_8, _CFG_WDATA_MARGIN_EN, 0x0,
                             regVal);
    REG_WR32(FECS, LW_XP_PEX_PAD_CTL_8, regVal);

    //
    // Loop through the laneSelectMask and reset lastMarginType and
    // lastMarginOffset for the Stop margin requested lane. Also clear the 
    // error count for the corresponding lanes
    //
    pLastStepMarginType   = MARGINING_DATA_GET()->lastStepMarginType;
    pLastStepMarginOffset = MARGINING_DATA_GET()->lastStepMarginOffset;

    for (laneNum = 0U; laneNum < BIF_MAX_PCIE_LANES; laneNum++)
    {
        if ((BIT(laneNum) & laneSelectMask) != 0U)
        {
            pLastStepMarginType[laneNum]   = BIF_ILWALID_STEP_MARGIN_TYPE;
            pLastStepMarginOffset[laneNum] = BIF_ILWALID_STEP_MARGIN_OFFSET;

            //
            // For some cases(like link entering recovery), we don't want to
            // reset the error count.
            //
            if (bResetErrorCount)
            {
                // 
                // Resetting of error count from stop step margining is required
                // because by default HW is supposed to do this but
                // for GA100 it has regressed and now SW has to explicitly do it
                //
                bifClearErrorLog_HAL(&Bif, laneNum);
            }

            // Disable error counting from XVE
            bifEnableMarginErrorCounting_HAL(&Bif, laneNum, LW_FALSE);
        }
    }
}

/*!
 * @brief Get error count for the specified lane(using laneSelectMask).
 *
 * @param[in] laneSelectMask    Bit mask for selected lanes. 'i'th bit would be
 *                              set if lane #i needs to be selected
 * @param[out] pErrorCount      Margining error count for the requested lane
 *
 * @return    FLCN_OK if error count is returned correctly
 */
FLCN_STATUS
bifGetMarginingErrorCount_GA100
(
    LwU32 laneSelectMask,
    LwU8  *pErrorCount
)
{
    LwU8  laneNum;
    LwU8  offset;
    LwU32 regVal;
    LwU32 baseReg;
    FLCN_STATUS status = FLCN_OK;

    status = s_bifGetLaneNum(laneSelectMask, &laneNum);
    if (status != FLCN_OK)
    {
        goto bifGetMarginingErrorCount_GA100_exit;
    }

    status = s_bifGetMarginErrorCountBase_GA100(laneNum, &baseReg);
    if (status != FLCN_OK)
    {
        goto bifGetMarginingErrorCount_GA100_exit;
    }

    //
    // Get the value of entire margin error count register - this will include
    // error count for all four lanes in that group. Group 'i' will have error
    // count for lanes from lane #(4*i) to lane #(4*i + 3)
    //
    regVal = BAR0_CFG_REG_RD32(baseReg);

    // Now get the error count for the requested lane
    offset = laneNum % 4U;
    switch (offset)
    {
        case 0U:
        {
            *pErrorCount = DRF_VAL(_XVE, _MARGINING_ERROR_COUNT, _OFFSET_0, regVal);
            break;
        }
        case 1U:
        {
            *pErrorCount = DRF_VAL(_XVE, _MARGINING_ERROR_COUNT, _OFFSET_1, regVal);
            break;
        }
        case 2U:
        {
            *pErrorCount = DRF_VAL(_XVE, _MARGINING_ERROR_COUNT, _OFFSET_2, regVal);
            break;
        }
        case 3U:
        {
            *pErrorCount = DRF_VAL(_XVE, _MARGINING_ERROR_COUNT, _OFFSET_3, regVal);
            break;
        }
        // No need for default case. All cases are taken care already
    }

bifGetMarginingErrorCount_GA100_exit:
    return status;
}

/*!
 * @brief Enable Margin error counting in XVE for a single lane.
 *
 * @param[in] laneSelectMask          Bit mask for selected lanes. 'i'th bit
 *                                    would be set if margining error count
 *                                    is to enabled/disabled for 'i'th lane
 * @param[in] bEnableErrorCounting    If this is 1, error count is enabled.
 *                                    If this is 0, error count is disabled.
 *
 * @return   FLCN_OK if Margin error count is enabled correctly
 */
FLCN_STATUS
bifEnableMarginErrorCounting_GA100
(
    LwU8   laneNum,
    LwBool bEnableErrorCounting
)
{
    LwU8  offset;
    LwU32 regVal;
    LwU32 baseReg;
    FLCN_STATUS status = FLCN_OK;

    status = s_bifGetMarginErrorCountBase_GA100(laneNum, &baseReg);
    if (status != FLCN_OK)
    {
        goto bifEnableMarginErrorCounting_GA100_exit;
    }

    regVal = BAR0_CFG_REG_RD32(baseReg);

    offset = laneNum % 4U;
    switch (offset)
    {
        case 0U:
        {
            if (bEnableErrorCounting)
            {
                regVal = FLD_SET_DRF(_XVE, _MARGINING_ERROR_COUNT_START_COUNTING,
                                     _OFFSET_0, _ENABLE, regVal);
            }
            else
            {
                regVal = FLD_SET_DRF(_XVE, _MARGINING_ERROR_COUNT_START_COUNTING,
                                     _OFFSET_0, _DISABLE, regVal);
            }
            break;
        }
        case 1U:
        {
            if (bEnableErrorCounting)
            {
                regVal = FLD_SET_DRF(_XVE, _MARGINING_ERROR_COUNT_START_COUNTING,
                                     _OFFSET_1, _ENABLE, regVal);
            }
            else
            {
                regVal = FLD_SET_DRF(_XVE, _MARGINING_ERROR_COUNT_START_COUNTING,
                                     _OFFSET_1, _DISABLE, regVal);
            }
            break;
        }
        case 2U:
        {
            if (bEnableErrorCounting)
            {
                regVal = FLD_SET_DRF(_XVE, _MARGINING_ERROR_COUNT_START_COUNTING,
                                     _OFFSET_2, _ENABLE, regVal);
            }
            else
            {
                regVal = FLD_SET_DRF(_XVE, _MARGINING_ERROR_COUNT_START_COUNTING,
                                     _OFFSET_2, _DISABLE, regVal);
            }
            break;
        }
        case 3U:
        {
            if (bEnableErrorCounting)
            {
                regVal = FLD_SET_DRF(_XVE, _MARGINING_ERROR_COUNT_START_COUNTING,
                                     _OFFSET_3, _ENABLE, regVal);
            }
            else
            {
                regVal = FLD_SET_DRF(_XVE, _MARGINING_ERROR_COUNT_START_COUNTING,
                                     _OFFSET_3, _DISABLE, regVal);
            }
            break;
        }
        // No need to have default case here. All possible cases covered above.
    }

    // Actually enable margin count for the requsted lane
    BAR0_CFG_REG_WR32(baseReg, regVal);

bifEnableMarginErrorCounting_GA100_exit:
    return status;
}

/*!
 * @brief Resets error count for the requested lane
 *
 * @param[in] laneNum    Requested lane number
 *
 * @return    FLCN_OK if error count cleared successfully
 */
FLCN_STATUS
bifClearErrorLog_GA100
(
    LwU8 laneNum
)
{
    LwU8  offset;
    LwU32 regVal;
    LwU32 baseReg;
    FLCN_STATUS status = FLCN_OK;

    if (laneNum >= BIF_MAX_PCIE_LANES)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        REG_WR32(CSB,
                 LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_BIF_LANE_MARGINING),
                 status);
        PMU_BREAKPOINT();
        goto bifClearErrorLog_GA100_exit;
    }

    status = s_bifGetMarginErrorCountBase_GA100(laneNum, &baseReg);
    if (status != FLCN_OK)
    {
        goto bifClearErrorLog_GA100_exit;
    }

    regVal = BAR0_CFG_REG_RD32(baseReg);

    offset = laneNum % 4U;
    switch (offset)
    {
        case 0U:
        {
            regVal = FLD_SET_DRF_NUM(_XVE, _MARGINING_ERROR_COUNT_RESET,
                                     _OFFSET_0, 0x1U, regVal);
            break;
        }
        case 1U:
        {
            regVal = FLD_SET_DRF_NUM(_XVE, _MARGINING_ERROR_COUNT_RESET,
                                     _OFFSET_1, 0x1U, regVal);
            break;
        }
        case 2U:
        {
            regVal = FLD_SET_DRF_NUM(_XVE, _MARGINING_ERROR_COUNT_RESET,
                                     _OFFSET_2, 0x1U, regVal);
            break;
        }
        case 3U:
        {
            regVal = FLD_SET_DRF_NUM(_XVE, _MARGINING_ERROR_COUNT_RESET,
                                     _OFFSET_3, 0x1U, regVal);
            break;
        }
        // No need to handle default since all possible cases are taken care.
    }

    // Now actually reset the error count
    BAR0_CFG_REG_WR32(baseReg, regVal);

    bifClearErrorLog_GA100_exit:
        return status;
}

/*!
 * @brief Handles error count exceeded interrupt during Margining
 *
 * @param[in] regVal    Current value of LW_XVE_MARGINING_INTERRUPT register
 *
 * return FLCN_OK    If the interrupt is handled successfully
 */
FLCN_STATUS
bifHandleErrorCountExcdIntr_GA100
(
    LwU32 regVal
)
{
    LwU8  laneNum;
    LwU32 tempRegVal;
    LwU32 laneResetMask;
    FLCN_STATUS status = FLCN_OK;

    tempRegVal = BAR0_CFG_REG_RD32(LW_XVE_MARGINING_INTERRUPT_1);

    //
    // Check for which particular lane the interrrupt is pending
    // We don't need to check if interrupts are pending for more than one lane.
    // If interrupts are pending for more than one lanes, PMU will be getting
    // subsequent interrupts for unhandled lanes
    //
    laneResetMask = 0x1U;
    for (laneNum = 0U; laneNum < BIF_MAX_PCIE_LANES; laneNum++)
    {
        if (tempRegVal & laneResetMask)
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
        goto bifHandleErrorCountExcdIntr_GA100_exit;
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
        goto bifHandleErrorCountExcdIntr_GA100_exit;
    }

    //
    // For task LOWLATENCY, following statement is as good as atomic since
    // this task runs at priority of 4 and task BIF won't be able to preempt it
    //
    MARGINING_DATA_GET()->marginingScheduledMask &= ~(laneResetMask);

    //
    // SW does not maintain error count thresholds. In fact, for Turing it does
    // but not in a very correct way. Need to remove any error count threshold
    // state from SW for Turing
    //

    //
    // Ack the interrupt by setting the corresponding bit to RESET
    // Let's not write RESET(0x1) for other lanes. Writing 0 should be okay
    // since that won't reset the interrupt status.
    //
    BAR0_CFG_REG_WR32(LW_XVE_MARGINING_INTERRUPT_1, laneResetMask);

    //
    // Ack the interrupt.
    // regVal contains value of LW_XVE_MARGINING_INTERRUPT register
    //
    regVal = FLD_SET_DRF(_XVE, _MARGINING_INTERRUPT, 
                         _ERROR_COUNT_LIMIT_EXCD, _RESET, regVal);
    BAR0_CFG_REG_WR32(LW_XVE_MARGINING_INTERRUPT, regVal);

bifHandleErrorCountExcdIntr_GA100_exit:
    return status;
}

/*!
 * @brief Checks if Margining is enabled from the fuse and from the SW
 *
 * @return    LW_TRUE  if Lane Margining is enabled
 *            LW_FALSE otherwise
 */
LwBool
bifMarginingEnabled_GA100(void)
{
    LwU32 fuseVal;
    //
    // Please do not change the default value of this. Rest of the code
    // assumes default value to be false
    //
    LwBool bMarginingEnabled = LW_FALSE;

    fuseVal = fuseRead(LW_FUSE_SPARE_BIT_ENDIS_0);

    //
    // TODO - NM: Add back check of Bif.bMarginingEnabledFromSW. Removin this
    // as of now since there seems to be too much for dmem.
    //
    if (FLD_TEST_DRF(_FUSE, _SPARE_BIT_ENDIS_0, _DATA, _NO, fuseVal))
    {
        bMarginingEnabled = LW_TRUE;
    }

    return bMarginingEnabled;
}

/*!
 * @brief Get Margin-Error-Count base register for the given lane
 *
 * @param[in] laneNum      Lane number for which Margin-Error-Count base
 *                         register is to be found
 * @param[out] pBaseReg    Points to Margin-Error-Count base register
 *
 * @return    FLCN_OK   If the valid base register is found
 */
static FLCN_STATUS
s_bifGetMarginErrorCountBase_GA100
(
    LwU8  laneNum,
    LwU32 *pBaseReg
)
{
    FLCN_STATUS status = FLCN_OK;

    if (laneNum >= BIF_MAX_PCIE_LANES)
    {
        status = FLCN_ERR_OUT_OF_RANGE;
        REG_WR32(CSB,
                 LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_BIF_LANE_MARGINING),
                 status);
        PMU_BREAKPOINT();
        goto s_bifGetMarginErrorCountBase_GA100_exit;
    }

    switch (laneNum / 4U)
    {
        case 0U:
        {
            *pBaseReg = LW_XVE_MARGINING_ERROR_COUNT_0;
            break;
        }
        case 1U:
        {
            *pBaseReg = LW_XVE_MARGINING_ERROR_COUNT_1;
            break;
        }
        case 2U:
        {
            *pBaseReg = LW_XVE_MARGINING_ERROR_COUNT_2;
            break;
        }
        case 3U:
        {
            *pBaseReg = LW_XVE_MARGINING_ERROR_COUNT_3;
            break;
        }
        default:
        {
            status = FLCN_ERR_OUT_OF_RANGE;
            REG_WR32(CSB,
                     LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_BIF_LANE_MARGINING),
                     status);
            PMU_BREAKPOINT();
            goto s_bifGetMarginErrorCountBase_GA100_exit;
        }
    }

s_bifGetMarginErrorCountBase_GA100_exit:
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

    // If no lane is specified, might indicate something amiss.
    if (laneSelectMask == 0U)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
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
 * @brief Update margining status register during different stages of
 * Step Margining
 *
 * @param[in] laneNum       Lane Number for which Margining status needs
 *                          to be updated
 * @param[in] execStatus    Current exelwtion status of Step Margining
 *
 * @return FLCN_OK    If the status register is updated correctly
 */
FLCN_STATUS
bifUpdateStepMarginingExecStatus_GA100
(
    LwU8 laneNum,
    LwU8 execStatus
)
{
    LwU32 regVal;
    FLCN_STATUS status = FLCN_OK;

    regVal = BAR0_CFG_REG_RD32(LW_XVE_MARGINING_SHADOW_LANE_STATUS(laneNum));

    switch (execStatus)
    {
        case PCIE_LANE_MARGIN_TOO_MANY_ERRORS:
        {
            regVal = FLD_SET_DRF(_XVE, _MARGINING_SHADOW_LANE_STATUS,
                                 _MARGIN_PAYLOAD_EXEC_STATUS,
                                 _TOO_MANY_ERRORS, regVal);
            break;
        }
        case PCIE_LANE_MARGIN_SET_UP_IN_PROGRESS:
        {
            regVal = FLD_SET_DRF(_XVE, _MARGINING_SHADOW_LANE_STATUS,
                                 _MARGIN_PAYLOAD_EXEC_STATUS,
                                 _SET_UP_IN_PROGRESS, regVal);
            break;
        }
        case PCIE_LANE_MARGIN_MARGINING_IN_PROGRESS:
        {
            regVal = FLD_SET_DRF(_XVE, _MARGINING_SHADOW_LANE_STATUS,
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
            goto bifUpdateStepMarginingExecStatus_GA100_exit;
        }
    }

    BAR0_CFG_REG_WR32(LW_XVE_MARGINING_SHADOW_LANE_STATUS(laneNum), regVal);

bifUpdateStepMarginingExecStatus_GA100_exit:
    return status;
}

/*!
 * @brief Update Margining ready bits in the Margining status register to
 * indicate if SW is ready to do Margining
 *
 * @param[in] bEnableReady    If true, SW is ready to do Margining otherwise
 *                            it is not ready.
 *
 * @return    FLCN_OK    If Margining ready bits are updated correctly
 */
FLCN_STATUS
bifUpdateMarginingRdyStatus_GA100
(
    LwBool bEnableReady
)
{
    LwU32 tempRegVal;
    FLCN_STATUS status = FLCN_OK;

    tempRegVal = BAR0_CFG_REG_RD32(LW_XVE_MARGINING_SHADOW_PORT_STATUS);

    if (bEnableReady)
    {
        tempRegVal = FLD_SET_DRF_NUM(_XVE, _MARGINING_SHADOW_PORT_STATUS,
                                     _MARGINING_RDY, 0x1U, tempRegVal);
        tempRegVal = FLD_SET_DRF_NUM(_XVE, _MARGINING_SHADOW_PORT_STATUS,
                                     _MARGINING_SW_RDY, 0x1U, tempRegVal);
    }
    else
    {
        tempRegVal = FLD_SET_DRF_NUM(_XVE, _MARGINING_SHADOW_PORT_STATUS,
                                     _MARGINING_RDY, 0x0, tempRegVal);
        tempRegVal = FLD_SET_DRF_NUM(_XVE, _MARGINING_SHADOW_PORT_STATUS,
                                     _MARGINING_SW_RDY, 0x0, tempRegVal);
    }
    BAR0_CFG_REG_WR32(LW_XVE_MARGINING_SHADOW_PORT_STATUS, tempRegVal);

    return status;
}
