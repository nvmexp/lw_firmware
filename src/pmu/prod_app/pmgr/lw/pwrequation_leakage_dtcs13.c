/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pwrequation_leakage_dtcs13.c
 * @brief  PMGR Power Equation Leakage DTCS 1.3 Model Management
 *
 * This module is a collection of functions managing and manipulating state
 * related to Power Equation Leakage DTCS 1.3 objects in the Power Equation Table.
 *
 * https://wiki.lwpu.com/engwiki/index.php/Resman/PState/Data_Tables/Power_Tables/Power_Equation_Table_1.X
 *
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "lib/lib_fxp.h"
#include "lib_lwf32.h"
#include "pmu_objhal.h"
#include "pmu_objpmgr.h"
#include "pmu_objperf.h"
#include "therm/objtherm.h"
#include "perf/3x/vfe.h"
#include "pmgr/pwrequation_leakage_dtcs13.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * DTCS 1.3 implementation.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
pwrEquationGrpIfaceModel10ObjSetImpl_LEAKAGE_DTCS13
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_PMGR_PWR_EQUATION_LEAKAGE_DTCS13_BOARDOBJ_SET    *pDtcs13Desc =
        (RM_PMU_PMGR_PWR_EQUATION_LEAKAGE_DTCS13_BOARDOBJ_SET *)pBoardObjDesc;
    PWR_EQUATION_LEAKAGE_DTCS13                             *pDtcs13;
    FLCN_STATUS                                              status;

    // Construct and initialize parent-class object.
    status = pwrEquationGrpIfaceModel10ObjSetImpl_LEAKAGE_DTCS12(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        return status;
    }
    pDtcs13 = (PWR_EQUATION_LEAKAGE_DTCS13 *)*ppBoardObj;

    // Set the class-specific values:
    pDtcs13->gpcrgEff = pDtcs13Desc->gpcrgEff;
    if (pDtcs13->gpcrgEff > LW_TYPES_U32_TO_UFXP_X_Y(4, 12, 1))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_STATE;
        goto pwrEquationGrpIfaceModel10ObjSetImpl_LEAKAGE_DTCS13_done;
    }

    pDtcs13->grrpgEff = pDtcs13Desc->grrpgEff;
    if (pDtcs13->grrpgEff > LW_TYPES_U32_TO_UFXP_X_Y(4, 12, 1))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_STATE;
        goto pwrEquationGrpIfaceModel10ObjSetImpl_LEAKAGE_DTCS13_done;
    }

pwrEquationGrpIfaceModel10ObjSetImpl_LEAKAGE_DTCS13_done:
    return status;
}

/*!
 * DTCS 1.3 implementation.
 *
 * @copydoc PwrEquationLeakageEvaluatemA
 */
FLCN_STATUS
pwrEquationLeakageEvaluatemA_DTCS13
(
    PWR_EQUATION_LEAKAGE   *pLeakage,
    LwU32                   voltageuV,
    PMGR_LPWR_RESIDENCIES  *pPgRes,
    LwU32                  *pLeakageVal
)
{
    FLCN_STATUS                     status  = 
       FLCN_OK;
    PWR_EQUATION_LEAKAGE_DTCS13    *pDtcs13 =
       (PWR_EQUATION_LEAKAGE_DTCS13 *)pLeakage;
    LwUFXP20_12  gpcrgTemp;
    LwUFXP20_12  grrpgTemp;
    LwUFXP20_12  tmp;

    status = pwrEquationLeakageEvaluatemA_DTCS12(pLeakage, voltageuV, pPgRes, pLeakageVal);
    if (pPgRes == NULL)
    {
        goto pwrEquationLeakageEvaluatemA_DTCS13_done;
    }

    //
    // For DTCS1.3 we are interested in GPCRG and GRRPG residencies.
    // Sanity check the same to ensure they are between 0.0 to 1.0
    //
    if (pPgRes->lpwrResidency[RM_PMU_LPWR_CTRL_ID_GR_PG] >
        LW_TYPES_U32_TO_UFXP_X_Y(20, 12, 1))
    {
        PMU_BREAKPOINT();
        *pLeakageVal = 0;
        goto pwrEquationLeakageEvaluatemA_DTCS13_done;
    }
    if (pPgRes->lpwrResidency[RM_PMU_LPWR_CTRL_ID_GR_RG] >
        LW_TYPES_U32_TO_UFXP_X_Y(20, 12, 1))
    {
        PMU_BREAKPOINT();
        *pLeakageVal = 0;
        goto pwrEquationLeakageEvaluatemA_DTCS13_done;
    }
    if ((pPgRes->lpwrResidency[RM_PMU_LPWR_CTRL_ID_GR_RG] +
        pPgRes->lpwrResidency[RM_PMU_LPWR_CTRL_ID_GR_PG]) >
        LW_TYPES_U32_TO_UFXP_X_Y(20, 12, 1))
    {
        PMU_BREAKPOINT();
        *pLeakageVal = 0;
        goto pwrEquationLeakageEvaluatemA_DTCS13_done;
    }

    /*!
     *
     * leakage12mA = leakage without GPC-RG and GRRPG residency i.e. leakage
     *               as callwlated by DTCS1.2 equation
     *
     * grrpgRes = pPgRes->lpwrResidency[RM_PMU_LPWR_CTRL_ID_GR_RG]
     * gpcrgRes = pPgRes->lpwrResidency[RM_PMU_LPWR_CTRL_ID_GR_PG]
     *
     * res_{non_gated} = 1 - grrpgRes - gpcrgRes
     *
     * leakage13mA = leakage12mA * res_{non_gated} +
     *               leakage12mA * grrpgRes * (1 - grrpgEff) +
     *               leakage12mA * gpcrgRes * (1 - gpcrgEff)
     *
     *             = leakage12mA * [res_{non_gated} +
     *               grrpgRes * (1 - grrpgEff) + gpcrgRes * (1 - gpcrgEff)]
     *
     *             = leakage12mA * [1 - gpcrgRes - grrpgRes + gpcrgRes * (1 - gpcrgEff)
     *               + grrpgRes * (1 - grrpgEff)]
     *
     *             = leakage12mA * [1 - gpcrgRes + gpcrgRes *
     *               (1 - gpcrgEff) - grrpgRes + grrpgRes * (1 - grrpgEff)]
     *
     *             = leakage12mA * [1 + gpcrgRes * (1 - 1 - gpcrgEff) +
     *               grrpgRes * (1 - 1 - grrpgEff)]
     *
     *             = leakage12mA * [1 - gpcrgRes * gpcrgEff - grrpgRes * grrpgEff]
     */
    gpcrgTemp = multUFXP20_12((LwUFXP20_12)pDtcs13->gpcrgEff,
                pPgRes->lpwrResidency[RM_PMU_LPWR_CTRL_ID_GR_PG]);
    grrpgTemp = multUFXP20_12((LwUFXP20_12)pDtcs13->grrpgEff,
                pPgRes->lpwrResidency[RM_PMU_LPWR_CTRL_ID_GR_RG]);
    tmp       = gpcrgTemp + grrpgTemp;
    if (tmp > LW_TYPES_U32_TO_UFXP_X_Y(20, 12, 1))
    {
        *pLeakageVal = 0;
        PMU_BREAKPOINT();
        goto pwrEquationLeakageEvaluatemA_DTCS13_done;
    }

    //
    // Handling for efficiency.
    //
    // Numerical Analysis:-
    //
    // For AD10X_AND_LATER chips:-
    //
    // The multiplication is handled by a PMGR wrapper over
    // multUFXP20_12FailSafe() which does intermediate math safely in 64-bit as
    // follows:-
    //
    // 32.0 * 20.12 => 52.12
    // 52.12 >> 12 => 52.0
    //
    // If the upper 32 bits are non-zero (=> 32-bit overflow), it
    // returns LW_U32_MAX, otherwise
    // LWU64_LO(52.0) => 32.0 (the least significant 32 bits are
    //                         returned)
    //
    // Note: For PRE_AD10X chips, there is a possible FXP20.12 overflow
    //       at 1048576 mA.
    //
    *pLeakageVal = pmgrMultUFXP20_12OverflowFixes(*pLeakageVal,
                     (LW_TYPES_U32_TO_UFXP_X_Y(20, 12, 1) - tmp));

pwrEquationLeakageEvaluatemA_DTCS13_done:
    return status;
}
