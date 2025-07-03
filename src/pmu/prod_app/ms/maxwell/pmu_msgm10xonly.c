/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2012-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_msgm10xonly.c
 * @brief  HAL-interface for the GM10X Memory System Engine.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_pwr_csb.h"
#include "dev_pwr_csb_addendum.h"
#include "dev_pwr_pri.h"
#include "dev_master.h"
#include "dev_top.h"
#include "dev_fuse.h"
#include "dev_fb.h"
#include "dev_fbpa.h"
#include "dev_fbpa_addendum.h"
#include "dev_lw_xve.h"
#include "dev_bus.h"
#include "dev_trim.h"
#include "dev_pri_ringstation_sys.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objms.h"
#include "pmu_objpmu.h"
#include "pmu_objclk.h"
#include "pmu_objfifo.h"
#include "pmu_objfuse.h"
#include "pmu_bar0.h"
#include "lib_mutex.h"

#include "dbgprintf.h"
#include "config/g_ms_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */

// On GM10X, MSCG only gates GPC and XBAR clock
#define MS_SEL_VCO_ALLCLK_OUT_BYPASS                                          \
        (                                                                     \
            FLD_SET_DRF(_PTRIM_SYS, _SEL_VCO, _GPC2CLK_OUT, _BYPASS, 0)      |\
            FLD_SET_DRF(_PTRIM_SYS, _SEL_VCO, _XBAR2CLK_OUT, _BYPASS, 0)      \
        )

#define MS_SEL_VCO_ALLCLK_OUT_BYPASS_MASK                          (0xffffffff)

#define MS_SEL_VCO_ALLCLK_OUT_BYPASS_TIMEOUT_NS                         (10000)
/* ------------------------ Static Variables -------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* --------------------------- Prototypes ------------------------------------*/
/* ------------------------ Public Functions  ------------------------------- */

/*!
 * Initializes the idle mask for MS engine.
 */
void
msMscgIdleMaskInit_GM10X(void)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_MS_MSCG);
    LwU32       im0      = 0;
    LwU32       im1      = 0;

    if (FIFO_IS_ENGINE_PRESENT(GR))
    {
        im0 = (FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _GR, _ENABLED, im0)     |
                FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _GR_SYS, _ENABLED, im0) |
                FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _GR_GPC, _ENABLED, im0) |
                FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _GR_BE, _ENABLED, im0));

        im1 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _GR_INTR_NOSTALL,
                            _ENABLED, im1);
    }
    if (FIFO_IS_ENGINE_PRESENT(CE0))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _CE_0, _ENABLED, im0);

        im1 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _CE0_INTR_NOSTALL,
                          _ENABLED, im1);
    }
    if (FIFO_IS_ENGINE_PRESENT(CE2))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _CE_2, _ENABLED, im0);

        im1 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _CE2_INTR_NOSTALL,
                            _ENABLED, im1);
    }
    //  LWDEC = BSP  (reusing MSVLD code for LWDEC)
    if (FIFO_IS_ENGINE_PRESENT(BSP))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _VD_LWDEC, _ENABLED, im0);

        im1 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _VD_LWDEC_INTR_NOSTALL,
                          _ENABLED, im1);
    }
    if (FIFO_IS_ENGINE_PRESENT(ENC0))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _VD_LWENC0, _ENABLED, im0);

        im1 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _VD_LWENC0_INTR_NOSTALL,
                          _ENABLED, im1);
    }

    if (FIFO_IS_ENGINE_PRESENT(SEC2))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _SEC, _ENABLED, im0);

        im1 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _SEC_INTR_NOSTALL,
                          _ENABLED, im1);
    }
    im1 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _MS_NISO_EX_ISO, _ENABLED, im1);

    pPgState->idleMask[0] = im0;
    pPgState->idleMask[1] = im1;
}

/*!
 * Initializes the Holdoff mask for MS engine.
 */
void
msHoldoffMaskInit_GM10X(void)
{
    OBJPGSTATE *pMsState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_MS_MSCG);
    LwU32       hm       = 0;

    if (FIFO_IS_ENGINE_PRESENT(GR))
    {
        hm |= BIT(GET_FIFO_ENG(PMU_ENGINE_GR));
    }
    if (FIFO_IS_ENGINE_PRESENT(CE0))
    {
        hm |= BIT(GET_FIFO_ENG(PMU_ENGINE_CE0));
    }
    if (FIFO_IS_ENGINE_PRESENT(CE2))
    {
        hm |= BIT(GET_FIFO_ENG(PMU_ENGINE_CE2));
    }
    //  LWDEC = BSP  (reusing MSVLD code for LWDEC)
    if (FIFO_IS_ENGINE_PRESENT(BSP))
    {
        hm |= BIT(GET_FIFO_ENG(PMU_ENGINE_BSP));
    }
    if (FIFO_IS_ENGINE_PRESENT(ENC0))
    {
        hm |= BIT(GET_FIFO_ENG(PMU_ENGINE_ENC0));
    }

    if (FIFO_IS_ENGINE_PRESENT(SEC2))
    {
        hm |= BIT(GET_FIFO_ENG(PMU_ENGINE_SEC2));
    }

    pMsState->holdoffMask = hm;
}

/*!
 * Check if any non-stalling interrupt is pending on each engine.
 *
 * @return   LW_TRUE  if a interrupt is pending on any engine
 * @return   LW_FALSE otherwise
 */
LwBool
msIsEngineIntrPending_GM10X(void)
{
    LwU32       val          = 0;
    LwBool      bIntrPending = LW_FALSE;

    val = REG_RD32(BAR0, LW_PMC_INTR_1);
    bIntrPending = (FLD_TEST_DRF(_PMC, _INTR_1, _PGRAPH, _PENDING, val) ||
                    FLD_TEST_DRF(_PMC, _INTR_1, _LWDEC,  _PENDING, val) ||
                    FLD_TEST_DRF(_PMC, _INTR_1, _LWENC,  _PENDING, val) ||
                    FLD_TEST_DRF(_PMC, _INTR_1, _SEC,    _PENDING, val) ||
                    FLD_TEST_DRF(_PMC, _INTR_1, _CE0,    _PENDING, val) ||
                    FLD_TEST_DRF(_PMC, _INTR_1, _CE2,    _PENDING, val));

    return bIntrPending;
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
void msInitActiveFbios_GM10X(void)
{
    LwU32     fbioFloorsweptMask;
    LwU32     fbioValidMask;
    LwU32     val;
    OBJSWASR *pSwAsr = MS_GET_SWASR();

    val = REG_RD32(BAR0, LW_PFB_FBHUB_NUM_ACTIVE_FBPS);
    pSwAsr->bMixedMemDensity = FLD_TEST_DRF(_PFB, _FBHUB_NUM_ACTIVE_FBPS,
                                            _MIXED_MEM_DENSITY, _ON, val);

    val = fuseRead(LW_FUSE_STATUS_OPT_FBIO);
    fbioFloorsweptMask =
        DRF_VAL(_FUSE, _STATUS_OPT_FBIO, _DATA, val);

    fbioValidMask =
        BIT(REG_RD32(BAR0, LW_PTOP_SCAL_NUM_FBPS)) - 1;

    pSwAsr->regs.mmActiveFBIOs =
        (~fbioFloorsweptMask) & fbioValidMask;
}

/*!
 * @brief Program and enable Disallow Ranges
 *
 * Disallow Range allows us to Denylist a register or a range which has been
 * mistakenly Allowlisted in the XVE blocker by HW. This is essentially a CYA.
 *
 * Denylisting register range is two step process -
 * 1) Add start and end address in DISALLOW_RANGE_START_ADDR_ADDR(i) and
 *    DISALLOW_RANGE_END_ADDR_ADDR(i)
 * 2) Enable DISALLOW_RANGEi
 */
void
msProgramDisallowRanges_GM10X(void)
{
    LwU32       barBlockerVal;
    LwU8        gpioMutexPhyId = MUTEX_LOG_TO_PHY_ID(LW_MUTEX_ID_GPIO);
    OBJPGSTATE *pMsState       = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_MS_MSCG);

    barBlockerVal = REG_RD32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA);

    // Add register in CYA_DISALLOW_RANGE0
    REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA_DISALLOW_RANGE_START_ADDR(0),
                       LW_PBUS_BAR2_BLOCK);
    REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA_DISALLOW_RANGE_END_ADDR(0),
                       LW_PBUS_BAR2_BLOCK);

    // Enable DISALLOW_RANGE0
    barBlockerVal = FLD_SET_DRF(_XVE, _BAR_BLOCKER_CYA, _DISALLOW_RANGE0,
                               _ENABLED, barBlockerVal);

    //
    // In the true sense LW_PPWR_PMU_MUTEX(gpioMutexPhyId) is not really
    // a Denylist register for MSCG. We have Denylisted GPIO Mutex register
    // so that any RM access to GPIO will disengage PSI by waking MSCG.
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_PSI_CTRL_MS) &&
        LPWR_CTRL_IS_SF_SUPPORTED(pMsState, MS, PSI))
    {
        // Add register in CYA_DISALLOW_RANGE1
        REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA_DISALLOW_RANGE_START_ADDR(1),
                           LW_PPWR_PMU_MUTEX(gpioMutexPhyId));
        REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA_DISALLOW_RANGE_END_ADDR(1),
                           LW_PPWR_PMU_MUTEX(gpioMutexPhyId));

        // Enable DISALLOW_RANGE1
        barBlockerVal = FLD_SET_DRF(_XVE, _BAR_BLOCKER_CYA, _DISALLOW_RANGE1,
                                   _ENABLED, barBlockerVal);
    }

    //
    // Reference Bug 1372848. Due to SEC/LWDEC engine behaviour in GM10X, upon reset,
    // engines will report non-idle. If MSCG was engaged and SEC/LWDEC are reset, then
    // they report non-idle, resulting in PG controller CFG error during MSCG wakeup.
    // If future WAR need CYA ranges,  then blocking PMC_ENABLE and disengaging 
    // MSCG can be moved to RM thus making CYA range available.
    //
    // Add register in CYA_DISALLOW_RANGE2
    REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA_DISALLOW_RANGE_START_ADDR(2),
                       LW_PMC_ENABLE);
    REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA_DISALLOW_RANGE_END_ADDR(2),
                       LW_PMC_ENABLE);

    // Enable DISALLOW_RANGE2
    barBlockerVal = FLD_SET_DRF(_XVE, _BAR_BLOCKER_CYA, _DISALLOW_RANGE2,
                               _ENABLED, barBlockerVal);

    REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA, barBlockerVal);
}

/*!
 * Check if host interrupt is pending
 *
 * @return   LW_TRUE  if a host interrupt is pending
 * @return   LW_FALSE otherwise
 *
 * TODO: This function is duplicate of msIsHostIntrPending_GMXXX.
 * For now i am submitting this CL. In follow up CL we will have a
 * clean implementation.
 */
LwBool
msIsHostIntrPending_GM10X(void)
{
    if (REG_FLD_TEST_DRF_DEF(BAR0, _PMC, _INTR_0, _PFIFO, _PENDING) ||
        REG_FLD_TEST_DRF_DEF(BAR0, _PMC, _INTR_1, _PFIFO, _PENDING) ||
        REG_FLD_TEST_DRF_DEF(BAR0, _PMC, _INTR_2, _PFIFO, _PENDING))
    {
        return LW_TRUE;
    }
    return LW_FALSE;
}
