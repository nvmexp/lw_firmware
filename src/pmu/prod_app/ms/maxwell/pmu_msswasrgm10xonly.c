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
 * @file   pmu_msswasrgm10xonly.c
 * @brief  HAL-interface for the GM10X Memory System Engine.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_fb.h"
#include "dev_fbpa_addendum.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objms.h"
#include "pmu_swasr.h"
#include "pmu_objpmu.h"

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
 * @brief  Power Down COMP Pads
 *
 * On kepler COMP pad did not have bypass regulator.
 * Main regulator was always ON. Maxwell-GM107 - COMP
 * pad has a bypass regulator which can be used in MSCG/GC6 
 * GM108 - VTTGEN of neighboring data brick is used,
 * data brick goes on bypass regulator and that puts COMP 
 * pad as well into powerdown. i.e when we go into GC6/MSCG 
 * Maxwell circuit change was to save additional GC6 power. 
 * That circuit change has resulted in sequence change. 
 *
 * Steps 1) Disable AUTOCAL Primary
 *       2) Power down COMP pads
 */
void
msSwAsrCompPadPwrDown_GM10X(void)
{
    LwU32     regVal;
    OBJSWASR *pSwAsr = MS_GET_SWASR();

    // This function should be a NOP for any chips other than GM108.
    if (!IsGPU(_GM108))
    {
        return;
    }

    // Disable AUTOCAL Primary
    pSwAsr->regs.fbFbioCalmasterCfg = 
        REG_RD32(FECS, LW_PFB_FBIO_CALMASTER_CFG);
    regVal = FLD_SET_DRF(_PFB, _FBIO_CALMASTER_CFG, _AUTOCOMP,
                         _DISABLE, pSwAsr->regs.fbFbioCalmasterCfg);
    REG_WR32(FECS, LW_PFB_FBIO_CALMASTER_CFG, regVal);

    // Power down COMP pad VTTGEN
    pSwAsr->regs.fbFbioCalgroupVttgelwauxgen = 
        REG_RD32(FECS, LW_PFB_FBIO_CALGROUP_VTTGEN_VAUXGEN);
    regVal = FLD_SET_DRF(_PFB, _FBIO_CALGROUP_VTTGEN_VAUXGEN,
                         _VTTGEN_PWRD, _INIT, 
                         pSwAsr->regs.fbFbioCalgroupVttgelwauxgen);
    REG_WR32(FECS, LW_PFB_FBIO_CALGROUP_VTTGEN_VAUXGEN, regVal);
}

/*!
 * @brief  Power up COMP Pads
 *
 * Steps 1) Power up COMP pads
 *       2) Enable AUTOCAL Primary
 *       3) Wait for 0.5uSec & then trigger calibration cycle
 */
void
msSwAsrCompPadPwrUp_GM10X(void)
{
    LwU32     regVal = 0;
    OBJSWASR *pSwAsr = MS_GET_SWASR();

    // This function should be a NOP for any chips other than GM108.
    if (!IsGPU(_GM108))
    {
        return;
    }

    // Power Up COMP pad VTTGEN
    REG_WR32(FECS, LW_PFB_FBIO_CALGROUP_VTTGEN_VAUXGEN, 
        pSwAsr->regs.fbFbioCalgroupVttgelwauxgen);

    // Enable AUTOCAL Primary
    REG_WR32(FECS, LW_PFB_FBIO_CALMASTER_CFG, 
        pSwAsr->regs.fbFbioCalmasterCfg);

    //
    // Wait for .5uSec to let Calibration engine settle. The delay has been
    // provided by Qing from MED team & is precautionary.
    //
    OS_PTIMER_SPIN_WAIT_NS(500);

    //
    // Bug- 1349345 - Trigger an Auto Calibration cycle to
    // make sure the engine output has sane values for pads.
    // This is  required as in-case of a PState change to higher
    // Mclk, the calibration engine values will not be good enough
    // & this will cause FB to go in bad state.
    // This cycle should complete in 52uSec but worst case should
    // not be more than 70-80uSec as per bug given above.
    //
    regVal = REG_RD32(FECS, LW_PFB_FBIO_CALMASTER_CFG2);
    regVal = FLD_SET_DRF(_PFB, _FBIO_CALMASTER_CFG2, _TRIGGER_CAL_CYCLE,
                         _ENABLED, regVal);
    REG_WR32(FECS, LW_PFB_FBIO_CALMASTER_CFG2, regVal);

    //
    // Poll for CAL_CYCLE to fall back to "0" which will ensure that
    // calibration cycles has been triggred.
    //
    if (!PMU_REG32_POLL_NS(
        USE_FECS(LW_PFB_FBIO_CALMASTER_CFG2),
        DRF_SHIFTMASK(LW_PFB_FBIO_CALMASTER_CFG2_TRIGGER_CAL_CYCLE),
        DRF_DEF(_PFB, _FBIO_CALMASTER_CFG2, _TRIGGER_CAL_CYCLE, _INACTIVE),
        SWASR_AUTO_CALIB_TRIGGER_TIMEOUT_NS,
        PMU_REG_POLL_MODE_BAR0_EQ))
    {
        DBG_PRINTF_PG(("TO: Cal Cycle Trigger\n", 0, 0, 0, 0));
        PMU_HALT();
    }

    //
    // Reset the CALMASTER register which will be polled for 
    // status during SW-ASR exit to make sure calibration
    // is completed before MSCG exit
    //
    regVal = REG_RD32(FECS, LW_PFB_FBIO_CALMASTER_RB);
    regVal = FLD_SET_DRF(_PFB, _FBIO_CALMASTER_RB, _CAL_CYCLE,
                         _RESET, regVal);
    REG_WR32(FECS, LW_PFB_FBIO_CALMASTER_RB, regVal);

    pSwAsr->bCalibCycleAsserted = LW_TRUE;
}
