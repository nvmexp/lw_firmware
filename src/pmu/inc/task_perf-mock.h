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
 * @file    task_perf-mock.h
 * @brief   Mock declaratiosn for perfTask
 */

#ifndef TASK_PERF_MOCK_H
#define TASK_PERF_MOCK_H

/* ------------------------- System Includes -------------------------------- */
#include "ut_port_types.h"
#include "fff.h"
#include "flcnifcmn.h"
#include "task_perf.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Defines ---------------------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Function Prototypes ---------------------------- */
/*!
 * @brief   Initializes mocking for perfTask
 */
void perfTaskMockInit(void);

/* ------------------------- Mock Prototypes -------------------------------- */
/*!
 * @copydoc perfPreInitTask
 *
 * @note    Mock implementation of @ref perfPreInitTask
 */
DECLARE_FAKE_VALUE_FUNC(FLCN_STATUS, perfPreInitTask_MOCK);

#endif // TASK_PERF_MOCK_H
