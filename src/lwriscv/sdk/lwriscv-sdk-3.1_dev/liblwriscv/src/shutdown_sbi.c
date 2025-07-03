/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#include <lwriscv/gcc_attrs.h>
#include <lwriscv/gcc_builtins.h>
#include <lwriscv/sbi.h>

GCC_ATTR_NAKED
GCC_ATTR_NOINLINE
GCC_ATTR_NORETURN
GCC_ATTR_NO_SSP
void riscvPanic(void)
{
    __asm__ ("li a7, 8 \n" // SBI_EXTENSION_SHUTDOWN
             "li a6, 0 \n" // SBI_LWFUNC_FIRST
             "ecall \n"
             "1: \n"
             "j 1b \n"
    );
    // If one of those assert is triggered, that means function needs to be updated
    _Static_assert(SBI_EXTENSION_SHUTDOWN == 8, "Shutdown extension id changed.");
    _Static_assert(SBI_LWFUNC_FIRST == 0, "Function id changed.");
    LWRV_BUILTIN_UNREACHABLE();
}

GCC_ATTR_NORETURN
GCC_ATTR_NO_SSP
void riscvShutdown(void)
{
    sbicall0(SBI_EXTENSION_SHUTDOWN, SBI_LWFUNC_FIRST);
    LWRV_BUILTIN_UNREACHABLE();
}
