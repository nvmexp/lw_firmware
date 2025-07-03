/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2009-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pmu_thrmintrgk10x.c
 * @brief   PMU HAL functions for GK10X+ THERM interrupt control.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"
#include "dev_pwr_csb_addendum.h"

/* ------------------------- Application Includes --------------------------- */
#include "objnne.h"
#include "pmu_objpmgr.h"
#include "therm/objtherm.h"

#include "config/g_therm_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief Interrupt handler for all supported THERM interrupts.
 */
void
thermService_GM10X(void)
{
#if PMUCFG_FEATURE_ENABLED(PMU_SMBPBI)
    smbpbiService_HAL(&Smbpbi);
#endif

#if PMUCFG_FEATURE_ENABLED(PMU_THERM_INTR)
    thermEventIntrService_HAL(&Therm);
#endif

    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_DEVICE_BEACON_INTERRUPT))
    {
        pmgrPwrDevBeaconInterruptService_HAL(&Pmgr);
    }

    if (PMUCFG_ENGINE_ENABLED(NNE))
    {
        nneService();
    }
}
