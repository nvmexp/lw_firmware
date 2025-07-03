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
 * @file    pmu_pmugp10x-mock.h
 * @brief   Mock declaratiosn for pmugp10x
 */

#ifndef PMU_PMUGP10X_MOCK_H
#define PMU_PMUGP10X_MOCK_H

/* ------------------------- System Includes -------------------------------- */
#include "ut_port_types.h"
#include "fff.h"
#include "flcnifcmn.h"
#include "pmu_objpmu.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Defines ---------------------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Function Prototypes ---------------------------- */
/*!
 * @brief   Initializes mocking for pmugp10x
 */
void pmugp10xMockInit(void);

/* ------------------------- Mock Prototypes -------------------------------- */
/*!
 * @copydoc pmuDmemVaInit_GP10X
 *
 * @note    Mock implementation of @ref pmuDmemVaInit_GP10X
 *
 * @todo    Suffix this with "_MOCK" when HAL mocking solved
 */
DECLARE_FAKE_VALUE_FUNC(FLCN_STATUS, pmuDmemVaInit_GP10X, LwU32);

#endif // PMU_PMUGP10X_MOCK_H
