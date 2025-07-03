/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    lib_pwm-mock.h
 * @brief   Data required for configuring mock LIB_PWM interfaces.
 */

#ifndef LIB_PWM_MOCK_H
#define LIB_PWM_MOCK_H

/* ------------------------- System Includes -------------------------------- */
#include "ut_port_types.h"
#include "fff.h"
#include "flcnifcmn.h"
#include "lib/lib_pwm.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Defines ---------------------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/*!
 * Configuration variables for @ref pwmParamsGet_MOCK.
 */
typedef struct PWM_PARAMS_GET_MOCK_CONFIG
{
    // Duty cycle in raw units.
    LwU32 pwmDutyCycle;

    // PWM period in raw units.
    LwU32 pwmPeriod;

    // PWM transaction HW status.
    LwBool bDone;

    /*!
     * Mock return status
     */
    FLCN_STATUS status;
} PWM_PARAMS_GET_MOCK_CONFIG;

/* ------------------------- External Definitions --------------------------- */
extern PWM_PARAMS_GET_MOCK_CONFIG pwmParamsGet_MOCK_CONFIG;

/* ------------------------- Function Prototypes ---------------------------- */
/*!
 * @brief   Initializes mocking for {MODULE}
 */
void libPwmMockInit(void);

/* ------------------------- Mock Prototypes -------------------------------- */
/*!
 * @copydoc pwmSourceDescriptorAllocate
 *
 * @note    Mock implementation of @ref pwmSourceDescriptorAllocate
 */
DECLARE_FAKE_VALUE_FUNC(PWM_SOURCE_DESCRIPTOR *, pwmSourceDescriptorAllocate_MOCK,
    LwU8, RM_PMU_PMGR_PWM_SOURCE, PWM_SOURCE_DESCRIPTOR *);

#endif // LIB_PWM_MOCK_H
