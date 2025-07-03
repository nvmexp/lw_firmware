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
 * @file  fancooler_active_pwm_tach_corr.c
 * @brief FAN Fan Cooler Specific to ACTIVE_PWM_TACH_CORR
 */

/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "fan/fancooler.h"
#include "dbgprintf.h"
#include "lib/lib_fxp.h"

/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * Construct a FAN_COOLER_ACTIVE_PWM_TACH_CORR object.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
fanCoolerGrpIfaceModel10ObjSetImpl_ACTIVE_PWM_TACH_CORR
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_FAN_FAN_COOLER_ACTIVE_PWM_TACH_CORR_BOARDOBJ_SET *pCoolerAPTCSet =
        (RM_PMU_FAN_FAN_COOLER_ACTIVE_PWM_TACH_CORR_BOARDOBJ_SET *)pBoardObjDesc;
    FAN_COOLER_ACTIVE_PWM_TACH_CORR                         *pCoolerAPTC;
    FLCN_STATUS                                              status;

    // Construct and initialize parent-class object.
    status = fanCoolerGrpIfaceModel10ObjSetImpl_ACTIVE_PWM(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        return status;
    }

    pCoolerAPTC = (FAN_COOLER_ACTIVE_PWM_TACH_CORR *)*ppBoardObj;

    // Set member variables.
    pCoolerAPTC->propGain      = pCoolerAPTCSet->propGain;
    pCoolerAPTC->bRpmSimActive = pCoolerAPTCSet->bRpmSimActive;
    pCoolerAPTC->rpmSim        = pCoolerAPTCSet->rpmSim;
    if (PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_COOLER_ACTIVE_PWM_TACH_CORR_ACT_CORR))
    {
        pCoolerAPTC->pwmFloorLimitOffset = pCoolerAPTCSet->pwmFloorLimitOffset;
    }

    return status;
}

/*!
 * Class implementation.
 *
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
fanCoolerActivePwmTachCorrIfaceModel10GetStatus
(
    BOARDOBJ_IFACE_MODEL_10               *pModel10,
    RM_PMU_BOARDOBJ        *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_FAN_FAN_COOLER_ACTIVE_PWM_TACH_CORR_BOARDOBJ_GET_STATUS 
                                       *pCoolerStatus;
    FAN_COOLER_ACTIVE_PWM_TACH_CORR    *pCool;
    FLCN_STATUS                         status;

    // Call super-class implementation.
    status = fanCoolerActivePwmIfaceModel10GetStatus(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        return status;
    }
    pCool         = (FAN_COOLER_ACTIVE_PWM_TACH_CORR *)pBoardObj;
    pCoolerStatus =
        (RM_PMU_FAN_FAN_COOLER_ACTIVE_PWM_TACH_CORR_BOARDOBJ_GET_STATUS *)pPayload;

    // Set current and target RPM.
    pCoolerStatus->rpmLast    = pCool->rpmLast;
    pCoolerStatus->rpmTarget  = pCool->rpmTarget;
    pCoolerStatus->pwmActual  = pCool->pwmActual;

    return FLCN_OK;
}

/* ------------------------- Private Functions ------------------------------ */

/*!
 * @copydoc FanCoolerLoad
 */
FLCN_STATUS
fanCoolerActivePwmTachCorrLoad
(
    FAN_COOLER *pCooler
)
{
    FAN_COOLER_ACTIVE_PWM_TACH_CORR  *pCool =
        (FAN_COOLER_ACTIVE_PWM_TACH_CORR *)pCooler;
    FLCN_STATUS status;

    // Call super-class implementation.
    status = fanCoolerActivePwmLoad(pCooler);
    if (status != FLCN_OK)
    {
        return status;
    }

    // Set current and target RPM.
    pCool->rpmLast   = 0;
    pCool->rpmTarget = 0;

    // Set actual measured PWM.
    pCool->pwmActual = LW_TYPES_FXP_ZERO;

    return FLCN_OK;
}

/*!
 * @copydoc FanCoolerRpmSet
 */
FLCN_STATUS
fanCoolerActivePwmTachCorrRpmSet
(
    FAN_COOLER *pCooler,
    LwS32       rpm
)
{
    FAN_COOLER_ACTIVE_PWM_TACH_CORR *pCool =
        (FAN_COOLER_ACTIVE_PWM_TACH_CORR *)pCooler;
    FLCN_STATUS status;
    LwS32       rpmLwrrent;
    LwSFXP16_16 correction;
    LwSFXP16_16 pwmNew;

    // Supporting fanSim feature.
    if (pCool->bRpmSimActive)
    {
        rpm = (LwS32)pCool->rpmSim;
    }

    // Retrieve current RPM.
    status = fanCoolerRpmGet(pCooler, &rpmLwrrent);
    if (status != FLCN_OK)
    {
        goto fanCoolerActivePwmTachCorrRpmSet_exit;
    }

    // Bound the requested RPM settings.
    rpm = LW_MAX(rpm, (LwS32)pCool->super.super.rpmMin);
    rpm = LW_MIN(rpm, (LwS32)pCool->super.super.rpmMax);

    // Cache current and target RPM that will be used for tach correction.
    pCool->rpmLast   = (LwU32)rpmLwrrent;
    pCool->rpmTarget = (LwU32)rpm;

    // Compute new FAN speed and apply it.

    correction = LW_TYPES_S32_TO_SFXP_X_Y_SCALED(16, 16, (rpm - rpmLwrrent),
                    (LwS32)pCool->super.super.rpmMax);
    correction = multSFXP16_16(correction, pCool->propGain);

    pwmNew = pCool->super.pwmRequested +
             multSFXP16_16(pCool->super.pwmMax, correction);

    //
    // Bug 1398842: On Fan sharing designs (Gemini) using TACH driven policies
    // cooler GPU will try to slow down Fan speed driving it's PWM rate down to
    // pwmMin, while Fan speed will follow warmer GPU. Once GPUs switch their
    // roles and cooler one becames warmer, it might take a thermally
    // significant lag before it can impact Fan speed. Solution is to prevent
    // PWM requested by any GPU to fall much below combined/measured PWM rate.
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_COOLER_ACTIVE_PWM_TACH_CORR_ACT_CORR) &&
        (pCool->pwmFloorLimitOffset != LW_TYPES_FXP_ZERO))
    {
        LwSFXP16_16 pwmLim;

        // Obtain actual measured PWM.
        status = fanCoolerPwmGet(pCooler, LW_TRUE, &pwmLim);
        if (status != FLCN_OK)
        {
            // RM should filter out cases when this is not supported.
            PMU_HALT();
        }

        // Cache actual measured PWM.
        pCool->pwmActual = pwmLim;

        // Apply floor limit offset.
        pwmLim -= pCool->pwmFloorLimitOffset;
        pwmNew  = LW_MAX(pwmNew, pwmLim);
    }

    status = fanCoolerActivePwmPwmSet(pCooler, LW_TRUE, pwmNew);

fanCoolerActivePwmTachCorrRpmSet_exit:
    return status;
}

/*!
 * @copydoc FanCoolerPwmSet
 */
FLCN_STATUS
fanCoolerActivePwmTachCorrPwmSet
(
    FAN_COOLER *pCooler,
    LwBool      bBound,
    LwSFXP16_16 pwm
)
{
    FAN_COOLER_ACTIVE_PWM_TACH_CORR *pCool =
        (FAN_COOLER_ACTIVE_PWM_TACH_CORR *)pCooler;

    //
    // This code cover special case in which RPM sim is requested on the cooler
    // object while policy is using only PWM support. In such case fake call to
    // rpmSim() is issued allowing RPM sim to be applied  otherwise pwmSim() is
    // exelwted.
    //
    if (pCool->bRpmSimActive)
    {
        return fanCoolerRpmSet(pCooler, 0);
    }
    else
    {
        return fanCoolerActivePwmPwmSet(pCooler, bBound, pwm);
    }
}
