/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2008-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   dbgprintf.h
 * @brief  Provides the interfaces for a basic printf library for the PMU
 *         S/W application.
 *
 * TODO: RENAME THIS FILE TO pmudebug.h
 */

#ifndef DBGPRINTF_H
#define DBGPRINTF_H

#include "lwostrace.h"
#include "lwos_utility.h"
#include "config/pmu-config.h"
#include "flcntypes.h"
#include "lwctassert.h"

#ifdef UPROC_RISCV
# include <lwriscv/print.h>
#else // UPROC_RISCV
# define dbgPrintf(...)  lwosNOP()
#endif // UPROC_RISCV

/*!
 * DBG_PRINTF
 *
 * Properly define the DBG_PRINTF macro based on whether or not debug
 * print statement are enabled.  When not enabled, DBG_PRINTF should
 * evaluate to nothing.
 *
 * @param[in] x String to print
 *
 */
#if (PMUCFG_FEATURE_ENABLED(PMUDBG_PRINT))
    #ifdef UPROC_RISCV
        #define  DBG_PRINTF_INTERNAL(fmt, ...)  dbgPrintf(LEVEL_ALWAYS, fmt, ##__VA_ARGS__)
        #define  DBG_PRINTF(x)                  DBG_PRINTF_INTERNAL x
        #define  DBG_PRINTK(x)                  DBG_PRINTF_INTERNAL x
    #elif (PMUCFG_FEATURE_ENABLED(PMUDBG_OS_TRACE_REG))
        #define  DBG_PRINTF(x)                  osTraceReg x
    #else
        #error A debug-print mode must be enabled when printf is enabled
        #define  DBG_PRINTF(x)
    #endif
#else // PMUCFG_FEATURE_ENABLED(PMUDBG_PRINT)
    #define  DBG_PRINTF(x)
#endif // PMUCFG_FEATURE_ENABLED(PMUDBG_PRINT)

#ifndef DBG_PRINTK
#define  DBG_PRINTK(x)  DBG_PRINTF(x)
#endif // DBG_PRINTK

/*!
 * DBG_PRINTF_ISR
 */
#if (PMUCFG_FEATURE_ENABLED(PMUDBG_OS_TRACE_REG))
    #define  DBG_PRINTF_ISR(x)                  osTraceReg x
#else // PMUCFG_FEATURE_ENABLED(PMUDBG_OS_TRACE_REG)
    #define  DBG_PRINTF_ISR(x)
#endif // PMUCFG_FEATURE_ENABLED(PMUDBG_OS_TRACE_REG)

/*!
 * @brief PMU Compile-Time Assert
 *
 * PMU_CT_ASSERT asserts a condition that can be caught during compile time. It
 * ensures checking certain conditions without adding any code or data.
 *
 * Example:
 *  PMU_CT_ASSERT(sizeof(LwU32) == 4);
 *  PMU_CT_ASSERT(LWSTOM_DEFINE == LW_DEVICE_REGISTER_FIELD_VAL);
 */
#define PMU_CT_ASSERT(cond)  ct_assert(cond)

/*!
 * PMU_BUG() can be placed where we do not expect to be exelwting. It is useful
 * to catch a run-time exception (accessing an array with an out-of-boundary
 * index for example).
 *
 * Usage:
 * PMU_BUG() can be used in two ways - with or without an optional
 * parameter which becomes the return value of your choice. It is often useful
 * if you're using it in the assignment statement.
 *
 * Example 1)
 *    // g_idx can't be ILWALID_IDX.
 *    int val = g_idx == ILWALID_IDX ? PMU_BUG(0) : vals[g_idx];
 *
 * Example 2)
 *   // we should never get here
 *   PMU_BUG();
 *
 * Note:
 * - This is a GCC extension (compound statement expressions).
 * - The optional parameter x won't be evaluated as an expression because
 *   PMU_HALT is called first. Unless PMU is manually resumed, it has no
 *   effect.  It's there to make the compiler see this as a valid expression.
 */
#define PMU_BUG(x)  ({PMU_HALT(); x;})

/*!
 * The following is a series of defines (one for each OS task) that
 * allow debug print statements to be compiled in/out on a per-task
 * basis.  When zero, debug print statements for that task are
 * compiled out.  These have no effect when DEBUG is not defined.
 */
#define  DBG_PRINTF_ENABLED_CMDMGMT     0
#define  DBG_PRINTF_ENABLED_PG          0
#define  DBG_PRINTF_ENABLED_FIFO        0
#define  DBG_PRINTF_ENABLED_SEQ         0
#define  DBG_PRINTF_ENABLED_I2C         0
#define  DBG_PRINTF_ENABLED_DIDLE       0
#define  DBG_PRINTF_ENABLED_PBI         0
#define  DBG_PRINTF_ENABLED_PCM         0
#define  DBG_PRINTF_ENABLED_PERFMON     0
#define  DBG_PRINTF_ENABLED_DISP        0
#define  DBG_PRINTF_ENABLED_HDCP        0
#define  DBG_PRINTF_ENABLED_PMGR        0
#define  DBG_PRINTF_ENABLED_THERM       0
#define  DBG_PRINTF_ENABLED_SMBPBI      0
#define  DBG_PRINTF_ENABLED_PERF        0
#define  DBG_PRINTF_ENABLED_DPAUX       0
#define  DBG_PRINTF_ENABLED_ACR         0
#define  DBG_PRINTF_ENABLED_SPI         0
#define  DBG_PRINTF_ENABLED_PERF_DAEMON 0
#define  DBG_PRINTF_ENABLED_BIF         0
#define  DBG_PRINTF_ENABLED_PERF_CF     0
#define  DBG_PRINTF_ENABLED_WATCHDOG    0
#define  DBG_PRINTF_ENABLED_LOWLATENCY  0

/*!
 * Properly define all DBG_PRINTF_<task> defines based on whether or
 * not debug print-statements are enabled for that task.
 */
#if (DBG_PRINTF_ENABLED_CMDMGMT)
#define  DBG_PRINTF_CMDMGMT(x)          DBG_PRINTF(x)
#else
#define  DBG_PRINTF_CMDMGMT(x)
#endif

#if (DBG_PRINTF_ENABLED_PG)
#define  DBG_PRINTF_PG(x)               DBG_PRINTF(x)
#else
#define  DBG_PRINTF_PG(x)
#endif

#if (DBG_PRINTF_ENABLED_PCM)
#define  DBG_PRINTF_PCM(x)             DBG_PRINTF(x)
#else
#define  DBG_PRINTF_PCM(x)
#endif

#if (DBG_PRINTF_ENABLED_FIFO)
#define  DBG_PRINTF_FIFO(x)             DBG_PRINTF(x)
#else
#define  DBG_PRINTF_FIFO(x)
#endif

#if (DBG_PRINTF_ENABLED_SEQ)
#define  DBG_PRINTF_SEQ(x)              DBG_PRINTF(x)
#else
#define  DBG_PRINTF_SEQ(x)
#endif

#if (DBG_PRINTF_ENABLED_I2C)
#define  DBG_PRINTF_I2C(x)              DBG_PRINTF(x)
#else
#define  DBG_PRINTF_I2C(x)
#endif

#if (DBG_PRINTF_ENABLED_DIDLE)
#define  DBG_PRINTF_DIDLE(x)            DBG_PRINTF(x)
#else
#define  DBG_PRINTF_DIDLE(x)
#endif

#if (DBG_PRINTF_ENABLED_PBI)
#define DBG_PRINTF_PBI(x)               DBG_PRINTF(x)
#else
#define DBG_PRINTF_PBI(x)
#endif

#if (DBG_PRINTF_ENABLED_PERFMON)
#define DBG_PRINTF_PERFMON(x)           DBG_PRINTF(x)
#else
#define DBG_PRINTF_PERFMON(x)
#endif

#if (DBG_PRINTF_ENABLED_DISP)
#define DBG_PRINTF_DISP(x)           DBG_PRINTF(x)
#else
#define DBG_PRINTF_DISP(x)
#endif

#if (DBG_PRINTF_ENABLED_HDCP)
#define DBG_PRINTF_HDCP(x)              DBG_PRINTF(x)
#else
#define DBG_PRINTF_HDCP(x)
#endif

#if (DBG_PRINTF_ENABLED_PMGR)
#define DBG_PRINTF_PMGR(x)              DBG_PRINTF(x)
#else
#define DBG_PRINTF_PMGR(x)
#endif

#if (DBG_PRINTF_ENABLED_THERM)
#define DBG_PRINTF_THERM(x)             DBG_PRINTF(x)
#define DBG_BREAKPOINT_THERM()          PMU_BREAKPOINT()
#else
#define DBG_PRINTF_THERM(x)
#define DBG_BREAKPOINT_THERM()
#endif

#if (DBG_PRINTF_ENABLED_SMBPBI)
#define DBG_PRINTF_SMBPBI(x)            DBG_PRINTF(x)
#else
#define DBG_PRINTF_SMBPBI(x)
#endif

#if (DBG_PRINTF_ENABLED_PERF)
#define DBG_PRINTF_PERF(x)            DBG_PRINTF(x)
#else
#define DBG_PRINTF_PERF(x)
#endif

#if (DBG_PRINTF_ENABLED_PERF_DAEMON)
#define DBG_PRINTF_PERF_DAEMON(x)      DBG_PRINTF(x)
#else
#define DBG_PRINTF_PERF_DAEMON(x)
#endif

#if (DBG_PRINTF_ENABLED_DPAUX)
#define  DBG_PRINTF_DPAUX(x)           DBG_PRINTF(x)
#else
#define  DBG_PRINTF_DPAUX(x)
#endif

#if (DBG_PRINTF_ENABLED_ACR)
#define  DBG_PRINTF_ACR(x)             DBG_PRINTF(x)
#else
#define  DBG_PRINTF_ACR(x)
#endif

#if (DBG_PRINTF_ENABLED_SPI)
#define  DBG_PRINTF_SPI(x)             DBG_PRINTF(x)
#else
#define  DBG_PRINTF_SPI(x)
#endif

#if (DBG_PRINTF_ENABLED_BIF)
#define DBG_PRINTF_BIF(x)              DBG_PRINTF(x)
#else
#define DBG_PRINTF_BIF(x)
#endif

#if (DBG_PRINTF_ENABLED_PERF_CF)
#define DBG_PRINTF_PERF_CF(x)          DBG_PRINTF(x)
#else
#define DBG_PRINTF_PERF_CF(x)
#endif

#if (DBG_PRINTF_ENABLED_WATCHDOG)
#define DBG_PRINTF_WATCHDOG(x)         DBG_PRINTF(x)
#else
#define DBG_PRINTF_WATCHDOG(x)
#endif

#if (DBG_PRINTF_ENABLED_LOWLATENCY)
#define DBG_PRINTF_LOWLATENCY(x)       DBG_PRINTF(x)
#else
#define DBG_PRINTF_LOWLATENCY(x)
#endif

#endif // DBGPRINTF_H

