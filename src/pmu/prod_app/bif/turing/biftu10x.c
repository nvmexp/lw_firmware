/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   biftu10x.c
 * @brief  Contains BIF routines specific to TU10X.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmu_objpmu.h"

#include "dev_lw_xp.h"
#include "dev_lw_xve.h"
#include "dev_trim.h"

/* ------------------------- Application Includes --------------------------- */
#include "config/g_bif_private.h"
#include "pmu_objbif.h"
#include "cmdmgmt/cmdmgmt.h"
#include "g_pmurmifrpc.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */

// TODO: Remove after fixed manuals are available
#define LW_XVE_LINK_CONTROL_STATUS_LINK_SPEED_16P0 0x00000004 /* R---V */

/* ------------------------- Prototypes ------------------------------------- */
static LwBool       s_bifLtssmIsIdle_TU10X(void *pArgs)
    GCC_ATTRIB_SECTION("imem_libBif", "s_bifLtssmIsIdle_TU10X");
static LwBool       s_bifLtssmIsIdleAndLinkReady_TU10X(void *pArgs)
    GCC_ATTRIB_SECTION("imem_libBif", "s_bifLtssmIsIdleAndLinkReady_TU10X");

/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief Get the current BIF speed
 *
 * @param[out]  pSpeed  Current bus speed. Must not be NULL. Will be set to
 *                      RM_PMU_BIF_LINK_SPEED_ILWALID in case of an error.
 *
 * @return      FLCN_OK on success, otherwise an error code.
 */
FLCN_STATUS
bifGetBusSpeed_TU10X
(
    RM_PMU_BIF_LINK_SPEED   *pSpeed
)
{
    LwU32 tempRegVal;

    tempRegVal = bifBar0ConfigSpaceRead_HAL(&Bif, LW_TRUE, LW_XVE_LINK_CONTROL_STATUS);
    switch (DRF_VAL(_XVE, _LINK_CONTROL_STATUS, _LINK_SPEED, tempRegVal))
    {
        case LW_XVE_LINK_CONTROL_STATUS_LINK_SPEED_2P5:
            *pSpeed = RM_PMU_BIF_LINK_SPEED_GEN1PCIE;
            break;
        case LW_XVE_LINK_CONTROL_STATUS_LINK_SPEED_5P0:
            *pSpeed = RM_PMU_BIF_LINK_SPEED_GEN2PCIE;
            break;
        case LW_XVE_LINK_CONTROL_STATUS_LINK_SPEED_8P0:
            *pSpeed = RM_PMU_BIF_LINK_SPEED_GEN3PCIE;
            break;
        case LW_XVE_LINK_CONTROL_STATUS_LINK_SPEED_16P0:
            *pSpeed = RM_PMU_BIF_LINK_SPEED_GEN4PCIE;
            break;
        default:
            *pSpeed = RM_PMU_BIF_LINK_SPEED_ILWALID;
            return FLCN_ERROR;
    }

    return FLCN_OK;
}

/*!
 * @brief Sets the MAX_LINK_RATE field in xpPlLinkConfig0 according to speed.
 *
 * @param[in]   xpPlLinkConfig0     Register value to modify.
 * @param[in]   speed               Speed to set the register according to.
 *
 * @return      The modified value to be written back to XP_PL_LINK_CONFIG_0.
 */
LwU32
bifSetXpPlLinkRate_TU10X
(
    LwU32                   xpPlLinkConfig0,
    RM_PMU_BIF_LINK_SPEED   speed
)
{
    switch (speed)
    {
        default:
        case RM_PMU_BIF_LINK_SPEED_GEN1PCIE:
            xpPlLinkConfig0 = FLD_SET_DRF(_XP, _PL_LINK_CONFIG_0,
                                          _MAX_LINK_RATE, _2500_MTPS,
                                          xpPlLinkConfig0);
            break;
        case RM_PMU_BIF_LINK_SPEED_GEN2PCIE:
            xpPlLinkConfig0 = FLD_SET_DRF(_XP, _PL_LINK_CONFIG_0,
                                          _MAX_LINK_RATE, _5000_MTPS,
                                          xpPlLinkConfig0);
            break;
        case RM_PMU_BIF_LINK_SPEED_GEN3PCIE:
            xpPlLinkConfig0 = FLD_SET_DRF(_XP, _PL_LINK_CONFIG_0,
                                          _MAX_LINK_RATE, _8000_MTPS,
                                          xpPlLinkConfig0);
            break;
        case RM_PMU_BIF_LINK_SPEED_GEN4PCIE:
            xpPlLinkConfig0 = FLD_SET_DRF(_XP, _PL_LINK_CONFIG_0,
                                          _MAX_LINK_RATE, _16000_MTPS,
                                          xpPlLinkConfig0);
            break;
    }
    return xpPlLinkConfig0;
}

/*!
 * @brief Change the BIF speed
 *
 * @param[in]   speed           Bus speed to switch to.
 * @param[out]  pResultSpeed    Bus speed after the command. May be NULL if
 *                              this information isn't needed. Will be set to
 *                              RM_PMU_BIF_LINK_SPEED_ILWALID in case of an
 *                              error.
 * @param[out]  pErrorId        Additional error information in the case of an
 *                              error. If there is no error, this is zero. May
 *                              be NULL.
 *
 * @return      FLCN_OK on success, otherwise an error code
 */
FLCN_STATUS
bifChangeBusSpeed_TU10X
(
    RM_PMU_BIF_LINK_SPEED   speed,
    RM_PMU_BIF_LINK_SPEED   *pResultSpeed,
    LwU32                   *pErrorId
)
{
    LwU32                 errorId                       = LW_RM_PMU_BIF_LTSSM_LINK_READY_ERROR_NONE;
    FLCN_STATUS           status                        = FLCN_OK;
    LwBool                bL1Disabled                   = LW_TRUE;
    LwBool                bL0sDisabled                  = LW_TRUE;
    LwBool                bWarningForTdrSent            = LW_TRUE;
    LwU8                  linkSpeedChangeAttemptCounter = 0;
    FLCN_STATUS           statusRpc;
    FLCN_TIMESTAMP        timeout;
    RM_PMU_BIF_LINK_SPEED lwrrentLinkSpeed;
    LwU32                 savedDlMgr0;
    LwU32                 xpPlLinkConfig0;
    LwU32                 xveLinkCtrlStatus;
    LwU32                 ltssmLinkReadyTimeoutNs;
    LwU32                 linkReadyTimeTakenNs;
    PMU_RM_RPC_STRUCT_BIF_BUS_SPEED_CHANGE_LOG  rpc;

    ltssmLinkReadyTimeoutNs = Bif.linkReadyTimeoutNs;

    rpc.targetLinkSpeed = speed;
    bifGetBusSpeed_HAL(&Bif, &lwrrentLinkSpeed);
    rpc.lwrrentLinkSpeed = lwrrentLinkSpeed;

    //
    // AM-TODO: Adding additional speeds here in the future may not be ideal,
    //          since TU10X only supports these speeds, but we could move this
    //          check outside this function and not re-implement this HAL.
    //
    switch (speed)
    {
        case RM_PMU_BIF_LINK_SPEED_GEN1PCIE:
            if (FLD_TEST_DRF(_RM_PMU, _BIF_CAPS, _GEN1PCIE, _NOT_SUPPORTED, Bif.bifCaps))
            {
                goto bifChangeBusSpeed_Finish;
            }
            break;
        case RM_PMU_BIF_LINK_SPEED_GEN2PCIE:
            if (FLD_TEST_DRF(_RM_PMU, _BIF_CAPS, _GEN2PCIE, _NOT_SUPPORTED, Bif.bifCaps))
            {
                goto bifChangeBusSpeed_Finish;
            }
            break;
        case RM_PMU_BIF_LINK_SPEED_GEN3PCIE:
            if (FLD_TEST_DRF(_RM_PMU, _BIF_CAPS, _GEN3PCIE, _NOT_SUPPORTED, Bif.bifCaps))
            {
                goto bifChangeBusSpeed_Finish;
            }
            break;
        case RM_PMU_BIF_LINK_SPEED_GEN4PCIE:
            if (FLD_TEST_DRF(_RM_PMU, _BIF_CAPS, _GEN4PCIE, _NOT_SUPPORTED, Bif.bifCaps))
            {
                goto bifChangeBusSpeed_Finish;
            }
            break;

        default: // invalid
            errorId = LW_RM_PMU_BIF_LTSSM_LINK_READY_ERROR_ILWALID_SPEED;
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto bifChangeBusSpeed_Finish;
    }

    bifGetL0sMask_HAL(&Bif, &bL0sDisabled);

    // If L0s is enabled, disable l0s now
    if (!bL0sDisabled)
    {
        bifEnableL0sMask_HAL(&Bif, LW_TRUE);
    }

    bifGetL1Mask_HAL(&Bif, &bL1Disabled);

    // If L1 is enabled, disable l1 now
    if (!bL1Disabled)
    {
        //
        // Need to pass RM_PMU_BIF_LINK_SPEED_ILWALID since we want to disable L1
        // irrespective of what the pcie-platform table says
        //
        bifEnableL1Mask_HAL(&Bif, LW_TRUE, RM_PMU_BIF_LINK_SPEED_ILWALID);
    }

    lwrtosSemaphoreTakeWaitForever(BifResident.switchLock);
    {
        // Set safe timings while we do the switch. We'll restore the register when we're done.
        savedDlMgr0 = REG_RD32(FECS, LW_XP_DL_MGR_0(0));
        REG_WR32(FECS, LW_XP_DL_MGR_0(0), FLD_SET_DRF(_XP, _DL_MGR_0, _SAFE_TIMING, _ENABLE, savedDlMgr0));

        if (Bif.bRtlSim)
        {
            //
            // Timer WAR found by HW perf team.  They had a script for Gen3
            // transitions that worked, and they needed this to get around a
            // hang.  RM also sees this hang on RTL without this.  Bug 753939.
            //
            bifSetPllLockDelay_HAL(&Bif, 0x3);
        }

        //
        // For the gen3 design, we use LW_XP_PL_LINK_CONFIG_0(0)
        // The way to use this register is to do a
        // read-modify-write setting LW_XP_PL_LINK_CONFIG_0_MAX_LINK_RATE = [2.5/5/8GHz/16GHz)
        // and setting LW_XP_PL_LINK_CONFIG_0_LTSSM_DIRECTIVE = _CHANGE_SPEED
        //

        osPTimerTimeNsLwrrentGet(&timeout);
        // Wait for LTSSM to be in an idle state before we change the link state.
        if (!OS_PTIMER_SPIN_WAIT_NS_COND(s_bifLtssmIsIdle_TU10X, NULL, LW_RM_PMU_BIF_LTSSM_IDLE_TIMEOUT_NS))
        {
            // Timed out waiting for LTSSM to go idle
            status = FLCN_ERR_TIMEOUT;
            errorId = LW_RM_PMU_BIF_LTSSM_LINK_READY_ERROR_TIMEOUT_BEFORE_SPEED_CHANGE;
            rpc.timeTakenForLinkToGoIdleNs = osPTimerTimeNsElapsedGet(&timeout);
            goto bifChangeBusSpeed_Abort;
        }
        rpc.timeTakenForLinkToGoIdleNs = osPTimerTimeNsElapsedGet(&timeout);

        // Set MAX_LINK_RATE according to requested speed
        xpPlLinkConfig0 = bifSetXpPlLinkRate_HAL(&Bif, REG_RD32(FECS, LW_XP_PL_LINK_CONFIG_0(0)), speed);

        // Set target link width equal to current link width
        xveLinkCtrlStatus = bifBar0ConfigSpaceRead_HAL(&Bif, LW_TRUE, LW_XVE_LINK_CONTROL_STATUS);
        switch (DRF_VAL(_XVE, _LINK_CONTROL_STATUS, _NEGOTIATED_LINK_WIDTH, xveLinkCtrlStatus))
        {
            case LW_XVE_LINK_CONTROL_STATUS_NEGOTIATED_LINK_WIDTH_X16:
                xpPlLinkConfig0 = FLD_SET_DRF(_XP, _PL_LINK_CONFIG_0,
                                              _TARGET_TX_WIDTH, _x16,
                                              xpPlLinkConfig0);
                break;
            case LW_XVE_LINK_CONTROL_STATUS_NEGOTIATED_LINK_WIDTH_X8:
                xpPlLinkConfig0 = FLD_SET_DRF(_XP, _PL_LINK_CONFIG_0,
                                              _TARGET_TX_WIDTH, _x8,
                                              xpPlLinkConfig0);
                break;
            case LW_XVE_LINK_CONTROL_STATUS_NEGOTIATED_LINK_WIDTH_X4:
                xpPlLinkConfig0 = FLD_SET_DRF(_XP, _PL_LINK_CONFIG_0,
                                              _TARGET_TX_WIDTH, _x4,
                                              xpPlLinkConfig0);
                break;
            case LW_XVE_LINK_CONTROL_STATUS_NEGOTIATED_LINK_WIDTH_X2:
                xpPlLinkConfig0 = FLD_SET_DRF(_XP, _PL_LINK_CONFIG_0,
                                              _TARGET_TX_WIDTH, _x2,
                                              xpPlLinkConfig0);
                break;
            case LW_XVE_LINK_CONTROL_STATUS_NEGOTIATED_LINK_WIDTH_X1:
                xpPlLinkConfig0 = FLD_SET_DRF(_XP, _PL_LINK_CONFIG_0,
                                              _TARGET_TX_WIDTH, _x1,
                                              xpPlLinkConfig0);
                break;
            default:
                xpPlLinkConfig0 = FLD_SET_DRF(_XP, _PL_LINK_CONFIG_0,
                                              _TARGET_TX_WIDTH, _x16,
                                              xpPlLinkConfig0);
                break;
        }

        // Write the target link width+speed.
        REG_WR32(FECS, LW_XP_PL_LINK_CONFIG_0(0), xpPlLinkConfig0);

        // Setup register to trigger speed change
        xpPlLinkConfig0 = FLD_SET_DRF(_XP, _PL_LINK_CONFIG_0,
                                      _LTSSM_DIRECTIVE, _CHANGE_SPEED,
                                      xpPlLinkConfig0);

        // Poll after link speed changes are complete
        osPTimerTimeNsLwrrentGet(&timeout);
        do
        {
            status = bifLinkSpeedChangeAttempt_HAL(&Bif, xpPlLinkConfig0, speed, &errorId,
                                                   ltssmLinkReadyTimeoutNs, &linkReadyTimeTakenNs);
            linkSpeedChangeAttemptCounter++;
            // If the change got the correct link speed, or threw an error, leave the loop.
            if (status != FLCN_ERR_MORE_PROCESSING_REQUIRED)
            {
                break;
            }

            //
            // With a LINK_CHANGE_TIMEOUT of ~4.29s, there is a slight chance
            // of hitting TDR, hence do a one time logging here after
            // threshold of 1.5s is reached for link_change_timeout
            //
            if ((bWarningForTdrSent) && 
                (osPTimerTimeNsElapsedGet(&timeout) >= LW_RM_PMU_BIF_LINK_CHANGE_TIMEOUT_THRESHOLD_NS))
            {
                rpc.timeTakenForLinkSpeedChangeNs = osPTimerTimeNsElapsedGet(&timeout);
                rpc.linkSpeedChangeAttemptCounter = linkSpeedChangeAttemptCounter;
                rpc.timeTakenForLinkReadyAttemptingSpeedChangeNs = linkReadyTimeTakenNs;
                rpc.timeTakenForLinkReadyAfterSpeedChangeNs = 0;
                rpc.errorId = errorId;
                PMU_RM_RPC_EXELWTE_NON_BLOCKING(statusRpc, BIF, BUS_SPEED_CHANGE_LOG,
                                        &rpc);
                bWarningForTdrSent = LW_FALSE;
            }

            // Check if we've hit the timeout for trying to do the link change
            if (osPTimerTimeNsElapsedGet(&timeout) >= LW_RM_PMU_BIF_LINK_CHANGE_TIMEOUT_NS)
            {
                // Try one last time...
                status = bifLinkSpeedChangeAttempt_HAL(&Bif, xpPlLinkConfig0, speed, &errorId,
                                                       ltssmLinkReadyTimeoutNs, &linkReadyTimeTakenNs);
                linkSpeedChangeAttemptCounter++;
                if (status == FLCN_ERR_MORE_PROCESSING_REQUIRED)
                {
                    status = FLCN_ERR_TIMEOUT;
                    errorId = LW_RM_PMU_BIF_LTSSM_LINK_READY_ERROR_MORE_PROCESSING_REQUIRED;
                    break;
                }
                else
                {
                    break;
                }
            }
            else
            {
                status = FLCN_OK;
            }
        } while (status != FLCN_ERR_TIMEOUT);

        rpc.timeTakenForLinkSpeedChangeNs = osPTimerTimeNsElapsedGet(&timeout);
        rpc.linkSpeedChangeAttemptCounter = linkSpeedChangeAttemptCounter;
        rpc.timeTakenForLinkReadyAttemptingSpeedChangeNs = linkReadyTimeTakenNs;

        if (status != FLCN_OK)
        {
            // Set MAX_LINK_RATE according to our current link speed
            bifGetBusSpeed_HAL(&Bif, &lwrrentLinkSpeed);

            xpPlLinkConfig0 = bifSetXpPlLinkRate_HAL(&Bif, REG_RD32(FECS, LW_XP_PL_LINK_CONFIG_0(0)),
                                                     lwrrentLinkSpeed);

            REG_WR32(FECS, LW_XP_PL_LINK_CONFIG_0(0), xpPlLinkConfig0);
        }

bifChangeBusSpeed_Abort:
        if (status != FLCN_OK)
        {
            // Notify of failed link speed change.
            if (Bif.linkSpeedSwitchingErrorCount != LW_U32_MAX)
            {
                Bif.linkSpeedSwitchingErrorCount++;
            }
        }

        // Restore original timings
        REG_WR32(FECS, LW_XP_DL_MGR_0(0), savedDlMgr0);

        osPTimerTimeNsLwrrentGet(&timeout);
        // Wait for link to go ready (i.e., for everything to complete)
        if (!OS_PTIMER_SPIN_WAIT_NS_COND(s_bifLtssmIsIdleAndLinkReady_TU10X, NULL, ltssmLinkReadyTimeoutNs))
        {
            // Timed out waiting for LTSSM to go idle
            status = FLCN_ERR_TIMEOUT;
            errorId = LW_RM_PMU_BIF_LTSSM_LINK_READY_ERROR_TIMEOUT_AFTER_SPEED_CHANGE;
        }
        rpc.timeTakenForLinkReadyAfterSpeedChangeNs = osPTimerTimeNsElapsedGet(&timeout);

        //
        // There's not really anything we can do if the above times out, but we
        // want to try to wait for the link to be ready before we finish.
        //
    }
    lwrtosSemaphoreGive(BifResident.switchLock);

    // If L1 was disabled before the start of the speed change, re-enable it now
    if (!bL1Disabled)
    {
        bifEnableL1Mask_HAL(&Bif, LW_FALSE, RM_PMU_BIF_LINK_SPEED_ILWALID);
    }

    // If L0s was disabled before the start of the speed change, re-enable it now
    if (!bL0sDisabled)
    {
        bifEnableL0sMask_HAL(&Bif, LW_FALSE);
    }

bifChangeBusSpeed_Finish:
    if (pResultSpeed != NULL)
    {
        if (status != FLCN_OK)
        {
            *pResultSpeed = RM_PMU_BIF_LINK_SPEED_ILWALID;
        }
        else
        {
            status = bifGetBusSpeed_HAL(&Bif, pResultSpeed);
        }
    }
    if (pErrorId != NULL)
    {
        *pErrorId = errorId;
        rpc.errorId = errorId;
    }

    // RPC from PMU->RM to log link speed change details
    if (!((rpc.errorId == LW_RM_PMU_BIF_LTSSM_LINK_READY_ERROR_NONE) &&
        (rpc.timeTakenForLinkSpeedChangeNs <= LW_RM_PMU_BIF_SPEED_SWITCH_ACCEPTED_TIME_NS)))
    {
        PMU_RM_RPC_EXELWTE_NON_BLOCKING(statusRpc, BIF, BUS_SPEED_CHANGE_LOG,
                                        &rpc);
    }

    return status;
}

/**
 * HAL function to set ASPM L0s mask.
 *
 * @param  The boolean indicating to enable or disable L0s mask.
 *         LW_TRUE means the mask is enabled and L0s is disabled.
 *         LW_FALSE means the mask is disabled and L0s is enabled.
 */
void
bifEnableL0sMask_TU10X
(
    LwBool   bL0sMaskEnable
)
{
    LwU32  regVal;
    LwBool bLwrrentL0sMask;

    //
    // If L0s mask is enabled because of regkey, chipset side support or
    // because it is a Tesla board (ASPM not POR on Tesla) return early.
    // Though INIT value of CYA is to disable L0s, it is very restrictive to
    // assume that no other place modifies ASPM CYA. Therefore check before
    // returning early
    //
    if (FLD_TEST_DRF(_RM_PMU, _BIF_CAPS, _L0S_MASK, _ENABLED, Bif.bifCaps))
    {
        bifGetL0sMask_HAL(&Bif, &bLwrrentL0sMask);
        if (bLwrrentL0sMask)
        {
            return;
        }
    }

    // If L0s mask is enabled L0s will be disabled
    bL0sMaskEnable = FLD_TEST_DRF(_RM_PMU, _BIF_CAPS, _L0S_MASK, _ENABLED, Bif.bifCaps) || bL0sMaskEnable;

    regVal = bifBar0ConfigSpaceRead_HAL(&Bif, LW_FALSE, LW_XVE_PRIV_XV_0);
    regVal = bL0sMaskEnable ? (FLD_SET_DRF(_XVE, _PRIV_XV_0, _CYA_L0S_ENABLE, _DISABLED, regVal)) : (FLD_SET_DRF(_XVE, _PRIV_XV_0, _CYA_L0S_ENABLE, _ENABLED, regVal));
    bifBar0ConfigSpaceWrite_HAL(&Bif, LW_FALSE, LW_XVE_PRIV_XV_0, regVal);
}

/**
 * HAL function to set ASPM L1 mask.
 *
 * @param  The boolean indicating to enable or disable L1 mask.
 *         LW_TRUE means the mask is enabled and L1 is disabled.
 *         LW_FALSE means the mask is disabled and L1 is enabled.
 */
void
bifEnableL1Mask_TU10X
(
    LwBool                bL1MaskEnable,
    RM_PMU_BIF_LINK_SPEED genSpeed
)
{
    LwU32  regVal;
    LwBool bLwrrentL1Mask;

    //
    // bifCaps' L1 mask is based on regkey or chipset side support
    // or if it is a Tesla board(ASPM is not POR on Tesla). 
    // If bifCaps' L1 mask is set, irrespective of what bL1MaskEnable says,
    // we should set CYA's L1 bit to 1. If it is already set, return early.
    // If it is not set, following code would take care of setting that to 1
    //
    if (FLD_TEST_DRF(_RM_PMU, _BIF_CAPS, _L1_MASK, _ENABLED, Bif.bifCaps))
    {
        bifGetL1Mask_HAL(&Bif, &bLwrrentL1Mask);
        if (bLwrrentL1Mask)
        {
            return;
        }
    }

    //
    // If l1Enable field is present in PCIE Platform table then
    // it gets priority over per P-state based PCIE entries
    //
    if (Bif.platformParams.bL1EnablePresent)
    {
        LwU8 l1Enable = Bif.platformParams.l1Enable;
        switch (genSpeed)
        {
            case RM_PMU_BIF_LINK_SPEED_GEN1PCIE:
            {
                bL1MaskEnable = !(FLD_TEST_DRF(_PMU, _PCIE_PLATFORM, _GEN1_L1, _ENABLED, l1Enable));
                break;
            }
            case RM_PMU_BIF_LINK_SPEED_GEN2PCIE:
            {
                bL1MaskEnable = !(FLD_TEST_DRF(_PMU, _PCIE_PLATFORM, _GEN2_L1, _ENABLED, l1Enable));
                break;
            }
            case RM_PMU_BIF_LINK_SPEED_GEN3PCIE:
            {
                bL1MaskEnable = !(FLD_TEST_DRF(_PMU, _PCIE_PLATFORM, _GEN3_L1, _ENABLED, l1Enable));
                break;
            }
            case RM_PMU_BIF_LINK_SPEED_GEN4PCIE:
            {
                bL1MaskEnable = !(FLD_TEST_DRF(_PMU, _PCIE_PLATFORM, _GEN4_L1, _ENABLED, l1Enable));
                break;
            }
            default:
            {
                break;
            }
        }
    }

    // If L1 mask is enabled L1 will be disabled
    bL1MaskEnable = FLD_TEST_DRF(_RM_PMU, _BIF_CAPS, _L1_MASK, _ENABLED,
                                 Bif.bifCaps) || bL1MaskEnable;

    regVal = bifBar0ConfigSpaceRead_HAL(&Bif, LW_FALSE, LW_XVE_PRIV_XV_0);
    regVal = bL1MaskEnable ?
             (FLD_SET_DRF(_XVE, _PRIV_XV_0, _CYA_L1_ENABLE, _DISABLED, regVal)) :
             (FLD_SET_DRF(_XVE, _PRIV_XV_0, _CYA_L1_ENABLE, _ENABLED, regVal));
    bifBar0ConfigSpaceWrite_HAL(&Bif, LW_FALSE, LW_XVE_PRIV_XV_0, regVal);
}

/*!
 * Update the ASPM Winidle field
 *
 * @param[in]  bL1MaskEnable  boolean indicating to enable or disable L1-mask.
 *
 */
void
bifAspmWinidleUpdate_TU10X
(
    LwBool   bL1MaskEnable
)
{
    LwU32 regVal;

    regVal = BAR0_REG_RD32(LW_XP_DL_CYA_1(0));
    regVal = bL1MaskEnable ?
             (FLD_SET_DRF(_XP, _DL_CYA_1, _ASPM_WINIDLE, _INIT, regVal)) :
             (FLD_SET_DRF(_XP, _DL_CYA_1, _ASPM_WINIDLE, _L1_GEN1_ENABLE, regVal));
    BAR0_REG_WR32(LW_XP_DL_CYA_1(0), regVal);
}

/*!
 * @brief bifWriteL0sThreshold_TU10X
 *
 * param[in] l0sThreshold
 */
void
bifWriteL0sThreshold_TU10X
(
    LwU16 l0sThreshold
)
{
    LwU32 data;

    data = BAR0_REG_RD32(LW_XP_DL_MGR_1(0));
    data = FLD_SET_DRF_NUM (_XP, _DL_MGR_1, _L0S_THRESHOLD, l0sThreshold, data);
    BAR0_REG_WR32(LW_XP_DL_MGR_1(0), data);
}

/*!
 * This function enables or disables L1 substates(L1.1/L1.2)
 *
 * @param[in]  pGpu     GPU object pointer
 * @param[in]  pBif     Pointer to bif object
 * @param[in]  bEnable  Flag that enables or disables L1 substates
 *
 * @return  'LW_OK' if successful, an RM error code otherwise.
 */
FLCN_STATUS
bifL1SubstatesEnable_TU10X
(
    LwBool bEnable,
    LwU32  pstateIdx,
    LwU8   pcieIdx,
    LwBool bIsPerfChangeInprogress
)
{
    LwU32     regVal;
    LwBool    L11Allowed;
    LwBool    L12Allowed;

    // Don't allow enable/disable until L1 substates is setup
    if (FLD_TEST_DRF(_RM_PMU, _BIF_CAPS, _L1SS, _NOT_SUPPORTED, Bif.bifCaps))
    {
        return FLCN_ERROR;
    }

    if (bEnable)
    {
        L11Allowed = FLD_TEST_DRF(_RM_PMU, _BIF_CAPS, _L11, _SUPPORTED, Bif.bifCaps) && bifIsL11Allowed(pcieIdx, bIsPerfChangeInprogress);
        L12Allowed = FLD_TEST_DRF(_RM_PMU, _BIF_CAPS, _L12, _SUPPORTED, Bif.bifCaps) && bifIsL12Allowed(pcieIdx, bIsPerfChangeInprogress);

        if (L11Allowed && L12Allowed)
        {
            regVal = Bif.l1ssRegs.aspmL1SsCtrlEnable;
        }
        else if (L12Allowed)
        {
            regVal = Bif.l1ssRegs.aspmL12Ctr1Enable;
        }
        else if (L11Allowed)
        {
            regVal = Bif.l1ssRegs.aspmL11Ctr1Enable;
        }
        else
        {
            regVal = Bif.l1ssRegs.aspmL1SsCtr1Disable;
        }
    }
    else
    {
        regVal = Bif.l1ssRegs.aspmL1SsCtr1Disable;
    }

    bifBar0ConfigSpaceWrite_HAL(&Bif, LW_FALSE, LW_XVE_L1_PM_SUBSTATES_CTRL1, regVal);
    return FLCN_OK;
}

/**
 * HAL function to update Latency Tolerance Reporting values
 *
 * @param  pexLtrSnoopLatencyValue    Snoop Latency Value to be updated.
 * @param  pexLtrSnoopLatencyScale    Snoop Latency Scale to be updated.
 * @param  pexLtrNoSnoopLatencyValue  No Snoop Latency Value to be updated.
 * @param  pexLtrNoSnoopLatencyScale  No Snoop Latency Scale to be updated.
 * @return Returns LW_OK on success.  Anything else should be considered a failure.
 */
FLCN_STATUS
bifUpdateLtrValue_TU10X
(
    LwU16    pexLtrSnoopLatencyValue,
    LwU8     pexLtrSnoopLatencyScale,
    LwU16    pexLtrNoSnoopLatencyValue,
    LwU8     pexLtrNoSnoopLatencyScale
)
{
    FLCN_STATUS     status     = FLCN_OK;
    LwU32           tempRegVal;
    FLCN_TIMESTAMP  timeout;

    if (FLD_TEST_DRF(_RM_PMU, _BIF_CAPS, _ROOT_PORT_LTR, _NOT_SUPPORTED, Bif.bifCaps))
    {
        status = FLCN_ERR_NOT_SUPPORTED;
        return status;
    }

    tempRegVal = bifBar0ConfigSpaceRead_HAL(&Bif, LW_FALSE, LW_XVE_LTR_HIGH_LATENCY);
    tempRegVal = FLD_SET_DRF_NUM(_XVE, _LTR_HIGH_LATENCY,
                                 _SNOOP_LATENCY_VALUE,
                                 pexLtrSnoopLatencyValue, tempRegVal);
    tempRegVal = FLD_SET_DRF_NUM(_XVE, _LTR_HIGH_LATENCY,
                                 _SNOOP_LATENCY_SCALE,
                                 pexLtrSnoopLatencyScale, tempRegVal);
    tempRegVal = FLD_SET_DRF_NUM(_XVE, _LTR_HIGH_LATENCY,
                                 _NO_SNOOP_LATENCY_VALUE,
                                 pexLtrNoSnoopLatencyValue, tempRegVal);
    tempRegVal = FLD_SET_DRF_NUM(_XVE, _LTR_HIGH_LATENCY,
                                 _NO_SNOOP_LATENCY_SCALE,
                                 pexLtrNoSnoopLatencyScale, tempRegVal);
    bifBar0ConfigSpaceWrite_HAL(&Bif, LW_FALSE, LW_XVE_LTR_HIGH_LATENCY, tempRegVal);

    // Trigger a HW LTR message
    tempRegVal = bifBar0ConfigSpaceRead_HAL(&Bif, LW_FALSE, LW_XVE_LTR_MSG_CTRL);
    tempRegVal = FLD_SET_DRF(_XVE, _LTR_MSG_CTRL, _TRIGGER, _PENDING, tempRegVal);
    bifBar0ConfigSpaceWrite_HAL(&Bif, LW_FALSE, LW_XVE_LTR_MSG_CTRL, tempRegVal);

    // Poll for LTR message send
    osPTimerTimeNsLwrrentGet(&timeout);
    while (LW_TRUE)
    {
        //
        // Once we set the trigger in SW_MSG_CTRL register,
        // we should wait for it to go low to know that msg has been sent out
        //
        tempRegVal = bifBar0ConfigSpaceRead_HAL(&Bif, LW_FALSE, LW_XVE_LTR_MSG_CTRL);
        if (FLD_TEST_DRF(_XVE, _LTR_MSG_CTRL, _TRIGGER, _NOT_PENDING, tempRegVal))
        {
            break;
        }

        if (osPTimerTimeNsElapsedGet(&timeout) >= BIF_TLP_MSGSEND_TIMTOUT_NS)
        {
            status = FLCN_ERR_TIMEOUT;
        }
    }

    return status;
}

/*
 * This function reads and returns the ASPM-L1 enable\disable
 * status in ASPM CYA register
 *
 * @param[out]  pbL1Disabled   True if L1 is disabled
 */
void
bifGetL1Mask_TU10X
(
    LwBool *pbL1Disabled
)
{
    LwU32 tempRegVal;

    tempRegVal = bifBar0ConfigSpaceRead_HAL(&Bif, LW_FALSE, LW_XVE_PRIV_XV_0);
    *pbL1Disabled = FLD_TEST_DRF(_XVE, _PRIV_XV_0, _CYA_L1_ENABLE, _DISABLED, tempRegVal);
}

/*!
 * @brief This function reads and returns the aspm cya L0s enable\disable
 * status i.e. whether aspm cyas L0s is enabled or disabled.
 *
 * @param[out]  pbL0MaskDisabled   pointer to return value for L0s
 *                                 TRUE value implies L0s is Disabled
 *
 * @return : void
 */
void
bifGetL0sMask_TU10X
(
    LwBool *pbL0sMaskDisabled
)
{
    LwU32 tempRegVal;

    tempRegVal = bifBar0ConfigSpaceRead_HAL(&Bif, LW_FALSE, LW_XVE_PRIV_XV_0);
    *pbL0sMaskDisabled = FLD_TEST_DRF(_XVE, _PRIV_XV_0, _CYA_L0S_ENABLE, _DISABLED, tempRegVal);
}

/*
 * This function read PCIE config space register through BAR0 mirror
 *
 * @param[in]  bUseFECS          If to use GPU BAR0 config space through FECS Access Macros       
 * @param[in]  regAddress       The PCIE config space register address
 *
 * @return : register value
 */
LwU32
bifBar0ConfigSpaceRead_TU10X
(
    LwBool bUseFECS,
    LwU32 regAddress
)
{
    if (bUseFECS)
    {
        return FECS_CFG_REG_RD32(regAddress);
    }

    return BAR0_CFG_REG_RD32(regAddress);
}

/*
 * This function write PCIE config space register through BAR0 mirror
 *
 * @param[in]  bUseFECS          If to use GPU BAR0 config space through FECS Access Macros       
 * @param[in]  regAddress       The PCIE config space register address
 * @param[in]  regValue         The register value to update
 *
 * @return : void
 */
void
bifBar0ConfigSpaceWrite_TU10X
(
    LwBool bUseFECS,
    LwU32 regAddress,
    LwU32 regValue
)
{
    if (bUseFECS)
    {
        FECS_CFG_REG_WR32(regAddress, regValue);
    }

    BAR0_CFG_REG_WR32(regAddress, regValue);
}

/* ------------------------- Private Functions ------------------------------ */

/*!
 * @brief OS_PTIMER_COND_FUNC to check if the LTSSM is lwrrently idle
 *
 * @param   pArgs   Unused
 *
 * @return  LW_TRUE if idle, LW_FALSE otherwise
 */
static LwBool
s_bifLtssmIsIdle_TU10X
(
    void *pArgs
)
{
    LwU32 xpPlLinkConfig0;

    xpPlLinkConfig0 = REG_RD32(FECS, LW_XP_PL_LINK_CONFIG_0(0));
    return FLD_TEST_DRF(_XP, _PL_LINK_CONFIG_0, _LTSSM_STATUS, _IDLE, xpPlLinkConfig0) &&
           FLD_TEST_DRF(_XP, _PL_LINK_CONFIG_0, _LTSSM_DIRECTIVE, _NORMAL_OPERATIONS, xpPlLinkConfig0);
}

/*!
 * @brief OS_PTIMER_COND_FUNC to check if the LTSSM is idle and has indicated
 *        the link is ready.
 *
 * @param   pArgs   Unused
 *
 * @return  LW_TRUE if link is ready, LW_FALSE otherwise
 */
static LwBool
s_bifLtssmIsIdleAndLinkReady_TU10X
(
    void *pArgs
)
{
    LwU32 ltssmState;
    if (!s_bifLtssmIsIdle_TU10X(NULL))
    {
        return LW_FALSE;
    }

    ltssmState = REG_RD32(FECS, LW_XP_PL_LTSSM_STATE);
    return FLD_TEST_DRF(_XP_PL, _LTSSM_STATE, _IS_LINK_READY, _TRUE, ltssmState);
}

/*!
 * @brief Attempt to change link speed.
 *
 * @param[in]  xpPlLinkConfig0         New value to write to XP_PL_LINK_CONFIG_0
 * @param[in]  speed                   Expected new link speed.
 * @param[out] pErrorId                Set to an error ID number if a failure oclwrred.
 * @param[in]  ltssmLinkReadyTimeoutNs LTSSM link ready timeout used during speed change
 * @param[out] pLinkReadyTimeTakenNs   Time taken by link to get ready
 *
 * @return  FLCN_OK if the link speed was successful. FLCN_ERR_TIMEOUT if
 *          waiting for the LTSSM to declare the link ready took too long.
 *          FLCN_ERR_MORE_PROCESSING_REQUIRED if there were no errors, but the
 *          reported link speed is incorrect.
 */
FLCN_STATUS
bifLinkSpeedChangeAttempt_TU10X
(
    LwU32                 xpPlLinkConfig0,
    RM_PMU_BIF_LINK_SPEED speed,
    LwU32                 *pErrorId,
    LwU32                 ltssmLinkReadyTimeoutNs,
    LwU32                 *pLinkReadyTimeTakenNs
)
{
    FLCN_TIMESTAMP        timeout;
    RM_PMU_BIF_LINK_SPEED lwrrentLinkSpeed;
    FLCN_STATUS           status = FLCN_OK;

    // Trigger speed change
    REG_WR32(FECS, LW_XP_PL_LINK_CONFIG_0(0), xpPlLinkConfig0);

    osPTimerTimeNsLwrrentGet(&timeout);
    // Wait for link to go ready (i.e., for the speed change to complete)
    if (!OS_PTIMER_SPIN_WAIT_NS_COND(s_bifLtssmIsIdleAndLinkReady_TU10X, NULL, ltssmLinkReadyTimeoutNs))
    {
        *pErrorId = LW_RM_PMU_BIF_LTSSM_LINK_READY_ERROR_TIMEOUT_ATTEMPTING_SPEED_CHANGE;
        *pLinkReadyTimeTakenNs = osPTimerTimeNsElapsedGet(&timeout);
        return FLCN_ERR_TIMEOUT;
    }
    *pLinkReadyTimeTakenNs = osPTimerTimeNsElapsedGet(&timeout);

    // Check if we trained to target speeds.
    status = bifGetBusSpeed_HAL(&Bif, &lwrrentLinkSpeed);
    if (status != FLCN_OK)
    {
        *pErrorId = LW_RM_PMU_BIF_LTSSM_LINK_READY_ERROR_TRAINING_TO_TARGET_SPEED;
        return status;
    }
    if (lwrrentLinkSpeed != speed)
    {
        // Speed doesn't match, we'll need to try again.
        return FLCN_ERR_MORE_PROCESSING_REQUIRED;
    }
    return status;
}
