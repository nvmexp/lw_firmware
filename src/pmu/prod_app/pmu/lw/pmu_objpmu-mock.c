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
 * @file   pmu_objpmu-mock.c
 * @brief  Mock implementations for pmu
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmu-mock.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
void
pmuMockInit(void)
{
    RESET_FAKE(constructPmu_MOCK);

    // Initialize to return success by default
    RESET_FAKE(pmuPollReg32Ns_MOCK);
    pmuPollReg32Ns_MOCK_fake.return_val = LW_TRUE;
}

/* ------------------------- Mocked Functions ------------------------------- */
DEFINE_FAKE_VALUE_FUNC(FLCN_STATUS, constructPmu_MOCK);

DEFINE_FAKE_VALUE_FUNC(LwBool, pmuPollReg32Ns_MOCK, LwU32, LwU32, LwU32, LwU32, LwU32);

/* ------------------------- Static Functions ------------------------------- */
