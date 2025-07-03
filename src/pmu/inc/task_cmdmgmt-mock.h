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
 * @file    task_cmdmgmt-mock.h
 * @brief   Mock declaratiosn for cmdmgmtTask
 */

#ifndef TASK_CMDMGMT_MOCK_H
#define TASK_CMDMGMT_MOCK_H

/* ------------------------- System Includes -------------------------------- */
#include "ut_port_types.h"
#include "fff.h"
#include "flcnifcmn.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Defines ---------------------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Function Prototypes ---------------------------- */
/*!
 * @brief   Initializes mocking for cmdmgmtTask
 */
void cmdmgmtTaskMockInit(void);

/* ------------------------- Mock Prototypes -------------------------------- */
/*!
 * @copydoc cmdmgmtPreInitTask
 *
 * @note    Mock implementation of @ref cmdmgmtPreInitTask
 */
DECLARE_FAKE_VALUE_FUNC(FLCN_STATUS, cmdmgmtPreInitTask_MOCK);

#endif // TASK_CMDMGMT_MOCK_H
