/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <stdint.h>
#include <stddef.h>
#include <lwmisc.h>
#include <liblwriscv/io.h>
#include <liblwriscv/print.h>
#include <liblwriscv/shutdown.h>
#include <lwriscv/csr.h>
#include <lwriscv/fence.h>

#include "tests.h"
#include "util.h"


uint64_t testFenceMem(void)
{
    printf("Testing fence... \n");
    TRAP_CLEAR();
    riscvFenceRW();
    if (trap_mcause != 0) {
        printf("Got exception %d on fencerw\n", trap_mcause);
        return MAKE_DEBUG_NOVAL(TestId_LWDEC, 0, 1);

    }
    TRAP_CLEAR();
    __asm__ volatile ("csrrw x0, %0, %1\nnop" ::"i"(LW_RISCV_CSR_MFLUSH), "r"(1));
    if (trap_mcause != 0) {
        printf("Got exception %d on mflush\n", trap_mcause);
    }
    TRAP_CLEAR();
    printf("Done.\n");
    return PASSING_DEBUG_VALUE;
}

uint64_t test_lwdec(void)
{
    printf("Testing LWDEC bugs...\n");
    return testFenceMem();
}
