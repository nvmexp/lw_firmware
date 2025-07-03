/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file clk_vf_rel_ratio_volt.c
 *
 * @brief Module managing all state related to the CLK_VF_REL_RATIO_VOLT structure.
 */

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"
#include "dmemovl.h"

/* ------------------------ Static Function Prototypes --------------------- */
/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * CLK_VF_REL_RATIO_VOLT class constructor.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
clkVfRelGrpIfaceModel10ObjSet_RATIO_VOLT
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjSet
)
{
    RM_PMU_CLK_CLK_VF_REL_RATIO_VOLT_BOARDOBJ_SET *pVfRelRatioSet =
        (RM_PMU_CLK_CLK_VF_REL_RATIO_VOLT_BOARDOBJ_SET *)pBoardObjSet;
    CLK_VF_REL_RATIO_VOLT  *pVfRelRatio;
    FLCN_STATUS             status;

    status = clkVfRelGrpIfaceModel10ObjSet_RATIO(pModel10, ppBoardObj, size, pBoardObjSet);
    if (status != FLCN_OK)
    {
        // Super class triggers PMU Break point on error.
        goto clkVfRelGrpIfaceModel10ObjSet_RATIO_VOLT_exit;
    }
    pVfRelRatio = (CLK_VF_REL_RATIO_VOLT *)*ppBoardObj;

    // Copy the CLK_VF_REL_RATIO_VOLT-specific data.
    pVfRelRatio->vfSmoothDataGrp =
        pVfRelRatioSet->vfSmoothDataGrp;

clkVfRelGrpIfaceModel10ObjSet_RATIO_VOLT_exit:
    return status;
}

/*!
 * @copydoc ClkVfRelSmoothing
 */
FLCN_STATUS
clkVfRelSmoothing_RATIO_VOLT
(
    CLK_VF_REL             *pVfRel,
    CLK_DOMAIN_40_PROG     *pDomain40Prog,
    LwU8                    lwrveIdx,
    LwU8                    cacheIdx,
    CLK_VF_POINT_40_VOLT   *pVfPoint40VoltLast
)
{
    CLK_VF_REL_RATIO_VOLT  *pVfRelRatioVolt = (CLK_VF_REL_RATIO_VOLT *)pVfRel;
    CLK_VF_POINT_40_VOLT   *pVfPoint40Volt;
    FLCN_STATUS             status          = FLCN_OK;
    LwS16                   vfIdx;

    // Sanity check.
    if ((LW2080_CTRL_CLK_CLK_VF_REL_VF_LWRVE_IDX_ILWALID == lwrveIdx) ||
        (LW2080_CTRL_CLK_CLK_VF_POINT_IDX_ILWALID ==
            clkVfRelVfPointIdxFirstGet(pVfRel, lwrveIdx)) ||
        (LW2080_CTRL_CLK_CLK_VF_POINT_IDX_ILWALID ==
            clkVfRelVfPointIdxLastGet(pVfRel, lwrveIdx)))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto clkVfRelSmoothing_RATIO_VOLT_exit;
    }

    // Bail early if entire VF REL has no smoothing required.
    if (!clkVfRelRatioVoltIsVfSmoothingRequiredForVFRel(pVfRelRatioVolt))
    {
        goto clkVfRelSmoothing_RATIO_VOLT_exit;
    }

    //
    // Iterate over this CLK_VF_REL's CLK_VF_POINTs and load each of
    // them in order.
    //
    for (vfIdx  = clkVfRelVfPointIdxLastGet(pVfRel, lwrveIdx);
         vfIdx >= clkVfRelVfPointIdxFirstGet(pVfRel, lwrveIdx);
         vfIdx--)
    {
        pVfPoint40Volt =
            (CLK_VF_POINT_40_VOLT *)CLK_VF_POINT_GET_BY_LWRVE_ID(lwrveIdx, vfIdx);
        if (pVfPoint40Volt == NULL)
        {
            status = FLCN_ERR_ILWALID_INDEX;
            PMU_BREAKPOINT();
            goto clkVfRelSmoothing_RATIO_VOLT_exit;
        }

        status = clkVfPoint40VoltSmoothing(pVfPoint40Volt,
                                           pVfPoint40VoltLast,
                                           pDomain40Prog,
                                           pVfRelRatioVolt,
                                           lwrveIdx,
                                           cacheIdx);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkVfRelSmoothing_RATIO_VOLT_exit;
        }

        // Update the last VF point pointer.
        pVfPoint40VoltLast = pVfPoint40Volt;
    }

clkVfRelSmoothing_RATIO_VOLT_exit:
    return status;
}

/*!
 * @copydoc ClkVfRelRatioVoltMaxFreqStepSizeMHzGet
 */
GCC_ATTRIB_ALWAYSINLINE()
inline LwU16
clkVfRelRatioVoltMaxFreqStepSizeMHzGet
(
    CLK_VF_REL_RATIO_VOLT  *pVfRelRatioVolt,
    LwU32                   voltageuV
)
{
    LwS32 vfSmoothDataEntryIdx;
    //
    // We choose the maximum value of step size as a default value.
    // This is to ensure that if there is no smoothing intended,
    LwU16 maxFreqMHzStepSize = LW_U16_MAX;

    // FATAL error if no smoothing present
    if (clkVfRelRatioVoltVfSmoothDataEntriesCountGet(pVfRelRatioVolt) == 0)
    {
        PMU_BREAKPOINT();
        goto clkVfRelRatioVoltMaxFreqStepSizeMHzGet_exit;
    }

    // FATAL error if provided voltage is less than lowest base voltage.
    if (voltageuV < pVfRelRatioVolt->vfSmoothDataGrp.vfSmoothDataEntries[0].baseVFSmoothVoltuV)
    {
        PMU_BREAKPOINT();
        goto clkVfRelRatioVoltMaxFreqStepSizeMHzGet_exit;
    }

    //
    // To avoid infinite loop, vfSmoothDataEntryIdx must be a
    // SIGNED integer (definitely not unsigned).
    //
    for (vfSmoothDataEntryIdx =
            clkVfRelRatioVoltVfSmoothDataEntriesCountGet(pVfRelRatioVolt) - 1;
         vfSmoothDataEntryIdx >= 0;
         vfSmoothDataEntryIdx--)
    {
        if (voltageuV >= 
            pVfRelRatioVolt->vfSmoothDataGrp.vfSmoothDataEntries[vfSmoothDataEntryIdx].baseVFSmoothVoltuV)
        {
            maxFreqMHzStepSize = 
                pVfRelRatioVolt->vfSmoothDataGrp.vfSmoothDataEntries[vfSmoothDataEntryIdx].maxFreqStepSizeMHz;
            break;
        }
    }

clkVfRelRatioVoltMaxFreqStepSizeMHzGet_exit:
    return maxFreqMHzStepSize;
}

/* ------------------------- Private Functions ------------------------------ */
