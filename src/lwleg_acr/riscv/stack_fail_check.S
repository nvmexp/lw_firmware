#include "lwriscv/asm-helpers.h"

.section ".text.sspCheckFailHook","ax",@progbits
.balign 8
FUNC sspCheckFailHook
    // Check if we run already
    la a0, ssp_was_fail_hook_run
    lb a1, 0(a0)
    beq a1, zero, just_panic
    sb zero, 0(a0)

    // Setup temporary stack
    la sp, _liblwriscv_stack_bottom
    addi sp, sp, -0x10

    jal sspFailHandler
just_panic:
    j riscvPanic
_end:
    j _end
EFUNC sspCheckFailHook

.section .data, "a", @progbits
ssp_was_fail_hook_run:
.byte 1




