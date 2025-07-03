/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
 
#include <dev_riscv_csr_64.h>

.section .text.trap_entry
.balign 4
.global trap_entry

trap_entry:
    // Switch to SK stack; mscratch always contains the monitor stack;
    csrw	LW_RISCV_CSR_MSCRATCH2, sp
    csrr	sp, mscratch

    // Jump to SPARK Trap_Handler unconditionally
    j trap_handler
