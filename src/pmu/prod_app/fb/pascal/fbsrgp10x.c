/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   fbsrgp10x.c
 * @brief  FB self refresh Hal Functions for GP10X
 *
 * FBSR Hal Functions implement FB related functionalities for GP10X
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"
#include "dev_pwr_pri.h"
#include "dev_bus.h"
#include "dev_fbpa.h"
#include "dev_ltc.h"
#include "dev_trim.h"
#include "dev_master.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmu.h"
#include "pmu_objfifo.h"
#include "pmu_objfb.h"
#include "pmu_gc6.h"
#include "pmu_objpg.h"
#include "dbgprintf.h"
#include "hwproject.h"
#include "config/g_fb_private.h"
#include "ctrl/ctrl2080/ctrl2080fb.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
#define GCX_GC6_FB_L2_FLUSH_TIMEOUT_40_MSEC            (40*1000*1000) // 40ms for L2 flush
#define GCX_GC6_FB_L2_TSTAGE_DIRTY_TIMEOUT_40_MSEC     (40*1000*1000) // 40ms for tag stage clean
#define GCX_GC6_FB_PA_IDLE_TIMEOUT_40_MSEC             (40*1000*1000) // 40ms for fbpa idle

/* ------------------------- Prototypes ------------------------------------- */


/*!
 * @brief Get the TESTCMD self refresh entry values for GDDR5
 *
 * @param[out]    pEntry1   TESTCMD value for ENTRY1
 * @param[out]    pEntry2   TESTCMD value for ENTRY2
 */
void
fbGetSelfRefreshEntryValsGddr5_GP10X
(
    LwU32 *pEntry1,
    LwU32 *pEntry2
)
{
    *pEntry1 = DRF_DEF(_PFB_FBPA, _TESTCMD, _CKE,  _DDR5_SELF_REF_ENTRY1) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _CS0,  _DDR5_SELF_REF_ENTRY1) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _CS1,  _DDR5_SELF_REF_ENTRY1) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _RAS,  _DDR5_SELF_REF_ENTRY1) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _CAS,  _DDR5_SELF_REF_ENTRY1) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _WE,   _DDR5_SELF_REF_ENTRY1) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _RES,  _DDR5_SELF_REF_ENTRY1) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _BANK, _DDR5_SELF_REF_ENTRY1) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _ADR,  _DDR5_SELF_REF_ENTRY1) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _HOLD, _DDR5_SELF_REF_ENTRY1) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _GO,   _DDR5_SELF_REF_ENTRY1);

    *pEntry2 = DRF_DEF(_PFB_FBPA, _TESTCMD, _CKE,  _DDR5_SELF_REF_ENTRY2) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _CS0,  _DDR5_SELF_REF_ENTRY2) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _CS1,  _DDR5_SELF_REF_ENTRY2) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _RAS,  _DDR5_SELF_REF_ENTRY2) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _CAS,  _DDR5_SELF_REF_ENTRY2) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _WE,   _DDR5_SELF_REF_ENTRY2) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _RES,  _DDR5_SELF_REF_ENTRY2) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _BANK, _DDR5_SELF_REF_ENTRY2) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _ADR,  _DDR5_SELF_REF_ENTRY2) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _HOLD, _DDR5_SELF_REF_ENTRY2) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _GO,   _DDR5_SELF_REF_ENTRY2);
}

/*!
 * @brief Get the TESTCMD self refresh entry values for SDDR3.
 *
 * @param[out]    pEntry1   TESTCMD value for ENTRY1
 * @param[out]    pEntry2   TESTCMD value for ENTRY2
 */
void
fbGetSelfRefreshEntryValsSddr3_GP10X
(
    LwU32 *pEntry1,
    LwU32 *pEntry2
)
{
    *pEntry1 = DRF_DEF(_PFB_FBPA, _TESTCMD, _CKE,  _LEGACY_SELF_REF_ENTRY1) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _CS0,  _LEGACY_SELF_REF_ENTRY1) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _CS1,  _LEGACY_SELF_REF_ENTRY1) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _RAS,  _LEGACY_SELF_REF_ENTRY1) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _CAS,  _LEGACY_SELF_REF_ENTRY1) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _WE,   _LEGACY_SELF_REF_ENTRY1) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _RES,  _LEGACY_SELF_REF_ENTRY1) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _BANK, _LEGACY_SELF_REF_ENTRY1) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _ADR,  _LEGACY_SELF_REF_ENTRY1) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _HOLD, _LEGACY_SELF_REF_ENTRY1) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _GO,   _LEGACY_SELF_REF_ENTRY1);

    *pEntry2 = DRF_DEF(_PFB_FBPA, _TESTCMD, _CKE,  _LEGACY_SELF_REF_ENTRY2) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _CS0,  _LEGACY_SELF_REF_ENTRY2) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _CS1,  _LEGACY_SELF_REF_ENTRY2) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _RAS,  _LEGACY_SELF_REF_ENTRY2) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _CAS,  _LEGACY_SELF_REF_ENTRY2) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _WE,   _LEGACY_SELF_REF_ENTRY2) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _RES,  _LEGACY_SELF_REF_ENTRY2) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _BANK, _LEGACY_SELF_REF_ENTRY2) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _ADR,  _LEGACY_SELF_REF_ENTRY2) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _HOLD, _LEGACY_SELF_REF_ENTRY2) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _GO,   _LEGACY_SELF_REF_ENTRY2);
}

/*!
 * @brief Get the TESTCMD self refresh entry values for SDDR3.
 *        If cmd mapping is GDDR3_GT215_COMP_MIRR1, then
 *        TESTCMD_ADR value differs from GF100, else
 *        use GF100 values. (Bug 642189)
 *
 * @param[out]    pEntry1   TESTCMD value for ENTRY1
 * @param[out]    pEntry2   TESTCMD value for ENTRY2
 */
void
fbGetSelfRefreshEntryValsSddr3Gt215_GP10X
(
    LwU32 *pEntry1,
    LwU32 *pEntry2
)
{
    *pEntry1 = DRF_DEF(_PFB_FBPA, _TESTCMD, _CKE,  _LEGACY_SELF_REF_ENTRY1) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _CS0,  _LEGACY_SELF_REF_ENTRY1) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _CS1,  _LEGACY_SELF_REF_ENTRY1) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _RAS,  _LEGACY_SELF_REF_ENTRY1) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _CAS,  _LEGACY_SELF_REF_ENTRY1) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _WE,   _LEGACY_SELF_REF_ENTRY1) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _RES,  _LEGACY_SELF_REF_ENTRY1) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _BANK, _LEGACY_SELF_REF_ENTRY1) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _ADR,  _LEGACY_MIRR_SELF_REF_ENTRY1) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _HOLD, _LEGACY_SELF_REF_ENTRY1) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _GO,   _LEGACY_SELF_REF_ENTRY1);

    *pEntry2 = DRF_DEF(_PFB_FBPA, _TESTCMD, _CKE,  _LEGACY_SELF_REF_ENTRY2) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _CS0,  _LEGACY_SELF_REF_ENTRY2) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _CS1,  _LEGACY_SELF_REF_ENTRY2) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _RAS,  _LEGACY_SELF_REF_ENTRY2) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _CAS,  _LEGACY_SELF_REF_ENTRY2) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _WE,   _LEGACY_SELF_REF_ENTRY2) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _RES,  _LEGACY_SELF_REF_ENTRY2) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _BANK, _LEGACY_SELF_REF_ENTRY2) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _ADR,  _LEGACY_MIRR_SELF_REF_ENTRY2) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _HOLD, _LEGACY_SELF_REF_ENTRY2) |
               DRF_DEF(_PFB_FBPA, _TESTCMD, _GO,   _LEGACY_SELF_REF_ENTRY2);
}

/*!
 * @brief Enable DRAM self refresh.
 *
 * @param[in]     ramType   The VRAM type, passed-in by RM
 * @param[out]    pEntry1   TESTCMD value for ENTRY1
 * @param[out]    pEntry2   TESTCMD value for ENTRY2
 */
void
fbEnableSelfRefresh_GP10X
(
    LwU32  ramType,
    LwU32 *pEntry1,
    LwU32 *pEntry2
)
{
    LwU32 val = REG_RD32(FECS, LW_PFB_FBPA_FBIO_BROADCAST);

    switch (ramType)
    {
        case LW2080_CTRL_FB_INFO_RAM_TYPE_SDDR3:
        case LW2080_CTRL_FB_INFO_RAM_TYPE_SDDR4:
            if ((FLD_TEST_DRF(_PFB, _FBPA_FBIO_BROADCAST, _CMD_MAPPING, _GDDR3_GT215_COMP_MIRR1, val)))
            {
                fbGetSelfRefreshEntryValsSddr3Gt215_HAL(&Fb, pEntry1, pEntry2);
            }
            else
            {
                fbGetSelfRefreshEntryValsSddr3_HAL(&Fb, pEntry1, pEntry2);
            }
            break;
        case LW2080_CTRL_FB_INFO_RAM_TYPE_GDDR5:
        case LW2080_CTRL_FB_INFO_RAM_TYPE_GDDR5X:
        case LW2080_CTRL_FB_INFO_RAM_TYPE_HBM1:
        case LW2080_CTRL_FB_INFO_RAM_TYPE_HBM2:
            fbGetSelfRefreshEntryValsGddr5_HAL(&Fb, pEntry1, pEntry2);
            break;
        case LW2080_CTRL_FB_INFO_RAM_TYPE_GDDR6:
            fbGetSelfRefreshEntryValsGddr6_HAL(&Fb, pEntry1, pEntry2);
            break;
        default:
            // Need to have a proper error handling, given this is after GC6 link disable.
            PMU_HALT();
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
    if (ramType == LW2080_CTRL_FB_INFO_RAM_TYPE_GDDR6)
    {
        fbEnableSelfRefreshForGddr6_HAL(&Fb, pEntry1, pEntry2);
    }
    else
    {
        // Write LW_PFB_FBPA_TESTCMD to enable self-refresh.
        REG_WR32(FECS, LW_PFB_FBPA_TESTCMD, *pEntry1);
        REG_WR32(FECS, LW_PFB_FBPA_TESTCMD, *pEntry2);
    }
}

/*!
 * @brief Flush all the L2 rams/caches using the ELPG flush
 *
 * The ELPG flush is a comprehensive flush that of L2/FBP that hits all caches
 * and RAMS. It sequences a CBC flush followed by a L2 data flush.
 */
GCX_GC6_ENTRY_STATUS
fbFlushL2AllRamsAndCaches_GP10X(void)
{
    FLCN_TIMESTAMP         lwrrentTime;
    GCX_GC6_ENTRY_STATUS   gc6EntryStatus = GCX_GC6_OK;

    //L2 flush
    REG_WR32(FECS, LW_PLTCG_LTCS_LTSS_G_ELPG,
             FLD_SET_DRF(_PLTCG, _LTCS_LTSS_G_ELPG, _FLUSH, _PENDING,
             REG_RD32(FECS, LW_PLTCG_LTCS_LTSS_G_ELPG)));

    osPTimerTimeNsLwrrentGet(&lwrrentTime);
    while (FLD_TEST_DRF(_PLTCG, _LTCS_LTSS_G_ELPG,
                        _FLUSH, _PENDING,
                        REG_RD32(FECS, LW_PLTCG_LTCS_LTSS_G_ELPG)))
    {
        if ((osPTimerTimeNsElapsedGet(&lwrrentTime)) > GCX_GC6_FB_L2_FLUSH_TIMEOUT_40_MSEC)
        {
            pgGc6LogErrorStatus(GCX_GC6_L2_FLUSH_TIMEOUT);
            break;
        }

        OS_PTIMER_SPIN_WAIT_NS(500);
    }
    return gc6EntryStatus;
}


/*!
 * @brief Assert that FB and L2 are idle.
 */
GCX_GC6_ENTRY_STATUS
fbForceIdle_GP10X(void)
{
    LwU32                  val;
    LwU32                  ltc;
    LwU32                  ltcOffset;
    LwU32                  numActiveLTCs;
    FLCN_TIMESTAMP         lwrrentTime;
    GCX_GC6_ENTRY_STATUS   gc6EntryStatus = GCX_GC6_OK;

    // Wait for previous L2 flushes to complete.
    numActiveLTCs = fbGetActiveLTCCount_HAL();

    for (ltc = 0; ltc < numActiveLTCs; ++ltc)
    {
        ltcOffset = ltc * LW_LTC_PRI_STRIDE;

        osPTimerTimeNsLwrrentGet(&lwrrentTime);
        do
        {
            OS_PTIMER_SPIN_WAIT_NS(500);

            //
            // L2 HW has confirmed that virtual LTC indexes are used with this
            // register.
            //
            val = REG_RD32(FECS, LW_PLTCG_LTC0_LTSS_TSTG_STATUS + ltcOffset);

            if ((osPTimerTimeNsElapsedGet(&lwrrentTime)) > GCX_GC6_FB_L2_TSTAGE_DIRTY_TIMEOUT_40_MSEC)
            {
                pgGc6LogErrorStatus(GCX_GC6_L2_TSTAGE_CLEAN_TIMEOUT);
                break;
            }

        } while (!FLD_TEST_DRF_NUM(_PLTCG, _LTCS_LTSS_TSTG_STATUS, _DIRTY, 0, val));
    }

    // Wait for FB status to go idle.
    osPTimerTimeNsLwrrentGet(&lwrrentTime);
    do
    {
        val = REG_RD32(FECS, LW_PPWR_PMU_IDLE_STATUS);
        OS_PTIMER_SPIN_WAIT_NS(500);

        if ((osPTimerTimeNsElapsedGet(&lwrrentTime)) > GCX_GC6_FB_PA_IDLE_TIMEOUT_40_MSEC)
        {
            pgGc6LogErrorStatus(GCX_GC6_FBPA_IDLE_TIMEOUT);
            break;
        }

    } while (!FLD_TEST_DRF_NUM(_PPWR_PMU, _IDLE_STATUS, _FB_PA, 1, val));

    // Disable temperature tracking once FB is idle
    fbDisablingTemperatureTracking_HAL();

    return gc6EntryStatus;
}


// This code does not compile on HBM-supported builds.
#if PMUCFG_FEATURE_ENABLED(PMU_FB_DDR_SUPPORTED)

GCX_GC6_ENTRY_STATUS
fbEnterSrAndPowerDown_GP10X(LwU8 ramType)
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

    // Switch MCLK to ONESRC mode.
    val = REG_RD32(FECS, LW_PTRIM_SYS_FBIO_MODE_SWITCH);

    // Stop REFMPLL and ONESRC.
    val = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH, _CLAMP_P0, _ENABLE, val);
    val = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH, _REFMPLL_STOP_SYNCMUX, _ENABLE, val);
    val = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH, _ONESOURCE_STOP, _ENABLE, val);
    REG_WR32(FECS, LW_PTRIM_SYS_FBIO_MODE_SWITCH, val);

    // Switch to ONESRC.
    val = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH, _DRAMCLK_MODE, _ONESOURCE, val);
    REG_WR32(FECS, LW_PTRIM_SYS_FBIO_MODE_SWITCH, val);

    // Unstop ONESRC.
    val = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH, _ONESOURCE_STOP, _DISABLE, val);
    REG_WR32(FECS, LW_PTRIM_SYS_FBIO_MODE_SWITCH, val);

    // Disable REFMPLL.
    val = REG_RD32(FECS, LW_PFB_FBPA_REFMPLL_CFG);
    val = FLD_SET_DRF(_PFB, _FBPA_REFMPLL_CFG, _ENABLE, _NO, val);
    REG_WR32(FECS, LW_PFB_FBPA_REFMPLL_CFG, val);

    return GCX_GC6_OK;
}

/**
 * @brief Prepare FB for fb_clamp signal to be asserted.
 */
GCX_GC6_ENTRY_STATUS
fbPreGc6Clamp_GP10X(void)
{
    LwU32 val;

    // Disable serializer.
    val = REG_RD32(FECS, LW_PFB_FBPA_FBIO_SPARE);
    val |= BIT(12);
    REG_WR32(FECS, LW_PFB_FBPA_FBIO_SPARE, val);

    //
    // TODO SC, this extra BAR0 access is needed per experiment results.
    // MODS GC6 test can pass 10000 rounds.
    //
    REG_WR32(BAR0, LW_PFB_FBPA_FBIO_SPARE, val);

    // Stop ONESRC MCLK.
    val = REG_RD32(FECS, LW_PTRIM_SYS_FBIO_MODE_SWITCH);
    val = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH, _ONESOURCE_STOP, _ENABLE, val);
    REG_WR32(FECS, LW_PTRIM_SYS_FBIO_MODE_SWITCH, val);

    return GCX_GC6_OK;
}

#endif      // PMUCFG_FEATURE_ENABLED(PMU_FB_DDR_SUPPORTED)
