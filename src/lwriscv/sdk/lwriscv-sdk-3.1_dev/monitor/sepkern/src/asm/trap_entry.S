/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

.section .text.trap_entry
.balign 4
.global trap_entry

trap_entry:

    /**************************************************************************
     * *** Switch to SK stack
     * 'mscratch' will cointain S-mode, partition stack pointer so a quick
     * switch to partition stack can be made at end of Trap_Handler subprogram
    **************************************************************************/ 

    csrw mscratch, sp
    la	 sp, _sepkern_stack_top

    /**************************************************************************
     * *** Jump to SPARK Trap_Handler unconditionally
    **************************************************************************/ 

    j trap_handler
