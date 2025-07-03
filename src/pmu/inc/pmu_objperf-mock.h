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
 * @file    pmu_objperf-mock.h
 * @brief   Data required for configuring mock pmu_objperf interfaces.
 */

#ifndef PMU_OBJPERF_MOCK_H
#define PMU_OBJPERF_MOCK_H

/* ------------------------- System Includes -------------------------------- */
#include "ut_port_types.h"
#include "fff.h"
#include "flcnifcmn.h"
#include "pmu_objperf.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Defines ---------------------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Function Prototypes ---------------------------- */
/*!
 * @brief   Initializes mocking for perf
 */
void perfMockInit(void);

/* ------------------------- Mock Prototypes -------------------------------- */
/*!
 * @copydoc constructPerf_MOCK
 *
 * @note    Mock implementation of @ref constructPerf_MOCK
 */
DECLARE_FAKE_VALUE_FUNC(FLCN_STATUS, constructPerf_MOCK);

DECLARE_FAKE_VALUE_FUNC(FLCN_STATUS, perfPostInit_MOCK);

#endif // PMU_OBJPERF_MOCK_H
