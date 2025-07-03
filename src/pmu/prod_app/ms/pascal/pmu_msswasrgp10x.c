/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_msswasrgp10x.c
 * @brief  SW-ASR specific HAL-interface for the GP10X
 */

/* ------------------------- System Includes -------------------------------- */

#include "pmusw.h"
#include "dev_pwr_csb.h"
#include "dev_pwr_pri.h"
#include "dev_fb.h"
#include "dev_fbpa.h"
#include "dev_fbpa_addendum.h"
#include "dev_trim.h"
#include "dev_trim_addendum.h"
#include "dev_pri_ringstation_fbp.h"
/* ------------------------- Application Includes --------------------------- */

#include "pmu_objms.h"
#include "pmu_swasr.h"
#include "pmu_objpmu.h"
#include "pmu_objtimer.h"
#include "pmu_objfb.h"
#include "dbgprintf.h"
#include "config/g_ms_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------ Static Variables -------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* --------------------------- Prototypes ------------------------------------*/

/* ------------------------ Public Functions  ------------------------------- */

/*!
 * @brief Power down different components in FB pads
 * 
 * Function -
 * 1) Disables calibration updates
 * 2) Power down internal VREF circuit
 * 3) Power down CML tree, interpolator.
 *    Disable DC current for RX and TX.
 *    Power down different DIGDLLs.
 *    refer @ "LW_PFB_FBPA_FBIO_CFG_PWRD" in dev_fbpa.ref.
 * 4) Powerdown the BIAS pads
 *    refer @ "LW_PFB_FBPA_FBIO_CFG8" in dev_fbpa.ref.
 * 5) Powerdown the WL DDLL in CMD BRICKS
 *    refer @ "LW_PFB_FBPA_FBIO_CMD_PAD_CTRL" in dev_fbpa.ref.
 * 6) Lower voltage in address/command and data pads
 *    refer @ "LW_PFB_FBPA_FBIO_CFG_BRICK_VAUXGEN" and
 *    "LW_PFB_FBPA_FBIO_CFG_VTTGEN_VAUXGEN" in dev_fbpa.ref.
 */
void
msFbPadPwrDown_GP10X(void)
{
    LwU32     val;
    OBJSWASR *pSwAsr = MS_GET_SWASR();

    // Disable pad calibration  LW_PFB_FBPA_FBIO_SPARE[11]=1
    val = REG_RD32(FECS, LW_PFB_FBPA_FBIO_SPARE);
    val = FLD_SET_DRF(_PFB, _FBPA_FBIO_SPARE, _PAD_CALIBRATION, _DISABLE, val);
    REG_WR32(FECS, LW_PFB_FBPA_FBIO_SPARE, val);

    //
    // Powerdown all receivers, Digdlls, dlcell ..
    // _VREF_PWRD powers down the internal VREF
    // _RDQS_VREF_PWRD down internal VREF for RDQS in DQ brick
    //

    val = pSwAsr->regs.fbpaFbioBytePadCtrl2 =
        REG_RD32(FECS, LW_PFB_FBPA_FBIO_BYTE_PAD_CTRL2);
    val = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_BYTE_PAD_CTRL2, _VREF_PWRD, 1, val);
    val = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_BYTE_PAD_CTRL2, _RDQS_VREF_PWRD, 1, val);
    REG_WR32(FECS, LW_PFB_FBPA_FBIO_BYTE_PAD_CTRL2, val);

    val = pSwAsr->regs.fbpaFbioMddllCntl
         = REG_RD32(FECS, LW_PFB_FBPA_FBIO_MDDLL_CNTL);
    val = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_MDDLL_CNTL, _DQ_MDDLL_PWRD,
                      1, val);
    val = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_MDDLL_CNTL, _CMD_MDDLL_PWRD,
                      1, val);
    REG_WR32(FECS, LW_PFB_FBPA_FBIO_MDDLL_CNTL, val);

    val = pSwAsr->regs.fbpaFbioVttCntl2
         = REG_RD32(FECS, LW_PFB_FBPA_FBIO_VTT_CNTL2);
    val = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_VTT_CNTL2, _DQ_IREF_PWRD,
                      1, val);
    val = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_VTT_CNTL2, _CMD_IREF_PWRD,
                      1, val);
    val = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_VTT_CNTL2, _CK_IREF_PWRD,
                      1, val);
    val = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_VTT_CNTL2, _WCK_IREF_PWRD,
                      1, val);
    REG_WR32(FECS, LW_PFB_FBPA_FBIO_VTT_CNTL2, val);

    val = pSwAsr->regs.fbpaFbioCkCk2dqs
         = REG_RD32(FECS, LW_PFB_FBPA_FBIO_CK_CK2DQS);
    val = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_CK_CK2DQS, _TRIM_PWRD_SUBP0_CK,
                      1, val);
    val = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_CK_CK2DQS, _TRIM_PWRD_SUBP1_CK,
                      1, val);
    REG_WR32(FECS, LW_PFB_FBPA_FBIO_CK_CK2DQS, val);

    val = pSwAsr->regs.fbpaFbioSubp0WckCk2dqsMsb
         = REG_RD32(FECS, LW_PFB_FBPA_FBIO_SUBP0_WCK_CK2DQS_MSB);
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

    val = pSwAsr->regs.fbpaFbioSubp1WckCk2dqsMsb
         = REG_RD32(FECS, LW_PFB_FBPA_FBIO_SUBP1_WCK_CK2DQS_MSB);
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

    val = pSwAsr->regs.fbpaFbioCfgPwrd
         = REG_RD32(FECS, LW_PFB_FBPA_FBIO_CFG_PWRD);
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
    val = pSwAsr->regs.fbpaFbioCmdPadCtrl =
        REG_RD32(FECS, LW_PFB_FBPA_FBIO_CMD_PAD_CTRL);
    val = FLD_SET_DRF(_PFB, _FBPA_FBIO_CMD_PAD_CTRL, _DIGDLL_PWRD_SUBP0, _OFF,
                      val);
    val = FLD_SET_DRF(_PFB, _FBPA_FBIO_CMD_PAD_CTRL, _DIGDLL_PWRD_SUBP1, _OFF,
                      val);
    REG_WR32(FECS, LW_PFB_FBPA_FBIO_CMD_PAD_CTRL, val);

    //
    // Lower VAUXC, VAUXP, VCLAMP to 0.85V for DQ, AD, CK, WCK, COMP, CDB
    // Value of 0x2 means 0.85V for DQ, AD and CLKPAD
    //
    val = pSwAsr->regs.fbpaFbioCfgBrickVauxgen = REG_RD32(FECS, LW_PFB_FBPA_FBIO_CFG_BRICK_VAUXGEN);

    val = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_BRICK_VAUXGEN, _REG_LVAUXP, _850mV,
                      val);
    val = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_BRICK_VAUXGEN, _REG_LVCLAMP,
                      _850mV, val);
    val = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_BRICK_VAUXGEN, _REG_LVAUXC, _850mV,
                      val);
    REG_WR32(FECS, LW_PFB_FBPA_FBIO_CFG_BRICK_VAUXGEN, val);

    val = pSwAsr->regs.fbpaFbioCfgVttgelwauxgen =
        REG_RD32(FECS, LW_PFB_FBPA_FBIO_CFG_VTTGEN_VAUXGEN);

    val = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_VTTGEN_VAUXGEN, _REG_VAUXP_LVL,
                      _850mV, val);
    val = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_VTTGEN_VAUXGEN, _REG_VCLAMP_LVL,
                      _850mV, val);
    val = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_VTTGEN_VAUXGEN, _REG_VAUXC_LVL,
                      _850mV, val);
    REG_WR32(FECS, LW_PFB_FBPA_FBIO_CFG_VTTGEN_VAUXGEN, val);
}

/*!
 * @brief Power up all component in FB Pads
 *
 * This function reverts operation done in _msFbPadPwrDown
 */

void
msFbPadPwrUp_GP10X(void)
{
    LwU32     val;
    OBJSWASR *pSwAsr = MS_GET_SWASR();

    // Restore VAUXC, VAUXP, VCLAMP to 1V
    REG_WR32(FECS, LW_PFB_FBPA_FBIO_CFG_VTTGEN_VAUXGEN,
                      pSwAsr->regs.fbpaFbioCfgVttgelwauxgen);
    REG_WR32(FECS, LW_PFB_FBPA_FBIO_CFG_BRICK_VAUXGEN,
                      pSwAsr->regs.fbpaFbioCfgBrickVauxgen);

    // Enable PAD calibration   LW_PFB_FBPA_FBIO_SPARE[11]=0
    val = REG_RD32(FECS, LW_PFB_FBPA_FBIO_SPARE);
    val = FLD_SET_DRF(_PFB, _FBPA_FBIO_SPARE, _PAD_CALIBRATION, _ENABLE, val);
    REG_WR32(FECS, LW_PFB_FBPA_FBIO_SPARE, val);

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

/* ------------------------ Private Functions  ------------------------------ */

