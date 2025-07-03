/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_msswasrgm10x.c
 * @brief  SW-ASR specific HAL-interface for the GK10X
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_fb.h"
#include "dev_fbpa.h"
#include "dev_fbpa_addendum.h"
#include "dev_trim.h"
#include "dev_pri_ringstation_fbp.h"
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

/*!
 * @brief Gate DRAM clock
 *
 * This function gates DRAM clock from VCO and Bypass path. That is, it gates
 * clock from output of REFM PLL and OSM. Also it change DRAM clock source to
 * OSM.
 *
 * Standard practice of gating GPC/LTC/XRAB clocks is to switch them to ALT
 * path and gate them from ALT path. But for DRAM clock need to gate from ALT
 * and REF path before switching clock source.
 *
 * As per dislwssion with Mahipal and Preetham, DRAM clock will never source
 * from DRAM PLL in P8 and P5. (Reference bug #1357490) Thus, this function
 * doesnt considers the case of driving DRAM clock from DRAM PLL.
 */
void
msSwAsrDramClockGate_GM10X(void)
{
    LwU32 val;
    OBJSWASR *pSwAsr = MS_GET_SWASR();

    // Disable the DLL
    msSwAsrDramDisableDll_HAL();

    pSwAsr->regs.fbioModeSwitch = REG_RD32(FECS, LW_PTRIM_SYS_FBIO_MODE_SWITCH);
    // Gate DRAMCLK from reference path
    val = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH,
                      _REFMPLL_STOP_SYNCMUX, _ENABLE, pSwAsr->regs.fbioModeSwitch);

    // Gate DRAMCLK from bypass path
    val = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH,
                      _ONESOURCE_STOP, _ENABLE, val);

    REG_WR32(FECS, LW_PTRIM_SYS_FBIO_MODE_SWITCH, val);

    // Switch clock to bypass path
    val = REG_RD32(FECS, LW_PTRIM_SYS_FBIO_MODE_SWITCH);
    val = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH,
                      _DRAMCLK_MODE, _ONESOURCE, val);
    REG_WR32(FECS, LW_PTRIM_SYS_FBIO_MODE_SWITCH, val);

    // Read LW_PTRIM_SYS_FBIO_MODE_SWITCH flush all previous writes.
    val = REG_RD32(FECS, LW_PTRIM_SYS_FBIO_MODE_SWITCH);
}

/*!
 * @brief Ungate DRAM clock
 *
 * This function restores state of RFE and ALT path of DRAM clock.
 * It also restores clock source.
 */
void
msSwAsrDramClockUngate_GM10X(void)
{
    OBJSWASR *pSwAsr = MS_GET_SWASR();
    LwU32     val;
    LwU32     dramMode;
    LwU32     refmPllStop;
    LwU32     osmStop;


    // Get settings of DRAMCLK_MODE, REFMPLL_STOP_SYNCMUX and ONESOURCE_STOP
    dramMode    = DRF_VAL(_PTRIM, _SYS_FBIO_MODE_SWITCH,
                          _DRAMCLK_MODE, pSwAsr->regs.fbioModeSwitch);
    refmPllStop = DRF_VAL(_PTRIM, _SYS_FBIO_MODE_SWITCH,
                          _REFMPLL_STOP_SYNCMUX, pSwAsr->regs.fbioModeSwitch);
    osmStop     = DRF_VAL(_PTRIM, _SYS_FBIO_MODE_SWITCH,
                          _ONESOURCE_STOP, pSwAsr->regs.fbioModeSwitch);

    // Restore setting of DRAMCLK_MODE. i.e. Restore DRAM clock source path
    val = REG_RD32(FECS, LW_PTRIM_SYS_FBIO_MODE_SWITCH);
    val = FLD_SET_DRF_NUM(_PTRIM, _SYS_FBIO_MODE_SWITCH,
                          _DRAMCLK_MODE, dramMode, val);
    REG_WR32(FECS, LW_PTRIM_SYS_FBIO_MODE_SWITCH, val);

    // Restore settings of REFMPLL_STOP_SYNCMUX and ONESOURCE_STOP
    val = REG_RD32(FECS, LW_PTRIM_SYS_FBIO_MODE_SWITCH);
    val = FLD_SET_DRF_NUM(_PTRIM, _SYS_FBIO_MODE_SWITCH,
                          _REFMPLL_STOP_SYNCMUX, refmPllStop, val);
    val = FLD_SET_DRF_NUM(_PTRIM, _SYS_FBIO_MODE_SWITCH,
                          _ONESOURCE_STOP, osmStop, val);
    REG_WR32(FECS, LW_PTRIM_SYS_FBIO_MODE_SWITCH, val);

    // Read LW_PTRIM_SYS_FBIO_MODE_SWITCH flush all previous writes.
    val = REG_RD32(FECS, LW_PTRIM_SYS_FBIO_MODE_SWITCH);

    // Enable the DLL back again if required.
    msSwAsrDramEnableDll_HAL();
}

/*!
 * @brief Gate/Ungate DRAM clock if its driven from OSM
 *
 * This is helper function which Gates/Ungates DRAM clock, provided it sourced
 * from OSM. This function doesnt change clock source.
 */
void
msSwAsrDramClockGateFromBypass_GM10X(LwBool bGate)
{
    LwU32 val;

    val = REG_RD32(FECS, LW_PTRIM_SYS_FBIO_MODE_SWITCH);

    if (bGate)
    {
        val = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH,
                          _ONESOURCE_STOP, _ENABLE, val);
    }
    else
    {
        val = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH,
                          _ONESOURCE_STOP, _DISABLE, val);
    }

    REG_WR32(FECS, LW_PTRIM_SYS_FBIO_MODE_SWITCH, val);
}
