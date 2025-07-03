/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    vfe_var_single_sensed_fuse.c
 * @brief   VFE_VAR_SINGLE_SENSED_FUSE class implementation.
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"
#include "pmu_bar0.h"

/* ------------------------ Application Includes ---------------------------- */
#include "pmu_objperf.h"
#include "perf/3x/vfe_var_single_sensed_fuse_base.h"

/* ------------------------ Static Function Prototypes -----------------------*/
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */

/*!
 * @brief Constructor.
 * @memberof VFE_VAR_SINGLE_SENSED_FUSE_BASE
 * @public
 * @copydetails BoardObjGrpIfaceModel10ObjSet()
 */
FLCN_STATUS
vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_SENSED_FUSE_BASE
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    VFE_VAR_SINGLE_SENSED_FUSE_BASE                  *pVfeVar;
    RM_PMU_VFE_VAR_SINGLE_SENSED_FUSE_BASE           *pDescVar =
        (RM_PMU_VFE_VAR_SINGLE_SENSED_FUSE_BASE *)pBoardObjDesc;
    VFE_VAR_SINGLE_SENSED_FUSE_BASE_EXTENDED         *pVfeVarBaseExtended;
    FLCN_STATUS                                       status;

    PMU_ASSERT_OK_OR_GOTO(status,
        vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_SENSED(pModel10, ppBoardObj, size, pBoardObjDesc),
        vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_SENSED_FUSE_BASE_exit);

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pVfeVar, *ppBoardObj, PERF, VFE_VAR, SINGLE_SENSED_FUSE_BASE,
        &status, vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_SENSED_FUSE_BASE_exit);

    pVfeVarBaseExtended = vfeVarSingleSensedFuseBaseExtendedGet(pVfeVar);
    if (pVfeVarBaseExtended != NULL)
    {
        pVfeVarBaseExtended->overrideInfo       = pDescVar->overrideInfo;
        pVfeVarBaseExtended->fuseValDefault     = pDescVar->fuseValDefault;
        pVfeVarBaseExtended->bFuseValueSigned   = pDescVar->bFuseValueSigned;
        pVfeVarBaseExtended->verInfo            = pDescVar->verInfo;
        pVfeVarBaseExtended->hwCorrectionScale  = pDescVar->hwCorrectionScale;
        pVfeVarBaseExtended->hwCorrectionOffset = pDescVar->hwCorrectionOffset;
    }

vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_SENSED_FUSE_BASE_exit:
    return status;
}

/*!
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
vfeVarIfaceModel10GetStatus_SINGLE_SENSED_FUSE_BASE
(
    BOARDOBJ_IFACE_MODEL_10              *pModel10,
    RM_PMU_BOARDOBJ       *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    VFE_VAR_SINGLE_SENSED_FUSE_BASE *pVar;
    RM_PMU_PERF_VFE_VAR_SINGLE_SENSED_FUSE_BASE_GET_STATUS *pGetStatus =
        (RM_PMU_PERF_VFE_VAR_SINGLE_SENSED_FUSE_BASE_GET_STATUS *)pPayload;
    FLCN_STATUS status = FLCN_OK;

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pVar, pBoardObj, PERF, VFE_VAR, SINGLE_SENSED_FUSE_BASE,
        &status, vfeVarIfaceModel10GetStatus_SINGLE_SENSED_FUSE_BASE_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        vfeVarGetStatus_SINGLE_SENSED(pBoardObj, pPayload),
        vfeVarIfaceModel10GetStatus_SINGLE_SENSED_FUSE_BASE_exit);

    pGetStatus->bVersionCheckFailed  = pVar->bVersionCheckFailed;
    pGetStatus->fuseValueHwInteger   = pVar->fuseValueHwInteger;
    // Get Info will return value in integer format
    pGetStatus->fuseValueInteger     = pVar->fuseValueInteger;
    pGetStatus->fuseVersion          = pVar->fuseVersion;

vfeVarIfaceModel10GetStatus_SINGLE_SENSED_FUSE_BASE_exit:
    return status;
}

/*!
 * @copydoc VfeVarEval()
 * @memberof VFE_VAR_SINGLE_SENSED_FUSE_BASE
 * @public
 */
FLCN_STATUS
vfeVarEval_SINGLE_SENSED_FUSE_BASE
(
    VFE_CONTEXT    *pContext,
    VFE_VAR        *pVfeVar,
    LwF32          *pResult
)
{
    VFE_VAR_SINGLE_SENSED_FUSE_BASE *pVar;
    FLCN_STATUS                      status;

    pVar = (VFE_VAR_SINGLE_SENSED_FUSE_BASE *)pVfeVar;

    // Value in PMU.
    *pResult = pVar->fuseValue;

    // Finally at the end call parent implementation.
    status = vfeVarEval_SINGLE_SENSED(pContext, pVfeVar, pResult);

    return status;
}
