/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LIBOS_TIMER_H
#define LIBOS_TIMER_H

// coverity[include_relwrsion]
#include "task.h"

void                            KernelTimerInit(void);
void                            KernelTimerLoad(void);
void                            KernelTimerEnque(Task *target);
void                            KernelTimerCancel(Task *target);
LwU64                           KernelTimerProcessElapsed(LwU64 mtime);
LIBOS_SECTION_IMEM_PINNED void  KernelTimerArmInterrupt(LwU64 nextPreempt);
void LIBOS_SECTION_IMEM_PINNED  KernelTimerSet(Timer * timer, LwU64 timestamp);
#endif
