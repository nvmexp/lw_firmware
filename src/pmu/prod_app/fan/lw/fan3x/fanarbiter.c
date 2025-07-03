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
 * @file  fanarbiter.c
 * @brief FAN Fan Arbiter Model
 *
 * This module is a collection of functions managing and manipulating state
 * related to fan arbiters in the Fan Arbiter Table.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "fan/objfan.h"
#include "therm/objtherm.h"

#include "lwostimer.h"
#include "lwoslayer.h"
#include "main.h"

#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "rmpmusupersurfif.h"

/* ------------------------ Static Function Prototypes ---------------------- */
static OsTmrCallbackFunc (s_fanArbiterOsCallbackGeneric)
    GCC_ATTRIB_SECTION("imem_thermLibFanCommon", "s_fanArbiterOsCallbackGeneric")
    GCC_ATTRIB_USED();

static LwU32             s_fanArbiterCallbackGetTime(FAN_ARBITER *pArbiter)
    GCC_ATTRIB_SECTION("imem_thermLibFanCommon", "s_fanArbiterCallbackGetTime");
static LwU32             s_fanArbiterCallbackUpdateTime(FAN_ARBITER *pArbiter, LwU32 timeus)
    GCC_ATTRIB_SECTION("imem_thermLibFanCommon", "s_fanArbiterCallbackUpdateTime");
static FLCN_STATUS       s_fanArbiterCallback(void)
    GCC_ATTRIB_SECTION("imem_thermLibFanCommon", "s_fanArbiterCallback");

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/*!
 * Callback descriptor for FAN_ARBITER.
 */
static OS_TMR_CALLBACK OsCBFanArbiter;

/*!
 * Arbitrated fan control action considering all FAN_POLICIES for a FAN_ARBITER
 * when fan arbiter mode is @ref LW2080_CTRL_FAN_ARBITER_MODE_MAX.
 *
 * For MAX arbiter mode following is the arbitation scheme:
 * If all policies output INVALID control action, it's an error condition.
 * If any policy outputs INVALID control action, it's an error condition.
 * If all policies output RESTART control action then that wins.
 * If policies output mix of RESTART & STOP control action then RESTART wins.
 * If all policies output STOP control action then that wins.
 * If any policy outputs SPEED_CTRL action then that wins.
 */
static LwU8 fanCtrlActionArbModeMaxBitArray[8] = {
    LW2080_CTRL_FAN_CTRL_ACTION_ILWALID,     // 0
    LW2080_CTRL_FAN_CTRL_ACTION_RESTART,     // 1
    LW2080_CTRL_FAN_CTRL_ACTION_STOP,        // 2
    LW2080_CTRL_FAN_CTRL_ACTION_RESTART,     // 3
    LW2080_CTRL_FAN_CTRL_ACTION_SPEED_CTRL,  // 4
    LW2080_CTRL_FAN_CTRL_ACTION_SPEED_CTRL,  // 5
    LW2080_CTRL_FAN_CTRL_ACTION_SPEED_CTRL,  // 6
    LW2080_CTRL_FAN_CTRL_ACTION_SPEED_CTRL,  // 7
};

/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * Bit shift macros for preparing the offset into the bit array.
 */
#define FAN_ARBITER_FAN_CTRL_ACTION_RESTART_SHIFT       0x00
#define FAN_ARBITER_FAN_CTRL_ACTION_STOP_SHIFT          0x01
#define FAN_ARBITER_FAN_CTRL_ACTION_SPEED_CTRL_SHIFT    0x02

/* ------------------------- Public Functions ------------------------------- */

/*!
 * FAN_ARBITER handler for the RM_PMU_BOARDOBJ_CMD_GRP_SET interface.
 *
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
fanArbitersBoardObjGrpIfaceModel10Set
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    return BOARDOBJGRP_IFACE_MODEL_10_SET_AUTO_DMA(E32,      // _grpType
        FAN_ARBITER,                                // _class
        pBuffer,                                    // _pBuffer
        boardObjGrpIfaceModel10SetHeader,                 // _hdrFunc
        fanFanArbiterIfaceModel10SetEntry,                // _entryFunc
        all.fan.fanArbiterGrpSet,                   // _ssElement
        OVL_INDEX_DMEM(therm),                      // _ovlIdxDmem
        BoardObjGrpVirtualTablesNotImplemented);    // _ppObjectVtables
}

/*!
 * Super-class implementation.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
fanArbiterGrpIfaceModel10ObjSetImpl_SUPER
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_FAN_FAN_ARBITER_BOARDOBJ_SET *pSet =
        (RM_PMU_FAN_FAN_ARBITER_BOARDOBJ_SET *)pBoardObjDesc;
    FAN_ARBITER    *pArbiter;
    FLCN_STATUS     status;

    // Call into BOARDOBJ super constructor.
    status = boardObjGrpIfaceModel10ObjSet(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        goto fanArbiterGrpIfaceModel10ObjSetImpl_SUPER_exit;
    }
    pArbiter = (FAN_ARBITER *)*ppBoardObj;

    // Set member variables.
    pArbiter->mode                      = pSet->mode;
    pArbiter->samplingPeriodms          = pSet->samplingPeriodms;
    pArbiter->fanPoliciesControlUnit    = pSet->fanPoliciesControlUnit;

    // Cache the FAN_COOLER pointer corresponding to the given Fan Cooler Index.
    pArbiter->pCooler = FAN_COOLER_GET(pSet->coolerIdx);
    if (pArbiter->pCooler == NULL)
    {
        status = FLCN_ERR_ILWALID_INDEX;
        goto fanArbiterGrpIfaceModel10ObjSetImpl_SUPER_exit;
    }

    // Set the FAN_POLICY mask.
    boardObjGrpMaskInit_E32(&(pArbiter->fanPoliciesMask));
    status = boardObjGrpMaskImport_E32(&(pArbiter->fanPoliciesMask),
                                       &(pSet->fanPoliciesMask));
    if (status != FLCN_OK)
    {
        goto fanArbiterGrpIfaceModel10ObjSetImpl_SUPER_exit;
    }

fanArbiterGrpIfaceModel10ObjSetImpl_SUPER_exit:
    return status;
}

/*!
 * Load all FAN_ARBITERS.
 */
FLCN_STATUS
fanArbitersLoad(void)
{
    FAN_ARBITER    *pArbiter    = NULL;
    FLCN_STATUS     status      = FLCN_OK;
    LwBoardObjIdx   i;

    BOARDOBJGRP_ITERATOR_BEGIN(FAN_ARBITER, pArbiter, i, NULL)
    {
        status = fanArbiterLoad(pArbiter);
        if (status != FLCN_OK)
        {
            goto fanArbitersLoad_exit;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

    s_fanArbiterOsCallbackGeneric(NULL);

fanArbitersLoad_exit:
    return status;
}

/*!
 * @copydoc FanArbiterLoad
 */
FLCN_STATUS
fanArbiterLoad
(
    FAN_ARBITER    *pArbiter
)
{
    LwU32 lwrrentTimeus = thermGetLwrrentTimeInUs();

    // Initialize callbackTimeStampus.
    pArbiter->callbackTimestampus =
        lwrrentTimeus + pArbiter->samplingPeriodms * 1000;

    // Initialize fan control action.
    pArbiter->fanCtrlAction = LW2080_CTRL_FAN_CTRL_ACTION_ILWALID;

    // Initialize fan policy driving the fan.
    pArbiter->drivingPolicyIdx = LW2080_CTRL_BOARDOBJ_IDX_ILWALID;

    // Covering both PWM and RPM case.
    pArbiter->target.rpm = 0;

    return FLCN_OK;
}

/*!
 * Queries all FAN_ARBITERS.
 */
FLCN_STATUS
fanArbitersBoardObjGrpIfaceModel10GetStatus
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    return BOARDOBJGRP_IFACE_MODEL_10_GET_STATUS_AUTO_DMA(E32, // _grpType
        FAN_ARBITER,                            // _class
        pBuffer,                                // _pBuffer
        NULL,                                   // _hdrFunc
        fanFanArbiterIfaceModel10GetStatus,                 // _entryFunc
        all.fan.fanArbiterGrpGetStatus);        // _ssElement
}

/*!
 * @Copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
fanFanArbiterIfaceModel10GetStatus
(
    BOARDOBJ_IFACE_MODEL_10               *pModel10,
    RM_PMU_BOARDOBJ        *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    FAN_ARBITER    *pArbiter;
    RM_PMU_FAN_FAN_ARBITER_BOARDOBJ_GET_STATUS
                   *pStatus;
    FLCN_STATUS     status;

    // Call super-class implementation.
    PMU_HALT_OK_OR_GOTO(status,
        boardObjIfaceModel10GetStatus(pModel10, pPayload),
        fanFanArbiterIfaceModel10GetStatus_exit);

    pArbiter    = (FAN_ARBITER *)pBoardObj;
    pStatus     = (RM_PMU_FAN_FAN_ARBITER_BOARDOBJ_GET_STATUS *)pPayload;

    pStatus->fanCtrlAction    = pArbiter->fanCtrlAction;
    pStatus->drivingPolicyIdx = pArbiter->drivingPolicyIdx;

    switch (pArbiter->fanPoliciesControlUnit)
    {
        case LW2080_CTRL_FAN_COOLER_ACTIVE_CONTROL_UNIT_PWM:
        {
            pStatus->target.pwm = pArbiter->target.pwm;
            break;
        }
        case LW2080_CTRL_FAN_COOLER_ACTIVE_CONTROL_UNIT_RPM:
        {
            pStatus->target.rpm = pArbiter->target.rpm;
            break;
        }
        default:
        {
            status = FLCN_ERR_NOT_SUPPORTED;
            break;
        }
    }

fanFanArbiterIfaceModel10GetStatus_exit:
    return status;
}

/*!
 * @copydoc FanArbiterCallback
 */
FLCN_STATUS
fanArbiterCallback
(
    FAN_ARBITER    *pArbiter
)
{
    LwSFXP16_16     maxPwmTarget            = LW_TYPES_SFXP_INTEGER_MIN(16, 16);
    LwS32           maxRpmTarget            = LW_S32_MIN;
    FLCN_STATUS     status                  = FLCN_OK;
    LwBool          bFanCtrlActionSpeedCtrl = LW_FALSE;
    LwBool          bFanCtrlActionStop      = LW_FALSE;
    LwBool          bFanCtrlActionRestart   = LW_FALSE;
    LwU8            bitArrayOffset          = 0;
    LwBoardObjIdx   policyIdx               = LW2080_CTRL_BOARDOBJ_IDX_ILWALID;
    LwBoardObjIdx   policyIdxSpeedCtrl      = LW2080_CTRL_BOARDOBJ_IDX_ILWALID;
    LwBoardObjIdx   policyIdxFanStop        = LW2080_CTRL_BOARDOBJ_IDX_ILWALID;
    LwBoardObjIdx   policyIdxFanStart       = LW2080_CTRL_BOARDOBJ_IDX_ILWALID;

    FAN_POLICY     *pPolicy;
    LwU8            fanCtrlAction;

    // Sanity check that fan policy mask is non-zero.
    if (boardObjGrpMaskIsZero(&(pArbiter->fanPoliciesMask)))
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto s_fanArbiterCallback_exit;
    }

    // Only MAX mode is supported as of now.
    if (pArbiter->mode != LW2080_CTRL_FAN_ARBITER_MODE_MAX)
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto s_fanArbiterCallback_exit;
    }

    // Run control loop for all FAN_POLICIES and cache PWM/RPM.
    BOARDOBJGRP_ITERATOR_BEGIN(FAN_POLICY, pPolicy, policyIdx,
                                &pArbiter->fanPoliciesMask.super)
    {
        PMU_HALT_OK_OR_GOTO(status,
            fanPolicyCallback(pPolicy),
            s_fanArbiterCallback_exit);
    }
    BOARDOBJGRP_ITERATOR_END;

    // Fan control action per FAN_POLICY handling.
    BOARDOBJGRP_ITERATOR_BEGIN(FAN_POLICY, pPolicy, policyIdx,
                                &pArbiter->fanPoliciesMask.super)
    {
        PMU_HALT_OK_OR_GOTO(status,
            fanPolicyFanCtrlActionGet(pPolicy, &fanCtrlAction),
            s_fanArbiterCallback_exit);

        switch (fanCtrlAction)
        {
            // Query PWM/RPM from FAN_POLICIES and select MAX amongst them.
            case LW2080_CTRL_FAN_CTRL_ACTION_SPEED_CTRL:
            {
                bFanCtrlActionSpeedCtrl = LW_TRUE;
                switch (pArbiter->fanPoliciesControlUnit)
                {
                    case LW2080_CTRL_FAN_COOLER_ACTIVE_CONTROL_UNIT_PWM:
                    {
                        LwSFXP16_16 pwmTarget = LW_TYPES_SFXP_INTEGER_MIN(16, 16);

                        status = fanPolicyFanPwmGet(pPolicy, &pwmTarget);
                        if (pwmTarget > maxPwmTarget)
                        {
                            maxPwmTarget        = pwmTarget;
                            policyIdxSpeedCtrl  = policyIdx;
                        }
                        break;
                    }
                    case LW2080_CTRL_FAN_COOLER_ACTIVE_CONTROL_UNIT_RPM:
                    {
                        LwS32 rpmTarget = LW_S32_MIN;

                        status = fanPolicyFanRpmGet(pPolicy, &rpmTarget);
                        if (rpmTarget > maxRpmTarget)
                        {
                            maxRpmTarget        = rpmTarget;
                            policyIdxSpeedCtrl  = policyIdx;
                        }
                        break;
                    }
                    default:
                    {
                        status = FLCN_ERR_NOT_SUPPORTED;
                        break;
                    }
                }
                break;
            }
            case LW2080_CTRL_FAN_CTRL_ACTION_STOP:
            {
                bFanCtrlActionStop  = LW_TRUE;
                policyIdxFanStop    = policyIdx;
                break;
            }
            case LW2080_CTRL_FAN_CTRL_ACTION_RESTART:
            {
                bFanCtrlActionRestart = LW_TRUE;
                policyIdxFanStart     = policyIdx;
                break;
            }
            default:
            {
                status = FLCN_ERR_NOT_SUPPORTED;
                break;
            }
        }
    }
    BOARDOBJGRP_ITERATOR_END;

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_fanArbiterCallback_exit;
    }

    // Compute the offset into the bit array for MAX arbiter mode.
    if (bFanCtrlActionSpeedCtrl)
    {
        bitArrayOffset |= BIT(FAN_ARBITER_FAN_CTRL_ACTION_SPEED_CTRL_SHIFT);
    }
    if (bFanCtrlActionStop)
    {
        bitArrayOffset |= BIT(FAN_ARBITER_FAN_CTRL_ACTION_STOP_SHIFT);
    }
    if (bFanCtrlActionRestart)
    {
        bitArrayOffset |= BIT(FAN_ARBITER_FAN_CTRL_ACTION_RESTART_SHIFT);
    }

    // Find out arbitrated fan control action for MAX arbiter mode.
    pArbiter->fanCtrlAction = fanCtrlActionArbModeMaxBitArray[bitArrayOffset];

    // Fan control action per FAN_ARBITER handling.
    switch (pArbiter->fanCtrlAction)
    {
        case LW2080_CTRL_FAN_CTRL_ACTION_SPEED_CTRL:
        {
            // Set the target PWM/RPM and cache it.
            switch (pArbiter->fanPoliciesControlUnit)
            {
                case LW2080_CTRL_FAN_COOLER_ACTIVE_CONTROL_UNIT_PWM:
                {
                    status = fanCoolerPwmSet(pArbiter->pCooler, LW_TRUE, maxPwmTarget);
                    pArbiter->target.pwm = maxPwmTarget;
                    pArbiter->drivingPolicyIdx  = policyIdxSpeedCtrl;
                    break;
                }
                case LW2080_CTRL_FAN_COOLER_ACTIVE_CONTROL_UNIT_RPM:
                {
                    status = fanCoolerRpmSet(pArbiter->pCooler, maxRpmTarget);
                    pArbiter->target.rpm = maxRpmTarget;
                    pArbiter->drivingPolicyIdx  = policyIdxSpeedCtrl;
                    break;
                }
                default:
                {
                    status = FLCN_ERR_NOT_SUPPORTED;
                    break;
                }
            }
            break;
        }
        case LW2080_CTRL_FAN_CTRL_ACTION_STOP:
        {
            pArbiter->drivingPolicyIdx = policyIdxFanStop;

            PMU_HALT_OK_OR_GOTO(status,
                fanCoolerPwmSet(pArbiter->pCooler, LW_FALSE, 0),
                s_fanArbiterCallback_exit);

            switch (pArbiter->fanPoliciesControlUnit)
            {
                case LW2080_CTRL_FAN_COOLER_ACTIVE_CONTROL_UNIT_RPM:
                {
                    FAN_COOLER_ACTIVE_PWM_TACH_CORR *pCoolerTachCorr = (FAN_COOLER_ACTIVE_PWM_TACH_CORR *)pArbiter->pCooler;
  
                    //
                    // Do not update RPM target if user has requested RPM Sim.
                    // This is required since if user sets fan sim, GPU needs to
                    // honour the target level that is requested by user.
                    //
                    if (!pCoolerTachCorr->bRpmSimActive)
                    {
                        pCoolerTachCorr->rpmTarget = 0;
                    }
                    break;
                }
                default:
                {
                    break;
                }
            }
            break;
        }
        case LW2080_CTRL_FAN_CTRL_ACTION_RESTART:
        {
            pArbiter->drivingPolicyIdx = policyIdxFanStart;
            status = fanCoolerPwmSet(pArbiter->pCooler, LW_TRUE, 0);
            break;
        }
        default:
        {
            status = FLCN_ERR_NOT_SUPPORTED;
            break;
        }
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_fanArbiterCallback_exit;
    }

s_fanArbiterCallback_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */

/*!
 * @copydoc BoardObjGrpIfaceModel10SetEntry
 */
FLCN_STATUS
fanFanArbiterIfaceModel10SetEntry
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    RM_PMU_BOARDOBJ    *pBuf
)
{
    switch (pBuf->type)
    {
        case LW2080_CTRL_FAN_ARBITER_TYPE_V10:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_THERM_FAN_ARBITER_V10);
            return fanArbiterGrpIfaceModel10ObjSetImpl_V10(pModel10, ppBoardObj,
                    sizeof(FAN_ARBITER_V10), pBuf);
            break;
        }
    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * @ref     OsTmrCallbackFunc
 *
 * Generic fan callback.
 */
static FLCN_STATUS
s_fanArbiterOsCallbackGeneric
(
    OS_TMR_CALLBACK *pCallback
)
{
    if (Fan.bFan2xAndLaterEnabled)
    {
        OSTASK_OVL_DESC ovlDescList[] = {
            OSTASK_OVL_DESC_DEFINE_FAN_CTRL_COMMON_BASE
            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libPwm)
            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libFxpBasic)
        };

        if (!(Fan.bTaskDetached))
        {
            OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        }

        (void)s_fanArbiterCallback();

        if (!(Fan.bTaskDetached))
        {
            OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
        }
    }

    return FLCN_OK; // NJ-TODO-CB
}

/*!
 * @brief Returns the time, in microseconds the next callback should
 *        occur for the FAN_ARBITER specified by @ref pArbiter.
 *
 * @param[in]  pArbiter Pointer to the FAN_ARBITER object
 *
 * @return LwU32 The time, in microseconds the next callback should occur
 */
static LwU32
s_fanArbiterCallbackGetTime
(
    FAN_ARBITER    *pArbiter
)
{
    return pArbiter->callbackTimestampus;
}

/*!
 * @brief Updates the callback time for the FAN_ARBITER specified by @ref pArbiter.
 *
 * @param[in]  pArbiter Pointer to the FAN_ARBITER object
 * @param[in]  timeus   The current time in microseconds
 *
 * @return LwU32 The updated time, in microseconds the next callback should occur.
 */
static LwU32
s_fanArbiterCallbackUpdateTime
(
    FAN_ARBITER    *pArbiter,
    LwU32           timeus
)
{
    LwU32   samplingPeriodus    = (LwU32)pArbiter->samplingPeriodms * 1000;
    LwU32   numElapsedPeriods;

    //
    // The following callwlation is done for a couple of reasons:
    // 1. The RM went into a STATE_UNLOAD that may have caused us to miss one
    //    or more callbacks. We are now handling the callback during the
    //    STATE_LOAD.
    // 2. The overall callback handling took long enough that this arbiter may
    //    have multiple periods expire.
    //

    numElapsedPeriods = (LwU32)(((timeus - pArbiter->callbackTimestampus) /
                                  samplingPeriodus) + 1);
    pArbiter->callbackTimestampus +=
        (numElapsedPeriods * samplingPeriodus);

    return pArbiter->callbackTimestampus;
}

/*!
 * Fan arbiter callback helper.
 */
static FLCN_STATUS
s_fanArbiterCallback(void)
{
    FLCN_STATUS     status = FLCN_OK;
    FAN_ARBITER    *pArbiter;
    LwU32           lwrrentTimeus;
    LwU32           arbiterTimeus;
    LwU32           callbackTimeus;
    LwU32           callbackDelayus;
    LwBoardObjIdx   i;

    do
    {
        LwU32 timeTillCallbackus = FAN_ARBITER_TIME_INFINITY;
        LwU32 timeTillArbiterus;

        lwrrentTimeus = thermGetLwrrentTimeInUs();

        BOARDOBJGRP_ITERATOR_BEGIN(FAN_ARBITER, pArbiter, i, NULL)
        {
            arbiterTimeus = s_fanArbiterCallbackGetTime(pArbiter);

            //
            // Determine if the arbiter's callback timer has expired; if so,
            // execute the arbiter's callback.
            //
            if (osTmrGetTicksDiff(arbiterTimeus, lwrrentTimeus) >= 0)
            {
                // Execute timer callback
                PMU_HALT_OK_OR_GOTO(status,
                    fanArbiterCallback(pArbiter),
                    s_fanArbiterCallback_exit);

                // Update arbiter with a new callback time.
                arbiterTimeus = s_fanArbiterCallbackUpdateTime(pArbiter, lwrrentTimeus);
            }

            //
            // In order to find the the earliest arbiter expiration, we must use
            // the difference between the arbiter time and the current time.
            // The above check 'osTmrGetTicksDiff(arbiterTimeus, lwrrentTimeus)'
            // ensures that this difference is always positive so no need to
            // check for signed overflow.
            //
            timeTillArbiterus = arbiterTimeus - lwrrentTimeus;

            // Save the earliest arbiter expiration to timeTillCallback.
            timeTillCallbackus = LW_MIN(timeTillCallbackus, timeTillArbiterus);
        }
        BOARDOBJGRP_ITERATOR_END;

        callbackTimeus = lwrrentTimeus + timeTillCallbackus;

        //
        // Need to determine if a lower index arbiter's timer expired while
        // processing higher index arbiters (i.e. arbiter B's timer expired
        // while processing arbiter D's timer). Repeat the process if a timer
        // expired while processing.
        //
        lwrrentTimeus = thermGetLwrrentTimeInUs();
    } while (osTmrGetTicksDiff(callbackTimeus, lwrrentTimeus) >= 0);

    callbackDelayus = callbackTimeus - lwrrentTimeus;

    if (!OS_TMR_CALLBACK_WAS_CREATED(&OsCBFanArbiter))
    {
        osTmrCallbackCreate(&OsCBFanArbiter,                // pCallback
            OS_TMR_CALLBACK_TYPE_RELWRRENT_TO_ACTUAL_END,   // type
            OVL_INDEX_IMEM(thermLibFanCommon),              // ovlImem
            s_fanArbiterOsCallbackGeneric,                  // pTmrCallbackFunc
            LWOS_QUEUE(PMU, THERM),                         // queueHandle
            callbackDelayus,                                // periodNormalus
            callbackDelayus,                                // periodSleepus
            OS_TIMER_RELAXED_MODE_USE_NORMAL,               // bRelaxedUseSleep
            RM_PMU_TASK_ID_THERM);                          // taskId
        osTmrCallbackSchedule(&OsCBFanArbiter);
    }
    else
    {
        // Update callback period
        osTmrCallbackUpdate(&OsCBFanArbiter, callbackDelayus, callbackDelayus,
                            OS_TIMER_RELAXED_MODE_USE_NORMAL, OS_TIMER_UPDATE_USE_BASE_LWRRENT);
    }

s_fanArbiterCallback_exit:
    return status;
}
