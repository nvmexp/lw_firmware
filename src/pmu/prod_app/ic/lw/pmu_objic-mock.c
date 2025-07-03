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
 * @file   pmu_objic-mock.c
 * @brief  Mock implementations for ic
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "pmu_objic-mock.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
void
icMockInit(void)
{
    RESET_FAKE(constructIc_MOCK)
}

/* ------------------------- Mocked Functions ------------------------------- */
DEFINE_FAKE_VALUE_FUNC(FLCN_STATUS, constructIc_MOCK);

/* ------------------------- Static Functions ------------------------------- */
