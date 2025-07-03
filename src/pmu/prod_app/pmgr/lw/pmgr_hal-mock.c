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
 * @file    pmgr_hal-mock.c
 * @brief   Mock implementations for PMGR HAL functions
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "lwtypes.h"
#include "pmusw.h"
#include "pmu/pmuifpmgrpwm.h"
#include "pmu_objpmgr.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
void
pmgrHalMockInit(void)
{
    RESET_FAKE(pmgrPwmSourceDescriptorConstruct_MOCK);
}

/* ------------------------- Mocked Functions ------------------------------- */
DEFINE_FAKE_VALUE_FUNC(PWM_SOURCE_DESCRIPTOR *, pmgrPwmSourceDescriptorConstruct_MOCK,
    RM_PMU_PMGR_PWM_SOURCE, LwU8);

/* ------------------------- Static Functions ------------------------------- */
