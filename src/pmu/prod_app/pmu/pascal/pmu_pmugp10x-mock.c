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
 * @file   pmu_pmugp10x-mock.c
 * @brief  Mock implementations for pmugp10x
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "pmu/pmu_pmugp10x-mock.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
void
pmugp10xMockInit(void)
{
    RESET_FAKE(pmuDmemVaInit_GP10X);
}

/* ------------------------- Mocked Functions ------------------------------- */
// TODO: suffix this with "_MOCK" when HAL mocking sovled
DEFINE_FAKE_VALUE_FUNC(FLCN_STATUS, pmuDmemVaInit_GP10X, LwU32);

/* ------------------------- Static Functions ------------------------------- */
