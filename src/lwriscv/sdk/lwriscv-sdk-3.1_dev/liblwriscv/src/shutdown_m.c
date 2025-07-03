/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#include <stdbool.h>
#include <lwmisc.h>
#include <lwriscv/csr.h>
#include <lwriscv/fence.h>
#include <lwriscv/gcc_attrs.h>
#include <lwriscv/gcc_builtins.h>

#include "liblwriscv/shutdown.h"

GCC_ATTR_NAKED
GCC_ATTR_NORETURN
GCC_ATTR_NO_SSP
static void riscvShutdownInt(void)
{
    __asm__("csrw   mie,  zero \n" // Disable interrupts
            "csrrw  zero, 0x800, zero\n" // lwfenceIO
#if LW_RISCV_CSR_MOPT
            "csrrwi zero, 0x7d8, 0\n" // write MOPT.HALT
#endif
            "1: \n"
            "j      1b\n" // infinite loop if halt fails (or not available)
    );
    _Static_assert(LW_RISCV_CSR_LWFENCEIO == 0x800,
                   "LWFENCEIO address has changed. Update assembly in riscvShutdownInt.");
    _Static_assert(LW_RISCV_CSR_MOPT == 0x7d8,
                   "MOPT address has changed. Update assembly in riscvShutdownInt.");
    _Static_assert(LW_RISCV_CSR_MOPT_CMD_HALT == 0,
                   "MOPT_CMD_HALT value has changed. Update assembly in riscvShutdownInt.");
    LWRV_BUILTIN_UNREACHABLE();
}

GCC_ATTR_NAKED
GCC_ATTR_NORETURN
GCC_ATTR_NO_SSP
void riscvPanic(void)
{
    riscvShutdownInt();
}

GCC_ATTR_NORETURN
GCC_ATTR_NO_SSP
void riscvShutdown(void)
{
    riscvShutdownInt();
}
