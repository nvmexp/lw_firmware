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
 * @file    therm_hal-mock.c
 * @brief   Mock implementations for therm HAL functions
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "pmusw.h"
#include "therm/objtherm.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
void
thermHalMockInit(void)
{
    RESET_FAKE(thermPwmSourceDescriptorConstruct_MOCK);
    RESET_FAKE(thermService_MOCK);
}

/* ------------------------- Mocked Functions ------------------------------- */
DEFINE_FAKE_VALUE_FUNC(PWM_SOURCE_DESCRIPTOR *, thermPwmSourceDescriptorConstruct_MOCK,
    RM_PMU_PMGR_PWM_SOURCE, LwU8);

DEFINE_FAKE_VOID_FUNC(thermService_MOCK);

/* ------------------------- Static Functions ------------------------------- */
