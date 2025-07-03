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
 * @file        fetch.S
 * @brief       Instruction-fetch helper.
 */

// Exclude header portions only meant for C code.
#define AS_ILWOKED


// LWRISCV includes.
#include <lwriscv/asm-helpers.h>

// Local includes.
#include "assembly.h"
#include "fetch.h"


/*!
 * @brief Attempts to jump to the specified target address.
 *
 * @param[in] addr  The target address to jump to.
 *
 * @return
 *    FETCH_RET_CODE.
 *
 * @note Will not return unless the code at the target location also returns.
 */
.section .text
.balign 4
FUNC fetchAddress // uint32_t fetchAddress(uintptr_t target);
    // Save context.
    save_ctx

    //
    // Try fetching from the target address. If successful, the code at the
    // target should return us here normally. If unsuccessful, the excption-
    // handler will do it instead.
    //
    jalr a0

    // Restore context.
    load_ctx

    // Set expected return value.
    lui a0, FETCH_RET_CODE_HI
    addi a0, a0, FETCH_RET_CODE_LO

    // Return to caller.
    ret
EFUNC fetchAddress
