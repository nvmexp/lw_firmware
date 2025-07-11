/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#include "lwriscv/asm-helpers.h"

.section ".text.exceptionEntry","ax",@progbits
.balign 4
FUNC exceptionEntry
    jal riscvPanic
EFUNC exceptionEntry
