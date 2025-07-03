/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    therm_hal-mock.h
 * @brief   Mock declarations for therm HAL functions.
 */

#ifndef THERM_HAL_MOCK_H
#define THERM_HAL_MOCK_H

#ifdef UTF_FUNCTION_MOCKING

/* ------------------------- System Includes -------------------------------- */
#include "ut_port_types.h"
#include "fff.h"
#include "flcnifcmn.h"
#include "pmu/pmuifpmgrpwm.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Defines ---------------------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Function Prototypes ---------------------------- */
/*!
 * @brief   Initializes mocking for thermHal
 */
void thermHalMockInit(void);

/* ------------------------- Mock Prototypes -------------------------------- */
/*
 * @copydoc thermPwmSourceDescriptorConstruct_HAL
 *
 * @note    Mock implementation of @ref thermPwmSourceDescriptorConstruct_HAL
 */
PWM_SOURCE_DESCRIPTOR *thermPwmSourceDescriptorConstruct_MOCK(RM_PMU_PMGR_PWM_SOURCE pwmSource, LwU8 ovlIdxDmem);
DECLARE_FAKE_VALUE_FUNC(PWM_SOURCE_DESCRIPTOR *, thermPwmSourceDescriptorConstruct_MOCK,
    RM_PMU_PMGR_PWM_SOURCE, LwU8);
#undef thermPwmSourceDescriptorConstruct_HAL
#define thermPwmSourceDescriptorConstruct_HAL(pPmgr, pwmSource, ovlIdxDmem)  \
        thermPwmSourceDescriptorConstruct_MOCK((pwmSource), (ovlIdxDmem))

/*
 * @copydoc thermService_HAL
 *
 * @note    Mock implementation of @ref thermService_HAL
 */
void thermService_MOCK(void);
DECLARE_FAKE_VOID_FUNC(thermService_MOCK);
#undef thermService_HAL
#define thermService_HAL(pTherm)  \
        thermService_MOCK()

#endif // UTF_FUNCTION_MOCKING
#endif // THERM_HAL_MOCK_H
