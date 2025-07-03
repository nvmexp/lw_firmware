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
#include "asmmacro.h"

.balign 4
.section ".text.startup","ax",@progbits
.global startup
startup:
	contextZero

	la    t0, mtrap_handler_asm
	csrw  mtvec, t0
	la    t0, strap_handler_asm
	csrw  stvec, t0

	/* Test Bug  2667213 */
	/*
	li    x1, 0
	li    x2, 0
	li    x3, 10
        add   x2, x3, x0
	bltu  x2, x1, _Panic
	*/
 
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

.balign 4
FUNC mtrap_handler_asm
	addi sp, sp, -(CONTEXT_SIZE)
	contextSave
	//TODO does stack point to first free location or last used location

	jal mtrap_handler

	contextRestore
	addi sp, sp, CONTEXT_SIZE
	mret	
EFUNC mtrap_handler_asm

.balign 4
FUNC strap_handler_asm
	addi sp, sp, -(CONTEXT_SIZE)
	contextSave

	jal strap_handler

	contextRestore
	addi sp, sp, CONTEXT_SIZE
	sret	
EFUNC strap_handler_asm


