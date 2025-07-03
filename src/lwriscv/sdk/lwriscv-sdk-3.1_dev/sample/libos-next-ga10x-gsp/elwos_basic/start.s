.section .text.startup
.global start

/*
    *   Disable linker relaxation to ensure code is compiled with PC
    *   relative relocations.
    */
.option push
.option norelax

start:
    /* Setup the stack */
.stack.relocation:
    la    sp, .stack_top

break:
    /* The continuation variable points to the current entry point to enable SeparationKernelPartitionCall */
    la    t0, StartInContinuation
    lb    t0, 0(t0)
    bne   t0, x0, SeparationKernelPartitionContinuation
    j     InitPartitionEntry

.section .data
.skip 4096
.stack_top:







