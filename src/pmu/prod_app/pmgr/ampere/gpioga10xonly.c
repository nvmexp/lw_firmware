/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   gpioga10xonly.c
 * @brief  Contains GPIO specific initialization for GA10X.
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "lib/lib_gpio.h"

#include "dev_pmgr.h"

/* ------------------------- Application Includes --------------------------- */
#include "lwosreg.h"
#include "pmu_objpmu.h"
#include "pmu_objpmgr.h"

#include "config/g_pmgr_private.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * Check and set to _INIT for 9/10/11/17/18/20 GPIO
 * _INPUT_CNTL _BYPASS FILTER fields. Refer Bug 3153224
 *
 * @note    This fixes VBIOS settings of GPIO INPUT_CNTL registers.
 */
void
pmgrGpioPatchBug3153224_GA10X()
{

    LwU32       reg32;

    // Initialize CNTL_9/10/11 related with RASTER_SYNC_0/1/2 if not initialized
    reg32 = REG_RD32(FECS, LW_PMGR_GPIO_INPUT_CNTL_9);
    if (!FLD_TEST_DRF(_PMGR, _GPIO_INPUT_CNTL_9, _BYPASS_FILTER, _INIT, reg32))
    {
        reg32 = FLD_SET_DRF(_PMGR, _GPIO_INPUT_CNTL_9, _BYPASS_FILTER, _INIT, reg32);
        REG_WR32(FECS, LW_PMGR_GPIO_INPUT_CNTL_9, reg32);
    }

    reg32 = REG_RD32(FECS, LW_PMGR_GPIO_INPUT_CNTL_10);
    if (!FLD_TEST_DRF(_PMGR, _GPIO_INPUT_CNTL_10, _BYPASS_FILTER, _INIT, reg32))
    {
        reg32 = FLD_SET_DRF(_PMGR, _GPIO_INPUT_CNTL_10, _BYPASS_FILTER, _INIT, reg32);
        REG_WR32(FECS, LW_PMGR_GPIO_INPUT_CNTL_10, reg32);
    }

    reg32 = REG_RD32(FECS, LW_PMGR_GPIO_INPUT_CNTL_11);
    if (!FLD_TEST_DRF(_PMGR, _GPIO_INPUT_CNTL_11, _BYPASS_FILTER, _INIT, reg32))
    {
        reg32 = FLD_SET_DRF(_PMGR, _GPIO_INPUT_CNTL_11, _BYPASS_FILTER, _INIT, reg32);
        REG_WR32(FECS, LW_PMGR_GPIO_INPUT_CNTL_11, reg32);
    }

    // Initialize CNTL_17/18 related with SWAP_READY_0/1 if not initialized
    reg32 = REG_RD32(FECS, LW_PMGR_GPIO_INPUT_CNTL_17);
    if (!FLD_TEST_DRF(_PMGR, _GPIO_INPUT_CNTL_17, _BYPASS_FILTER, _INIT, reg32))
    {
        reg32 = FLD_SET_DRF(_PMGR, _GPIO_INPUT_CNTL_17, _BYPASS_FILTER, _INIT, reg32);
        REG_WR32(FECS, LW_PMGR_GPIO_INPUT_CNTL_17, reg32);
    }

    reg32 = REG_RD32(FECS, LW_PMGR_GPIO_INPUT_CNTL_18);
    if (!FLD_TEST_DRF(_PMGR, _GPIO_INPUT_CNTL_18, _BYPASS_FILTER, _INIT, reg32))
    {
        reg32 = FLD_SET_DRF(_PMGR, _GPIO_INPUT_CNTL_18, _BYPASS_FILTER, _INIT, reg32);
        REG_WR32(FECS, LW_PMGR_GPIO_INPUT_CNTL_18, reg32);
    }
}

