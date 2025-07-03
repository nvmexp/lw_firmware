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
 * @file   pmu_msgm20x.c
 * @brief  HAL-interface for the GM20X Memory System Engine.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_lw_xve.h"
#include "dev_pwr_pri.h"
#include "dev_pwr_csb.h"
#include "dev_master.h"
#include "dev_top.h"
#include "dev_fuse.h"
#include "dev_fb.h"
#include "dev_trim.h"
#include "dev_pri_ringstation_sys.h"

/* ------------------------- Application Includes --------------------------- */
#include "lib_mutex.h"
#include "pmu_objms.h"
#include "pmu_objlpwr.h"
#include "pmu_objpmu.h"
#include "pmu_objclk.h"
#include "pmu_objfifo.h"
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
 * @brief Initializes the Holdoff mask for MS engine.
 */
void
msHoldoffMaskInit_GM20X(void)
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
    if (FIFO_IS_ENGINE_PRESENT(CE1))
    {
        hm |= BIT(GET_FIFO_ENG(PMU_ENGINE_CE1));
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
    if (FIFO_IS_ENGINE_PRESENT(ENC1))
    {
        hm |= BIT(GET_FIFO_ENG(PMU_ENGINE_ENC1));
    }
    if (FIFO_IS_ENGINE_PRESENT(SEC2))
    {
        hm |= BIT(GET_FIFO_ENG(PMU_ENGINE_SEC2));
    }

    pMsState->holdoffMask = hm;
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
msProgramDisallowRanges_GM20X(void)
{
    LwU32       barBlockerVal;
    LwU8        gpioMutexPhyId = MUTEX_LOG_TO_PHY_ID(LW_MUTEX_ID_GPIO);
    OBJPGSTATE *pMsState       = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_MS_MSCG);

    //
    // In the true sense LW_PPWR_PMU_MUTEX(gpioMutexPhyId) is not really
    // a Denylist register for MSCG. We have Denylisted GPIO Mutex register
    // so that any RM access to GPIO will disengage PSI by waking MSCG.
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_PSI_CTRL_MS) &&
        LPWR_CTRL_IS_SF_SUPPORTED(pMsState, MS, PSI))
    {
        barBlockerVal = REG_RD32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA);

        // Add register in CYA_DISALLOW_RANGE0
        REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA_DISALLOW_RANGE_START_ADDR(0),
                           LW_PPWR_PMU_MUTEX(gpioMutexPhyId));
        REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA_DISALLOW_RANGE_END_ADDR(0),
                           LW_PPWR_PMU_MUTEX(gpioMutexPhyId));

        // Enable DISALLOW_RANGE0
        barBlockerVal = FLD_SET_DRF(_XVE, _BAR_BLOCKER_CYA, _DISALLOW_RANGE0,
                                   _ENABLED, barBlockerVal);

        REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA, barBlockerVal);
    }
}
