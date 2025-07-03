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
 * @file    pmu_objpmu-mock.h
 * @brief   Mock declaratiosn for pmu
 */

#ifndef PMU_OBJPMU_MOCK_H
#define PMU_OBJPMU_MOCK_H

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
 * @brief   Initializes mocking for pmu
 */
void pmuMockInit(void);

/* ------------------------- Mock Prototypes -------------------------------- */
/*!
 * @copydoc constructPmu_MOCK
 *
 * @note    Mock implementation of @ref constructPmu_MOCK
 */
DECLARE_FAKE_VALUE_FUNC(FLCN_STATUS, constructPmu_MOCK);

/*!
 * @copydoc pmuPollReg32Ns
 *
 * @note    Mock implementation of @ref pmuPollReg32Ns
 */
DECLARE_FAKE_VALUE_FUNC(LwBool, pmuPollReg32Ns_MOCK, LwU32, LwU32, LwU32, LwU32, LwU32);

#endif // PMU_OBJPMU_MOCK_H
