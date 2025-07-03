.section .text
.global LwosPartitionStart

/*
    *   Disable linker relaxation to ensure code is compiled with PC
    *   relative relocations.
    */
.option push
.option norelax

LwosPartitionStart:
    /* Setup the stack */
.stack.relocation:
    auipc sp, %pcrel_hi(.stack_top)
    addi  sp, sp, %pcrel_lo(.stack.relocation)    
    j RootPartitionEntry

.section .data
.skip 4096
.stack_top:





