/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  fanpolicy.c
 * @brief FAN Fan Policy Model
 *
 * This module is a collection of functions managing and manipulating state
 * related to fan policies in the Fan Policy Table.
 *
 * FAN_POLICY is a virtual class/interface. It alone does not specify a full
 * fan policy/mechanism. Classes which implement FAN_POLICY specify a full
 * mechanism for control.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "fan/objfan.h"
#include "fan/fanpolicy.h"
#include "therm/objtherm.h"
#include "dbgprintf.h"
#include "main.h"
#include "task_therm.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "rmpmusupersurfif.h"

/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * FAN_POLICY handler for the RM_PMU_BOARDOBJ_CMD_GRP_SET interface.
 *
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
fanPoliciesBoardObjGrpIfaceModel10Set
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    return BOARDOBJGRP_IFACE_MODEL_10_SET_AUTO_DMA(E32,      // _grpType
        FAN_POLICY,                                 // _class
        pBuffer,                                    // _pBuffer
        fanFanPolicyIfaceModel10SetHeader,                // _hdrFunc
        fanFanPolicyIfaceModel10SetEntry,                 // _entryFunc
        all.fan.fanPolicyGrpSet,                    // _ssElement
        OVL_INDEX_DMEM(therm),                      // _ovlIdxDmem
        BoardObjGrpVirtualTablesNotImplemented);    // _ppObjectVtables
}

/*!
 * @copydoc BoardObjGrpIfaceModel10SetHeader()
 */
FLCN_STATUS
fanFanPolicyIfaceModel10SetHeader
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJGRP *pHdrDesc
)
{
    RM_PMU_FAN_FAN_POLICY_BOARDOBJGRP_SET_HEADER   *pSetHdr;
    FLCN_STATUS                                     status;

    // Call to Board Object Group constructor must always be first!
    status = boardObjGrpIfaceModel10SetHeader(pModel10, pHdrDesc);
    if (status != FLCN_OK)
    {
        goto fanFanPolicyIfaceModel10SetHeader_exit;
    }

    pSetHdr = (RM_PMU_FAN_FAN_POLICY_BOARDOBJGRP_SET_HEADER *)pHdrDesc;

    Fan.policies.version = pSetHdr->version;

fanFanPolicyIfaceModel10SetHeader_exit:
    return status;
}

/*!
 * Super-class implementation.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
fanPolicyGrpIfaceModel10ObjSetImpl_SUPER
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_FAN_FAN_POLICY_BOARDOBJ_SET *pSet =
        (RM_PMU_FAN_FAN_POLICY_BOARDOBJ_SET *)pBoardObjDesc;
    FAN_POLICY         *pPolicy;
    FLCN_STATUS         status;

    // Call into BOARDOBJ_VTABLE super constructor.
    status = boardObjVtableGrpIfaceModel10ObjSet(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        goto fanPolicyGrpIfaceModel10ObjSetImpl_SUPER_exit;
    }
    pPolicy = (FAN_POLICY *)*ppBoardObj;

    // Set member variables.
    pPolicy->thermChIdx = pSet->thermChIdx;

#if PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_POLICY_THERM_CHANNEL_USE)
    //
    // Cache the THERM_CHANNEL pointer corresponding to the
    // given thermal channel index.
    //
    pPolicy->pThermChannel = THERM_CHANNEL_GET(pPolicy->thermChIdx);
    if (pPolicy->pThermChannel == NULL)
    {
        status = FLCN_ERR_ILWALID_INDEX;
        goto fanPolicyGrpIfaceModel10ObjSetImpl_SUPER_exit;
    }
#endif // PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_POLICY_THERM_CHANNEL_USE)

fanPolicyGrpIfaceModel10ObjSetImpl_SUPER_exit:
    return status;
}

/*!
 * Load all FAN_POLICIES.
 */
FLCN_STATUS
fanPoliciesLoad(void)
{
    FAN_POLICY   *pPolicy   = NULL;
    FLCN_STATUS   status    = FLCN_OK;
    LwBoardObjIdx i;

    BOARDOBJGRP_ITERATOR_BEGIN(FAN_POLICY, pPolicy, i, NULL)
    {
        status = fanPolicyLoad(pPolicy);
        if (status != FLCN_OK)
        {
            goto fanPoliciesLoad_exit;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

    // Call version specific Fan Policy Table load.
    switch (Fan.policies.version)
    {
        case LW2080_CTRL_FAN_POLICIES_VERSION_20:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_THERM_FAN_POLICY_V20);
            status = fanPoliciesLoad_V20();
            break;
        }
        case LW2080_CTRL_FAN_POLICIES_VERSION_30:
        {
            // To pacify coverity.
            break;
        }
        case LW2080_CTRL_FAN_POLICIES_VERSION_ILWALID:
        default:
        {
            status = LW_ERR_ILWALID_STATE;
            break;
        }
    }

fanPoliciesLoad_exit:
    return status;
}

/*!
 * Queries all FAN_POLICYs.
 */
FLCN_STATUS
fanPoliciesBoardObjGrpIfaceModel10GetStatus
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    return BOARDOBJGRP_IFACE_MODEL_10_GET_STATUS_AUTO_DMA(E32, // _grpType
        FAN_POLICY,                             // _class
        pBuffer,                                // _pBuffer
        NULL,                                   // _hdrFunc
        fanFanPolicyIfaceModel10GetStatus,                  // _entryFunc
        all.fan.fanPolicyGrpGetStatus);         // _ssElement
}

/*!
 * @Copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
fanFanPolicyIfaceModel10GetStatus
(
    BOARDOBJ_IFACE_MODEL_10               *pModel10,
    RM_PMU_BOARDOBJ        *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (BOARDOBJ_GET_TYPE(pBoardObj))
    {
        case LW2080_CTRL_FAN_POLICY_TYPE_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V20:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(
                PMU_THERM_FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V20);
            status = fanPolicyIfaceModel10GetStatus_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V20(pModel10, pPayload);
            break;
        }
        case LW2080_CTRL_FAN_POLICY_TYPE_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(
                PMU_THERM_FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30);
            status = fanPolicyIfaceModel10GetStatus_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30(pModel10, pPayload);
            break;
        }
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto fanFanPolicyIfaceModel10GetStatus_exit;
    }

fanFanPolicyIfaceModel10GetStatus_exit:
    return status;
}

/*!
 * Dispatch function for processing PMGR event that sends latest
 * PWR_CHANNEL status.
 *
 * @param[in]  pPwrChannelQuery         Pointer to the dispatch structure
 *
 * @return FLCN_OK
 *      PWR_CHANNEL status successfully obtained.
 * @return FLCN_ERR_NOT_SUPPORTED
 *      Interface isn't supported.
 * @return FLCN_ERR_ILWALID_INDEX
 *      Invalid fan policy index is passed.
 * @return FLCN_ERR_FEATURE_NOT_ENABLED
 *      If Fan Stop feature isn't enabled.
 * @return Other unexpected errors
 *      Unexpected errors propagated from other functions.
 */
FLCN_STATUS
fanPolicyPwrChannelQueryResponse
(
    DISPATCH_FANCTRL_PWR_CHANNEL_QUERY_RESPONSE *pPwrChannelQuery
)
{
    FAN_POLICY *pPolicy =
                 BOARDOBJGRP_OBJ_GET(FAN_POLICY, pPwrChannelQuery->fanPolicyIdx);
    FLCN_STATUS status  = FLCN_ERR_NOT_SUPPORTED;

    // Sanity checks.
    if (pPolicy == NULL)
    {
        status = FLCN_ERR_ILWALID_INDEX;
        PMU_BREAKPOINT();
        goto fanPolicyPwrChannelQueryResponse_exit;
    }
    if (!PMUCFG_FEATURE_ENABLED(
            PMU_THERM_FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_FAN_STOP))
    {
        status = FLCN_ERR_FEATURE_NOT_ENABLED;
        PMU_BREAKPOINT();
        goto fanPolicyPwrChannelQueryResponse_exit;
    }

    switch (BOARDOBJ_GET_TYPE(pPolicy))
    {
        case LW2080_CTRL_FAN_POLICY_TYPE_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V20:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(
                PMU_THERM_FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V20);
            status = fanPolicyPwrChannelQueryResponse_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V20(
                        pPolicy, pPwrChannelQuery);
            break;
        }
        case LW2080_CTRL_FAN_POLICY_TYPE_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(
                PMU_THERM_FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30);
            status = fanPolicyPwrChannelQueryResponse_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30(
                        pPolicy, pPwrChannelQuery);
            break;
        }
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }

fanPolicyPwrChannelQueryResponse_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */

/*!
 * @copydoc FanPolicyLoad
 */
FLCN_STATUS
fanPolicyLoad
(
    FAN_POLICY *pPolicy
)
{
    switch (BOARDOBJ_GET_TYPE(pPolicy))
    {
        case LW2080_CTRL_FAN_POLICY_TYPE_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V20:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(
                PMU_THERM_FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V20);
            return fanPolicyLoad_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V20(pPolicy);
            break;
        }
        case LW2080_CTRL_FAN_POLICY_TYPE_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(
                PMU_THERM_FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30);
            return fanPolicyLoad_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30(pPolicy);
            break;
        }
    }

    return FLCN_OK;
}

/*!
 * @copydoc FanPolicyCallback
 */
FLCN_STATUS
fanPolicyCallback
(
    FAN_POLICY *pPolicy
)
{
    switch (BOARDOBJ_GET_TYPE(pPolicy))
    {
        case LW2080_CTRL_FAN_POLICY_TYPE_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V20:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(
                PMU_THERM_FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V20);
            return fanPolicyCallback_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V20(pPolicy);
            break;
        }
        case LW2080_CTRL_FAN_POLICY_TYPE_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(
                PMU_THERM_FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30);
            return fanPolicyCallback_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30(pPolicy);
            break;
        }
    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * FAN_POLICY implementation.
 *
 * @copydoc FanPolicyFanPwmSet
 */
FLCN_STATUS
fanPolicyFanPwmSet
(
    FAN_POLICY *pPolicy,
    LwBool      bBound,
    LwSFXP16_16 pwm
)
{
    switch (BOARDOBJ_GET_TYPE(pPolicy))
    {
        case LW2080_CTRL_FAN_POLICY_TYPE_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V20:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(
                PMU_THERM_FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V20);
            return fanPolicyFanPwmSet_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V20(
                    pPolicy, bBound, pwm);
            break;
        }
        case LW2080_CTRL_FAN_POLICY_TYPE_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(
                PMU_THERM_FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30);
            return fanPolicyFanPwmSet_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30(
                    pPolicy, bBound, pwm);
            break;
        }
    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * FAN_POLICY implementation.
 *
 * @copydoc FanPolicyFanRpmSet
 */
FLCN_STATUS
fanPolicyFanRpmSet
(
    FAN_POLICY *pPolicy,
    LwS32       rpm
)
{
    switch (BOARDOBJ_GET_TYPE(pPolicy))
    {
        case LW2080_CTRL_FAN_POLICY_TYPE_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V20:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(
                PMU_THERM_FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V20);
            return fanPolicyFanRpmSet_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V20(
                    pPolicy, rpm);
            break;
        }
        case LW2080_CTRL_FAN_POLICY_TYPE_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(
                PMU_THERM_FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30);
            return fanPolicyFanRpmSet_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30(
                    pPolicy, rpm);
            break;
        }
    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * FAN_POLICY implementation.
 *
 * @copydoc FanPolicyFanPwmGet
 */
FLCN_STATUS
fanPolicyFanPwmGet
(
    FAN_POLICY     *pPolicy,
    LwSFXP16_16    *pPwm
)
{
    switch (BOARDOBJ_GET_TYPE(pPolicy))
    {
        case LW2080_CTRL_FAN_POLICY_TYPE_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V20:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(
                PMU_THERM_FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V20);
            return fanPolicyFanPwmGet_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V20(
                    pPolicy, pPwm);
            break;
        }
        case LW2080_CTRL_FAN_POLICY_TYPE_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(
                PMU_THERM_FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30);
            return fanPolicyFanPwmGet_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30(
                    pPolicy, pPwm);
            break;
        }
    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * FAN_POLICY implementation.
 *
 * @copydoc FanPolicyFanRpmGet
 */
FLCN_STATUS
fanPolicyFanRpmGet
(
    FAN_POLICY     *pPolicy,
    LwS32          *pRpm
)
{
    switch (BOARDOBJ_GET_TYPE(pPolicy))
    {
        case LW2080_CTRL_FAN_POLICY_TYPE_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V20:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(
                PMU_THERM_FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V20);
            return fanPolicyFanRpmGet_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V20(
                    pPolicy, pRpm);
            break;
        }
        case LW2080_CTRL_FAN_POLICY_TYPE_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(
                PMU_THERM_FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30);
            return fanPolicyFanRpmGet_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30(
                    pPolicy, pRpm);
            break;
        }
    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * FAN_POLICY implementation.
 *
 * @copydoc FanPolicyFanCtrlActionGet
 */
FLCN_STATUS
fanPolicyFanCtrlActionGet
(
    FAN_POLICY  *pPolicy,
    LwU8        *pFanCtrlAction
)
{
    FLCN_STATUS status = FLCN_OK;

    // Sanity checks.
    if (pFanCtrlAction == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto fanPolicyFanCtrlActionGet_exit;
    }

    *pFanCtrlAction = pPolicy->fanCtrlAction;

fanPolicyFanCtrlActionGet_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10SetEntry
 */
FLCN_STATUS
fanFanPolicyIfaceModel10SetEntry
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    RM_PMU_BOARDOBJ    *pBuf
)
{
    switch (pBuf->type)
    {
        case LW2080_CTRL_FAN_POLICY_TYPE_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V20:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(
                PMU_THERM_FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V20);
            return fanPolicyGrpIfaceModel10ObjSetImpl_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V20(pModel10, ppBoardObj,
                    sizeof(FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V20), pBuf);
            break;
        }
        case LW2080_CTRL_FAN_POLICY_TYPE_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(
                PMU_THERM_FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30);
            return fanPolicyGrpIfaceModel10ObjSetImpl_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30(pModel10, ppBoardObj,
                    sizeof(FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30), pBuf);
            break;
        }
    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * @Copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
fanPolicyIfaceModel10GetStatus_SUPER
(
    BOARDOBJ_IFACE_MODEL_10               *pModel10,
    RM_PMU_BOARDOBJ        *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    FAN_POLICY *pPolicy;
    RM_PMU_FAN_FAN_POLICY_BOARDOBJ_GET_STATUS
               *pStatus;
    FLCN_STATUS status;

    // Call super-class implementation.
    PMU_HALT_OK_OR_GOTO(status,
        boardObjIfaceModel10GetStatus(pModel10, pPayload),
        fanFanPolicyIfaceModel10GetStatus_exit);

    pPolicy = (FAN_POLICY *)pBoardObj;
    pStatus = (RM_PMU_FAN_FAN_POLICY_BOARDOBJ_GET_STATUS *)pPayload;

    pStatus->fanCtrlAction = pPolicy->fanCtrlAction;

fanFanPolicyIfaceModel10GetStatus_exit:
    return status;
}

/*!
 * SUPER-class implementation.
 *
 * @copydoc FanPolicyLoad
 */
FLCN_STATUS
fanPolicyLoad_SUPER
(
    FAN_POLICY *pPolicy
)
{
    // Initialize fan control action.
    pPolicy->fanCtrlAction = LW2080_CTRL_FAN_CTRL_ACTION_ILWALID;

    return FLCN_OK;
}
