/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    perf_cf_controller.c
 * @copydoc perf_cf_controller.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"
#include "dmemovl.h"
#include "lwostimer.h"
#include "lwostmrcallback.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "rmpmusupersurfif.h"

/* ------------------------ Application Includes ---------------------------- */
#include "perf/cf/perf_cf.h"
#include "perf/cf/perf_cf_controller.h"
#include "pmu_objperf.h"
#include "pmu_objclk.h"

/* ------------------------ Static Function Prototypes ---------------------- */
static PerfCfControllerLoad         (s_perfCfControllerLoad)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "s_perfCfControllerLoad");
static PerfCfControllerExelwte      (s_perfCfControllerExelwte);
static PerfCfControllerSetMultData  (s_perfCfControllerSetMultData)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "s_perfCfControllerSetMultData");
static BoardObjIfaceModel10GetStatus                (s_perfCfControllerIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "s_perfCfControllerIfaceModel10GetStatus");

static OsTmrCallbackFunc            (s_perfCfControllersCallback)
    GCC_ATTRIB_USED();

static FLCN_STATUS s_perfCfControllersFilter(PERF_CF_CONTROLLERS *pControllers, BOARDOBJGRPMASK_E32 *pMaskExpired, LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_PROFILING *pProfiling);
static FLCN_STATUS s_perfCfControllersEvaluate(PERF_CF_CONTROLLERS *pControllers, BOARDOBJGRPMASK_E32 *pMaskActiveExpired, LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_PROFILING *pProfiling);
static FLCN_STATUS s_perfCfControllersLimitsInit(PERF_CF_CONTROLLERS *pControllers);
static FLCN_STATUS s_perfCfControllersMultDataConstruct(PERF_CF_CONTROLLERS *pControllers)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "s_perfCfControllersMultDataConstruct")
    GCC_ATTRIB_NOINLINE();
static FLCN_STATUS s_perfCfControllersInflectionPointsHandle(PERF_CF_CONTROLLERS *pControllers);

/* ------------------------ Macros and Defines ------------------------------ */
// TODO - Document
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLERS_2X)
#define PMU_PERF_CF_PERF_LIMITS_CLIENTS_ENTRIES_MAX  LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_MAX_LIMITS
#elif PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLERS_1X)
#define PMU_PERF_CF_PERF_LIMITS_CLIENTS_ENTRIES_MAX  6
#else
ct_assert(0);
#endif

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * @PERF_CF_TMR_CALLBACK_CONTROLLERS
 * derived callback structure from base class OS_TMR_CALLBACK.
 */
typedef struct
{
    /*!
     * OS_TMR_CALLBACK super class. This should always be the first member!
     */
    OS_TMR_CALLBACK         super;
    /*!
     * PERF_CF group of CONTROLLER objects.
     */
    PERF_CF_CONTROLLERS    *pControllers;
} PERF_CF_TMR_CALLBACK_CONTROLLERS;

/* ------------------------ Global Variables -------------------------------- */
PERF_CF_TMR_CALLBACK_CONTROLLERS OsCBPerfCfControllers;

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @copydoc perfCfControllerGrpConstruct_SUPER
 */
FLCN_STATUS
perfCfControllerGrpConstruct_SUPER
(
    PERF_CF_CONTROLLERS *pControllers,
    LwBool               bFirstConstruct
)
{
    FLCN_STATUS status = LW_OK;

    if (bFirstConstruct)
    {
        status = s_perfCfControllersMultDataConstruct(pControllers);
        if (status != FLCN_OK)
        {
            goto perfCfControllerGrpConstruct_SUPER_exit;
        }
    }

    // Trigger Perf CF Controllers sanity check.
    status = perfCfControllersSanityCheck_HAL(&PerfCf);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfCfControllerGrpConstruct_SUPER_exit;
    }

perfCfControllerGrpConstruct_SUPER_exit:
    return status;
}
/*!
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
perfCfControllerGrpIfaceModel10ObjSetImpl_SUPER
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_PERF_CF_CONTROLLER *pDescController =
        (RM_PMU_PERF_CF_CONTROLLER *)pBoardObjDesc;
    PERF_CF_CONTROLLER *pController;
    LwBool              bFirstConstruct = (*ppBoardObj == NULL);
    FLCN_STATUS         status;

    status = boardObjGrpIfaceModel10ObjSet(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        goto perfCfControllerGrpIfaceModel10ObjSetImpl_SUPER_exit;
    }
    pController = (PERF_CF_CONTROLLER *)*ppBoardObj;

    if (!bFirstConstruct)
    {
        // We cannot allow subsequent SET calls to change these parameters.
        if ((pController->samplingMultiplier    != pDescController->samplingMultiplier) ||
            (pController->topologyIdx           != pDescController->topologyIdx))
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto perfCfControllerGrpIfaceModel10ObjSetImpl_SUPER_exit;
        }
    }
    else
    {
        LW2080_CTRL_PMGR_PWR_POLICIES_INFLECTION_POINTS_DISABLE_REQUEST *pRequest;

        pController->pMultData = NULL;

        // Initialize the inflection points disable request, if available.
        PMU_ASSERT_OK_OR_GOTO(status,
            perfCfControllerInflectionPointsDisableRequestGet(
                pController, &pRequest),
            perfCfControllerGrpIfaceModel10ObjSetImpl_SUPER_exit);
        if (pRequest != NULL)
        {
            LW2080_CTRL_PMGR_PWR_POLICIES_INFLECTION_POINTS_DISABLE_REQUEST_INIT(
                pRequest);
        }
    }

    // Set member variables.
    pController->samplingMultiplier = pDescController->samplingMultiplier;
    pController->topologyIdx        = pDescController->topologyIdx;

    // Reset controller states on the next cycle after SET.
    pController->bReset = LW_TRUE;

    // Validate member variables.
    if (pController->samplingMultiplier == 0)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto perfCfControllerGrpIfaceModel10ObjSetImpl_SUPER_exit;
    }
    if ((LW2080_CTRL_PERF_PERF_CF_CONTROLLER_INFO_TOPOLOGY_IDX_NONE !=
                                                pController->topologyIdx) &&
        !BOARDOBJGRP_IS_VALID(PERF_CF_TOPOLOGY, pController->topologyIdx))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto perfCfControllerGrpIfaceModel10ObjSetImpl_SUPER_exit;
    }

perfCfControllerGrpIfaceModel10ObjSetImpl_SUPER_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpQuery
 */
FLCN_STATUS
perfCfControllerIfaceModel10GetStatus_SUPER
(
    BOARDOBJ_IFACE_MODEL_10              *pModel10,
    RM_PMU_BOARDOBJ       *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_PERF_CF_CONTROLLER_GET_STATUS *pStatus =
        (RM_PMU_PERF_CF_CONTROLLER_GET_STATUS *)pPayload;
    PERF_CF_CONTROLLERS *pControllers =
        PERF_CF_GET_CONTROLLERS();
    PERF_CF_CONTROLLER *pController =
        (PERF_CF_CONTROLLER *)pBoardObj;
    LwU8        i;
    FLCN_STATUS status = FLCN_OK;
    const LW2080_CTRL_PMGR_PWR_POLICIES_INFLECTION_POINTS_DISABLE_REQUEST *pRequest;

    // Call super-class impementation;
    status = boardObjIfaceModel10GetStatus(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        goto perfCfControllerIfaceModel10GetStatus_SUPER_exit;
    }

    // SUPER class specific implementation.
    for (i = 0; i < LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_MAX_LIMITS; i++)
    {
        pStatus->limitFreqkHz[i]  = pController->limitFreqkHz[i];
    }
    pStatus->iteration                  = pController->iteration;
    pStatus->bReset                     = pController->bReset;
    pStatus->bActive                    = boardObjGrpMaskBitGet(&(pControllers->maskActive),
                                            BOARDOBJ_GET_GRP_IDX(pBoardObj));
    PMU_ASSERT_OK_OR_GOTO(status,
        perfCfControllerInflectionPointsDisableRequestGetConst(
            pController, &pRequest),
        perfCfControllerIfaceModel10GetStatus_SUPER_exit);
    if (pRequest != NULL)
    {
        pStatus->inflectionPointsDisableRequest = *pRequest;
    }
    else
    {
        LW2080_CTRL_PMGR_PWR_POLICIES_INFLECTION_POINTS_DISABLE_REQUEST_INIT(
            &pStatus->inflectionPointsDisableRequest);
    }

perfCfControllerIfaceModel10GetStatus_SUPER_exit:
    return status;
}

/*!
 * @copydoc PerfCfControllerExelwte
 */
FLCN_STATUS
perfCfControllerExelwte_SUPER
(
    PERF_CF_CONTROLLER *pController
)
{
    FLCN_STATUS status = FLCN_OK;
    PWR_POLICIES_STATUS *pPwrPoliciesStatus;
    LW2080_CTRL_PMGR_PWR_POLICIES_INFLECTION_POINTS_DISABLE_REQUEST *pDisableRequest;

    if (pController->bReset)
    {
        pController->bReset = LW_FALSE;

        status = FLCN_ERR_STATE_RESET_NEEDED;
        goto perfCfControllerExelwte_SUPER_exit;
    }

    pController->iteration++;

    PMU_ASSERT_OK_OR_GOTO(status,
        perfCfControllerMultiplierDataPwrPoliciesStatusGet(
            pController->pMultData, &pPwrPoliciesStatus),
        perfCfControllerExelwte_SUPER_exit);
    PMU_ASSERT_OK_OR_GOTO(status,
        perfCfControllerInflectionPointsDisableRequestGet(
            pController, &pDisableRequest),
        perfCfControllerExelwte_SUPER_exit);
    if ((pPwrPoliciesStatus != NULL) &&
        (pDisableRequest != NULL))
    {
        pDisableRequest->timestamp = pPwrPoliciesStatus->timestamp;
        // Store the controller's index as client debugging data.
        pDisableRequest->clientData = BOARDOBJ_GET_GRP_IDX(&pController->super);
    }

perfCfControllerExelwte_SUPER_exit:
    return status;
}

/*!
 * @copydoc PerfCfControllersLoad
 */
FLCN_STATUS
perfCfControllersLoad
(
    PERF_CF_CONTROLLERS    *pControllers,
    LwBool                  bLoad
)
{
    PERF_CF_CONTROLLER *pController;
    LwBoardObjIdx       i;
    FLCN_STATUS         status      = FLCN_OK;
    OSTASK_OVL_DESC     ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfCfController)
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perf)              // Used by CONTROLLER_UTIL (to check for valid CLK domain)
        OSTASK_OVL_DESC_DEFINE_PERF_CF_PWR_MODEL_LOAD
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        // Nothing to do when there are no CONTROLLER objects.
        if (boardObjGrpMaskIsZero(&(pControllers->super.objMask)))
        {
            goto perfCfControllersLoad_exit;
        }

        if (bLoad)
        {
            // We cannot support PERF_CF controllers when there is no P-state.
            if (BOARDOBJGRP_IS_EMPTY(PSTATE))
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_NOT_SUPPORTED;
                goto perfCfControllersLoad_exit;
            }

            // Initialize limits (e.g. clkDomIdx)
            status = s_perfCfControllersLimitsInit(pControllers);
            if (status != FLCN_OK)
            {
                goto perfCfControllersLoad_exit;
            }

            //
            // Load all controllers and setup callback.
            //

            BOARDOBJGRP_ITERATOR_BEGIN(PERF_CF_CONTROLLER, pController, i, NULL)
            {
                status = s_perfCfControllerLoad(pController);
                if (status != FLCN_OK)
                {
                    break;
                }
            }
            BOARDOBJGRP_ITERATOR_END;

            if (!OS_TMR_CALLBACK_WAS_CREATED(&(OsCBPerfCfControllers.super)))
            {
                PERF_CF_CONTROLLER_MULTIPLIER_DATA *pMD;
                LwU32                               nowTicks;

                OsCBPerfCfControllers.pControllers = pControllers;

                appTaskCriticalEnter();
                {
                    nowTicks = lwrtosTaskGetTickCount32();

                    FOR_EACH_MULTIPLIER(pControllers, pMD)
                    {
                        pMD->expirationTimeTicks = nowTicks;
                    }

                    osTmrCallbackCreate(&(OsCBPerfCfControllers.super),         // pCallback
                        OS_TMR_CALLBACK_TYPE_RELWRRENT_TO_EXPECTED_SKIP_MISSED, // type
                        OVL_INDEX_ILWALID,                                      // ovlImem
                        s_perfCfControllersCallback,                            // pTmrCallbackFunc
                        LWOS_QUEUE(PMU, PERF_CF),                               // queueHandle
                        pControllers->baseSamplingPeriodus,                     // periodNormalus
                        pControllers->baseSamplingPeriodus,                     // periodSleepus
                        OS_TIMER_RELAXED_MODE_USE_NORMAL,                       // bRelaxedUseSleep
                        RM_PMU_TASK_ID_PERF_CF);                                // taskId
                    osTmrCallbackSchedule(&(OsCBPerfCfControllers.super));
                }
                appTaskCriticalExit();

                // Run the first controller cycle (reset) now.
                s_perfCfControllersCallback(&(OsCBPerfCfControllers.super));
             }
        }
        else
        {
            if (OS_TMR_CALLBACK_WAS_CREATED(&(OsCBPerfCfControllers.super)))
            {
                if (!osTmrCallbackCancel(&(OsCBPerfCfControllers.super)))
                {
                    PMU_BREAKPOINT();
                    status = FLCN_ERR_ILWALID_STATE;
                    goto perfCfControllersLoad_exit;
                }
            }
        }
    }
perfCfControllersLoad_exit:
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    return status;
}

/*!
 * @copydoc PerfCfControllersMaskActiveSet
 */
FLCN_STATUS
perfCfControllersMaskActiveSet
(
    PERF_CF_CONTROLLERS *pControllers,
    BOARDOBJGRPMASK_E32 *pMask
)
{
    PERF_CF_CONTROLLER *pController;
    LwBoardObjIdx       i;
    LwU8                j;
    FLCN_STATUS         status      = FLCN_OK;

    if (!boardObjGrpMaskIsSubset(pMask, &(pControllers->super.objMask)))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto perfCfControllersMaskActiveSet_exit;
    }

    // Skip when nothing has changed.
    if (boardObjGrpMaskIsEqual(&(pControllers->maskActive), pMask))
    {
        goto perfCfControllersMaskActiveSet_exit;
    }

    // Reset the controllers going from active to inactive.
    BOARDOBJGRP_ITERATOR_BEGIN(PERF_CF_CONTROLLER, pController, i, &(pControllers->maskActive.super))
    {
        if (!boardObjGrpMaskBitGet(pMask, i))
        {
            pController->bReset     = LW_TRUE;
            pController->iteration  = 0;

            for (j = 0; j < LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_MAX_LIMITS; j++)
            {
                pController->limitFreqkHz[j] =
                    LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_FREQ_KHZ_NO_REQUEST;
            }
        }
    }
    BOARDOBJGRP_ITERATOR_END;

    // Update the mask of active controllers.
    status = boardObjGrpMaskCopy(&(pControllers->maskActive), pMask);
    if (status != FLCN_OK)
    {
        goto perfCfControllersMaskActiveSet_exit;
    }

perfCfControllersMaskActiveSet_exit:
    return status;
}

/*!
 * @copydoc PerfCfControllersGetMinLimitIdxFromClkDomIdx
 */
LwBoardObjIdx
perfCfControllersGetMinLimitIdxFromClkDomIdx
(
    PERF_CF_CONTROLLERS    *pControllers,
    LwBoardObjIdx           clkDomIdx
)
{
    LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT *pLimit;
    LwBoardObjIdx   i;

    for (i = LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_IDX_MIN_START;
         i < LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_IDX_MAX_START;
         i++)
    {
        if (!LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_IDX_IS_MIN(i))
        {
            PMU_BREAKPOINT();
            goto perfCfControllersGetMinLimitIdxFromClkDomIdx_exit;
        }

        pLimit = &(pControllers->limits[i]);

        if (pLimit->clkDomIdx == clkDomIdx)
        {
            return i;
        }
    }

perfCfControllersGetMinLimitIdxFromClkDomIdx_exit:
    return PERF_CF_CONTROLLERS_LIMIT_IDX_ILWALID;
}

/*!
 * @copydoc PerfCfControllersGetMaxLimitIdxFromClkDomIdx
 */
LwBoardObjIdx
perfCfControllersGetMaxLimitIdxFromClkDomIdx
(
    PERF_CF_CONTROLLERS    *pControllers,
    LwBoardObjIdx           clkDomIdx
)
{
    LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT *pLimit;
    LwBoardObjIdx   i;

    for (i = LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_IDX_MAX_START;
         i < LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_MAX_LIMITS;
         i++)
    {
        if (!LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_IDX_IS_MAX(i))
        {
            PMU_BREAKPOINT();
            goto perfCfControllersGetMaxLimitIdxFromClkDomIdx_exit;
        }

        pLimit = &(pControllers->limits[i]);

        if (pLimit->clkDomIdx == clkDomIdx)
        {
            return i;
        }
    }

perfCfControllersGetMaxLimitIdxFromClkDomIdx_exit:
    return PERF_CF_CONTROLLERS_LIMIT_IDX_ILWALID;
}

/*!
 * @copydoc PerfCfControllersOptpUpdate
 */
FLCN_STATUS
perfCfControllersOptpUpdate
(
    PERF_CF_CONTROLLERS    *pControllers,
    LwUFXP8_24              perfRatio
)
{
    FLCN_STATUS status = FLCN_OK;

    pControllers->optpPerfRatio = perfRatio;

    //
    // The choices made by DLPPC_1X and OPTP to achieve perf targets will
    // conflict with each other, but lwrrently, DLPPC_1X has not been fully
    // updated to handle perf targets. Therefore, for now, we disable DLPPC_1X
    // when there is a perf target and re-enable it when there is not a perf
    // target.
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_POLICY) &&
        PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_DLPPC_1X))
    {
        //
        // Check if a DLPPC controller exists by looping through all
        // controllers, and if so, disable its PERF_CF_POLICY
        //
        const PERF_CF_CONTROLLER *pController;
        LwBoardObjIdx idx;

        BOARDOBJGRP_ITERATOR_BEGIN(PERF_CF_CONTROLLER, pController, idx, NULL)
        {
            if ((BOARDOBJ_GET_TYPE(&pController->super) ==
                    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_TYPE_DLPPC_1X) &&
                !((const PERF_CF_CONTROLLER_DLPPC_1X *)pController)->bOptpSupported)
            {
                PMU_ASSERT_OK_OR_GOTO(status,
                    perfCfPolicysDeactivateMaskSet(
                        PERF_CF_GET_POLICYS(),
                        LW2080_CTRL_PERF_PERF_CF_POLICY_LABEL_DLPPC_1X,
                        LW2080_CTRL_PERF_PERF_CF_POLICY_DEACTIVATE_REQUEST_ID_PMU_OPTP_CONFLICT,
                        (perfRatio !=
                            LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_OPTP_PERF_RATIO_INACTIVE)),
                    perfCfControllersOptpUpdate_exit);

                //
                // We only lwrrently have support for one DLPPC_1X POLICY we can
                // disable, so if we've found one DLPPC_1X controller and
                // disabled the POLICY, we can exit.
                //
                break;
            }
        }
        BOARDOBJGRP_ITERATOR_END;
    }

perfCfControllersOptpUpdate_exit:
    return status;
}

/*!
 * @brief   Wrapper to handle version differences for @ref
 *          PerfCfControllersPostInit interface.
 *
 * @return  FLCN_OK                 Success.
 * @return  FLCN_ERR_NOT_SUPPORTED  No PERF_CF_CONTROLLERS version supported.
 * @return  Others                  Errors propagated from callees.
 */
FLCN_STATUS
perfCfControllersPostInit(void)
{
    FLCN_STATUS status;

    ct_assert(PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLERS_1X) ||
              PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLERS_2X));

    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLERS_1X))
    {
        status = perfCfControllersPostInit_1X(&PERF_CF_GET_OBJ()->pControllers);
    }
    else if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLERS_2X))
    {
        status = perfCfControllersPostInit_2X(&PERF_CF_GET_OBJ()->pControllers);
    }
    else
    {
        // Can't happen due to ct_assert above.
        status = FLCN_ERR_NOT_SUPPORTED;
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfCfControllersPostInit_exit;
    }

perfCfControllersPostInit_exit:
    return status;
}

/*!
 * @copydoc PerfCfControllersPostInit
 */
FLCN_STATUS
perfCfControllersPostInit_SUPER
(
    PERF_CF_CONTROLLERS **ppControllers
)
{
    LwU8        i;
    FLCN_STATUS status = FLCN_OK;

    boardObjGrpMaskInit_E32(&((*ppControllers)->maskActive));

    (*ppControllers)->pMultDataHead     = NULL;
    (*ppControllers)->limitsArbErrCount = 0;
    (*ppControllers)->limitsArbErrLast  = 0;
    (*ppControllers)->optpPerfRatio =
        LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_OPTP_PERF_RATIO_INACTIVE;

    // Initialize the list of limit IDs.
    (*ppControllers)->limits[LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_IDX(_DRAM, _MIN)].id = LW2080_CTRL_PERF_PERF_LIMIT_ID_PERF_CF_CONTROLLER_DRAM_MIN;
    (*ppControllers)->limits[LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_IDX(_GPC,  _MIN)].id = LW2080_CTRL_PERF_PERF_LIMIT_ID_PERF_CF_CONTROLLER_GPC_MIN;
    (*ppControllers)->limits[LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_IDX(_LWD,  _MIN)].id = LW2080_CTRL_PERF_PERF_LIMIT_ID_PERF_CF_CONTROLLER_LWD_MIN;
    (*ppControllers)->limits[LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_IDX(_XBAR, _MIN)].id = LW2080_CTRL_PERF_PERF_LIMIT_ID_PERF_CF_CONTROLLER_XBAR_MAX;   // PP-TODO: Unused.
    (*ppControllers)->limits[LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_IDX(_DRAM, _MAX)].id = LW2080_CTRL_PERF_PERF_LIMIT_ID_PERF_CF_CONTROLLER_DRAM_MAX;
    (*ppControllers)->limits[LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_IDX(_GPC,  _MAX)].id = LW2080_CTRL_PERF_PERF_LIMIT_ID_PERF_CF_CONTROLLER_GPC_MAX;
    (*ppControllers)->limits[LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_IDX(_LWD,  _MAX)].id = LW2080_CTRL_PERF_PERF_LIMIT_ID_PERF_CF_CONTROLLER_LWD_MAX;
    (*ppControllers)->limits[LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_IDX(_XBAR, _MAX)].id = LW2080_CTRL_PERF_PERF_LIMIT_ID_PERF_CF_CONTROLLER_XBAR_MAX;

    for (i = 0; i < LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_MAX_LIMITS; i++)
    {
        (*ppControllers)->limits[i].freqkHz =
            LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_FREQ_KHZ_NO_REQUEST;
    }

    //
    // Allocate the PERF_LIMITS_CLIENT buffer by which PERF_CF will
    // communicate its PERF_LIMIT input to PERF task.
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_PERF_LIMIT))
    {
        status = perfLimitsClientInit(&((*ppControllers)->pLimitsClient),
            OVL_INDEX_DMEM(perfCfLimitClient),
            PMU_PERF_CF_PERF_LIMITS_CLIENTS_ENTRIES_MAX,
            LW_FALSE);

        (*ppControllers)->pLimitsClient->hdr.applyFlags = LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_ASYNC;
    }

    return status;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10SetHeader
 */
FLCN_STATUS
perfCfControllerIfaceModel10SetHeader_SUPER
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJGRP *pHdrDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_PERF_CF_CONTROLLER_BOARDOBJGRP_SET_HEADER *pHdr =
        (RM_PMU_PERF_CF_CONTROLLER_BOARDOBJGRP_SET_HEADER *)pHdrDesc;
    PERF_CF_CONTROLLERS    *pControllers    = (PERF_CF_CONTROLLERS *)pBObjGrp;
    FLCN_STATUS             status          = FLCN_OK;

    status = boardObjGrpIfaceModel10SetHeader(pModel10, pHdrDesc);
    if (status != FLCN_OK)
    {
        goto perfCfControllerIfaceModel10SetHeader_SUPER_exit;
    }

    pControllers->maxVGpuVMCount = pHdr->maxVGpuVMCount;

    pControllers->baseSamplingPeriodus = ((LwU32)pHdr->baseSamplingPeriodms) * 1000;

    if (pControllers->baseSamplingPeriodus == 0)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto perfCfControllerIfaceModel10SetHeader_SUPER_exit;
    }

perfCfControllerIfaceModel10SetHeader_SUPER_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10GetStatusHeader
 */
FLCN_STATUS
perfCfControllerIfaceModel10GetStatusHeader_SUPER
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJGRP *pBuf,
    BOARDOBJGRPMASK    *pMask
)
{
    BOARDOBJGRP *pBoardObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_PERF_CF_CONTROLLER_BOARDOBJGRP_GET_STATUS_HEADER *pHdr =
        (RM_PMU_PERF_CF_CONTROLLER_BOARDOBJGRP_GET_STATUS_HEADER *)pBuf;
    PERF_CF_CONTROLLERS *pControllers =
        (PERF_CF_CONTROLLERS *)pBoardObjGrp;
    LwU8        i;
    FLCN_STATUS status = FLCN_OK;
    LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_PROFILING *pProfiling;

    PMU_ASSERT_OK_OR_GOTO(status,
        perfCfControllersProfilingGet(pControllers, &pProfiling),
        perfCfControllerIfaceModel10GetStatusHeader_SUPER_exit);

    for (i = 0; i < LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_MAX_LIMITS; i++)
    {
        pHdr->limits[i] = pControllers->limits[i];
    }

    status = boardObjGrpMaskExport_E32(&(pControllers->maskActive),
        &(pHdr->maskActive));
    if (status != FLCN_OK)
    {
        goto perfCfControllerIfaceModel10GetStatusHeader_SUPER_exit;
    }

    pHdr->optpPerfRatio     = pControllers->optpPerfRatio;
    pHdr->limitsArbErrCount = pControllers->limitsArbErrCount;
    pHdr->limitsArbErrLast  = pControllers->limitsArbErrLast;

    if (pProfiling != NULL)
    {
        pHdr->profiling = *pProfiling;
    }

perfCfControllerIfaceModel10GetStatusHeader_SUPER_exit:
    return status;
}

/*!
 * @copydoc PerfCfControllersStatusGet
 */
FLCN_STATUS
perfCfControllersStatusGet
(
    PERF_CF_CONTROLLERS          *pControllers,
    PERF_CF_CONTROLLERS_STATUS   *pStatus
)
{
    PERF_CF_CONTROLLER  *pController;
    LwBoardObjIdx        i;
    FLCN_STATUS          status = FLCN_OK;

    if (!boardObjGrpMaskIsSubset(&(pStatus->mask), &(pControllers->super.objMask)))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto perfCfControllersStatusGet_exit;
    }

    BOARDOBJGRP_ITERATOR_BEGIN(PERF_CF_CONTROLLER, pController, i, &(pStatus->mask.super))
    {
        BOARDOBJ_IFACE_MODEL_10 *pObjModel10 =
            boardObjIfaceModel10FromBoardObjGet(&pController->super);

        status = s_perfCfControllerIfaceModel10GetStatus(
            pObjModel10,
            &pStatus->controllers[i].boardObj);

        if (status != FLCN_OK)
        {
            goto perfCfControllersStatusGet_exit;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

perfCfControllersStatusGet_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
/*!
 * @copydoc OsTmrCallbackFunc
 */
static FLCN_STATUS
s_perfCfControllersCallback
(
    OS_TMR_CALLBACK *pCallback
)
{
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_DEFINE_PERF_CF_CONTROLLERS_COMMON
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        perfReadSemaphoreTake();
        {
            PERF_CF_CONTROLLERS    *pControllers;
            BOARDOBJGRPMASK_E32     mask;
            FLCN_STATUS             status;
            LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_PROFILING *pProfiling = NULL;

            pControllers = ((PERF_CF_TMR_CALLBACK_CONTROLLERS *)pCallback)->pControllers;

            PMU_ASSERT_OK_OR_GOTO(status,
                perfCfControllersProfilingGet(pControllers, &pProfiling),
                s_perfCfControllersCallback_exit);

            perfCfControllersProfileReset(pProfiling);

            perfCfControllersProfileRegionBegin(
                pProfiling,
                LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_PROFILING_REGION_TOTAL);

            perfCfControllersProfileRegionBegin(
                pProfiling,
                LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_PROFILING_REGION_FILTER);

            boardObjGrpMaskInit_E32(&mask);

            // Perform multiplier data bookkeeping and get mask of expired controllers.
            status = s_perfCfControllersFilter(pControllers, &mask, pProfiling);
            if (status != FLCN_OK)
            {
                pControllers->limitsArbErrCount++;
                pControllers->limitsArbErrLast = status;
                goto s_perfCfControllersCallback_exit;
            }

            // Filter out inactive controllers.
            status = boardObjGrpMaskAnd(&mask, &mask, &(pControllers->maskActive));

            perfCfControllersProfileRegionEnd(
                pProfiling,
                LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_PROFILING_REGION_FILTER);

            if (status != FLCN_OK)
            {
                pControllers->limitsArbErrCount++;
                pControllers->limitsArbErrLast = status;
                goto s_perfCfControllersCallback_exit;
            }

            // Execute and apply limits.
            status = s_perfCfControllersEvaluate(pControllers, &mask, pProfiling);
            if (status != FLCN_OK)
            {
                pControllers->limitsArbErrCount++;
                pControllers->limitsArbErrLast = status;
                goto s_perfCfControllersCallback_exit;
            }

            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_PWR_POLICY_INFLECTION_POINTS_DISABLE))
            {
                PMU_ASSERT_OK_OR_GOTO(status,
                    s_perfCfControllersInflectionPointsHandle(pControllers),
                    s_perfCfControllersCallback_exit);
            }

s_perfCfControllersCallback_exit:
            perfCfControllersProfileRegionEnd(
                pProfiling,
                LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_PROFILING_REGION_TOTAL);
        }
        perfReadSemaphoreGive();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return FLCN_OK; // NJ-TODO-CB
}

//
// A "best-effort" was made to have the PWR_POLICY mult data work on non-ODP
// builds, but it was not actually tested. This ct_assert is left as evidence of
// that; if an effort is made to get the PWR_POLICY mult data working on
// non-ODP, then this can be removed.
//
#ifndef ON_DEMAND_PAGING_BLK
ct_assert(!PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_MULT_DATA_PWR_POLICY));
#endif

/*!
 * Helper function performing multiplier data bookkeeping and returning mask
 * of Controllers that have expired.
 *                                                                     .
 * @param[in]   pControllers    PERF_CF_CONTROLLERS object pointer.
 * @param[in]   pMaskExpired    E32 mask to hold the filtered Controllers.
 * @param[in]   pProfiling      Pointer to structure in which to profile
 *
 * @return  FLCN_OK     Controllers were successfully filtered
 * @return  other       Propagates return values from various calls
 */
static FLCN_STATUS
s_perfCfControllersFilter
(
    PERF_CF_CONTROLLERS    *pControllers,
    BOARDOBJGRPMASK_E32    *pMaskExpired,
    LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_PROFILING *pProfiling
)
{
    PERF_CF_CONTROLLER_MULTIPLIER_DATA *pMD;
    BOARDOBJGRPMASK_E32                 maskActive;
    FLCN_STATUS                         status        = FLCN_OK;
    LwU32                               nowTicks;
    OSTASK_OVL_DESC                     ovlDescListCommon[] = {
        OSTASK_OVL_DESC_DEFINE_PERF_CF_CONTROLLERS_FILTER_COMMON
    };
    OSTASK_OVL_DESC                     ovlDescListPerf[] = {
        OSTASK_OVL_DESC_DEFINE_PERF_CF_CONTROLLERS_FILTER_PERF
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescListCommon);
    {
        boardObjGrpMaskInit_E32(&maskActive);

        nowTicks = lwrtosTaskGetTickCount32();

        FOR_EACH_MULTIPLIER(pControllers, pMD)
        {
            // Check if multiplier has expired.
            if (!OS_TMR_IS_BEFORE(nowTicks, pMD->expirationTimeTicks))
            {
                LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_PROFILING_MULT_DATA_SAMPLE *pMdProfiling;
                PWR_POLICIES_STATUS *pPwrPoliciesStatus;

                PMU_ASSERT_OK_OR_GOTO(status,
                    perfCfControllersProfilingMultDataSampleGetNext(
                        pProfiling,
                        &pMdProfiling),
                    s_perfCfControllersFilter_exit);

                perfCfControllersProfileMultDataSampleMaskSet(
                        pMdProfiling,
                        &pMD->maskControllers);

                perfCfControllersProfileMultDataRegionBegin(
                    pMdProfiling,
                    LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_PROFILING_MULT_DATA_SAMPLE_REGION_TOTAL);

                PMU_ASSERT_OK_OR_GOTO(status,
                    perfCfControllerMultiplierDataPwrPoliciesStatusGet(
                        pMD, &pPwrPoliciesStatus),
                    s_perfCfControllersFilter_exit);

                // Compute next expiration time.
                do
                {
                    pMD->expirationTimeTicks += pMD->samplingPeriodTicks;
                } while (!OS_TMR_IS_BEFORE(nowTicks, pMD->expirationTimeTicks));

                // Query topology status (input data to controllers exelwtion).
                OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescListPerf);
                {
                    perfCfControllersProfileMultDataRegionBegin(
                        pMdProfiling,
                        LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_PROFILING_MULT_DATA_SAMPLE_REGION_PERF_CF_TOPOLOGYS_STATUS_GET);

                    status = perfCfTopologysStatusGet(PERF_CF_GET_TOPOLOGYS(),
                        &(pMD->topologysStatus));

                    perfCfControllersProfileMultDataRegionEnd(
                        pMdProfiling,
                        LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_PROFILING_MULT_DATA_SAMPLE_REGION_PERF_CF_TOPOLOGYS_STATUS_GET);
                }
                OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescListPerf);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto s_perfCfControllersFilter_exit;
                }

                if (pPwrPoliciesStatus != NULL)
                {
                    OSTASK_OVL_DESC ovlDescListPwrPolicy[] = {
                        OSTASK_OVL_DESC_DEFINE_PERF_CF_CONTROLLERS_FILTER_PWR_POLICY
                    };

                    perfCfControllersProfileMultDataRegionBegin(
                        pMdProfiling,
                        LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_PROFILING_MULT_DATA_SAMPLE_REGION_PWR_POLICIES_STATUS_GET);

                    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescListPwrPolicy);
                    ATTACH_AND_LOAD_OVERLAY_PWR_POLICY_CLIENT_LOCKED;
                    {
                        PMU_ASSERT_OK_OR_GOTO(status,
                            pwrPoliciesStatusGet(
                                PWR_POLICIES_GET(),
                                pPwrPoliciesStatus),
                            s_perfCfControllersFilter_policyDetach);
s_perfCfControllersFilter_policyDetach:
                        lwosNOP();
                    }
                    DETACH_OVERLAY_PWR_POLICY_CLIENT_LOCKED;
                    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescListPwrPolicy);

                    perfCfControllersProfileMultDataRegionEnd(
                        pMdProfiling,
                        LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_PROFILING_MULT_DATA_SAMPLE_REGION_PWR_POLICIES_STATUS_GET);
                }

                // Query pwr channels status (input data to controllers exelwtion).
                if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_MULT_DATA_PWR_CHANNEL))
                {
                    OSTASK_OVL_DESC                     ovlDescListPwr[] = {
                        OSTASK_OVL_DESC_DEFINE_PERF_CF_CONTROLLERS_FILTER_PWR
                    };

                    PWR_CHANNELS_STATUS *const pPwrChannelsStatus =
                        perfCfControllerMultiplierDataPwrChannelsStatusGet(pMD);
                    if (pPwrChannelsStatus != NULL)
                    {
                        perfCfControllersProfileMultDataRegionBegin(
                            pMdProfiling,
                            LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_PROFILING_MULT_DATA_SAMPLE_REGION_PWR_CHANNELS_STATUS_GET);
                        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescListPwr);
                        {
                            if (!boardObjGrpMaskIsZero(&pPwrChannelsStatus->mask))
                            {
                                status = pwrChannelsStatusGet(pPwrChannelsStatus);
                            }
                        }
                        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescListPwr);

                        perfCfControllersProfileMultDataRegionEnd(
                            pMdProfiling,
                            LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_PROFILING_MULT_DATA_SAMPLE_REGION_PWR_CHANNELS_STATUS_GET);
                        if (status != FLCN_OK)
                        {
                            PMU_BREAKPOINT();
                            goto s_perfCfControllersFilter_exit;
                        }
                    }
                }

                if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PM_SENSOR))
                {
                    OSTASK_OVL_DESC  ovlDescListPmSensors[] = {
                        OSTASK_OVL_DESC_DEFINE_PERF_CF_PM_SENSORS
                    };

                    PERF_CF_PM_SENSORS_STATUS *const pPmSensorsStatus =
                        perfCfControllerMultiplierDataPmSensorsStatusGet(pMD);
                    if ((pPmSensorsStatus != NULL) &&
                        !boardObjGrpMaskIsZero(&pPmSensorsStatus->mask))
                    {
                        perfCfControllersProfileMultDataRegionBegin(
                            pMdProfiling,
                            LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_PROFILING_MULT_DATA_SAMPLE_REGION_PERF_CF_PM_SENSORS_STATUS_GET);
                        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescListPmSensors);
                        {
                            status = perfCfPmSensorsStatusGet(
                                PERF_CF_PM_GET_SENSORS(),
                                pPmSensorsStatus);
                        }
                        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescListPmSensors);

                        perfCfControllersProfileMultDataRegionEnd(
                            pMdProfiling,
                            LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_PROFILING_MULT_DATA_SAMPLE_REGION_PERF_CF_PM_SENSORS_STATUS_GET);
                        if (status != FLCN_OK)
                        {
                            PMU_BREAKPOINT();
                            goto s_perfCfControllersFilter_exit;
                        }
                    }
                }

                // Include expired controllers in the current callback exelwtion.
                status = boardObjGrpMaskOr(pMaskExpired, pMaskExpired,
                    &(pMD->maskControllers));
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto s_perfCfControllersFilter_exit;
                }

                perfCfControllersProfileMultDataRegionEnd(
                    pMdProfiling,
                    LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_PROFILING_MULT_DATA_SAMPLE_REGION_TOTAL);
            }
        }
    }
s_perfCfControllersFilter_exit:
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescListCommon);
    return status;
}

/*!
 * Helper function exelwting the controllers, process the results, and trigger
 * changes to limits.
 *                                                                     .
 * @param[in]   pControllers        PERF_CF_CONTROLLERS object pointer.
 * @param[in]   pMaskActiveExpired  E32 mask to hold list of controllers to
 *                                  execute.
 * @param[in]   pProfiling          Pointer to structure in which to profile
 *
 * @return  FLCN_OK     Controllers were successfully exelwted and processed
 * @return  other       Propagates return values from various calls
 */
static FLCN_STATUS
s_perfCfControllersEvaluate
(
    PERF_CF_CONTROLLERS    *pControllers,
    BOARDOBJGRPMASK_E32    *pMaskActiveExpired,
    LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_PROFILING *pProfiling
)
{
    LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT *pLimit;
    const PERF_PSTATE_35_CLOCK_ENTRY *pcPstateClkFreq;
    PERF_CF_CONTROLLER         *pController;
    LwU32                       lastFreqkHz[LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_MAX_LIMITS];
    LW2080_CTRL_CLK_VF_INPUT    vfInput;
    LW2080_CTRL_CLK_VF_OUTPUT   vfOutput;
    CLK_DOMAIN                 *pDomain;
    CLK_DOMAIN_PROG            *pDomainProg;
    PSTATE_35                  *pPstate35;
    LwBoardObjIdx               i;
    LwBoardObjIdx               j;
    LwBool                      bNoPstate;
    LwBool                      bSkipPerfLimitUpdate = LW_FALSE;
    FLCN_STATUS                 status        = FLCN_OK;
    OSTASK_OVL_DESC             ovlDescList[] = {
        OSTASK_OVL_DESC_DEFINE_PERF_CF_CONTROLLERS_EVALUATE
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        perfCfControllersProfileRegionBegin(
            pProfiling,
            LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_PROFILING_REGION_CONTROLLERS_EXELWTE);

        // Execute active expired controllers.
        BOARDOBJGRP_ITERATOR_BEGIN(PERF_CF_CONTROLLER, pController, i, &(pMaskActiveExpired->super))
        {
            status = s_perfCfControllerExelwte(pController);
            if (status != FLCN_OK)
            {
                goto s_perfCfControllersEvaluate_detach;
            }
        }
        BOARDOBJGRP_ITERATOR_END;

        perfCfControllersProfileRegionEnd(
            pProfiling,
            LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_PROFILING_REGION_CONTROLLERS_EXELWTE);

        perfCfControllersProfileRegionBegin(
            pProfiling,
            LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_PROFILING_REGION_ARBITRATE);

        // Get the raw frequency floor and ceiling for all directly controlled clock domains.
        for (i = 0; i < LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_MAX_LIMITS; i++)
        {
            // Save old and clear for new frequencies.
            pLimit = &(pControllers->limits[i]);
            lastFreqkHz[i] = pLimit->freqkHz;
            pLimit->freqkHz = LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_FREQ_KHZ_NO_REQUEST;

            //
            // Skip arbitration for limits that are not supported (e.g.
            // DRAMCLK is fixed).
            //
            if (!pLimit->bSupported)
            {
                continue;
            }

            // Arbitrate among all active controllers.
            BOARDOBJGRP_ITERATOR_BEGIN(PERF_CF_CONTROLLER, pController, j, &(pControllers->maskActive.super))
            {
                // No request from controller.
                if (LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_FREQ_KHZ_NO_REQUEST ==
                        pController->limitFreqkHz[i])
                {
                    continue;
                }

                // First request. Just assign. No min/max.
                if (LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_FREQ_KHZ_NO_REQUEST ==
                        pLimit->freqkHz)
                {
                    pLimit->freqkHz = pController->limitFreqkHz[i];
                    continue;
                }

                // Find min of max's and max of min's.
                if (LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_IDX_IS_MIN(i))
                {
                    pLimit->freqkHz =
                        LW_MAX(pLimit->freqkHz, pController->limitFreqkHz[i]);
                }
                else
                {
                    pLimit->freqkHz =
                        LW_MIN(pLimit->freqkHz, pController->limitFreqkHz[i]);
                }
            }
            BOARDOBJGRP_ITERATOR_END;
        }

        perfCfControllersProfileRegionEnd(
            pProfiling,
            LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_PROFILING_REGION_ARBITRATE);

        perfCfControllersProfileRegionBegin(
            pProfiling,
            LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_PROFILING_REGION_POST_PROCESS);

        // Quantize the target frequency based on step size, DVCO and PState range.
        for (i = 0; i < LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_MAX_LIMITS; i++)
        {
            pLimit = &(pControllers->limits[i]);

            //
            // Skip quantization for limits that are not supported (e.g.
            // DRAMCLK is fixed).
            //
            if (!pLimit->bSupported)
            {
                continue;
            }

            // Skip quantization when there is no request.
            if (LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_FREQ_KHZ_NO_REQUEST ==
                    pLimit->freqkHz)
            {
                continue;
            }

            // Hack to workaround round down behavior below DVCO min of NAFLL clocks.
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_DVCO_MIN_WAR))
            {
                CLK_DOMAIN *pClkDomain;
                LwBool      bSkipPldivBelowDvcoMin = LW_FALSE;
                LwU16       dvcoMinFreqMHz         = 0;
                LwU32       dvcoMinFreqkHz         = 0;

                pClkDomain = BOARDOBJGRP_OBJ_GET(CLK_DOMAIN, pLimit->clkDomIdx);
                if (pClkDomain == NULL)
                {
                    PMU_BREAKPOINT();
                    status = FLCN_ERR_ILWALID_INDEX;
                    goto s_perfCfControllersEvaluate_detach;
                }

                status = clkNafllDvcoMinFreqMHzGet(
                            clkDomainApiDomainGet(pClkDomain),
                            &dvcoMinFreqMHz,
                            &bSkipPldivBelowDvcoMin);
                if ((status == FLCN_ERR_ILWALID_INDEX) ||
                    (bSkipPldivBelowDvcoMin))
                {
                    //
                    // Do nothing if
                    // 1. This CLK DOMAIN does not support NAFLL.
                    // 2. This CLK DOMAIN does not engage PLDIV below DVCO Min.
                    //
                    status = FLCN_OK;
                }
                else if (status != FLCN_OK)
                {
                    goto s_perfCfControllersEvaluate_detach;
                }
                else
                {
                    // Add 20MHz margin to account for potential DVCO min shift.
                    dvcoMinFreqkHz = (dvcoMinFreqMHz + 20) * 1000;

                    if (pLimit->freqkHz < dvcoMinFreqkHz / 4)
                    {
                        // Let it be. Likely limit = P8 min, effective = div 4.
                    }
                    else if (pLimit->freqkHz < dvcoMinFreqkHz / 2)
                    {
                        // Set to mid point between div1 and div2 to get div 2.
                        pLimit->freqkHz = dvcoMinFreqkHz * 3 / 4;
                    }
                    else if (pLimit->freqkHz < dvcoMinFreqkHz)
                    {
                        // Set to DVCO min + margin to get DVCO min.
                        pLimit->freqkHz = dvcoMinFreqkHz;
                    }
                }
            }

            //
            // Search to keep frequency within any P-state's range.
            //

            // Always cap to max P-state's max freq, if there is one.
            bNoPstate = LW_TRUE;

            BOARDOBJGRP_ITERATOR_DYNAMIC_CAST_BEGIN(PERF, PSTATE, 35, pPstate35,
                                                    j, NULL, &status,
                                                    s_perfCfControllersEvaluate_detach)
            {
                status = perfPstate35ClkFreqGetRef(pPstate35,
                    pLimit->clkDomIdx, &pcPstateClkFreq);
                if (status != FLCN_OK)
                {
                    goto s_perfCfControllersEvaluate_detach;
                }

                //
                // Do not set frequency floor when the floor value <= min frequency.
                // Issue 1:
                // Starting Hopper, we have multiple Pstate with same DRAM frequency.
                // Due to propagation, this results into Pstate stuck at max Pstate with
                // min POR frequency and never going down to idle Pstate aka P8.
                // Issue 2:
                // Do not set min LWDCLK limit <= min freq. We do not want the
                // side effect of raising GPCCLK to ~600MHz all the time.
                // (bug 200453295)
                //
                if (((PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_FREQ_FLOOR_AT_POR_MIN_DISABLED) &&
                     (LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_IDX_IS_MIN(i))) ||
                    (LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_IDX(_LWD, _MIN) == i)) &&
                    bNoPstate &&    // no Pstate before == first Pstate
                    (pLimit->freqkHz <= pcPstateClkFreq->min.freqVFMaxkHz))
                {
                    pLimit->freqkHz =
                        LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_FREQ_KHZ_NO_REQUEST;
                    break;
                }

                bNoPstate = LW_FALSE;

                if (pLimit->freqkHz < pcPstateClkFreq->min.freqVFMaxkHz)
                {
                    //
                    // Frequency is either below p[0]'s min or between
                    // p[j-1]'s max and p[j]'s min. Raise to p[j]'s min.
                    //
                    pLimit->freqkHz = pcPstateClkFreq->min.freqVFMaxkHz;
                    break;
                }
                else if (pLimit->freqkHz <= pcPstateClkFreq->max.freqVFMaxkHz)
                {
                    // Frequency is within range. Search is done.
                    break;
                }
            }
            BOARDOBJGRP_ITERATOR_DYNAMIC_CAST_END;

            // Skip further quantization when we are not setting min LWDCLK limit.
            if (LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_FREQ_KHZ_NO_REQUEST ==
                    pLimit->freqkHz)
            {
                continue;
            }

            // Cap frequency to max P-state's max.
            if (!bNoPstate && (pLimit->freqkHz > pcPstateClkFreq->max.freqVFMaxkHz))
            {
                pLimit->freqkHz = pcPstateClkFreq->max.freqVFMaxkHz;
            }

            // Quantize frequency to a VF point.

            pDomain = CLK_DOMAIN_GET(pLimit->clkDomIdx);
            if (pDomain == NULL)
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_ILWALID_STATE;
                goto s_perfCfControllersEvaluate_detach;
            }

            pDomainProg = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(pDomain, PROG);
            if (pDomainProg == NULL)
            {
                status = FLCN_ERR_ILWALID_STATE;
                PMU_BREAKPOINT();
                goto s_perfCfControllersEvaluate_detach;
            }

            LW2080_CTRL_CLK_VF_INPUT_INIT(&vfInput);
            LW2080_CTRL_CLK_VF_OUTPUT_INIT(&vfOutput);

            // Cap frequency to max if needed.
            vfInput.flags = FLD_SET_DRF(2080_CTRL_CLK, _VF_INPUT_FLAGS,
                        _VF_POINT_SET_DEFAULT, _YES, vfInput.flags);

            vfInput.value = pLimit->freqkHz /
                clkDomainFreqkHzScaleGet(pDomain);

            status = clkDomainProgFreqQuantize(
                        pDomainProg,                    // pDomainProg
                        &vfInput,                       // pInputFreq
                        &vfOutput,                      // pOutputFreq
                        LW_FALSE,                       // bFloor
                        LW_TRUE);                       // bBound
            if (status != FLCN_OK)
            {
                goto s_perfCfControllersEvaluate_detach;
            }

            pLimit->freqkHz = vfOutput.value *
                clkDomainFreqkHzScaleGet(pDomain);
        }

        //
        // Callwlate XBAR ceiling based on Fmax @ V propagation from all
        // directly controlled clock domains frequency.
        //
        if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_XBAR_FMAX_AT_VOLT))
        {
            PERF_LIMITS_VF_ARRAY     vfArrayIn;
            PERF_LIMITS_VF_ARRAY     vfArrayOut;
            PERF_LIMITS_PSTATE_RANGE pstateRange;
            BOARDOBJGRPMASK_E32      domainsMaskIn;
            BOARDOBJGRPMASK_E32      domainsMaskOut;
            LwBool                   bXbarMaxRequired = LW_FALSE;

            // Get XBAR MAX Limit.
            pLimit = &(pControllers->limits[LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_IDX(_XBAR, _MAX)]);

            // Init input
            PERF_LIMITS_VF_ARRAY_INIT(&vfArrayIn);
            PERF_LIMITS_VF_ARRAY_INIT(&vfArrayOut);
            boardObjGrpMaskInit_E32(&domainsMaskIn);
            boardObjGrpMaskInit_E32(&domainsMaskOut);
            vfArrayIn.pMask  = &domainsMaskIn;
            vfArrayOut.pMask = &domainsMaskOut;

            // No need to set the XBAR limit if all MAX limit are unset.
            for (i = 0; i < LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_MAX_CLKDOMS; i++)
            {
                LwU8 limitIdMax = LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_IDX_MAX_START + i;

                LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT *pLimitMax =
                    &(pControllers->limits[limitIdMax]);

                if ((!pLimitMax->bSupported) ||
                    (pLimitMax->freqkHz == LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_FREQ_KHZ_NO_REQUEST))
                {
                    continue;
                }

                // If at least one MAX limit is set, evaluate XBAR MAX.
                bXbarMaxRequired = LW_TRUE;
                break;
            }

            if (!bXbarMaxRequired)
            {
                pLimit->freqkHz =
                    LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_FREQ_KHZ_NO_REQUEST;
            }
            else
            {
                //
                // Init the propagation PSTATE range to the entire
                // PSTATE range, will constrain further based on
                // frequencies below.
                //
                {
                    BOARDOBJGRPMASK_E32 *pPstateMaskE32 = &(PSTATES_GET()->super.objMask);
                    pstateRange.values[PERF_LIMITS_VF_RANGE_IDX_MIN] = boardObjGrpMaskBitIdxLowest(pPstateMaskE32);
                    pstateRange.values[PERF_LIMITS_VF_RANGE_IDX_MAX] = boardObjGrpMaskBitIdxHighest(pPstateMaskE32);
                }

                // Build propagation tuple.
                for (i = 0; i < LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_MAX_CLKDOMS; i++)
                {
                    LwU8 limitIdMin = LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_IDX_MIN_START + i;
                    LwU8 limitIdMax = LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_IDX_MAX_START + i;

                    LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT *pLimitMin =
                        &(pControllers->limits[limitIdMin]);
                    LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT *pLimitMax =
                        &(pControllers->limits[limitIdMax]);

                    // Sanity check both min and max are of the same clock domain.
                    if (pLimitMin->clkDomIdx != pLimitMax->clkDomIdx)
                    {
                        PMU_BREAKPOINT();
                        status = FLCN_ERR_ILWALID_STATE;
                        goto s_perfCfControllersEvaluate_exit;
                    }

                    // Skip XBAR clock domain.
                    if (i == LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_CLKDOM_XBAR)
                    {
                        continue;
                    }

                    //
                    // Skip limits that are not supported (e.g.
                    // DRAMCLK is fixed).
                    //
                    if ((pLimitMin->bSupported) &&
                        (pLimitMin->freqkHz != LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_FREQ_KHZ_NO_REQUEST))
                    {
                        PERF_LIMITS_VF           vfDomain;
                        PERF_LIMITS_PSTATE_RANGE clkPstateRange;

                        boardObjGrpMaskBitSet(vfArrayIn.pMask, pLimitMin->clkDomIdx);
                        vfArrayIn.values[pLimitMin->clkDomIdx] = pLimitMin->freqkHz ;

                        if ((pLimitMax->bSupported) &&
                            (pLimitMax->freqkHz != LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_FREQ_KHZ_NO_REQUEST))
                        {
                            vfArrayIn.values[pLimitMin->clkDomIdx] =
                                LW_MIN(vfArrayIn.values[pLimitMin->clkDomIdx], pLimitMax->freqkHz);
                        }

                        //
                        // Look up the PSTATE range which contains the
                        // input frequency.  Use the maximum PSTATE to
                        // constrain the minimum PSTATE for propagation.
                        //
                        vfDomain.idx = pLimitMin->clkDomIdx;
                        vfDomain.value = vfArrayIn.values[pLimitMin->clkDomIdx];
                        PMU_ASSERT_OK_OR_GOTO(status,
                            perfLimitsFreqkHzToPstateIdxRange(&vfDomain, &clkPstateRange),
                            s_perfCfControllersEvaluate_detach);
                        pstateRange.values[PERF_LIMITS_VF_RANGE_IDX_MIN] = LW_MAX(
                            pstateRange.values[PERF_LIMITS_VF_RANGE_IDX_MIN],
                            clkPstateRange.values[PERF_LIMITS_VF_RANGE_IDX_MAX]);
                    }
                }

                // If input mask is unset, set default (zero) frequency for XBAR.
                if (boardObjGrpMaskIsZero(&domainsMaskIn))
                {
                    pLimit->freqkHz =
                        LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_FREQ_KHZ_NO_REQUEST;
                }
                // Propagate to XBAR frequency.
                else
                {
                    // Set output domain mask
                    boardObjGrpMaskBitSet(vfArrayOut.pMask, pLimit->clkDomIdx);

                    // Propagate from input -> output
                    PMU_ASSERT_OK_OR_GOTO(status,
                        perfLimitsFreqPropagate(&pstateRange, &vfArrayIn, &vfArrayOut,
                            LW2080_CTRL_CLK_CLK_PROP_TOP_IDX_ILWALID),
                        s_perfCfControllersEvaluate_exit);

                    // Final XBAR frequency to set the Fmax cap.
                    pLimit->freqkHz = vfArrayOut.values[pLimit->clkDomIdx];
                }
            }
        }

        perfCfControllersProfileRegionEnd(
            pProfiling,
            LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_PROFILING_REGION_POST_PROCESS);

        // Do not trigger if there is no change.
        for (i = 0; i < LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_MAX_LIMITS; i++)
        {
            pLimit = &(pControllers->limits[i]);
            if (lastFreqkHz[i] != pLimit->freqkHz)
            {
                break;
            }
        }
        if (LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_MAX_LIMITS == i)
        {
            bSkipPerfLimitUpdate = LW_TRUE;
            goto s_perfCfControllersEvaluate_detach;
        }
    }
s_perfCfControllersEvaluate_detach:
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    if ((!bSkipPerfLimitUpdate) && PMUCFG_FEATURE_ENABLED(PMU_PERF_PERF_LIMIT))
    {
        OSTASK_OVL_DESC ovlDescListPerfLimit[] = {
            OSTASK_OVL_DESC_DEFINE_PERF_CF_CONTROLLERS_PERF_LIMIT
        };

        perfCfControllersProfileRegionBegin(
            pProfiling,
            LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_PROFILING_REGION_SET_LIMITS);

        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescListPerfLimit);
        {
            PERF_LIMITS_CLIENT_SEMAPHORE_TAKE(pControllers->pLimitsClient);
            {
                for (i = 0; i < LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_MAX_LIMITS; i++)
                {
                    PERF_LIMITS_CLIENT_ENTRY *pEntry;
                    pLimit = &(pControllers->limits[i]);

                    //
                    // Skip PERF_CF_CONTROLLER limits which aren't
                    // supported on this PMU profile.
                    //
                    if (!pLimit->bSupported)
                    {
                        continue;
                    }

                    // Find corresponding entry in the PERF_LIMITS_CLIENT buffer for this PERF_CF_CONTROLLER limit's LIMIT_ID.
                    PMU_HALT_OK_OR_GOTO(status,
                        (((pEntry = perfLimitsClientEntryGet(pControllers->pLimitsClient, pLimit->id)) != NULL) ?
                                FLCN_OK : FLCN_ERR_ILWALID_STATE),
                        s_perfCfControllersEvaluate_exit);

                    if (LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_FREQ_KHZ_NO_REQUEST ==
                            pLimit->freqkHz)
                    {
                        PERF_LIMIT_CLIENT_CLEAR_LIMIT(
                            pEntry,
                            pLimit->id);
                    }
                    else
                    {
                        PERF_LIMITS_CLIENT_FREQ_LIMIT(
                            pEntry,
                            pLimit->id,
                            pLimit->clkDomIdx, pLimit->freqkHz);
                    }
                }

                status = perfLimitsClientArbitrateAndApply(pControllers->pLimitsClient);
            }
            PERF_LIMITS_CLIENT_SEMAPHORE_GIVE(pControllers->pLimitsClient);
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescListPerfLimit);

        perfCfControllersProfileRegionEnd(
            pProfiling,
            LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_PROFILING_REGION_SET_LIMITS);

        if (status != FLCN_OK)
        {
            goto s_perfCfControllersEvaluate_exit;
        }
    }

s_perfCfControllersEvaluate_exit:
    return status;
}

/*!
 * @brief   Construct MULTIPLIER data for all PERF_CF_CONTROLLERS.
 *
 * @param[in]   pControllers    PERF_CF_CONTROLLERS object pointer.
 *
 * @return  FLCN_OK     MULTIPLIER data constructed successfully
 * @return  other       Propagates return values from various calls
 */
static FLCN_STATUS
s_perfCfControllersMultDataConstruct
(
    PERF_CF_CONTROLLERS *pControllers
)
{
    PERF_CF_CONTROLLER_MULTIPLIER_DATA *pMD;
    PERF_CF_CONTROLLER                 *pController;
    LwBoardObjIdx                       i;
    FLCN_STATUS                         status = FLCN_OK;

    BOARDOBJGRP_ITERATOR_BEGIN(PERF_CF_CONTROLLER, pController, i, NULL)
    {
        // If Multiplier data were allready allocated => just look-up to re-use.
        FOR_EACH_MULTIPLIER(pControllers, pMD)
        {
            if (pMD->samplingMultiplier == pController->samplingMultiplier)
            {
                break;
            }
        }

        // Otherwise create new Multiplier data structure.
        if (pMD == NULL)
        {
            // Multiplier encountered for the first time => allocate node, ...
            pMD = lwosCallocType(pControllers->super.super.ovlIdxDmem, 1U,
                                 PERF_CF_CONTROLLER_MULTIPLIER_DATA);
            if (pMD == NULL)
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_NO_FREE_MEM;
                goto s_perfCfControllersMultDataConstruct_exit;
            }

            //
            // ... initialize it, ...
            //

            boardObjGrpMaskInit_E32(&(pMD->maskControllers));
            boardObjGrpMaskInit_E32(&(pMD->topologysStatus.mask));

            pMD->samplingMultiplier = pController->samplingMultiplier;

            pMD->samplingPeriodTicks =
                LWRTOS_TIME_US_TO_TICKS(pControllers->baseSamplingPeriodus);
            pMD->samplingPeriodTicks *= pMD->samplingMultiplier;

            // ... and insert it into the linked list.
            pMD->pNext                  = pControllers->pMultDataHead;
            pControllers->pMultDataHead = pMD;
        }

        status = s_perfCfControllerSetMultData(pController, pMD);
        if (status != FLCN_OK)
        {
            goto s_perfCfControllersMultDataConstruct_exit;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

s_perfCfControllersMultDataConstruct_exit:
    return status;
}

/*!
 * @brief   Checks all controllers and disables/enables @ref PWR_POLICIES
 *          inflection points if any controller has requested so.
 *
 * @param[in]   pControllers    Controllers to check
 *
 * @return  @ref FLCN_OK                Success
 * @return  @ref FLCN_ERR_ILWALID_STATE The inflection point disablement
 *                                      structure could not be retrieved from
 *                                      the @ref PWR_POLICIES
 * @return  Others                      Errors propagated from callees.
 */
static FLCN_STATUS
s_perfCfControllersInflectionPointsHandle
(
    PERF_CF_CONTROLLERS *pControllers
)
{
    FLCN_STATUS status = FLCN_OK;

    // Take the client lock before accessing PMGR/PWR_POLICIES data.
    ATTACH_AND_LOAD_OVERLAY_PWR_POLICY_CLIENT_LOCKED;
    {
        PERF_CF_CONTROLLER *pController;
        LwBoardObjIdx i;
        PWR_POLICIES_INFLECTION_POINTS_DISABLE *pDisable;
        LW2080_CTRL_PMGR_PWR_POLICIES_INFLECTION_POINTS_DISABLE_REQUEST_ID controllerRequestId =
            LW2080_CTRL_PMGR_PWR_POLICIES_INFLECTION_POINTS_DISABLE_REQUEST_ID_PERF_CF_CONTROLLERS__START;

        // Get the disable pointer and ensure it's not NULL
        PMU_ASSERT_OK_OR_GOTO(status,
            pwrPoliciesInflectionPointsDisableGet(
                PWR_POLICIES_GET(), &pDisable),
            s_perfCfControllersInflectionPointsHandle_detach);
        PMU_ASSERT_TRUE_OR_GOTO(status,
            (pDisable != NULL),
            FLCN_ERR_ILWALID_STATE,
            s_perfCfControllersInflectionPointsHandle_detach);

        // Copy the request for each controller to the PWR_POLICIES
        BOARDOBJGRP_ITERATOR_BEGIN(
            PERF_CF_CONTROLLER, pController, i, &pControllers->maskActive.super)
        {
            const LW2080_CTRL_PMGR_PWR_POLICIES_INFLECTION_POINTS_DISABLE_REQUEST *pRequest;

            PMU_ASSERT_OK_OR_GOTO(status,
                perfCfControllerInflectionPointsDisableRequestGetConst(
                    pController, &pRequest),
                s_perfCfControllersInflectionPointsHandle_detach);

            // If the request is valid, copy it over to the PWR_POLICIES
            if (LW2080_CTRL_PMGR_PWR_POLICIES_INFLECTION_POINTS_DISABLE_REQUEST_VALID(
                    pRequest))
            {
                //
                // Ensure that we haven't used all of the PERF_CF_CONTROLLERS
                // slots.
                //
                PMU_ASSERT_TRUE_OR_GOTO(status,
                    (controllerRequestId <=
                        LW2080_CTRL_PMGR_PWR_POLICIES_INFLECTION_POINTS_DISABLE_REQUEST_ID_PERF_CF_CONTROLLERS__END),
                    FLCN_ERR_ILWALID_STATE,
                    s_perfCfControllersInflectionPointsHandle_detach);

                pwrPoliciesInflectionPointsDisableRequest(
                    pDisable, controllerRequestId, pRequest);

                // Increment to the REQUEST_ID for the next controller.
                controllerRequestId++;
            }
        }
        BOARDOBJGRP_ITERATOR_END;

        // Clear any requests that were not actually used by any controllers.
        for (;
             controllerRequestId <=
                LW2080_CTRL_PMGR_PWR_POLICIES_INFLECTION_POINTS_DISABLE_REQUEST_ID_PERF_CF_CONTROLLERS__END;
             controllerRequestId++)
        {
            pwrPoliciesInflectionPointsDisableClear(
                pDisable, controllerRequestId);

        }
s_perfCfControllersInflectionPointsHandle_detach:
        lwosNOP();
    }
    DETACH_OVERLAY_PWR_POLICY_CLIENT_LOCKED;

    return status;
}

/*!
 * @copydoc PerfCfControllerLoad
 */
static FLCN_STATUS
s_perfCfControllerLoad
(
    PERF_CF_CONTROLLER *pController
)
{
    switch (BOARDOBJ_GET_TYPE(pController))
    {
        case LW2080_CTRL_PERF_PERF_CF_CONTROLLER_TYPE_UTIL:
            return perfCfControllerLoad_UTIL(pController);
        case LW2080_CTRL_PERF_PERF_CF_CONTROLLER_TYPE_UTIL_2X:
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_UTIL_2X))
            {
                return perfCfControllerLoad_UTIL_2X(pController);
            }
        case LW2080_CTRL_PERF_PERF_CF_CONTROLLER_TYPE_DLPPC_1X:
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_DLPPC_1X))
            {
                return perfCfControllerLoad_DLPPC_1X(pController);
            }
        case LW2080_CTRL_PERF_PERF_CF_CONTROLLER_TYPE_MEM_TUNE_1X:
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_MEM_TUNE))
            {
                return perfCfControllerLoad_MEM_TUNE(pController);
            }
        case LW2080_CTRL_PERF_PERF_CF_CONTROLLER_TYPE_MEM_TUNE_2X:
            if (perfCfControllerMemTune2xIsEnabled())
            {
                return perfCfControllerLoad_MEM_TUNE_2X(pController);
            }
        // No load for LW2080_CTRL_PERF_PERF_CF_CONTROLLER_TYPE_OPTP_2X.
    }

    return FLCN_OK;
}

/*!
 * @copydoc PerfCfControllerExelwte
 */
static FLCN_STATUS
s_perfCfControllerExelwte
(
    PERF_CF_CONTROLLER *pController
)
{
    switch (BOARDOBJ_GET_TYPE(pController))
    {
        case LW2080_CTRL_PERF_PERF_CF_CONTROLLER_TYPE_UTIL:
            return perfCfControllerExelwte_UTIL(pController);
        case LW2080_CTRL_PERF_PERF_CF_CONTROLLER_TYPE_UTIL_2X:
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_UTIL_2X))
            {
                return perfCfControllerExelwte_UTIL_2X(pController);
            }
        case LW2080_CTRL_PERF_PERF_CF_CONTROLLER_TYPE_OPTP_2X:
            return perfCfControllerExelwte_OPTP_2X(pController);
        case LW2080_CTRL_PERF_PERF_CF_CONTROLLER_TYPE_DLPPC_1X:
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_DLPPC_1X))
            {
                return perfCfControllerExelwte_DLPPC_1X(pController);
            }
        case LW2080_CTRL_PERF_PERF_CF_CONTROLLER_TYPE_MEM_TUNE_1X:
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_MEM_TUNE))
            {
                return perfCfControllerExelwte_MEM_TUNE(pController);
            }
        case LW2080_CTRL_PERF_PERF_CF_CONTROLLER_TYPE_MEM_TUNE_2X:
            if (perfCfControllerMemTune2xIsEnabled())
            {
                return perfCfControllerExelwte_MEM_TUNE_2X(pController);
            }
    }

    return FLCN_OK;
}

/*!
 * @copydoc PerfCfControllerSetMultData
 */
static FLCN_STATUS
s_perfCfControllerSetMultData
(
    PERF_CF_CONTROLLER                 *pController,
    PERF_CF_CONTROLLER_MULTIPLIER_DATA *pMultData
)
{
    // Let Controller know what Multiplier data structure to use.
    pController->pMultData = pMultData;

    // Let Multiplier data know what object(s) belong to it.
    boardObjGrpMaskBitSet(&(pMultData->maskControllers), BOARDOBJ_GET_GRP_IDX(&(pController->super)));

    // Set the topology the controller need.
    if (LW2080_CTRL_PERF_PERF_CF_CONTROLLER_INFO_TOPOLOGY_IDX_NONE !=
            pController->topologyIdx)
    {
        boardObjGrpMaskBitSet(&(pMultData->topologysStatus.mask), pController->topologyIdx);
    }

    // Skip the first controller cycle because topology status readings will not be valid.
    pController->bReset = LW_TRUE;

    switch (BOARDOBJ_GET_TYPE(pController))
    {
        case LW2080_CTRL_PERF_PERF_CF_CONTROLLER_TYPE_UTIL:
            return perfCfControllerSetMultData_UTIL(pController, pMultData);
        case LW2080_CTRL_PERF_PERF_CF_CONTROLLER_TYPE_UTIL_2X:
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_UTIL_2X))
            {
                return perfCfControllerSetMultData_UTIL_2X(pController, pMultData);
            }
        case LW2080_CTRL_PERF_PERF_CF_CONTROLLER_TYPE_OPTP_2X:
            return perfCfControllerSetMultData_OPTP_2X(pController, pMultData);
        case LW2080_CTRL_PERF_PERF_CF_CONTROLLER_TYPE_DLPPC_1X:
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_DLPPC_1X))
            {
                return perfCfControllerSetMultData_DLPPC_1X(pController, pMultData);
            }
        case LW2080_CTRL_PERF_PERF_CF_CONTROLLER_TYPE_MEM_TUNE_1X:
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_MEM_TUNE))
            {
                return perfCfControllerSetMultData_MEM_TUNE(pController, pMultData);
            }
        case LW2080_CTRL_PERF_PERF_CF_CONTROLLER_TYPE_MEM_TUNE_2X:
            if (perfCfControllerMemTune2xIsEnabled())
            {
                return perfCfControllerSetMultData_MEM_TUNE_2X(pController, pMultData);
            }
    }

    return FLCN_OK;
}

/*!
 * @Copydoc BoardObjIfaceModel10GetStatus
 */
static FLCN_STATUS
s_perfCfControllerIfaceModel10GetStatus
(
    BOARDOBJ_IFACE_MODEL_10               *pModel10,
    RM_PMU_BOARDOBJ        *pBuf
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLERS_1X))
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            perfCfControllerIfaceModel10GetStatus_1X(pModel10, pBuf),
            s_perfCfControllerIfaceModel10GetStatus_exit);
    }
    else if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLERS_2X))
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            perfCfControllerIfaceModel10GetStatus_2X(pModel10, pBuf),
            s_perfCfControllerIfaceModel10GetStatus_exit);
    }
    else
    {
        PMU_BREAKPOINT();
    }

s_perfCfControllerIfaceModel10GetStatus_exit:
    return status;
}

/*!
 * Helper function to initialize the limits structure (i.e. clkDomIdx) at load.
 *                                                                     .
 * @param[in]   pControllers    PERF_CF_CONTROLLERS object pointer.
 *
 * @return  FLCN_OK     Limits successfully initialized.
 * @return  other       Propagates return values from various calls
 */
static FLCN_STATUS
s_perfCfControllersLimitsInit
(
    PERF_CF_CONTROLLERS *pControllers
)
{
    CLK_DOMAIN     *pClkDom;
    LwU8            i;
    FLCN_STATUS     status          = FLCN_OK;
    OSTASK_OVL_DESC ovlDescList[]   = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfVf)
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perf)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        //
        // Find the clock domain index for DRAMCLK.
        //

        pClkDom = clkDomainsGetByDomain(clkWhich_MClk);
        if (pClkDom == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_NOT_SUPPORTED;
            goto s_perfCfControllersLimitsInit_exit;
        }

        pControllers->limits[LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_IDX(_DRAM, _MIN)].clkDomIdx =
        pControllers->limits[LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_IDX(_DRAM, _MAX)].clkDomIdx =
            BOARDOBJ_GET_GRP_IDX_8BIT(&pClkDom->super.super);

        pControllers->limits[LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_IDX(_DRAM, _MIN)].bSupported =
        pControllers->limits[LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_IDX(_DRAM, _MAX)].bSupported =
            LW_TRUE;

        // Find the clock domain index for GPCCLK.
        pClkDom = clkDomainsGetByDomain(clkWhich_GpcClk);
        if (pClkDom == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_NOT_SUPPORTED;
            goto s_perfCfControllersLimitsInit_exit;
        }

        pControllers->limits[LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_IDX(_GPC, _MIN)].clkDomIdx =
        pControllers->limits[LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_IDX(_GPC, _MAX)].clkDomIdx =
            BOARDOBJ_GET_GRP_IDX_8BIT(&pClkDom->super.super);

        pControllers->limits[LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_IDX(_GPC, _MIN)].bSupported =
        pControllers->limits[LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_IDX(_GPC, _MAX)].bSupported =
            LW_TRUE;

        // Find the clock domain index for LWDCLK.
        pClkDom = clkDomainsGetByDomain(clkWhich_LwdClk);
        if (pClkDom == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_NOT_SUPPORTED;
            goto s_perfCfControllersLimitsInit_exit;
        }

        pControllers->limits[LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_IDX(_LWD, _MIN)].clkDomIdx =
        pControllers->limits[LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_IDX(_LWD, _MAX)].clkDomIdx =
            BOARDOBJ_GET_GRP_IDX_8BIT(&pClkDom->super.super);

        pControllers->limits[LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_IDX(_LWD, _MIN)].bSupported =
        pControllers->limits[LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_IDX(_LWD, _MAX)].bSupported =
            LW_TRUE;

        // Find the clock domain index for XBARCLK.
        pClkDom = clkDomainsGetByDomain(clkWhich_XbarClk);
        if (pClkDom == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_NOT_SUPPORTED;
            goto s_perfCfControllersLimitsInit_exit;
        }

        pControllers->limits[LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_IDX(_XBAR, _MIN)].clkDomIdx =
        pControllers->limits[LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_IDX(_XBAR, _MAX)].clkDomIdx =
            BOARDOBJ_GET_GRP_IDX_8BIT(&pClkDom->super.super);

        // By default XBAR clock control is disabled.
        pControllers->limits[LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_IDX(_XBAR, _MIN)].bSupported =
            LW_FALSE;
        pControllers->limits[LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_IDX(_XBAR, _MAX)].bSupported =
            LW_FALSE;

        // Only enable XBAR max to support Fmax @ V bahavior on XBAR and its secondaries.
        if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_XBAR_FMAX_AT_VOLT))
        {
            pControllers->limits[LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_IDX(_XBAR, _MAX)].bSupported =
                LW_TRUE;
        }

        for (i = 0; i < LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_MAX_LIMITS; i++)
        {
            // Verify that clk domain is fixed. (e.g. DRAMCLK is not fixed.)
            if (clkDomainIsFixed(CLK_DOMAIN_GET(pControllers->limits[i].clkDomIdx)))
            {
                pControllers->limits[i].bSupported = LW_FALSE;
            }
        }
    }
s_perfCfControllersLimitsInit_exit:
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    return status;
}
