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
 * @file    lib_sandbag.c
 * @brief   Implementation of sandbag related interfaces.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmu.h"
#include "lib/lib_sandbag.h"
#include "lwSandbagList.h"

#include "g_lwconfig.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/*!
 * @brief   Override default placement of @ref aGLSandbagAllowList
 *          into a data section holding pre-init only data.
 */
static const GL_SANDBAG_ALLOWLIST aGLSandbagAlloweList[]
    GCC_ATTRIB_SECTION("dmem_initData", "aGLSandbagAllowList");

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
#if LWCFG(GLOBAL_FEATURE_SANDBAG) && PMUCFG_FEATURE_ENABLED(PMU_SANDBAG_ENABLED)
/*!
 * @brief   Cached value of the PMU's sandbagged state.
 *
 * Start in the sandbagged state in case we fail to call @ref sandbagInit(),
 * or @ref sandbagIsRequested() is ilwoked before the feature is initialized.
 */
static LwBool BSandbagState = LW_TRUE;
#endif // LWCFG(GLOBAL_FEATURE_SANDBAG) && PMUCFG_FEATURE_ENABLED(PMU_SANDBAG_ENABLED)

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief   Initializes the state of the PMU sandbagging feature.
 *
 * @note    Implementation must be kept in sync with @ref sandbagIsRequested().
 */
FLCN_STATUS
sandbagInit(void)
{
#if LWCFG(GLOBAL_FEATURE_SANDBAG) && PMUCFG_FEATURE_ENABLED(PMU_SANDBAG_ENABLED)
    LwU16 devId = Pmu.gpuDevId;
    LwU32 i;

    // If this interface was ilwoked the state should already be LW_TRUE.
    BSandbagState = LW_TRUE;

    if (devId == 0U)
    {
        // In a case of an invalid devId remain in sandbagged state.
        goto sandbagInit_exit;
    }

    for (i = 0U; i < LW_ARRAY_ELEMENTS(aGLSandbagAllowList); i++)
    {
        if (aGLSandbagAllowList[i].ulDevID == devId)
        {
            goto sandbagInit_exit;
        }
    }

    // No match -> no sandbagging requested.
    BSandbagState = LW_FALSE;

sandbagInit_exit:
#endif // LWCFG(GLOBAL_FEATURE_SANDBAG) && PMUCFG_FEATURE_ENABLED(PMU_SANDBAG_ENABLED)
    return FLCN_OK;
}

/*!
 * @brief   Queries the state of the PMU sandbagging feature.
 *
 * @note    Implementation must be kept in sync with @ref sandbagInit().
 *
 * @return  LW_TRUE     When sandbagging is required and dev. ID is in the list
 * @return  LW_FALSE    Otherwise
 */
LwBool
sandbagIsRequested(void)
{
#if LWCFG(GLOBAL_FEATURE_SANDBAG) && PMUCFG_FEATURE_ENABLED(PMU_SANDBAG_ENABLED)
    return BSandbagState;
#else // LWCFG(GLOBAL_FEATURE_SANDBAG) && PMUCFG_FEATURE_ENABLED(PMU_SANDBAG_ENABLED)
    return LW_FALSE;
#endif // LWCFG(GLOBAL_FEATURE_SANDBAG) && PMUCFG_FEATURE_ENABLED(PMU_SANDBAG_ENABLED)
}

/* ------------------------- Private Functions ------------------------------ */
