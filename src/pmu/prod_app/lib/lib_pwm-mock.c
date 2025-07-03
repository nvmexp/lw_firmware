/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    lib_pwm-mock.c
 * @brief   Mock implementations of LIB_PWM public interfaces.
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"
#include "lib/lib_pwm.h"
#include "lib/lib_pwm-mock.h"

/* ------------------------ Application Includes ---------------------------- */
/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
PWM_PARAMS_GET_MOCK_CONFIG pwmParamsGet_MOCK_CONFIG;

/* ------------------------ Public Functions -------------------------------- */
void
libPwmMockInit(void)
{
    RESET_FAKE(pwmSourceDescriptorAllocate_MOCK);
}

/*!
 * @brief   MOCK implementation of pwmParamsGet.
 *
 * @details Configuration for this mock interface is controlled via values in
 *          @ref pwmParamsGet_MOCK_CONFIG. See
 *          @ref pwmParamsGet_IMPL() for original interface.
 *
 * @param[in]   pPwmSrcDesc     Pointer to descriptor of the PWM source driving the PWM
 * @param[out]  pPwmDutyCycle   Buffer in which to return duty cycle in raw units
 * @param[out]  pPwmPeriod      Buffer in which to return period in raw units
 * @param[out]  pBDone          Buffer in which to return Done status, TRUE/FALSE
 * @param[in]   bIlwert         Denotes if PWM source should be ilwerted
 *
 * @return  FLCN_OK                 PWM parameters successfully obtained.
 * @return  FLCN_ERR_NOT_SUPPORTED  PWM source descriptor type is not supported.
 * @return  Other unexpected errors Unexpected errors propagated from other functions.
 */
FLCN_STATUS
pwmParamsGet_MOCK
(
    PWM_SOURCE_DESCRIPTOR  *pPwmSrcDesc,
    LwU32                  *pPwmDutyCycle,
    LwU32                  *pPwmPeriod,
    LwBool                 *pBDone,
    LwBool                  bIlwert
)
{
    *pPwmDutyCycle = pwmParamsGet_MOCK_CONFIG.pwmDutyCycle;
    *pPwmPeriod    = pwmParamsGet_MOCK_CONFIG.pwmPeriod;
    *pBDone        = pwmParamsGet_MOCK_CONFIG.bDone;
    return pwmParamsGet_MOCK_CONFIG.status;
}

DEFINE_FAKE_VALUE_FUNC(PWM_SOURCE_DESCRIPTOR *, pwmSourceDescriptorAllocate_MOCK,
    LwU8, RM_PMU_PMGR_PWM_SOURCE, PWM_SOURCE_DESCRIPTOR *);

/* ------------------------ Static Functions -------------------------------- */
