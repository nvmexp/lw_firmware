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
 * @file   lanemargining_tu10x.c
 * @brief  Contains Lane margining routines for Tu10x and later chips
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmu_objpmu.h"

#include "dev_lw_xp.h"
#include "dev_lw_xve.h"
#include "dev_lw_xve_addendum.h"
#include "dev_lw_xp_addendum.h"
#include "dev_pwr_csb.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "config/g_bif_private.h"
#include "pmu_objbif.h"
#include "pmu/pmumailboxid.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
//
// NM-TODO: Need to check if and how these could be transferred to HW manuals?
// If not, following defines need to be there in the addendum files for each
// chip. Already part of addendum files for Turing and GA10X
//
#if !(PMU_PROFILE_TU10X)
#define UPHY_AE_EOM_MARGIN_XY_CTRL     0xBC
#define UPHY_AE_EOM_MARGIN_STATUS      0xC0
#define UPHY_AE_EOM_CTRL1              0xAF
#define UPHY_AE_EOM_CTRL1_MARGIN_START 0x13
#define UPHY_AE_EOM_CTRL1_MARGIN_STOP  0x03

#define LW_XVE_MARGINING_LANE_CTRL_MARGIN_PAYLOAD_OFFSET                                    14:8
#define LW_XVE_MARGINING_SHADOW_LANE_STATUS_MARGIN_PAYLOAD_ERROR_COUNT                      13:8
#define LW_XVE_MARGINING_SHADOW_LANE_STATUS_MARGIN_PAYLOAD_EXEC_STATUS                     15:14
#define LW_XVE_MARGINING_SHADOW_LANE_STATUS_MARGIN_PAYLOAD_EXEC_STATUS_TOO_MANY_ERRORS       0x0
#define LW_XVE_MARGINING_SHADOW_LANE_STATUS_MARGIN_PAYLOAD_EXEC_STATUS_SET_UP_IN_PROGRESS    0x1
#define LW_XVE_MARGINING_SHADOW_LANE_STATUS_MARGIN_PAYLOAD_EXEC_STATUS_MARGINING_IN_PROGRESS 0x2
#endif

/* ------------------------- Prototypes ------------------------------------- */
static FLCN_STATUS s_bifHandleMarginCommand_TU10X(LwU8 laneNum, LwU32 laneResetMask)
        GCC_ATTRIB_SECTION("imem_libBifMargining", "s_bifHandleMarginCommand_TU10X");

static FLCN_STATUS s_bifHandleStepMarginCommand_TU10X(LwU8 laneNum, LwU8 rcvrNum, LwU8 marginType, LwU8 usageModel)
        GCC_ATTRIB_SECTION("imem_libBifMargining", "s_bifHandleStepMarginCommand_TU10X");

static FLCN_STATUS s_bifScheduleNextStepMarginCmd_TU10X(LwU32 marginRequestMask, LwU8 laneNum, LwBool *bQueuedStepMarginCmd)
    GCC_ATTRIB_SECTION("imem_libBifMargining", "s_bifScheduleNextStepMarginCmd_TU10X");

static FLCN_STATUS s_bifMarginHandleLinkRecovery(LwU8 laneNum)
    GCC_ATTRIB_SECTION("imem_libBifMargining", "s_bifMarginHandleLinkRecovery");

static FLCN_STATUS s_bifMarginHandleLinkResumeRecovery(void)
    GCC_ATTRIB_SECTION("imem_libBifMargining", "s_bifMarginHandleLinkResumeRecovery");

/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief This function enables lane margining interrupts to PMU only when
 * Gen4 is supported and enabled.
 */
void
bifInitMarginingIntr_TU10X(void)
{
    LwU32 regVal;
    regVal = REG_RD32(CSB, LW_CPWR_PMU_GPIO_2_INTR_HIGH_EN);
    if (FLD_TEST_DRF(_RM_PMU, _BIF_CAPS, _GEN4PCIE, _SUPPORTED, Bif.bifCaps))
    {
        regVal = FLD_SET_DRF(_CPWR_PMU, _GPIO_2_INTR_HIGH_EN,
                             _XVE_MARGINING_INTR, _ENABLED, regVal);
    }
    else
    {
        regVal = FLD_SET_DRF(_CPWR_PMU, _GPIO_2_INTR_HIGH_EN,
                             _XVE_MARGINING_INTR, _DISABLED, regVal);
    }
    REG_WR32(CSB, LW_CPWR_PMU_GPIO_2_INTR_HIGH_EN, regVal);
}

/*!
 * @brief This function which is called by lowlatency task handles the
 * margining interrupts. For step margining interrupts, this updates margining
 * status register and adds command in task BIF's queue if required
 *
 * @return FLCN_OK if interrupt is handled correctly
 */
FLCN_STATUS
bifServiceMarginingIntr_TU10X(void)
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
    // Lane specific interrupt is set
    else
    {
        status = bifHandleMarginCommandIntr_HAL(&Bif, regVal);
    }

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
 * @brief Deferred routine scheduled to execute step margining commands
 * This routine actually exelwtes margining for all lanes until margining for
 * all lanes is not stopped
 *
 * @param[in] laneNum    Lane number for which step margin command is triggered
 *
 * @return    FLCN_OK    If this margin request is handled correctly and the 
                         command to handle next step margin request is added
                         in task BIF's queue
 */
FLCN_STATUS
bifDoLaneMargining_TU10X
(
    LwU8 laneNum
)
{
    LwU32  regVal;
    LwU32  tempRegVal;
    LwU32  ltssmState;
    LwU32  tempMarginMask;
    LwU8   rcvrNum;
    LwU8   marginType;
    LwU8   errorCount;
    LwU8   marginOffset;
    LwU8   usageModel;
    LwU8   *pLastStepMarginOffset;
    LwU8   *pLastStepMarginType;
    LwBool bMarginingInProgress;
    LwBool bHwEnableErrorThreshold;
    LwBool bQueuedStepMarginCmd = LW_FALSE;
    FLCN_STATUS status = FLCN_OK;


    bHwEnableErrorThreshold = PMUCFG_FEATURE_ENABLED(
                                  PMU_BIF_LANE_MARGINING_HW_ERROR_THRESHOLD);

    pLastStepMarginType   = MARGINING_DATA_GET()->lastStepMarginType;
    pLastStepMarginOffset = MARGINING_DATA_GET()->lastStepMarginOffset;

    // Read margin ctrl register and decide what margining command is issued
    regVal        = BAR0_CFG_REG_RD32(LW_XVE_MARGINING_LANE(laneNum));

    // Store the margining command seen for later reference
    tempRegVal    = regVal;

    rcvrNum       = DRF_VAL(_XVE, _MARGINING_LANE_CTRL, _RCVR_NUM, regVal);
    marginType    = DRF_VAL(_XVE, _MARGINING_LANE_CTRL, _MARGIN_TYPE, regVal);
    usageModel    = DRF_VAL(_XVE, _MARGINING_LANE_CTRL, _USAGE_MODEL, regVal);

    switch (marginType)
    {
        case PCIE_LANE_MARGIN_TYPE_3:
        case PCIE_LANE_MARGIN_TYPE_4:
        {
            //
            // For Turing, MIndLeftRightTiming is 0 and therefore 
            // If MIndLeftRightTiming for the targeted Receiver is Clear
            // Margin Payload [6]: Reserved.
            // Margin Payload [5:0] indicates the number of steps beyond
            // the normal setting.
            //
            marginOffset = DRF_VAL(_XVE, _MARGINING_LANE_CTRL,
                                   _MARGIN_PAYLOAD_OFFSET, regVal);

            //
            // For rcvrNum, marginType and usageModel, just need to reflect
            // back what is there in ctrl register's payload
            // 
            regVal = BAR0_CFG_REG_RD32(
                        LW_XVE_MARGINING_SHADOW_LANE_STATUS(laneNum));
            regVal = FLD_SET_DRF_NUM(_XVE, _MARGINING_SHADOW_LANE_STATUS,
                                     _RCVR_NUM, rcvrNum, regVal);
            regVal = FLD_SET_DRF_NUM(_XVE, _MARGINING_SHADOW_LANE_STATUS,
                                     _MARGIN_TYPE, marginType, regVal);
            regVal = FLD_SET_DRF_NUM(_XVE, _MARGINING_SHADOW_LANE_STATUS,
                                     _USAGE_MODEL, usageModel, regVal);

            //
            // Handling of link entering recovery is not required for Turing
            // since it uses independent sampler
            //
            if (PMUCFG_FEATURE_ENABLED(
                    PMU_BIF_LANE_MARGINING_LINK_RECOVERY_WAR))
            {
                ltssmState = REG_RD32(FECS, LW_XP_PL_LTSSM_STATE);
                if (FLD_TEST_DRF(_XP, _PL_LTSSM_STATE, _IS_L0,
                                 _FALSE, ltssmState))
                {
                    status = s_bifMarginHandleLinkRecovery(laneNum);
                    if (status != FLCN_OK)
                    {
                        goto bifDoLaneMargining_TU10X_exit;
                    }
                }
            }

            //
            // If this is not the same margining command as that of last
            // margining command then we need to initiate a new margining
            // request in UPHY
            //
            if ((marginOffset != pLastStepMarginOffset[laneNum])||
                (marginType   != pLastStepMarginType[laneNum]))
            {
                status = bifStartStepMargining_HAL(&Bif, marginType,
                                                   marginOffset, BIT(laneNum));
                if (status != FLCN_OK)
                {
                    goto bifDoLaneMargining_TU10X_exit;
                }
            }

            status = bifGetMarginingErrorCount_HAL(&Bif, BIT(laneNum),
                                                   &errorCount);
            if (status != FLCN_OK)
            {
                goto bifDoLaneMargining_TU10X_exit;
            }

            //
            // Assumption here is that regVal still contains value of Lane
            // Status register. Check for error threshold only when it is not
            // being handled by HW. In case it is handled by HW, it generates
            // error count limit exceeded interrupt.
            //
            if ((!bHwEnableErrorThreshold) &&
                (errorCount > MARGINING_DATA_GET()->marginingErrorCountLimit))
            {
                regVal = FLD_SET_DRF(_XVE, _MARGINING_SHADOW_LANE_STATUS,
                                     _MARGIN_PAYLOAD_EXEC_STATUS,
                                     _TOO_MANY_ERRORS, regVal);
            }
            else
            {
                regVal = FLD_SET_DRF(_XVE, _MARGINING_SHADOW_LANE_STATUS,
                                     _MARGIN_PAYLOAD_EXEC_STATUS,
                                     _MARGINING_IN_PROGRESS, regVal);
            }
            regVal = FLD_SET_DRF_NUM(_XVE, _MARGINING_SHADOW_LANE_STATUS,
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
                    ((BIT(laneNum) & MARGINING_DATA_GET()->marginingScheduledMask)
                     != 0U);
                if (bMarginingInProgress &&
                    (tempRegVal == BAR0_CFG_REG_RD32(LW_XVE_MARGINING_LANE(laneNum))))
                {
                    BAR0_CFG_REG_WR32(LW_XVE_MARGINING_SHADOW_LANE_STATUS(laneNum),
                                      regVal);
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
        goto bifDoLaneMargining_TU10X_exit;
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
        status = s_bifScheduleNextStepMarginCmd_TU10X(tempMarginMask, laneNum,
                                                      &bQueuedStepMarginCmd);
        if (status != FLCN_OK)
        {
            goto bifDoLaneMargining_TU10X_exit;
        }
    }

bifDoLaneMargining_TU10X_exit:
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
            status = s_bifScheduleNextStepMarginCmd_TU10X(tempMarginMask,
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

/* ------------------------- Private Functions ------------------------------ */

/* @brief Schedule command in task BIF's queue for handling Step margin
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
s_bifScheduleNextStepMarginCmd_TU10X
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
        goto s_bifScheduleNextStepMarginCmd_TU10X_exit;
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

s_bifScheduleNextStepMarginCmd_TU10X_exit:
    return status;
}

/*!
 * @brief Get error count for the specified lanes(using laneSelectMask).
 *
 * @param[in]  laneSelectMask    Bit mask for selected lanes. 'i'th bit would
 *                               be set if lane #i needs to be selected
 * @param[out] pErrorCount       Margining error count for the requested lane
 *
 * @return    FLCN_OK if error count is returned correctly
 */
FLCN_STATUS
bifGetMarginingErrorCount_TU10X
(
    LwU32 laneSelectMask,
    LwU8  *pErrorCount
)
{
    LwU32 tempRegVal;
    LwU16 cfgReadData;
    FLCN_STATUS status = FLCN_OK;

    if (laneSelectMask == 0U)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        REG_WR32(CSB,
                 LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_BIF_LANE_MARGINING),
                 status);
        PMU_BREAKPOINT();
        goto bifGetMarginingErrorCount_TU10X_exit;
    }
    //
    // First do lane select and then 1 step READ in PHY control register to get
    // margin error count. Read margin status register to get the error count.
    // To trigger state machine to read margin status register, first write to
    // CTL_8 register with margin status register's address(0xC0) and CFG_RDS
    // bit set
    //
    tempRegVal = REG_RD32(FECS, LW_XP_PEX_PAD_CTL_SEL);
    tempRegVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_SEL, _LANE_SELECT,
                                 laneSelectMask, tempRegVal);
    REG_WR32(FECS, LW_XP_PEX_PAD_CTL_SEL, tempRegVal);

    tempRegVal = 0U;

    tempRegVal = FLD_SET_DRF(_XP, _PEX_PAD_CTL_8, _CFG_RESET, _INIT,
                             tempRegVal);
    tempRegVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_8, _CFG_RDS, 0x1U,
                                 tempRegVal);
    tempRegVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_8, _CFG_ADDR,
                                 UPHY_AE_EOM_MARGIN_STATUS, tempRegVal);

    REG_WR32(FECS, LW_XP_PEX_PAD_CTL_8, tempRegVal);

    tempRegVal  = REG_RD32(FECS, LW_XP_PEX_PAD_CTL_9);
    cfgReadData = DRF_VAL(_XP, _PEX_PAD_CTL_9, _CFG_RDATA, tempRegVal);

    // Mask bits other than 5:0. Width for margining error count is 6 bits
    *pErrorCount = cfgReadData & PCIE_LANE_MARGIN_MAX_ERROR_COUNT;

bifGetMarginingErrorCount_TU10X_exit:
    return status;
}

/*!
 * @brief Start margining for the specified lanes(using laneSelectMask).
 *
 * @param[in] marginType        Margin type specified in the margining command
 * @param[in] marginOffset      Margin offset specified in the margining command
 * @param[in] laneSelectMask    Lane number for which margining has to be
 *                              started. For 'i'th lane to be selected, bit 'i'
 *                              needs to be set in the laneSelectMask
 */
FLCN_STATUS
bifStartStepMargining_TU10X
(
    LwU8  marginType,
    LwU8  marginOffset,
    LwU32 laneSelectMask
)
{
    LwU32       tempRegVal;
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
        goto bifStartStepMargining_TU10X_exit;
    }

    tempRegVal = REG_RD32(FECS, LW_XP_PEX_PAD_CTL_SEL);
    tempRegVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_SEL, _LANE_SELECT,
                                 laneSelectMask, tempRegVal);
    REG_WR32(FECS, LW_XP_PEX_PAD_CTL_SEL, tempRegVal);

    tempRegVal = 0U;
    tempRegVal = FLD_SET_DRF(_XP, _PEX_PAD_CTL_8, _CFG_RESET, _INIT,
                             tempRegVal);
    tempRegVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_8,
                                 _CFG_WDS,
                                 0x1U, tempRegVal);
    tempRegVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_8,
                                 _CFG_ADDR,
                                 UPHY_AE_EOM_MARGIN_XY_CTRL, tempRegVal);

    // 1 step WRITE in PHY control register for margin offset + dir
    if (marginType == PCIE_LANE_MARGIN_TYPE_3)
    {
        //
        // For x-offset bits 6:0 are used. See pkg-dln-table-spec for details
        // tempRegVal = 0x9bc0000 | marginOffset;
        //
        tempRegVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_8,
                                     _CFG_WDATA_MARGIN_X_OFFSET,
                                     marginOffset, tempRegVal);
    }
    else
    {
        //
        // For y-offset bits 13:8 are used. See pkg-dln-table-spec for details
        // tempRegVal = 0x9bc0000 | ((bits 6:0 of marginOffset) << 8));
        //
        tempRegVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_8,
                                     _CFG_WDATA_MARGIN_Y_OFFSET,
                                     marginOffset, tempRegVal);
    }
    REG_WR32(FECS, LW_XP_PEX_PAD_CTL_8, tempRegVal);

    // 1 step WRITE in PHY control register for margin start
    tempRegVal = 0U;
    tempRegVal = FLD_SET_DRF(_XP, _PEX_PAD_CTL_8, _CFG_RESET, _INIT,
                             tempRegVal);
    tempRegVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_8,
                                 _CFG_WDS,
                                 0x1U, tempRegVal);
    tempRegVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_8,
                                 _CFG_ADDR,
                                 UPHY_AE_EOM_CTRL1, tempRegVal);
    tempRegVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_8,
                                 _CFG_WDATA,
                                 UPHY_AE_EOM_CTRL1_MARGIN_START, tempRegVal);
    REG_WR32(FECS, LW_XP_PEX_PAD_CTL_8, tempRegVal);

    //
    // 1 step READ in PHY to confirm margin started
    // This is not supported on Turing. Needs to be modified for Ampere.
    //

bifStartStepMargining_TU10X_exit:
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
bifClearErrorLog_TU10X
(
    LwU8 laneNum
)
{
    LwU8  lastStepMarginType;
    LwU8  lastStepMarginOffset;
    FLCN_STATUS status = FLCN_OK;

    if (laneNum >= BIF_MAX_PCIE_LANES)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        REG_WR32(CSB,
                 LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_BIF_LANE_MARGINING),
                 status);
        PMU_BREAKPOINT();
        goto bifClearErrorLog_TU10X_exit;
    }

    //
    // It is safe to assume that if corresponding bit of marginingScheduledMask
    // is not set, margining is not in progress.
    // Since for tu10x and ga10x, marginingScheduledMask is updated only from
    // task LL and whenever there is an update to reset the corresponding bit,
    // margining is stopped synchronously
    //
    if ((BIT(laneNum) & MARGINING_DATA_GET()->marginingScheduledMask) == 0U)
    {
        goto bifClearErrorLog_TU10X_exit;
    }

    bifStopStepMargining_HAL(&Bif, BIT(laneNum), LW_TRUE);

    lastStepMarginType = MARGINING_DATA_GET()->lastStepMarginType[laneNum];
    lastStepMarginOffset = MARGINING_DATA_GET()->lastStepMarginOffset[laneNum];

    //
    // We cannot really start margining with invalid offsets. Exit without
    // starting margining. Task BIF will take care of starting margining if not
    // already
    //
    if (lastStepMarginType == BIF_ILWALID_STEP_MARGIN_TYPE ||
        lastStepMarginOffset == BIF_ILWALID_STEP_MARGIN_OFFSET)
    {
        goto bifClearErrorLog_TU10X_exit;
    }

    status = bifStartStepMargining_HAL(&Bif, lastStepMarginType,
                                       lastStepMarginOffset, BIT(laneNum));
    if (status != FLCN_OK)
    {
        goto bifClearErrorLog_TU10X_exit;
    }

bifClearErrorLog_TU10X_exit:
    return status;
}

/*!
 * @brief Handle interrupt for margining command for a specific lane
 *
 * @param[in] regVal    Current value of LW_XVE_MARGINING_INTERRUPT register
 *
 * @return    FLCN_OK   If the interrupt is handled correctly
 */
FLCN_STATUS
bifHandleMarginCommandIntr_TU10X
(
    LwU32 regVal
)
{
    LwU8  laneNum;
    LwU32 laneResetMask;
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
        // regVal contains value of LW_XVE_MARGINING_INTERRUPT register.
        // Check if the bit corresponding to lane #'laneNum' is set
        //
        if (regVal & laneResetMask)
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
        goto bifHandleMarginCommandIntr_TU10X_exit;
    }

    // Ack the interrupt
    regVal |= laneResetMask;
    BAR0_CFG_REG_WR32(LW_XVE_MARGINING_INTERRUPT, regVal);

    // Now handle the actual margining command for the identified lane
    status = s_bifHandleMarginCommand_TU10X(laneNum, laneResetMask);

bifHandleMarginCommandIntr_TU10X_exit:
    return status;
}

/*!
 * @brief Handle Margining Abort interrupt
 *
 * @param[in] regVal    Current value of LW_XVE_MARGINING_INTERRUPT register
 */
void
bifHandleMarginingAbortIntr_TU10X
(
    LwU32 regVal
)
{
    MARGINING_DATA_GET()->marginingScheduledMask = 0U;

    // Stop margining for all lanes
    bifStopStepMargining_HAL(&Bif, LW_U16_MAX, LW_TRUE);

    // Reset error threshold
    MARGINING_DATA_GET()->marginingErrorCountLimit = PCIE_LANE_MARGIN_MAX_ERROR_COUNT;

    // Ack the interrupt
    regVal = FLD_SET_DRF(_XVE, _MARGINING_INTERRUPT, _ABORT, _RESET, regVal);
    BAR0_CFG_REG_WR32(LW_XVE_MARGINING_INTERRUPT, regVal);

    //
    // Ilwalidate SW state used for handling link transitions to recovery. Just
    // setting bMarginingStoppedLinkRecovery to LW_FALSE is sufficient
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_BIF_LANE_MARGINING_LINK_RECOVERY_WAR))
    {
        MARGINING_DATA_GET()->bMarginingStoppedLinkRecovery = LW_FALSE;
    }
}

/*!
 * @brief Handle Margining Ready interrupt
 *
 * @param[in] regVal    Current value of LW_XVE_MARGINING_INTERRUPT register
 */
void
bifHandleMarginingReadyIntr_TU10X
(
    LwU32 regVal
)
{
    LwU32 tempRegVal;
    LwU8  laneNum;
    LwU8  *pLastStepMarginOffset;
    LwU8  *pLastStepMarginType;

    // Handle the interrupt
    tempRegVal = BAR0_CFG_REG_RD32(LW_XVE_MARGINING_SHADOW_PORT_STATUS);

    //
    // Now the system is ready to take any margining commands from Root
    // port. Before declaring ourselves to be Margining ready, we check
    // if Margining is enabled at compile time and if Margining is enabled at
    // runtime(bifMarginingEnabled_HAL)
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_BIF_LANE_MARGINING)
        && bifMarginingEnabled_HAL(&Bif))
    {
        tempRegVal = FLD_SET_DRF_NUM(_XVE, _MARGINING_SHADOW_PORT_STATUS,
                                     _MARGINING_RDY, 0x1U, tempRegVal);
        tempRegVal = FLD_SET_DRF_NUM(_XVE, _MARGINING_SHADOW_PORT_STATUS,
                                     _MARGINING_SW_RDY, 0x1U, tempRegVal);
        //
        // This interrupt indicates start of margining requests.
        // Initialize error threshold and margining scheduled mask
        //
        MARGINING_DATA_GET()->marginingScheduledMask   = 0U;
        MARGINING_DATA_GET()->marginingErrorCountLimit = PCIE_LANE_MARGIN_MAX_ERROR_COUNT;
    }
    else
    {
        tempRegVal = FLD_SET_DRF(_XVE, _MARGINING_SHADOW_PORT_STATUS,
                                 _MARGINING_RDY, _INIT, tempRegVal);
        tempRegVal = FLD_SET_DRF(_XVE, _MARGINING_SHADOW_PORT_STATUS,
                                 _MARGINING_SW_RDY, _INIT, tempRegVal);
    }
    BAR0_CFG_REG_WR32(LW_XVE_MARGINING_SHADOW_PORT_STATUS, tempRegVal);

    //
    // Ack the interrupt
    // regVal contains value of LW_XVE_MARGINING_INTERRUPT at this point
    //
    regVal = FLD_SET_DRF(_XVE, _MARGINING_INTERRUPT, _READY, _RESET, regVal);
    BAR0_CFG_REG_WR32(LW_XVE_MARGINING_INTERRUPT, regVal);

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

    if (PMUCFG_FEATURE_ENABLED(PMU_BIF_LANE_MARGINING_LINK_RECOVERY_WAR))
    {
        if (MARGINING_DATA_GET()->bMarginingStoppedLinkRecovery)
        {
            //
            // This will be set to true when link goes out of L0 during Step
            // Margining
            //
            s_bifMarginHandleLinkResumeRecovery();
        }
    }
}

/*!
 * @brief Stop margining for the specified lanes(using laneSelectMask).
 *
 * @param[in] laneSelectMask    Lane number for which Step Margin needs to be
 *                              stopped. For 'i'th lane to be selected,
 *                              bit 'i' needs to be set in the laneSelectMask
 * @param[in] bResetErrorCount  For Turing, this has no effect. Added for HAL
 *                              compatibility purpose. For Turing, error count
 *                              is reset implicitly as a part of Stop Margining.
 */
void
bifStopStepMargining_TU10X
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
    regVal = REG_RD32(FECS, LW_XP_PEX_PAD_CTL_SEL);
    regVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_SEL,
                             _LANE_SELECT, laneSelectMask, regVal);
    REG_WR32(FECS, LW_XP_PEX_PAD_CTL_SEL, regVal);

    // One step WRITE in PHY control register for Margin stop
    regVal = 0U;
    regVal = FLD_SET_DRF(_XP, _PEX_PAD_CTL_8, _CFG_RESET, _INIT, regVal);
    regVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_8, _CFG_WDS, 0x1U, regVal);
    regVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_8, _CFG_ADDR,
                             UPHY_AE_EOM_CTRL1, regVal);
    regVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_8, _CFG_WDATA,
                             UPHY_AE_EOM_CTRL1_MARGIN_STOP, regVal);
    REG_WR32(FECS, LW_XP_PEX_PAD_CTL_8, regVal);

    //
    // Loop through the laneSelectMask and reset lastMarginType and
    // lastMarginOffset for the Stop margin requested lane
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
bifUpdateMarginingStatus_TU10X
(
    LwU8  laneNum,
    LwU8  marginType,
    LwU8  rcvrNum,
    LwU8  usageModel,
    LwU8  marginPayload
)
{
    LwU32 regVal;
    regVal = BAR0_CFG_REG_RD32(LW_XVE_MARGINING_SHADOW_LANE_STATUS(laneNum));

    regVal = FLD_SET_DRF_NUM(_XVE, _MARGINING_SHADOW_LANE_STATUS,
                             _MARGIN_TYPE, marginType, regVal);
    regVal = FLD_SET_DRF_NUM(_XVE, _MARGINING_SHADOW_LANE_STATUS,
                             _RCVR_NUM, rcvrNum, regVal);
    regVal = FLD_SET_DRF_NUM(_XVE, _MARGINING_SHADOW_LANE_STATUS,
                             _USAGE_MODEL, usageModel, regVal);
    regVal = FLD_SET_DRF_NUM(_XVE, _MARGINING_SHADOW_LANE_STATUS,
                             _MARGIN_PAYLOAD, marginPayload, regVal);

    BAR0_CFG_REG_WR32(LW_XVE_MARGINING_SHADOW_LANE_STATUS(laneNum), regVal);
}

/*!
 * @brief Handle margining command for a specific lane
 *
 * @param[in] laneNum          Lane number for which this margin command needs
 *                             to be handled
 * @param[in] laneResetMask    Reset mask used to clear the pending interrupt
 *
 * @return    FLCN_OK    If margining command is handled correctly
 */
static FLCN_STATUS
s_bifHandleMarginCommand_TU10X
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
    regVal = BAR0_CFG_REG_RD32(LW_XVE_MARGINING_LANE(laneNum));

    rcvrNum       = DRF_VAL(_XVE, _MARGINING_LANE_CTRL, _RCVR_NUM, regVal);
    marginType    = DRF_VAL(_XVE, _MARGINING_LANE_CTRL, _MARGIN_TYPE,
                            regVal);
    usageModel    = DRF_VAL(_XVE, _MARGINING_LANE_CTRL, _USAGE_MODEL,
                            regVal);
    marginPayload = DRF_VAL(_XVE, _MARGINING_LANE_CTRL, _MARGIN_PAYLOAD,
                            regVal);
    switch (marginType)
    {
        case PCIE_LANE_MARGIN_TYPE_1:
        {
            //
            // We no longer need to support report margining commands. XVE
            // would be taking care of it without needing support from SW.
            //
            break;
        }
        case PCIE_LANE_MARGIN_TYPE_2:
        {
            // Check payload to identify the margining command
            switch (marginPayload)
            {
                case PCIE_LANE_MARGIN_PAYLOAD_GO_TO_NORMAL_SETTINGS:
                {
                    bifStopStepMargining_HAL(&Bif, BIT(laneNum), LW_TRUE);
                    bifUpdateMarginingStatus_HAL(&Bif, laneNum, marginType,
                        rcvrNum, usageModel,
                        PCIE_LANE_MARGIN_PAYLOAD_GO_TO_NORMAL_SETTINGS);

                    MARGINING_DATA_GET()->marginingScheduledMask &= ~(laneResetMask);

                    // Reset error threshold
                    MARGINING_DATA_GET()->marginingErrorCountLimit =
                        PCIE_LANE_MARGIN_MAX_ERROR_COUNT;
                    break;
                }
                case PCIE_LANE_MARGIN_PAYLOAD_CLEAR_ERROR_LOG:
                {
                    bifStopStepMargining_HAL(&Bif, BIT(laneNum), LW_TRUE);

                    bifClearErrorLog_HAL(&Bif, laneNum);
                    bifUpdateMarginingStatus_HAL(&Bif, laneNum, marginType,
                         rcvrNum, usageModel,
                         PCIE_LANE_MARGIN_PAYLOAD_CLEAR_ERROR_LOG);
                    break;
                }
            } // switch (marginPayload)

            //
            // For Set Error Count Limit, MarginPayload[7:6] => 11b,
            // MarginPayload[5:0] => Error Count Limit
            //
            if ((marginPayload &
                 PCIE_LANE_MARGIN_PAYLOAD_SET_ERROR_COUNT_CMD_MASK) ==
                 PCIE_LANE_MARGIN_PAYLOAD_SET_ERROR_COUNT_LIMIT)
            {
                MARGINING_DATA_GET()->marginingErrorCountLimit =
                    marginPayload &
                    PCIE_LANE_MARGIN_PAYLOAD_SET_ERROR_COUNT_VALUE_MASK;
                bifUpdateMarginingStatus_HAL(&Bif, laneNum, marginType, rcvrNum,
                                                usageModel, marginPayload);
            }
            break;
        }
        // Step time/voltage margin command
        case PCIE_LANE_MARGIN_TYPE_3:
        case PCIE_LANE_MARGIN_TYPE_4:
        {
            status = s_bifHandleStepMarginCommand_TU10X(laneNum, rcvrNum,
                         marginType, usageModel);
            break;
        }
        // No command
        case PCIE_LANE_MARGIN_TYPE_7:
        {
            //
            // As per spec, for type 7, receiver number must always be 0
            // and payload must be for the 'No Command'
            //
            if ((rcvrNum != 0U) ||
                (marginPayload != PCIE_LANE_MARGIN_PAYLOAD_NO_COMMAND))
            {
                status = FLCN_ERR_ILWALID_ARGUMENT;
                PMU_BREAKPOINT();
                goto s_bifHandleMarginCommand_TU10X_exit;
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

s_bifHandleMarginCommand_TU10X_exit:
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
s_bifHandleStepMarginCommand_TU10X
(
    LwU8 laneNum,
    LwU8 rcvrNum,
    LwU8 marginType,
    LwU8 usageModel
)
{
    LwU32 regVal;
    FLCN_STATUS status = FLCN_OK;

    //
    // Set 'setup for margin in progress' and queue the bif task
    // for the remaining handling of step margin command
    //
    regVal = BAR0_CFG_REG_RD32(LW_XVE_MARGINING_SHADOW_LANE_STATUS(laneNum));

    regVal = FLD_SET_DRF_NUM(_XVE, _MARGINING_SHADOW_LANE_STATUS,
                             _RCVR_NUM, rcvrNum, regVal);
    regVal = FLD_SET_DRF_NUM(_XVE, _MARGINING_SHADOW_LANE_STATUS,
                             _USAGE_MODEL, usageModel, regVal);
    regVal = FLD_SET_DRF_NUM(_XVE, _MARGINING_SHADOW_LANE_STATUS,
                             _MARGIN_TYPE, marginType, regVal);
    regVal = FLD_SET_DRF(_XVE, _MARGINING_SHADOW_LANE_STATUS,
                         _MARGIN_PAYLOAD_EXEC_STATUS,
                         _SET_UP_IN_PROGRESS, regVal);

    BAR0_CFG_REG_WR32(LW_XVE_MARGINING_SHADOW_LANE_STATUS(laneNum), regVal);

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
            MARGINING_DATA_GET()->marginingScheduledMask|= BIT(laneNum);
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
                MARGINING_DATA_GET()->marginingScheduledMask|= BIT(laneNum);
                MARGINING_DATA_GET()->bStepMarginCmdQueuedOrInProgress = LW_TRUE;
            }
            else
            {
                PMU_BREAKPOINT();
                goto s_bifHandleStepMarginCommand_TU10X_exit;
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
        goto s_bifHandleStepMarginCommand_TU10X_exit;
    }

s_bifHandleStepMarginCommand_TU10X_exit:
    return status;
}

/*!
 * @brief Handle the case when the link goes to recovery while the Step
 * Margining is in progress
 *
 * @param[in] laneNum    Lane Number for which Margining status needs to be
 *                       updated
 *
 * @return FLCN_OK    If link recovery case is handled successfully
 */
FLCN_STATUS
s_bifMarginHandleLinkRecovery
(
    LwU8 laneNum
)
{
    FLCN_STATUS status = FLCN_OK;

    // Stop Margining for all lanes, without clearing the errors
    bifStopStepMargining_HAL(&Bif, LW_U16_MAX, LW_FALSE);

    //
    // SW is no longer ready for Margining. Program *_MARGINING_RDY
    // and *_MARGINING_SW_RDY to 0.
    //
    bifUpdateMarginingRdyStatus_HAL(&Bif, LW_FALSE);

    //
    // Program SW state for link recovery state. This will be used for
    // restoring the Margining state when link comes out of recovery
    //
    MARGINING_DATA_GET()->bMarginingStoppedLinkRecovery        = LW_TRUE;
    MARGINING_RECOVERY_STATE_GET()->lastActiveLaneNum          = laneNum;
    MARGINING_RECOVERY_STATE_GET()->lastMarginingScheduledMask =
        MARGINING_DATA_GET()->marginingScheduledMask;

    return status;
}

/*!
 * @brief Exelwtes the required sequence after link resumes from recovery
 *
 * @return    FLCN_OK    If the required sequence is exelwted correctly
 */
static FLCN_STATUS
s_bifMarginHandleLinkResumeRecovery(void)
{
    DISPATCH_BIF dispatchBif;
    FLCN_STATUS status = FLCN_OK;

    //
    // Link is no longer in recovery state. We don't need to reset entire
    // recovery state. Recovery state in ilwalidated as a result of setting
    // bMarginingStoppedLinkRecovery to FALSE
    //
    MARGINING_DATA_GET()->bMarginingStoppedLinkRecovery = LW_FALSE;

    //
    // Is there any chance that this would be pre-empted by the task BIF? - No.
    // So there is no need to worry about race condition in task LOWLATENCY
    // Restore marginingScheduled mask before scheduling Step Margin command
    //
    MARGINING_DATA_GET()->marginingScheduledMask = 
        MARGINING_RECOVERY_STATE_GET()->lastMarginingScheduledMask;

    dispatchBif.hdr.eventType = BIF_EVENT_ID_LANE_MARGIN;
    dispatchBif.signalLaneMargining.laneNum =
        MARGINING_RECOVERY_STATE_GET()->lastActiveLaneNum;;

    status = lwrtosQueueIdSend(LWOS_QUEUE_ID(PMU, BIF), &dispatchBif,
                               sizeof(dispatchBif),
                               BIF_STEP_MARGINING_TIMEOUT_US);
    if (status != FLCN_OK)
    {
        //
        // Not doing error handling here right away. When PMU_BREAK is hit in
        // a static function, PMU logs do not correctly point to source location
        // since more often that not, static functions would be made inline by
        // the compiler. Error handling would be done in the caller function
        //
        goto s_bifMarginHandleLinkResumeRecovery_exit;
    }

s_bifMarginHandleLinkResumeRecovery_exit:
    return status;
}
