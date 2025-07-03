/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "lwriscv/csr.h"
#include "lwriscv/asm-helpers.h"

.section ".text.rtos_startup","ax",@progbits
.balign 4
FUNC rtos_startup

    // Set Partition RTOS trap handler
    la    t0, rtos_trap_handler
    csrw  stvec, t0

    // Setup stack for partition 0
    la sp, _rtos_stack_top

    jal rtos_init

_end:
    j _end
EFUNC rtos_startup
