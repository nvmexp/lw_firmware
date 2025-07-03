/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#pragma once

#include <lwtypes.h>
#include "libos.h"

LibosStatus LibosLockCreate(LibosLockHandle *lock);
LibosStatus LibosLockAsyncAcquire(LibosLockHandle lock, LibosShuttleHandle shuttle);
LibosStatus LibosLockSyncAcquire(LibosLockHandle lock, LwU64 timeout);
LibosStatus LibosLockRelease(LibosLockHandle lock);

/**
 * @brief Enters a scheduler critical section
 *
 * Once in a scheduler critical section the kernel will not context switch
 * to another task on end of timeslice.  The kernel interrupt ISR is still
 * enabled but all user-space interrupt servicing is disabled.
 *
 * Ports:: Waits on shuttle completion are forbidden.  Asynchronous send/recv
 *         is allowed.
 *
 *
 * @param[in] behavior   Controls whether the critical section is released on
 *                       task crash.
 */

void LibosCriticalSectionEnter();

/**
 * @brief Leaves a scheduler critical section
 */
void LibosCriticalSectionLeave(void);