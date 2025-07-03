/*
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_gc6lwlinktu10x.c
 * @brief  PMU GC6 LWLink Hal Functions for TU10X
 *
 * Functions implement chip-specific power state (GCx) init and sequences for LWLink.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_gc6_island.h"
#include "dev_gc6_island_addendum.h"
#include "dev_ioctrlmif.h"
#include "dev_trim.h"
#include "dev_top.h"
#include "pmu_gc6.h"
#include "dev_master.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpg.h"
#include "config/g_pg_private.h"

/*
 * LWLINK L2 entry sequence
 */
void
pgGc6LwlinkL2Entry_TU10X(void)
{
// register removed in Hopper
#ifdef LW_PTRIM_SYS_AUX_TX_RDET_DIV
    LwU32 val;
    // gateVal set to 1 to gate all clocks
    LwU32 gateVal = 1;

    // Skip LWLINK registers on FMODEL as they are not modelled
    val = REG_RD32(BAR0, LW_PTOP_PLATFORM);
    if (!FLD_TEST_DRF(_PTOP, _PLATFORM, _TYPE, _FMODEL, val))
    {
        // Gate clocks
        val = REG_RD32(FECS, LW_PTRIM_SYS_LWLINK_FGC6_CLK_GATE);
        val = FLD_SET_DRF_NUM(_PTRIM, _SYS_LWLINK_FGC6_CLK_GATE, _SYSCLK_LNK_DISABLE, gateVal, val);
        REG_WR32(FECS, LW_PTRIM_SYS_LWLINK_FGC6_CLK_GATE, val);

        val = REG_RD32(FECS, LW_PTRIM_SYS_LWLINK_AUX_TX_RDET_DIV);
        val = FLD_SET_DRF_NUM(_PTRIM, _SYS_LWLINK_AUX_TX_RDET_DIV, _LX_AUX_TX_RDET_CLK_LWLINK_DIS_OVRD, gateVal, val);
        val = FLD_SET_DRF_NUM(_PTRIM, _SYS_LWLINK_AUX_TX_RDET_DIV, _LX_AUX_TX_RDET_CLK_LWLINK_DIS, gateVal, val);
        REG_WR32(FECS, LW_PTRIM_SYS_LWLINK_AUX_TX_RDET_DIV, val);
    }

    val = REG_RD32(FECS, LW_PTRIM_SYS_AUX_TX_RDET_DIV);
    val = FLD_SET_DRF_NUM(_PTRIM, _SYS_AUX_TX_RDET_DIV, _RX_BYPASS_REFCLK_DISABLE, gateVal, val);
    REG_WR32(FECS, LW_PTRIM_SYS_AUX_TX_RDET_DIV, val);

    val = REG_RD32(FECS, LW_PTRIM_SYS_FGC6_CLK_GATE);
    val = FLD_SET_DRF_NUM(_PTRIM, _SYS_FGC6_CLK_GATE, _MGMT_CLK_DISABLE, gateVal, val);
    REG_WR32(FECS, LW_PTRIM_SYS_FGC6_CLK_GATE, val);
    val = FLD_SET_DRF_NUM(_PTRIM, _SYS_FGC6_CLK_GATE, _ALT_LWL_COMMON_CLK_DISABLE, gateVal, val);
    REG_WR32(FECS, LW_PTRIM_SYS_FGC6_CLK_GATE, val);

    // Enable SCI LWLINK LATCH
    val = REG_RD32(FECS, LW_PGC6_SCI_FGC6_LWLINK);
    val = FLD_SET_DRF(_PGC6, _SCI_FGC6_LWLINK, _LATCH, _ENABLE, val);
    REG_WR32(FECS, LW_PGC6_SCI_FGC6_LWLINK, val);

    // Enable SCI LWLINK CLAMP
    val = REG_RD32(FECS, LW_PGC6_SCI_FGC6_LWLINK);
    val = FLD_SET_DRF(_PGC6, _SCI_FGC6_LWLINK, _CLAMP, _ENABLE, val);
    REG_WR32(FECS, LW_PGC6_SCI_FGC6_LWLINK, val);

    OS_PTIMER_SPIN_WAIT_NS(1000);

    val = REG_RD32(FECS, LW_PTRIM_SYS_FGC6_CLK_GATE);
    val = FLD_SET_DRF_NUM(_PTRIM, _SYS_FGC6_CLK_GATE, _MGMT_CLK_DISABLE, 0, val);
    REG_WR32(FECS, LW_PTRIM_SYS_FGC6_CLK_GATE, val);
    val = FLD_SET_DRF_NUM(_PTRIM, _SYS_FGC6_CLK_GATE, _ALT_LWL_COMMON_CLK_DISABLE, 0, val);
    REG_WR32(FECS, LW_PTRIM_SYS_FGC6_CLK_GATE, val);

    val = REG_RD32(FECS, LW_PTRIM_SYS_AUX_TX_RDET_DIV);
    val = FLD_SET_DRF_NUM(_PTRIM, _SYS_AUX_TX_RDET_DIV, _RX_BYPASS_REFCLK_DISABLE, 0, val);
    REG_WR32(FECS, LW_PTRIM_SYS_AUX_TX_RDET_DIV, val);
#endif
}

/*
 * LWLINK L2 exit sequence
 *
 * @param[in]   lwlinkMask  Mask of all enabled lwlinks
 *
 * @return      FLCN_OK     On success
 * @return      FLCN_ERROR  On unreachable switch default
 */
FLCN_STATUS
pgGc6LwlinkL2Exit_TU10X(LwU32 lwlinkMask)
{
    LwU32 i;
    //LwU32 val;
    LwU32 regAddr = 0;
    FLCN_STATUS status = FLCN_OK;

    //Check if we have any enabled lwlinks
    if (lwlinkMask == 0)
    {
        goto pgGc6LwlinkL2Exit_TU10X_exit;
    }
// Moved to Phase1. Commented out for documentation
/*
    // deassert lwlink reset before unclamp
    val = REG_RD32(BAR0, LW_PMC_ENABLE);
    val = FLD_SET_DRF(_PMC, _ENABLE, _LWLINK, _ENABLED, val);
    REG_WR32(BAR0, LW_PMC_ENABLE, val);

    // Gate clocks in SYS chiplet
    val = REG_RD32(FECS, LW_PTRIM_SYS_AUX_TX_RDET_DIV);
    val = FLD_SET_DRF_NUM(_PTRIM, _SYS_AUX_TX_RDET_DIV, _RX_BYPASS_REFCLK_DISABLE, 1, val);
    REG_WR32(FECS, LW_PTRIM_SYS_AUX_TX_RDET_DIV, val);

    val = REG_RD32(FECS, LW_PTRIM_SYS_FGC6_CLK_GATE);
    val = FLD_SET_DRF_NUM(_PTRIM, _SYS_FGC6_CLK_GATE, _MGMT_CLK_DISABLE, 1, val);
    REG_WR32(FECS, LW_PTRIM_SYS_FGC6_CLK_GATE, val);
    val = FLD_SET_DRF_NUM(_PTRIM, _SYS_FGC6_CLK_GATE, _ALT_LWL_COMMON_CLK_DISABLE, 1, val);
    REG_WR32(FECS, LW_PTRIM_SYS_FGC6_CLK_GATE, val);

    // Disable SCI LWLINK CLAMP
    val = REG_RD32(FECS, LW_PGC6_SCI_FGC6_LWLINK);
    val = FLD_SET_DRF(_PGC6, _SCI_FGC6_LWLINK, _CLAMP, _DISABLE, val);
    REG_WR32(FECS, LW_PGC6_SCI_FGC6_LWLINK, val);

    // Disable SCI LWLINK LATCH
    val = REG_RD32(FECS, LW_PGC6_SCI_FGC6_LWLINK);
    val = FLD_SET_DRF(_PGC6, _SCI_FGC6_LWLINK, _LATCH, _DISABLE, val);
    REG_WR32(FECS, LW_PGC6_SCI_FGC6_LWLINK, val);

    // Ungate clocks in SYS
    val = REG_RD32(FECS, LW_PTRIM_SYS_AUX_TX_RDET_DIV);
    val = FLD_SET_DRF_NUM(_PTRIM, _SYS_AUX_TX_RDET_DIV, _RX_BYPASS_REFCLK_DISABLE, 0, val);
    REG_WR32(FECS, LW_PTRIM_SYS_AUX_TX_RDET_DIV, val);

    val = REG_RD32(FECS, LW_PTRIM_SYS_FGC6_CLK_GATE);
    val = FLD_SET_DRF_NUM(_PTRIM, _SYS_FGC6_CLK_GATE, _MGMT_CLK_DISABLE, 0, val);
    REG_WR32(FECS, LW_PTRIM_SYS_FGC6_CLK_GATE, val);
    val = FLD_SET_DRF_NUM(_PTRIM, _SYS_FGC6_CLK_GATE, _ALT_LWL_COMMON_CLK_DISABLE, 0, val);
    REG_WR32(FECS, LW_PTRIM_SYS_FGC6_CLK_GATE, val);

    // Skip LWLINK registers in FMODEL as they are not modelled
    val = REG_RD32(BAR0, LW_PTOP_PLATFORM);
    if (!FLD_TEST_DRF(_PTOP, _PLATFORM, _TYPE, _FMODEL, val))
    {
        // Ungate clocks in LNK0
        val = REG_RD32(FECS, LW_PTRIM_SYS_LWLINK_FGC6_CLK_GATE);
        val = FLD_SET_DRF_NUM(_PTRIM, _SYS_LWLINK_FGC6_CLK_GATE, _SYSCLK_LNK_DISABLE, 0, val);
        REG_WR32(FECS, LW_PTRIM_SYS_LWLINK_FGC6_CLK_GATE, val);

        val = REG_RD32(FECS, LW_PTRIM_SYS_LWLINK_AUX_TX_RDET_DIV);
        val = FLD_SET_DRF_NUM(_PTRIM, _SYS_LWLINK_AUX_TX_RDET_DIV, _LX_AUX_TX_RDET_CLK_LWLINK_DIS_OVRD, 0, val);
        val = FLD_SET_DRF_NUM(_PTRIM, _SYS_LWLINK_AUX_TX_RDET_DIV, _LX_AUX_TX_RDET_CLK_LWLINK_DIS, 0, val);
        REG_WR32(FECS, LW_PTRIM_SYS_LWLINK_AUX_TX_RDET_DIV, val);
    }
*/
    // pulse bit for each lwlink for credit clear
    FOR_EACH_INDEX_IN_MASK(32, i, lwlinkMask)
    {
        switch (i)
        {
            case 0:
            {
                regAddr = LW_PIOCTRLMIF_SYS0_SCRATCH_PRIVMASK1;
                break;
            }
            case 1:
            {
                regAddr = LW_PIOCTRLMIF_SYS1_SCRATCH_PRIVMASK1;
                break;
            }
            // Invalid case.
            default:
            {
                status = FLCN_ERROR;
                PMU_BREAKPOINT();
                goto pgGc6LwlinkL2Exit_TU10X_exit;
            }
        }
        REG_WR32(FECS, regAddr, 0);
        REG_WR32(FECS, regAddr, 1);
    }
    FOR_EACH_INDEX_IN_MASK_END;

pgGc6LwlinkL2Exit_TU10X_exit:
    return status;
}

