/*
 * Copyright 2009-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_detach_GMXXX.c
 * @brief  PMU Hal Functions for for handling the detach sequence for GK10X
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_pwr_falcon_csb.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "pmu_objpmu.h"
#include "pmu_objic.h"

#include "config/g_pmu_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Implementation --------------------------------- */
/*!
 * Last function that gets called upon receiving DETACH command.
 */
void
pmuDetach_GMXXX(void)
{
    // Disable Watchdog timer since there is no one to report errors to anyway.
    if (PMUCFG_FEATURE_ENABLED(PMUTASK_WATCHDOG))
    {
        timerWatchdogStop_HAL();
    }

    icDisableDetachInterrupts_HAL(&Ic);

#if (!PMUCFG_FEATURE_ENABLED(PMU_RISCV_COMPILATION_HACKS))
    // Disable irq0.
    falc_sclrb_i(CSW_FIELD_IE0);
#endif
}
