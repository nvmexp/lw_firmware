/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LIBOS_EXTINTR_H
#define LIBOS_EXTINTR_H

void kernel_external_interrupt_init(void);
void kernel_external_interrupt_load(void);
void kernel_external_interrupt(void);
void kernel_external_interrupt_report(void);
void kernel_syscall_interrupt_arm_external(LwU32 interruptWaitingMask);

#endif
