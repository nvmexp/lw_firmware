/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2012-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pmu_thrm_pwr_ba1x.c
 * @brief   PMU HAL functions for BA v1.x power device (GK11X and GK20X)
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_chiplet_pwr.h"
#include "dev_pwr_csb.h"
#include "dev_pwr_csb_addendum.h"

/* ------------------------- Application Includes --------------------------- */
#include "dbgprintf.h"
#include "pmu_objpmgr.h"
#include "pmu_objpmu.h"
#include "therm/objtherm.h"

#include "config/g_therm_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Compile-time checks----------------------------- */
PMU_CT_ASSERT(PWR_DEVICE_BA1XHW_LIMIT_COUNT ==
              LW_CPWR_THERM_BA1XHW_THRESHOLD_COUNT);

/* ------------------------- Public Functions ------------------------------- */
/*!
 * Enables/disables BA units within monitored partitions, controlling whether
 * those units send weighted BA samples to the BA aclwmulators/fifos in the
 * PWR/THERM.  This effectively disables/eanbles the entire BA HW feature,
 * applying to all possible BA PWR_DEVICEs.
 *
 * @note Need to keep this interface in sync with the BA units which are enabled
 * by the RM via the PMGR_PWR_BA_REG_SETS_GET HAL.
 *
 * @param[in] bEnable
 *     Boolean indicating whether to enable/disable the BA HW.
 */
void
thermPwrDevBaEnable_BA12
(
    LwBool bEnable
)
{
    // Enable/disable GPCS and FBP BA.
    REG_WR32(BAR0, LW_PCHIPLET_PWR_GPCS_CONFIG_1,
        bEnable ?
            DRF_DEF(_PCHIPLET_PWR, _GPCS_CONFIG_1, _BA_ENABLE, _YES) :
            DRF_DEF(_PCHIPLET_PWR, _GPCS_CONFIG_1, _BA_ENABLE, _NO));
    REG_WR32(BAR0, LW_PCHIPLET_PWR_FBPS_CONFIG_1,
        bEnable ?
            DRF_DEF(_PCHIPLET_PWR, _FBPS_CONFIG_1, _BA_ENABLE, _YES) :
            DRF_DEF(_PCHIPLET_PWR, _FBPS_CONFIG_1, _BA_ENABLE, _NO));
}
