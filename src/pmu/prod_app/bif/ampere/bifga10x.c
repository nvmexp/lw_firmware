/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   bifga10x.c
 * @brief  Contains GA10X based HAL implementations
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmu_objpmu.h"

#include "dev_lw_xp.h"
#include "dev_lw_xve.h"
#include "dev_lw_xp_addendum.h"
#include "dev_pwr_csb.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "config/g_bif_private.h"
#include "pmu_objbif.h"
#include "pmu_objclk.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
OS_TMR_CALLBACK OsCBBifCheckL1ResidencyTimeout;
static OBJBIF_L1_RESIDENCY_QUEUE_PARAMS l1ResidencyQueue;
static OBJBIF_L1_RESIDENCY_PARAMS l1ResidencyParams;

/* ------------------------- Macros and Defines ----------------------------- */

static FLCN_STATUS s_enQueue(LwU32 value)
    GCC_ATTRIB_SECTION("imem_libBif", "s_enQueue");
static FLCN_STATUS s_deQueue(void)
    GCC_ATTRIB_SECTION("imem_libBif", "s_deQueue");
static void s_bifCallwlateL1Residency(LwU32 *pL1Residency)
    GCC_ATTRIB_SECTION("imem_libBif", "s_bifCallwlateL1Residency");
static OsTmrCallbackFunc (s_bifEngageAspmL1Aggressively)
    GCC_ATTRIB_USED();

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
bifWriteL1Threshold_GA10X
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
        }
    }

    // Verify values are within HW defined range
    if (PMUCFG_FEATURE_ENABLED(PMU_SELWRITY_HARDENING))
    {
        if ((l1Threshold < LW_XP_DL_MGR_1_L1_THRESHOLD_MIN) ||
            (l1Threshold > LW_XP_DL_MGR_1_L1_THRESHOLD_MAX) ||
            (l1ThresholdFrac < LW_XP_DL_MGR_0_L1_THRESHOLD_FRAC_MIN) ||
            (l1ThresholdFrac > LW_XP_DL_MGR_0_L1_THRESHOLD_FRAC_MAX))
        {
            PMU_BREAKPOINT();
            goto bifWriteL1Threshold_exit;
        }
    }

    data = BAR0_REG_RD32(LW_XP_DL_MGR_1(0));
    data = FLD_SET_DRF_NUM(_XP, _DL_MGR_1, _L1_THRESHOLD, l1Threshold, data);
    BAR0_REG_WR32(LW_XP_DL_MGR_1(0), data);

    data = BAR0_REG_RD32(LW_XP_DL_MGR_0(0));
    data = FLD_SET_DRF_NUM(_XP, _DL_MGR_0, _L1_THRESHOLD_FRAC,
                           l1ThresholdFrac, data);
    BAR0_REG_WR32(LW_XP_DL_MGR_0(0), data);

bifWriteL1Threshold_exit:
    return;
}

/*!
 * @brief bifCheckL1ThresholdData_GA10X()
 *  These checks are based on:
 *  https://confluence.lwpu.com/display/PMU/Input+validation+for+pmuRpcBifCfgInit
 *
 */
FLCN_STATUS
bifCheckL1Threshold_GA10X(void)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32 idx;

    if (Bif.platformParams.bAspmThreshTablePresent)
    {
        if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_BIF))
        {
            if (((DRF_VAL(_XVE, _L1_PM_SUBSTATES_CTRL1, _LTR_L1_2_THRES_VAL,
                          Bif.l1ssRegs.aspmL1SsCtrlEnable) !=
                  LW_BIF_LTR_L1_2_THRESHOLD_VALUE) &&
                 (DRF_VAL(_XVE, _L1_PM_SUBSTATES_CTRL1, _LTR_L1_2_THRES_VAL,
                          Bif.l1ssRegs.aspmL1SsCtrlEnable) !=
                  LW_XVE_L1_PM_SUBSTATES_CTRL1_LTR_L1_2_THRES_VAL_INIT)) ||
                ((DRF_VAL(_XVE, _L1_PM_SUBSTATES_CTRL1, _LTR_L1_2_THRES_SCALE,
                          Bif.l1ssRegs.aspmL1SsCtrlEnable) !=
                  LW_BIF_LTR_L1_2_THRESHOLD_SCALE) &&
                 (DRF_VAL(_XVE, _L1_PM_SUBSTATES_CTRL1, _LTR_L1_2_THRES_SCALE,
                          Bif.l1ssRegs.aspmL1SsCtrlEnable) !=
                  LW_XVE_L1_PM_SUBSTATES_CTRL1_LTR_L1_2_THRES_SCALE_INIT)))
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_ILWALID_ARGUMENT;
                goto bifCheckL1ThresholdData_exit;
            }
        }

        for (idx = RM_PMU_BIF_GEN1_INDEX;
             idx < RM_PMU_BIF_MAX_GEN_SPEED_SUPPORTED; ++idx)
        {
            if ((Bif.platformParams.l1ThresholdData[idx].l1Threshold <
                 LW_XP_DL_MGR_1_L1_THRESHOLD_MIN)      ||
                (Bif.platformParams.l1ThresholdData[idx].l1Threshold >
                 LW_XP_DL_MGR_1_L1_THRESHOLD_MAX)      ||
                (Bif.platformParams.l1ThresholdData[idx].l1ThresholdFrac <
                 LW_XP_DL_MGR_0_L1_THRESHOLD_FRAC_MIN) ||
                (Bif.platformParams.l1ThresholdData[idx].l1ThresholdFrac >
                 LW_XP_DL_MGR_0_L1_THRESHOLD_FRAC_MAX))
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_ILWALID_ARGUMENT;
                goto bifCheckL1ThresholdData_exit;
            }
        }
    }

bifCheckL1ThresholdData_exit:
    return status;
}

/*!
 * @brief Enable/disable audio device from the PCI tree.
 *
 * @param[in]  cmd       Specifies the action to perform.
 *   0 represents device was enabled.
 *   1 represents device was disabled.
 *
 * @return 'FLCN_ERR_NOT_SUPPORTED' if the function receives invalid command.
 *          Otherwise return FLCN_OK.
 */
FLCN_STATUS
bifServiceMultifunctionState_GA10X
(
    LwU8  cmd
)
{
    LwU32       data = 0;
    FLCN_STATUS status = FLCN_OK;

    data = BAR0_REG_RD32(DEVICE_BASE(LW_PCFG) + LW_XVE_PRIV_XV_1);

    if (cmd == BIF_DISABLE_GPU_MULTIFUNC_STATE)
    {
        data = FLD_SET_DRF(_XVE, _PRIV_XV_1, _CYA_XVE_MULTIFUNC,
                           _DISABLE, data);
    }
    else if (cmd == BIF_ENABLE_GPU_MULTIFUNC_STATE)
    {
        data = FLD_SET_DRF(_XVE, _PRIV_XV_1, _CYA_XVE_MULTIFUNC,
                           _ENABLE, data);
    }
    else
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_NOT_SUPPORTED;
    }

    BAR0_REG_WR32(DEVICE_BASE(LW_PCFG) + LW_XVE_PRIV_XV_1, data);

    return status;
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
bifSetEomParameters_GA10X
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
    // UPHY_*_CTRL2 register writes
    // To not alter the other bits of LW_UPHY_DLM_AE_EOM_MEAS_CTRL2,
    // we first read the register, then change only the required field
    // (EOM Mode) and finally write the value
    //
    tempRegVal = REG_RD32(FECS, LW_XP_PEX_PAD_CTL_8);
    tempRegVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_8, _CFG_RDS, 0x1, tempRegVal);
    tempRegVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_8, _CFG_WDS, 0x0, tempRegVal);
    tempRegVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_8, _CFG_ADDR,
                                  LW_UPHY_DLM_AE_EOM_MEAS_CTRL2, tempRegVal);
    REG_WR32(FECS, LW_XP_PEX_PAD_CTL_8, tempRegVal);

    rData = DRF_VAL(_XP, _PEX_PAD_CTL_9, _CFG_RDATA,
                    REG_RD32(FECS, LW_XP_PEX_PAD_CTL_9));

    rData = FLD_SET_DRF_NUM(_UPHY_DLM, _AE_EOM_MEAS_CTRL2, _FOM_MODE, 0xb, rData);
    rData = FLD_SET_DRF_NUM(_UPHY_DLM, _AE_EOM_MEAS_CTRL2, _NERRS, eomNerrs, rData);
    rData = FLD_SET_DRF_NUM(_UPHY_DLM, _AE_EOM_MEAS_CTRL2, _NBLKS, eomNblks, rData);

    tempRegVal = REG_RD32(FECS, LW_XP_PEX_PAD_CTL_8);
    tempRegVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_8, _CFG_RDS, 0x0, tempRegVal);
    tempRegVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_8, _CFG_WDS, 0x1, tempRegVal);
    tempRegVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_8, _CFG_WDATA, rData, tempRegVal);
    REG_WR32(FECS, LW_XP_PEX_PAD_CTL_8, tempRegVal);

    return FLCN_OK;
}

/*!
 * @brief Read PEX UPHY DLN registers
 *
 * @param[in]   regAddress         Register whose value needs to be read
 * @param[in]   laneSelectMask     Which lane to read from
 * @param[out]  pRegValue          The value of the register
 *
 * @return FLCN_OK After reading the regAddress
 */
FLCN_STATUS
bifGetUphyDlnCfgSpace_GA10X
(
    LwU32 regAddress,
    LwU32 laneSelectMask,
    LwU16 *pRegValue
)
{
  LwU32 tempRegVal;
  LwU32 rData;

  tempRegVal = REG_RD32(FECS, LW_XP_PEX_PAD_CTL_SEL);
  tempRegVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_SEL, _LANE_SELECT,
                               laneSelectMask, tempRegVal);
  REG_WR32(FECS, LW_XP_PEX_PAD_CTL_SEL, tempRegVal);

  tempRegVal = 0;
  tempRegVal = FLD_SET_DRF(_XP, _PEX_PAD_CTL_8, _CFG_RESET, _INIT, tempRegVal);
  tempRegVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_8, _CFG_RDS, 0x1, tempRegVal);
  tempRegVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_8, _CFG_WDS, 0x0, tempRegVal);
  tempRegVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_8, _CFG_ADDR, regAddress,
                               tempRegVal);
  REG_WR32(FECS, LW_XP_PEX_PAD_CTL_8, tempRegVal);

  rData = DRF_VAL(_XP, _PEX_PAD_CTL_9, _CFG_RDATA,
                  REG_RD32(FECS, LW_XP_PEX_PAD_CTL_9));
  *pRegValue = (LwU16) rData;

  return FLCN_OK;
}

/*!
 * @brief Helper interface to determine whether memory tuning controller
 *        decision needs to be overridden based on active PCIe settings.
 *
 *        This interface reads active PCIe link speed and width to make
 *        decision on whether the override is required.
 *
 * @return
 *  LW_TRUE
 *        Controller decision needs to be overridden.
 *  LW_FALSE
 *        Controller decision does not need to be overridden.
 */
LwBool
bifIsMemTuneControllerPcieOverrideEn_GA10X(void)
{
    LwU32   data            = 0;
    LwBool  bPcieOverrideEn = LW_FALSE;

    data = bifBar0ConfigSpaceRead_HAL(&Bif, LW_TRUE, LW_XVE_LINK_CONTROL_STATUS);

    //
    // Override memory tuning controller decision of current active link
    // speed is Gen 3 or above and current active link width is x8 or above.
    //
    if ((DRF_VAL(_XVE, _LINK_CONTROL_STATUS, _NEGOTIATED_LINK_WIDTH, data)
            >= LW_XVE_LINK_CONTROL_STATUS_NEGOTIATED_LINK_WIDTH_X8)         &&
        (DRF_VAL(_XVE, _LINK_CONTROL_STATUS, _LINK_SPEED, data)
            >= LW_XVE_LINK_CONTROL_STATUS_LINK_SPEED_8P0))
    {
        bPcieOverrideEn = LW_TRUE;
    }

    return bPcieOverrideEn;
}

/*!
 * @brief Helper function to enqueue L1 residencies in the cirlwlar queue
 *
 * @param[in] value Value to be enqueued
 *
 * @return    FLCN_OK                   when value enqueued
 *            FLCN_ERR_ILWALID_ARGUMENT when queue is full
 */
static FLCN_STATUS s_enQueue(LwU32 value)
{
    FLCN_STATUS status = FLCN_OK;
    LwS8        front  = l1ResidencyQueue.front;
    LwS8        rear   = l1ResidencyQueue.rear;
    LwU8        size   = l1ResidencyQueue.size;

    if ((front == 0 && (rear == (size-1))) ||
        (rear == (front-1)%(size-1)))
    {
        // Queue is full
        status = FLCN_ERR_ILWALID_ARGUMENT;
        return status;
    }
    else if (front == -1)
    {
        front = rear = 0;
        l1ResidencyQueue.l1ResidenciesArray[rear] = value;
    }
    else if (rear == (size-1) && front != 0)
    {
        rear = 0;
        l1ResidencyQueue.l1ResidenciesArray[rear] = value;
    }
    else
    {
        rear++;
        l1ResidencyQueue.l1ResidenciesArray[rear] = value;
    }

    l1ResidencyQueue.front = front;
    l1ResidencyQueue.rear = rear;

    return status;
}

/*!
 * @brief Helper function to dequeue L1 residencies from the cirlwlar queue
 *
 * @return    FLCN_OK                   when value dequeued
 *            FLCN_ERR_ILWALID_ARGUMENT when queue is empty
 */
static FLCN_STATUS s_deQueue()
{
    FLCN_STATUS status = FLCN_OK;
    LwS8        front  = l1ResidencyQueue.front;
    LwS8        rear   = l1ResidencyQueue.rear;
    LwU8        size   = l1ResidencyQueue.size;

    if (front == -1)
    {
        // Queue is empty
        status = FLCN_ERR_ILWALID_ARGUMENT;
        return status;
    }

    l1ResidencyQueue.l1ResidenciesArray[front] = -1;

    if (front == rear)
    {
        front = -1;
        rear = -1;
    }
    else if (front == (size-1))
    {
        front = 0;
    }
    else
    {
        front++;
    }

    l1ResidencyQueue.front = front;
    l1ResidencyQueue.rear = rear;

    return status;
}

/*!
 * @brief Create / Cancel the call back to engage ASPM L1 aggressively
 *
 * @param[in] bCancelCallback True if we are in PMU Deinit and call back
 *                            should be cancelled
 */
void
bifEngageAspmAggressivelyInitiateCallback_GA10X(LwBool bCancelCallback)
{
    LwU32 utilCountersEnable;
    LwU8  i;
    LwU8  size = 5;

    // Initialize the structure
    for (i = 0; i < size; i++)
    {
        l1ResidencyQueue.l1ResidenciesArray[i] = 0;
    }
    l1ResidencyQueue.front = -1;
    l1ResidencyQueue.rear = -1;
    l1ResidencyQueue.size = size;
    l1ResidencyParams.prevTxL0CountUs = 0;
    l1ResidencyParams.prevRxL0CountUs = 0;
    l1ResidencyParams.prevTxL0sCountUs = 0;
    l1ResidencyParams.prevRxL0sCountUs = 0;
    l1ResidencyParams.prevNonL0L0sCountUs = 0;

    if (!bCancelCallback)
    {
        utilCountersEnable = BAR0_REG_RD32(DEVICE_BASE(LW_PCFG) + LW_XVE_PCIE_UTIL_CTRL);
        utilCountersEnable = FLD_SET_DRF_NUM(_XVE_PCIE, _UTIL_CTRL, _ENABLE,
                                             1, utilCountersEnable);
        BAR0_REG_WR32(DEVICE_BASE(LW_PCFG) + LW_XVE_PCIE_UTIL_CTRL, utilCountersEnable);

        if (!OS_TMR_CALLBACK_WAS_CREATED(&OsCBBifCheckL1ResidencyTimeout))
        {
            osTmrCallbackCreate(&OsCBBifCheckL1ResidencyTimeout,         // pCallback
                OS_TMR_CALLBACK_TYPE_RELWRRENT_TO_EXPECTED_SKIP_MISSED,  // type
                OVL_INDEX_ILWALID,                                       // ovlImem
                s_bifEngageAspmL1Aggressively,                           // pTmrCallbackFunc
                LWOS_QUEUE(PMU, BIF),                                    // queueHandle
                BIF_CHECK_L1_RESIDENCY_CALLBACK_PERIOD_US,               // periodNormalus
                BIF_CHECK_L1_RESIDENCY_CALLBACK_PERIOD_US,               // periodSleepus
                OS_TIMER_RELAXED_MODE_USE_NORMAL,                        // bRelaxedUseSleep
                RM_PMU_TASK_ID_BIF);                                     // taskId
        }
        osTmrCallbackSchedule(&OsCBBifCheckL1ResidencyTimeout);
    }
    else if (bCancelCallback)
    {
        if (OS_TMR_CALLBACK_WAS_CREATED(&OsCBBifCheckL1ResidencyTimeout))
        {
            osTmrCallbackCancel(&OsCBBifCheckL1ResidencyTimeout);
        }
    }
}

/*!
 * @brief Update L1 threshold value based on difference between two conselwtive
 *        L1 residencies
 *        L1_ASPM_ENTRY_8us   0x6
 *        L1_ASPM_ENTRY_32us  0x8
 *        L1_ASPM_ENTRY_256us 0xB
 *
 * @param[in] newL1Threshold      new L1 threshold
 * @param[in] newL1ThresholdFrac  new L1 threshold fraction
 */
void
bifUpdateL1Threshold_GA10X
(
    LwU8 newL1Threshold,
    LwU8 newL1ThresholdFrac
)
{
    LwU32 regVal;

    regVal = BAR0_REG_RD32(LW_XP_DL_MGR_1(0));
    regVal = FLD_SET_DRF_NUM(_XP, _DL_MGR_1, _L1_THRESHOLD,
                             newL1Threshold, regVal);
    BAR0_REG_WR32(LW_XP_DL_MGR_1(0), regVal);

    regVal = BAR0_REG_RD32(LW_XP_DL_MGR_0(0));
    regVal = FLD_SET_DRF_NUM(_XP, _DL_MGR_0, _L1_THRESHOLD_FRAC,
                             newL1ThresholdFrac, regVal);
    BAR0_REG_WR32(LW_XP_DL_MGR_0(0), regVal);
}

/*!
 * @brief Callwlate L1 residency percentage
 *
 * @param[out] pL1Residency L1 residency percentage
 */
static void
s_bifCallwlateL1Residency
(
    LwU32 *pL1Residency
)
{
    LwU32 lwrrTxL0CountUs;
    LwU32 lwrrRxL0CountUs;
    LwU32 lwrrTxL0sCountUs;
    LwU32 lwrrRxL0sCountUs;
    LwU32 lwrrNonL0L0sCountUs;
    LwU32 txL0CountUs;
    LwU32 rxL0CountUs;
    LwU32 txL0sCountUs;
    LwU32 rxL0sCountUs;
    LwU32 nonL0L0sCountUs;
    LwU32 totalTxTimeUs;
    LwU32 totalRxTimeUs;
    LwU32 avgTotalTimeUs;

    // Callwlate the residencies
    lwrrTxL0CountUs = BAR0_REG_RD32(DEVICE_BASE(LW_PCFG) + LW_XVE_PCIE_UTIL_TX_L0);
    lwrrRxL0CountUs = BAR0_REG_RD32(DEVICE_BASE(LW_PCFG) + LW_XVE_PCIE_UTIL_RX_L0);
    lwrrTxL0sCountUs = BAR0_REG_RD32(DEVICE_BASE(LW_PCFG) + LW_XVE_PCIE_UTIL_TX_L0S);
    lwrrRxL0sCountUs = BAR0_REG_RD32(DEVICE_BASE(LW_PCFG) + LW_XVE_PCIE_UTIL_RX_L0S);
    lwrrNonL0L0sCountUs = BAR0_REG_RD32(DEVICE_BASE(LW_PCFG) + LW_XVE_PCIE_UTIL_NON_L0_L0S);

    // Check for wrap around
    if ((lwrrTxL0CountUs >> 31) > 0)
    {
        lwrrTxL0CountUs = (lwrrTxL0CountUs & LW_BIF_BIT_MASK) +
                              LW_BIF_BIT_MASK;
        BAR0_REG_WR32(DEVICE_BASE(LW_PCFG) + LW_XVE_PCIE_UTIL_TX_L0,
                      LW_XVE_PCIE_UTIL_CLR_DONE);
    }
    if ((lwrrRxL0CountUs >> 31) > 0)
    {
        lwrrRxL0CountUs = (lwrrRxL0CountUs & LW_BIF_BIT_MASK) +
                              LW_BIF_BIT_MASK;
        BAR0_REG_WR32(DEVICE_BASE(LW_PCFG) + LW_XVE_PCIE_UTIL_RX_L0,
                                  LW_XVE_PCIE_UTIL_CLR_DONE);
    }
    if ((lwrrTxL0sCountUs >> 31) > 0)
    {
        lwrrTxL0sCountUs = (lwrrTxL0sCountUs & LW_BIF_BIT_MASK) +
                               LW_BIF_BIT_MASK;
        BAR0_REG_WR32(DEVICE_BASE(LW_PCFG) + LW_XVE_PCIE_UTIL_TX_L0S,
                                  LW_XVE_PCIE_UTIL_CLR_DONE);
    }
    if ((lwrrRxL0sCountUs >> 31) > 0)
    {
        lwrrRxL0sCountUs = (lwrrRxL0sCountUs & LW_BIF_BIT_MASK) +
                               LW_BIF_BIT_MASK;
        BAR0_REG_WR32(DEVICE_BASE(LW_PCFG) + LW_XVE_PCIE_UTIL_RX_L0S,
                                  LW_XVE_PCIE_UTIL_CLR_DONE);
    }
    if ((lwrrNonL0L0sCountUs >> 31) > 0)
    {
        lwrrNonL0L0sCountUs = (lwrrNonL0L0sCountUs & LW_BIF_BIT_MASK) +
                                   LW_BIF_BIT_MASK;
        BAR0_REG_WR32(DEVICE_BASE(LW_PCFG) + LW_XVE_PCIE_UTIL_NON_L0_L0S,
                                  LW_XVE_PCIE_UTIL_CLR_DONE);
    }

    txL0CountUs = (lwrrTxL0CountUs - l1ResidencyParams.prevTxL0CountUs);
    rxL0CountUs = (lwrrRxL0CountUs - l1ResidencyParams.prevRxL0CountUs);
    txL0sCountUs = (lwrrTxL0sCountUs - l1ResidencyParams.prevTxL0sCountUs);
    rxL0sCountUs = (lwrrRxL0sCountUs - l1ResidencyParams.prevRxL0sCountUs);
    nonL0L0sCountUs = (lwrrNonL0L0sCountUs - l1ResidencyParams.prevNonL0L0sCountUs);

    l1ResidencyParams.prevTxL0CountUs = lwrrTxL0CountUs;
    l1ResidencyParams.prevRxL0CountUs = lwrrRxL0CountUs;
    l1ResidencyParams.prevTxL0sCountUs = lwrrTxL0sCountUs;
    l1ResidencyParams.prevRxL0sCountUs = lwrrRxL0sCountUs;
    l1ResidencyParams.prevNonL0L0sCountUs = lwrrNonL0L0sCountUs;

    totalTxTimeUs = txL0CountUs + txL0sCountUs + nonL0L0sCountUs;
    totalRxTimeUs = rxL0CountUs + rxL0sCountUs + nonL0L0sCountUs;
    avgTotalTimeUs = (totalTxTimeUs + totalRxTimeUs)/2;

    if (avgTotalTimeUs != 0)
    {
        *pL1Residency = (nonL0L0sCountUs*100)/avgTotalTimeUs;
    }
}

/*!
 * @brief Call back function to engage ASPM aggressively in P3
 *
 * @param[in] pCallback Call back
 *
 * @return    FLCN_OK
 */
static FLCN_STATUS
s_bifEngageAspmL1Aggressively
(
    OS_TMR_CALLBACK *pCallback
)
{
    LwU32       l1Residency = 0;
    LwU32       avgPrevFive;
    LwU32       sumPrevFive = 0;
    LwU8        size        = l1ResidencyQueue.size;
    LwU32       diff;
    LwU8        i;
    FLCN_STATUS status;

    if ((Bif.pstateNum != PSTATE_NUM_P3) ||
        (Bif.bPowerSrcDc == LW_FALSE))
    {
        return FLCN_OK;
    }
    else
    {
        s_bifCallwlateL1Residency(&l1Residency);

        // store in queue
        status = s_enQueue(l1Residency);
        if (status != FLCN_OK)
        {
            for (i = 0; i < size; i++)
            {
                sumPrevFive = sumPrevFive + l1ResidencyQueue.l1ResidenciesArray[i];
            }
            // average of previous 5 entries
            avgPrevFive = sumPrevFive/size;

            if (l1Residency > avgPrevFive)
            {
                diff = l1Residency - avgPrevFive;
                if (avgPrevFive != 0)
                {
                    diff = (diff*100)/avgPrevFive;
                }

                // reduce the idle threshold as the residency has increased
                if (l1Residency >= Bif.upperLimitNewL1Residency &&
                    diff >= Bif.upperLimitDiffPercentage)
                {
                    bifUpdateL1Threshold_HAL(&Bif,
                        BIF_L1_IDLE_THRESHOLD_8_US,
                        BIF_L1_IDLE_THRESHOLD_FRAC_0);
                }
                else if (l1Residency >= Bif.lowerLimitNewL1Residency &&
                         diff >= Bif.lowerLimitDiffPercentage)
                {
                    bifUpdateL1Threshold_HAL(&Bif,
                        BIF_L1_IDLE_THRESHOLD_32_US,
                        BIF_L1_IDLE_THRESHOLD_FRAC_0);
                }
            }
            else if (l1Residency < avgPrevFive)
            {
                diff = avgPrevFive - l1Residency;
                if (avgPrevFive != 0)
                {
                    diff = (diff*100)/avgPrevFive;
                }

                // increase the idle threshold as the residency has decreased
                if (l1Residency < Bif.lowerLimitNewL1Residency)
                {
                    bifUpdateL1Threshold_HAL(&Bif,
                        BIF_L1_IDLE_THRESHOLD_256_US,
                        BIF_L1_IDLE_THRESHOLD_FRAC_0);
                }
                else if (l1Residency < Bif.upperLimitNewL1Residency &&
                         diff >= Bif.upperLimitDiffPercentage)
                {
                    bifUpdateL1Threshold_HAL(&Bif,
                        BIF_L1_IDLE_THRESHOLD_256_US,
                        BIF_L1_IDLE_THRESHOLD_FRAC_0);
                }
                else if (diff >= Bif.lowerLimitDiffPercentage)
                {
                    bifUpdateL1Threshold_HAL(&Bif,
                        BIF_L1_IDLE_THRESHOLD_32_US,
                        BIF_L1_IDLE_THRESHOLD_FRAC_0);
                }
            }

            s_deQueue();
            s_enQueue(l1Residency);
        }
    }

    return FLCN_OK;
}
