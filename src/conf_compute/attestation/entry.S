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

.section ".text.partition_attestation_entry","ax",@progbits
.balign 4
FUNC partition_attestation_entry

    // Set trap handler
    la    t0, partition_attestation_trap_handler
    csrw  stvec, t0

    // Setup stack
    partitionInitSpAndMpu _partition_attestation_stack_bottom, _partition_attestation_stack_top

    jal partition_attestation_main

_end:
    j _end
EFUNC partition_attestation_entry
