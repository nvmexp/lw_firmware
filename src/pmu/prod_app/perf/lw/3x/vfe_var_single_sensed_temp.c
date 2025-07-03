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
 * @file    vfe_var_single_sensed_temp.c
 * @brief   VFE_VAR_SINGLE_SENSED_TEMP class implementation.
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "pmu_objperf.h"
#include "perf/3x/vfe_var_single_sensed_temp.h"
#include "therm/objtherm.h"
#include "therm/thrmchannel.h"

/* ------------------------ Static Function Prototypes -----------------------*/
static BoardObjVirtualTableDynamicCast (s_vfeVarDynamicCast_SINGLE_SENSED_TEMP)
    GCC_ATTRIB_SECTION("imem_perf", "s_vfeVarDynamicCast_SINGLE_SENSED_TEMP")
    GCC_ATTRIB_USED();

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/*!
 * Virtual table for VFE_VAR_SINGLE_SENSED_TEMP object interfaces.
 */
BOARDOBJ_VIRTUAL_TABLE VfeVarSingleSensedTempVirtualTable =
{
    BOARDOBJ_VIRTUAL_TABLE_ASSIGN_DYNAMIC_CAST(s_vfeVarDynamicCast_SINGLE_SENSED_TEMP)
};

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief Constructor.
 * @memberof VFE_VAR_SINGLE_SENSED_TEMP
 * @public
 * @copydetails BoardObjGrpIfaceModel10ObjSet()
 */
FLCN_STATUS
vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_SENSED_TEMP
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_VFE_VAR_SINGLE_SENSED_TEMP  *pDescVar =
        (RM_PMU_VFE_VAR_SINGLE_SENSED_TEMP *)pBoardObjDesc;
    VFE_VAR_SINGLE_SENSED_TEMP         *pVfeVar;
    FLCN_STATUS                         status;

    PMU_ASSERT_OK_OR_GOTO(status,
        vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_SENSED(pModel10, ppBoardObj, size, pBoardObjDesc),
        vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_SENSED_TEMP_exit);

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pVfeVar, *ppBoardObj, PERF, VFE_VAR, SINGLE_SENSED_TEMP,
                                  &status, vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_SENSED_TEMP_exit);

    // Set member variables.
    pVfeVar->thermChannelIndex      = pDescVar->thermChannelIndex;
    pVfeVar->tempHysteresisPositive = pDescVar->tempHysteresisPositive;
    pVfeVar->tempHysteresisNegative = pDescVar->tempHysteresisNegative;
    pVfeVar->tempDefault            = pDescVar->tempDefault;

    // Start with something consistent.
    pVfeVar->lwTemp = RM_PMU_LW_TEMP_0_C;
    pVfeVar->fpTemp = 0.0f;

vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_SENSED_TEMP_exit:
    return status;
}

/*!
 * @copydoc VfeVarEval()
 * @memberof VFE_VAR_SINGLE_SENSED_TEMP
 * @public
 */
FLCN_STATUS
vfeVarEval_SINGLE_SENSED_TEMP
(
    VFE_CONTEXT    *pContext,
    VFE_VAR        *pVfeVar,
    LwF32          *pResult
)
{
    VFE_VAR_SINGLE_SENSED_TEMP *pVar;
    FLCN_STATUS                 status;

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pVar, &pVfeVar->super, PERF, VFE_VAR, SINGLE_SENSED_TEMP,
                                  &status, vfeVarEval_SINGLE_SENSED_TEMP_exit);

    // Most recent sample (by VFE callback) that exceeded hysteresis range.
    *pResult = pVar->fpTemp;

    // Finally at the end call parent implementation.
    status = vfeVarEval_SINGLE_SENSED(pContext, pVfeVar, pResult);

vfeVarEval_SINGLE_SENSED_TEMP_exit:
    return status;
}

/*!
 * @copydoc VfeVarSample()
 * @memberof VFE_VAR_SINGLE_SENSED_TEMP
 * @public
 */
FLCN_STATUS
vfeVarSample_SINGLE_SENSED_TEMP
(
    VFE_VAR    *pVfeVar,
    LwBool     *pBChanged
)
{
    VFE_VAR_SINGLE_SENSED_TEMP *pVar;
    THERM_CHANNEL  *pThermChannel;
    FLCN_STATUS     status = FLCN_OK;
    LwTemp          temp;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_DEFINE_LIB_THERM_CHANNEL
    };

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pVar, &pVfeVar->super, PERF, VFE_VAR, SINGLE_SENSED_TEMP,
                                  &status, vfeVarSample_SINGLE_SENSED_TEMP_exit);

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        pThermChannel = BOARDOBJGRP_OBJ_GET(THERM_CHANNEL, pVar->thermChannelIndex);
        if (pThermChannel == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto vfeVarSample_SINGLE_SENSED_TEMP_exit;
        }

        if (pVar->tempDefault != RM_PMU_CELSIUS_TO_LW_TEMP(150))
        {
            temp = pVar->tempDefault;
        }
        else
        {
            PMU_HALT_OK_OR_GOTO(status,
                thermChannelTempValueGet(pThermChannel, &temp),
                vfeVarSample_SINGLE_SENSED_TEMP_exit);
        }

        // Apply hysteresis
        if (((temp - pVar->lwTemp) > pVar->tempHysteresisPositive) ||
            ((pVar->lwTemp - temp) > pVar->tempHysteresisNegative))
        {
            pVar->lwTemp = temp;
            pVar->fpTemp = lwF32ColwertFromS32(temp);
            // Temperature requires scaling by 1/2^8.
            pVar->fpTemp = lwF32Mul(pVar->fpTemp, 1.0f / (LwF32)256);

            *pBChanged = LW_TRUE;
        }
        else
        {
            *pBChanged = LW_FALSE;
        }
vfeVarSample_SINGLE_SENSED_TEMP_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @copydoc VfeVarInputClientValueGet()
 * @memberof VFE_VAR_SINGLE_SENSED_TEMP
 * @public
 */
FLCN_STATUS
vfeVarInputClientValueGet_SINGLE_SENSED_TEMP
(
    VFE_VAR_SINGLE             *pVfeVar,
    RM_PMU_PERF_VFE_VAR_VALUE  *pClientVarValue,
    LwF32                      *pValue
)
{
    // Temperature requires scaling by 1/2^8.
    *pValue = LW_TYPES_UFXP_X_Y_TO_F32(24, 8, pClientVarValue->temperature.varValue);

    return FLCN_OK;
}

/* ------------------------ Private Functions ------------------------------- */
/*!
 * @brief   VFE_VAR_SINGLE_SENSED_TEMP implementation of
 *          @ref BoardObjVirtualTableDynamicCast()
 *
 * @copydoc BoardObjVirtualTableDynamicCast()
 */
static void *
s_vfeVarDynamicCast_SINGLE_SENSED_TEMP
(
    BOARDOBJ   *pBoardObj,
    LwU8        requestedType
)
{
    void                       *pObject           = NULL;
    VFE_VAR_SINGLE_SENSED_TEMP *pVfeVarSensedTemp = (VFE_VAR_SINGLE_SENSED_TEMP *)pBoardObj;

    if (BOARDOBJ_GET_TYPE(pBoardObj) !=
        LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_VAR, SINGLE_SENSED_TEMP))
    {
        PMU_BREAKPOINT();
        goto s_vfeVarDynamicCast_SINGLE_SENSED_TEMP_exit;
    }

    switch (requestedType)
    {
        case LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_VAR, BASE):
        {
            VFE_VAR *pVfeVar = &pVfeVarSensedTemp->super.super.super;
            pObject = (void *)pVfeVar;
            break;
        }
        case LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_VAR, SINGLE):
        {
            VFE_VAR_SINGLE *pVfeVarSingle = &pVfeVarSensedTemp->super.super;
            pObject = (void *)pVfeVarSingle;
            break;
        }
        case LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_VAR, SINGLE_SENSED):
        {
            VFE_VAR_SINGLE_SENSED *pVfeVarSingleSensed = &pVfeVarSensedTemp->super;
            pObject = (void *)pVfeVarSingleSensed;
            break;
        }
        case LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_VAR, SINGLE_SENSED_TEMP):
        {
            pObject = (void *)pVfeVarSensedTemp;
            break;
        }
        default:
        {
            PMU_BREAKPOINT();
            break;
        }
    }

s_vfeVarDynamicCast_SINGLE_SENSED_TEMP_exit:
    return pObject;
}
