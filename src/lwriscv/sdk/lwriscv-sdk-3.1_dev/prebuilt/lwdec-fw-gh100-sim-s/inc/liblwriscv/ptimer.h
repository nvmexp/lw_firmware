/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef LIBLWRISCV_PTIMER_H
#define LIBLWRISCV_PTIMER_H
#include <stdint.h>
#include <lwriscv/perfctr.h>

/*
 * Chips prior to HOPPER have timer shifted by 5 bits.
 * For Hopper or later, rdtime returns PTIMER value.
 */
#if LWRISCV_HAS_BINARY_PTIMER
#define TIME_TO_PTIMER_SHIFT 0
#else // LWRISCV_HAS_BINARY_PTIMER
#define TIME_TO_PTIMER_SHIFT 5
#endif // LWRISCV_HAS_BINARY_PTIMER

/*!
 * \brief Read 64-bit PTIMER
 * \return PTIMER value (1ns resolution)
 *
 * This function reads PTIMER, avoiding non-trivial reads from two 32-bit
 * localIO registers (PTIMER0/PTIMER1).
 *
 * \warning mcounteren.tm (for S- and U-mode) and scounteren.tm (for U-mode)
 * must be set or this function will cause invalid instruction exception.
 */
static inline uint64_t ptimer_read(void)
{
    return rdtime() << TIME_TO_PTIMER_SHIFT;
}

#endif // LIBLWRISCV_PTIMER_H
