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
 * @file    osptimer-mock.c
 * @brief   Definitions for mocked versions of functions in OSPTimer
 */

/* ------------------------- System Includes -------------------------------- */
#include "osptimer.h"
#include "osptimer-mock.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/*!
 * @brief   Further lwstomized behavior for @ref osPTimerCondSpinWaitNs_MOCK
 */
static LwBool s_osPTimerCondSpinWaitNs_MOCK_Lwstom(
    OS_PTIMER_COND_FUNC fp, void *pArgs, LwU32 delayNs);

/* ------------------------- Public Functions ------------------------------- */
void
osPTimerMockInit(void)
{
    RESET_FAKE(osPTimerTimeNsLwrrentGet_MOCK);

    RESET_FAKE(osPTimerCondSpinWaitNs_MOCK);
    osPTimerCondSpinWaitNs_MOCK_fake.lwstom_fake =
        s_osPTimerCondSpinWaitNs_MOCK_Lwstom;
}

/* ------------------------- Mocked Functions ------------------------------- */
DEFINE_FAKE_VOID_FUNC(osPTimerTimeNsLwrrentGet_MOCK,
    PFLCN_TIMESTAMP);

DEFINE_FAKE_VALUE_FUNC(LwBool, osPTimerCondSpinWaitNs_MOCK,
    OS_PTIMER_COND_FUNC, void *, LwU32);

/* ------------------------- Static Functions ------------------------------- */
static LwBool
s_osPTimerCondSpinWaitNs_MOCK_Lwstom
(
    OS_PTIMER_COND_FUNC fp,
    void *pArgs,
    LwU32 delayNs
)
{
    return fp(pArgs);
}
