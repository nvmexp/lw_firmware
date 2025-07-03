/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "lwriscv/asm-helpers.h"
#include "dev_pmu_prgnlcl.h"

.section ".text.partition_rtos_entry","ax",@progbits
.balign 4
FUNC partition_rtos_entry
    la    t0, partition_rtos_trap_handler
    csrw  stvec, t0

    la sp, _partition_rtos_stack_top

    jal partition_rtos_init

_partition_rtos_entry_end:
    j _partition_rtos_entry_end
EFUNC partition_rtos_entry


.section ".text.partition_rtos_trap_handler","ax",@progbits
.balign 4
FUNC partition_rtos_trap_handler
    li      gp, LW_PRGNLCL_RISCV_ICD_CMD
    sw      zero, 0(gp) // DRF_DEF(_PRISCV, _RISCV_ICD_CMD, _OPC, _STOP)
    fence   io,io

_partition_rtos_trap_handler_end:
    j _partition_rtos_trap_handler_end
EFUNC partition_rtos_trap_handler
