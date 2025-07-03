/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_msgv11b.c
 * @brief  HAL-interface for the PASCAL and VOLTA Memory System Engine.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "hwproject.h"
#include "dev_lw_xve.h"
#include "dev_lw_xve1.h"
#include "dev_lw_xp.h"
#include "dev_master.h"
#include "dev_bus.h"
#include "dev_fifo.h"
#include "dev_timer.h"
#include "dev_pbdma.h"
#include "dev_top.h"
#include "dev_disp.h"
#include "dev_perf.h"
#include "dev_hdafalcon_pri.h"
#include "dev_hdacodec.h"
#include "dev_pwr_pri.h"
#include "dev_fuse.h"
#include "dev_pmgr.h"
#include "mscg_wl_bl_init.h"
#include "dev_sec_pri.h"
#include "dev_pwr_csb.h"
#include "dev_top.h"
#include "dev_trim.h"
#include "dev_sec_pri.h"
#include "dev_fbpa.h"
#include "dev_therm.h"
#include "dev_flush.h"
#include "dev_ram.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objlpwr.h"
#include "pmu_objms.h"
#include "pmu_objpmu.h"
#include "pmu_objfifo.h"
#include "pmu_objic.h"
#include "pmu_objfuse.h"
#include "pmu_bar0.h"

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
 * Check if host interrupt is pending
 *
 * @return   LW_TRUE  if a host interrupt is pending
 * @return   LW_FALSE otherwise
 */
LwBool
msIsHostIntrPending_GV11B(void)
{
    // for Pascal LW_PMC_INTR__SIZE_1 == 2
    if (REG_FLD_IDX_TEST_DRF_DEF(BAR0, _PMC, _INTR, 0, _PFIFO, _PENDING) ||
        REG_FLD_IDX_TEST_DRF_DEF(BAR0, _PMC, _INTR, 1, _PFIFO, _PENDING))
    {
        return LW_TRUE;
    }
    return LW_FALSE;
}

/*!
 * @brief Program and enable Disallow Ranges
 *
 * Disallow Range allows us to Disallow a register or a range which has been
 * mistakenly allowed in the XVE blocker by HW. This is essentially a CYA.
 *
 * Disallowing register range is two step process -
 * 1) Add start and end address in DISALLOW_RANGE_START_ADDR_ADDR(i) and
 *    DISALLOW_RANGE_END_ADDR_ADDR(i)
 * 2) Enable DISALLOW_RANGEi
 */
void
msProgramDisallowRanges_GV11B(void)
{
    LwU32       barBlockerVal;
    LwU8        gpioMutexPhyId = MUTEX_LOG_TO_PHY_ID(LW_MUTEX_ID_GPIO);
    OBJPGSTATE *pMsState       = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_MS_MSCG);

    barBlockerVal = REG_RD32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA);

    //
    // In the true sense LW_PPWR_PMU_MUTEX(gpioMutexPhyId) is not really
    // a Disallow register for MSCG. We have disallowed GPIO Mutex register
    // so that any RM access to GPIO will disengage PSI by waking MSCG.
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_PSI_CTRL_MS) &&
        LPWR_CTRL_IS_SF_SUPPORTED(pMsState, MS, PSI))
    {
        // Add register in CYA_DISALLOW_RANGE0
        REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA_DISALLOW_RANGE_START_ADDR(0),
                 LW_PPWR_PMU_MUTEX(gpioMutexPhyId));
        REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA_DISALLOW_RANGE_END_ADDR(0),
                 LW_PPWR_PMU_MUTEX(gpioMutexPhyId));

        // Enable DISALLOW_RANGE0
        barBlockerVal = FLD_SET_DRF(_XVE, _BAR_BLOCKER_CYA, _DISALLOW_RANGE0,
                                    _ENABLED, barBlockerVal);
    }

    //
    // All the below BL ranges are derived from the Bug: 200354284 comment #10.
    // All these ranges are provided by HOST HW Folks.
    //
    // Add register LW_PFIFO_ERROR_SCHED_DISABLE, LW_PFIFO_ERROR_SCHED_DISABLE,
    // LW_PFIFO_RUNLIST_PREEMPT, in CYA_DISALLOW_RANGE1
    //
    REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA_DISALLOW_RANGE_START_ADDR(1),
             LW_PFIFO_ERROR_SCHED_DISABLE);
    REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA_DISALLOW_RANGE_END_ADDR(1),
             LW_PFIFO_RUNLIST_PREEMPT);

    // Enable DISALLOW_RANGE1
    barBlockerVal = FLD_SET_DRF(_XVE, _BAR_BLOCKER_CYA, _DISALLOW_RANGE1,
                                _ENABLED, barBlockerVal);

    // Add register space LW_PCCSR in CYA_DISALLOW_RANGE2
    REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA_DISALLOW_RANGE_START_ADDR(2),
             DEVICE_BASE(LW_PCCSR));
    REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA_DISALLOW_RANGE_END_ADDR(2),
             DEVICE_EXTENT(LW_PCCSR));

    // Enable DISALLOW_RANGE2
    barBlockerVal = FLD_SET_DRF(_XVE, _BAR_BLOCKER_CYA, _DISALLOW_RANGE2,
                                _ENABLED, barBlockerVal);

    // Add register range LW_UFLUSH in CYA_DISALLOW_RANGE4
    REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA_DISALLOW_RANGE_START_ADDR(3),
             DEVICE_BASE(LW_UFLUSH));
    REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA_DISALLOW_RANGE_END_ADDR(3),
             DEVICE_EXTENT(LW_UFLUSH));

    // Enable DISALLOW_RANGE3
    barBlockerVal = FLD_SET_DRF(_XVE, _BAR_BLOCKER_CYA, _DISALLOW_RANGE3,
                                _ENABLED, barBlockerVal);

    // Add register range LW_PRAMIN in CYA_DISALLOW_RANGE5
    REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA_DISALLOW_RANGE_START_ADDR(4),
             DEVICE_BASE(LW_PRAMIN));
    REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA_DISALLOW_RANGE_END_ADDR(4),
             DEVICE_EXTENT(LW_PRAMIN));

    // Enable DISALLOW_RANGE4
    barBlockerVal = FLD_SET_DRF(_XVE, _BAR_BLOCKER_CYA, _DISALLOW_RANGE4,
                                _ENABLED, barBlockerVal);

    // Add register LW_PFIFO_FB_IFACE in CYA_DISALLOW_RANGE5
    REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA_DISALLOW_RANGE_START_ADDR(5),
             LW_PFIFO_FB_IFACE);
    REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA_DISALLOW_RANGE_END_ADDR(5),
             LW_PFIFO_FB_IFACE);

    // Enable DISALLOW_RANGE5
    barBlockerVal = FLD_SET_DRF(_XVE, _BAR_BLOCKER_CYA, _DISALLOW_RANGE5,
                                _ENABLED, barBlockerVal);

    REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA, barBlockerVal);
}

/*!
 * @brief       Extract Active FBIOs for Mixed-Mode memory configuration.
 *
 * @details     Per-partition registers (LW_PFB_FBPA_<num>) space gets handled
 *              differently from their corresponding broadcast registers. Per-
 *              partition registers get mapped to the corresponding physical FBP
 *              and FBIO. Thus, we must check "Floor Sweeping" status of FBP and
 *              FBIO before accessing these registers.
 *
 * @see         bug 653484
 * @see         https://wiki.lwpu.com/fermi/index.php/Fermi_Floorsweeping#FB_to_FBIO_connections
 */
void msInitActiveFbios_GV11B(void)
{
    LwU32     fbioFloorsweptMask;
    LwU32     fbioValidMask;
    LwU32     val;
    OBJSWASR *pSwAsr = MS_GET_SWASR();

    val = fuseRead(LW_FUSE_STATUS_OPT_FBIO);

    fbioFloorsweptMask =
        DRF_VAL(_FUSE, _STATUS_OPT_FBIO, _DATA, val);

    fbioValidMask =
        BIT(REG_RD32(BAR0, LW_PTOP_SCAL_NUM_FBPAS)) - 1;

    pSwAsr->regs.mmActiveFBIOs =
        (~fbioFloorsweptMask) & fbioValidMask;
}

/*!
 * @brief Initialize FB parameters
 */
void
msInitFbParams_GV11B(void)
{
    OBJSWASR *pSwAsr = MS_GET_SWASR();
    LwU32     val;

    val = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_BROADCAST);

    //
    // On Pascal we support both GDDR5 and GDDR5x memory types. Therefor setting
    // this varible if memory type is either GDDR5 or GDDR5x.
    //
    // Also we have same SW ASR sequence for both GDDR5 and GDDR5X.
    //
    pSwAsr->bIsGDDRx = (FLD_TEST_DRF(_PFB, _FBPA_FBIO_BROADCAST, _DDR_MODE,
                                     _GDDR5, val) ||
                        FLD_TEST_DRF(_PFB, _FBPA_FBIO_BROADCAST, _DDR_MODE,
                                     _GDDR5X, val));

    pSwAsr->bIsGDDR3MirrorCmdMapping =
        (FLD_TEST_DRF(_PFB, _FBPA_FBIO_BROADCAST, _CMD_MAPPING,
                      _GDDR3_GT215_COMP_MIRR, val )             |
         FLD_TEST_DRF(_PFB, _FBPA_FBIO_BROADCAST, _CMD_MAPPING,
                      _GDDR3_GT215_COMP_MIRR1, val));

    if (PMUCFG_FEATURE_ENABLED(PMU_MS_HALF_FBPA))
    {
        // Determine if Half-FBPA is enabled on the system
        msInitHalfFbpaMask_HAL();
    }
}

void
msEnableIntr_GV11B(void)
{
    LwU32 intrMask;

    appTaskCriticalEnter();
    {
        intrMask = Ms.intrRiseEnMask;

        // Make sure that all current non-MS interrupts will remain untouched
        intrMask |= REG_RD32(CSB, LW_CPWR_PMU_GPIO_1_INTR_RISING_EN);

        REG_WR32_STALL(CSB, LW_CPWR_PMU_GPIO_1_INTR_RISING_EN, intrMask);
    }
    appTaskCriticalExit();
}
