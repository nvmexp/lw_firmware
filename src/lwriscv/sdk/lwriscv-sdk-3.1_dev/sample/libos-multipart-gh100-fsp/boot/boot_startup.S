/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <lwriscv/csr.h>
#include <lwriscv/asm-helpers.h>

.section ".text.partition_boot_startup","ax",@progbits
.balign 4
FUNC partition_boot_startup

    la    t0, partition_boot_trap_handler
    csrw  stvec, t0

    la sp, _partition_boot_stack_top

    jal partition_boot_main

_end:
    j _end
EFUNC partition_boot_startup
