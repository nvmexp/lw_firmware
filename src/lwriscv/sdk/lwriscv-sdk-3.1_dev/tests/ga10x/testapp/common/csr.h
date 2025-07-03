/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef CSR_H
#define CSR_H
#include <dev_riscv_csr_64.h>

#define csr_write(csr, value) \
    __asm__ __volatile__ ("csrw %1, %0" : : "r"(value), "i"(csr) : "memory")

#define csr_set(csr, mask) \
    __asm__ __volatile__ ("csrs %1, %0" : : "r"(mask), "i"(csr) : "memory");

#define csr_clear(csr, mask) \
    __asm__ __volatile__ ("csrc %1, %0" : : "r"(mask), "i"(csr) : "memory");

#define csr_read(csr) \
    ({ \
    unsigned long long __v; \
    __asm__ __volatile__ ("csrr %0, %1" : "=r"(__v) : "i"(csr) : "memory"); \
    __v; \
    })

#endif
