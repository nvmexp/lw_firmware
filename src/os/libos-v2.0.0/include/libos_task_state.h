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
     *  @note This address is software corrected on Turing.
     *        @see kernel_datafault for WAR.
     *
     *  @note When xcause indicates an external interrupt (M_EINT) AND
     *        it corresponds with a MemErr (priv_err_info != 0),
     *        xbadaddr is a physical address. In all other cases,
     *        xbadaddr is a virtual address.
     */
    LwU64 xbadaddr;

    /** LWRISCV-specific state
     *
     * This data, which is not available through standard CSRs, provides
     * additional info for the task state.
     */
    struct
    {
        /** Faulting I/O load/store error info.
         *
         * This field is reset only when xcause indicates M_EINT or LACC_FAULT,
         * and it should only be read for those causes. Only certain error
         * conditions within those causes will populate this field with a
         * non-zero PRI error code. Otherwise, this field will remain 0.
         *
         * Reading this field for other causes will likely yield a stale error
         * code from a prior fault.
         */
        LwU32 priv_err_info;

        /** Align structure size to 8 bytes **/
        LwU32 reserved;
    } lwriscv;
} LibosTaskState;

#endif
