/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "kernel.h"
#include "lwriscv-2.0/sbi.h"
#include "diagnostics.h"

LIBOS_NORETURN LIBOS_SECTION_TEXT_COLD void KernelPanic()
{
    KernelPrintf("Kernel panic.\n");
#if LIBOS_LWRISCV >= LIBOS_LWRISCV_2_0
    KernelSbiShutdown();
#else
    while(1);
#endif
}
