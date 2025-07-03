/* _LWRM_COPYRIGHT_BEGIN_
*
* Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* _LWRM_COPYRIGHT_END_
*/

/* ------------------------ System Includes -------------------------------- */
#include "soesw.h"

/* ------------------------ Application Includes --------------------------- */
#include "dev_lw_xp.h"
#include "dev_lw_xve.h"
#include "dev_soe_csb.h"
#include "soe_bar0.h"
#include "soe_objbif.h"
#include "soe_cmdmgmt.h"
#include "dev_lw_xp_addendum.h"
#include "dev_soe_csb.h"
#include "soe_objdiscovery.h"

/* ------------------------ Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
#if !defined(LW_XVE_LINK_CONTROL_STATUS_LINK_SPEED_16P0)
#define LW_XVE_LINK_CONTROL_STATUS_LINK_SPEED_16P0       0x00000004 /* R---V */
#endif

/* ------------------------ Function Prototypes ---------------------------- */
static LwBool _bifLtssmIsIdle_LR10(void)
    GCC_ATTRIB_SECTION("imem_bif", "_bifLtssmIsIdle_LR10");
static LwBool _bifIsLinkReady_LR10(void)
    GCC_ATTRIB_SECTION("imem_bif", "_bifIsLinkReady_LR10");
static LwBool _bifLtssmIsIdleAndLinkReady_LR10(void)
    GCC_ATTRIB_SECTION("imem_bif", "_bifLtssmIsIdleAndLinkReady_LR10");
static FLCN_STATUS _bifGetBusSpeed_LR10(LwU32 *pSpeed)
    GCC_ATTRIB_SECTION("imem_bif", "_bifGetBusSpeed_LR10");
static FLCN_STATUS _bifWriteNewPcieLinkSpeed_LR10(LwU32 xpPlLinkConfig0, LwU32 linkSpeed)
    GCC_ATTRIB_SECTION("imem_bif", "_bifWriteNewPcieLinkSpeed_LR10");
static FLCN_STATUS _bifSetXpPlLinkRate_LR10(LwU32 *pXpPlLinkConfig0, LwU32 linkSpeed)
    GCC_ATTRIB_SECTION("imem_bif", "_bifSetXpPlLinkRate_LR10");
static void _bifResetXpPlState_LR10(LwU32 xpPlLinkConfig0)
    GCC_ATTRIB_SECTION("imem_bif", "_bifResetXpPlState_LR10");

/*!
 * @brief Set UPHY EOM parameters to control the BER target.
 *
 * @param eomMode      Mode of EOM.
 * @param eomNblks     Number of blocks.
 * @param eomNerrs     Number of errors.
 * @param eomBerEyeSel BER eye select
 *
 * @return FLCN_OK  Return FLCN_OK once these parameters sent by MODS are set
 */
FLCN_STATUS
bifSetEomParameters_LR10
(
    LwU8  eomMode,
    LwU8  eomNblks,
    LwU8  eomNerrs,
    LwU8  eomBerEyeSel
)
{
    LwU32 tempRegVal = 0;
    LwU32 wData      = 0;
    LwU32 rData      = 0;

    //
    // UPHY_*_CTRL0 register writes
    // To not alter the other bits of LW_UPHY_DLM_AE_EOM_MEAS_CTRL0,
    // we first read the register, then change only the required field
    // (EOM Mode) and finally write the value
    //
    tempRegVal = FLD_SET_DRF(_XP, _PEX_PAD_CTL_8, _CFG_RESET, _INIT, tempRegVal);
    tempRegVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_8, _CFG_RDS, 0x1, tempRegVal);
    tempRegVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_8, _CFG_WDS, 0x0, tempRegVal);
    tempRegVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_8, _CFG_ADDR,
                                 LW_UPHY_DLM_AE_EOM_MEAS_CTRL0, tempRegVal);
    BAR0_REG_WR32(LW_XP_PEX_PAD_CTL_8, tempRegVal);

    rData = DRF_VAL(_XP, _PEX_PAD_CTL_9, _CFG_RDATA,
                    BAR0_REG_RD32(LW_XP_PEX_PAD_CTL_9));

    rData = FLD_SET_DRF_NUM(_UPHY_DLM, _AE_EOM_MEAS_CTRL0,
                          _FOM_MODE, 0x2, rData);

    tempRegVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_8, _CFG_RDS, 0x0, tempRegVal);
    tempRegVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_8, _CFG_WDS, 0x1, tempRegVal);
    tempRegVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_8,
                                 _CFG_WDATA, rData, tempRegVal);
    BAR0_REG_WR32(LW_XP_PEX_PAD_CTL_8, tempRegVal);

    //
    // UPHY_*_CTRL1 register writes
    // Since we are altering all the bits of LW_UPHY_DLM_AE_EOM_MEAS_CTRL1,
    // we directly set the values of nerrs and nblks
    //
    wData = DRF_NUM(_UPHY_DLM, _AE_EOM_MEAS_CTRL1, _BER_YH_NBLKS_MAX, eomNblks) |
            DRF_NUM(_UPHY_DLM, _AE_EOM_MEAS_CTRL1, _BER_YL_NBLKS_MAX, eomNblks) |
            DRF_NUM(_UPHY_DLM, _AE_EOM_MEAS_CTRL1, _BER_YH_NERRS_MIN, eomNerrs) |
            DRF_NUM(_UPHY_DLM, _AE_EOM_MEAS_CTRL1, _BER_YL_NERRS_MIN, eomNerrs);
    tempRegVal = 0;
    tempRegVal = FLD_SET_DRF(_XP, _PEX_PAD_CTL_8, _CFG_RESET, _INIT, tempRegVal);
    tempRegVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_8, _CFG_WDS, 0x1, tempRegVal);
    tempRegVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_8, _CFG_ADDR,
                                 LW_UPHY_DLM_AE_EOM_MEAS_CTRL1, tempRegVal);

    tempRegVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_8, _CFG_WDATA, wData,
                                 tempRegVal);

    BAR0_REG_WR32(LW_XP_PEX_PAD_CTL_8, tempRegVal);

    return FLCN_OK;
}

/*!
 * @brief Read PEX UPHY DLN registers
 *
 * Store the value on a scratch register for the driver to read.
 *
 * @param[in]   regAddress         Register whose value needs to be read
 * @param[in]   laneSelectMask     Mask of lanes to read from
 *
 * @return FLCN_OK on success
 */

FLCN_STATUS
bifGetUphyDlnCfgSpace_LR10
(
    LwU32 regAddress,
    LwU32 laneSelectMask
)
{
    LwU32 tempRegVal = 0;

    tempRegVal = REG_RD32(BAR0, LW_XP_PEX_PAD_CTL_SEL);
    tempRegVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_SEL, _LANE_SELECT,
                                 laneSelectMask, tempRegVal);
    REG_WR32(BAR0, LW_XP_PEX_PAD_CTL_SEL, tempRegVal);

    tempRegVal = 0;
    tempRegVal = FLD_SET_DRF(_XP, _PEX_PAD_CTL_8, _CFG_RESET, _INIT, tempRegVal);
    tempRegVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_8, _CFG_RDS, 0x1, tempRegVal);
    tempRegVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_8, _CFG_WDS, 0x0, tempRegVal);
    tempRegVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_8, _CFG_ADDR, regAddress,
                                 tempRegVal);
    REG_WR32(BAR0, LW_XP_PEX_PAD_CTL_8, tempRegVal);

    tempRegVal = REG_RD32(BAR0, LW_XP_PEX_PAD_CTL_9);
    tempRegVal = DRF_VAL(_XP, _PEX_PAD_CTL_9, _CFG_RDATA, tempRegVal);
    REG_WR32_STALL(CSB, LW_CSOE_MAILBOX(0), tempRegVal);

    return FLCN_OK;
}

/*!
 * @brief Check if the LTSSM is lwrrently idle
 *
 * @return  LW_TRUE if idle, LW_FALSE otherwise
 */
static LwBool
_bifLtssmIsIdle_LR10(void)
{
    LwU32 xpPlLinkConfig0;

    xpPlLinkConfig0 = REG_RD32(BAR0, LW_XP_PL_LINK_CONFIG_0(0));

    return FLD_TEST_DRF(_XP, _PL_LINK_CONFIG_0, _LTSSM_STATUS, _IDLE, xpPlLinkConfig0) &&
           FLD_TEST_DRF(_XP, _PL_LINK_CONFIG_0, _LTSSM_DIRECTIVE, _NORMAL_OPERATIONS, xpPlLinkConfig0);
}

/*!
 * @brief Check if the link is ready
 *
 * @return  LW_TRUE if ready, LW_FALSE otherwise
 */
static LwBool
_bifIsLinkReady_LR10(void)
{
    LwU32 ltssmState;

    ltssmState = REG_RD32(BAR0, LW_XP_PL_LTSSM_STATE);
    return FLD_TEST_DRF(_XP_PL, _LTSSM_STATE, _IS_LINK_READY, _TRUE, ltssmState);
}

/*!
 * @brief Check if the LTSSM is idle and has indicated
 *        the link is ready.
 *
 * @return  LW_TRUE if link is ready, LW_FALSE otherwise
 */
static LwBool
_bifLtssmIsIdleAndLinkReady_LR10(void)
{
    return (_bifLtssmIsIdle_LR10() && _bifIsLinkReady_LR10());
}


/*!
 * @brief Get the current BIF speed
 *
 * @param[out]  pSpeed  Current bus speed.
 *
 * @return      FLCN_OK on success, otherwise an error code.
 */
static FLCN_STATUS
_bifGetBusSpeed_LR10
(
    LwU32 *pSpeed
)
{
    LwU32 tempRegVal;

    tempRegVal = REG_RD32(BAR0, DEVICE_BASE(LW_PCFG) + LW_XVE_LINK_CONTROL_STATUS);
    switch (DRF_VAL(_XVE, _LINK_CONTROL_STATUS, _LINK_SPEED, tempRegVal))
    {
        case LW_XVE_LINK_CONTROL_STATUS_LINK_SPEED_2P5:
            *pSpeed = RM_SOE_BIF_LINK_SPEED_GEN1PCIE;
            break;
        case LW_XVE_LINK_CONTROL_STATUS_LINK_SPEED_5P0:
            *pSpeed = RM_SOE_BIF_LINK_SPEED_GEN2PCIE;
            break;
        case LW_XVE_LINK_CONTROL_STATUS_LINK_SPEED_8P0:
            *pSpeed = RM_SOE_BIF_LINK_SPEED_GEN3PCIE;
           break;
        case LW_XVE_LINK_CONTROL_STATUS_LINK_SPEED_16P0:
            *pSpeed = RM_SOE_BIF_LINK_SPEED_GEN4PCIE;
            break;
        default:
            *pSpeed = RM_SOE_BIF_LINK_SPEED_ILWALID;
            return FLCN_ERROR;
    }

    return FLCN_OK;
}

/*!
 * @brief Write xpPlLinkConfig0 register to set new PCIE link speed.
 *
 * @param   xpPlLinkConfig0      LW_XP_PL_LINK_CONFIG_0 register.
 * @param   linkSpeed            Expected new link speed.
 *
 * @return  FLCN_OK if the link speed was successful.
 *          FLCN_ERR_TIMEOUT if waiting for the LTSSM to declare the link ready took too long.
 *          FLCN_ERR_MORE_PROCESSING_REQUIRED if there were no errors, but the reported link speed is incorrect.
 */
static FLCN_STATUS
_bifWriteNewPcieLinkSpeed_LR10
(
    LwU32 xpPlLinkConfig0,
    LwU32 linkSpeed
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32 lwrrentLinkSpeed;

    // Trigger speed change
    REG_WR32(BAR0, LW_XP_PL_LINK_CONFIG_0(0), xpPlLinkConfig0);

    // Wait for link to go ready (i.e., for the speed change to complete)
    if (!OS_PTIMER_SPIN_WAIT_NS_COND(_bifLtssmIsIdleAndLinkReady_LR10, NULL,
                                     BIF_LTSSM_LINK_READY_TIMEOUT_NS))
    {
        // Timed out waiting for LTSSM to indicate the link is ready
        return FLCN_ERR_TIMEOUT;
    }

    // Check if we trained to target speeds.
    status = _bifGetBusSpeed_LR10(&lwrrentLinkSpeed);
    if (status != FLCN_OK)
    {
        return status;
    }

    if (lwrrentLinkSpeed != linkSpeed)
    {
        // Speed doesn't match, we'll need to try again.
        return FLCN_ERR_MORE_PROCESSING_REQUIRED;
    }

    return FLCN_OK;
}

/*!
 * @brief Sets the MAX_LINK_RATE field in xpPlLinkConfig0 according to speed.
 *
 * @param[in]   xpPlLinkConfig0     Register value to modify.
 * @param[in]   speed               Speed to set the register according to.
 *
 * @return      FLCN_OK on success, otherwise an error code.
 */
FLCN_STATUS
_bifSetXpPlLinkRate_LR10
(
    LwU32 *pXpPlLinkConfig0,
    LwU32 linkSpeed
)
{
    switch (linkSpeed)
    {
    case RM_SOE_BIF_LINK_SPEED_GEN1PCIE:
        *pXpPlLinkConfig0 = FLD_SET_DRF(_XP, _PL_LINK_CONFIG_0, _MAX_LINK_RATE, _2500_MTPS,
                                        *pXpPlLinkConfig0);
        break;
    case RM_SOE_BIF_LINK_SPEED_GEN2PCIE:
        *pXpPlLinkConfig0 = FLD_SET_DRF(_XP, _PL_LINK_CONFIG_0, _MAX_LINK_RATE, _5000_MTPS,
                                        *pXpPlLinkConfig0);
        break;
    case RM_SOE_BIF_LINK_SPEED_GEN3PCIE:
        *pXpPlLinkConfig0 = FLD_SET_DRF(_XP, _PL_LINK_CONFIG_0, _MAX_LINK_RATE, _8000_MTPS,
                                        *pXpPlLinkConfig0);
        break;
    case RM_SOE_BIF_LINK_SPEED_GEN4PCIE:
        *pXpPlLinkConfig0 = FLD_SET_DRF(_XP, _PL_LINK_CONFIG_0, _MAX_LINK_RATE, _16000_MTPS,
                                        *pXpPlLinkConfig0);
        break;
    default:
        return FLCN_ERR_ILWALID_ARGUMENT;
    }
    
    return FLCN_OK;
}

/*!
 * @brief Reset xpPlLinkConfig0 to its original state.
 *
 * @param[in]   xpPlLinkConfig0     Register value of XP_PL_LINK_CONFIG_0.
 *
 */
static void
_bifResetXpPlState_LR10
(
    LwU32 xpPlLinkConfig0
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32 lwrrentLinkSpeed;

    status = _bifGetBusSpeed_LR10(&lwrrentLinkSpeed);
    if (status != FLCN_OK)
        return;

    status = _bifSetXpPlLinkRate_LR10(&xpPlLinkConfig0, lwrrentLinkSpeed);
    if (status != FLCN_OK)
        return;
    
    REG_WR32(BAR0, LW_XP_PL_LINK_CONFIG_0(0), xpPlLinkConfig0);
}

FLCN_STATUS
bifSetPcieLinkSpeed_LR10
(
    LwU32 linkSpeed
)
{
    FLCN_STATUS status = FLCN_OK;
    FLCN_TIMESTAMP timeout;
    LwU32 savedDlMgr0;
    LwU32 tempRegVal;
    LwU32 xpPlLinkConfig0;

    // Set safe timings while we do the switch. We'll restore the register when we're done.
    savedDlMgr0 = REG_RD32(BAR0, LW_XP_DL_MGR_0(0));
    tempRegVal = FLD_SET_DRF(_XP, _DL_MGR_0, _SAFE_TIMING, _ENABLE, savedDlMgr0);
    REG_WR32(BAR0, LW_XP_DL_MGR_0(0), tempRegVal);

    // Wait for LTSSM to be in an idle state before we change the link state.
    if (!OS_PTIMER_SPIN_WAIT_NS_COND(_bifLtssmIsIdle_LR10, NULL, BIF_LTSSM_IDLE_TIMEOUT_NS))
    {
        // Timed out waiting for LTSSM to go idle
        status = FLCN_ERR_TIMEOUT;
        goto bifSetPcieLinkSpeed_Exit;
    }

    // Set MAX_LINK_RATE according to requested speed
    xpPlLinkConfig0 = REG_RD32(BAR0, LW_XP_PL_LINK_CONFIG_0(0));
    status = _bifSetXpPlLinkRate_LR10(&xpPlLinkConfig0, linkSpeed);
    if (status != FLCN_OK)
    {
        goto bifSetPcieLinkSpeed_Exit;
    }

    // Setup register to trigger speed change
    xpPlLinkConfig0 = FLD_SET_DRF(_XP, _PL_LINK_CONFIG_0, _LTSSM_DIRECTIVE, _CHANGE_SPEED,
                                  xpPlLinkConfig0);

    // Trigger a speed change and poll if the changes are complete
    osPTimerTimeNsLwrrentGet(&timeout);
    do
    {
        status = _bifWriteNewPcieLinkSpeed_LR10(xpPlLinkConfig0, linkSpeed);

       // If the change got the correct link speed, or threw an error, leave the loop.
       if (status != FLCN_ERR_MORE_PROCESSING_REQUIRED)
       {
           break;
       }

       // Check if we've hit the timeout
       if (osPTimerTimeNsElapsedGet(&timeout) >= BIF_LINK_CHANGE_TIMEOUT_NS)
       {
           // Try one last time...
           status = _bifWriteNewPcieLinkSpeed_LR10(xpPlLinkConfig0, linkSpeed);
           if (status == FLCN_ERR_MORE_PROCESSING_REQUIRED)
           {
               status = FLCN_ERR_TIMEOUT;
               break;
           }
       }
    } while (LW_TRUE);

    if (status != FLCN_OK)
    {
        _bifResetXpPlState_LR10(xpPlLinkConfig0);
    }

bifSetPcieLinkSpeed_Exit:
    // Restore original timings
    REG_WR32(BAR0, LW_XP_DL_MGR_0(0), savedDlMgr0);
    return status;
}

/*!
 * @brief       Get PCIe Speed and Width
 *
 * @param[out]  pSpeed   Pointer to returned PCIe link speed value
 * @param[out]  pWidth   Pointer to returned PCIe link width value
 *
 * @return      FLCN_OK on success, otherwise an error code.
 */
FLCN_STATUS
bifGetPcieSpeedWidth_LR10
(
    LwU32 *pSpeed,
    LwU32 *pWidth
)
{
    LwU32 tempRegVal;

    // Get PCIe Speed
    tempRegVal = REG_RD32(BAR0, DEVICE_BASE(LW_PCFG) + LW_XVE_LINK_CONTROL_STATUS);
    switch (DRF_VAL(_XVE, _LINK_CONTROL_STATUS, _LINK_SPEED, tempRegVal))
    {
        case LW_XVE_LINK_CONTROL_STATUS_LINK_SPEED_2P5:
            *pSpeed = RM_SOE_BIF_LINK_SPEED_GEN1PCIE;
            break;
        case LW_XVE_LINK_CONTROL_STATUS_LINK_SPEED_5P0:
            *pSpeed = RM_SOE_BIF_LINK_SPEED_GEN2PCIE;
            break;
        case LW_XVE_LINK_CONTROL_STATUS_LINK_SPEED_8P0:
            *pSpeed = RM_SOE_BIF_LINK_SPEED_GEN3PCIE;
           break;
        case LW_XVE_LINK_CONTROL_STATUS_LINK_SPEED_16P0:
            *pSpeed = RM_SOE_BIF_LINK_SPEED_GEN4PCIE;
            break;
        default:
            *pSpeed = RM_SOE_BIF_LINK_SPEED_ILWALID;
            return FLCN_ERROR;
    }

    // Get PCIe Width
    switch (DRF_VAL(_XVE, _LINK_CONTROL_STATUS, _NEGOTIATED_LINK_WIDTH, tempRegVal))
    {
        case LW_XVE_LINK_CONTROL_STATUS_NEGOTIATED_LINK_WIDTH_X1:
        {
            *pWidth = RM_SOE_BIF_LINK_WIDTH_X1;
            break;
        }
        case LW_XVE_LINK_CONTROL_STATUS_NEGOTIATED_LINK_WIDTH_X2:
        {
            *pWidth = RM_SOE_BIF_LINK_WIDTH_X2;
            break;
        }
        case LW_XVE_LINK_CONTROL_STATUS_NEGOTIATED_LINK_WIDTH_X4:
        {
            *pWidth = RM_SOE_BIF_LINK_WIDTH_X4;
            break;
        }
        case LW_XVE_LINK_CONTROL_STATUS_NEGOTIATED_LINK_WIDTH_X8:
        {
            *pWidth = RM_SOE_BIF_LINK_WIDTH_X8;
            break;
        }
        case LW_XVE_LINK_CONTROL_STATUS_NEGOTIATED_LINK_WIDTH_X16:
        {
            *pWidth = RM_SOE_BIF_LINK_WIDTH_X16;
            break;
        }
        default:
        {
            *pWidth = RM_SOE_BIF_LINK_WIDTH_ILWALID;
            return FLCN_ERROR;
        }
    }

    return FLCN_OK;
}

/*!
 * @brief       Get PCIe fatal, non-fatal, unsupported req error counts
 *
 * @param[out]  pFatalCount       Pointer to PCIe link fatal error count
 * @param[out]  pNonFatalCount    Pointer to PCIe link non-fatal error count
 * @param[out]  pUnsuppReqCount   Pointer to PCIe link unsupported req error count
 */
void
bifGetPcieFatalNonFatalUnsuppReqCount_LR10
(
    LwU32 *pFatalCount,
    LwU32 *pNonFatalCount,
    LwU32 *pUnsuppReqCount
)
{
    LwU32 reg = 0;

    reg = REG_RD32(BAR0, DEVICE_BASE(LW_PCFG) + LW_XVE_ERROR_COUNTER);

    *pNonFatalCount = DRF_VAL(_XVE, _ERROR_COUNTER,
                              _NON_FATAL_ERROR_COUNT_VALUE, reg);

    *pFatalCount = DRF_VAL(_XVE, _ERROR_COUNTER,
                           _FATAL_ERROR_COUNT_VALUE, reg);

    *pUnsuppReqCount = DRF_VAL(_XVE, _ERROR_COUNTER,
                               _UNSUPP_REQ_COUNT_VALUE, reg);
}

/*!
 * @brief       Get PCIe link correctable error count
 *
 * @param[out]  pCorrErrCount  Pointer to returned PCIe link
 *                             correctable error count
 */
void
bifGetPcieCorrectableErrorCount_LR10
(
    LwU32 *pCorrErrCount
)
{
    LwU32 reg = 0;

    reg = REG_RD32(BAR0, DEVICE_BASE(LW_PCFG) + LW_XVE_ERROR_COUNTER1);

    *pCorrErrCount = DRF_VAL(_XVE, _ERROR_COUNTER1,
                             _CORR_ERROR_COUNT_VALUE, reg);
}

/*!
 * @brief       Get PCIe link L0 to recovery count
 *
 * @param[out]  pL0ToRecCount Pointer to returned PCIe link
 *                            L0 to recovery count
 */
void
bifGetPcieL0ToRecoveryCount_LR10
(
    LwU32 *pL0ToRecCount
)
{
    LwU32 reg = 0;

    reg = REG_RD32(BAR0, LW_XP_L0_TO_RECOVERY_COUNT(0));

    *pL0ToRecCount = DRF_VAL(_XP, _L0_TO_RECOVERY_COUNT, _VALUE, reg);
}

/*!
 * @brief       Get PCIe link replay count
 *
 * @param[out]  pReplayCount  Pointer to returned PCIe link
 *                            replay count
 */
void
bifGetPcieReplayCount_LR10
(
    LwU32 *pReplayCount
)
{
    LwU32 reg = 0;

    reg = REG_RD32(BAR0, LW_XP_REPLAY_COUNT(0));

    *pReplayCount = DRF_VAL(_XP, _REPLAY_COUNT, _VALUE, reg);
}

/*!
 * @brief       Get PCIe link replay rollover count
 *
 * @param[out]  pReplayRolloverCount    Pointer to returned PCIe link
 *                                      replay rollover count
 */
void
bifGetPcieReplayRolloverCount_LR10
(
    LwU32 *pReplayRolloverCount
)
{
    LwU32 reg = 0;

    reg = REG_RD32(BAR0, LW_XP_REPLAY_COUNT(0));

    *pReplayRolloverCount = DRF_VAL(_XP, _REPLAY_ROLLOVER_COUNT, _VALUE, reg);
}

/*!
 * @brief       Get PCIe link NAKS received count
 *
 * @param[out]  pNaksRcvdCount  Pointer to returned PCIe link
 *                              NAKS received count
 */
void
bifGetPcieNaksRcvdCount_LR10
(
    LwU32 *pNaksRcvdCount
)
{
    LwU32 reg = 0;

    reg = REG_RD32(BAR0, LW_XP_NAKS_RCVD_COUNT(0));

    *pNaksRcvdCount = DRF_VAL(_XP, _NAKS_RCVD_COUNT, _VALUE, reg);
}

/*!
 * @brief       Get PCIe link NAKS sent count
 *
 * @param[out]  pNaksSentCount  Pointer to returned PCIe link
 *                              NAKS sent count
 */
void
bifGetPcieNaksSentCount_LR10
(
    LwU32 *pNaksSentCount
)
{
    LwU32 reg = 0;

    reg = REG_RD32(BAR0, LW_XP_NAKS_SENT_COUNT(0));

    *pNaksSentCount = DRF_VAL(_XP, _NAKS_SENT_COUNT, _VALUE, reg);
}

void
bifGetPcieConfigSpaceIds_LR10
(
    LwU16 *pVendorId,
    LwU16 *pDevId,
    LwU16 *pSubVendorId,
    LwU16 *pSubId
)
{
    LwU32 pciId;

    pciId = REG_RD32(BAR0, DEVICE_BASE(LW_PCFG) + LW_XVE_ID);
    *pVendorId = (LwU16)DRF_VAL(_XVE, _ID, _VENDOR, pciId);
    *pDevId = (LwU16)DRF_VAL(_XVE, _ID, _DEVICE_CHIP, pciId);

    pciId = REG_RD32(BAR0, DEVICE_BASE(LW_PCFG) + LW_XVE_SUBSYSTEM);
    *pSubVendorId = (LwU16)DRF_VAL(_XVE, _ID, _VENDOR, pciId);
    *pSubId = (LwU16)DRF_VAL(_XVE, _ID, _DEVICE_CHIP, pciId);
}

void
bifSetupProdValues_LR10(void)
{
    LwU32 baseAddress;
    LwU32 data32;

    //
    // Setup PRODs for XP/XVE
    //
    baseAddress = getEngineBaseAddress(LW_DISCOVERY_ENGINE_NAME_XVE, 0, ADDRESS_UNICAST, 0);
    data32 = REG_RD32(BAR0, baseAddress + LW_XVE_PRIV_INTR_EN);
    data32 = FLD_SET_DRF(_XVE, _PRIV_INTR_EN, _CPL_TIMEOUT, __PROD, data32);
    REG_WR32(BAR0, baseAddress + LW_XVE_PRIV_INTR_EN, data32);

    data32 = REG_RD32(BAR0, LW_XP_DL_MGR_0(0));
    data32 = FLD_SET_DRF(_XP, _DL_MGR_0, _SAFE_TIMING, __PROD, data32);
    REG_WR32(BAR0, LW_XP_DL_MGR_0(0), data32);

    data32 = REG_RD32(BAR0, LW_XP_DL_CYA_0(0));
    data32 = FLD_SET_DRF(_XP, _DL_CYA_0, _RETIMING_STAGES, __PROD, data32);
    REG_WR32(BAR0, LW_XP_DL_CYA_0(0), data32);
}

/*
 * @brief Run EOM sequence and fill the output buffer with EOM status values
 *
 * @Note: This sequence is same as sequence in TuringPcie::DoGetEomStatus in MODS
 *
 * @param[in]   eomMode     Mode on EOM
 * @param[in]   eomNblks    Number of blocks
 * @param[in]   eomNerrs    Number of errors
 * @param[in]   laneMask    Lanemask specifying which lanes needs to be selected
 *                          for running EOM sequence
 * @param[in]   dmaHandle   DMA handle of the output buffer
 * @return FLCN_OK  If EOM sequence is run correctly
 */
FLCN_STATUS
bifGetEomStatus_LR10
(
    LwU8   eomMode,
    LwU8   eomNblks,
    LwU8   eomNerrs,
    LwU8   eomBerEyeSel,
    LwU32  laneMask,
    RM_FLCN_U64 dmaHandle
)
{
    LwU8 laneNum;
    LwU32 regVal;
    LwBool bEomDoneStatus;
    LwU8 numRequestedLanes = 0U;
    LwU8 firstLaneFromLsb = LW_U8_MAX;
    FLCN_STATUS status = FLCN_OK;
    LwU16  eomStatus[BIF_MAX_PCIE_LANES];

    if (laneMask == 0U)
    {
        // Nothing to do
        goto bifGetEomStatus_LR10_exit;
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
        SOE_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto bifGetEomStatus_LR10_exit;
    }

    // Set EOM params for requested lanes
    bifDoLaneSelect_HAL(&Bif, laneMask);
    bifSetEomParameters_HAL(&Bif, eomMode, eomNblks, eomNerrs, eomBerEyeSel);

    // Disable EOM for requested lanes.
    bifDoLaneSelect_HAL(&Bif, laneMask);
    regVal = REG_RD32(BAR0, LW_XP_PEX_PAD_CTL_5);
    regVal = FLD_SET_DRF(_XP, _PEX_PAD_CTL_5, _RX_EOM_EN,
                         _DISABLE, regVal);
    REG_WR32(BAR0, LW_XP_PEX_PAD_CTL_5, regVal);

    // Poll for EOM done status to be FALSE for all requested lanes
    bEomDoneStatus = LW_FALSE;
    status = bifPollEomDoneStatus_HAL(&Bif, numRequestedLanes, firstLaneFromLsb,
                bEomDoneStatus, LW_UPHY_EOM_DONE_FALSE_TIMEOUT_NS);
    if (status != FLCN_OK)
    {
        goto bifGetEomStatus_LR10_exit;
    }

    // Enable EOM for requested lanes.
    bifDoLaneSelect_HAL(&Bif, laneMask);
    regVal = REG_RD32(BAR0, LW_XP_PEX_PAD_CTL_5);
    regVal = FLD_SET_DRF(_XP, _PEX_PAD_CTL_5, _RX_EOM_OVRD, _ENABLE, regVal);
    regVal = FLD_SET_DRF(_XP, _PEX_PAD_CTL_5, _RX_EOM_EN, _ENABLE, regVal);
    REG_WR32(BAR0, LW_XP_PEX_PAD_CTL_5, regVal);

    bEomDoneStatus = LW_TRUE;
    status = bifPollEomDoneStatus_HAL(&Bif, numRequestedLanes, firstLaneFromLsb,
                bEomDoneStatus, LW_UPHY_EOM_DONE_TRUE_TIMEOUT_NS);
    if (status != FLCN_OK)
    {
        goto bifGetEomStatus_LR10_exit;
    }

    // Disable EOM for requested lanes
    bifDoLaneSelect_HAL(&Bif, laneMask);
    regVal = REG_RD32(BAR0, LW_XP_PEX_PAD_CTL_5);
    regVal = FLD_SET_DRF(_XP, _PEX_PAD_CTL_5, _RX_EOM_EN, _DISABLE, regVal);
    regVal = FLD_SET_DRF(_XP, _PEX_PAD_CTL_5, _RX_EOM_OVRD, _DISABLE, regVal);
    REG_WR32(BAR0, LW_XP_PEX_PAD_CTL_5, regVal);

    // Fill up EOM status array starting from index 0 always.
    for (laneNum = firstLaneFromLsb;
         laneNum < (firstLaneFromLsb + numRequestedLanes); laneNum++)
    {
        bifDoLaneSelect_HAL(&Bif, BIT(laneNum));
        eomStatus[laneNum] = DRF_VAL(_XP, _PEX_PAD_CTL_5, _RX_EOM_STATUS,
                                      REG_RD32(BAR0, LW_XP_PEX_PAD_CTL_5));
    }

    if (FLCN_OK != soeDmaWriteToSysmem_HAL(&Soe,
                                           (void*)&eomStatus,
                                           dmaHandle,
                                           0,
                                           sizeof(eomStatus)))
    {
        SOE_HALT();
    }

bifGetEomStatus_LR10_exit:
    return status;
}

/*!
 * @brief Select requsted PCIe lanes.
 *
 * @param[in] laneMask  Bit mask representing requested lanes. If 0th lane needs
 *                      to be selected, 0th bit should be set in the laneMask.
 *                      If 0th and 1st lane need to be selected, 0th and 1st
 *                      bits need to be set and so on.
 *
 * @return FLCN_OK
 */
FLCN_STATUS
bifDoLaneSelect_LR10
(
    LwU32 laneMask
)
{
    LwU32 regVal;
    regVal = REG_RD32(BAR0, LW_XP_PEX_PAD_CTL_SEL);
    regVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_SEL, _LANE_SELECT,
                             laneMask, regVal);
    REG_WR32(BAR0, LW_XP_PEX_PAD_CTL_SEL, regVal);
    return FLCN_OK;
}

/*!
 * @brief OS_PTIMER_COND_FUNC to check if EOM done status is set as expected
 *
 * @param[in] pArgs  Pointer to boolean indicating expected EOM done status
 *
 * @return  LW_TRUE  If EOM done status matches expected EOM done status
 */
static LwBool
_bifEomDoneStatus_LR10
(
    void *pArgs
)
{
    LwU32 regVal;
    const LwBool *pEomDoneStatus = pArgs;
    regVal = REG_RD32(BAR0, LW_XP_PEX_PAD_CTL_5);
    return (DRF_VAL(_XP, _PEX_PAD_CTL_5, _RX_EOM_DONE, regVal)) ==
           (*pEomDoneStatus);
}

/*!
 * @brief Poll EOM done status for requested lanes
 *
 * @param[in] numRequestedLanes  Number of requested lanes for which EOM done
 *                               status needs to be polled
 * @param[in]  firstLaneFromLsb  First lane number from LSB for which EOM done
 *                               status needs to be polled
 * @param[in]  bEomDoneStatus    Poll for EOM done status to be set to this value
 * @param[in]  timeoutNs         Timeout in nanoseconds after which this should
 *                               break out of polling loop
 *
 * @return FLCN_OK  If all requested lanes have expected EOM status
 */
FLCN_STATUS
bifPollEomDoneStatus_LR10
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

    bIsFmodel = FLD_TEST_DRF(_PSMC, _BOOT_2, _FMODEL, _YES,
                             REG_RD32(BAR0, LW_PSMC_BOOT_2));

    // Poll for EOM done status to be set to FALSE
    for (laneNum = 0; laneNum < numRequestedLanes; laneNum++)
    {
        // Select one lane at a time here
        bifDoLaneSelect_HAL(&Bif, BIT(firstLaneFromLsb + laneNum));

        // Skip following polling on FMODEL
        if (!bIsFmodel)
        {
            if (!OS_PTIMER_SPIN_WAIT_NS_COND(_bifEomDoneStatus_LR10,
                    &bEomDoneStatus, timeoutNs))
            {
                //
                // TODO: Use mailbox register that would be used across
                // the PEX SW in PMU for indicating failure points
                //
                #if (PMU_PROFILE_LR10_RISCV)
                {
                    dbgPrintf(LEVEL_ERROR,
                              "Timed out waiting for EOM status to be set to %x\n",
                              bEomDoneStatus);
                }
                #endif
                SOE_BREAKPOINT();
                status = FLCN_ERR_TIMEOUT;
                goto bifPollEomDoneStatus_LR10_exit;
            }
        }
    }

bifPollEomDoneStatus_LR10_exit:
    return status;
}
