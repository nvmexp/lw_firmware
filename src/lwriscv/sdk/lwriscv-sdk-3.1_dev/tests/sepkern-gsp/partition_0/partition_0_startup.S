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

.section ".text.partition_0_startup","ax",@progbits
.balign 4
FUNC partition_0_startup

    // Set Partition 0 trap handler
    la    t0, partition_0_trap_handler
    csrw  stvec, t0

    // Setup stack for partition 0
    la sp, _partition_0_stack_top

    jal partition_0_init

_end:
    j _end
EFUNC partition_0_startup
