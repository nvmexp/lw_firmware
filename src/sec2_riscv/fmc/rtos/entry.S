/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "lwriscv/csr.h"
#include "lwriscv/asm-helpers.h"

.section ".text.partition_rtos_entry","ax",@progbits
.balign 4
FUNC partition_rtos_entry
    la    t0, partition_rtos_trap_handler
    csrw  stvec, t0

    la sp, _partition_rtos_stack_top

    jal partition_rtos_init

_end:
    j _end
EFUNC partition_rtos_entry
