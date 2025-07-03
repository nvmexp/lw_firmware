/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2009-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_OBJTIMER_H
#define PMU_OBJTIMER_H

/*!
 * @file   pmu_objtimer.h
 * @brief  PMU Timer Library HAL header file. HAL timer functions can be called
 *         via the 'TimerHal' global variable.
 */

/* ------------------------- System includes -------------------------------- */
#include "flcntypes.h"
#include "osptimer.h"

/* ------------------------- Application includes --------------------------- */
#include "pmu_timer.h"

#include "config/g_timer_hal.h"

/* ------------------------- Defines ---------------------------------------- */
/*!
 * Tracking 64-bit time is consuming resources while keeping [ns] time in 32-bit
 * field overflows ~ 4.2s. Colwerting time to [us] simplifies storage and solves
 * overflow problem, but division is expensive. This macro can be used when time
 * precision is not essential (i.e. when time is in ~[ms]) since colwersion adds
 * some 2.5% error (result is multiple of 1024[ns]).
 */
#define TIMER_TO_1024_NS(_time64)                                              \
    (((_time64).parts.hi << 22) | ((_time64).parts.lo >> 10))

/* ------------------------- Types definitions ------------------------------ */
/* ------------------------- External definitions --------------------------- */
/*!
 * TIMER object Definition
 */
typedef struct
{
    LwU8    dummy;    // unused -- for compilation purposes only
} OBJTIMER;

extern OBJTIMER Timer;

/* ------------------------- Static variables ------------------------------- */
/* ------------------------- Function Prototypes ---------------------------- */

#endif // PMU_OBJTIMER_H
