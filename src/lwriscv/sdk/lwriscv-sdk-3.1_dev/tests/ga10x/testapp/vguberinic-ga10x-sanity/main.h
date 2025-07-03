/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#include <lwtypes.h>
#include <lwmisc.h>
#include <dev_riscv_csr_64.h>
#include <dev_riscv_addendum.h>
#include <dev_fbif_v4.h>
#include <dev_fbif_addendum.h>
#include "io.h"
#include "csr.h"
#include "debug.h"

#define HALT() halt(__LINE__)

extern LwBool exceptionExpectSet;

void halt(int line);
void exceptionExpect(LwU64 cause);
void exceptionReset(void);
LwBool tryLoad(LwU64 addr, LwU32 *pVal);
LwBool tryStore(LwU64 addr, LwU32 val);
void mToS(void);
void mToU(void);

#define sToM() do { __asm__ volatile ("ecall"); } while(0)
#define uToM() do { __asm__ volatile ("ecall"); } while(0)




