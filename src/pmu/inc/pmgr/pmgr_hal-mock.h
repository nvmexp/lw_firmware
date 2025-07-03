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
 * @file    pmgr_hal-mock.h
 * @brief   Mock declarations for PMGR HAL functions.
 */

#ifndef PMGR_HAL_MOCK_H
#define PMGR_HAL_MOCK_H

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
 * @brief   Initializes mocking for pmgrHal
 */
void pmgrHalMockInit(void);

/* ------------------------- Mock Prototypes -------------------------------- */
/*
 * @copydoc pmgrPwmSourceDescriptorConstruct_HAL
 *
 * @note    Mock implementation of @ref pmgrPwmSourceDescriptorConstruct_HAL
 */
PWM_SOURCE_DESCRIPTOR *pmgrPwmSourceDescriptorConstruct_MOCK(RM_PMU_PMGR_PWM_SOURCE pwmSource, LwU8 ovlIdxDmem);
DECLARE_FAKE_VALUE_FUNC(PWM_SOURCE_DESCRIPTOR *, pmgrPwmSourceDescriptorConstruct_MOCK,
    RM_PMU_PMGR_PWM_SOURCE, LwU8);
#undef pmgrPwmSourceDescriptorConstruct_HAL
#define pmgrPwmSourceDescriptorConstruct_HAL(pPmgr, pwmSource, ovlIdxDmem)  \
        pmgrPwmSourceDescriptorConstruct_MOCK((pwmSource), (ovlIdxDmem))

#endif // UTF_FUNCTION_MOCKING
#endif // PMGR_HAL_MOCK_H
