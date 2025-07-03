/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   fbsrga10x.c
 * @brief  FB self refresh Hal Functions for GA10X
 *
 * FBSR Hal Functions implement FB related functionalities for GA10X
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_fbpa.h"
#include "dev_master.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmu.h"
#include "pmu_objfb.h"
#include "dbgprintf.h"
#include "dev_trim.h"
#include "hwproject.h"
#include "config/g_fb_private.h"
#include "ctrl/ctrl2080/ctrl2080fb.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */

/*!
 * @brief Enable DRAM self refresh. The umbrella function for all types of rams
 *
 * @param[in]     ramType   The VRAM type, passed-in by RM
 * @param[out]    pEntry1   TESTCMD value for ENTRY1, NOP for Ampere G6X
 * @param[out]    pEntry2   TESTCMD value for ENTRY2, NOP for Ampere G6X
 */
void
fbEnableSelfRefresh_GA10X
(
    LwU32  ramType,
    LwU32 *pEntry1,
    LwU32 *pEntry2
)
{
    // for SIM compatibility
    if ((ramType == LW2080_CTRL_FB_INFO_RAM_TYPE_GDDR5) ||
        (ramType == LW2080_CTRL_FB_INFO_RAM_TYPE_GDDR5X))
    {
            fbGetSelfRefreshEntryValsGddr5_HAL(&Fb, pEntry1, pEntry2);
    }

    // Issue a individual precharge
    REG_WR32(FECS, LW_PFB_FBPA_PRE, LW_PFB_FBPA_PRE_CMD_PRECHARGE_1);

    //
    // Disable DRAM auto refresh.  On GT21x this is just before _SELF_REF_CMD.
    // Since _SELF_REF_CMD is broken we have to use _TESTCMD, which does not
    // wait for pending transactions.  So we have to make sure all of the refreshes
    // are done.  Bug 665236.
    //
    REG_WR32(FECS, LW_PFB_FBPA_REFCTRL, DRF_DEF(_PFB_FBPA, _REFCTRL, _VALID, _0));
    REG_RD32(FECS, LW_PMC_BOOT_0);
    OS_PTIMER_SPIN_WAIT_NS(2000);

    // Issue Autorefresh commands.
    // This gains us a bit of time ahead of the standard refresh counter.
    REG_WR32(FECS, LW_PFB_FBPA_REF, LW_PFB_FBPA_REF_CMD_REFRESH_1);
    REG_WR32(FECS, LW_PFB_FBPA_REF, LW_PFB_FBPA_REF_CMD_REFRESH_1);
    REG_RD32(FECS, LW_PMC_BOOT_0);
    OS_PTIMER_SPIN_WAIT_NS(1000);

    // use 1 dummy read to fbio register to introduce a delay of ~1 usec sim time.
    REG_RD32(FECS, LW_PFB_FBPA_FBIO_CFG1);
    OS_PTIMER_SPIN_WAIT_NS(2000);

    // On GDDR6, need special handling before enabling self refresh
    if ((ramType == LW2080_CTRL_FB_INFO_RAM_TYPE_GDDR6) ||
        (ramType == LW2080_CTRL_FB_INFO_RAM_TYPE_GDDR6X))
    {
        fbEnableSelfRefreshForGddr6_HAL(&Fb, pEntry1, pEntry2);
    }
    else // for SIM compatibility, GDDR5/GDDR5X
    {
        // Write LW_PFB_FBPA_TESTCMD to enable self-refresh.
        REG_WR32(FECS, LW_PFB_FBPA_TESTCMD, *pEntry1);
        REG_WR32(FECS, LW_PFB_FBPA_TESTCMD, *pEntry2);
    }
}

/*!
 * @brief Enable DRAM self refresh for GDDR6/6X.
 *
 * @param[in]     ramType   The VRAM type, passed-in by RM
 * @param[out]    pEntry1   TESTCMD value for ENTRY1, NOP for Ampere G6X
 * @param[out]    pEntry2   TESTCMD value for ENTRY2, NOP for Ampere G6X
 */
void
fbEnableSelfRefreshForGddr6_GA10X
(
    LwU32 *pEntry1,
    LwU32 *pEntry2
)
{
    LwU32 val = REG_RD32(FECS, LW_PFB_FBPA_SW_CONFIG);

    //
    // If LW_PFB_FBPA_SW_CONFIG_GC6_HIB_OPT is set
    // then we enter hibernation mode
    // VBIOS devinit programs the _GC6_HIB_OPT value for
    // next GC6 entry usage
    //
    if ( FLD_TEST_DRF(_PFB_FBPA, _SW_CONFIG, _GC6_HIB_OPT, _SET, val))
    {
        val = REG_RD32(FECS, LW_PFB_FBPA_GENERIC_MRS7);
        val = FLD_SET_DRF(_PFB_FBPA, _GENERIC_MRS7, _ADR_GDDR6X_HIBERNATE_SR, _ON, val);
        REG_WR32(FECS, LW_PFB_FBPA_GENERIC_MRS7, val);
    }

    // sequence from http://lwbugs/2645319/10
    val = REG_RD32(FECS, LW_PFB_FBPA_CFG0);
    val = FLD_SET_DRF(_PFB, _FBPA_CFG0, _DRAM_ACK, _ENABLED, val);
    REG_WR32_STALL(FECS, LW_PFB_FBPA_CFG0, val);

    val = REG_RD32(FECS, LW_PFB_FBPA_SELF_REF);
    val = FLD_SET_DRF(_PFB, _FBPA_SELF_REF, _CMD, _ENABLED, val);
    REG_WR32_STALL(FECS, LW_PFB_FBPA_SELF_REF, val);

    val = REG_RD32(FECS, LW_PFB_FBPA_CFG0);
    val = FLD_SET_DRF(_PFB, _FBPA_CFG0, _DRAM_ACK, _DISABLED, val);
    REG_WR32_STALL(FECS, LW_PFB_FBPA_CFG0, val);
}

/*!
 * @brief Functon to power down DRAM and enter self refresh
 *
 * @param[in]     ramType   The VRAM type, passed-in by RM
 */
GCX_GC6_ENTRY_STATUS
fbEnterSrAndPowerDown_GA10X(LwU8 ramType)
{
    LwU32 val    = 0;
    LwU32 entry1 = 0;
    LwU32 entry2 = 0;

    // Disable APCD and ZQ -> should be ok to directly write these regs as header files are relative.
    // TODO: copy seq to _GM10X too.
    val = REG_RD32(FECS, LW_PFB_FBPA_CFG0);
    val = FLD_SET_DRF(_PFB, _FBPA_CFG0, _DRAM_ACPD, _NO_POWERDOWN, val);
    REG_WR32(FECS, LW_PFB_FBPA_CFG0, val);

    val = REG_RD32(FECS, LW_PFB_FBPA_ZQ);
    val = FLD_SET_DRF(_PFB_FBPA, _ZQ, _CL_CMD, _0, val);
    val = FLD_SET_DRF(_PFB_FBPA, _ZQ, _CS_CMD, _0, val);
    val = FLD_SET_DRF(_PFB_FBPA, _ZQ, _CL_PERIODIC, _DISABLED, val);
    val = FLD_SET_DRF(_PFB_FBPA, _ZQ, _CS_PERIODIC, _DISABLED, val);
    REG_WR32(FECS, LW_PFB_FBPA_ZQ, val);

    //
    //Refer bug - http://lwbugs/3042411 - EDC pin needs to be hiz
    //Also refer - http://lwbugs/1731988 - For proper vref offset calibration of EDC pads
    //MRS8[5] is the control for gddr6/gddr6x
    //
    if ((ramType == LW2080_CTRL_FB_INFO_RAM_TYPE_GDDR6X) ||
        (ramType == LW2080_CTRL_FB_INFO_RAM_TYPE_GDDR6))
    {
        val = REG_RD32(FECS, LW_PFB_FBPA_GENERIC_MRS8);
        val = FLD_SET_DRF(_PFB, _FBPA_GENERIC_MRS8, _ADR_GDDR6X_EDC_HIZ, _ON, val);
        REG_WR32_STALL(FECS, LW_PFB_FBPA_GENERIC_MRS8, val);
   }
    // Enable FBSR based on ram type
    fbEnableSelfRefresh_HAL(&Fb, ramType, &entry1, &entry2);

    // Issue a bunch of dummy FBIO reads to introduce a delay of 2 usec sim time.
    REG_RD32(FECS, LW_PFB_FBPA_FBIO_CFG1);
    REG_RD32(FECS, LW_PFB_FBPA_FBIO_CFG1);

    // Do not Program MCLK and REFMPLL in Turing

    return GCX_GC6_OK;
}

/*!
 * @brief function to disable FB realted power/clock for FBSR entry
 */
GCX_GC6_ENTRY_STATUS
fbGc6EntryClockDisable_GA10X()
{
    LwU32 val = REG_RD32(FECS,LW_PFB_FBPA_SW_CONFIG);

    //
    // Need to guarantee the sequence so blocking access all the registers
    //

    if (FLD_TEST_DRF(_PFB_FBPA, _SW_CONFIG, _PTRIM_OPT, _INIT, val))
    {
        // 1. FBIO_MODE_SWITCH_REFMPLL_STOP_SYNCMUX  = 1
        val = REG_RD32(FECS, LW_PTRIM_SYS_FBIO_MODE_SWITCH);
        val = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH, _REFMPLL_STOP_SYNCMUX, _ENABLE, val);
        REG_WR32_STALL(FECS, LW_PTRIM_SYS_FBIO_MODE_SWITCH, val);

        // 2. LW_PFB_FBPA_FBIO_REFMPLL_CONFIG_BYPASS = 1
        val = REG_RD32(FECS, LW_PFB_FBPA_FBIO_REFMPLL_CONFIG);
        val = FLD_SET_DRF(_PFB, _FBPA_FBIO_REFMPLL_CONFIG, _BYPASS, _ENABLE, val);
        REG_WR32_STALL(FECS, LW_PFB_FBPA_FBIO_REFMPLL_CONFIG, val);

        // 3. FBIO_MODE_SWITCH_REFMPLL_STOP_SYNCMUX  = 0
        val = REG_RD32(FECS, LW_PTRIM_SYS_FBIO_MODE_SWITCH);
        val = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH, _REFMPLL_STOP_SYNCMUX, _DISABLE, val);
        REG_WR32_STALL(FECS, LW_PTRIM_SYS_FBIO_MODE_SWITCH, val);

        // 4. LW_PFB_FBPA_REFMPLL_CFG_ENABLE         = 0
        val = REG_RD32(FECS, LW_PFB_FBPA_REFMPLL_CFG);
        val = FLD_SET_DRF(_PFB, _FBPA_REFMPLL_CFG, _ENABLE, _NO, val);
        REG_WR32_STALL(FECS, LW_PFB_FBPA_REFMPLL_CFG, val);

        // 5. FBIO_MODE_SWITCH_REFMPLL_STOP_SYNCMUX  = 1
        val = REG_RD32(FECS, LW_PTRIM_SYS_FBIO_MODE_SWITCH);
        val = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH, _REFMPLL_STOP_SYNCMUX, _ENABLE, val);
        REG_WR32_STALL(FECS, LW_PTRIM_SYS_FBIO_MODE_SWITCH, val);
    }
    else
    {
         // 1. FBIO_MODE_SWITCH_REFMPLL_STOP_SYNCMUX  = 1
        val = REG_RD32(FECS, LW_PFB_FBPA_FBIO_REFMPLL_CONFIG);
        val = FLD_SET_DRF(_PFB, _FBPA_FBIO_REFMPLL_CONFIG, _STOP_SYNCMUX, _ENABLE, val);
        REG_WR32_STALL(FECS, LW_PFB_FBPA_FBIO_REFMPLL_CONFIG, val);

        // 2. LW_PFB_FBPA_FBIO_REFMPLL_CONFIG_BYPASS = 1
        val = REG_RD32(FECS, LW_PFB_FBPA_FBIO_REFMPLL_CONFIG);
        val = FLD_SET_DRF(_PFB, _FBPA_FBIO_REFMPLL_CONFIG, _BYPASS, _ENABLE, val);
        REG_WR32_STALL(FECS, LW_PFB_FBPA_FBIO_REFMPLL_CONFIG, val);

        // 3. FBIO_MODE_SWITCH_REFMPLL_STOP_SYNCMUX  = 0
        val = REG_RD32(FECS, LW_PFB_FBPA_FBIO_REFMPLL_CONFIG);
        val = FLD_SET_DRF(_PFB, _FBPA_FBIO_REFMPLL_CONFIG, _STOP_SYNCMUX, _DISABLE, val);
        REG_WR32_STALL(FECS, LW_PFB_FBPA_FBIO_REFMPLL_CONFIG, val);

        // 4. LW_PFB_FBPA_REFMPLL_CFG_ENABLE         = 0
        val = REG_RD32(FECS, LW_PFB_FBPA_REFMPLL_CFG);
        val = FLD_SET_DRF(_PFB, _FBPA_REFMPLL_CFG, _ENABLE, _NO, val);
        REG_WR32_STALL(FECS, LW_PFB_FBPA_REFMPLL_CFG, val);

        // 5. FBIO_MODE_SWITCH_REFMPLL_STOP_SYNCMUX  = 1
        val = REG_RD32(FECS, LW_PFB_FBPA_FBIO_REFMPLL_CONFIG);
        val = FLD_SET_DRF(_PFB, _FBPA_FBIO_REFMPLL_CONFIG, _STOP_SYNCMUX, _ENABLE, val);
        REG_WR32_STALL(FECS, LW_PFB_FBPA_FBIO_REFMPLL_CONFIG, val);
    }

    return GCX_GC6_OK;
}

/*!
 * @brief Disable FB temperature tracking
 */
void
fbDisablingTemperatureTracking_GA10X()
{
    LwU32 val = REG_RD32(FECS, LW_PFB_FBPA_DQR_CTRL);

    val = FLD_SET_DRF(_PFB, _FBPA_DQR_CTRL, _EN_IC0_SUBP0, _INIT, val);
    val = FLD_SET_DRF(_PFB, _FBPA_DQR_CTRL, _EN_IC0_SUBP1, _INIT, val);
    val = FLD_SET_DRF(_PFB, _FBPA_DQR_CTRL, _EN_IC1_SUBP0, _INIT, val);
    val = FLD_SET_DRF(_PFB, _FBPA_DQR_CTRL, _EN_IC1_SUBP1, _INIT, val);

    REG_WR32_STALL(FECS, LW_PFB_FBPA_DQR_CTRL, val);
}

