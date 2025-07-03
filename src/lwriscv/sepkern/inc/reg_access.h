/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef __REG_ACCESS_H_
#define __REG_ACCESS_H_

// Register headers
#if !__ASSEMBLER__
#include "lwmisc.h"
#endif
#include "engine.h"
#include "asm_register.h"

#define csr_read(csrnum) ({ unsigned long __tmp; \
    __asm__ volatile ("csrr %0, %1" : "=r"(__tmp) : "i"(csrnum)); \
    __tmp; })

#define csr_write(csrnum, val) ({ \
    __asm__ volatile ("csrw %0, %1" ::"i"(csrnum), "r"(val)); })

#define csr_write_immediate(csrnum, val) ({ \
    __asm__ volatile ("csrwi %0, %1" ::"i"(csrnum), "i"(val)); })

#define csr_set(csrnum, val) ({ \
    __asm__ volatile ("csrs %0, %1" ::"i"(csrnum), "r"(val)); })

#define csr_clear(csrnum, val) ({ \
    __asm__ volatile ("csrc %0, %1" ::"i"(csrnum), "r"(val)); })

#endif // __MONITOR_REG_ACCESS_H_
