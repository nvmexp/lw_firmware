/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  thrmpolicy.c
 * @brief Thermal Policy Model
 *
 * This module is a collection of functions managing and manipulating state
 * related to Thermal Policies
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmgr.h"
#include "pmu_objperf.h"
#include "therm/objtherm.h"
#include "therm/thrmpolicy.h"
#include "therm/thrmchannel.h"
#include "task_therm.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "rmpmusupersurfif.h"

#include "g_pmurpc.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
#if (PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))
OS_TMR_CALLBACK OsCBThermPolicy;
#endif

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
static OsTimerCallback   (s_thermPolicyTimerCallbackExelwte)
    GCC_ATTRIB_SECTION("imem_thermLibPolicy", "s_thermPolicyTimerCallbackExelwte")
    GCC_ATTRIB_USED();
static OsTmrCallbackFunc (s_thermPolicyTimerOsCallbackExelwte)
    GCC_ATTRIB_SECTION("imem_thermLibPolicy", "s_thermPolicyTimerOsCallbackExelwte")
    GCC_ATTRIB_USED();

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief   Assures non-ZERO initial THERM_POLICIES state.
 */
void
thermPoliciesPreInit(void)
{
    thermPolicyPerfLimitsClear(&Therm.lwrrentPerfLimits);
}

/*!
 * THERM_POLICY handler for the RM_PMU_BOARDOBJ_CMD_GRP_SET interface.
 *
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
thermPolicyBoardObjGrpIfaceModel10Set
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    return BOARDOBJGRP_IFACE_MODEL_10_SET_AUTO_DMA(E32,      // _grpType
        THERM_POLICY,                               // _class
        pBuffer,                                    // _pBuffer
        thermThermPolicyIfaceModel10SetHeader,            // _hdrFunc
        thermThermPolicyIfaceModel10SetEntry,             // _entryFunc
        all.therm.thermPolicyGrpSet,                // _ssElement
        OVL_INDEX_DMEM(therm),                      // _ovlIdxDmem
        BoardObjGrpVirtualTablesNotImplemented);    // _ppObjectVtables
}

/*!
 * Super-class implementation.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
thermPolicyGrpIfaceModel10ObjSetImpl_SUPER
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_THERM_THERM_POLICY_BOARDOBJ_SET *pSet     =
        (RM_PMU_THERM_THERM_POLICY_BOARDOBJ_SET *)pBoardObjDesc;
    THERM_POLICY                           *pPolicy  = NULL;
    THERM_POLICY_DIAGNOSTICS               *pDiagnostics;
    FLCN_STATUS                             status   = FLCN_OK;

    // Sanity check.
    if (!THERM_CHANNEL_INDEX_IS_VALID(pSet->chIdx))
    {
        status = FLCN_ERR_ILWALID_STATE;
        goto thermPolicyGrpIfaceModel10ObjSetImpl_SUPER_exit;
    }

    status = boardObjGrpIfaceModel10ObjSet(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        return status;
    }
    pPolicy = (THERM_POLICY *)*ppBoardObj;

    // Set member variables
    pPolicy->pChannel            = THERM_CHANNEL_GET(pSet->chIdx);
    if (pPolicy->pChannel == NULL)
    {
        status = FLCN_ERR_ILWALID_INDEX;
        goto thermPolicyGrpIfaceModel10ObjSetImpl_SUPER_exit;
    }

    pPolicy->limitLwrr                 = pSet->limitLwrr;
    pPolicy->callbackPeriodus          = pSet->callbackPeriodus;
    thermPolicyPffSet(pPolicy, pSet->pff);

#if PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_DEVICE_TEMPERATURE_OFFSET)
    pPolicy->maxMemSpecTj              = pSet->maxMemSpecTj;
    pPolicy->maxMemReportTj            = pSet->maxMemReportTj;
#endif

    //
    // Obtain pointer to the structure that encapsulates functionality for
    // Thermal Policy Diagnostic features at a per-policy level.
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        thermPolicyDiagnosticsGet(pPolicy, &pDiagnostics),
        thermPolicyGrpIfaceModel10ObjSetImpl_SUPER_exit);
    if (pDiagnostics != NULL)
    {
        // Construct Thermal Policy Diagnostics state at a per-policy level.
        PMU_ASSERT_OK_OR_GOTO(status,
            thermPolicyDiagnosticsConstruct(pDiagnostics,
                &pSet->diagnostics),
            thermPolicyGrpIfaceModel10ObjSetImpl_SUPER_exit);
    }

thermPolicyGrpIfaceModel10ObjSetImpl_SUPER_exit:
    return status;
}

/*!
 * SUPER-class implementation.
 *
 * @copydoc ThermPolicyLoad
 */
FLCN_STATUS
thermPolicyLoad_SUPER
(
    THERM_POLICY *pPolicy
)
{
    LwU32 lwrrentTimeus = thermGetLwrrentTimeInUs();

    // Initialize callbackTimeStampus
    pPolicy->callbackTimestampus = lwrrentTimeus + pPolicy->callbackPeriodus;

    return FLCN_OK;
}

/*!
 * SUPER-class implementation.
 *
 * @copydoc ThermPolicyQuery
 */
FLCN_STATUS
thermPolicyIfaceModel10GetStatus_SUPER
(
    BOARDOBJ_IFACE_MODEL_10               *pModel10,
    RM_PMU_BOARDOBJ        *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_THERM_THERM_POLICY_BOARDOBJ_GET_STATUS  *pGetStatus;
    THERM_POLICY                                   *pPolicy;
    THERM_POLICY_DIAGNOSTICS                       *pDiagnostics;
    PIECEWISE_FREQUENCY_FLOOR                      *pPff;
    FLCN_STATUS                                     status = FLCN_OK;

    status = boardObjIfaceModel10GetStatus(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        goto thermPolicyIfaceModel10GetStatus_SUPER_exit;
    }
    pPolicy     = (THERM_POLICY *)pBoardObj;
    pGetStatus  = (RM_PMU_THERM_THERM_POLICY_BOARDOBJ_GET_STATUS *)pPayload;
    pPff        = thermPolicyPffGet(pPolicy);

    pGetStatus->valueLwrr = pPolicy->channelReading;

    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PIECEWISE_FREQUENCY_FLOOR) &&
        (pPff != NULL))
    {
        pGetStatus->pff = pPff->status;
    }

    // Get Status of Thermal Policy Diagnostics metrics at a per-policy level.
    PMU_ASSERT_OK_OR_GOTO(status,
        thermPolicyDiagnosticsGet(pPolicy, &pDiagnostics),
        thermPolicyIfaceModel10GetStatus_SUPER_exit);
    if (pDiagnostics != NULL)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            thermPolicyDiagnosticsGetStatus(pDiagnostics,
                &pGetStatus->diagnostics),
            thermPolicyIfaceModel10GetStatus_SUPER_exit);
    }

thermPolicyIfaceModel10GetStatus_SUPER_exit:
    return status;
}

/*!
 * @copydoc ThermPolicyGetPffCachedFrequencyFloor
 */
FLCN_STATUS
thermPolicyGetPffCachedFrequencyFloor_SUPER
(
    THERM_POLICY   *pPolicy,
    LwU32          *pFreqkHz
)
{
    PIECEWISE_FREQUENCY_FLOOR  *pPff    = thermPolicyPffGet(pPolicy);
    FLCN_STATUS                 status  = FLCN_OK;

    if (pPff == NULL)
    {
        status = FLCN_ERR_NOT_SUPPORTED;
        goto thermPolicyGetPffCachedFrequencyFloor_SUPER_exit;
    }

    *pFreqkHz = pffGetCachedFrequencyFloor(pPff);

thermPolicyGetPffCachedFrequencyFloor_SUPER_exit:
    return status;
}

/*!
 * @copydoc ThermPolicyVfIlwalidate
 */
FLCN_STATUS
thermPolicyVfIlwalidate_SUPER
(
    THERM_POLICY   *pPolicy
)
{
    PIECEWISE_FREQUENCY_FLOOR  *pPff    = thermPolicyPffGet(pPolicy);
    FLCN_STATUS                 status  = FLCN_OK;

    if (pPff == NULL)
    {
        status = FLCN_ERR_NOT_SUPPORTED;
        goto thermPolicyVfIlwalidate_SUPER_exit;
    }

    PMU_HALT_OK_OR_GOTO(status,
        pffSanityCheck(thermPolicyPffGet(pPolicy), THERM_POLICY_LIMIT_GET(pPolicy)),
        thermPolicyVfIlwalidate_SUPER_exit);

    PMU_HALT_OK_OR_GOTO(status,
        pffIlwalidate(pPff, pPolicy->channelReading, THERM_POLICY_LIMIT_GET(pPolicy)),
        thermPolicyVfIlwalidate_SUPER_exit);

thermPolicyVfIlwalidate_SUPER_exit:
    return status;
}

/*!
 * SUPER-class implementation.
 *
 * @copydoc ThermPolicyCallbackGetTime
 */
LwU32
thermPolicyCallbackGetTime
(
    THERM_POLICY *pPolicy
)
{
    return pPolicy->callbackTimestampus;
}

/*!
 * SUPER-class implementation.
 *
 * @copydoc ThermPolicyCallbackUpdateTime
 */
LwU32
thermPolicyCallbackUpdateTime
(
    THERM_POLICY *pPolicy,
    LwU32         timeus
)
{
    LwU32 n;

    //
    // The following callwlation is done for a couple of reasons:
    // 1. The RM went into a STATE_UNLOAD that may have caused us to miss one
    //    or more callbacks. We are now handling the callback during the
    //    STATE_LOAD.
    // 2. The overall callback handling took long enough that this policy may
    //    have multiple periods expire.
    //
    // For most policies, we just want to reschedule the callback to the
    // next regularly scheduled time. If a policy needs to absolutely needs to
    // handle every callback, then it can override this or the callbackExelwte
    // method.
    //
    n = (LwU32)(((timeus - pPolicy->callbackTimestampus) /
        pPolicy->callbackPeriodus) + 1);
    pPolicy->callbackTimestampus += (n * pPolicy->callbackPeriodus);

    return pPolicy->callbackTimestampus;
}

/*!
 * @brief   Execute routine for THERM_POLICY_LOAD
 *
 * @param[in/out]   pParams pointer to RM_PMU_RPC_STRUCT_THERM_THERM_POLICY_LOAD
 */
FLCN_STATUS
pmuRpcThermThermPolicyLoad
(
    RM_PMU_RPC_STRUCT_THERM_THERM_POLICY_LOAD *pParams
)
{
    FLCN_STATUS status = FLCN_OK;

    if (pParams->bLoad)
    {
        //
        // Overlays permanently attached to Therm task will be detached
        // (to make room for thermal policy libraries) and re-attached
        // by thermPoliciesLoad(). Don't repeat the same thing here.
        //
        status = thermPoliciesLoad();

        if (PMUCFG_FEATURE_ENABLED(PMU_THERM_PERF_VF_ILWALIDATION_NOTIFY) &&
            (status == FLCN_OK))
        {
            OSTASK_OVL_DESC ovlDescList[] = {
                OSTASK_OVL_DESC_DEFINE_PFF_ILWALIDATE
            };

            OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
            {
                status = thermPoliciesVfIlwalidate();
            }
            OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
        }
    }
    else
    {
        // Always return FLCN_OK on unload path (i.e. ignore output)
        (void)thermPoliciesUnload();
    }

    return status;
}

/*!
 * Loads all THERM_POLICY SW state
 *
 * @return FLCN_OK
 *      All THERM_POLICY SW state loaded successfully.
 * @return Other errors
 *      Errors propagated up from functions called.
 */
FLCN_STATUS
thermPoliciesLoad(void)
{
    THERM_POLICY *pPolicy;
    LwBoardObjIdx i;
    FLCN_STATUS   status = FLCN_OK;
    LwBool        bThermPoliciesPresent = LW_FALSE;

    // Execute each policy's load routine.
    BOARDOBJGRP_ITERATOR_BEGIN(THERM_POLICY, pPolicy, i, NULL)
    {
        bThermPoliciesPresent = LW_TRUE;

        PMU_HALT_OK_OR_GOTO(status,
            thermPolicyLoad(pPolicy),
            thermPoliciesLoad_exit);
    }
    BOARDOBJGRP_ITERATOR_END;

    // If there are no therm policies, skip scheduling callback
    if (bThermPoliciesPresent)
    {
        //
        // Execute the timer callback. This is to catch any timer interrupts that
        // may have oclwrred while we were in STATE_LOAD. This will also schedule
        // the next timer callback.
        //
        s_thermPolicyTimerCallbackExelwte(NULL, 0, 0);
    }

thermPoliciesLoad_exit:
    return status;
}

/*!
 * @brief   Cancels the thermal policies' timer callback.
 */
void
thermPoliciesUnload(void)
{
    // Cancel all timer callbacks.
#if (PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))
    osTmrCallbackCancel(&OsCBThermPolicy);
#else
    osTimerDeScheduleCallback(&ThermOsTimer,
        THERM_OS_TIMER_ENTRY_THERM_POLICY_CALLBACKS);
#endif
}

/*!
 * Calls vf ilwalidate on all thermal policies
 *
 * @return FLCN_OK
 * @return Other errors
 *      Errors propagated up from functions called.
 */
FLCN_STATUS
thermPoliciesVfIlwalidate(void)
{
    THERM_POLICY *pPolicy;
    LwBoardObjIdx i;
    FLCN_STATUS   status = FLCN_OK;

    //
    // Note that when taking both the perf semaphore and the PWR_POLICIES
    // semaphore, the perf semaphore must be taken first.
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PIECEWISE_FREQUENCY_FLOOR))
    {
        perfReadSemaphoreTake();
    }
    PWR_POLICIES_SEMAPHORE_TAKE;

    // Execute each policy's VfIlwalidate routine.
    BOARDOBJGRP_ITERATOR_BEGIN(THERM_POLICY, pPolicy, i, NULL)
    {
        PMU_HALT_OK_OR_GOTO(status,
            thermPolicyVfIlwalidate(pPolicy),
            thermPoliciesVfIlwalidate_exit);
    }
    BOARDOBJGRP_ITERATOR_END;

thermPoliciesVfIlwalidate_exit:
    PWR_POLICIES_SEMAPHORE_GIVE;
    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PIECEWISE_FREQUENCY_FLOOR))
    {
        perfReadSemaphoreGive();
    }
    return status;
}

/*!
 * THERM_POLICY handler for the RM_PMU_BOARDOBJ_CMD_GRP_GET_STATUS interface.
 *
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
thermPolicyBoardObjGrpIfaceModel10GetStatus
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    return BOARDOBJGRP_IFACE_MODEL_10_GET_STATUS_AUTO_DMA(E32, // _grpType
        THERM_POLICY,                           // _class
        pBuffer,                                // _pBuffer
        thermThermPolicyIfaceModel10GetStatusHeader,        // _hdrFunc
        thermThermPolicyIfaceModel10GetStatus,              // _entryFunc
        all.therm.thermPolicyGrpGetStatus);     // _ssElement
}

/*!
 * @Copydoc BoardObjGrpIfaceModel10GetStatusHeader
 */
FLCN_STATUS
thermThermPolicyIfaceModel10GetStatusHeader
(
    BOARDOBJGRP_IFACE_MODEL_10          *pModel10,
    RM_PMU_BOARDOBJGRP   *pBuf,
    BOARDOBJGRPMASK      *pMask
)
{
    THERM_POLICYS_DIAGNOSTICS                               *pDiagnostics;
    RM_PMU_THERM_THERM_POLICY_BOARDOBJGRP_GET_STATUS_HEADER *pHdr =
        (RM_PMU_THERM_THERM_POLICY_BOARDOBJGRP_GET_STATUS_HEADER *)pBuf;
    FLCN_STATUS                                              status = FLCN_OK;

    pHdr->limitArbLwrrent = Therm.lwrrentPerfLimits;

    //
    // Get Status of Thermal Policy Diagnostics metrics at a policyGrp and a
    // per-channel level.
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        thermPolicysDiagnosticsGet(&Therm.policyMetadata,
            &pDiagnostics),
        thermThermPolicyIfaceModel10GetStatusHeader_exit);
    if (pDiagnostics != NULL)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            thermPolicysDiagnosticsGetStatus(pDiagnostics, &pHdr->diagnostics),
            thermThermPolicyIfaceModel10GetStatusHeader_exit);
    }

thermThermPolicyIfaceModel10GetStatusHeader_exit:
    return status;
}

/*!
 * @Copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
thermThermPolicyIfaceModel10GetStatus
(
    BOARDOBJ_IFACE_MODEL_10               *pModel10,
    RM_PMU_BOARDOBJ        *pBuf
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    switch (BOARDOBJ_GET_TYPE(pBoardObj))
    {
        case LW2080_CTRL_THERMAL_POLICY_TYPE_DTC_VF:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_THERM_THERM_POLICY_DTC_VF);
            return thermPolicyIfaceModel10GetStatus_DTC_VF(pModel10, pBuf);
        }

        case LW2080_CTRL_THERMAL_POLICY_TYPE_DTC_VOLT:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_THERM_THERM_POLICY_DTC_VOLT);
            return thermPolicyIfaceModel10GetStatus_DTC_VOLT(pModel10, pBuf);
        }

        case LW2080_CTRL_THERMAL_POLICY_TYPE_DTC_PWR:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_THERM_THERM_POLICY_DTC_PWR);
            return thermPolicyIfaceModel10GetStatus_DTC_PWR(pModel10, pBuf);
        }
    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * @copydoc ThermPolicyLoad
 */
FLCN_STATUS
thermPolicyLoad
(
    THERM_POLICY *pPolicy
)
{
    switch (BOARDOBJ_GET_TYPE(pPolicy))
    {
        case LW2080_CTRL_THERMAL_POLICY_TYPE_DTC_VF:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_THERM_THERM_POLICY_DTC_VF);
            return thermPolicyLoad_DTC_VF(pPolicy);
            break;
        }

        case LW2080_CTRL_THERMAL_POLICY_TYPE_DTC_VOLT:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_THERM_THERM_POLICY_DTC_VOLT);
            return thermPolicyLoad_DTC_VOLT(pPolicy);
            break;
        }
    }

    return thermPolicyLoad_SUPER(pPolicy);
}

/*!
 * @copydoc ThermPolicyCallbackExelwte
 */
FLCN_STATUS
thermPolicyCallbackExelwte
(
    THERM_POLICY  *pPolicy
)
{
    THERM_POLICY_DIAGNOSTICS *pDiagnostics;
    FLCN_STATUS               status = FLCN_ERR_NOT_SUPPORTED;

    //
    // Re-initialize dynamic functionality of Thermal Policy Diagnostic metrics
    // at a policyGrp and a per-channel level.
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        thermPolicyDiagnosticsGet(pPolicy, &pDiagnostics),
        thermPolicyCallbackExelwte_exit);
    if (pDiagnostics != NULL)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            thermPolicyDiagnosticsReinit(pDiagnostics),
            thermPolicyCallbackExelwte_exit);
    }

    // Evaluate the policy's pff lwrve, if applicable
    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PIECEWISE_FREQUENCY_FLOOR) &&
        (thermPolicyPffGet(pPolicy) != NULL))
    {
        status = pffEvaluate(thermPolicyPffGet(pPolicy),
                             pPolicy->channelReading,
                             THERM_POLICY_LIMIT_GET(pPolicy));
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto thermPolicyCallbackExelwte_exit;
        }
    }

    switch (BOARDOBJ_GET_TYPE(pPolicy))
    {
        case LW2080_CTRL_THERMAL_POLICY_TYPE_DTC_VF:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_THERM_THERM_POLICY_DTC_VF);
            status = thermPolicyCallbackExelwte_DTC_VF(pPolicy);
            break;
        }

        case LW2080_CTRL_THERMAL_POLICY_TYPE_DTC_VOLT:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_THERM_THERM_POLICY_DTC_VOLT);
            status = thermPolicyCallbackExelwte_DTC_VOLT(pPolicy);
            break;
        }

        case LW2080_CTRL_THERMAL_POLICY_TYPE_DTC_PWR:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_THERM_THERM_POLICY_DTC_PWR);
            status = thermPolicyCallbackExelwte_DTC_PWR(pPolicy);
            break;
        }

        default:
            //
            // All child classes that require a timer callback MUST implement their
            // own. If we are hitting this assert, it means that the child class
            // has not overridden the callbackExelwte pointer to use it's
            // implementation.
            //
            PMU_BREAKPOINT();
    }

    //
    // Evaluate Thermal Policy Diagnostic metrics at a per-policy
    // level.
    //
    if (pDiagnostics != NULL)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            thermPolicyDiagnosticsEvaluate(pPolicy, pDiagnostics),
            thermPolicyCallbackExelwte_exit);
    }

thermPolicyCallbackExelwte_exit:
    return status;
}

/*!
 * @copydoc ThermPolicyGetPffCachedFrequencyFloor
 */
FLCN_STATUS
thermPolicyGetPffCachedFrequencyFloor
(
    THERM_POLICY   *pPolicy,
    LwU32          *pFreqkHz
)
{
    return thermPolicyGetPffCachedFrequencyFloor_SUPER(pPolicy, pFreqkHz);
}

/*!
 * @copydoc ThermPolicyVfIlwalidate
 */
FLCN_STATUS
thermPolicyVfIlwalidate
(
    THERM_POLICY   *pPolicy
)
{
    return thermPolicyVfIlwalidate_SUPER(pPolicy);
}

/*!
 * @ref      OsTmrCallbackFunc
 *
 * @brief   Exelwtes each thermal policies' timer callback functions.
 *
 * This function will scan all the thermal policies and determine
 * if it needs to be exelwted. If it is determined that a policy's timer
 * expired during the scanning of the policies, the process will repeat again.
 *
 * Additionally, this function will schedule the next timer callback.
 * But will not schedule the next timer callback on GP10x onwards.
 *
 */
static FLCN_STATUS
s_thermPolicyTimerOsCallbackExelwte
(
    OS_TMR_CALLBACK *pCallback
)
{
    THERM_POLICY              *pPolicy;
    THERM_POLICYS_DIAGNOSTICS *pDiagnostics;
    LwU32                      lwrrentTimeus;
    LwU32                      policyTimeus;
    LwU32                      callbackTimeus;
    LwU32                      callbackDelayus;
    LwBoardObjIdx              i;
    FLCN_STATUS                status        = FLCN_OK;
    OSTASK_OVL_DESC            ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, smbpbi)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, thermLibSensor2X)
        OSTASK_OVL_DESC_DEFINE_THERM_POLICY_CALLBACK_EXELWTE_PIECEWISE_FREQUENCY_FLOOR
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        //
        // Note that when taking both the perf semaphore and the PWR_POLICIES
        // semaphore, the perf semaphore must be taken first.
        //
        if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PIECEWISE_FREQUENCY_FLOOR))
        {
            perfReadSemaphoreTake();
        }

        if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_POLICY_DTC_PWR))
        {
            ATTACH_AND_LOAD_OVERLAY_PWR_POLICY_CLIENT_LOCKED;
        }

        //
        // Re-initialize dynamic functionality of Thermal Policy Diagnostic metrics
        // at a policyGrp and a per-channel level.
        //
        PMU_ASSERT_OK_OR_GOTO(status,
            thermPolicysDiagnosticsGet(&Therm.policyMetadata,
                &pDiagnostics),
            s_thermPolicyTimerOsCallbackExelwte_exit);
        if (pDiagnostics != NULL)
        {
            PMU_ASSERT_OK_OR_GOTO(status,
                thermPolicysDiagnosticsReinit(pDiagnostics, LW_FALSE),
                s_thermPolicyTimerOsCallbackExelwte_exit);
        }

        do
        {
            LwU32 timeTillPolicyus;
            LwU32 timeTillCallbackus = THERM_POLICY_TIME_INFINITY;

            lwrrentTimeus = thermGetLwrrentTimeInUs();

            BOARDOBJGRP_ITERATOR_BEGIN(THERM_POLICY, pPolicy, i, NULL)
            {
                policyTimeus = thermPolicyCallbackGetTime(pPolicy);

                //
                // Determine if the policy's callback timer has expired; if so,
                // execute the policy's callback.
                //
                if (osTmrGetTicksDiff(policyTimeus, lwrrentTimeus) >= 0)
                {
                    // Read the channel temperature.
                    PMU_ASSERT_OK_OR_GOTO(status,
                        thermChannelTempValueGet(pPolicy->pChannel,
                            &(pPolicy->channelReading)),
                        s_thermPolicyTimerOsCallbackExelwte_exit);

                    // Execute timer callback.
                    PMU_ASSERT_OK_OR_GOTO(status,
                        thermPolicyCallbackExelwte(pPolicy),
                        s_thermPolicyTimerOsCallbackExelwte_exit);

                    // Update policy with a new callback time.
                    policyTimeus = thermPolicyCallbackUpdateTime(pPolicy, lwrrentTimeus);
                }

                //
                // In order to find the the earliest policy expiration, we must use
                // the difference between the policy time and the current time.
                // The above check 'osTmrGetTicksDiff(policyTimeus, lwrrentTimeus)'
                // ensures that this difference is always positive so no need to
                // check for signed overflow.
                //
                timeTillPolicyus = policyTimeus - lwrrentTimeus;

                // Save the earliest policy expiration to timeTillCallback.
                timeTillCallbackus = LW_MIN(timeTillCallbackus, timeTillPolicyus);
            }
            BOARDOBJGRP_ITERATOR_END;

            callbackTimeus = lwrrentTimeus + timeTillCallbackus;

            //
            // Need to determine if a lower index policy's timer expired while
            // processing higher index policies (i.e. policy B's timer expired
            // while processing policy D's timer). Repeat the process if a timer
            // expired while processing.
            //
            lwrrentTimeus = thermGetLwrrentTimeInUs();
        } while (osTmrGetTicksDiff(callbackTimeus, lwrrentTimeus) >= 0);

        //
        // Schedule next callback based on the earliest policy expiration.
        // The above check 'osTmrGetTicksDiff(callbackTimeus, lwrrentTimeus)'
        // ensures that this difference is always positive so no need to
        // check for signed overflow
        //
        callbackDelayus = callbackTimeus - lwrrentTimeus;

        //
        // Evaluate Thermal Policy Diagnostics state at a policyGrp and a
        // per-channel level.
        //
        if (pDiagnostics != NULL)
        {
            PMU_ASSERT_OK_OR_GOTO(status,
                thermPolicysDiagnosticsEvaluate(pDiagnostics),
                s_thermPolicyTimerOsCallbackExelwte_exit);
        }

#if (PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))

        if (!OS_TMR_CALLBACK_WAS_CREATED(&OsCBThermPolicy))
        {
            osTmrCallbackCreate(&OsCBThermPolicy,               // pCallback
                OS_TMR_CALLBACK_TYPE_RELWRRENT_TO_ACTUAL_END,   // type
                OVL_INDEX_IMEM(thermLibPolicy),                 // ovlImem
                s_thermPolicyTimerOsCallbackExelwte,            // pTmrCallbackFunc
                LWOS_QUEUE(PMU, THERM),                         // queueHandle
                callbackDelayus,                                // periodNormalus
                callbackDelayus,                                // periodSleepus
                OS_TIMER_RELAXED_MODE_USE_NORMAL,               // bRelaxedUseSleep
                RM_PMU_TASK_ID_THERM);                          // taskId
            osTmrCallbackSchedule(&OsCBThermPolicy);
        }
        else
        {
            // Update callback period
            osTmrCallbackUpdate(&OsCBThermPolicy, callbackDelayus, callbackDelayus,
                                OS_TIMER_RELAXED_MODE_USE_NORMAL, OS_TIMER_UPDATE_USE_BASE_LWRRENT);
        }

#else
        osTimerScheduleCallback(
            &ThermOsTimer,                                            // pOsTimer
            THERM_OS_TIMER_ENTRY_THERM_POLICY_CALLBACKS,              // index
            lwrtosTaskGetTickCount32(),                               // ticksPrev
            callbackDelayus,                                          // usDelay
            DRF_DEF(_OS_TIMER, _FLAGS, _RELWRRING, _YES_MISSED_EXEC), // flags
            s_thermPolicyTimerCallbackExelwte,                        // pCallback
            NULL,                                                     // pParam
            OVL_INDEX_IMEM(thermLibPolicy));                          // ovlIdx
#endif

s_thermPolicyTimerOsCallbackExelwte_exit:
        if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_POLICY_DTC_PWR))
        {
            DETACH_OVERLAY_PWR_POLICY_CLIENT_LOCKED;
        }

        if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PIECEWISE_FREQUENCY_FLOOR))
        {
            perfReadSemaphoreGive();
        }
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status; // NJ-TODO-CB
}

/*!
 * @ref     OsTimerCallback
 *
 * @brief   This is the old callback path which
 * calls the new s_thermPolicyTimerOsCallbackExelwte function.
 *
 */
static void
s_thermPolicyTimerCallbackExelwte
(
    void   *pParam,
    LwU32   ticksExpired,
    LwU32   skipped
)
{
    s_thermPolicyTimerOsCallbackExelwte(NULL);
}

/*!
 * @Copydoc BoardObjGrpIfaceModel10SetHeader
 */
FLCN_STATUS
thermThermPolicyIfaceModel10SetHeader
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJGRP *pHdrDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_THERM_THERM_POLICY_BOARDOBJGRP_SET_HEADER  *pSetHdr;
    THERM_POLICYS_DIAGNOSTICS                         *pDiagnostics;
    FLCN_STATUS                                        status;

    // Call to Board Object Group constructor must always be first!
    status = boardObjGrpIfaceModel10SetHeader(pModel10, pHdrDesc);
    if (status != FLCN_OK)
    {
        goto thermThermPolicyIfaceModel10SetHeader_exit;
    }

    pSetHdr = (RM_PMU_THERM_THERM_POLICY_BOARDOBJGRP_SET_HEADER *)pHdrDesc;

    Therm.policyMetadata.gpsPolicyIdx                    =
        pSetHdr->gpsPolicyIdx;
    Therm.policyMetadata.acousticPolicyIdx               =
        pSetHdr->acousticPolicyIdx;
    Therm.policyMetadata.memPolicyIdx                    =
        pSetHdr->memPolicyIdx;
    Therm.policyMetadata.gpuSwSlowdownPolicyIdx          =
        pSetHdr->gpuSwSlowdownPolicyIdx;
    Therm.policyMetadata.lwTempWorstCaseGpuMaxToAvgDelta =
        pSetHdr->lwTempWorstCaseGpuMaxToAvgDelta;

    //
    // Obtain pointer to the structure that encapsulates functionality for
    // Thermal Policy Diagnostic features at a policyGrp and a per-channel
    // level.
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        thermPolicysDiagnosticsGet(&Therm.policyMetadata, &pDiagnostics),
        thermThermPolicyIfaceModel10SetHeader_exit);
    if (pDiagnostics != NULL)
    {
        //
        // Construct Thermal Policy Diagnostics state at a policyGrp and a
        // per-channel level.
        //
        PMU_ASSERT_OK_OR_GOTO(status,
            thermPolicysDiagnosticsConstruct(pBObjGrp, pDiagnostics,
                &pSetHdr->diagnostics),
            thermThermPolicyIfaceModel10SetHeader_exit);
    }

thermThermPolicyIfaceModel10SetHeader_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10SetEntry
 */
FLCN_STATUS
thermThermPolicyIfaceModel10SetEntry
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    RM_PMU_BOARDOBJ    *pBuf
)
{
    switch (pBuf->type)
    {
        case LW2080_CTRL_THERMAL_POLICY_TYPE_DTC_VF:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_THERM_THERM_POLICY_DTC_VF);
            return thermPolicyGrpIfaceModel10ObjSetImpl_DTC_VF(pModel10, ppBoardObj,
                sizeof(THERM_POLICY_DTC_VF), pBuf);
            break;
        }

        case LW2080_CTRL_THERMAL_POLICY_TYPE_DTC_VOLT:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_THERM_THERM_POLICY_DTC_VOLT);
            return thermPolicyGrpIfaceModel10ObjSetImpl_DTC_VOLT(pModel10, ppBoardObj,
                sizeof(THERM_POLICY_DTC_VOLT), pBuf);
            break;
        }

        case LW2080_CTRL_THERMAL_POLICY_TYPE_DTC_PWR:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_THERM_THERM_POLICY_DTC_PWR);
            return thermPolicyGrpIfaceModel10ObjSetImpl_DTC_PWR(pModel10, ppBoardObj,
                sizeof(THERM_POLICY_DTC_PWR), pBuf);
            break;
        }
    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}
