/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef RISCV_INTRINSICS_H
#define RISCV_INTRINSICS_H

/*!
 * @file    riscv-intrinsics.h
 *
 * This header emulates subset of falcon instructions via RISCV instruction set.
 * So existing falcon ucode can be easier ported to RISCV with minimal change.
 */

#include "lwmisc.h"
#include "bar0_defs.h"
#include "memmap.h"
#include "dev_falcon_v4.h"

#if LWRISCV_PRINT_RAW_MODE
# include "shlib/string.h"
#endif // LWRISCV_PRINT_RAW_MODE

void riscv_stx( unsigned int *cfgAdr, unsigned int val );
void riscv_stx_i( unsigned int *cfgAdr, const unsigned int index_imm, unsigned int val );
void riscv_stxb_i( unsigned int *cfgAdr, const unsigned int index_imm, unsigned int val );
unsigned int riscv_ldx( unsigned int *cfgAdr, unsigned int index );
unsigned int riscv_ldx_i( unsigned int *cfgAdr, const unsigned int index_imm );
unsigned int riscv_ldxb_i( unsigned int *cfgAdr, const unsigned int index_imm );

/*!
 * @brief DRF defines for embedding the line number and the
 *        bp type code in the breakpoint arg.
 *
 * The low bits of the 64-bit register value are reserved for the
 * bp type code, the rest can be used for the line number (generous).
 *
 * The arg received by riscv_break is expected to be constructed
 * with the use of these DRF defines.
 */
#define LW_LWRISCV_INTRINSIC_TRAP_ARG_BP_TYPE                0:0
#define LW_LWRISCV_INTRINSIC_TRAP_ARG_BP_TYPE_HALT           0x0
#define LW_LWRISCV_INTRINSIC_TRAP_ARG_BP_TYPE_RESUMABLE      0x1
#define LW_LWRISCV_INTRINSIC_TRAP_ARG_BP_TYPE_INIT           0x0
#define LW_LWRISCV_INTRINSIC_TRAP_ARG_LOC                    63:1
#define LW_LWRISCV_INTRINSIC_TRAP_ARG_LOC_INIT               0x0

/*!
 * This is the baseline function for ebreak ilwocations, providing an interface
 * to pass a 64-bit arg to ebreak.
 * The argument is expected to be constructed via LW_LWRISCV_INTRINSIC_TRAP_ARG_*.
 *
 * @param[in] arg  used to pass the generic arg.
 */
static LW_FORCEINLINE void
riscv_break(LwU64 arg)
{
    register LwU64 a0 __asm__( "a0" ) = arg;
    __asm__ __volatile__("ebreak" : "+r"( a0 ) ::);
}

#if LWRISCV_PRINT_RAW_MODE

/*!
 * This macro can be passed as a dispatcher to the logging interface,
 * it will pass the logging metadata to riscv_break as proper handling
 * for the _halt call.
 *
 * @param[in] _numTokens  ignored, assumed to always be 1.
 * @param[in] _tokens     only _tokens[0] is used (as the metadata).
 */
#define riscv_halt_print_dispatcher(_numTokens, _tokens) \
    do                                                   \
    {                                                    \
        ct_assert((_numTokens) == 1);                    \
        /* metadata ptr always has alignment > byte,  */ \
        /* this saves the compiler a few instructions */ \
        /* to set up the immediate in a0.             */ \
        /* The low byte is always 0 in this case.     */ \
        riscv_break((_tokens)[0]); /* tokens[0] & ~1U */ \
        __builtin_unreachable();                         \
    } while (LW_FALSE)

/*!
 * This macro can be passed as a dispatcher to the logging interface,
 * it will pass the logging metadata to as per proper handling
 * for the _trap1 call. This is designed for old-style fatal breakpoints,
 * so it simply expands to a "normal" halt handler.
 * MMINTS-TODO: Eventually this will have to be removed.
 *
 * @param[in] _numTokens  ignored, assumed to always be 1.
 * @param[in] _tokens     only _tokens[0] is used (as the metadata).
 */
#define riscv_trap1_print_dispatcher(_numTokens, _tokens) \
    riscv_halt_print_dispatcher(_numTokens, _tokens)

//! Intermediate paster macro to generate dispatcher name
#define DISPATCHER_NAME(dispatcher)  dispatcher##_print_dispatcher

//! Short-hand for ilwoking the logging macro, expects an arg like INSN_NAME(halt).
# define LWRISCV_BREAK_RAW_MODE(dispatcher)                                   \
    LWRISCV_LOG_INTERNAL(DISPATCHER_NAME(dispatcher), PRINT_DYN_LEVEL_ERROR,  \
                         "Halted at "__FILE__":"LWRISCV_TO_STR(__LINE__)"\n")

#endif // LWRISCV_PRINT_RAW_MODE

/*!
 * This function is implemented to satisfy platform-agnostic user-code halt calls.
 * PMU, lwos etc. use lwuc_halt, which expands to falc_halt or riscv_halt.
 * This will expand to riscv_halt, setting _BP_TYPE to _HALT. The other
 * _BP_TYPE-s are set by custom ilwocations of riscv_trap1 in macros etc.
 *
 * @param[in] loc  used to pass the line-number/location where the halt oclwrred.
 */
__attribute__ ((noreturn)) static LW_FORCEINLINE void
riscv_halt(LwU64 loc)
{
    LwU64 arg = 0;

    arg = FLD_SET_DRF_NUM64(_LWRISCV_INTRINSIC, _TRAP_ARG, _LOC, loc, arg);
    arg = FLD_SET_DRF64(_LWRISCV_INTRINSIC, _TRAP_ARG, _BP_TYPE, _HALT, arg);

    riscv_break(arg);

    __builtin_unreachable();
}

/*!
 * This function is implemented to match lwuc_trap1, used on PMU etc.
 * for old-style fatal breakpoints. Simply expands to a "normal" halt.
 * MMINTS-TODO: Eventually this will have to be removed.
 *
 * @param[in] loc  used to pass the line-number/location where the fatal breakpoint oclwrred.
 */
__attribute__ ((noreturn)) static LW_FORCEINLINE void
riscv_trap1(LwU64 loc)
{
    riscv_halt(loc);
}

#endif // RISCV_INTRINSICS_H
