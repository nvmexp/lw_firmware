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
 * @file    pg_hal-mock.h
 * @brief   Mock declarations for PG HAL functions.
 */

#ifndef PG_HAL_MOCK_H
#define PG_HAL_MOCK_H

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
 * @brief   Initializes mocking for pgHal
 */
void pgHalMockInit(void);

/* ------------------------- Mock Prototypes -------------------------------- */
/*
 * @copydoc pgIslandPwmSourceDescriptorConstruct_HAL
 *
 * @note    Mock implementation of @ref pgIslandPwmSourceDescriptorConstruct_HAL
 */
PWM_SOURCE_DESCRIPTOR *pgIslandPwmSourceDescriptorConstruct_MOCK(RM_PMU_PMGR_PWM_SOURCE pwmSource, LwU8 ovlIdxDmem);
DECLARE_FAKE_VALUE_FUNC(PWM_SOURCE_DESCRIPTOR *, pgIslandPwmSourceDescriptorConstruct_MOCK,
    RM_PMU_PMGR_PWM_SOURCE, LwU8);
#undef pgIslandPwmSourceDescriptorConstruct_HAL
#define pgIslandPwmSourceDescriptorConstruct_HAL(pPmgr, pwmSource, ovlIdxDmem)  \
        pgIslandPwmSourceDescriptorConstruct_MOCK((pwmSource), (ovlIdxDmem))

#endif // UTF_FUNCTION_MOCKING
#endif // PG_HAL_MOCK_H
