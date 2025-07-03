/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifdef LWRISCV_SDK
#include <liblwriscv/g_lwriscv_config.h>
#include <lwriscv/csr.h>
#else /* LWRISCV_SDK */
#ifndef RISCV_CSR_H
#define RISCV_CSR_H

#include "engine.h"
#include "dev_riscv_csr_64_addendum.h"

#define csr_read(csrnum) ({ unsigned long __tmp; \
    __asm__ volatile ("csrr %0, %1" : "=r"(__tmp) : "i"(csrnum)); \
    __tmp; })
#define csr_write(csrnum, val) ({ \
    __asm__ volatile ("csrw %0, %1" ::"i"(csrnum), "r"(val)); })
#define csr_set(csrnum, val) ({ \
    __asm__ volatile ("csrs %0, %1" ::"i"(csrnum), "r"(val)); })
#define csr_clear(csrnum, val) ({ \
    __asm__ volatile ("csrc %0, %1" ::"i"(csrnum), "r"(val)); })
#define csr_read_and_clear(csrnum, mask) ({ unsigned long __tmp; \
    __asm__ volatile ("csrrc %0, %1, %2" : "=r"(__tmp) :"i"(csrnum), "r"(mask)); \
    __tmp; })
#define csr_read_and_set(csrnum, mask) ({ unsigned long __tmp; \
    __asm__ volatile ("csrrs %0, %1, %2" : "=r"(__tmp) :"i"(csrnum), "r"(mask)); \
    __tmp; })
#define csr_read_and_clear_imm(csrnum, mask) ({ unsigned long __tmp; \
    __asm__ volatile ("csrrc %0, %1, %2" : "=r"(__tmp) :"i"(csrnum), "i"(mask)); \
    __tmp; })
#define csr_read_and_set_imm(csrnum, mask) ({ unsigned long __tmp; \
    __asm__ volatile ("csrrs %0, %1, %2" : "=r"(__tmp) :"i"(csrnum), "i"(mask)); \
    __tmp; })

#endif /* RISCV_CSR_H */
#endif /* LWRISCV_SDK */
