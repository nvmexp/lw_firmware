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

.section ".text.selwreaction_startup","ax",@progbits
.balign 4
FUNC selwreaction_startup

    // Set Partition SelwreAction trap handler
    la    t0, selwreaction_trap_handler
    csrw  stvec, t0

    // Setup stack for partition SelwreAction
    la sp, _selwreaction_stack_top

    jal selwreaction_main

_end:
    j _end
EFUNC selwreaction_startup
