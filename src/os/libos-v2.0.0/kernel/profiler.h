/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LIBOS_PROFILER_H
#define LIBOS_PROFILER_H

void kernel_profiler_init(void);
void kernel_profiler_save_counters(void);

void LIBOS_SECTION_TEXT_COLD kernel_syscall_profiler_enable(void);
void LIBOS_SECTION_TEXT_COLD kernel_syscall_profiler_get_counter_mask(void);

#endif // LIBOS_PROFILER_H
