/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PLM_WPR_ASSEMBLY_H
#define PLM_WPR_ASSEMBLY_H

/*!
 * @file        assembly.h
 * @brief       Assembly utilities.
 */

// Macro for saving task context.
.macro save_ctx
        // Reserve stack space.
        addi    sp, sp, -8 * 31

        // Save all GPRs.
        sd  x1,  0 * 8(sp)
        sd  x2,  1 * 8(sp)
        sd  x3,  2 * 8(sp)
        sd  x4,  3 * 8(sp)
        sd  x5,  4 * 8(sp)
        sd  x6,  5 * 8(sp)
        sd  x7,  6 * 8(sp)
        sd  x8,  7 * 8(sp)
        sd  x9,  8 * 8(sp)
        sd x10,  9 * 8(sp)
        sd x11, 10 * 8(sp)
        sd x12, 11 * 8(sp)
        sd x13, 12 * 8(sp)
        sd x14, 13 * 8(sp)
        sd x15, 14 * 8(sp)
        sd x16, 15 * 8(sp)
        sd x17, 16 * 8(sp)
        sd x18, 17 * 8(sp)
        sd x19, 18 * 8(sp)
        sd x20, 19 * 8(sp)
        sd x21, 20 * 8(sp)
        sd x22, 21 * 8(sp)
        sd x23, 22 * 8(sp)
        sd x24, 23 * 8(sp)
        sd x25, 24 * 8(sp)
        sd x26, 25 * 8(sp)
        sd x27, 26 * 8(sp)
        sd x28, 27 * 8(sp)
        sd x29, 28 * 8(sp)
        sd x30, 29 * 8(sp)
        sd x31, 30 * 8(sp)
.endm

// Macro for restoring task context.
.macro load_ctx
        // Restore saved GPR contents.
        ld  x1,  0 * 8(sp)
        ld  x2,  1 * 8(sp)
        ld  x3,  2 * 8(sp)
        ld  x4,  3 * 8(sp)
        ld  x5,  4 * 8(sp)
        ld  x6,  5 * 8(sp)
        ld  x7,  6 * 8(sp)
        ld  x8,  7 * 8(sp)
        ld  x9,  8 * 8(sp)
        ld x10,  9 * 8(sp)
        ld x11, 10 * 8(sp)
        ld x12, 11 * 8(sp)
        ld x13, 12 * 8(sp)
        ld x14, 13 * 8(sp)
        ld x15, 14 * 8(sp)
        ld x16, 15 * 8(sp)
        ld x17, 16 * 8(sp)
        ld x18, 17 * 8(sp)
        ld x19, 18 * 8(sp)
        ld x20, 19 * 8(sp)
        ld x21, 20 * 8(sp)
        ld x22, 21 * 8(sp)
        ld x23, 22 * 8(sp)
        ld x24, 23 * 8(sp)
        ld x25, 24 * 8(sp)
        ld x26, 25 * 8(sp)
        ld x27, 26 * 8(sp)
        ld x28, 27 * 8(sp)
        ld x29, 28 * 8(sp)
        ld x30, 29 * 8(sp)
        ld x31, 30 * 8(sp)

        // Restore stack pointer.
        addi    sp, sp, 8 * 31
.endm

#endif // PLM_WPR_ASSEMBLY_H
