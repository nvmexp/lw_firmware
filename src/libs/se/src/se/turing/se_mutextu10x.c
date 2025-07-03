/*
 * Copyright 2016-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   se_mutexgp10x.c
 * @brief  SE MUTEX HAL Functions for GP10X
 *
 * SE HAL functions implement mutex and initialization functions.
 */

#include "se_objse.h"
#include "secbus_ls.h"
#include "secbus_hs.h"
#include "osptimer.h"
#include "config/g_se_hal.h"
#include "dev_se_seb.h"
#include "se_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Implementation --------------------------------- */

/*!
 * @brief Tries to acquire the SE mutex. If successful, SE_OK is returned.
 *
 * @return SE_OK if successful. SE_ERR_MUTEX_ACQUIRE if unsuccessful.
 */
SE_STATUS seMutexAcquire_TU10X(void)
{
    SE_STATUS status = SE_OK;
    LwU32     val    = 0;
    FLCN_TIMESTAMP     timeStart;

    //
    // Check default timeout register to verify it has not been tampered with
    // before starting mutex acquire sequence
    //
    CHECK_SE_STATUS(seSelwreBusReadRegisterErrChk(LW_SSE_SE_CTRL_SE_MUTEX_TMOUT_DFTVAL, &val));
    if (val != LW_SSE_SE_CTRL_SE_MUTEX_TMOUT_DFTVAL_TMOUT_MAX)
    {
        // If this register has been tampered with,  HALT
        lwuc_halt();
    }

    //
    // TODO PJAMBHLEKAR: rewire seMutexAcquire_TU10X to pascal HAL and stubbed out these calls
    // From Pascal for Turing chips
    // Initialize default timeout register
    //CHECK_SE_STATUS(seSelwreBusWriteRegisterErrChk(LW_SSE_SE_CTRL_SE_MUTEX_TMOUT_DFTVAL,
    //                                               LW_SSE_SE_CTRL_SE_MUTEX_TMOUT_DFTVAL_TMOUT_MAX));
    //
    // Acquire the mutex.  Timeout if acquire takes too long.
    // TODO:  Pass a timeout value
    //
    // Initialize timer with current time
    //
    osPTimerTimeNsLwrrentGet(&timeStart);
    do
    {
        CHECK_SE_STATUS(seSelwreBusReadRegisterErrChk(LW_SSE_SE_CTRL_PKA_MUTEX, &val));
        val = DRF_VAL(_SSE, _SE_CTRL_PKA_MUTEX, _RC_VAL, val);
    } while ((val != LW_SSE_SE_CTRL_PKA_MUTEX_RC_SUCCESS) &&
             (SE_MUTEX_TIMEOUT_VAL_USEC > osPTimerTimeNsElapsedGet(&timeStart)));

    // Check to see if mutex was acquired in case timeout oclwrred .
    if (val != LW_SSE_SE_CTRL_PKA_MUTEX_RC_SUCCESS)
    {
        return SE_ERR_MUTEX_ACQUIRE;
    }

    //
    // Disable the watchdog register
    // Watchdog should be set after the mutex is acquired.
    // IAS section 6.1.2.3  - https://p4viewer/get/p4hw:2001//hw/ar/doc/t18x/se/SE_PKA_IAS.doc
    //
    //CHECK_SE_STATUS(seMutexSetTimeoutValue_HAL(&Se, LW_SSE_SE_CTRL_PKA_MUTEX_WDTMR_VAL_DISABLE));

    // set PKA mutex timeout action to be release mutex & reset PKA core
    CHECK_SE_STATUS(seSelwreBusWriteRegisterErrChk(LW_SSE_SE_CTRL_PKA_MUTEX_TMOUT_ACTION,
                                                   DRF_DEF(_SSE, _SE_CTRL_PKA_MUTEX_TMOUT_ACTION, _INTERRUPT,     _ENABLE) |
                                                   DRF_DEF(_SSE, _SE_CTRL_PKA_MUTEX_TMOUT_ACTION, _RELEASE_MUTEX, _ENABLE) |
                                                   DRF_DEF(_SSE, _SE_CTRL_PKA_MUTEX_TMOUT_ACTION, _RESET_PKA,     _ENABLE)));

    // Make sure we own the mutex
    CHECK_SE_STATUS(seSelwreBusReadRegisterErrChk(LW_SSE_SE_CTRL_PKA_MUTEX_STATUS, &val));
    val = DRF_VAL(_SSE, _SE_CTRL_PKA_MUTEX_STATUS, _LOCKED, val);

    // No one owns the mutex, return error
    if (val == LW_SSE_SE_CTRL_PKA_MUTEX_STATUS_LOCKED_FALSE)
    {
        return SE_ERR_MUTEX_ACQUIRE;
    }

    //
    // Mutex is owned by a Falcon. Check the lock value to be sure the expected Falcon
    // owns the lock.  The lock value depends on which falcon is using SELib.
    //
#ifdef SEC2_RTOS
    if (val != LW_SSE_SE_CTRL_PKA_MUTEX_STATUS_LOCKED_SEC)
    {
        return SE_ERR_MUTEX_ACQUIRE;
    }
#endif

#ifdef PMU_RTOS
    if (val != LW_SSE_SE_CTRL_PKA_MUTEX_STATUS_LOCKED_PMU)
    {
        return SE_ERR_MUTEX_ACQUIRE;
    }
#endif

#ifdef DPU_RTOS
    if (val != LW_SSE_SE_CTRL_PKA_MUTEX_STATUS_LOCKED_DPU)
    {
        return SE_ERR_MUTEX_ACQUIRE;
    }
#endif

#ifdef LWDEC_RTOS
    if (val != LW_SSE_SE_CTRL_PKA_MUTEX_STATUS_LOCKED_LWDEC)
    {
        return SE_ERR_MUTEX_ACQUIRE;
    }
#endif

#ifdef GSPLITE_RTOS
    if (val != LW_SSE_SE_CTRL_PKA_MUTEX_STATUS_LOCKED_DPU)
    {
        return SE_ERR_MUTEX_ACQUIRE;
    }
#endif

    // Make sure the watchdog timer is still disabled
    // CHECK_SE_STATUS(seSelwreBusReadRegisterErrChk(LW_SSE_SE_CTRL_PKA_MUTEX_WDTMR, &val));
    // if (val != LW_SSE_SE_CTRL_PKA_MUTEX_WDTMR_VAL_DISABLE)
    // {
    //     return SE_ERR_MUTEX_ACQUIRE;
    // }

ErrorExit:
    return status;
}
