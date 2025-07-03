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
 * @file  fanpolicy_iir_tj_fixed_dual_slope_pwm_v30.c
 * @brief FAN Fan Policy Specific to IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30
 */

/* ------------------------- System Includes -------------------------------- */
#include "fan/objfan.h"
#include "pmusw.h"
#include "main.h"

/* ------------------------- Application Includes --------------------------- */
#include "fan/fan3x/30/fanpolicy_iir_tj_fixed_dual_slope_pwm_v30.h"

/* ------------------------ Static Function Prototypes ---------------------- */
static BoardObjVirtualTableGetInterfaceFromBoardObj
        (s_fanPolicyGetInterfaceFromBoardObj_ITFDSP_V30);

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/*!
 * Main structure for all FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30_VIRTUAL_TABLE data.
 */
BOARDOBJ_VIRTUAL_TABLE fanPolicyITFDSPV30VirtualTable =
{
    BOARDOBJ_VIRTUAL_TABLE_ASSIGN_GET_INTERFACE_FROM_BOARDOBJ(
        s_fanPolicyGetInterfaceFromBoardObj_ITFDSP_V30)
};

/*!
 * Main structure for all FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_VIRTUAL_TABLE data.
 */
INTERFACE_VIRTUAL_TABLE fanPolicyITFDSPVirtualTable_V30 =
{
    LW_OFFSETOF(FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30, iirTFDSP)   // offset
};

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * Construct a FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM object.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
fanPolicyGrpIfaceModel10ObjSetImpl_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_FAN_FAN_POLICY_BOARDOBJ_SET_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30 *pSet =
        (RM_PMU_FAN_FAN_POLICY_BOARDOBJ_SET_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30 *)pBoardObjDesc;
    FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30     *pPolicy;
    BOARDOBJ_VTABLE                                *pBoardObjVtable;
    FLCN_STATUS                                     status;

    // Construct and initialize parent-class object.
    status = fanPolicyGrpIfaceModel10ObjSetImpl_V30(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        goto fanPolicyGrpIfaceModel10ObjSetImpl_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30_exit;
    }
    pPolicy         = (FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30 *)*ppBoardObj;
    pBoardObjVtable = (BOARDOBJ_VTABLE *)*ppBoardObj;

    // Set member variables.
    pPolicy->pwmMin = pSet->pwmMin;
    pPolicy->pwmMax = pSet->pwmMax;
    pPolicy->rpmMin = pSet->rpmMin;
    pPolicy->rpmMax = pSet->rpmMax;

    // Call IIR_TJ_FIXED_DUAL_SLOPE_PWM_INTERFACE super-class implementation.
    status = fanPolicyConstructImpl_IIR_TJ_FIXED_DUAL_SLOPE_PWM_INTERFACE(pBObjGrp,
                &pPolicy->iirTFDSP.super, &pSet->iirTFDSP.super,
                &fanPolicyITFDSPVirtualTable_V30);
    if (status != FLCN_OK)
    {
        goto fanPolicyGrpIfaceModel10ObjSetImpl_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30_exit;
    }

    // Override the Vtable pointer.
    pBoardObjVtable->pVirtualTable = &fanPolicyITFDSPV30VirtualTable;

fanPolicyGrpIfaceModel10ObjSetImpl_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30_exit:
    return status;
}

/*!
 * Class implementation.
 *
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
fanPolicyIfaceModel10GetStatus_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30
(
    BOARDOBJ_IFACE_MODEL_10               *pModel10,
    RM_PMU_BOARDOBJ        *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_FAN_FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30_BOARDOBJ_GET_STATUS *pGetStatus   =
        (RM_PMU_FAN_FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30_BOARDOBJ_GET_STATUS *)pPayload;
    FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30                                *pPolicy      =
        (FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30 *)pBoardObj;
    FLCN_STATUS status;

    // Call super-class implementation.
    PMU_HALT_OK_OR_GOTO(status,
        fanPolicyIfaceModel10GetStatus_V30(pModel10, pPayload),
        fanPolicyIfaceModel10GetStatus_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30_exit);

    // Call IIR_TJ_FIXED_DUAL_SLOPE_PWM_INTERFACE super-class implementation.
    PMU_HALT_OK_OR_GOTO(status,
        fanPolicyQuery_IIR_TJ_FIXED_DUAL_SLOPE_PWM_INTERFACE(
            &pPolicy->iirTFDSP, &pGetStatus->iirTFDSP),
        fanPolicyIfaceModel10GetStatus_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30_exit);

fanPolicyIfaceModel10GetStatus_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30_exit:
    return status;
}

/*!
 * Class implementation.
 *
 * @copydoc FanPolicyLoad
 */
FLCN_STATUS
fanPolicyLoad_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30
(
    FAN_POLICY *pPolicy
)
{
    FLCN_STATUS status;

    // Call super-class implementation.
    PMU_HALT_OK_OR_GOTO(status,
        fanPolicyLoad_V30(pPolicy),
        fanPolicyLoad_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30_exit);

    // Call IIR_TJ_FIXED_DUAL_SLOPE_PWM_INTERFACE super-class implementation.
    PMU_HALT_OK_OR_GOTO(status,
        fanPolicyLoad_IIR_TJ_FIXED_DUAL_SLOPE_PWM_INTERFACE(pPolicy),
        fanPolicyLoad_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30_exit);

fanPolicyLoad_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30_exit:
    return status;
}

/*!
 * Class implementation.
 *
 * @copydoc FanPolicyFanPwmSet
 */
FLCN_STATUS
fanPolicyFanPwmSet_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30
(
    FAN_POLICY *pPolicy,
    LwBool      bBound,
    LwSFXP16_16 pwm
)
{
    FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30 *pPolicyV30 =
        (FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30 *)pPolicy;
    FLCN_STATUS status;

    // Bound the requested PWM settings, if requested.
    if (bBound)
    {
        pwm = LW_MAX(pwm, pPolicyV30->pwmMin);
        pwm = LW_MIN(pwm, pPolicyV30->pwmMax);
    }

    // Call IIR_TJ_FIXED_DUAL_SLOPE_PWM_INTERFACE super-class implementation.
    PMU_HALT_OK_OR_GOTO(status,
        fanPolicyFanPwmSet_IIR_TJ_FIXED_DUAL_SLOPE_PWM_INTERFACE(pPolicy, bBound, pwm),
        fanPolicyFanPwmSet_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30_exit);

fanPolicyFanPwmSet_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30_exit:
    return status;
}

/*!
 * Class implementation.
 *
 * @copydoc FanPolicyFanRpmSet
 */
FLCN_STATUS
fanPolicyFanRpmSet_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30
(
    FAN_POLICY *pPolicy,
    LwS32       rpm
)
{
    FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30 *pPolicyV30 =
        (FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30 *)pPolicy;
    FLCN_STATUS status;

    // Bound the requested RPM settings.
    rpm = LW_MAX(rpm, (LwS32)pPolicyV30->rpmMin);
    rpm = LW_MIN(rpm, (LwS32)pPolicyV30->rpmMax);

    // Call IIR_TJ_FIXED_DUAL_SLOPE_PWM_INTERFACE super-class implementation.
    PMU_HALT_OK_OR_GOTO(status,
        fanPolicyFanRpmSet_IIR_TJ_FIXED_DUAL_SLOPE_PWM_INTERFACE(pPolicy, rpm),
        fanPolicyFanRpmSet_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30_exit);

fanPolicyFanRpmSet_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30_exit:
    return status;
}

/* ------------------------ Private Static Functions ------------------------*/

/*!
 * @copydoc BoardObjVirtualTableGetInterfaceFromBoardObj
 */
static BOARDOBJ_INTERFACE *
s_fanPolicyGetInterfaceFromBoardObj_ITFDSP_V30
(
    BOARDOBJ   *pBoardObj,
    LwU8        interfaceType
)
{
    FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30 *pPolicy =
        (FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30 *)pBoardObj;

    switch (interfaceType)
    {
        case LW2080_CTRL_FAN_FAN_POLICY_INTERFACE_TYPE_IIR_TJ_FIXED_DUAL_SLOPE_PWM:
        {
            return &pPolicy->iirTFDSP.super;
        }
    }

    PMU_BREAKPOINT();
    return NULL;
}
