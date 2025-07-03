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
 * @file   pmu_msswasrga10x.c
 * @brief  SW-ASR specific HAL-interface for the GA10X
 */

/* ------------------------- System Includes -------------------------------- */

#include "pmusw.h"
#include "dev_fb.h"
#include "dev_fbpa.h"
#include "dev_fbpa_addendum.h"
#include "dev_trim.h"

/* ------------------------- Application Includes --------------------------- */

#include "pmu_objms.h"
#include "pmu_objpmu.h"
#include "config/g_ms_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------ Static Variables -------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* --------------------------- Prototypes ------------------------------------*/
static void s_msSwAsrIssueEntryExitCommand_GA10X(LwBool bEntry);

/* ------------------------ Public Functions  ------------------------------- */

/*!
 * @brief: Put the Dram in Self Refresh mode
 */
void
msSwAsrIssueEntryCommand_GA10X(void)
{
    s_msSwAsrIssueEntryExitCommand_GA10X(LW_TRUE);
}

/*!
 * @brief: Issue Self Refresh exit commands
 */
void
msSwAsrIssueExitCommand_GA10X(void)
{
    s_msSwAsrIssueEntryExitCommand_GA10X(LW_FALSE);
}

/*!
 * @brief Gate DRAM clock
 *
 * This function gates DRAM clock from VCO path. That is, it gates
 * clock from output of REFM PLL
 *
 * As per dislwssion with Mahipal and Preetham, DRAM clock will never source
 * from DRAM PLL in P8 and P5. (Reference bug #1357490) Thus, this function
 * doesn't considers the case of driving DRAM clock from DRAM PLL.
 */
void
msSwAsrDramClockGate_GA10X(void)
{
    LwU32 val;
    OBJSWASR *pSwAsr = MS_GET_SWASR();

    // If PTRIM_OPT is SET, use the FBIO register interface
    if (FLD_TEST_DRF(_PFB, _FBPA_SW_CONFIG, _PTRIM_OPT, _SET, pSwAsr->regs.fbpaSwConfig))
    {
        pSwAsr->regs.fbioRefmpllConfig = REG_RD32(FECS, LW_PFB_FBPA_FBIO_REFMPLL_CONFIG);

        // Gate DRAMCLK from reference path
        val = FLD_SET_DRF(_PFB, _FBPA_FBIO_REFMPLL_CONFIG,
                          _STOP_SYNCMUX, _ENABLE, pSwAsr->regs.fbioRefmpllConfig);

        REG_WR32(FECS, LW_PFB_FBPA_FBIO_REFMPLL_CONFIG, val);

        // Read the LW_PFB_FBPA_FBIO_CTRL_SER_PRIV to flush all previous writes
        val = REG_RD32(FECS, LW_PFB_FBPA_FBIO_CTRL_SER_PRIV);
    }
    else // Legacy Sequence
    {
        //
        // TODO-pankumar: This is a temp code to catch the exelwtion of this legacy sequence
        // on GA10X+ chips.
        // We were not able to remove this code in chips_a because of the AXL Sanity
        // of GA102, which is still using this older path.
        // But for new chip, we need to enforce removal of this code and update the
        // CMOS Vbios to have the correct settings.
        // Hence adding this code to catch the older settings.
        //
        // Once CMOS Vbios are update, i will move this check
        //
#if ((PMU_PROFILE_GH20X || PMU_PROFILE_GH20X_RISCV || PMU_PROFILE_AD10X_RISCV))
        PMU_BREAKPOINT();
#endif

        pSwAsr->regs.fbioModeSwitch = REG_RD32(FECS, LW_PTRIM_SYS_FBIO_MODE_SWITCH);

        // Gate DRAMCLK from reference path
        val = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH,
                          _REFMPLL_STOP_SYNCMUX, _ENABLE, pSwAsr->regs.fbioModeSwitch);

        REG_WR32(FECS, LW_PTRIM_SYS_FBIO_MODE_SWITCH, val);

        // Read LW_PTRIM_SYS_FBIO_MODE_SWITCH flush all previous writes.
        val = REG_RD32(FECS, LW_PTRIM_SYS_FBIO_MODE_SWITCH);
    }
}

/*!
 * @brief Ungate DRAM clock
 *
 * This function restores state of RFE path of DRAM clock.
 */
void
msSwAsrDramClockUngate_GA10X(void)
{
    OBJSWASR *pSwAsr = MS_GET_SWASR();
    LwU32     val;
    LwU32     refmPllStop;

    // If PTRIM_OPT is SET, use the FBIO register interface
    if (FLD_TEST_DRF(_PFB, _FBPA_SW_CONFIG, _PTRIM_OPT, _SET, pSwAsr->regs.fbpaSwConfig))
    {
        refmPllStop = DRF_VAL(_PFB, _FBPA_FBIO_REFMPLL_CONFIG,
                              _STOP_SYNCMUX, pSwAsr->regs.fbioRefmpllConfig);

        val = REG_RD32(FECS, LW_PFB_FBPA_FBIO_REFMPLL_CONFIG);

        // restore setting for REFMPLL_STOP_SYNCMUX
        val = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_REFMPLL_CONFIG,
                              _STOP_SYNCMUX, refmPllStop, val);

        REG_WR32(FECS, LW_PFB_FBPA_FBIO_REFMPLL_CONFIG, val);

        // Read the LW_PFB_FBPA_FBIO_CTRL_SER_PRIV to flush all previous writes
        val = REG_RD32(FECS, LW_PFB_FBPA_FBIO_CTRL_SER_PRIV);
    }
    else // Legacy Sequence
    {
        //
        // TODO-pankumar: This is a temp code to catch the exelwtion of this legacy sequence
        // on GA10X+ chips.
        // We were not able to remove this code in chips_a because of the AXL Sanity
        // of GA102, which is still using this older path.
        // But for new chip, we need to enforce removal of this code and update the
        // CMOS Vbios to have the correct settings.
        // Hence adding this code to catch the older settings.
        //
        // Once CMOS Vbios are update, i will move this check
        //
#if ((PMU_PROFILE_GH20X || PMU_PROFILE_GH20X_RISCV || PMU_PROFILE_AD10X_RISCV))
        PMU_BREAKPOINT();
#endif

        // Get settings of REFMPLL_STOP_SYNCMUX
        refmPllStop = DRF_VAL(_PTRIM, _SYS_FBIO_MODE_SWITCH,
                              _REFMPLL_STOP_SYNCMUX, pSwAsr->regs.fbioModeSwitch);

        val = REG_RD32(FECS, LW_PTRIM_SYS_FBIO_MODE_SWITCH);

        // Restore settings of REFMPLL_STOP_SYNCMUX
        val = FLD_SET_DRF_NUM(_PTRIM, _SYS_FBIO_MODE_SWITCH,
                              _REFMPLL_STOP_SYNCMUX, refmPllStop, val);

        REG_WR32(FECS, LW_PTRIM_SYS_FBIO_MODE_SWITCH, val);

        // Read LW_PTRIM_SYS_FBIO_MODE_SWITCH flush all previous writes.
        val = REG_RD32(FECS, LW_PTRIM_SYS_FBIO_MODE_SWITCH);
    }
}

/*!
 * @brief Disable/restore the FB Temperature controls
 *
 * @params[in] bDisable - LW_TRUE -> Disable the Temperature tracking
 *                        LW_FALSE -> Restore the original value
 */
void
msFbTemperatureControlDisable_GA10X
(
    LwBool bDisable
)
{
    OBJSWASR *pSwAsr = MS_GET_SWASR();
    LwU32     regVal = 0;

    if (bDisable)
    {
        // Cache the Temperature control settings
        regVal = pSwAsr->regs.fbpaTempCtrl = REG_RD32(FECS, LW_PFB_FBPA_DQR_CTRL);

        // Disable the Temperature control as we are going into self refresh mode
        regVal = FLD_SET_DRF(_PFB, _FBPA_DQR_CTRL, _EN_IC0_SUBP0, _INIT, regVal);
        regVal = FLD_SET_DRF(_PFB, _FBPA_DQR_CTRL, _EN_IC0_SUBP1, _INIT, regVal);
        regVal = FLD_SET_DRF(_PFB, _FBPA_DQR_CTRL, _EN_IC1_SUBP0, _INIT, regVal);
        regVal = FLD_SET_DRF(_PFB, _FBPA_DQR_CTRL, _EN_IC1_SUBP1, _INIT, regVal);

        REG_WR32_STALL(FECS, LW_PFB_FBPA_DQR_CTRL, regVal);
    }
    else
    {
        // Restore the original cached value for Temperature Control
        REG_WR32_STALL(FECS, LW_PFB_FBPA_DQR_CTRL, pSwAsr->regs.fbpaTempCtrl);
    }
}

/*!
 * @brief: Update the Pad Vref setting before training
 */
void
msSwAsrPadVrefSet_GA10X(void)
{
    OBJSWASR *pSwAsr = MS_GET_SWASR();
    LwU32     regVal;

    regVal = REG_RD32(FECS, LW_PFB_FBPA_FBIO_CFG10);

    // Save DQ_TERM_MODE
    pSwAsr->regs.cfgDqTermMode = DRF_VAL(_PFB, _FBPA_FBIO_CFG10, _DQ_TERM_MODE,
                                         regVal);

    // if DQ_TERM_MODE is not equal to _PULLUP then peform below operations.
    if (pSwAsr->regs.cfgDqTermMode != LW_PFB_FBPA_FBIO_CFG10_DQ_TERM_MODE_PULLUP)
    {
        regVal = REG_RD32(FECS, LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE3);

        // Save PAD8 and PAD9 Values
        pSwAsr->regs.vrefPad8 = DRF_VAL(_PFB, _FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE3,
                                        _PAD8, regVal);
        pSwAsr->regs.vrefPad9 = DRF_VAL(_PFB, _FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE3,
                                        _PAD9, regVal);

        regVal = REG_RD32(FECS, LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_VREF_MID_CODE1);

        // Save Mid Code
        pSwAsr->regs.vrefMidCode = DRF_VAL(_PFB, _FBPA_FBIOTRNG_SUBP0BYTE0_VREF_MID_CODE1,
                                           _PAD0, regVal);

        regVal = REG_RD32(FECS, LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_VREF_UPPER_CODE1);

        // Save Upper Code
        pSwAsr->regs.vrefUpperCode = DRF_VAL(_PFB, _FBPA_FBIOTRNG_SUBP0BYTE0_VREF_UPPER_CODE1,
                                             _PAD0, regVal);

        regVal = REG_RD32(FECS, LW_PFB_FBPA_FBIO_DELAY_BROADCAST_MISC1);

        // Set DQ_VREF_CODE 0x90
        regVal = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_DELAY_BROADCAST_MISC1,
                                 _DQ_VREF_CODE, 0x90, regVal);

        // Set DQS_VREF_CODE 0x90
        regVal = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_DELAY_BROADCAST_MISC1,
                                 _DQS_VREF_CODE, 0x90, regVal);

        // Set VERF_MID_CODE
        regVal = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_DELAY_BROADCAST_MISC1,
                                 _DQ_VREF_MID_CODE, 0x90, regVal);

        // Set VREF_UPPER_CODE
        regVal = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_DELAY_BROADCAST_MISC1,
                                 _DQ_VREF_UPPER_CODE, 0x90, regVal);

        REG_WR32(FECS, LW_PFB_FBPA_FBIO_DELAY_BROADCAST_MISC1, regVal);

        regVal = REG_RD32(FECS, LW_PFB_FBPA_FBIO_CFG10);

        // set DQ_TERM_MODE to PULLUP
        regVal = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG10, _DQ_TERM_MODE,
                             _PULLUP, regVal);

        REG_WR32_STALL(FECS, LW_PFB_FBPA_FBIO_CFG10, regVal);

        //
        // as per HW folks, we need to wait for 1Us to get the VREF setting updated
        // in the HW. So adding the 1uS delay for propagation
        //
        OS_PTIMER_SPIN_WAIT_US(1);
    }
}

/*!
 * @brief: Restore the Pad Vref setting before training
 */
void
msSwAsrPadVrefRestore_GA10X(void)
{
    OBJSWASR *pSwAsr = MS_GET_SWASR();
    LwU32     regVal;

    // if DQ_TERM_MODE is not equal to _PULLUP then peform below operations.
    if (pSwAsr->regs.cfgDqTermMode != LW_PFB_FBPA_FBIO_CFG10_DQ_TERM_MODE_PULLUP)
    {
        // Set PAD8, PAD9, VREF MID and UPPER Values
        regVal = REG_RD32(FECS, LW_PFB_FBPA_FBIO_DELAY_BROADCAST_MISC1);
        regVal = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_DELAY_BROADCAST_MISC1,
                                 _DQ_VREF_CODE, pSwAsr->regs.vrefPad8, regVal);
        regVal = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_DELAY_BROADCAST_MISC1,
                                 _DQS_VREF_CODE, pSwAsr->regs.vrefPad9, regVal);
        regVal = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_DELAY_BROADCAST_MISC1,
                                 _DQ_VREF_MID_CODE, pSwAsr->regs.vrefMidCode, regVal);
        regVal = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_DELAY_BROADCAST_MISC1,
                                 _DQ_VREF_UPPER_CODE, pSwAsr->regs.vrefUpperCode, regVal);

        REG_WR32(FECS, LW_PFB_FBPA_FBIO_DELAY_BROADCAST_MISC1, regVal);

        // Restore DQ_TERM_MODE
        regVal = REG_RD32(FECS, LW_PFB_FBPA_FBIO_CFG10);
        regVal = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_CFG10, _DQ_TERM_MODE,
                                 pSwAsr->regs.cfgDqTermMode, regVal);
        REG_WR32_STALL(FECS, LW_PFB_FBPA_FBIO_CFG10, regVal);

        //
        // as per HW folks, we need to wait for 1Us to get the VREF setting updated
        // in the HW. So adding the 1uS delay for propagation
        //
        OS_PTIMER_SPIN_WAIT_US(1);
    }
}

/*!
 * @brief Enable Periodic Training
 *
 * We do Periodic Training for GDDR5 when MCLK > 405MHz (in P5 and above
 * PState). If we are doing SW-ASR for MCLK greater than 405MHz then disable
 * Periodic Training before putting DRAM into Self Refresh.
 */
void
msPeriodicTrainingEnable_GA10X(void)
{
    LwU32 val = 0;

    val = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _WCK,        _DISABLED, 0);
    val = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _WR,         _ENABLED, val);
    val = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _STROBE,     _ENABLED, val);
    val = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _QUSE,       _DISABLED, val);
    val = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _QUSE_PRIME, _DISABLED, val);
    val = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _PERIODIC,   _ENABLED, val);
    val = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _CONTROL,    _DLL, val);
    val = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _STEP,       _INIT, val);
    val = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _GO,         _INIT, val);

    // Write the training register with considering HALF FBPA mode
    msFbTrainingCommandUpdate_HAL(&Ms, val);
}

/* ------------------------- Private Functions ----------------------------- */

static void
s_msSwAsrIssueEntryExitCommand_GA10X
(
    LwBool bEntry
)
{
    LwU32 regVal;

    //
    // Detail on this sequence is present in the Bug: 2645698
    //

    regVal = REG_RD32(FECS, LW_PFB_FBPA_CFG0);

    //
    // As per HW guidelines, DRAM_ACK Should never be kept enabled.
    // If it was already kept enabled then flag it as error.
    //
    if (FLD_TEST_DRF(_PFB, _FBPA_CFG0, _DRAM_ACK, _ENABLED, regVal))
    {
        PMU_BREAKPOINT();
    }

    // Enable the DRAM ACK
    regVal = FLD_SET_DRF(_PFB, _FBPA_CFG0, _DRAM_ACK, _ENABLED, regVal);
    REG_WR32_STALL(FECS, LW_PFB_FBPA_CFG0, regVal);

    // Update the Self refresh command
    regVal = REG_RD32(FECS, LW_PFB_FBPA_SELF_REF);

    if (bEntry)
    {
        // Enable the Self Refresh of DRAM
        regVal = FLD_SET_DRF(_PFB, _FBPA_SELF_REF, _CMD, _ENABLED, regVal);
    }
    else
    {
        // Disable the Self Refresh of DRAM
        regVal = FLD_SET_DRF(_PFB, _FBPA_SELF_REF, _CMD, _DISABLED, regVal);
    }
    REG_WR32_STALL(FECS, LW_PFB_FBPA_SELF_REF, regVal);

    // Disable the DRAM ACK
    regVal = REG_RD32(FECS, LW_PFB_FBPA_CFG0);
    regVal = FLD_SET_DRF(_PFB, _FBPA_CFG0, _DRAM_ACK, _DISABLED, regVal);
    REG_WR32_STALL(FECS, LW_PFB_FBPA_CFG0, regVal);
}

/*!
 * @brief Power down different components in FB pads
 *
 * Function -
 * 1) Disables Pad calibration updates
 * 2) if LW_PFB_FBPA_SW_CONFIG_MSCG_OPT is set,
 *    then follow the new sequnece otheriwise the
 *    legacy Sequence.
 *
 * New Sequence: (Ampere and later)
 *   Clamp all pad controls to deep power down state
 *   refer @LW_PFB_FBPA_FBIO_CTRL_SER_PRIV_DPD = 1
 *
 * Legacy Sequence
 * 1) Power down internal VREF circuit
 * 2) Power down CML tree, interpolator.
 *    Disable DC current for RX and TX.
 *    Power down different DIGDLLs.
 *    a. LW_PFB_FBPA_FBIO_BYTE_PAD_CTRL2
 *    b. LW_PFB_FBPA_FBIO_MDDLL_CNTL
 *    c. LW_PFB_FBPA_FBIO_VTT_CNTL2
 *    d. LW_PFB_FBPA_FBIO_CK_CK2DQS
 *    e. LW_PFB_FBPA_FBIO_SUBP0_WCK_CK2DQS_MSB
 *    f. LW_PFB_FBPA_FBIO_SUBP1_WCK_CK2DQS_MSB
 *    g. LW_PFB_FBPA_FBIO_CFG_PWRD
 * 3) Powerdown the WL DDLL in CMD BRICKS
 *    in LW_PFB_FBPA_FBIO_CMD_PAD_CTRL
 * 4) Lower voltage in LW_PFB_FBPA_FBIO_VTT_CTRL2
 */
void
msFbPadPwrDown_GA10X(void)
{
    LwU32     val;
    OBJSWASR *pSwAsr = MS_GET_SWASR();

    val = REG_RD32(FECS, LW_PFB_FBPA_FBIO_SPARE);

    // Disable pad calibration  LW_PFB_FBPA_FBIO_SPARE[11]=1
    val = FLD_SET_DRF(_PFB, _FBPA_FBIO_SPARE, _VALUE_PAD_CALIBRATION, _DISABLE, val);

    //
    // Disable the Clock serializer before gating the clocks. This is needed for
    // preventing the Load glicthes in the PAD. Details are in Bug: 2645314 comment #39
    // BIT 12 is used to disbale the serializer.
    //
    val = FLD_SET_DRF(_PFB, _FBPA_FBIO_SPARE, _VALUE_SERIALIZER, _DISABLE, val);
    REG_WR32(FECS, LW_PFB_FBPA_FBIO_SPARE, val);

    // Read back the register to make sure write is flushed to FBP partitions
    val = REG_RD32(FECS, LW_PFB_FBPA_FBIO_SPARE);

    if (FLD_TEST_DRF(_PFB, _FBPA_SW_CONFIG, _MSCG_OPT, _SET, pSwAsr->regs.fbpaSwConfig))
    {
        //
        // Clamp all pad controls to deep power down state
        //
        // All FB pads'DPD expect COMP are in SER PRIV.
        //
        val = pSwAsr->regs.fbpaFbioCtrlSerPriv = REG_RD32(FECS, LW_PFB_FBPA_FBIO_CTRL_SER_PRIV);
        val = FLD_SET_DRF(_PFB, _FBPA_FBIO_CTRL_SER_PRIV, _DPD, _ENABLE, val);
        REG_WR32_STALL(FECS, LW_PFB_FBPA_FBIO_CTRL_SER_PRIV, val);
    }
    else
    {
        //
        // TODO-pankumar: This is a temp code to catch the exelwtion of this legacy sequence
        // on GA10X+ chips.
        // We were not able to remove this code in chips_a because of the AXL Sanity
        // of GA102, which is still using this older path.
        // But for new chip, we need to enforce removal of this code and update the
        // CMOS Vbios to have the correct settings.
        // Hence adding this code to catch the older settings.
        //
        // Once CMOS Vbios are update, i will move this check
        //
#if ((PMU_PROFILE_GH20X || PMU_PROFILE_GH20X_RISCV || PMU_PROFILE_AD10X_RISCV))
        PMU_BREAKPOINT();
#endif

        //
        // Powerdown all receivers, Digdlls, dlcell ..
        // _VREF_PWRD powers down the internal VREF
        // _RDQS_VREF_PWRD down internal VREF for RDQS in DQ brick
        //
        val = pSwAsr->regs.fbpaFbioBytePadCtrl2 = REG_RD32(FECS, LW_PFB_FBPA_FBIO_BYTE_PAD_CTRL2);
        val = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_BYTE_PAD_CTRL2, _VREF_PWRD, 1, val);
        val = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_BYTE_PAD_CTRL2, _RDQS_VREF_PWRD, 1, val);
        REG_WR32(FECS, LW_PFB_FBPA_FBIO_BYTE_PAD_CTRL2, val);

        val = pSwAsr->regs.fbpaFbioMddllCntl = REG_RD32(FECS, LW_PFB_FBPA_FBIO_MDDLL_CNTL);
        val = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_MDDLL_CNTL, _DQ_MDDLL_PWRD,
                              1, val);
        val = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_MDDLL_CNTL, _CMD_MDDLL_PWRD,
                              1, val);
        REG_WR32(FECS, LW_PFB_FBPA_FBIO_MDDLL_CNTL, val);

        val = pSwAsr->regs.fbpaFbioVttCntl2 = REG_RD32(FECS, LW_PFB_FBPA_FBIO_VTT_CNTL2);
        val = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_VTT_CNTL2, _DQ_IREF_PWRD,
                              1, val);
        val = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_VTT_CNTL2, _CMD_IREF_PWRD,
                              1, val);
        val = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_VTT_CNTL2, _CK_IREF_PWRD,
                              1, val);
        val = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_VTT_CNTL2, _WCK_IREF_PWRD,
                              1, val);
        REG_WR32(FECS, LW_PFB_FBPA_FBIO_VTT_CNTL2, val);

        val = pSwAsr->regs.fbpaFbioCkCk2dqs = REG_RD32(FECS, LW_PFB_FBPA_FBIO_CK_CK2DQS);
        val = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_CK_CK2DQS, _TRIM_PWRD_SUBP0_CK,
                              1, val);
        val = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_CK_CK2DQS, _TRIM_PWRD_SUBP1_CK,
                              1, val);
        REG_WR32(FECS, LW_PFB_FBPA_FBIO_CK_CK2DQS, val);

        val = pSwAsr->regs.fbpaFbioSubp0WckCk2dqsMsb = REG_RD32(FECS, LW_PFB_FBPA_FBIO_SUBP0_WCK_CK2DQS_MSB);
        val = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_SUBP0_WCK_CK2DQS_MSB, _TRIM_PWRD_WCK01,
                              1, val);
        val = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_SUBP0_WCK_CK2DQS_MSB, _TRIM_PWRD_WCK23,
                              1, val);
        val = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_SUBP0_WCK_CK2DQS_MSB, _TRIM_WCKB01,
                              1, val);
        val = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_SUBP0_WCK_CK2DQS_MSB, _TRIM_PWRD_WCKB01,
                              1, val);
        val = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_SUBP0_WCK_CK2DQS_MSB, _TRIM_PWRD_WCKB23,
                              1, val);
        REG_WR32(FECS, LW_PFB_FBPA_FBIO_SUBP0_WCK_CK2DQS_MSB, val);

        val = pSwAsr->regs.fbpaFbioSubp1WckCk2dqsMsb = REG_RD32(FECS, LW_PFB_FBPA_FBIO_SUBP1_WCK_CK2DQS_MSB);
        val = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_SUBP1_WCK_CK2DQS_MSB, _TRIM_PWRD_WCK01,
                              1, val);
        val = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_SUBP1_WCK_CK2DQS_MSB, _TRIM_PWRD_WCK23,
                              1, val);
        val = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_SUBP1_WCK_CK2DQS_MSB, _TRIM_WCKB01,
                              1, val);
        val = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_SUBP1_WCK_CK2DQS_MSB, _TRIM_PWRD_WCKB01,
                              1, val);
        val = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_SUBP1_WCK_CK2DQS_MSB, _TRIM_PWRD_WCKB23,
                              1, val);
        REG_WR32(FECS, LW_PFB_FBPA_FBIO_SUBP1_WCK_CK2DQS_MSB, val);

        val = pSwAsr->regs.fbpaFbioCfgPwrd = REG_RD32(FECS, LW_PFB_FBPA_FBIO_CFG_PWRD);
        val = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _CML_PWRD,
                          _ENABLE, val);
        val = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _INTERP_PWRD,
                          _ENABLE, val);
        val = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _INTERP_RDQS_PWRD,
                          _ENABLE, val);
        val = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _DQ_TX_PWRD,
                          _ENABLE, val);
        val = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _RDQS_TX_PWRD,
                          _ENABLE, val);
        val = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _DQ_RX_GDDR5_PWRD,
                          _ENABLE, val);
        val = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _RDQS_RX_GDDR5_PWRD,
                          _ENABLE, val);
        val = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _DQ_RX_LEGACY_PWRD,
                          _ENABLE, val);
        val = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _RDQS_RX_LEGACY_PWRD,
                          _ENABLE, val);
        val = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _DQ_RX_SCHMT_PWRD,
                          _ENABLE, val);
        val = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _RDQS_RX_SCHMT_PWRD,
                          _ENABLE, val);
        val = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _DQ_RX_BIAS_CTRL,
                          _ENABLE, val);
        val = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _DQ_RX_BIAS_DYNAMIC_CTRL,
                          _ON, val);
        val = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _CLK_TX_PWRD,
                          _ENABLE, val);
        val = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_CFG_PWRD, _WCK_TX_PWRD,
                              0x1, val);
        val = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_CFG_PWRD, _WCKB_TX_PWRD,
                              0x1, val);
        val = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _DIFFAMP_PWRD,
                          _ENABLE, val);
        val = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _DQ_RX_DIGDLL_PWRD,
                          _ENABLE, val);
        val = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _DQ_TX_DIGDLL_PWRD,
                          _ENABLE, val);
        val = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _DQDQS_DIGDLL_PWRD,
                          _ENABLE, val);
        val = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _QUSE_DIGDLL_PWRD,
                          _ENABLE, val);
        val = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _CK_DIGDLL_PWRD,
                          _ENABLE, val);
        val = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _WCK_DIGDLL_PWRD,
                          _ENABLE, val);
        val = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _WCKB_DIGDLL_PWRD,
                          _ENABLE, val);
        val = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _RDQS_DIFFAMP_PWRD,
                          _ENABLE, val);

        REG_WR32(FECS, LW_PFB_FBPA_FBIO_CFG_PWRD, val);

        // Powerdown DIGDLL in CMD PADS
        val = pSwAsr->regs.fbpaFbioCmdPadCtrl = REG_RD32(FECS, LW_PFB_FBPA_FBIO_CMD_PAD_CTRL);
        val = FLD_SET_DRF(_PFB, _FBPA_FBIO_CMD_PAD_CTRL, _DIGDLL_PWRD_SUBP0,
                          _OFF, val);
        val = FLD_SET_DRF(_PFB, _FBPA_FBIO_CMD_PAD_CTRL, _DIGDLL_PWRD_SUBP1,
                          _OFF, val);
        REG_WR32(FECS, LW_PFB_FBPA_FBIO_CMD_PAD_CTRL, val);

        //
        // Lower VAUXC, VAUXP, VCLAMP to 0.85V for DQ, AD, CK, WCK, COMP, CDB
        // Value of 0x2 means 0.85V for DQ, AD and CLKPAD
        //
        val = pSwAsr->regs.fbpaFbioVttCtrl2 = REG_RD32(FECS, LW_PFB_FBPA_FBIO_VTT_CTRL2);

        val = FLD_SET_DRF(_PFB, _FBPA_FBIO_VTT_CTRL2, _CDB_VAUXC, _850mV, val);
        val = FLD_SET_DRF(_PFB, _FBPA_FBIO_VTT_CTRL2, _CDB_VAUXP, _850mV, val);
        val = FLD_SET_DRF(_PFB, _FBPA_FBIO_VTT_CTRL2, _CDB_VCLAMP, _850mV,
                          val);
        REG_WR32(FECS, LW_PFB_FBPA_FBIO_VTT_CTRL2, val);
    }
}

/*!
 * @brief Power up all component in FB Pads
 *
 * This function reverts operation done in msFbPadPwrDown_GA10X
 */
void
msFbPadPwrUp_GA10X(void)
{
    LwU32     val;
    OBJSWASR *pSwAsr = MS_GET_SWASR();

    if (FLD_TEST_DRF(_PFB, _FBPA_SW_CONFIG, _MSCG_OPT, _SET, pSwAsr->regs.fbpaSwConfig))
    {
        // Restore DPD for other Pads
        REG_WR32_STALL(FECS, LW_PFB_FBPA_FBIO_CTRL_SER_PRIV,
                                pSwAsr->regs.fbpaFbioCtrlSerPriv);
    }
    else
    {
        //
        // TODO-pankumar: This is a temp code to catch the exelwtion of this legacy sequence
        // on GA10X+ chips.
        // We were not able to remove this code in chips_a because of the AXL Sanity
        // of GA102, which is still using this older path.
        // But for new chip, we need to enforce removal of this code and update the
        // CMOS Vbios to have the correct settings.
        // Hence adding this code to catch the older settings.
        //
        // Once CMOS Vbios are update, i will move this check
        //
#if ((PMU_PROFILE_GH20X || PMU_PROFILE_GH20X_RISCV || PMU_PROFILE_AD10X_RISCV))
        PMU_BREAKPOINT();
#endif

        // Restore VAUXC, VAUXP, VCLAMP to 1V
        REG_WR32(FECS, LW_PFB_FBPA_FBIO_VTT_CTRL2,
                                pSwAsr->regs.fbpaFbioVttCtrl2);

        REG_WR32(FECS, LW_PFB_FBPA_FBIO_CMD_PAD_CTRL,
                                pSwAsr->regs.fbpaFbioCmdPadCtrl);

        // Disable powerdowns. LW_PFB_FBPA_1_FBIO_BYTE_PAD_CTRL2_VREF_PWRD=0
        REG_WR32(FECS, LW_PFB_FBPA_FBIO_BYTE_PAD_CTRL2,
                                pSwAsr->regs.fbpaFbioBytePadCtrl2);

        // Restore various registers to their pre-powerdown values.
        REG_WR32(FECS, LW_PFB_FBPA_FBIO_MDDLL_CNTL,
                                pSwAsr->regs.fbpaFbioMddllCntl);
        REG_WR32(FECS, LW_PFB_FBPA_FBIO_VTT_CNTL2,
                                pSwAsr->regs.fbpaFbioVttCntl2);
        REG_WR32(FECS, LW_PFB_FBPA_FBIO_CK_CK2DQS,
                                pSwAsr->regs.fbpaFbioCkCk2dqs);
        REG_WR32(FECS, LW_PFB_FBPA_FBIO_SUBP0_WCK_CK2DQS_MSB,
                                pSwAsr->regs.fbpaFbioSubp0WckCk2dqsMsb);
        REG_WR32(FECS, LW_PFB_FBPA_FBIO_SUBP1_WCK_CK2DQS_MSB,
                                pSwAsr->regs.fbpaFbioSubp1WckCk2dqsMsb);

        // Restore LW_PFB_FBPA_FBIO_CFG_PWRD to original value
        REG_WR32(FECS, LW_PFB_FBPA_FBIO_CFG_PWRD,
                                pSwAsr->regs.fbpaFbioCfgPwrd);
    }

    // Update the FBIO Spare register
    val = REG_RD32(FECS, LW_PFB_FBPA_FBIO_SPARE);

    // Enable PAD calibration   LW_PFB_FBPA_FBIO_SPARE[11]=0
    val = FLD_SET_DRF(_PFB, _FBPA_FBIO_SPARE, _VALUE_PAD_CALIBRATION, _ENABLE, val);

    // Re enable the serializer
    val = FLD_SET_DRF(_PFB, _FBPA_FBIO_SPARE, _VALUE_SERIALIZER, _ENABLE, val);

    REG_WR32(FECS, LW_PFB_FBPA_FBIO_SPARE, val);
    // Read back the register to make sure write is flushed to FBP partitions
    val = REG_RD32(FECS, LW_PFB_FBPA_FBIO_SPARE);
}
