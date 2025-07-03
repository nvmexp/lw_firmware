/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef LWRISCV_PERFCTR_H
#define LWRISCV_PERFCTR_H
#include <stdint.h>
#include <lwriscv/csr.h>

/**
 * @brief Atomically reads timer csr.
 * @return Contents of time CSR.
 *
 * This register contains value of PTIMER (also available via PTIMER0/PTIMER1
 * localIO registers).
 * \note For pre-Hopper, this value is shifted: time = PTIMER >> 5.
 *
 * \warning This function requires permissions to execute.
 * Invalid instruction exception will be thrown if they're not granted.
 * - For M mode - it can always execute
 * - For S mode, mcounteren.tm must be set
 * - For U mode, mcounteren.tm and scounteren.tm must be set
 */
static inline uint64_t rdtime(void)
{
    uint64_t val = 0;

    __asm__ volatile("rdtime %0": "=r"(val));

    return val;
}

/**
 * @brief Atomically reads cycle counter csr.
 * @return Number of cpu cycles from arbitrary moment in time (RISC-V start)
 *
 * \warning This function requires permissions to execute.
 * Invalid instruction exception will be thrown if they're not granted.
 * - For M mode - it can always execute
 * - For S mode, mcounteren.cy must be set
 * - For U mode, mcounteren.cy and scounteren.cy must be set
 */
static inline uint64_t rdcycle(void)
{
    uint64_t cycle = 0;

    __asm__ volatile("rdcycle %0": "=r"(cycle));

    return cycle;
}

/**
 * @brief Atomically reads instruction retired counter csr.
 * @return Number of instructions retired from arbitrary moment in time (RISC-V start)
 *
 * \warning This function requires permissions to execute.
 * Invalid instruction exception will be thrown if they're not granted.
 * - For M mode - it can always execute
 * - For S mode, mcounteren.ir must be set
 * - For U mode, mcounteren.ir and scounteren.ir must be set
 */
static inline uint64_t rdinstret(void)
{
    uint64_t instret = 0;

    __asm__ volatile("rdinstret %0": "=r"(instret));

    return instret;
}

#endif // LWRISCV_PERFCTR_H
