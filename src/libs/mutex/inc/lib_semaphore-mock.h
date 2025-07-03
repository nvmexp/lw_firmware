/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LIN_SEMAPHORE_MOCK_H
#define LIN_SEMAPHORE_MOCK_H

/*!
 * @file    osptimer-mock.h
 * @brief   Declarations for mocked versions of functions in OSPTimer
 */

/* ------------------------- System Includes -------------------------------- */
#include "lib_semaphore.h"
#include "fff.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Defines ---------------------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Function Prototypes ---------------------------- */
DECLARE_FAKE_VALUE_FUNC(PSEMAPHORE_RW, osSemaphoreCreateRW_MOCK, LwU8);

#endif // LIN_SEMAPHORE_MOCK_H
