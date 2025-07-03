/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#include <lwmisc.h>
#include <lwriscv/fence.h>
#include <liblwriscv/io.h>
#include <liblwriscv/shutdown.h>

/*
 * Enter ICD on crash to simplify debugging.
 */

__attribute__((noreturn)) void ccPanic(void)
{
#if CC_PERMIT_DEBUG
    localWrite(LW_PRGNLCL_RISCV_ICD_CMD, 0);
#endif // CC_PERMIT_DEBUG

    riscvLwfenceIO();

    // Just shut down the core.
    riscvShutdown();

    // In case SBI fails. Not much more we can do here.
    // coverity[no_escape]
    while(1)
    {
    }
    __builtin_unreachable();
}
