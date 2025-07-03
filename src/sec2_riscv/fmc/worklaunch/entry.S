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

// level 3

.section ".text.partition_wl_entry","ax",@progbits
.balign 4
FUNC partition_wl_entry

    la    t0, partition_wl_trap_handler
    csrw  stvec, t0

    la sp, _partition_wl_stack_top

    jal partition_wl_main

_end:
    j _end
EFUNC partition_wl_entry
