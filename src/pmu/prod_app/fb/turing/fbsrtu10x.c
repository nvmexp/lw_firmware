/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017 - 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   fbsrtu10x.c
 * @brief  FB self refresh Hal Functions for TU10X
 *
 * FBSR Hal Functions implement FB related functionalities for TU10X
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_fbpa.h"

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
 * @brief Get the TESTCMD self refresh entry values for GDDR5
 *
 * @param[out]    pEntry1   TESTCMD value for ENTRY1
 * @param[out]    pEntry2   TESTCMD value for ENTRY2
 */
void
fbGetSelfRefreshEntryValsGddr6_TU10X
(
    LwU32 *pEntry1,
    LwU32 *pEntry2
)
{
    *pEntry1 = DRF_DEF(_PFB_FBPA, _TESTCMD, _CKE,  _DDR5_SELF_REF_ENTRY1) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _CS0,  _DDR5_SELF_REF_ENTRY1) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _CS1,  _DDR5_SELF_REF_ENTRY1) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _RAS,  _DDR6_SELF_REF_ENTRY1) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _CAS,  _DDR6_SELF_REF_ENTRY1) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _WE,   _DDR6_SELF_REF_ENTRY1) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _RES,  _DDR5_SELF_REF_ENTRY1) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _BANK, _DDR5_SELF_REF_ENTRY1) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _ADR,  _DDR5_SELF_REF_ENTRY1) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _HOLD, _DDR5_SELF_REF_ENTRY1) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _GO,   _DDR5_SELF_REF_ENTRY1);

    *pEntry2 = DRF_DEF(_PFB_FBPA, _TESTCMD, _CKE,  _DDR5_SELF_REF_ENTRY2) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _CS0,  _DDR6_SELF_REF_ENTRY2) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _CS1,  _DDR6_SELF_REF_ENTRY2) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _RAS,  _DDR6_SELF_REF_ENTRY2) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _CAS,  _DDR6_SELF_REF_ENTRY2) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _WE,   _DDR6_SELF_REF_ENTRY2) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _RES,  _DDR5_SELF_REF_ENTRY2) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _BANK, _DDR5_SELF_REF_ENTRY2) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _ADR,  _DDR5_SELF_REF_ENTRY2) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _HOLD, _DDR5_SELF_REF_ENTRY2) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _GO,   _DDR5_SELF_REF_ENTRY2);
}

void
fbEnableSelfRefreshForGddr6_TU10X
(
    LwU32 *pEntry1,
    LwU32 *pEntry2
)
{
    LwU32 testcmdExt = DRF_DEF(_PFB_FBPA, _TESTCMD_EXT, _ACT, _INIT);

    // On GDDR6, need to clear TESTCMD_EXT and TESTCMD_EXT_1 first
    REG_WR32(FECS, LW_PFB_FBPA_TESTCMD_EXT, testcmdExt);
    REG_WR32(FECS, LW_PFB_FBPA_TESTCMD_EXT_1, 0x0);

    REG_WR32(FECS, LW_PFB_FBPA_TESTCMD, *pEntry1);
    REG_WR32(FECS, LW_PFB_FBPA_TESTCMD_EXT, testcmdExt);
    REG_WR32(FECS, LW_PFB_FBPA_TESTCMD, *pEntry2);
}

GCX_GC6_ENTRY_STATUS
fbEnterSrAndPowerDown_TU10X(LwU8 ramType)
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

    // Enable FBSR based on ram type
    fbEnableSelfRefresh_HAL(&Fb, ramType, &entry1, &entry2);

    // Issue a bunch of dummy FBIO reads to introduce a delay of 2 usec sim time.
    REG_RD32(FECS, LW_PFB_FBPA_FBIO_CFG1);
    REG_RD32(FECS, LW_PFB_FBPA_FBIO_CFG1);

    // Do not Program MCLK and REFMPLL in Turing

    return GCX_GC6_OK;
}

GCX_GC6_ENTRY_STATUS
fbGc6EntryClockDisable_TU10X()
{
    LwU32 val = 0;

    //
    // Need to guarantee the sequence so blocking access all the registers
    //

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

    return GCX_GC6_OK;
}

