/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

// Dispatcher to jump to partitions
// This is done like that (for now) so we don't need to hardcode partition entries
// in policy (and update when partitions grow/shrink)

#include "lwriscv/csr.h"
#include "lwriscv/asm-helpers.h"

.section ".text.partition_dispatcher","ax",@progbits
.balign 16
// 16 bytes per partition
FUNC partition_dispatcher
// Partition 0: Secure worklaunch
    j partition_wl_entry
    .balign 16
// Partition 1: RTOS
    j partition_rtos_entry
    .balign 16
_end:
    j _end
EFUNC partition_dispatcher
