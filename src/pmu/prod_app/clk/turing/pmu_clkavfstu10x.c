/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_clkavfstu10X.c
 * @brief  PMU Hal Functions for TU10X for Clocks AVFS
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_trim.h"

/* ------------------------- Application Includes --------------------------- */
#include "dbgprintf.h"
#include "pmu_objclk.h"
#include "pmu_objpmu.h"
#include "config/g_clk_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief Update the register that controls operation of the quick_slowdown
 * module based on SW support / capabilities.
 *
 * @return FLCN_OK                Operation completed successfully
 * @return FLCN_ERR_ILWALID_STATE If the slowdown control fields are not
 *                                consistent with devinit values
 */
FLCN_STATUS
clkNafllLutUpdateQuickSlowdown_TU10X(void)
{
    LwU32       data32;
    FLCN_STATUS status = FLCN_OK;

    // Read the devinit slowdown setting.
    data32 = REG_RD32(FECS, LW_PTRIM_GPC_BCAST_AVFS_QUICK_SLOWDOWN_CTRL);

    // If slow down is completely disabled, ensure that secondary VF lwrve is NOT supported in VBIOS.
    if (FLD_TEST_DRF(_PTRIM, _GPC_BCAST_AVFS_QUICK_SLOWDOWN_CTRL, _ENABLE, _NO, data32))
    {
        if (clkDomainsIsSecVFLwrvesEnabled())
        {
            status = FLCN_ERR_ILWALID_STATE;
            PMU_BREAKPOINT();
        }
        goto clkNafllLutUpdateQuickSlowdown_TU10X_exit;
    }

    //
    // There is an agreement with SSG that they will only toggle "_REQ_OVR" in devinit
    // while the "REQ_OVR_VAL" will be always ZERO.
    //
    if (FLD_TEST_DRF(_PTRIM, _GPC_BCAST_AVFS_QUICK_SLOWDOWN_CTRL, _REQ_OVR_VAL, _YES, data32))
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto clkNafllLutUpdateQuickSlowdown_TU10X_exit;
    }

    //
    // There are four mode of operations
    // HW Slowdown enable   SecLwrve       ----------- Valid
    // HW Slowdown enable   no SecLwrve    ----------- Not valid - Disable HW Slowdown
    // HW Slowdown disable  SecLwrve       ----------- Not valid - Enable HW Slowdown
    // HW Slowdown disable  no SecLwrve    ----------- Valid
    //

    //
    // If HW has slowdown disabled but SW/POR supports secondary lwrve,
    // enable HW slowdown
    //
    if ((FLD_TEST_DRF(_PTRIM, _GPC_BCAST_AVFS_QUICK_SLOWDOWN_CTRL, _REQ_OVR, _YES, data32)) &&
        (clkDomainsIsSecVFLwrvesEnabled()))
    {
        data32 = FLD_SET_DRF(_PTRIM, _GPC_BCAST_AVFS_QUICK_SLOWDOWN_CTRL,
                                  _REQ_OVR, _NO, data32);

    }
    //
    // If HW has slowdown enabled but SW/POR do NOT supports secondary lwrve,
    // disable HW slowdown
    //
    else if ((FLD_TEST_DRF(_PTRIM, _GPC_BCAST_AVFS_QUICK_SLOWDOWN_CTRL, _REQ_OVR, _NO, data32)) &&
        (!clkDomainsIsSecVFLwrvesEnabled()))
    {
        data32 = FLD_SET_DRF(_PTRIM, _GPC_BCAST_AVFS_QUICK_SLOWDOWN_CTRL,
                                  _REQ_OVR, _YES, data32);
    }
    else
    {
        // Do nothing
    }

    // Update the slowdown settings based on SW capabilities and POR.
    REG_WR32(FECS, LW_PTRIM_GPC_BCAST_AVFS_QUICK_SLOWDOWN_CTRL, data32);

clkNafllLutUpdateQuickSlowdown_TU10X_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
