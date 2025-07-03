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
 * @file    task_therm-mock.h
 * @brief   Mock declaratiosn for thermTask
 */

#ifndef TASK_THERM_MOCK_H
#define TASK_THERM_MOCK_H

/* ------------------------- System Includes -------------------------------- */
#include "ut_port_types.h"
#include "fff.h"
#include "flcnifcmn.h"
#include "task_therm.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Defines ---------------------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Function Prototypes ---------------------------- */
/*!
 * @brief   Initializes mocking for thermTask
 */
void thermTaskMockInit(void);

/* ------------------------- Mock Prototypes -------------------------------- */
/*!
 * @copydoc thermPreInitTask
 *
 * @note    Mock implementation of @ref thermPreInitTask
 */
DECLARE_FAKE_VALUE_FUNC(FLCN_STATUS, thermPreInitTask_MOCK);
/*!
 * @copydoc thermPreInit
 *
 * @note    Mock implementation of @ref thermPreInit
 */
DECLARE_FAKE_VOID_FUNC(thermPreInit_MOCK);

#endif // TASK_THERM_MOCK_H
