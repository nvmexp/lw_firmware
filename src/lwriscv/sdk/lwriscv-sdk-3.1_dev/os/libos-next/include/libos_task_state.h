/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef LIBOS_TASK_STATE_H_
#define LIBOS_TASK_STATE_H_
#include <libos-config.h>
#include <lwtypes.h>

/**
 *
 * @file libos_task_state.h
 *
 * @brief Task state shared between user API and kernel.
 *
 */

/**
 * @brief Fields in this structure are readable from user-space
 *        with libosTaskStateQuery provided your have debug
 *        permissions on the target isolation pasid.
 *
 */
typedef struct
{
    /** @see riscv-isa.h for register indices */
    LwU64 registers[32];

    /** PC address at time of trap */
    LwU64 xepc;

    /** Reason for trap
     *
     * Check interrupt bit REF_NUM64(LW_RISCV_CSR_MCAUSE_INT, 1ULL)
     * to determine exception/interupt.
     *
     * # Exceptions
     *  * LW_RISCV_CSR_MCAUSE_EXCODE_UCALL - Environment call from user-space
     *  * LW_RISCV_CSR_MCAUSE_EXCODE_IACC_FAULT - Instruction address fault
     *  * LW_RISCV_CSR_MCAUSE_EXCODE_LACC_FAULT - Load address fault
     *  * LW_RISCV_CSR_MCAUSE_EXCODE_SACC_FAULT - Store address fault
     *  * LW_RISCV_CSR_MCAUSE_EXCODE_SAMA_FAULT - Store alignment fault
     *  * LW_RISCV_CSR_MCAUSE_EXCODE_IAMA_FAULT - Instruction alignment fault
     *  * LW_RISCV_CSR_MCAUSE_EXCODE_LAMA_FAULT - Load alignment fault
     * # Interrupts
     *  * LW_RISCV_CSR_MCAUSE_EXCODE_M_TINT - Timer interrupt
     *  * LW_RISCV_CSR_MCAUSE_EXCODE_M_EINT - External interrupt
     */
    LwU64 xcause;

    /** Faulting load/store/exelwtion address
     *
     *  @note This address is software corrected on TU11x.
     *        @see kernel_trap_dispatch for WAR.
     */
    LwU64 xbadaddr;

#if !LIBOS_TINY
    /**
     * @brief The full kernel has tasks running in both S-mode and U-mode
     *        so we must context switch this register.  It would also be
     *        required to handle re-entrant faults in S-mode.
     * 
     *        LIBOS_TINY does not fault in S-mode, and strictly returns
     *        back to user-space.  Thus we don't context switch since the
     *        hardwares "previous privelege" level fields have the desired
     *        behavior in all cases.
     */
    LwU64 xstatus;
#endif


#if !LIBOS_TINY
    LwU64 radix;
#endif
} LibosTaskState;

#endif
