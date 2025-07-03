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
 * @file    main-mock.h
 * @brief   Mock declaratiosn for main
 */

#ifndef MAIN_MOCK_H
#define MAIN_MOCK_H

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
 * @brief   Initializes mocking for main
 */
void mainMockInit(void);

/* ------------------------- Mock Prototypes -------------------------------- */
/*!
 * @copydoc idlePreInitTask
 *
 * @note    Mock implementation of @ref idlePreInitTask
 */
DECLARE_FAKE_VALUE_FUNC(FLCN_STATUS, idlePreInitTask_MOCK);

#endif // MAIN_MOCK_H
