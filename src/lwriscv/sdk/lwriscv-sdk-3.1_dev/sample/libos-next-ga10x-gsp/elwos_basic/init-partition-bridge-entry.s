.section .text.init_partition_bridge
.global bridge_entry
.global bridge_trap_handler

bridge_entry:
    /* Setup trap handler for init partition */
    la    t0, bridge_trap_handler
    csrw  stvec, t0

    /* Setup the stack */
    la    sp, .stack_end
    jal   InitPartitionBridge

_unexpected_return:
    j _unexpected_return

bridge_trap_handler:
    j bridge_trap_handler 

.section .data
.skip 512
.stack_end:







