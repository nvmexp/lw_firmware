/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pmu_pmughxxx.c
 *          PMU HAL Functions for GHXX and later GPUs
 *
 * PMU HAL functions implement chip-specific initialization, helper functions
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */

#include "dev_xtl_ep_pri.h"

#include "pmu_bar0.h"
#include "pmu_objpmu.h"

#include "config/g_pmu_private.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Prototypes ------------------------------------- */

/*
 * @brief Raise external priv level masks so that RM doesn't interfere with PMU
 * operation until they are lowered again.
 */
void
pmuPreInitRaiseExtPrivLevelMasks_GHXXX(void)
{
    if (Pmu.bLSMode)
    {
        //
        // Only raise priv level masks when we're in booted in LS mode,
        // else we'll take away our own ability to reset the masks when we
        // unload.
        //
        // TODO: figure out just how much protection we want here
        // Do minimal for now to not allow RM to reprogram
        //
        LwU32 val = XTL_REG_RD32(LW_XTL_EP_PRI_INTR_CTRL_PRIV_LEVEL_MASK(2));
        val = FLD_SET_DRF(_XTL_EP_PRI,
                          _INTR_CTRL_PRIV_LEVEL_MASK_WRITE_PROTECTION,
                          _LEVEL0, _DISABLE,
                          val);
        XTL_REG_WR32(LW_XTL_EP_PRI_INTR_CTRL_PRIV_LEVEL_MASK(2), val);

        val = XTL_REG_RD32(LW_XTL_EP_PRI_INTR_RETRIGGER_PRIV_LEVEL_MASK(2));
        val = FLD_SET_DRF(_XTL_EP_PRI,
                          _INTR_RETRIGGER_PRIV_LEVEL_MASK_WRITE_PROTECTION,
                          _LEVEL0, _DISABLE,
                          val);
        XTL_REG_WR32(LW_XTL_EP_PRI_INTR_RETRIGGER_PRIV_LEVEL_MASK(2), val);
    }
}

/*!
 * @brief Lower external priv level masks when we unload
 */
void
pmuLowerExtPrivLevelMasks_GHXXX(void)
{
    LwU32 val = XTL_REG_RD32(LW_XTL_EP_PRI_INTR_CTRL_PRIV_LEVEL_MASK(2));
    val = FLD_SET_DRF(_XTL_EP_PRI,
                      _INTR_CTRL_PRIV_LEVEL_MASK_WRITE_PROTECTION,
                      _LEVEL0, _ENABLE,
                      val);
    XTL_REG_WR32(LW_XTL_EP_PRI_INTR_CTRL_PRIV_LEVEL_MASK(2), val);

    val = XTL_REG_RD32(LW_XTL_EP_PRI_INTR_RETRIGGER_PRIV_LEVEL_MASK(2));
    val = FLD_SET_DRF(_XTL_EP_PRI,
                      _INTR_RETRIGGER_PRIV_LEVEL_MASK_WRITE_PROTECTION,
                      _LEVEL0, _ENABLE,
                      val);
    XTL_REG_WR32(LW_XTL_EP_PRI_INTR_RETRIGGER_PRIV_LEVEL_MASK(2), val);
}

/* ------------------------- Private Functions ------------------------------ */

