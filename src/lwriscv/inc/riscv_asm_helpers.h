/* _LWRM_COPYRIGHT_BEGIN_
*
* Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* _LWRM_COPYRIGHT_END_
*/

#ifdef LWRISCV_SDK
#include <lwriscv/asm-helpers.h>
#else /* LWRISCV_SDK */
#ifndef RISCV_ASM_HELPERS_H
#define RISCV_ASM_HELPERS_H

#define REGBYTES    8
#define SREG        sd
#define LREG        ld
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

#endif /* RISCV_ASM_HELPERS_H */
#endif /* LWRISCV_SDK */
