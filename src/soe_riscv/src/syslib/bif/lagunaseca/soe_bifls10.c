/* _LWRM_COPYRIGHT_BEGIN_
*
* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* _LWRM_COPYRIGHT_END_
*/

/* ------------------------ System Includes -------------------------------- */
#include "soesw.h"

/* ------------------------ Application Includes --------------------------- */
#include "dev_lw_xpl.h"
#include "dev_xtl_ep_pri.h"
#include "dev_soe_csb.h"
#include "dev_master.h"
#include "soe_bar0.h"
#include "soe_objbif.h"
#include "cmdmgmt.h"
#include "dev_lw_uxl_pri_ctx.h"
#include "dev_lw_uphy_dlm_addendum.h"
#include "dev_xtl_ep_pcfg_gpu.h"
#include "dev_soe_csb.h"
#include "ptop_discovery_ip.h"
#include "soe_objdiscovery.h"
#include "soe_bar0.h"
#include "soe_objsoe.h"

/* ------------------------ Static Variables ------------------------------- */
/* ------------------------- Macros and Defines -----------------------------*/
/* ------------------------ Function Prototypes ---------------------------- */
sysSYSLIB_CODE static LwBool _bifLtssmIsIdle_LS10(void *pArgs);
sysSYSLIB_CODE static LwBool _bifIsLinkReady_LS10(void *pArgs);
sysSYSLIB_CODE static LwBool _bifLtssmIsIdleAndLinkReady_LS10(void *pArgs);
sysSYSLIB_CODE static FLCN_STATUS _bifGetBusSpeed_LS10(LwU32 *pSpeed);
sysSYSLIB_CODE static FLCN_STATUS _bifWriteNewPcieLinkSpeed_LS10(LwU32 xpPlLinkConfig0, LwU32 linkSpeed);
sysSYSLIB_CODE static FLCN_STATUS _bifSetXpPlLinkRate_LS10(LwU32 *pXpPlLinkConfig0, LwU32 linkSpeed);
sysSYSLIB_CODE static void _bifResetXpPlState_LS10(LwU32 xpPlLinkConfig0);

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

sysSYSLIB_CODE FLCN_STATUS
bifGetUphyDlnCfgSpace_LS10
(
    LwU32 regAddress,
    LwU32 laneSelectMask
)
{
    LwU32 tempRegVal = 0;

    // Select the desired lane(s) in PHY
    tempRegVal = BAR0_REG_RD32(LW_UXL_LANE_COMMON_MCAST_CTL);
    tempRegVal = FLD_SET_DRF_NUM(_UXL, _LANE_COMMON_MCAST_CTL, _LANE_MASK,
                                 laneSelectMask, tempRegVal);
    BAR0_REG_WR32(LW_UXL_LANE_COMMON_MCAST_CTL, tempRegVal);

    tempRegVal = 0U;
    tempRegVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_RDS,
                                 0x1U, tempRegVal);
    tempRegVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_WDS,
                                 0x0U, tempRegVal);
    tempRegVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_ADDR,
                                 regAddress, tempRegVal);
    BAR0_REG_WR32(LW_UXL_LANE_REG_DIRECT_CTL_1(0), tempRegVal);

    tempRegVal = DRF_VAL(_UXL, _LANE_REG_DIRECT_CTL_2, _CFG_RDATA,
                    BAR0_REG_RD32(LW_UXL_LANE_REG_DIRECT_CTL_2(0)));
    csbWrite(LW_CSOE_MAILBOX(0), tempRegVal);

    return FLCN_OK;
}

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
sysSYSLIB_CODE FLCN_STATUS
bifSetEomParameters_LS10
(
    LwU8  eomMode,
    LwU8  eomNblks,
    LwU8  eomNerrs,
    LwU8  eomBerEyeSel
)
{
    LwU8  laneNum;
    LwU32 tempRegVal = 0;
    LwU32 rData      = 0;
    LwU32 laneMask   = 0x3;   
    LwU8  numRequestedLanes = 0U;
    LwU8  firstLaneFromLsb  = LW_U8_MAX;

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

    for (laneNum = 0; laneNum < numRequestedLanes; laneNum++)
    {
        // Select one lane at a time here
        bifDoLaneSelect_HAL(&Bif, BIT(firstLaneFromLsb + laneNum));

        // Update EOM_FOM Mode
        // To not alter the other bits of LW_UPHY_DLM_AE_EOM_MEAS_MODE_CTRL,
        // we first read the register, then change only the required field
        // (EOM Mode) and finally write the value
        //

        tempRegVal = 0U;
        tempRegVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_RDS, 0x1, tempRegVal);
        tempRegVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_WDS, 0x0, tempRegVal);
        tempRegVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_ADDR,
                                    LW_UPHY_DLM_AE_EOM_MEAS_MODE_CTRL, tempRegVal);
        BAR0_REG_WR32(LW_UXL_LANE_REG_DIRECT_CTL_1(0), tempRegVal);

        rData = DRF_VAL(_UXL, _LANE_REG_DIRECT_CTL_2, _CFG_RDATA,
                        BAR0_REG_RD32(LW_UXL_LANE_REG_DIRECT_CTL_2(0)));

        rData = FLD_SET_DRF_NUM(_UPHY_DLM, _AE_EOM_MEAS_MODE_CTRL, _FOM_MODE, eomMode, rData);

        tempRegVal = 0U;
        tempRegVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_RDS, 0x0, tempRegVal);
        tempRegVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_WDS, 0x1, tempRegVal);
        tempRegVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_WDATA, rData, tempRegVal);
        BAR0_REG_WR32(LW_UXL_LANE_REG_DIRECT_CTL_1(0), tempRegVal);

        // Update EOM_NBLKS and EOM_NERRS
        // To not alter the other bits of LW_UPHY_DLM_AE_EOM_MEAS_BER_CTRL,
        // we first read the register, then change only the required field
        // (EOM Mode) and finally write the value
        //

        tempRegVal = 0U;
        tempRegVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_RDS, 0x1, tempRegVal);
        tempRegVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_WDS, 0x0, tempRegVal);
        tempRegVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_ADDR,
                                    LW_UPHY_DLM_AE_EOM_MEAS_BER_CTRL, tempRegVal);
        BAR0_REG_WR32(LW_UXL_LANE_REG_DIRECT_CTL_1(0), tempRegVal);

        rData = DRF_VAL(_UXL, _LANE_REG_DIRECT_CTL_2, _CFG_RDATA,
                        BAR0_REG_RD32(LW_UXL_LANE_REG_DIRECT_CTL_2(0)));
        rData = FLD_SET_DRF_NUM(_UPHY_DLM, _AE_EOM_MEAS_BER_CTRL, _NBLKS_MAX, eomNblks, rData);  
        rData = FLD_SET_DRF_NUM(_UPHY_DLM, _AE_EOM_MEAS_BER_CTRL, _NERRS_MIN, eomNerrs, rData);

        tempRegVal = 0U;
        tempRegVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_RDS, 0x0, tempRegVal);
        tempRegVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_WDS, 0x1, tempRegVal);
        tempRegVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_WDATA, rData, tempRegVal);
        BAR0_REG_WR32(LW_UXL_LANE_REG_DIRECT_CTL_1(0), tempRegVal);

        // Update BER_EYE_SEL and PAM_EYE_SEL
        // To not alter the other bits of LW_UPHY_DLM_AE_EOM_MEAS_EYE_CTRL,
        // we first read the register, then change only the required field
        // (EOM Mode) and finally write the value
        //

        tempRegVal = 0U;
        tempRegVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_RDS, 0x1, tempRegVal);
        tempRegVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_WDS, 0x0, tempRegVal);
        tempRegVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_ADDR,
                                    LW_UPHY_DLM_AE_EOM_MEAS_EYE_CTRL, tempRegVal);
        BAR0_REG_WR32(LW_UXL_LANE_REG_DIRECT_CTL_1(0), tempRegVal);

        rData = DRF_VAL(_UXL, _LANE_REG_DIRECT_CTL_2, _CFG_RDATA,
                        BAR0_REG_RD32(LW_UXL_LANE_REG_DIRECT_CTL_2(0)));
        rData = FLD_SET_DRF(_UPHY_DLM, _AE_EOM_MEAS_EYE_CTRL, _PAM_EYE_SEL, _DEFAULT, rData);
        rData = FLD_SET_DRF_NUM(_UPHY_DLM, _AE_EOM_MEAS_EYE_CTRL, _BER_EYE_SEL, eomBerEyeSel, rData);

        tempRegVal = 0U;
        tempRegVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_RDS, 0x0, tempRegVal);
        tempRegVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_WDS, 0x1, tempRegVal);
        tempRegVal = FLD_SET_DRF_NUM(_UXL, _LANE_REG_DIRECT_CTL_1, _CFG_WDATA, rData, tempRegVal);
        BAR0_REG_WR32(LW_UXL_LANE_REG_DIRECT_CTL_1(0), tempRegVal);
    }

    return FLCN_OK;
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
sysSYSLIB_CODE FLCN_STATUS
bifDoLaneSelect_LS10
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
 * @brief OS_PTIMER_COND_FUNC to check if EOM done status is set as expected
 *
 * @param[in] pArgs  Pointer to boolean indicating expected EOM done status
 *
 * @return  LW_TRUE  If EOM done status matches expected EOM done status
 */
sysSYSLIB_CODE static LwBool
_bifEomDoneStatus_LS10
(
    void *pArgs
)
{
    LwU32 regVal;
    const LwBool *pEomDoneStatus = pArgs;
    regVal = BAR0_REG_RD32(LW_UXL_LANE_REG_MISC_CTL_2(0));
    return (DRF_VAL(_UXL, _LANE_REG_MISC_CTL_2, _RX_EOM_DONE, regVal)) == (*pEomDoneStatus);
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
sysSYSLIB_CODE FLCN_STATUS
bifPollEomDoneStatus_LS10
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
    bIsFmodel = FLD_TEST_DRF(_PMC, _BOOT_2, _FMODEL, _YES,
        REG_RD32(BAR0, LW_PMC_BOOT_2));

    // Poll for EOM done status to be set to FALSE
    for (laneNum = 0; laneNum < numRequestedLanes; laneNum++)
    {
        // Select one lane at a time here
        bifDoLaneSelect_HAL(&Bif, BIT(firstLaneFromLsb + laneNum));

        // Skip following polling on FMODEL
        if (!bIsFmodel)
        {
            if (!OS_PTIMER_SPIN_WAIT_NS_COND(_bifEomDoneStatus_LS10,
                                             &bEomDoneStatus, timeoutNs))
            {
                //
                // TODO: Use mailbox register that would be used across
                // the PEX SW in PMU for indicating failure points
                //
                #if (PMU_PROFILE_LS10_RISCV)
                {
                    dbgPrintf(LEVEL_ERROR,
                              "Timed out waiting for EOM status to be set to %x\n",
                              bEomDoneStatus);
                }
                #endif
                SOE_BREAKPOINT();
                status = FLCN_ERR_TIMEOUT;
                goto bifPollEomDoneStatus_LS10_exit;
            }
        }
    }

bifPollEomDoneStatus_LS10_exit:
    return status;    
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
 * @param[in]   laneMask     Lanemask specifying which lanes needs to be
 *                           selected for running EOM sequence
 * @params[out] pEomStatus   Pointer to output buffer
 *
 * @return FLCN_OK  If EOM sequence is run correctly
 */
sysSYSLIB_CODE FLCN_STATUS
bifGetEomStatus_LS10
(
    LwU8  eomMode,
    LwU8  eomNblks,
    LwU8  eomNerrs,
    LwU8  eomBerEyeSel,
    LwU32 laneMask,
    RM_FLCN_U64 dmaHandle
)
{
    LwU8        laneNum;
    LwU32       regVal;
    LwBool      bEomDoneStatus;
    LwU8        numRequestedLanes = 0U;
    LwU8        firstLaneFromLsb  = LW_U8_MAX;
    FLCN_STATUS status            = FLCN_OK;
    LwU16  eomStatus[BIF_MAX_PCIE_LANES];

    if (laneMask == 0U)
    {
        // Nothing to do
        goto bifGetEomStatus_LS10_exit;
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
        goto bifGetEomStatus_LS10_exit;
    }

    // Set EOM params for requested lanes
    bifDoLaneSelect_HAL(&Bif, laneMask);
    bifSetEomParameters_HAL(&Bif, eomMode, eomNblks, eomNerrs, eomBerEyeSel);

    // Disable EOM for requested lanes
    for (laneNum = 0; laneNum < numRequestedLanes; laneNum++)
    {
        // Select one lane at a time here
        bifDoLaneSelect_HAL(&Bif, BIT(firstLaneFromLsb + laneNum));
        regVal = BAR0_REG_RD32(LW_UXL_LANE_REG_MISC_CTL_2(0));
        regVal = FLD_SET_DRF(_UXL, _LANE_REG_MISC_CTL_2, _RX_EOM_EN, _DEASSERTED,
                            regVal);
        regVal = FLD_SET_DRF(_UXL, _LANE_REG_MISC_CTL_2, _RX_EOM_OVRD, _DEASSERTED,
                            regVal);
        BAR0_REG_WR32(LW_UXL_LANE_REG_MISC_CTL_2(0), regVal);
    }

    // Poll for EOM done status to be FALSE for all requested lanes
    bEomDoneStatus = LW_FALSE;
    status = bifPollEomDoneStatus_HAL(&Bif, numRequestedLanes, firstLaneFromLsb,
                bEomDoneStatus, LW_UPHY_EOM_DONE_FALSE_TIMEOUT_NS);
    if (status != FLCN_OK)
    {
        goto bifGetEomStatus_LS10_exit;
    }

    // Enable EOM for requested lanes.
    for (laneNum = 0; laneNum < numRequestedLanes; laneNum++)
    {
        // Select one lane at a time here
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
        goto bifGetEomStatus_LS10_exit;
    }

    // Disable EOM for requested lanes
    for (laneNum = 0; laneNum < numRequestedLanes; laneNum++)
    {
        // Select one lane at a time here
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
        eomStatus[laneNum] = DRF_VAL(_UXL, _LANE_REG_MISC_CTL_2, _RX_EOM_STATUS,
                                      BAR0_REG_RD32(LW_UXL_LANE_REG_MISC_CTL_2(0)));
    }

    if (FLCN_OK != soeDmaWriteToSysmem_HAL(&Soe,
                                           (void*)&eomStatus,
                                           dmaHandle,
                                           0,
                                           sizeof(eomStatus)))
    {
        SOE_HALT();
    }

bifGetEomStatus_LS10_exit:
    return status;
}

sysSYSLIB_CODE FLCN_STATUS
bifSetPcieLinkSpeed_LS10
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
    savedDlMgr0 = BAR0_REG_RD32(LW_PTOP_UNICAST_SW_DEVICE_BASE_XPL_0 + LW_XPL_DL_MGR_CTRL0);
    tempRegVal = FLD_SET_DRF(_XPL, _DL_MGR_CTRL0, _SAFE_TIMING, _ENABLE, savedDlMgr0);
    REG_WR32(BAR0, LW_PTOP_UNICAST_SW_DEVICE_BASE_XPL_0 + LW_XPL_DL_MGR_CTRL0, tempRegVal);

    // Wait for LTSSM to be in an idle state before we change the link state.
    if (!OS_PTIMER_SPIN_WAIT_NS_COND(_bifLtssmIsIdle_LS10, NULL, BIF_LTSSM_IDLE_TIMEOUT_NS))
    {
        // Timed out waiting for LTSSM to go idle
        status = FLCN_ERR_TIMEOUT;
        goto bifSetPcieLinkSpeed_Exit;
    }

    // Set MAX_LINK_RATE according to requested speed
    xpPlLinkConfig0 = REG_RD32(BAR0, LW_PTOP_UNICAST_SW_DEVICE_BASE_XPL_0 + LW_XPL_PL_LTSSM_LINK_CONFIG_0);
    status = _bifSetXpPlLinkRate_LS10(&xpPlLinkConfig0, linkSpeed);
    if (status != FLCN_OK)
    {
        goto bifSetPcieLinkSpeed_Exit;
    }

    // Setup register to trigger speed change
    xpPlLinkConfig0 = FLD_SET_DRF(_XPL, _PL_LTSSM_LINK_CONFIG_0, _LTSSM_DIRECTIVE, _CHANGE_SPEED,
                                  xpPlLinkConfig0);

    // Trigger a speed change and poll if the changes are complete
    osPTimerTimeNsLwrrentGet(&timeout);
    do
    {
        status = _bifWriteNewPcieLinkSpeed_LS10(xpPlLinkConfig0, linkSpeed);

       // If the change got the correct link speed, or threw an error, leave the loop.
       if (status != FLCN_ERR_MORE_PROCESSING_REQUIRED)
       {
           break;
       }

       // Check if we've hit the timeout
       if (osPTimerTimeNsElapsedGet(&timeout) >= BIF_LINK_CHANGE_TIMEOUT_NS)
       {
           // Try one last time...
           status = _bifWriteNewPcieLinkSpeed_LS10(xpPlLinkConfig0, linkSpeed);
           if (status == FLCN_ERR_MORE_PROCESSING_REQUIRED)
           {
               status = FLCN_ERR_TIMEOUT;
               break;
           }
       }
    } while (LW_TRUE);

    if (status != FLCN_OK)
    {
        _bifResetXpPlState_LS10(xpPlLinkConfig0);
    }

bifSetPcieLinkSpeed_Exit:
    // Restore original timings
    REG_WR32(BAR0, LW_PTOP_UNICAST_SW_DEVICE_BASE_XPL_0 + LW_XPL_DL_MGR_CTRL0, savedDlMgr0);
    return status;
}

/*!
 * @brief Check if the LTSSM is lwrrently idle
 *
 * @return  LW_TRUE if idle, LW_FALSE otherwise
 */
sysSYSLIB_CODE static LwBool
_bifLtssmIsIdle_LS10(void *pArgs)
{
    LwU32 xpPlLinkConfig0;

    xpPlLinkConfig0 = REG_RD32(BAR0, LW_PTOP_UNICAST_SW_DEVICE_BASE_XPL_0 + LW_XPL_PL_LTSSM_LINK_CONFIG_0);

    return FLD_TEST_DRF(_XPL, _PL_LTSSM_LINK_CONFIG_0, _LTSSM_STATUS, _IDLE, xpPlLinkConfig0) &&
           FLD_TEST_DRF(_XPL, _PL_LTSSM_LINK_CONFIG_0, _LTSSM_DIRECTIVE, _NORMAL_OPERATIONS, xpPlLinkConfig0);
}

/*!
 * @brief Sets the MAX_LINK_RATE field in xpPlLinkConfig0 according to speed.
 *
 * @param[in]   xpPlLinkConfig0     Register value to modify.
 * @param[in]   speed               Speed to set the register according to.
 *
 * @return      FLCN_OK on success, otherwise an error code.
 */
sysSYSLIB_CODE FLCN_STATUS
_bifSetXpPlLinkRate_LS10
(
    LwU32 *pXpPlLinkConfig0,
    LwU32 linkSpeed
)
{
    switch (linkSpeed)
    {
    case RM_SOE_BIF_LINK_SPEED_GEN1PCIE:
        *pXpPlLinkConfig0 = FLD_SET_DRF(_XPL, _PL_LTSSM_LINK_CONFIG_0, _TARGET_LINK_SPEED, _2500_MTPS,
                                        *pXpPlLinkConfig0);
        break;
    case RM_SOE_BIF_LINK_SPEED_GEN2PCIE:
        *pXpPlLinkConfig0 = FLD_SET_DRF(_XPL, _PL_LTSSM_LINK_CONFIG_0, _TARGET_LINK_SPEED, _5000_MTPS,
                                        *pXpPlLinkConfig0);
        break;
    case RM_SOE_BIF_LINK_SPEED_GEN3PCIE:
        *pXpPlLinkConfig0 = FLD_SET_DRF(_XPL, _PL_LTSSM_LINK_CONFIG_0, _TARGET_LINK_SPEED, _8000_MTPS,
                                        *pXpPlLinkConfig0);
        break;
    case RM_SOE_BIF_LINK_SPEED_GEN4PCIE:
        *pXpPlLinkConfig0 = FLD_SET_DRF(_XPL, _PL_LTSSM_LINK_CONFIG_0, _TARGET_LINK_SPEED, _16000_MTPS,
                                        *pXpPlLinkConfig0);
        break;
    case RM_SOE_BIF_LINK_SPEED_GEN5PCIE:
        *pXpPlLinkConfig0 = FLD_SET_DRF(_XPL, _PL_LTSSM_LINK_CONFIG_0, _TARGET_LINK_SPEED, _32000_MTPS,
                                        *pXpPlLinkConfig0);
        break;
    default:
        return FLCN_ERR_ILWALID_ARGUMENT;
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
sysSYSLIB_CODE static FLCN_STATUS
_bifWriteNewPcieLinkSpeed_LS10
(
    LwU32 xpPlLinkConfig0,
    LwU32 linkSpeed
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32 lwrrentLinkSpeed;

    // Trigger speed change
    REG_WR32(BAR0, LW_PTOP_UNICAST_SW_DEVICE_BASE_XPL_0 + LW_XPL_PL_LTSSM_LINK_CONFIG_0, xpPlLinkConfig0);

    // Wait for link to go ready (i.e., for the speed change to complete)
    if (!OS_PTIMER_SPIN_WAIT_NS_COND(_bifLtssmIsIdleAndLinkReady_LS10, NULL,
                                     BIF_LTSSM_LINK_READY_TIMEOUT_NS))
    {
        // Timed out waiting for LTSSM to indicate the link is ready
        return FLCN_ERR_TIMEOUT;
    }

    // Check if we trained to target speeds.
    status = _bifGetBusSpeed_LS10(&lwrrentLinkSpeed);
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
 * @brief Check if the link is ready
 *
 * @return  LW_TRUE if ready, LW_FALSE otherwise
 */
sysSYSLIB_CODE static LwBool
_bifIsLinkReady_LS10(void *pArgs)
{
    LwU32 ltssmState;

    ltssmState = REG_RD32(BAR0, LW_PTOP_UNICAST_SW_DEVICE_BASE_XPL_0 + LW_XPL_PL_LTSSM_STATE);
    return FLD_TEST_DRF(_XPL_PL, _LTSSM_STATE, _IS_LINK_READY, _TRUE, ltssmState);
}

/*!
 * @brief Check if the LTSSM is idle and has indicated
 *        the link is ready.
 *
 * @return  LW_TRUE if link is ready, LW_FALSE otherwise
 */
sysSYSLIB_CODE static LwBool
_bifLtssmIsIdleAndLinkReady_LS10(void *pArgs)
{
    return (_bifLtssmIsIdle_LS10(NULL) && _bifIsLinkReady_LS10(NULL));
}

/*!
 * @brief Get the current BIF speed
 *
 * @param[out]  pSpeed  Current bus speed.
 *
 * @return      FLCN_OK on success, otherwise an error code.
 */
sysSYSLIB_CODE static FLCN_STATUS
_bifGetBusSpeed_LS10
(
    LwU32 *pSpeed
)
{
    LwU32 tempRegVal;

    tempRegVal = REG_RD32(BAR0, DEVICE_BASE(LW_EP_PCFGM) + LW_EP_PCFG_GPU_LINK_CONTROL_STATUS);
    switch (DRF_VAL(_EP, _PCFG_GPU_LINK_CONTROL_STATUS, _LWRRENT_LINK_SPEED, tempRegVal))
    {
        case LW_EP_PCFG_GPU_LINK_CONTROL_STATUS_LWRRENT_LINK_SPEED_2P5:
            *pSpeed = RM_SOE_BIF_LINK_SPEED_GEN1PCIE;
            break;
        case LW_EP_PCFG_GPU_LINK_CONTROL_STATUS_LWRRENT_LINK_SPEED_5P0:
            *pSpeed = RM_SOE_BIF_LINK_SPEED_GEN2PCIE;
            break;
        case LW_EP_PCFG_GPU_LINK_CONTROL_STATUS_LWRRENT_LINK_SPEED_8P0:
            *pSpeed = RM_SOE_BIF_LINK_SPEED_GEN3PCIE;
           break;
        case LW_EP_PCFG_GPU_LINK_CONTROL_STATUS_LWRRENT_LINK_SPEED_16P0:
            *pSpeed = RM_SOE_BIF_LINK_SPEED_GEN4PCIE;
            break;
        case LW_EP_PCFG_GPU_LINK_CONTROL_STATUS_LWRRENT_LINK_SPEED_32P0:
            *pSpeed = RM_SOE_BIF_LINK_SPEED_GEN5PCIE;
        default:
            *pSpeed = RM_SOE_BIF_LINK_SPEED_ILWALID;
            return FLCN_ERROR;
    }

    return FLCN_OK;
}

/*!
 * @brief Reset xpPlLinkConfig0 to its original state.
 *
 * @param[in]   xpPlLinkConfig0     Register value of XP_PL_LINK_CONFIG_0.
 *
 */
sysSYSLIB_CODE static void
_bifResetXpPlState_LS10
(
    LwU32 xpPlLinkConfig0
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32 lwrrentLinkSpeed;

    status = _bifGetBusSpeed_LS10(&lwrrentLinkSpeed);
    if (status != FLCN_OK)
        return;

    status = _bifSetXpPlLinkRate_LS10(&xpPlLinkConfig0, lwrrentLinkSpeed);
    if (status != FLCN_OK)
        return;

    REG_WR32(BAR0, LW_PTOP_UNICAST_SW_DEVICE_BASE_XPL_0 + LW_XPL_PL_LTSSM_LINK_CONFIG_0, xpPlLinkConfig0);
}
