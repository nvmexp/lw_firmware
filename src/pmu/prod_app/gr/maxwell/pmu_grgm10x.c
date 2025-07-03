
/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_grgm10x.c
 * @brief  HAL-interface for the GM10X Graphics Engine.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_master.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "pmu_objpg.h"
#include "pmu_objgr.h"
#include "pmu_bar0.h"

#include "dbgprintf.h"
#include "config/g_gr_private.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * Initialize the mask for GR PRIV Error Handling
 *
 * Internally till Pascal, we are blocking the GR
 * PRIV access
 */
void
grPgPrivErrHandlingInit_GM10X(void)
{
    //
    // Placeholder HAL function:
    //
    // Adding PMU Halt to detect that this HAL is not written for
    // MAXWELL_and_above chips.
    //
    PMU_HALT();
}
