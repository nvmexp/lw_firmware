/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "lwriscv/csr.h"
#include "lwriscv/asm-helpers.h"

.section ".text.partition_1_startup","ax",@progbits
.balign 4
FUNC partition_1_startup

    // Set Partition 1 trap handler
    la    t0, partition_1_trap_handler
    csrw  stvec, t0

    // Setup stack for partition 1
    la sp, _partition_1_stack_top

    jal partition_1_main

_end:
    j _end
EFUNC partition_1_startup
