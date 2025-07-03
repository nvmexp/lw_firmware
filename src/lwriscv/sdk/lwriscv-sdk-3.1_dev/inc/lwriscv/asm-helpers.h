/* _LWRM_COPYRIGHT_BEGIN_
*
* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* _LWRM_COPYRIGHT_END_
*/
#ifndef LWRISCV_ASM_HELPERS_H
#define LWRISCV_ASM_HELPERS_H

#if (__riscv_xlen == 32)
#define REGBYTES    4
#define SREG        sw
#define LREG        lw
#else
#define REGBYTES    8
#define SREG        sd
#define LREG        ld
#endif // (__riscv_xlen == 32)
#define ZREG(rd)    mv rd, zero

.macro FUNC name
   .global \name
   \name :
   .func \name, \name
    .type \name, STT_FUNC
.endm

.macro EFUNC name
    .endfunc
    .size \name, .-\name
.endm

#endif /* LWRISCV_ASM_HELPERS_H */
