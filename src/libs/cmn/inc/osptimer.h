/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2007-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef OSPTIMER_H
#define OSPTIMER_H

/*!
 * @file osptimer.h
 * @brief      This headerfile contains the function prototypes for
 *             osptimer.c
 */

/* ------------------------ System includes --------------------------------- */
#include "lwuproc.h"
#include "flcntypes.h"

/* ------------------------ Application includes ---------------------------- */
/* ------------------------ Types definitions ------------------------------- */
/*!
 * @brief      Timestamp structure used for tracking time based on the PTIMER
 *             registers (PTIMER0 and PTIMER1).
 *
 * @details    Time is a 64-bit integer representing nanoseconds. Falcon-GCC
 *             supports hardware emulation of simple 64-bit integer (add/sub/
 *             comparisons). 64-bit add/sub is translated into 2 instructions.
 *             Stay away from any multiplication or division as these require
 *             software.
 */
typedef FLCN_U64  FLCN_TIMESTAMP;

/*!
 * @brief      Pointer typedef for @ref FLCN_TIMESTAMP.
 */
typedef FLCN_U64 *PFLCN_TIMESTAMP;

/*!
 * @brief      Typedef for functions called until either a condition is reached
 *             or a time limit is reached.
 */
typedef LwBool (* OS_PTIMER_COND_FUNC)(void *pArgs);

/* ------------------------ Defines ----------------------------------------- */
/*!
 * @brief      Spin-wait for 'delayNs' nanoseconds.
 *
 * @copydoc    osPTimerSpinWaitNs
 *
 * @param[in]  _delayNs  Number of nanoseconds to delay for.
 */
#define OS_PTIMER_SPIN_WAIT_NS(_delayNs)                                       \
    osPTimerSpinWaitNs(_delayNs)

/*!
 * @brief      Spin-wait for 'delayUs' microseconds.
 *
 * @note       This macro performs multiplication, use it with caution. Only
 *             delay values that are known at compile-time should be passed in.
 *
 * @param[in]  _delayUs  The number of microseconds to delay for.
 */
#define OS_PTIMER_SPIN_WAIT_US(_delayUs)                                       \
    OS_PTIMER_SPIN_WAIT_NS((_delayUs) * 1000U)

/*!
 * @brief      Wait until condition is satisfied or delayNs nanoseconds have
 *             elapsed.
 *
 * @copydoc osPTimerCondSpinWaitNs
 */
#define OS_PTIMER_SPIN_WAIT_NS_COND(_fp, _pArgs, _delayNs)                     \
    osPTimerCondSpinWaitNs(                                                    \
        (OS_PTIMER_COND_FUNC)(_fp), (void *)(_pArgs), (_delayNs))

/*!
 * @brief      Wait until condition is satisfied or delay for delayUs
 *             microseconds. See @ref OS_PTIMER_SPIN_WAIT_NS_COND
 */
#define OS_PTIMER_SPIN_WAIT_US_COND(_fp, _pArgs, _delayUs)                     \
    OS_PTIMER_SPIN_WAIT_NS_COND((_fp), (_pArgs), (_delayUs) * 1000U)

/* ------------------------ External definitions ---------------------------- */
/* ------------------------ Static variables -------------------------------- */
/* ------------------------ Function Prototypes ----------------------------- */
#define osPTimerTimeNsLwrrentGet MOCKABLE(osPTimerTimeNsLwrrentGet)
void   osPTimerTimeNsLwrrentGet(PFLCN_TIMESTAMP pTime);
LwU32  osPTimerTimeNsElapsedGet(PFLCN_TIMESTAMP pTime);

#define osPTimerCondSpinWaitNs MOCKABLE(osPTimerCondSpinWaitNs)
LwBool osPTimerCondSpinWaitNs(OS_PTIMER_COND_FUNC fp, void *pArgs, LwU32 delayNs);
void   osPTimerSpinWaitNs(LwU32 delayNs);

void   osPTimerTimeNsLwrrentGet_hs(PFLCN_TIMESTAMP pTime)
    GCC_ATTRIB_SECTION("imem_libCommonHs", "osPTimerTimeNsLwrrentGet_hs");
LwU32  osPTimerTimeNsElapsedGet_hs(PFLCN_TIMESTAMP pTime)
    GCC_ATTRIB_SECTION("imem_libCommonHs", "osPTimerTimeNsElapsedGet_hs");
LwBool osPTimerCondSpinWaitNs_hs(OS_PTIMER_COND_FUNC fp, void *pArgs, LwU32 delayNs)
    GCC_ATTRIB_SECTION("imem_libCommonHs", "osPTimerCondSpinWaitNs_hs");

/* ------------------------ Inline Functions -------------------------------- */

#ifdef UPROC_RISCV
// Not inlined on RISCV
void   osPTimerTimeNsLwrrentGetUnaligned(LwU64_ALIGN32 *pTimeData);
LwU32  osPTimerTimeNsElapsedGetUnaligned(LwU64_ALIGN32 *pTimeData);
#else // UPROC_RISCV
/*!
 * @brief      Used to get the current time in nanoseconds as an LwU64_ALIGN32
 *             (Falcon version, uses 32-bit alignment because Falcon is 32-bit)
 *
 * @details    Note: although this function casts to FLCN_TIMESTAMP*, you should
 *             NOT be casting to FLCN_TIMESTAMP* manually in order to avoid
 *             alignment issues and prevent RISCV issues!
 *
 * @param[out] pTimeData  Pointer to LwU64_ALIGN32 to be filled with current
 *                        time in nS.
 */
static inline void osPTimerTimeNsLwrrentGetUnaligned(LwU64_ALIGN32 *pTimeData)
{
    osPTimerTimeNsLwrrentGet((FLCN_TIMESTAMP *)pTimeData);
}

/*!
 * @brief      Measure the time in nanoseconds elapsed since 'pTime', where
 *             'pTime' is passed in as an LwU64_ALIGN32
 *             (Falcon version, uses 32-bit alignment because Falcon is 32-bit)
 *
 * @details    Note: although this function casts to FLCN_TIMESTAMP*, you should
 *             NOT be casting to FLCN_TIMESTAMP* manually in order to avoid
 *             alignment issues and prevent RISCV issues!
 *
 * @param[in]  pTime  pointer to LwU64_ALIGN32 timestamp to measure from.
 *
 */
static inline LwU32 osPTimerTimeNsElapsedGetUnaligned(LwU64_ALIGN32 *pTimeData)
{
    return osPTimerTimeNsElapsedGet((FLCN_TIMESTAMP *)pTimeData);
}

#endif // UPROC_RISCV

/*!
 * @brief      Used to get the current time in nanoseconds as an LwU64
 *
 * @details    This assumes that the LwU64 has 64-bit alignment.
 *             Note: although this function casts to FLCN_TIMESTAMP*, you should
 *             NOT be casting to FLCN_TIMESTAMP* manually in order to avoid
 *             alignment issues and prevent RISCV issues!
 *
 * @param[out] pTimeData  Pointer to LwU64 to be filled with current
 *                        time in nS.
 */
static inline void osPTimerTimeNsLwrrentGetAsLwU64(LwU64 *pTime)
{
    osPTimerTimeNsLwrrentGet((FLCN_TIMESTAMP *)pTime);
}

#endif // OSPTIMER_H
