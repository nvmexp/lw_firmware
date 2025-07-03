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
 * @file    vfe_var_single_frequency.c
 * @brief   VFE_VAR_SINGLE_FREQUENCY class implementation.
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "pmu_objperf.h"
#include "perf/3x/vfe_var_single_frequency.h"

/* ------------------------ Static Function Prototypes ---------------------- */
static BoardObjVirtualTableDynamicCast (s_vfeVarDynamicCast_SINGLE_FREQUENCY)
    GCC_ATTRIB_SECTION("imem_perf", "s_vfeVarDynamicCast_SINGLE_FREQUENCY")
    GCC_ATTRIB_USED();

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/*!
 * Virtual table for VFE_VAR_SINGLE_FREQUENCY object interfaces.
 */
BOARDOBJ_VIRTUAL_TABLE VfeVarSingleFrequencyVirtualTable =
{
    BOARDOBJ_VIRTUAL_TABLE_ASSIGN_DYNAMIC_CAST(s_vfeVarDynamicCast_SINGLE_FREQUENCY)
};

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief Constructor.
 * @memberof VFE_VAR_SINGLE_FREQUENCY
 * @public
 * @copydetails BoardObjGrpIfaceModel10ObjSet()
 */
FLCN_STATUS
vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_FREQUENCY
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_VFE_VAR_SINGLE_FREQUENCY *pDescVar =
        (RM_PMU_VFE_VAR_SINGLE_FREQUENCY *)pBoardObjDesc;
    VFE_VAR_SINGLE_FREQUENCY  *pVfeVar;
    FLCN_STATUS                status;

    PMU_ASSERT_OK_OR_GOTO(status,
        vfeVarGrpIfaceModel10ObjSetImpl_SINGLE(pModel10, ppBoardObj, size, pBoardObjDesc),
        vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_FREQUENCY_exit);

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pVfeVar, *ppBoardObj, PERF, VFE_VAR, SINGLE_FREQUENCY,
                                  &status, vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_FREQUENCY_exit);

    // Set member variables.
    pVfeVar->clkDomainIdx = pDescVar->clkDomainIdx;

vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_FREQUENCY_exit:
    return status;
}

/*!
 * @copydoc VfeVarSingleClientInputMatch()
 * @memberof VFE_VAR_SINGLE_FREQUENCY
 * @public
 */
FLCN_STATUS
vfeVarSingleClientInputMatch_SINGLE_FREQUENCY
(
    VFE_VAR_SINGLE            *pVfeVar,
    RM_PMU_PERF_VFE_VAR_VALUE *pClientVarValue
)
{
    VFE_VAR_SINGLE_FREQUENCY *pVar;
    FLCN_STATUS               status;

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pVar, &pVfeVar->super.super, PERF, VFE_VAR, SINGLE_FREQUENCY,
                                  &status, vfeVarSingleClientInputMatch_SINGLE_FREQUENCY_exit);

    // Match against the clock domain index
    if (pClientVarValue->frequency.clkDomainIdx == pVar->clkDomainIdx)
    {
        return FLCN_OK;
    }

vfeVarSingleClientInputMatch_SINGLE_FREQUENCY_exit:
    return FLCN_ERR_OBJECT_NOT_FOUND;
}

/* ------------------------ Private Functions ------------------------------- */
/*!
 * @brief   VFE_VAR_SINGLE_FREQUENCY implementation of
 *          @ref BoardObjVirtualTableDynamicCast()
 *
 * @copydoc BoardObjVirtualTableDynamicCast()
 */
static void *
s_vfeVarDynamicCast_SINGLE_FREQUENCY
(
    BOARDOBJ   *pBoardObj,
    LwU8        requestedType
)
{
    void                       *pObject          = NULL;
    VFE_VAR_SINGLE_FREQUENCY   *pVfeVarFrequency = (VFE_VAR_SINGLE_FREQUENCY *)pBoardObj;

    if (BOARDOBJ_GET_TYPE(pBoardObj) !=
        LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_VAR, SINGLE_FREQUENCY))
    {
        PMU_BREAKPOINT();
        goto s_vfeVarDynamicCast_SINGLE_FREQUENCY_exit;
    }

    switch (requestedType)
    {
        case LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_VAR, BASE):
        {
            VFE_VAR *pVfeVar = &pVfeVarFrequency->super.super;
            pObject = (void *)pVfeVar;
            break;
        }
        case LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_VAR, SINGLE):
        {
            VFE_VAR_SINGLE *pVfeVarSingle = &pVfeVarFrequency->super;
            pObject = (void *)pVfeVarSingle;
            break;
        }
        case LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_VAR, SINGLE_FREQUENCY):
        {
            pObject = (void *)pVfeVarFrequency;
            break;
        }
        default:
        {
            PMU_BREAKPOINT();
            break;
        }
    }

s_vfeVarDynamicCast_SINGLE_FREQUENCY_exit:
    return pObject;
}
