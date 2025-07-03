/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

 /*!
 * @file        trap.S
 * @brief       Exception-handling.
 */

// Exclude header portions only meant for C code.
#define AS_ILWOKED


// LWRISCV includes.
#include <lwriscv/asm-helpers.h>

// Local includes.
#include "assembly.h"


 // Wrapper function for the C exception-handler.
.section ".text.exceptionEntry","ax",@progbits
.balign 4
FUNC exceptionWrapper
    // Save context.
    save_ctx

    // Call the C handler.
    csrr a0, mepc
    csrr a1, mcause
    csrr a2, mtval
    jal harnessExceptionHandler

    // Check return value for $mepc override request.
    bnez a0, override

    // Restore context.
    load_ctx

    // Return to caller.
    mret

  override:
    // Restore context.
    load_ctx

    // Override $mepc with $ra
    csrw mepc, ra

    // Return to caller.
    mret
EFUNC exceptionWrapper
