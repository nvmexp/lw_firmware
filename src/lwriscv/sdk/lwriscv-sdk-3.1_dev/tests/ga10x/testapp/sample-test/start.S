/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#include <dev_riscv_csr_64.h>

.section ".text.startup","ax",@progbits
.balign 4
.global startup
startup:
	j    do_reset

do_reset:
	li    x1, 0
	li    x2, 0
	li    x3, 0
	li    x4, 0
	li    x5, 0
	li    x6, 0
	li    x7, 0
	li    x8, 0
	li    x9, 0
	li    x10, 0
	li    x11, 0
	li    x12, 0
	li    x13, 0
	li    x14, 0
	li    x15, 0
	li    x16, 0
	li    x17, 0
	li    x18, 0
	li    x19, 0
	li    x20, 0
	li    x21, 0
	li    x22, 0
	li    x23, 0
	li    x24, 0
	li    x25, 0
	li    x26, 0
	li    x27, 0
	li    x28, 0
	li    x29, 0
	li    x30, 0
	li    x31, 0

	// write mtvec and make sure it sticks
	la    t0, _panic
	csrw  mtvec, t0

	la    sp, _monitor_stack_top - 0x10
	sd    zero, 0(sp)
	sd    zero, 8(sp)
	jal   main
	// halt the core
	csrwi LW_RISCV_CSR_MOPT, LW_RISCV_CSR_MOPT_CMD_HALT
_end:
	j _end

# Panic
.balign 4
_panic:
	j _panic
