/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef COMMON_CACHE_H
#define COMMON_CACHE_H
#include "csr.h"

static inline void lwfenceAll(void)
{
    __asm__ volatile ("csrrw zero, %0, zero" : : "i"(LW_RISCV_CSR_LWFENCEALL));
}

static inline void lwfenceIo(void)
{
    __asm__ volatile ("csrrw zero, %0, zero" : : "i"(LW_RISCV_CSR_LWFENCEIO));
}

static inline void lwfenceMem(void)
{
    __asm__ volatile ("csrrw zero, %0, zero" : : "i"(LW_RISCV_CSR_LWFENCEMEM));
}

#endif // COMMON_CACHE_H
