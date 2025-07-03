/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2022 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   bifgh100.c
 * @brief  Contains GH100 based HAL implementations
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmu_bar0.h"
#include "pmu_objpmu.h"

#include "dev_lw_xpl.h"
#include "dev_xtl_ep_pri.h"
#include "dev_xtl_ep_pcfg_gpu.h"
#include "dev_lw_uxl_pri_ctx.h"
#include "dev_lw_uphy_dlm_addendum.h"
#include "dev_top.h"
#include "pri_lw_xal_ep.h"
#include "dbgprintf.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "config/g_bif_private.h"
#include "pmu_objbif.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
static LwBool       s_bifLtssmIsIdle_GH100(void *pArgs)
    GCC_ATTRIB_SECTION("imem_libBif", "s_bifLtssmIsIdle_GH100");
static LwBool       s_bifLtssmIsIdleAndLinkReady_GH100(void *pArgs)
    GCC_ATTRIB_SECTION("imem_libBif", "s_bifLtssmIsIdleAndLinkReady_GH100");
static LwBool       s_bifEomDoneStatus_GH100(void *pArgs)
    GCC_ATTRIB_SECTION("imem_libBif", "s_bifEomDoneStatus_GH100");
static void         s_bifDisableHigherGenSpeeds_GH100(LwU32 speed)
    GCC_ATTRIB_SECTION("imem_libBif", "s_bifDisableHigherGenSpeeds_GH100");
/* ------------------------- Macros and Defines ----------------------------- */

/*
 * This function read PCIE config space register through BAR0 mirror
 *
 * @param[in]  bUseFECS         If to use GPU BAR0 config space through FECS Access Macros       
 * @param[in]  regAddress       The PCIE config space register address
 *
 * @return : register value
 */
LwU32
bifBar0ConfigSpaceRead_GH100
(
    LwBool bUseFECS,
    LwU32 regAddress
)
{
    LwU32 regData;

    if (bUseFECS)
    {
        regData = FECS_REG_RD32(DEVICE_BASE(LW_EP_PCFGM) + regAddress);
    }
    else
    {
        regData = BAR0_REG_RD32(DEVICE_BASE(LW_EP_PCFGM) + regAddress);
    }

    return regData;
}

/*
 * This function write PCIE config space register through BAR0 mirror
 *
 * @param[in]  bUseFECS         If to use GPU BAR0 config space through FECS Access Macros       
 * @param[in]  regAddress       The PCIE config space register address
 * @param[in]  regValue         The register value to update
 *
 * @return : void
 */
void
bifBar0ConfigSpaceWrite_GH100
(
    LwBool bUseFECS,
    LwU32 regAddress,
    LwU32 regValue
)
{
    if (bUseFECS)
    {
        FECS_REG_WR32(DEVICE_BASE(LW_EP_PCFGM) + regAddress, regValue);
    }
    else
    {
        BAR0_REG_WR32(DEVICE_BASE(LW_EP_PCFGM) + regAddress, regValue);
    }
}

/*!
 * @brief Get the current BIF speed
 *
 * @param[out]  pSpeed  Current bus speed. Must not be NULL. Will be set to
 *                      RM_PMU_BIF_LINK_SPEED_ILWALID in case of an error.
 *
 * @return      FLCN_OK on success, otherwise an error code.
 */
FLCN_STATUS
bifGetBusSpeed_GH100
(
    RM_PMU_BIF_LINK_SPEED   *pSpeed
)
{
    LwU32 tempRegVal;

    tempRegVal = bifBar0ConfigSpaceRead_HAL(&Bif, LW_TRUE, LW_EP_PCFG_GPU_LINK_CONTROL_STATUS);

    switch (DRF_VAL(_EP_PCFG_GPU, _LINK_CONTROL_STATUS, _LWRRENT_LINK_SPEED, tempRegVal))
    {
        case LW_EP_PCFG_GPU_LINK_CONTROL_STATUS_LWRRENT_LINK_SPEED_2P5:
            *pSpeed = RM_PMU_BIF_LINK_SPEED_GEN1PCIE;
            break;
        case LW_EP_PCFG_GPU_LINK_CONTROL_STATUS_LWRRENT_LINK_SPEED_5P0:
            *pSpeed = RM_PMU_BIF_LINK_SPEED_GEN2PCIE;
            break;
        case LW_EP_PCFG_GPU_LINK_CONTROL_STATUS_LWRRENT_LINK_SPEED_8P0:
            *pSpeed = RM_PMU_BIF_LINK_SPEED_GEN3PCIE;
            break;
        case LW_EP_PCFG_GPU_LINK_CONTROL_STATUS_LWRRENT_LINK_SPEED_16P0:
            *pSpeed = RM_PMU_BIF_LINK_SPEED_GEN4PCIE;
            break;
        case LW_EP_PCFG_GPU_LINK_CONTROL_STATUS_LWRRENT_LINK_SPEED_32P0:
            *pSpeed = RM_PMU_BIF_LINK_SPEED_GEN5PCIE;
            break;
        default:
            *pSpeed = RM_PMU_BIF_LINK_SPEED_ILWALID;
            return FLCN_ERROR;
    }

    return FLCN_OK;
}

/*!
 * @brief Disable higher gen speeds. To be called only during gen speed switch.
 *        Refer Bug 3476266 comment #6 and #15
 *
 * @param[in] speed  Speed to set the register according to.
 */
static void
s_bifDisableHigherGenSpeeds_GH100
(
    RM_PMU_BIF_LINK_SPEED speed
)
{
    LwU32 regVal = XPL_REG_RD32(FECS, LW_XPL_PL_LTSSM_TRAINING_0);

    switch (speed)
    {
        default:
        case RM_PMU_BIF_LINK_SPEED_GEN1PCIE:
            regVal = FLD_SET_DRF(_XPL, _PL_LTSSM_TRAINING_0,
                                 _DISABLE_GEN2_SPEED, _TRUE, regVal);
            regVal = FLD_SET_DRF(_XPL, _PL_LTSSM_TRAINING_0,
                                 _DISABLE_GEN3_SPEED, _TRUE, regVal);
            regVal = FLD_SET_DRF(_XPL, _PL_LTSSM_TRAINING_0,
                                 _DISABLE_GEN4_SPEED, _TRUE, regVal);
            regVal = FLD_SET_DRF(_XPL, _PL_LTSSM_TRAINING_0,
                                 _DISABLE_GEN5_SPEED, _TRUE, regVal);
            break;
        case RM_PMU_BIF_LINK_SPEED_GEN2PCIE:
            regVal = FLD_SET_DRF(_XPL, _PL_LTSSM_TRAINING_0,
                                 _DISABLE_GEN2_SPEED, _FALSE, regVal);
            regVal = FLD_SET_DRF(_XPL, _PL_LTSSM_TRAINING_0,
                                 _DISABLE_GEN3_SPEED, _TRUE, regVal);
            regVal = FLD_SET_DRF(_XPL, _PL_LTSSM_TRAINING_0,
                                 _DISABLE_GEN4_SPEED, _TRUE, regVal);
            regVal = FLD_SET_DRF(_XPL, _PL_LTSSM_TRAINING_0,
                                 _DISABLE_GEN5_SPEED, _TRUE, regVal);
            break;
        case RM_PMU_BIF_LINK_SPEED_GEN3PCIE:
            regVal = FLD_SET_DRF(_XPL, _PL_LTSSM_TRAINING_0,
                                 _DISABLE_GEN2_SPEED, _FALSE, regVal);
            regVal = FLD_SET_DRF(_XPL, _PL_LTSSM_TRAINING_0,
                                 _DISABLE_GEN3_SPEED, _FALSE, regVal);
            regVal = FLD_SET_DRF(_XPL, _PL_LTSSM_TRAINING_0,
                                 _DISABLE_GEN4_SPEED, _TRUE, regVal);
            regVal = FLD_SET_DRF(_XPL, _PL_LTSSM_TRAINING_0,
                                 _DISABLE_GEN5_SPEED, _TRUE, regVal);
            break;
        case RM_PMU_BIF_LINK_SPEED_GEN4PCIE:
            regVal = FLD_SET_DRF(_XPL, _PL_LTSSM_TRAINING_0,
                                 _DISABLE_GEN2_SPEED, _FALSE, regVal);
            regVal = FLD_SET_DRF(_XPL, _PL_LTSSM_TRAINING_0,
                                 _DISABLE_GEN3_SPEED, _FALSE, regVal);
            regVal = FLD_SET_DRF(_XPL, _PL_LTSSM_TRAINING_0,
                                 _DISABLE_GEN4_SPEED, _FALSE, regVal);
            regVal = FLD_SET_DRF(_XPL, _PL_LTSSM_TRAINING_0,
                                 _DISABLE_GEN5_SPEED, _TRUE, regVal);
            break;
        case RM_PMU_BIF_LINK_SPEED_GEN5PCIE:
            regVal = FLD_SET_DRF(_XPL, _PL_LTSSM_TRAINING_0,
                                 _DISABLE_GEN2_SPEED, _FALSE, regVal);
            regVal = FLD_SET_DRF(_XPL, _PL_LTSSM_TRAINING_0,
                                 _DISABLE_GEN3_SPEED, _FALSE, regVal);
            regVal = FLD_SET_DRF(_XPL, _PL_LTSSM_TRAINING_0,
                                 _DISABLE_GEN4_SPEED, _FALSE, regVal);
            regVal = FLD_SET_DRF(_XPL, _PL_LTSSM_TRAINING_0,
                                 _DISABLE_GEN5_SPEED, _FALSE, regVal);
            break;
    }

    XPL_REG_WR32(FECS, LW_XPL_PL_LTSSM_TRAINING_0, regVal);
}

/*!
 * @brief Sets the MAX_LINK_RATE field in xplPlLinkConfig0 according to speed.
 *
 * @param[in]   xplPlLinkConfig0     Register value to modify.
 * @param[in]   speed               Speed to set the register according to.
 *
 * @return      The modified value to be written back to LW_XPL_PL_LTSSM_LINK_CONFIG_0
 */
LwU32
bifSetXpPlLinkRate_GH100
(
    LwU32                   xplPlLinkConfig0,
    RM_PMU_BIF_LINK_SPEED   speed
)
{
    switch (speed)
    {
        default:
        case RM_PMU_BIF_LINK_SPEED_GEN1PCIE:
            xplPlLinkConfig0 = FLD_SET_DRF(_XPL_PL, _LTSSM_LINK_CONFIG_0,
                                           _TARGET_LINK_SPEED, _2500_MTPS,
                                           xplPlLinkConfig0);
            break;
        case RM_PMU_BIF_LINK_SPEED_GEN2PCIE:
            xplPlLinkConfig0 = FLD_SET_DRF(_XPL_PL, _LTSSM_LINK_CONFIG_0,
                                           _TARGET_LINK_SPEED, _5000_MTPS,
                                           xplPlLinkConfig0);
            break;
        case RM_PMU_BIF_LINK_SPEED_GEN3PCIE:
            xplPlLinkConfig0 = FLD_SET_DRF(_XPL_PL, _LTSSM_LINK_CONFIG_0,
                                           _TARGET_LINK_SPEED, _8000_MTPS,
                                           xplPlLinkConfig0);
            break;
        case RM_PMU_BIF_LINK_SPEED_GEN4PCIE:
            xplPlLinkConfig0 = FLD_SET_DRF(_XPL_PL, _LTSSM_LINK_CONFIG_0,
                                           _TARGET_LINK_SPEED, _16000_MTPS,
                                           xplPlLinkConfig0);
            break;
        case RM_PMU_BIF_LINK_SPEED_GEN5PCIE:
            xplPlLinkConfig0 = FLD_SET_DRF(_XPL_PL, _LTSSM_LINK_CONFIG_0,
                                           _TARGET_LINK_SPEED, _32000_MTPS,
                                           xplPlLinkConfig0);
            break;
    }

    if (IsSilicon() && Bif.bDisableHigherGenSpeedsDuringGenSpeedSwitch == LW_TRUE)
    {
        s_bifDisableHigherGenSpeeds_GH100(speed);
    }

    return xplPlLinkConfig0;
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
bifChangeBusSpeed_GH100
(
    RM_PMU_BIF_LINK_SPEED   speed,
    RM_PMU_BIF_LINK_SPEED   *pResultSpeed,
    LwU32                   *pErrorId
)
{
    LwU32                   errorId = 0;
    FLCN_STATUS             status = FLCN_OK;
    FLCN_TIMESTAMP          timeout;
    RM_PMU_BIF_LINK_SPEED   lwrrentLinkSpeed;
    LwU32                   savedDlMgr0;
    LwU32                   xplPlLinkConfig0;
    LwU32                   busLinkCtrlStatus;
    LwU32                   ltssmLinkReadyTimeoutNs;
    LwBool                  bL1Disabled = LW_TRUE;

    ltssmLinkReadyTimeoutNs = LW_RM_PMU_BIF_LTSSM_LINK_READY_TIMEOUT_NS;

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
        case RM_PMU_BIF_LINK_SPEED_GEN5PCIE:
            if (FLD_TEST_DRF(_RM_PMU, _BIF_CAPS, _GEN5PCIE, _NOT_SUPPORTED, Bif.bifCaps))
            {
                goto bifChangeBusSpeed_Finish;
            }
            break;

        default: // invalid
            errorId = 1;
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto bifChangeBusSpeed_Finish;
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
        savedDlMgr0 = XPL_REG_RD32(FECS, LW_XPL_DL_MGR_CTRL0);

        XPL_REG_WR32(FECS, LW_XPL_DL_MGR_CTRL0, FLD_SET_DRF(_XPL, _DL_MGR_CTRL0, _SAFE_TIMING, _ENABLE, savedDlMgr0));

        if (Bif.bRtlSim)
        {
            //
            // Timer WAR found by HW perf team.  They had a script for Gen3
            // transitions that worked, and they needed this to get around a
            // hang.  RM also sees this hang on RTL without this.  Bug 753939.
            //

            //
            // TODO by anaikwade: Removed the usage of LW_PTRIM_SYS_PEXPLL* register
            // as it is going to be renamed to LW_PTRIM_PCIE_PEXPLL*
            // SW to add back the code with updated naming scheme
            // Tracked in Bug 200570282.
            // Refer bug 2764273, comment#69 for detailed understanding
            //
        } 
        //
        // The way to use this register is to do a
        // read-modify-write setting LW_XP_PL_LINK_CONFIG_0_MAX_LINK_RATE = [2.5/5/8GHz/16GHz)
        // and setting LW_XP_PL_LINK_CONFIG_0_LTSSM_DIRECTIVE = _CHANGE_SPEED
        //

        // Wait for LTSSM to be in an idle state before we change the link state.
        if (!OS_PTIMER_SPIN_WAIT_NS_COND(s_bifLtssmIsIdle_GH100, NULL, LW_RM_PMU_BIF_LTSSM_IDLE_TIMEOUT_NS))
        {
            // Timed out waiting for LTSSM to go idle
            status = FLCN_ERR_TIMEOUT;
            errorId = 2;
            goto bifChangeBusSpeed_Abort;
        }

        // Set MAX_LINK_RATE according to requested speed
        xplPlLinkConfig0 = bifSetXpPlLinkRate_HAL(&Bif, XPL_REG_RD32(FECS, LW_XPL_PL_LTSSM_LINK_CONFIG_0), speed);

        // Set target link width equal to current link width
        busLinkCtrlStatus = bifBar0ConfigSpaceRead_HAL(&Bif, LW_TRUE, LW_EP_PCFG_GPU_LINK_CONTROL_STATUS);

        switch (DRF_VAL(_EP_PCFG_GPU, _LINK_CONTROL_STATUS, _NEGOTIATED_LINK_WIDTH, busLinkCtrlStatus))
        {
            case LW_EP_PCFG_GPU_LINK_CONTROL_STATUS_NEGOTIATED_LINK_WIDTH_X32:
                // LW_XPL_PL_LTSSM_LINK_CONFIG_0_TARGET_TX_WIDTH_X32 is not defined yet. Default to x16
                xplPlLinkConfig0 = FLD_SET_DRF(_XPL, _PL_LTSSM_LINK_CONFIG_0,
                                               _TARGET_TX_WIDTH, _X16,
                                               xplPlLinkConfig0);
                break;
            case LW_EP_PCFG_GPU_LINK_CONTROL_STATUS_NEGOTIATED_LINK_WIDTH_X16:
                xplPlLinkConfig0 = FLD_SET_DRF(_XPL, _PL_LTSSM_LINK_CONFIG_0,
                                               _TARGET_TX_WIDTH, _X16,
                                               xplPlLinkConfig0);
                break;
            case LW_EP_PCFG_GPU_LINK_CONTROL_STATUS_NEGOTIATED_LINK_WIDTH_X8:
                xplPlLinkConfig0 = FLD_SET_DRF(_XPL, _PL_LTSSM_LINK_CONFIG_0,
                                               _TARGET_TX_WIDTH, _X8,
                                               xplPlLinkConfig0);
                break;
            case LW_EP_PCFG_GPU_LINK_CONTROL_STATUS_NEGOTIATED_LINK_WIDTH_X4:
                xplPlLinkConfig0 = FLD_SET_DRF(_XPL, _PL_LTSSM_LINK_CONFIG_0,
                                               _TARGET_TX_WIDTH, _X4,
                                               xplPlLinkConfig0);
                break;
            case LW_EP_PCFG_GPU_LINK_CONTROL_STATUS_NEGOTIATED_LINK_WIDTH_X2:
                xplPlLinkConfig0 = FLD_SET_DRF(_XPL, _PL_LTSSM_LINK_CONFIG_0,
                                               _TARGET_TX_WIDTH, _X2,
                                               xplPlLinkConfig0);
                break;
            case LW_EP_PCFG_GPU_LINK_CONTROL_STATUS_NEGOTIATED_LINK_WIDTH_X1:
                xplPlLinkConfig0 = FLD_SET_DRF(_XPL, _PL_LTSSM_LINK_CONFIG_0,
                                               _TARGET_TX_WIDTH, _X1,
                                               xplPlLinkConfig0);
                break;
            default:
                xplPlLinkConfig0 = FLD_SET_DRF(_XPL, _PL_LTSSM_LINK_CONFIG_0,
                                               _TARGET_TX_WIDTH, _X16,
                                               xplPlLinkConfig0);
                break; 
        } 
        // Write the target link width+speed.
        XPL_REG_WR32(FECS, LW_XPL_PL_LTSSM_LINK_CONFIG_0, xplPlLinkConfig0);

        // Setup register to trigger speed change
        xplPlLinkConfig0 = FLD_SET_DRF(_XPL, _PL_LTSSM_LINK_CONFIG_0,
                                       _LTSSM_DIRECTIVE, _CHANGE_SPEED,
                                       xplPlLinkConfig0);

        // Poll after link speed changes are complete
        osPTimerTimeNsLwrrentGet(&timeout);
        do
        {
            status = bifLinkSpeedChangeAttempt_HAL(&Bif, xplPlLinkConfig0, speed, &errorId,
                                                   ltssmLinkReadyTimeoutNs, NULL);
            // If the change got the correct link speed, or threw an error, leave the loop.
            if (status != FLCN_ERR_MORE_PROCESSING_REQUIRED)
            {
                break;
            }

            // Check if we've hit the timeout for trying to do the link change
            if (osPTimerTimeNsElapsedGet(&timeout) >= LW_RM_PMU_BIF_LINK_CHANGE_TIMEOUT_NS)
            {
                // Try one last time...
                status = bifLinkSpeedChangeAttempt_HAL(&Bif, xplPlLinkConfig0, speed, &errorId,
                                                       ltssmLinkReadyTimeoutNs, NULL);
                if (status == FLCN_ERR_MORE_PROCESSING_REQUIRED)
                {
                    status = FLCN_ERR_TIMEOUT;
                    errorId = 5;
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

        if (status != FLCN_OK)
        {
            // Set MAX_LINK_RATE according to our current link speed
            bifGetBusSpeed_HAL(&Bif, &lwrrentLinkSpeed);

            xplPlLinkConfig0 = bifSetXpPlLinkRate_HAL(&Bif, XPL_REG_RD32(FECS, LW_XPL_PL_LTSSM_LINK_CONFIG_0),
                                                     lwrrentLinkSpeed);

            XPL_REG_WR32(FECS, LW_XPL_PL_LTSSM_LINK_CONFIG_0, xplPlLinkConfig0);
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
        XPL_REG_WR32(FECS, LW_XPL_DL_MGR_CTRL0, savedDlMgr0);

        // Wait for link to go ready (i.e., for everything to complete)
        if (!OS_PTIMER_SPIN_WAIT_NS_COND(s_bifLtssmIsIdleAndLinkReady_GH100, NULL, ltssmLinkReadyTimeoutNs))
        {
            // Timed out waiting for LTSSM to go idle
            status = FLCN_ERR_TIMEOUT;
            errorId = 6;
        }
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
    }
    return status;
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
bifL1SubstatesEnable_GH100
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

    bifBar0ConfigSpaceWrite_HAL(&Bif, LW_FALSE, LW_EP_PCFG_GPU_L1_PM_SS_CONTROL_1_REGISTER, regVal);

    return FLCN_OK;
}

/*!
 * This function programs priv protected registers for enabling L1-PLL-PD, L1-CPM
 * Additional CYAs(other than CYA1 and CYA2), will be programmed here if required
 * 
 * @return  FLCN_OK if successful
 */
FLCN_STATUS
bifL1LpwrFeatureSetup_GH100()
{
    //
    // Lwrrently, this function just programs the idle threshold value to the HW
    // default. Ideally, this is not required but having placeholder will help
    // during power-on if we require to program some other value
    //
    LwU32 regVal = XPL_REG_RD32(FECS, LW_XPL_PL_LTSSM_LP_12);
    regVal = FLD_SET_DRF(_XPL, _PL_LTSSM_LP_12, _START_PLL_OFF_MIN_DLY, _INIT, regVal);
    XPL_REG_WR32(FECS, LW_XPL_PL_LTSSM_LP_12, regVal);

    return FLCN_OK;
}

/*!
 * This function enables or disables L1-PLL Power Down
 *
 * @param[in]  bEnable  True if PLL-PD needs to be enabled
 *
 * @return  FLCN_OK if successful
 */
FLCN_STATUS
bifEnableL1PllPowerDown_GH100
(
    LwBool bEnable
)
{
    LwU32 regVal = XTL_REG_RD32(LW_XTL_EP_PRI_PWR_MGMT_CYA1);

    regVal = bEnable ? FLD_SET_DRF_NUM(_XTL_EP_PRI, _PWR_MGMT_CYA1,
                        _ASPM_L1_PLL_PD_ENABLE, 0x1, regVal) :
                       FLD_SET_DRF_NUM(_XTL_EP_PRI, _PWR_MGMT_CYA1,
                        _ASPM_L1_PLL_PD_ENABLE, 0x0, regVal);
    XTL_REG_WR32(LW_XTL_EP_PRI_PWR_MGMT_CYA1, regVal);

    return FLCN_OK;
}

/*!
 * This function enables or disables L1 Clock power management(CPM)
 *
 * @param[in]  bEnable  True if L1-CPM needs to be enabled
 *
 * @return  FLCN_OK if successful
 */
FLCN_STATUS
bifEnableL1ClockPwrMgmt_GH100
(
    LwBool bEnable
)
{
    LwU32 regVal = XTL_REG_RD32(LW_XTL_EP_PRI_PWR_MGMT_CYA1);

    regVal = bEnable ? FLD_SET_DRF_NUM(_XTL_EP_PRI, _PWR_MGMT_CYA1,
                        _CLOCK_PM_ENABLE, 0x1, regVal) :
                       FLD_SET_DRF_NUM(_XTL_EP_PRI, _PWR_MGMT_CYA1,
                        _CLOCK_PM_ENABLE, 0x0, regVal);
    XTL_REG_WR32(LW_XTL_EP_PRI_PWR_MGMT_CYA1, regVal);

    return FLCN_OK;
}

/*!
 * @brief   Modifies PCIE poison control error data state
 *
 * @param[in]   bReportEnable
 *  Whether to enable or disable the poison control error data reporting.
 */
void
bifPciePoisonControlErrDataControl_GH100
(
    LwBool bReportEnable
)
{
    LwU32 poisonControl = REG_RD32(BAR0, LW_XAL_EP_POISON_CONTROL);

    if (bReportEnable)
    {
        poisonControl = FLD_SET_DRF(
            _XAL, _EP_POISON_CONTROL, _ERR_DATA_REPORT, _ENABLED,
            poisonControl);
    }
    else
    {
        poisonControl = FLD_SET_DRF(
            _XAL, _EP_POISON_CONTROL, _ERR_DATA_REPORT, _DISABLED,
            poisonControl);
    }

    REG_WR32(BAR0, LW_XAL_EP_POISON_CONTROL, poisonControl);
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
s_bifLtssmIsIdle_GH100
(
    void *pArgs
)
{
    LwU32 xplPlLinkConfig0;

    xplPlLinkConfig0 = XPL_REG_RD32(FECS, LW_XPL_PL_LTSSM_LINK_CONFIG_0);
    return FLD_TEST_DRF(_XPL, _PL_LTSSM_LINK_CONFIG_0, _LTSSM_STATUS, _IDLE, xplPlLinkConfig0) &&
           FLD_TEST_DRF(_XPL, _PL_LTSSM_LINK_CONFIG_0, _LTSSM_DIRECTIVE, _NORMAL_OPERATIONS, xplPlLinkConfig0);
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
s_bifLtssmIsIdleAndLinkReady_GH100
(
    void *pArgs
)
{
    LwU32 ltssmState;
    if (!s_bifLtssmIsIdle_GH100(NULL))
    {
        return LW_FALSE;
    }

    ltssmState = XPL_REG_RD32(FECS, LW_XPL_PL_LTSSM_STATE);
    return FLD_TEST_DRF(_XPL, _PL_LTSSM_STATE, _IS_LINK_READY, _TRUE, ltssmState);
}

/*!
 * @brief Attempt to change link speed.
 *
 * @param[in]  xplPlLinkConfig0        New value to write to XP_PL_LINK_CONFIG_0
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
bifLinkSpeedChangeAttempt_GH100
(
    LwU32                 xplPlLinkConfig0,
    RM_PMU_BIF_LINK_SPEED speed,
    LwU32                 *pErrorId,
    LwU32                 ltssmLinkReadyTimeoutNs,
    LwU32                 *pLinkReadyTimeTakenNs
)
{
    RM_PMU_BIF_LINK_SPEED lwrrentLinkSpeed;
    FLCN_STATUS status = FLCN_OK;

    // Trigger speed change
    XPL_REG_WR32(FECS, LW_XPL_PL_LTSSM_LINK_CONFIG_0, xplPlLinkConfig0);

    // Wait for link to go ready (i.e., for the speed change to complete)
    if (!OS_PTIMER_SPIN_WAIT_NS_COND(s_bifLtssmIsIdleAndLinkReady_GH100, NULL, ltssmLinkReadyTimeoutNs))
    {
        // Timed out waiting for LTSSM to indicate the link is ready
        *pErrorId = 3;
        return FLCN_ERR_TIMEOUT;
    }

    // Check if we trained to target speeds.
    status = bifGetBusSpeed_HAL(&Bif, &lwrrentLinkSpeed);
    if (status != FLCN_OK)
    {
        *pErrorId = 4;
        return status;
    }
    if (lwrrentLinkSpeed != speed)
    {
        // Speed doesn't match, we'll need to try again.
        return FLCN_ERR_MORE_PROCESSING_REQUIRED;
    }
    return FLCN_OK;
}

/*!
 * @brief Write the threshold and threshold frac value to its respective
 *        register
 *
 * @param[in] threshold     l1 threshold value
 * @param[in] thresholdFrac l1 threshold fraction value
 * @param[in] genSpeed      Gen speed
 *
 */
void
bifWriteL1Threshold_GH100
(
    LwU8                   threshold,
    LwU8                   thresholdFrac,
    RM_PMU_BIF_LINK_SPEED  genSpeed
)
{
    LwU32 data;
    LwU8  l1Threshold                       = threshold;
    LwU8  l1ThresholdFrac                   = thresholdFrac;
    RM_PMU_BIF_PCIE_L1_THRESHOLD_DATA *pTemp = Bif.platformParams.l1ThresholdData;

    if (Bif.platformParams.bAspmThreshTablePresent)
    {
        switch (genSpeed)
        {
            case RM_PMU_BIF_LINK_SPEED_GEN1PCIE:
            {
                l1Threshold     = pTemp[RM_PMU_BIF_GEN1_INDEX].l1Threshold;
                l1ThresholdFrac = pTemp[RM_PMU_BIF_GEN1_INDEX].l1ThresholdFrac;
                break;
            }
            case RM_PMU_BIF_LINK_SPEED_GEN2PCIE:
            {
                if (FLD_TEST_DRF(_RM_PMU, _BIF_CAPS, _GEN2PCIE, _SUPPORTED,
                                 Bif.bifCaps))
                {
                    l1Threshold     = pTemp[RM_PMU_BIF_GEN2_INDEX].l1Threshold;
                    l1ThresholdFrac = pTemp[RM_PMU_BIF_GEN2_INDEX].l1ThresholdFrac;
                }
                break;
            }
            case RM_PMU_BIF_LINK_SPEED_GEN3PCIE:
            {
                if (FLD_TEST_DRF(_RM_PMU, _BIF_CAPS, _GEN3PCIE, _SUPPORTED,
                                 Bif.bifCaps))
                {
                    l1Threshold     = pTemp[RM_PMU_BIF_GEN3_INDEX].l1Threshold;
                    l1ThresholdFrac = pTemp[RM_PMU_BIF_GEN3_INDEX].l1ThresholdFrac;
                }
                break;
            }
            case RM_PMU_BIF_LINK_SPEED_GEN4PCIE:
            {
                if (FLD_TEST_DRF(_RM_PMU, _BIF_CAPS, _GEN4PCIE, _SUPPORTED,
                                 Bif.bifCaps))
                {
                    l1Threshold     = pTemp[RM_PMU_BIF_GEN4_INDEX].l1Threshold;
                    l1ThresholdFrac = pTemp[RM_PMU_BIF_GEN4_INDEX].l1ThresholdFrac;
                }
                break;
            }
            case RM_PMU_BIF_LINK_SPEED_GEN5PCIE:
            {
                if (FLD_TEST_DRF(_RM_PMU, _BIF_CAPS, _GEN5PCIE, _SUPPORTED,
                                 Bif.bifCaps))
                {
                    //
                    // Todo: The Add GEN5 support
                    // RM_PMU_BIF_PCIE_L1_THRESHOLD_DATA does not have GEN5 info yet.
                    //
                    l1Threshold     = LW_XPL_DL_MGR_CTRL1_L1_THRESHOLD_MAX;
                    l1ThresholdFrac = LW_XPL_DL_MGR_CTRL1_L1_THRESHOLD_FRAC_MAX;
                }
                break;
            }
        }
    }

    // Verify values are within HW defined range
    if (PMUCFG_FEATURE_ENABLED(PMU_SELWRITY_HARDENING))
    {
        if ((l1Threshold < LW_XPL_DL_MGR_CTRL1_L1_THRESHOLD_MIN) ||
            (l1Threshold > LW_XPL_DL_MGR_CTRL1_L1_THRESHOLD_MAX) ||
            (l1ThresholdFrac < LW_XPL_DL_MGR_CTRL1_L1_THRESHOLD_FRAC_MIN) ||
            (l1ThresholdFrac > LW_XPL_DL_MGR_CTRL1_L1_THRESHOLD_FRAC_MAX))
        {
            PMU_BREAKPOINT();
            goto exit;
        }
    }

    data = XPL_REG_RD32(FECS, LW_XPL_DL_MGR_CTRL1);

    data = FLD_SET_DRF_NUM(_XPL, _DL_MGR_CTRL1, _L1_THRESHOLD, l1Threshold, data);
    data = FLD_SET_DRF_NUM(_XPL, _DL_MGR_CTRL1, _L1_THRESHOLD_FRAC, l1ThresholdFrac, data);

    XPL_REG_WR32(FECS, LW_XPL_DL_MGR_CTRL1, data);

exit:
    return;
}

/*!
 * @brief bifCheckL1ThresholdData_GA10X()
 *  These checks are based on:
 *  https://confluence.lwpu.com/display/PMU/Input+validation+for+pmuRpcBifCfgInit
 *
 */
FLCN_STATUS
bifCheckL1Threshold_GH100(void)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32 idx;

    if (Bif.platformParams.bAspmThreshTablePresent)
    {
        if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_BIF))
        {
            if (((DRF_VAL(_EP_PCFG_GPU, _L1_PM_SS_CONTROL_1_REGISTER, _LTR_L12_THRESHOLD_VALUE,
                          Bif.l1ssRegs.aspmL1SsCtrlEnable) !=
                  LW_BIF_LTR_L1_2_THRESHOLD_VALUE) &&
                 (DRF_VAL(_EP_PCFG_GPU, _L1_PM_SS_CONTROL_1_REGISTER, _LTR_L12_THRESHOLD_VALUE,
                          Bif.l1ssRegs.aspmL1SsCtrlEnable) !=
                  LW_EP_PCFG_GPU_L1_PM_SS_CONTROL_1_REGISTER_LTR_L12_THRESHOLD_SCALE_DEFAULT))
                ||
                ((DRF_VAL(_EP_PCFG_GPU, _L1_PM_SS_CONTROL_1_REGISTER, _LTR_L12_THRESHOLD_SCALE,
                          Bif.l1ssRegs.aspmL1SsCtrlEnable) !=
                  LW_BIF_LTR_L1_2_THRESHOLD_SCALE) &&
                 (DRF_VAL(_EP_PCFG_GPU, _L1_PM_SS_CONTROL_1_REGISTER, _LTR_L12_THRESHOLD_SCALE,
                          Bif.l1ssRegs.aspmL1SsCtrlEnable) !=
                  LW_EP_PCFG_GPU_L1_PM_SS_CONTROL_1_REGISTER_LTR_L12_THRESHOLD_SCALE_DEFAULT)))
            {
                // PMU_BREAKPOINT();
                status = FLCN_ERR_ILWALID_ARGUMENT;
                goto _bifCheckL1Threshold_GH100_exit;
            }
        }

        for (idx = RM_PMU_BIF_GEN1_INDEX;
             idx < RM_PMU_BIF_MAX_GEN_SPEED_SUPPORTED; ++idx)
        {
            if ((Bif.platformParams.l1ThresholdData[idx].l1Threshold < LW_XPL_DL_MGR_CTRL1_L1_THRESHOLD_MIN) ||
                (Bif.platformParams.l1ThresholdData[idx].l1Threshold > LW_XPL_DL_MGR_CTRL1_L1_THRESHOLD_MAX) ||
                (Bif.platformParams.l1ThresholdData[idx].l1ThresholdFrac < LW_XPL_DL_MGR_CTRL1_L1_THRESHOLD_FRAC_MIN) ||
                (Bif.platformParams.l1ThresholdData[idx].l1ThresholdFrac > LW_XPL_DL_MGR_CTRL1_L1_THRESHOLD_FRAC_MAX))
            {
                // PMU_BREAKPOINT();
                status = FLCN_ERR_ILWALID_ARGUMENT;
                goto _bifCheckL1Threshold_GH100_exit;
            }
        }
    }

_bifCheckL1Threshold_GH100_exit:
    return status;
}

/*!
 * @brief Read PEX UPHY DLN registers
 *
 * @param[in]  regAddress     Register whose value needs to be read
 * @param[in]  laneSelectMask Which lane to read from
 * @param[out] pRegValue      The value of the register
 *
 * @return FLCN_OK After reading the regAddress
 */
FLCN_STATUS
bifGetUphyDlnCfgSpace_GH100
(
    LwU32 regAddress,
    LwU32 laneSelectMask,
    LwU16 *pRegValue
)
{
    LwU32 tempRegVal;
    LwU32 rData;

    // Select the desired lane(s) in PHY
    tempRegVal = BAR0_REG_RD32(LW_UXL_LANE_COMMON_MCAST_CTL);
    tempRegVal = FLD_SET_DRF_NUM(_UXL, _LANE_COMMON_MCAST_CTL, _LANE_MASK,
                                 laneSelectMask, tempRegVal);
    BAR0_REG_WR32(LW_UXL_LANE_COMMON_MCAST_CTL, tempRegVal);

    tempRegVal = 0;
    tempRegVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_RDS,
                                 0x1U, tempRegVal);
    tempRegVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_WDS,
                                 0x0U, tempRegVal);
    tempRegVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_ADDR,
                                 regAddress, tempRegVal);
    BAR0_REG_WR32(LW_UXL_LANE_REG_DIRECT_CTL_1(0), tempRegVal);

    rData = DRF_VAL(_UXL, _LANE_REG_DIRECT_CTL_2, _CFG_RDATA,
                    BAR0_REG_RD32(LW_UXL_LANE_REG_DIRECT_CTL_2(0)));
    *pRegValue = (LwU16) rData;

    return FLCN_OK;
}

/**
 * HAL function to set ASPM L1 mask. This PMU function sets CYA1 only.
 * For ASPM to get really enabled, CYA2(controlled by CPU) has to be enabled
 * as well.
 *
 * @param  The boolean indicating to enable or disable L1 mask.
 *         LW_TRUE means the mask is enabled and L1 is disabled.
 *         LW_FALSE means the mask is disabled and L1 is enabled.
 */
void
bifEnableL1Mask_GH100
(
    LwBool                bL1MaskEnable,
    RM_PMU_BIF_LINK_SPEED genSpeed
)
{
    LwU32  regVal;
    LwBool bLwrrentL1Mask = 0;

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
            case RM_PMU_BIF_LINK_SPEED_GEN5PCIE:
            {
                bL1MaskEnable = !(FLD_TEST_DRF(_PMU, _PCIE_PLATFORM, _GEN5_L1, _ENABLED, l1Enable));
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

    regVal = XTL_REG_RD32(LW_XTL_EP_PRI_PWR_MGMT_CYA1);
    regVal = bL1MaskEnable ?
             (FLD_SET_DRF_NUM(_XTL_EP_PRI, _PWR_MGMT_CYA1, _ASPM_L1_ENABLE, 0x0, regVal)) :
             (FLD_SET_DRF_NUM(_XTL_EP_PRI, _PWR_MGMT_CYA1, _ASPM_L1_ENABLE, 0x1, regVal));
    XTL_REG_WR32(LW_XTL_EP_PRI_PWR_MGMT_CYA1, regVal);
}

/*
 * This function reads and returns the ASPM-L1 enable\disable
 * status in CYA1
 *
 * @param[out]  pbL1Disabled  True if L1 is disabled
 */
void
bifGetL1Mask_GH100
(
    LwBool *pbL1Disabled
)
{
    LwU32 regVal;

    regVal = XTL_REG_RD32(LW_XTL_EP_PRI_PWR_MGMT_CYA1);
    *pbL1Disabled = FLD_TEST_DRF_NUM(_XTL_EP_PRI, _PWR_MGMT_CYA1, _ASPM_L1_ENABLE,
                                     0x0, regVal);
}

/*!
 * @brief Run EOM sequence and fill the output buffer with EOM status values
 *
 * @Note: This sequence is same as sequence in TuringPcie::DoGetEomStatus in
 *        MODS
 *
 * @param[in]   eomMode      Mode on EOM
 * @param[in]   eomNblks     Number of blocks
 * @param[in]   eomNerrs     Number of errors
 * @param[in]   eomBerEyeSel BER eye select
 * @param[in]   eomPamEyeSel PAM eye select
 * @param[in]   laneMask     Lanemask specifying which lanes needs to be
 *                           selected for running EOM sequence
 * @params[out] pEomStatus   Pointer to output buffer
 *
 * @return FLCN_OK  If EOM sequence is run correctly
 */
FLCN_STATUS
bifGetEomStatus_GH100
(
    LwU8  eomMode,
    LwU8  eomNblks,
    LwU8  eomNerrs,
    LwU8  eomBerEyeSel,
    LwU8  eomPamEyeSel,
    LwU32 laneMask,
    LwU16 *pEomStatus
)
{
    LwU8        laneNum;
    LwU32       regVal;
    LwBool      bEomDoneStatus;
    LwU8        numRequestedLanes = 0U;
    LwU8        firstLaneFromLsb  = LW_U8_MAX;
    FLCN_STATUS status            = FLCN_OK;

    if (laneMask == 0U)
    {
        // Nothing to do
        goto bifGetEomStatus_GH100_exit;
    }

    for (laneNum = 0; laneNum < BIF_MAX_PCIE_LANES; laneNum++)
    {
        if ((BIT(laneNum) & laneMask) != 0U)
        {
            if (firstLaneFromLsb == LW_U8_MAX)
            {
                firstLaneFromLsb = laneNum;
            }
            numRequestedLanes++;
        }
    }

    if (numRequestedLanes > BIF_MAX_PCIE_LANES ||
        (firstLaneFromLsb + numRequestedLanes) > BIF_MAX_PCIE_LANES)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto bifGetEomStatus_GH100_exit;
    }

    // Set EOM params for requested lanes
    for (laneNum = 0; laneNum < numRequestedLanes; laneNum++)
    {
        bifDoLaneSelect_HAL(&Bif, BIT(firstLaneFromLsb + laneNum));
        bifSetEomParameters_HAL(&Bif, eomMode, eomNblks, eomNerrs,
                                eomBerEyeSel, eomPamEyeSel);
    }

    // Disable EOM for requested lanes.
    for (laneNum = 0; laneNum < numRequestedLanes; laneNum++)
    {
        bifDoLaneSelect_HAL(&Bif, BIT(firstLaneFromLsb + laneNum));
        regVal = BAR0_REG_RD32(LW_UXL_LANE_REG_MISC_CTL_2(0));
        regVal = FLD_SET_DRF(_UXL, _LANE_REG_MISC_CTL_2, _RX_EOM_EN,
                             _DEASSERTED, regVal);
        regVal = FLD_SET_DRF(_UXL, _LANE_REG_MISC_CTL_2, _RX_EOM_OVRD,
                             _DEASSERTED, regVal);
        BAR0_REG_WR32(LW_UXL_LANE_REG_MISC_CTL_2(0), regVal);
    }

    // Poll for EOM done status to be FALSE for all requested lanes
    bEomDoneStatus = LW_FALSE;
    status = bifPollEomDoneStatus_HAL(&Bif, numRequestedLanes, firstLaneFromLsb,
                bEomDoneStatus, LW_UPHY_EOM_DONE_FALSE_TIMEOUT_NS);
    if (status != FLCN_OK)
    {
        goto bifGetEomStatus_GH100_exit;
    }

    // Enable EOM for requested lanes.
    for (laneNum = 0; laneNum < numRequestedLanes; laneNum++)
    {
        bifDoLaneSelect_HAL(&Bif, BIT(firstLaneFromLsb + laneNum));
        regVal = BAR0_REG_RD32(LW_UXL_LANE_REG_MISC_CTL_2(0));
        regVal = FLD_SET_DRF(_UXL, _LANE_REG_MISC_CTL_2, _RX_EOM_OVRD, _ASSERTED,
                             regVal);
        regVal = FLD_SET_DRF(_UXL, _LANE_REG_MISC_CTL_2, _RX_EOM_EN, _ASSERTED,
                             regVal);
        BAR0_REG_WR32(LW_UXL_LANE_REG_MISC_CTL_2(0), regVal);
    }

    // Poll for EOM done status
    bEomDoneStatus = LW_TRUE;
    status = bifPollEomDoneStatus_HAL(&Bif, numRequestedLanes, firstLaneFromLsb,
                bEomDoneStatus, LW_UPHY_EOM_DONE_TRUE_TIMEOUT_NS);
    if (status != FLCN_OK)
    {
        goto bifGetEomStatus_GH100_exit;
    }

    // Disable EOM for requested lanes
    for (laneNum = 0; laneNum < numRequestedLanes; laneNum++)
    {
        bifDoLaneSelect_HAL(&Bif, BIT(firstLaneFromLsb + laneNum));
        regVal = BAR0_REG_RD32(LW_UXL_LANE_REG_MISC_CTL_2(0));
        regVal = FLD_SET_DRF(_UXL, _LANE_REG_MISC_CTL_2, _RX_EOM_EN, _DEASSERTED,
                             regVal);
        regVal = FLD_SET_DRF(_UXL, _LANE_REG_MISC_CTL_2, _RX_EOM_OVRD, _DEASSERTED,
                             regVal);
        BAR0_REG_WR32(LW_UXL_LANE_REG_MISC_CTL_2(0), regVal);
    }

    // Fill up EOM status array starting from index 0 always.
    for (laneNum = firstLaneFromLsb;
         laneNum < (firstLaneFromLsb + numRequestedLanes); laneNum++)
    {
        bifDoLaneSelect_HAL(&Bif, BIT(laneNum));
        pEomStatus[laneNum] = DRF_VAL(_UXL, _LANE_REG_MISC_CTL_2, _RX_EOM_STATUS,
                                      BAR0_REG_RD32(LW_UXL_LANE_REG_MISC_CTL_2(0)));
    }

bifGetEomStatus_GH100_exit:
    return status;
}

/*!
 * @brief Select requsted PCIe lanes.
 *
 * @param[in] laneMask  Bit mask representing requested lanes. If 0th lane
 *                      needs to be selected, 0th bit should be set in the
 *                      laneMask. If 0th and 1st lane need to be selected,
 *                      0th and 1st bits need to be set and so on.
 *
 * @return FLCN_OK
 */
FLCN_STATUS
bifDoLaneSelect_GH100
(
    LwU32 laneMask
)
{
    LwU32 regVal;
    regVal = BAR0_REG_RD32(LW_UXL_LANE_COMMON_MCAST_CTL);
    regVal = FLD_SET_DRF_NUM(_UXL, _LANE_COMMON_MCAST_CTL, _LANE_MASK,
                             laneMask, regVal);
    BAR0_REG_WR32(LW_UXL_LANE_COMMON_MCAST_CTL, regVal);
    return FLCN_OK;
}

/*!
 * @brief Set EOM parameters
 *
 * @param[in] eomMode      Mode of EOM (By default FOM)
 * @param[in] eomNblks     Number of blocks
 * @param[in] eomNerrs     Number of errors
 * @param[in] eomBerEyeSel BER eye select
 * @param[in] eomPamEyeSel PAM eye select
 *
 * @return FLCN_OK  Return FLCN_OK once these parameters sent by MODS are set
 */
FLCN_STATUS
bifSetEomParameters_GH100
(
    LwU8 eomMode,
    LwU8 eomNblks,
    LwU8 eomNerrs,
    LwU8 eomBerEyeSel,
    LwU8 eomPamEyeSel
)
{
    LwU32 tempRegVal;
    LwU32 rData;

    //
    // PAM_EYE_SEL for Hopper can be any non-zero value
    // Check PAM2-PR1 (PCIE Gen5) section at 
    // https://confluence.lwpu.com/display/LWHS/UPHY+6.1+EOM-FOM
    // Hopper and before this value is don't-care
    //
    eomPamEyeSel = LW_UPHY_DLM_AE_EOM_MEAS_EYE_CTRL_PAM_EYE_SEL_DEFAULT;

    // Update EOM_FOM_MODE
    tempRegVal = 0U;
    tempRegVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_RDS,
                                 0x1, tempRegVal);
    tempRegVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_WDS,
                                 0x0, tempRegVal);
    tempRegVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_ADDR,
                                 LW_UPHY_DLM_AE_EOM_MEAS_MODE_CTRL,
                                 tempRegVal);
    BAR0_REG_WR32(LW_UXL_LANE_REG_DIRECT_CTL_1(0), tempRegVal);

    rData = DRF_VAL(_UXL, _LANE_REG_DIRECT_CTL_2, _CFG_RDATA,
                    BAR0_REG_RD32(LW_UXL_LANE_REG_DIRECT_CTL_2(0)));

    rData = FLD_SET_DRF_NUM(_UPHY_DLM, _AE_EOM_MEAS_MODE_CTRL, _FOM_MODE,
                            eomMode, rData);

    tempRegVal = 0U;
    tempRegVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_RDS,
                                 0x0, tempRegVal);
    tempRegVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_WDS,
                                 0x1, tempRegVal);
    tempRegVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_WDATA,
                                 rData, tempRegVal);
    tempRegVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_ADDR,
                                 LW_UPHY_DLM_AE_EOM_MEAS_MODE_CTRL,
                                 tempRegVal);
    BAR0_REG_WR32(LW_UXL_LANE_REG_DIRECT_CTL_1(0), tempRegVal);

    // Update EOM_NBLKS and EOM_NERRS
    tempRegVal = 0U;
    tempRegVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_RDS,
                                 0x1, tempRegVal);
    tempRegVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_WDS,
                                 0x0, tempRegVal);
    tempRegVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_ADDR,
                                 LW_UPHY_DLM_AE_EOM_MEAS_BER_CTRL,
                                 tempRegVal);
    BAR0_REG_WR32(LW_UXL_LANE_REG_DIRECT_CTL_1(0), tempRegVal);

    rData = DRF_VAL(_UXL, _LANE_REG_DIRECT_CTL_2, _CFG_RDATA,
                    BAR0_REG_RD32(LW_UXL_LANE_REG_DIRECT_CTL_2(0)));

    rData = FLD_SET_DRF_NUM(_UPHY_DLM, _AE_EOM_MEAS_BER_CTRL, _NBLKS_MAX,
                            eomNblks, rData);
    rData = FLD_SET_DRF_NUM(_UPHY_DLM, _AE_EOM_MEAS_BER_CTRL, _NERRS_MIN,
                            eomNerrs, rData);

    tempRegVal = 0U;
    tempRegVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_RDS,
                                 0x0, tempRegVal);
    tempRegVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_WDS,
                                 0x1, tempRegVal);
    tempRegVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_WDATA,
                                 rData, tempRegVal);
    tempRegVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_ADDR,
                                 LW_UPHY_DLM_AE_EOM_MEAS_BER_CTRL,
                                 tempRegVal);
    BAR0_REG_WR32(LW_UXL_LANE_REG_DIRECT_CTL_1(0), tempRegVal);

    // Update BER_EYE_SEL and PAM_EYE_SEL
    tempRegVal = 0U;
    tempRegVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_RDS,
                                 0x1, tempRegVal);
    tempRegVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_WDS,
                                 0x0, tempRegVal);
    tempRegVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_ADDR,
                                 LW_UPHY_DLM_AE_EOM_MEAS_EYE_CTRL,
                                 tempRegVal);
    BAR0_REG_WR32(LW_UXL_LANE_REG_DIRECT_CTL_1(0), tempRegVal);

    rData = DRF_VAL(_UXL, _LANE_REG_DIRECT_CTL_2, _CFG_RDATA,
                    BAR0_REG_RD32(LW_UXL_LANE_REG_DIRECT_CTL_2(0)));

    rData = FLD_SET_DRF_NUM(_UPHY_DLM, _AE_EOM_MEAS_EYE_CTRL, _PAM_EYE_SEL,
                            eomPamEyeSel, rData);
    rData = FLD_SET_DRF_NUM(_UPHY_DLM, _AE_EOM_MEAS_EYE_CTRL, _BER_EYE_SEL,
                            eomBerEyeSel, rData);

    tempRegVal = 0U;
    tempRegVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_RDS,
                                 0x0, tempRegVal);
    tempRegVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_WDS,
                                 0x1, tempRegVal);
    tempRegVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_WDATA,
                                 rData, tempRegVal);
    tempRegVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_ADDR,
                                 LW_UPHY_DLM_AE_EOM_MEAS_EYE_CTRL,
                                 tempRegVal);
    BAR0_REG_WR32(LW_UXL_LANE_REG_DIRECT_CTL_1(0), tempRegVal);

    return FLCN_OK;
}

/*!
 * @brief Poll EOM done status for requested lanes
 *
 * @param[in] numRequestedLanes  Number of requested lanes for which EOM done
 *                               status needs to be polled
 * @param[in] firstLaneFromLsb   First lane number from LSB for which EOM done
 *                               status needs to be polled
 * @param[in] bEomDoneStatus     Poll for EOM done status to be set to this
 *                               value
 * @param[in] timeoutNs          Timeout in nanoseconds after which this should
 *                               break out of polling loop
 *
 * @return FLCN_OK  If all requested lanes have expected EOM status
 */
FLCN_STATUS
bifPollEomDoneStatus_GH100
(
    LwU8   numRequestedLanes,
    LwU8   firstLaneFromLsb,
    LwBool bEomDoneStatus,
    LwU32  timeoutNs
)
{
    LwU8 laneNum;
    LwBool bIsFmodel;
    FLCN_STATUS status = FLCN_OK;

    bIsFmodel = FLD_TEST_DRF(_PTOP, _PLATFORM, _TYPE, _FMODEL,
                             REG_RD32(BAR0, LW_PTOP_PLATFORM));

    // Poll for EOM done status to be set to FALSE
    for (laneNum = 0; laneNum < numRequestedLanes; laneNum++)
    {
        // Select one lane at a time here
        bifDoLaneSelect_HAL(&Bif, BIT(firstLaneFromLsb + laneNum));

        // Skip following polling on FMODEL
        if (!bIsFmodel)
        {
            if (!OS_PTIMER_SPIN_WAIT_NS_COND(s_bifEomDoneStatus_GH100,
                                             &bEomDoneStatus, timeoutNs))
            {
                //
                // TODO: Use mailbox register that would be used across
                // the PEX SW in PMU for indicating failure points
                //
                #if (PMU_PROFILE_GH100_RISCV)
                {
                    dbgPrintf(LEVEL_ERROR,
                              "Timed out waiting for EOM status to be set to %x\n",
                              bEomDoneStatus);
                }
                #endif
                PMU_BREAKPOINT();
                status = FLCN_ERR_TIMEOUT;
                goto bifPollEomDoneStatus_GH100_exit;
            }
        }
    }

bifPollEomDoneStatus_GH100_exit:
    return status;
}

/*!
 * @brief OS_PTIMER_COND_FUNC to check if EOM done status is set as expected
 *
 * @param[in] pArgs  Pointer to boolean indicating expected EOM done status
 *
 * @return  LW_TRUE  If EOM done status matches expected EOM done status
 */
static LwBool
s_bifEomDoneStatus_GH100
(
    void *pArgs
)
{
    LwU32 regVal;
    const LwBool *pEomDoneStatus = pArgs;
    regVal = BAR0_REG_RD32(LW_UXL_LANE_REG_MISC_CTL_2(0));
    return (DRF_VAL(_UXL, _LANE_REG_MISC_CTL_2, _RX_EOM_DONE, regVal)) ==
           (*pEomDoneStatus);
}
