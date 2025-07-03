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
// manual MPU setup, with 5 regions.
        .int    1, 5

// .text
        .dword  __PHDR0_START + 1
        .dword  0xFF00000000000000 + __PHDR0_OFFSET
        .dword  __PHDR0_SIZE
        .dword  0xA00

// .rodata
        .dword  __PHDR1_START + 1
        .dword  0xFF00000000000000 + __PHDR1_OFFSET
        .dword  __PHDR1_SIZE
        .dword  0x200

// .data+.bss+.stack
        .dword  __PHDR2_START + 1
        .dword  0xFF00000000000000 + __PHDR2_OFFSET
        .dword  __PHDR2_SIZE
        .dword  0x600

// FBGPA mapping
        .dword  0x200000001
        .dword  0x6180000000000000
        .dword  0x100000000
        .dword  0xC0E00

// PRIV mapping
        .dword  0x300000001
        .dword  0x2000000000000000
        .dword  0x100000000
        .dword  0xE00
