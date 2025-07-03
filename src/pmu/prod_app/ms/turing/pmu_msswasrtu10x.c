/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_msswasrtu10x.c
 * @brief  SW-ASR specific HAL-interface for the TU10X
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
#include "hwproject.h"
#include "pmu/pmumailboxid.h"

/* ------------------------- Application Includes --------------------------- */

#include "pmu_objms.h"
#include "pmu_swasr.h"
#include "pmu_objpmu.h"
#include "pmu_objtimer.h"
#include "dbgprintf.h"
#include "config/g_ms_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------ Static Variables -------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* --------------------------- Prototypes ------------------------------------*/
static void s_msGddr6TestCmdPtrReset_TU10X(void);
static void s_msGddr6TestCmdExtUpdate_TU10X(LwBool);
/* ------------------------ Public Functions  ------------------------------- */

/*!
 * @brief: Update the Pad Vref setting before training
 */
void
msSwAsrPadVrefSet_TU10X(void)
{
    OBJSWASR *pSwAsr = MS_GET_SWASR();
    LwU32     regVal;

    regVal = REG_RD32(FECS, LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE3);

    // Save PAD8 and PAD9 Values
    pSwAsr->regs.vrefPad8 = DRF_VAL(_PFB, _FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE3,
                                    _PAD8, regVal);
    pSwAsr->regs.vrefPad9 = DRF_VAL(_PFB, _FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE3,
                                    _PAD9, regVal);

    regVal = REG_RD32(FECS, LW_PFB_FBPA_FBIO_DELAY_BROADCAST_MISC1);

    // Set DQ_VREF_CODE 0x4D
    regVal = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_DELAY_BROADCAST_MISC1,
                             _DQ_VREF_CODE, 0x4D, regVal);

    // Set DQS_VREF_CODE 0x48
    regVal = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_DELAY_BROADCAST_MISC1,
                             _DQS_VREF_CODE, 0x48, regVal);
    REG_WR32(FECS, LW_PFB_FBPA_FBIO_DELAY_BROADCAST_MISC1, regVal);

    regVal = REG_RD32(FECS, LW_PFB_FBPA_FBIO_CFG10);

    // Save DQ_TERM_MODE
    pSwAsr->regs.cfgDqTermMode = DRF_VAL(_PFB, _FBPA_FBIO_CFG10, _DQ_TERM_MODE,
                                         regVal);

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

/*!
 * @brief: Restore the Pad Vref setting before training
 */
void
msSwAsrPadVrefRestore_TU10X(void)
{
    OBJSWASR *pSwAsr = MS_GET_SWASR();
    LwU32     regVal;

    // Set PAD8 and PAD9 Values
    regVal = REG_RD32(FECS, LW_PFB_FBPA_FBIO_DELAY_BROADCAST_MISC1);
    regVal = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_DELAY_BROADCAST_MISC1,
                             _DQ_VREF_CODE, pSwAsr->regs.vrefPad8, regVal);
    regVal = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_DELAY_BROADCAST_MISC1,
                             _DQS_VREF_CODE, pSwAsr->regs.vrefPad9, regVal);
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
/*!
 * @brief: Put the Dram in Self Refresh mode
 */
void
msSwAsrIssueEntryCommand_TU10X(void)
{
    OBJSWASR *pSwAsr = MS_GET_SWASR();
    LwU32     val;

    // Special Handling for GDDR6 DRAM
    if (pSwAsr->bIsGDDR6)
    {
        //
        // If we have GDDR6 DRAM, we need to do a TESTCMD PTR reset
        //
        s_msGddr6TestCmdPtrReset_TU10X();

        // Update the TESTCMD_EXT regsiter
        s_msGddr6TestCmdExtUpdate_TU10X(LW_TRUE);
    }

    val = REG_RD32(FECS, LW_PFB_FBPA_TESTCMD);

    // Prepare the Self refresh Entry 1 command
    val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _RES,  _DDR5_SELF_REF_ENTRY1, val);
    val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _RAS,  _DDR5_SELF_REF_ENTRY1, val);
    val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CAS,  _DDR5_SELF_REF_ENTRY1, val);
    val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CS0,  _DDR5_SELF_REF_ENTRY1, val);
    val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CS1,  _DDR5_SELF_REF_ENTRY1, val);
    val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _WE,   _DDR5_SELF_REF_ENTRY1, val);
    val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _HOLD, _DDR5_SELF_REF_ENTRY1, val);
    val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _GO,   _DDR5_SELF_REF_ENTRY1, val);
    val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CKE,  _DDR5_SELF_REF_ENTRY1, val);

    // If we do not have GDDRX type of Ram, override the CKE BIT
    if (!pSwAsr->bIsGDDRx)
    {
        val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CKE,
                          _LEGACY_SELF_REF_ENTRY1, val);
    }
    // if we have GDDR6 type DRAM Override GDDR6 specific commands
    else if (pSwAsr->bIsGDDR6)
    {
        val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _RAS, _DDR6_SELF_REF_ENTRY1, val);
        val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CAS, _DDR6_SELF_REF_ENTRY1, val);
        val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _WE,  _DDR6_SELF_REF_ENTRY1, val);
    }

    // Check if we need to set _ADR Bit
    if (pSwAsr->bIsGDDR3MirrorCmdMapping)
    {
        val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _ADR,
                          _LEGACY_MIRR_SELF_REF_ENTRY1, val);
    }

    // Issue the Self refresh entry command 1
    REG_WR32(FECS, LW_PFB_FBPA_TESTCMD, val);

    // Update the TESTCMD_EXT regsiter
    if (pSwAsr->bIsGDDR6)
    {
        s_msGddr6TestCmdExtUpdate_TU10X(LW_FALSE);
    }

    // Prepare the Self refresh Entry 2 comand
    val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _RES,  _DDR5_SELF_REF_ENTRY2, val);
    val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _RAS,  _DDR5_SELF_REF_ENTRY2, val);
    val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CAS,  _DDR5_SELF_REF_ENTRY2, val);
    val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CS0,  _DDR5_SELF_REF_ENTRY2, val);
    val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CS1,  _DDR5_SELF_REF_ENTRY2, val);
    val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _WE,   _DDR5_SELF_REF_ENTRY2, val);
    val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _HOLD, _DDR5_SELF_REF_ENTRY2, val);
    val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _GO,   _DDR5_SELF_REF_ENTRY2, val);
    val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CKE,  _DDR5_SELF_REF_ENTRY2, val);

    // If we do not have GDDRX type of Ram, override the CKE BIT
    if (!pSwAsr->bIsGDDRx)
    {
        val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CKE,
                          _LEGACY_SELF_REF_ENTRY2, val);
    }
    // Override GDDR6 specific commands
    else if (pSwAsr->bIsGDDR6)
    {
        val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _RAS, _DDR6_SELF_REF_ENTRY2, val);
        val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CAS, _DDR6_SELF_REF_ENTRY2, val);
        val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CS0, _DDR6_SELF_REF_ENTRY2, val);
        val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CS1, _DDR6_SELF_REF_ENTRY2, val);
        val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _WE,  _DDR6_SELF_REF_ENTRY2, val);
    }

    // Check if we need to set _ADR Bit
    if (pSwAsr->bIsGDDR3MirrorCmdMapping)
    {
        val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _ADR,
                          _LEGACY_MIRR_SELF_REF_ENTRY2, val);
    }

    // Issue the Self refresh entry command 2
    REG_WR32(FECS, LW_PFB_FBPA_TESTCMD, val);
}

/*!
 * @brief: Issue Self Refresh exit command
 */
void
msSwAsrIssueExitCommand_TU10X(void)
{
    OBJSWASR *pSwAsr = MS_GET_SWASR();

    if (pSwAsr->bIsGDDR6)
    {
        s_msGddr6TestCmdExtUpdate_TU10X(LW_FALSE);
    }

    msSwAsrIssueExitCommand_GMXXX();
}

/*!
 * @brief: Reset the TESTCMD_PTR for GDDR6
 */
static void
s_msGddr6TestCmdPtrReset_TU10X(void)
{
    LwU32 regVal;

    regVal = REG_RD32(FECS, LW_PFB_FBPA_PTR_RESET);

    // Issue the reset to testcmd for GDDR6
    regVal = FLD_SET_DRF(_PFB, _FBPA_PTR_RESET, _TESTCMD, _ON, regVal);
    REG_WR32(FECS, LW_PFB_FBPA_PTR_RESET, regVal);

    // Wait for 1 uS for the reset to propagate
    OS_PTIMER_SPIN_WAIT_US(1);

    // Remove the reset
    regVal = FLD_SET_DRF(_PFB, _FBPA_PTR_RESET, _TESTCMD, _OFF, regVal);
    REG_WR32(FECS, LW_PFB_FBPA_PTR_RESET, regVal);

    // Wait for 1 uS for the reset to propagate
    OS_PTIMER_SPIN_WAIT_US(1);
}

/*!
 * @brief: update the TESTCMD_EXT for GDDR6
 */
static void
s_msGddr6TestCmdExtUpdate_TU10X
(
    LwBool bEntry
)
{
    LwU32 regVal;

    if (bEntry)
    {
        // If we have GDDR6 DRAM, then Clear the TESTCMD_EXT_1 and
        REG_WR32(FECS, LW_PFB_FBPA_TESTCMD_EXT_1, 0);
    }

    //
    // If we have GDDR6 DRAM update _TESTCMD_EXT_ACT to 1'b1 to avoid CA
    // pins - bug 1964387
    //
    regVal = REG_RD32(FECS, LW_PFB_FBPA_TESTCMD_EXT);
    regVal = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT, _ACT, _INIT, regVal);
    REG_WR32(FECS, LW_PFB_FBPA_TESTCMD_EXT, regVal);
}
