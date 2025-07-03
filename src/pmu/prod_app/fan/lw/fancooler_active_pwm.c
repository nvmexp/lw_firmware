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
 * @file  fancooler_active_pwm.c
 * @brief FAN Fan Cooler Specific to ACTIVE_PWM
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "fan/fancooler.h"
#include "lib/lib_fxp.h"
#include "pmu_objpmgr.h"
#include "therm/objtherm.h"
#include "pmu_objtimer.h"

/* ------------------------ Static Function Prototypes ---------------------- */
#if !PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_COOLER_HW_MAX_FAN)
static LwU32           _fanCoolerViolationCountersSum(LwU32 evtMask);
#endif
static LwBool          s_fanCoolerMaxFanCheck(FAN_COOLER *pCooler);

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * Construct a FAN_COOLER_ACTIVE_PWM object.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
fanCoolerGrpIfaceModel10ObjSetImpl_ACTIVE_PWM
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_FAN_FAN_COOLER_ACTIVE_PWM_BOARDOBJ_SET *pCoolerAPSet    =
        (RM_PMU_FAN_FAN_COOLER_ACTIVE_PWM_BOARDOBJ_SET *)pBoardObjDesc;
    LwBool                                         bFirstConstruct = (*ppBoardObj == NULL);
    FAN_COOLER_ACTIVE_PWM                         *pCoolerAP;
    FLCN_STATUS                                    status;

    if (PMUCFG_FEATURE_ENABLED(PMU_SELWRITY_HARDENING))
    {
        // Sanity check minimum and maximum electrical fan speed.
        if ((pCoolerAPSet->pwmMin > pCoolerAPSet->pwmMax) ||
            // Sanity check max PWM.
            (pCoolerAPSet->pwmMax > LW_TYPES_U32_TO_UFXP_X_Y(16, 16, 1)))
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto fanCoolerGrpIfaceModel10ObjSetImpl_ACTIVE_PWM_exit;
        }

        // Sanity check MAX Fan feature settings.
        if (pCoolerAPSet->maxFanMinTime1024ns != 0)
        {
            if ((pCoolerAPSet->maxFanPwm == LW_TYPES_U32_TO_UFXP_X_Y(16, 16, 0)) || // Max Fan PWM shouldn't be 0.
                (pCoolerAPSet->maxFanPwm > pCoolerAPSet->pwmMax) || // Max Fan PWM shouldn't be greater than Maximum PWM.
                (pCoolerAPSet->maxFanEvtMask == RM_PMU_THERM_EVENT_MASK_NONE)) // Max Fan is enabled but not configured for any event.
            {
                status = FLCN_ERR_ILWALID_ARGUMENT;
                goto fanCoolerGrpIfaceModel10ObjSetImpl_ACTIVE_PWM_exit;
            }

            if ((PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_COOLER_HW_MAX_FAN)) &&
                !RM_PMU_PMGR_PWM_SOURCE_IS_BIT_SET(pCoolerAPSet->pwmSource, MASK_HW_MAX_FAN)) // PWM source cannot be other than THERM or SCI as HW MAXFAN is enabled.
            {
                status = FLCN_ERR_ILWALID_ARGUMENT;
                goto fanCoolerGrpIfaceModel10ObjSetImpl_ACTIVE_PWM_exit;
            }
        }
        else
        {
            if (pCoolerAPSet->maxFanPwm != LW_TYPES_U32_TO_UFXP_X_Y(16, 16, 0)) // Max Fan Min Hold Time shouldn't be 0 when Max Fan PWM is non-zero.
            {
                status = FLCN_ERR_ILWALID_ARGUMENT;
                goto fanCoolerGrpIfaceModel10ObjSetImpl_ACTIVE_PWM_exit;
            }
        }
    }

    // Construct and initialize parent-class object.
    status = fanCoolerGrpIfaceModel10ObjSetImpl_ACTIVE(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        goto fanCoolerGrpIfaceModel10ObjSetImpl_ACTIVE_PWM_exit;
    }
    pCoolerAP = (FAN_COOLER_ACTIVE_PWM *)*ppBoardObj;

    // Set member variables.
    pCoolerAP->pwmMin              = pCoolerAPSet->pwmMin;
    pCoolerAP->pwmMax              = pCoolerAPSet->pwmMax;
    pCoolerAP->period              = pCoolerAPSet->period;
    pCoolerAP->maxFanEvtMask       = pCoolerAPSet->maxFanEvtMask;
    pCoolerAP->maxFanPwm           = pCoolerAPSet->maxFanPwm;
    pCoolerAP->maxFanMinTime1024ns = pCoolerAPSet->maxFanMinTime1024ns;
    pCoolerAP->bMaxFanActive       = LW_FALSE;
    pCoolerAP->bPwmIlwerted        = pCoolerAPSet->bPwmIlwerted;
    pCoolerAP->bPwmSimActive       = pCoolerAPSet->bPwmSimActive;
    pCoolerAP->pwmSim              = pCoolerAPSet->pwmSim;

    if (bFirstConstruct)
    {
        // Construct PWM source descriptor data.
        pCoolerAP->pPwmSrcDesc =
            pwmSourceDescriptorConstruct(pBObjGrp->ovlIdxDmem, pCoolerAPSet->pwmSource);
        if (pCoolerAP->pPwmSrcDesc == NULL)
        {
            status = FLCN_ERR_ILWALID_STATE;
            goto fanCoolerGrpIfaceModel10ObjSetImpl_ACTIVE_PWM_exit;
        }

        pCoolerAP->pwmSource = pCoolerAPSet->pwmSource;
    }
    else
    {
        if (pCoolerAP->pwmSource != pCoolerAPSet->pwmSource)
        {
            status = FLCN_ERR_ILWALID_STATE;
            goto fanCoolerGrpIfaceModel10ObjSetImpl_ACTIVE_PWM_exit;
        }
    }

fanCoolerGrpIfaceModel10ObjSetImpl_ACTIVE_PWM_exit:
    return status;
}

/*!
 * Class implementation.
 *
 * @copydoc FanCoolerLoad
 */
FLCN_STATUS
fanCoolerActivePwmLoad
(
    FAN_COOLER *pCooler
)
{
    FAN_COOLER_ACTIVE_PWM  *pCool = (FAN_COOLER_ACTIVE_PWM *)pCooler;
    FLCN_STATUS             status;

#if !PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_COOLER_HW_MAX_FAN)
    pCool->maxFalwiolationCountersSum =
        _fanCoolerViolationCountersSum(pCool->maxFanEvtMask);
#endif

    // Call super-class implementation.
    status = fanCoolerActiveLoad(pCooler);
    if (status != FLCN_OK)
    {
        return status;
    }

    // Get current PWM rate.
    status = fanCoolerPwmGet(pCooler, LW_FALSE, &pCool->pwmLwrrent);
    if (status != FLCN_OK)
    {
        return status;
    }

    pCool->pwmRequested = pCool->pwmLwrrent;

    // Initialize HW MAXFAN feature.
    if (PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_COOLER_HW_MAX_FAN) &&
        FAN_COOLER_MAX_FAN_IS_ENABLED(pCool))
    {
        LwU32  maxFanDutyCycle;

        maxFanDutyCycle = multUFXP16_16(pCool->period, pCool->maxFanPwm);
        if (pCool->bPwmIlwerted)
        {
            maxFanDutyCycle = pCool->period - maxFanDutyCycle;
        }
        pmgrHwMaxFanInit_HAL(&Pmgr, pCool->pwmSource, maxFanDutyCycle);
    }

    return FLCN_OK;
}

/*!
 * @copydoc FanCoolerPwmSet
 */
FLCN_STATUS
fanCoolerActivePwmPwmSet
(
    FAN_COOLER *pCooler,
    LwBool      bBound,
    LwSFXP16_16 pwm
)
{
    FAN_COOLER_ACTIVE_PWM  *pCool = (FAN_COOLER_ACTIVE_PWM *)pCooler;
    FLCN_STATUS             status;
    LwS32                   dutyCycle;

    // Supporting fanSim feature.
    if (pCool->bPwmSimActive)
    {
        pwm = pCool->pwmSim;
    }

    // Failsafe: Check if Fan speed should be set to MAX.
    if (s_fanCoolerMaxFanCheck(pCooler))
    {
        pwm = pCool->maxFanPwm;
    }

    // Bound the requested PWM settings, if requested.
    if (bBound)
    {
        pwm = LW_MAX(pwm, pCool->pwmMin);
        pwm = LW_MIN(pwm, pCool->pwmMax);
    }

    // Include direct {scaling, offset} transformation (once needed).

    dutyCycle = multSFXP16_16(pCool->period, pwm);

    status = pwmParamsSet(pCool->pPwmSrcDesc, dutyCycle, pCool->period,
                          LW_TRUE, pCool->bPwmIlwerted);
    if (status == FLCN_OK)
    {
        // Cache requested PWM to ACTIVE_PWM FAN_COOLER.
        pCool->pwmRequested = pwm;

        pwm = LW_TYPES_S32_TO_SFXP_X_Y_SCALED(16, 16, dutyCycle, pCool->period);

        // Include ilwerse {scaling, offset} transformation (once needed).

        // Cache actual PWM applied into HW.
        pCool->pwmLwrrent = pwm;
    }

    return status;
}

/*!
 * Class implementation.
 *
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
fanCoolerActivePwmIfaceModel10GetStatus
(
    BOARDOBJ_IFACE_MODEL_10               *pModel10,
    RM_PMU_BOARDOBJ        *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_FAN_FAN_COOLER_ACTIVE_PWM_BOARDOBJ_GET_STATUS 
                           *pCoolerStatus;
    FAN_COOLER_ACTIVE_PWM  *pCool;
    FLCN_STATUS             status;

    // Call super-class implementation.
    status = fanCoolerActiveIfaceModel10GetStatus(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        return status;
    }
    pCool          = (FAN_COOLER_ACTIVE_PWM *)pBoardObj;
    pCoolerStatus  = 
        (RM_PMU_FAN_FAN_COOLER_ACTIVE_PWM_BOARDOBJ_GET_STATUS *)pPayload;

    // Set current fan speed.
    pCoolerStatus->pwmLwrrent    = pCool->pwmLwrrent;
    pCoolerStatus->pwmRequested  = pCool->pwmRequested;
    pCoolerStatus->bMaxFanActive = pCool->bMaxFanActive;

    return FLCN_OK;
}

/* ------------------------- Private Functions ------------------------------ */

/*!
 * @copydoc FanCoolerPwmGet
 */
FLCN_STATUS
fanCoolerActivePwmPwmGet
(
    FAN_COOLER     *pCooler,
    LwBool          bActual,
    LwSFXP16_16    *pPwm
)
{
    FAN_COOLER_ACTIVE_PWM  *pCool = (FAN_COOLER_ACTIVE_PWM *)pCooler;
    FLCN_STATUS             status;
    LwU32                   dutyCycle;
    LwU32                   period;
    LwBool                  bDone;

    if (PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_COOLER_ACTIVE_PWM_TACH_CORR_ACT_CORR) &&
        bActual)
    {
        status = pwmParamsActGet(pCool->pwmSource, &dutyCycle, &period,
                                 pCool->bPwmIlwerted);
        if (status == FLCN_OK)
        {
            //
            // ((19.0 << 12 = 19.12) / 19.0 = 1.12) << 4 = 1.16 = 16.16
            // While result has 16 fractional bits actual precision is 12 bits.
            //
            *pPwm = LW_TYPES_S32_TO_SFXP_X_Y(28, 4,
                LW_TYPES_S32_TO_SFXP_X_Y_SCALED(20, 12, dutyCycle, period));
            // Include ilwerse {scaling, offset} transformation (once needed).
        }
    }
    else
    {
        status = pwmParamsGet(pCool->pPwmSrcDesc, &dutyCycle, &period, &bDone,
                              pCool->bPwmIlwerted);
        if (status == FLCN_OK)
        {
            // (13.0 << 16 = 13.16) / 13.0 = 1.16 = 16.16
            *pPwm =
                LW_TYPES_S32_TO_SFXP_X_Y_SCALED(16, 16, dutyCycle, period);
            // Include ilwerse {scaling, offset} transformation (once needed).
        }
    }

    return status;
}

#if !PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_COOLER_HW_MAX_FAN)
/*!
 * Computes sum of violation counters for all events specified in the evtMask.
 *
 * @param[in]   evtMask RM_PMU_THERM_EVENT_<xyz> bitmask of requested events
 *
 * @return  Sum of violation counters specified by evtMask
 */
static LwU32
_fanCoolerViolationCountersSum
(
    LwU32   evtMask
)
{
    FLCN_STATUS status;
    LwU32       sum = 0;
    LwU32       cnt;
    LwU8        i;

    FOR_EACH_INDEX_IN_MASK(32, i, evtMask)
    {
        status = thermEventViolationGetCounter32(i, &cnt);
        if (status != FLCN_OK)
        {
            // RM should never send incorrect event mask.
            PMU_HALT();
        }
        sum += cnt;
    }
    FOR_EACH_INDEX_IN_MASK_END;

    return sum;
}
#endif

/*!
 * Performs check if SW/HW MAX FAN should be activated or not.
 *
 * @param[in/out]   pCooler     FAN_COOLER object pointer
 *
 * @return  LW_TRUE if MAX Fan failsafe should be activated, LW_FALSE otherwise
 */
static LwBool
s_fanCoolerMaxFanCheck
(
    FAN_COOLER *pCooler
)
{
    FAN_COOLER_ACTIVE_PWM  *pCool         = (FAN_COOLER_ACTIVE_PWM *)pCooler;
    LwBool                  bMaxFanForce  = LW_FALSE;
    LwBool                  bMaxFanActive;
#if !PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_COOLER_HW_MAX_FAN)
    LwU32                   sum;
#endif

    // Check if MAX Fan feature is enabled.
    if (!FAN_COOLER_MAX_FAN_IS_ENABLED(pCool))
    {
        return LW_FALSE;
    }

#if PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_COOLER_HW_MAX_FAN)
    //
    // Read the MAXFAN_STATUS bit of LW_THERM_MAXFAN register,
    // which indicates if HW MAXFAN is active.
    //
    bMaxFanActive = thermHwMaxFanStatusGet_HAL(&Therm, pCool->pwmSource);
    bMaxFanForce  = pmgrHwMaxFanTempAlertForceGet_HAL(&Pmgr, pCool->pwmSource);
#else
    // We MAX Fan speed if any selected event is asserted at the moment ...
    bMaxFanActive = (thermEventAssertedGet(&Therm) & pCool->maxFanEvtMask) != 0;

    // ... or was asserted since the last time we've checked it's state.
    sum = _fanCoolerViolationCountersSum(pCool->maxFanEvtMask);
    if (pCool->maxFalwiolationCountersSum != sum)
    {
        pCool->maxFalwiolationCountersSum = sum;
        bMaxFanActive                     = LW_TRUE;
    }
#endif

    if (bMaxFanActive || pCool->bMaxFanActive || bMaxFanForce)
    {
        FLCN_TIMESTAMP  now;
        LwU32           now1024ns;

        //
        // Original VBIOS limit is 16-bit value in milliseconds (max 65.535sec).
        // To avoid using 64-bit math that require significant IMEM we downscale
        // timestamp by 10 bits (wraps around every 4398 sec), while RM provides
        // time limit in same units.
        //
        osPTimerTimeNsLwrrentGet(&now);
        now1024ns = TIMER_TO_1024_NS(now);

        if (bMaxFanActive)
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_COOLER_HW_MAX_FAN))
            {
                //
                // HW MAXFAN signal has been asserted when there are no ongoing
                // MAXFAN signals being handled lwrrently. We need to clear the
                // MAXFAN_STATUS, so that it can be re-asserted again by 
                // another event, at a later point. But to ensure that fan 
                // operates at MAX speed even after clearing MAXFAN_STATUS
                // Set the _FORCE field in TEMP_ALERT.
                //
                pmgrHwMaxFanTempAlertForceSet_HAL(&Pmgr,
                    pCool->pwmSource, LW_TRUE);
                thermHwMaxFanStatusSet_HAL(&Therm, pCool->pwmSource, LW_FALSE);
            }

            pCool->maxFanTimestamp1024ns = now1024ns;
            pCool->bMaxFanActive         = LW_TRUE;
        }
        else
        {
            pCool->bMaxFanActive = (pCool->maxFanMinTime1024ns >
                                    (now1024ns - pCool->maxFanTimestamp1024ns));

            if (PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_COOLER_HW_MAX_FAN) &&
                !pCool->bMaxFanActive)
            {
                //
                // HW MAXFAN has been active for a duration more than 
                // maxFanMinTime. Clear the FORCE bit  in TEMP_ALERT. This 
                // will deactivate HW MAXFAN.
                //
                pmgrHwMaxFanTempAlertForceSet_HAL(&Pmgr,
                    pCool->pwmSource, LW_FALSE);
            }
        }
    }

    return pCool->bMaxFanActive;
}
