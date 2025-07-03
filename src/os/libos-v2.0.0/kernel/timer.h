/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LIBOS_TIMER_H
#define LIBOS_TIMER_H

#include "task.h"

extern LwU64 nextQuanta;
void kernel_timer_init(void);
void kernel_timer_load(void);
void kernel_timeout_queue(libos_task_t *target);
void kernel_timeout_cancel(libos_task_t *target);
void kernel_timer_interrupt(void);
void kernel_timer_reset_quanta(void);
LwU64 kernel_timer_process(LwU64 mtime);
LIBOS_NORETURN void kernel_syscall_timer_get_ns(void);
void kernel_timer_program(LwU64 next);

#endif
