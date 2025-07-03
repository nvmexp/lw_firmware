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
#include "partasmmacros.h"

.section ".text.partition_spdm_entry","ax",@progbits
.balign 4
FUNC partition_spdm_entry

    // Set trap handler
    la    t0, partition_spdm_trap_handler
    csrw  stvec, t0

    // Setup stack
    // MK TODO: we can't set VA for stack, because HMAC driver uses it for SHA operations
    // https://jirasw.lwpu.com/browse/COREUCODES-2066
    la sp, _partition_spdm_stack_top
    addi sp, sp, -0x10
    sd zero, 0(sp)
    sd zero, 8(sp)

    jal partition_spdm_main

_end:
    j _end
EFUNC partition_spdm_entry
