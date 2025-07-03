/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_i2cmtxgp10x.c
 * @brief  Contains all I2C Mutex routines specific to GP10X and later.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dbgprintf.h"

#include "dev_pwr_csb.h"

/* ------------------------- Application Includes --------------------------- */
#include "lib_mutex.h"
#include "pmu_obji2c.h"
#include "pmu_objtimer.h"
#include "config/g_i2c_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
ct_assert(MAX_I2C_PORTS == 16);

/*!
 * The current value of the generated mutex ID with which the PMGR ucode owns 
 * the HW mutex.
 *
 * These values are cached statically because callers really don't need to know
 * it, they can just call @ref i2cMutexAcquire_HAL() and trust the return value.
 */
static LwU8 I2CmutexId_GP10X[MAX_I2C_PORTS] =
{
    LW_U8_MAX,
    LW_U8_MAX,
    LW_U8_MAX,
    LW_U8_MAX,
    LW_U8_MAX,
    LW_U8_MAX,
    LW_U8_MAX,
    LW_U8_MAX,
    LW_U8_MAX,
    LW_U8_MAX,
    LW_U8_MAX,
    LW_U8_MAX,
    LW_U8_MAX,
    LW_U8_MAX,
    LW_U8_MAX,
    LW_U8_MAX,
};

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief I2C Mutex Acquire Mechanism
 *
 * This provides proper synchronization between the RM, VBIOS, and the PMU.
 *
 * @param[in]   port      I2C Port
 * @param[in]   bAcquire
 *     Boolean flag indicating whether the mutex should be acquired or released.
 * @param[in]   timeoutns
 *     Duration for which interface should retry attempting to acquire the lock.
 *     Specified in units of ns.  Zero (0) indicates that the interface only try
 *     acquire once and then return immediately.
 *
 * @return FLCN_OK
 *     Upon successful acquisition of the I2C PMGR mutex.
 *
 * @return FLCN_ERR_ILWALID_STATE
 *     Attempted to acquire mutex when already acquired, or release when not
 *     acquired.
 *
 * @return FLCN_ERR_MUTEX_ACQUIRED
 *     If the I2C mutex used to synchronize I2C access between the RM and PMU
 *     could not be acquired (indicates the bus is 'Busy' and the transaction
 *     may be retried at a later time).
 */
FLCN_STATUS
i2cMutexAcquire_GP10X
(
    LwU32  port,
    LwBool bAcquire,
    LwU32  timeoutns
)
{
    FLCN_STATUS     status = FLCN_ERR_MORE_PROCESSING_REQUIRED;
    FLCN_TIMESTAMP  time;
    LwU8            mutexIndex;

    // If timeout specified, acquire the timestamp for the start.
    if (timeoutns != 0)
    {
        osPTimerTimeNsLwrrentGet(&time);
    }

    // Get the logical mutex index for the corresponding port.
    mutexIndex = LW_MUTEX_ID_I2C_0 + port;

    //
    // Loop until have successfully acquired/released the mutex or until the
    // timeout expires.
    //
    while (FLCN_ERR_MORE_PROCESSING_REQUIRED == status)
    {
        if (bAcquire)
        {
            // Make sure that the mutex is not already acquired.
            if (LW_U8_MAX != I2CmutexId_GP10X[port])
            {
                PMU_BREAKPOINT();
                return FLCN_ERR_ILWALID_STATE;
            }

            status = mutexAcquire(mutexIndex, timeoutns, &I2CmutexId_GP10X[port]);
        }
        else
        {
            // Make sure that mutex is acquired.
            if (LW_U8_MAX == I2CmutexId_GP10X[port])
            {
                PMU_BREAKPOINT();
                return FLCN_ERR_ILWALID_STATE;
            }

            // Release the PMGR HW mutex.
            status = mutexRelease(mutexIndex, I2CmutexId_GP10X[port]);
            if (status == FLCN_OK)
            {
                I2CmutexId_GP10X[port] = LW_U8_MAX;
            }
        }

        //
        // If not yet acquired/released, check for timeout and fail out on
        // error.
        //
        if ((FLCN_ERR_MORE_PROCESSING_REQUIRED == status) &&
            ((timeoutns == 0) ||
             (timeoutns < osPTimerTimeNsElapsedGet(&time))))
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_TIMEOUT;
        }
    }

    return status;
}
