/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef OSPTIMER_MOCK_H
#define OSPTIMER_MOCK_H

/*!
 * @file    osptimer-mock.h
 * @brief   Declarations for mocked versions of functions in OSPTimer
 */

/* ------------------------- System Includes -------------------------------- */
#include "osptimer.h"
#include "fff.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Defines ---------------------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Function Prototypes ---------------------------- */
/*!
 * @brief   Initializes OSPtimer mocked state.
 */
void osPTimerMockInit(void);

/*!
 * @copydoc    osPTimerTimeNsLwrrentGet
 *
 * @brief   Mock implementation of osPTimerTimeNsLwrrentGet
 */
DECLARE_FAKE_VOID_FUNC(osPTimerTimeNsLwrrentGet_MOCK,
    PFLCN_TIMESTAMP);

/*!
 * @copydoc    osPTimerCondSpinWaitNs
 *
 * @brief   Mock implementation of osPTimerCondSpinWaitNs
 */
DECLARE_FAKE_VALUE_FUNC(LwBool, osPTimerCondSpinWaitNs_MOCK,
    OS_PTIMER_COND_FUNC, void *, LwU32);

#endif // OSPTIMER_MOCK_H
