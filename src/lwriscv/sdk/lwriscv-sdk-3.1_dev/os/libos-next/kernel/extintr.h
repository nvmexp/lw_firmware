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

#include <lwtypes.h>

void kernel_external_interrupt_init(void);
void KernelExternalInterruptLoad(void);
void KernelExternalInterrupt(void);
void KernelExternalInterruptReport(void);
void KernelSyscallExternalInterruptArm(LwU32 interruptWaitingMask);

#endif
