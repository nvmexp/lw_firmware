/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LIBOS_TRAP_H
#define LIBOS_TRAP_H

void                KernelTrapLoad(void);
LIBOS_NORETURN void TrapReturn(Task *return_to);

#endif
