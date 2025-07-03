/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   start.S
 * @brief  Entrypoint
 */

#include <lwriscv-csr.h>

        .section        .text
        .globl  _start
_start:
        la      sp, __STACK_END
        jal     main

        /* fall-through */
failed:
        csrw    CSR_LW_MTOHOST, a0
1:      j       1b

        .section        .LWPU.mpu_info
mpu_info:
// automatic MPU setup, with 0 extra regions.
        .int    2, 0
