/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file clk_vf_point_3x.c
 *
 * @brief Module managing all state related to the CLK_VF_POINT structure.  This
 * structure defines a point on the VF lwrve of a PRIMARY CLK_DOMAIN.  This
 * finite point is created from the PRIMARY CLK_DOMAIN's CLK_PROG entries and the
 * voltage and/or frequency points they specify.  These points are evaluated and
 * cached once per the VFE polling loop such that they can be referenced later
 * via lookups for various necessary operations.
 *
 * The CLK_VF_POINT class is a virtual interface.  It does not include any
 * specifcation for how to compute voltage or frequency as a function of the
 * other.  It is up to the implementing classes to apply those semantics.
 */

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"

/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Static Function Prototypes --------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */

/*!
 * @Copydoc ClkVfPointAccessor
 */
FLCN_STATUS
clkVfPointAccessor
(
    CLK_VF_POINT            *pVfPoint,
    CLK_PROG_3X_PRIMARY      *pProg3XPrimary,
    CLK_DOMAIN_3X_PRIMARY    *pDomain3XPrimary,
    LwU8                     clkDomIdx,
    LwU8                     voltRailIdx,
    LwU8                     voltageType,
    LwBool                   bOffseted,
    LW2080_CTRL_CLK_VF_PAIR *pVfPair
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    // Call type-specific status functions.
    switch (BOARDOBJ_GET_TYPE(pVfPoint))
    {
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_30_VOLT:
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_30_FREQ:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_30))
            {
                status = clkVfPointAccessor_30(pVfPoint,
                                             pProg3XPrimary,
                                             pDomain3XPrimary,
                                             clkDomIdx,
                                             voltRailIdx,
                                             voltageType,
                                             bOffseted,
                                             pVfPair);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_VOLT_PRI:
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_FREQ:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_35))
            {
                status = clkVfPointAccessor_35(pVfPoint,
                                             pProg3XPrimary,
                                             pDomain3XPrimary,
                                             clkDomIdx,
                                             voltRailIdx,
                                             voltageType,
                                             bOffseted,
                                             pVfPair);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_VOLT_SEC:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_SEC))
            {
                status = clkVfPointAccessor_35(pVfPoint,
                                             pProg3XPrimary,
                                             pDomain3XPrimary,
                                             clkDomIdx,
                                             voltRailIdx,
                                             voltageType,
                                             bOffseted,
                                             pVfPair);
            }
            break;
        }
        default:
        {
            // Nothing to do
            break;
        }
    }
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }

    return status;
}

/*!
 * @copydoc ClkVfPointLoad
 */
FLCN_STATUS
clkVfPointLoad_IMPL
(
    CLK_VF_POINT            *pVfPoint,
    CLK_PROG_3X_PRIMARY      *pProg3XPrimary,
    CLK_DOMAIN_3X_PRIMARY    *pDomain3XPrimary,
    LwU8                     voltRailIdx,
    LwU8                     lwrveIdx
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    // Call type-specific status functions.
    switch (BOARDOBJ_GET_TYPE(pVfPoint))
    {
        // Cache the quantized base voltage values.
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_VOLT_PRI:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_35))
            {
                status = clkVfPointLoad_35_VOLT_PRI(pVfPoint,
                            pProg3XPrimary, pDomain3XPrimary,
                            voltRailIdx, lwrveIdx);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_VOLT_SEC:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_SEC))
            {
                status = clkVfPointLoad_35_VOLT_SEC(pVfPoint,
                            pProg3XPrimary, pDomain3XPrimary,
                            voltRailIdx, lwrveIdx);
            }
            break;
        }
        // Callwlate the secondary clock frequency and cache it.
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_FREQ:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_35))
            {
                status = clkVfPointLoad_35_FREQ(pVfPoint,
                            pProg3XPrimary, pDomain3XPrimary,
                            voltRailIdx, lwrveIdx);
            }
            break;
        }
        default:
        {
            //
            // All the versions of clock vf points may not required
            // to be loaded. Return FLCN_OK.
            //
            status  = FLCN_OK;
            break;
        }
    }
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }

    return status;
}

/* ------------------------- Private Functions ------------------------------ */
