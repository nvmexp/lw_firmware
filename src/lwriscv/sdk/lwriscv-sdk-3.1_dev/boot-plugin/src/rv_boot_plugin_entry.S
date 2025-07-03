 /***************************************************************************\
|*                                                                           *|
|*      Copyright 2018-2019 LWPU Corporation.  All rights reserved.        *|
|*                                                                           *|
|*   THE SOFTWARE AND INFORMATION CONTAINED HEREIN IS PROPRIETARY AND        *|
|*   CONFIDENTIAL TO LWPU CORPORATION. THIS SOFTWARE IS FOR INTERNAL USE   *|
|*   ONLY AND ANY REPRODUCTION OR DISCLOSURE TO ANY PARTY OUTSIDE OF LWPU  *|
|*   IS STRICTLY PROHIBITED.                                                 *|
|*                                                                           *|
 \***************************************************************************/

#include "lw_ref_dev_prgnlcl.h"

#define CSR_MTVEC  0x305
#define CSR_MOPT   0x7d8
#define CSR_MBPCFG 0x7dd

#define RV_HALT  csrwi CSR_MOPT, 0

.section ".text.rv_brom_entry","ax",@progbits
.global  rv_brom_entry
rv_brom_entry:
/* set initial trap vector */
    la t0, halt_core_label
    csrw CSR_MTVEC, t0

/* skip the initial trap vector */
    j normal_path_lable

    /****************************************************
     * The initial value of mtvec CSR points at this label.
     * If an unexpected exception happens before BR sets up
     * the trap handler, BR halts the core with RV_HALT
    *****************************************************/
halt_core_label:
    RV_HALT

normal_path_lable:
    /****************************************************
     * 1. Lock the PMB
     * This is the very first step,
     * it will prevent the I/DMEM being accessed by PRIV
     * LW_PRGNLCL_FALCON_LOCKPMB_IMEM_LOCK
     * LW_PRGNLCL_FALCON_LOCKPMB_DMEM_LOCK
     * LW_PRGNLCL_FALCON_LOCKPMB_EXT2MEM_LOCK
    *****************************************************/
    li t0, LW_PRGNLCL_FALCON_LOCKPMB
    li t1, 0x13
    sw t1, 0(t0)

    /****************************************************
     * 2. Enforce the priv lock down again unless DBGMODE
    *****************************************************/
    li t0, LW_PRGNLCL_FALCON_HWCFG2
    li t2, 0x8        /* LW_PRGNLCL_FALCON_HWCFG2_DBGMODE is at pos 3 : 3 */

    lw t1, 0(t0)
    and t1, t1, t2
    beq t1, t2, setup_error_trap_handler_label

    li t0, LW_PRGNLCL_RISCV_BR_PRIV_LOCKDOWN
    li t1, 1          /* LW_PRGNLCL_RISCV_BR_PRIV_LOCKDOWN_LOCK is at pos 0 : 0*/
    sw t1, 0(t0)

    /****************************************************
     * 3. Set up trap handler
    *****************************************************/
setup_error_trap_handler_label:
    la t0, rv_boot_plugin_error_handling__trap_handler
    csrw CSR_MTVEC, t0

    /****************************************************
     * 4. Check for I/DMEM H/W scrubbing done, otherwise Halt (skip for sw brom)
    *****************************************************/
    // li t0, LW_PRGNLCL_FALCON_DMACTL
    // lw t1, 0(t0)
    // andi t1, t1, 0x6           /* DMEM_SCRUBBING and IMEM_SCRUBBING indicator are at pos 2 : 1 */
    // bnez t1, halt_core_label   /* H/W wiil guarantee all IMEM/DMEM SCRUBBING done, otherwise HALT */

    /****************************************************
     * 5. Scrub GPRs
    *****************************************************/
    li x1,  0
    li x2,  0
    li x3,  0
    li x4,  0
    li x5,  0
    li x6,  0
    li x7,  0
    li x8,  0
    li x9,  0
    li x10, 0
    li x11, 0
    li x12, 0
    li x13, 0
    li x14, 0
    li x15, 0
    li x16, 0
    li x17, 0
    li x18, 0
    li x19, 0
    li x20, 0
    li x21, 0
    li x22, 0
    li x23, 0
    li x24, 0
    li x25, 0
    li x26, 0
    li x27, 0
    li x28, 0
    li x29, 0
    li x30, 0
    li x31, 0

    /****************************************************
     * 6. Setup SP and GP
     * .stack section will be put at the top of DMEM, size = 4K
     * .sdata section will be put at the bottow of stack, size = 4K
     *  (This is the maximum size for .sdata)
     * __global_pointer$ should point to the start of .sdata
     * gp will point to the middle of .sdata for relaxation optimization.
     * --------------------------------------------------
     * You may curious about what norelax/relax mean and what is gp for.
     * Linker Relaxation is a mechanism for optimizing program at the link-time.
     * Lets take RISCV for example, there are 2 unconditional control transfer instructions
     * 1. AUIPC + JALR pair 2. JAL
     * Where the 1st instructions can address 32-bit singned offset range and
     * the 2nd instruction can only address 21-bit signed offset.
     * The Linker Relaxation will do optimization for them if the address range is small.
     * for example, remove AUIPC in the 1st form or replace 1st to 2nd if the jump offset
     * is in 21-bit signed offset range.
     * Here we should not do this optimization for gp register,
     * because we need to store the 32-bit address to gp for the .sdata section relaxation.
     *
     * RISCV compiler treats gp as constant, it wont change its value in the program running.
     * The linker assumes that if the __global_pointer$ symbol is defined,
     * then the gp register contains that value, which it can then use to relax accesses to
     * global symbols with that 12-bit signed range.
    *****************************************************/
    // .option push
    // .option norelax
    // la gp, __global_pointer$
    // .option pop

    la sp, __stack_end

    /****************************************************
     * 7. Enable branch predictor
    *****************************************************/
    csrwi CSR_MBPCFG, 0x7  // enable BTB, BHT and RAS

    /****************************************************
     * 8. Scrub stack region again to make sure there are no unexpected data
     * Note : BR should not have bss/sbss, data/sdata section.
    *****************************************************/
    la a0, __stack_start
    la a1, __stack_end
    bgeu a0, a1, halt_core_label
1:
    sd zero, 0(a0)
    sd zero, 8(a0)
    sd zero, 16(a0)
    sd zero, 24(a0)
    sd zero, 32(a0)
    sd zero, 40(a0)
    sd zero, 48(a0)
    sd zero, 56(a0)
    addi a0, a0, 64
    nop  // Adding nop to help the pipeline remove the bubble to gain better performance.
    nop
    bltu a0, a1, 1b
2:
    li a0, 0
    li a1, 0

    /****************************************************
     * 8. Jump to main function directly
    *****************************************************/
    j _ada_rv_boot_plugin

