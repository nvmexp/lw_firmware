/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2008-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMUSW_H
#define PMUSW_H

/*!
 * @file pmusw.h
 */
#if UPROC_FALCON
#include <falcon-intrinsics.h>
#endif
#include "lwuproc.h"
#include "flcntypes.h"
#include "lwmisc.h"
#include "csb.h"
#include "lwrtos.h"
#include "lwoslayer.h"
#include "dmemovl.h"
#include "lwoscmdmgmt.h"
#include "osptimer.h"
#include "lib_lw64.h"
#include "lib_lwf32.h"
#include "rmpmucmdif.h"
#include "rmpbicmdif.h"
#include "g_pmuifrpc.h"
#include "config/g_profile.h"
#include "config/g_tasks.h"
#include "config/pmu-config.h"

/*!
 *  PMU CSB Register Macros
 */

//
// Falcon CSB (Config-Space Bus) Access Macros {PMU_CSB_*}
//

/******************************************************************************/
/*!
 * Redefining NULL for Vectorcast since NULL to NULL comparisons
 * in compile time asserts fail with the default definition i.e. ((void *)0)
 */
#if defined(VCAST_INSTRUMENT) && defined(UPROC_RISCV)
#ifdef NULL
#undef NULL
#endif
#define NULL 0
#endif // VCAST_INSTRUMENT && UPROC_RISCV

/*!
 * Clear a single bit in the value pointed to by 'ptr'.  The bit cleared is
 * specified by drf-value.  DO NOT specify DRF values representing fields
 * larger than 1-bit.  Doing so will only clear the lowest bit in the range.
 *
 * @param[in]  ptr  Pointer to the value to modify
 * @param[in]  drf  The 1-bit field to clear
 */
#define PMU_CLRB(ptr, drf)                                                     \
            falc_clrb_i((LwU32*)(ptr), DRF_SHIFT(drf))

/*!
 * Sets a single bit in the value pointed to by 'ptr'.  The bit set is
 * specified specified by drf-value.  DO NOT specify DRF values representing
 * fields larger than 1-bit.  Doing so will only set the lowest bit in the
 * range.
 *
 * @param[in]  ptr  Pointer to the value to modify
 * @param[in]  drf  The 1-bit field to set
 */
#define PMU_SETB(ptr, drf)                                                     \
            falc_setb_i((LwU32*)(ptr), DRF_SHIFT(drf))

/******************************************************************************/

/*!
 * Following macros default to their respective OS_*** equivalents.
 * We keep them forked to allow fast change of PMU implementations.
 */
#define PMU_HALT()                      OS_HALT()

/*!
 * @brief   Temporary refactoring macro for JIRA task GPUSWDTMRD-740
 */
#ifdef VCAST_INSTRUMENT
#define PMU_HALT_COND(cond)                                                    \
    do                                                                         \
    {                                                                          \
/*!
 * Skipping insturmentation for this code block since it cannot be tested.
 */                                                                            \
_Pragma("vcast_dont_instrument_start")                                         \
        if (!(cond))                                                           \
        {                                                                      \
            PMU_HALT();                                                        \
        }                                                                      \
_Pragma("vcast_dont_instrument_end")                                           \
    } while (LW_FALSE)
#else
#define PMU_HALT_COND(cond)                                                    \
    do                                                                         \
    {                                                                          \
        if (!(cond))                                                           \
        {                                                                      \
            PMU_HALT();                                                        \
        }                                                                      \
    } while (LW_FALSE)
#endif

#define PMU_BREAKPOINT()                OS_BREAKPOINT()

/*!
 * @brief   Temporary refactoring macro for JIRA task GPUSWDTMRD-745
 */
#define PMU_TRUE_BP()                   OS_TRUE_BP()

/*!
 * Following defines control the total number of IMEM overlays needed for
 * the INIT code.
 */
#define PMU_INIT_IMEM_OVERLAYS_INDEX_INIT       0x00U
#define PMU_INIT_IMEM_OVERLAYS_INDEX_LIBINIT    0x01U
#define PMU_INIT_IMEM_OVERLAYS_INDEX_LIBGC6     0x02U
#define PMU_INIT_IMEM_OVERLAYS_INDEX_LIBVC      0x03U
#define PMU_INIT_IMEM_OVERLAYS_INDEX_SCP_RAND   0x04U
#define PMU_INIT_IMEM_OVERLAYS__COUNT           0x05U

//
// Call a function that returns FLCN_STATUS and assert that the return value is FLCN_OK.
// If status is not FLCN_OK, then jump to label.
//
#ifdef VCAST_INSTRUMENT
#define PMU_HALT_OK_OR_GOTO(STATUS, CALL, LABEL)                               \
    do                                                                         \
    {                                                                          \
        STATUS = (CALL);                                                       \
/*!
 * Skipping insturmentation for this code block since it cannot be tested.
 */                                                                            \
_Pragma("vcast_dont_instrument_start")                                         \
        if (STATUS != FLCN_OK)                                                 \
        {                                                                      \
            PMU_BREAKPOINT();                                                  \
            goto LABEL;                                                        \
        }                                                                      \
_Pragma("vcast_dont_instrument_end")                                           \
    } while (LW_FALSE)
#else
#define PMU_HALT_OK_OR_GOTO(STATUS, CALL, LABEL)                               \
    do                                                                         \
    {                                                                          \
        STATUS = (CALL);                                                       \
        if (STATUS != FLCN_OK)                                                 \
        {                                                                      \
            PMU_BREAKPOINT();                                                  \
            goto LABEL;                                                        \
        }                                                                      \
    } while (LW_FALSE)
#endif

/*!
 * @brief Helper macro to provide and error handling pattern for
 * calling a FLCN_STATUS-returning expression, and then handling
 * non-FLCN_OK return values with a PMU_TRUE_BP() (notify of failure)
 * and then jumping to provided label, which is usually expected to be
 * an exit case.
 *
 * This pattern is expected to be used in code paths where errors are
 * not expected to occur, and if they do, code must stop forward
 * progress.  This pattern will thus identify errors via @ref
 * PMU_TRUE_BP(), store the error status into the provided variable,
 * and then jump to label, which is usually expected to stop the code
 * flow and return the error back to the caller.
 *
 * This pattern is expected to be the most-frequently used error
 * handling pattern in all PMU app code.
 *
 * @param[in/out]  STATUS
 *      FLCN_STATUS variable in which to store the return value of the
 *      @ref CALL expression and to check against FLCN_OK.
 * @param[in]      CALL
 *      The FLCN_STATUS-returning expression to evaluate.
 * @param[in]      LABEL
 *      Code label to which will jump if @ref CALL does not evaluate to FLCN_OK.
 */
#ifdef VCAST_INSTRUMENT
#define PMU_ASSERT_OK_OR_GOTO(STATUS, CALL, LABEL)                             \
    do                                                                         \
    {                                                                          \
        (STATUS) = (CALL);                                                     \
/*!
 * Skipping insturmentation for this code block since it cannot be tested.
 */                                                                            \
_Pragma("vcast_dont_instrument_start")                                         \
        if ((STATUS) != FLCN_OK)                                               \
        {                                                                      \
            PMU_TRUE_BP();                                                     \
            goto LABEL;                                                        \
        }                                                                      \
_Pragma("vcast_dont_instrument_end")                                           \
    } while (LW_FALSE)
#else
#define PMU_ASSERT_OK_OR_GOTO(STATUS, CALL, LABEL)                             \
    do                                                                         \
    {                                                                          \
        (STATUS) = (CALL);                                                     \
        if ((STATUS) != FLCN_OK)                                               \
        {                                                                      \
            PMU_TRUE_BP();                                                     \
            goto LABEL;                                                        \
        }                                                                      \
    } while (LW_FALSE)
#endif

/*!
 * @brief Helper macro to provide and error handling pattern for
 * evaluating a LwBool-returning expression, and then handling
 * LW_FALSE case with a specified error value, a PMU_TRUE_BP() (notify
 * of failure) and then jumping to provided label, which is usually
 * expected to be an exit case.
 *
 * This is effectively just a wrapper of @ref PMU_ASSERT_OK_OR_GOTO().
 *
 * @param[in/out]  STATUS
 *     FLCN_STATUS variable in which to store the return value of the
 *     @ref CALL expression and to check against FLCN_OK.
 * @param[in]      CONDITION
 *     The LwBool-returning expression to evaluate.  If returns
 *     LW_TRUE, will assign @ref STATUS = @ref FLCN_OK, else will
 *     assign with the values specified in the @ref ERROR.
 * @param[in]      ERROR
 *      @ref FLCN_STATUS error value to assign when @ref CONDITION
 *      evaluates to LW_FALSE.
 * @param[in]      LABEL
 *     Code label to which will jump if @ref CALL does not evaluate to FLCN_OK.
 */
#define PMU_ASSERT_TRUE_OR_GOTO(STATUS, CONDITION, ERROR, LABEL) \
    PMU_ASSERT_OK_OR_GOTO((STATUS), ((CONDITION) ? FLCN_OK : (ERROR)), LABEL)

/*!
 * @brief Helper macro to provide and error handling pattern for
 * calling a FLCN_STATUS-returning expression, and then handling
 * non-FLCN_OK return values with a PMU_TRUE_BP() and saving that
 * FLCN_STATUS return value if the provided FLCN_STATUS variable is
 * lwrrently set to FLCN_OK.
 *
 * This pattern is expected to bed use in code paths which cannot fail
 * (i.e. must continue on with forward progress), but want to identify
 * errors via @ref PMU_TRUE_BP() and return the first encountered
 * error value back to the caller.  This is a common usecase for
 * "unload", "destroy", and error/exit type code paths.
 *
 * @param[in/out]  STATUS
 *      FLCN_STATUS variable in which to store the return value of the
 *      @ref CALL expression and to check against FLCN_OK.
 * @param[in]      CALL
 *      The FLCN_STATUS-returning expression to evaluate.
 */
#ifdef VCAST_INSTRUMENT
#define PMU_ASSERT_OK_OR_CAPTURE_FIRST_ERROR(STATUS, CALL)                     \
    do                                                                         \
    {                                                                          \
        FLCN_STATUS _localStatus = (CALL);                                     \
/*!
 * Skipping insturmentation for this code block since it cannot be tested.
 */                                                                            \
_Pragma("vcast_dont_instrument_start")                                         \
        if (_localStatus != FLCN_OK)                                           \
        {                                                                      \
            if ((STATUS) == FLCN_OK)                                           \
            {                                                                  \
                (STATUS) = _localStatus;                                       \
            }                                                                  \
            PMU_TRUE_BP();                                                     \
        }                                                                      \
_Pragma("vcast_dont_instrument_stop")                                          \
    } while (LW_FALSE)
#else
#define PMU_ASSERT_OK_OR_CAPTURE_FIRST_ERROR(STATUS, CALL)                     \
    do                                                                         \
    {                                                                          \
        FLCN_STATUS _localStatus = (CALL);                                     \
        if (_localStatus != FLCN_OK)                                           \
        {                                                                      \
            if ((STATUS) == FLCN_OK)                                           \
            {                                                                  \
                (STATUS) = _localStatus;                                       \
            }                                                                  \
            PMU_TRUE_BP();                                                     \
        }                                                                      \
    } while (LW_FALSE)
#endif

/*!
 * Coverity assert handling -- basically inform Coverity that the condition is
 * verified independently.
 * http://lwcov.lwpu.com/docs/en/cov_checker_ref.html
 * http://lwcov.lwpu.com/docs/en/index.html
 */

#if defined(__COVERITY__)
void __coverity_panic__(void);
#define PMU_COVERITY_ASSERT_FAIL() __coverity_panic__()
#else
#define PMU_COVERITY_ASSERT_FAIL() ((void) 0)
#endif

/*!
 * @brief   Descriptor for the PMU's data memory buffer.
 */
typedef struct
{
    void   *pBuf;   //<! Location of the buffer (virtual address).
    LwU32   size;   //<! Available size of the buffer in bytes.
} PMU_DMEM_BUFFER;

/*!
 * Macro for aligning a pointer up to requested granularity
 */
#define PMU_ALIGN_UP_PTR(ptr, gran)                                            \
    ((void *)(LW_ALIGN_UP(((LwUPtr)(ptr)), (gran))))

#endif // PMUSW_H
