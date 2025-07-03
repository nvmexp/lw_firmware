/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LIBS_CPU_INTRINSICS_H
#define LIBS_CPU_INTRINSICS_H

/*!
 * @file    cpu-intrinsics.h
 */

#if defined(UPROC_RISCV)
    #define ARCH riscv
    #include "riscv-intrinsics.h"
#elif defined(UPROC_FALCON)
    #define ARCH falc
    #include "falcon-intrinsics.h"
#else
    #error Unsupported CPU architecture!
#endif

/*!
 * Helper macros to concatenate the HW arch ("riscv" or "falc") with instruction name.
 * e.g. lwuc_stx will be replaced by riscv_stx on RISCV build or be replaced by falc_stx on Falcon build.
 */
#define PASTER(arch, insn)                   arch##_##insn
#define EVALUATOR(arch, insn)                PASTER(arch, insn)
#define INSN_NAME(insn)                      EVALUATOR(ARCH, insn)

#define lwuc_stx(a, b)                       INSN_NAME(stx)(a, b)
#define lwuc_stx_i(a, b, c)                  INSN_NAME(stx_i)(a, b, c)
#define lwuc_stxb(a, b)                      INSN_NAME(stxb)(a, b)
#define lwuc_stxb_i(a, b, c)                 INSN_NAME(stxb_i)(a, b, c)
#define lwuc_ldx(a, b)                       INSN_NAME(ldx)(a, b)
#define lwuc_ldxb(a, b)                      INSN_NAME(ldxb)(a, b)
#define lwuc_ldx_i(a, b)                     INSN_NAME(ldx_i)(a, b)
#define lwuc_ldxb_i(a, b)                    INSN_NAME(ldxb_i)(a, b)
#define lwuc_trap0()                         INSN_NAME(trap0)()
#ifdef UPROC_RISCV
# if LWRISCV_PRINT_RAW_MODE
#  define lwuc_halt()                        LWRISCV_BREAK_RAW_MODE(INSN_NAME(halt))
#  define lwuc_trap1()                       LWRISCV_BREAK_RAW_MODE(INSN_NAME(trap1))
# else // LWRISCV_PRINT_RAW_MODE
#  define lwuc_halt()                        INSN_NAME(halt)(__LINE__)
#  define lwuc_trap1()                       INSN_NAME(trap1)(__LINE__)
# endif // LWRISCV_PRINT_RAW_MODE
#else // UPROC_RISCV
# define lwuc_halt()                         INSN_NAME(halt)()
# define lwuc_trap1()                        INSN_NAME(trap1)()
#endif // UPROC_RISCV
#define lwuc_trap2()                         INSN_NAME(trap2)()
#define lwuc_trap3()                         INSN_NAME(trap3)()
#define lwuc_ssetb_i(a)                      INSN_NAME(ssetb_i)(a)
#define lwuc_wait(a)                         INSN_NAME(wait)(a)
#define lwuc_sethi_i(a, b)                   INSN_NAME(sethi_i)(a, b)
#define lwuc_mv32i(a)                        INSN_NAME(mv32i)(a)

#endif // LIBS_CPU_INTRINSICS_H
