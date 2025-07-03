/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2012-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_i2cmtxgk11x.c
 * @brief  Contains all I2C Mutex routines specific to GK11X and later.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dbgprintf.h"

#include "dev_pwr_csb.h"
#include "pmu/pmumailboxid.h" 

/* ------------------------- Application Includes --------------------------- */
#include "pmu_obji2c.h"
#include "pmu_objtimer.h"
#include "lib_mutex.h"
#include "config/g_i2c_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/*!
 * The current value of the generated mutex ID with which the PMU ucode owns the
 * HW mutex.
 *
 * This value is cached statically because callers really don't need to know it,
 * they can just call @ref i2cMutexAcquire_HAL() and trust the return value.
 */
static LwU8 I2CmutexId_GMXXX = LW_U8_MAX;

/*!
 * The current mask of physical I2C ports which the PMU ucode owns via the
 * per-port mutex scratch register: LW_PMU_PPWR_MAILBOX(1).
 *
 * This value is cached statically because callers really dont need to know it,
 * they can just call @ref i2cMutexAcquire_HAL() and trust the return value.
 */
static LwU32 I2CacquiredPhysicalMask_GMXXX = 0;

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief   Accessor of per-port I2C mutex state register.
 *
 * @return  Current state of per-port I2C mutex mask.
 */
LwU32
i2cMutexMaskRd32_GM10X(void)
{
    return REG_RD32(CSB, LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_I2C_MUTEX));
}

/*!
 * @brief   Mutator of per-port I2C mutex state register.
 *
 * @param[in]    New state of per-port I2C mutex mask.
 */
void
i2cMutexMaskWr32_GM10X(LwU32 mutexMask)
{
    REG_WR32(CSB, LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_I2C_MUTEX), mutexMask);
}

/*!
 * @brief I2C Mutex Acquire Mechanism
 *
 * This provides proper synchronization between the RM, VBIOS, and the PMU.
 *
 * @note This interface cannot be called when PMU already owns the mutex.
 * Otherwise, undefined behavior will result.
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
 *     Upon successful acquistion of the I2C Port mutex.
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
i2cMutexAcquire_GMXXX
(
    LwU32  port,
    LwBool bAcquire,
    LwU32  timeoutns
)
{
    LwU32           physicalMask  = BIT(port);
    LwU32           mutexRegister = 0;
    FLCN_TIMESTAMP  time;
    FLCN_STATUS     status        = FLCN_ERR_MORE_PROCESSING_REQUIRED;
    FLCN_STATUS     mutexStatus;

    // Sanity-check the input request.
    if (bAcquire)
    {
        // Make sure that the per-port mutex is not already acquired!
        if ((physicalMask & I2CacquiredPhysicalMask_GMXXX) != 0)
        {
            PMU_BREAKPOINT();
            return FLCN_ERR_ILWALID_STATE;
        }
    }
    else
    {
        // Make sure that the per-port mutex is acquired!
        if ((physicalMask & I2CacquiredPhysicalMask_GMXXX) != physicalMask)
        {
            PMU_BREAKPOINT();
            return FLCN_ERR_ILWALID_STATE;
        }
    }

    // If timeout specified, acquire the timestamp for the start.
    if (timeoutns != 0)
    {
        osPTimerTimeNsLwrrentGet(&time);
    }

    //
    // Loop until have successfully acquired/released the mutex or until the
    // timeout expires.
    //
    while (FLCN_ERR_MORE_PROCESSING_REQUIRED == status)
    {
        // Make sure that the mutex is not already acquired.
        if (LW_U8_MAX != I2CmutexId_GMXXX)
        {
            mutexStatus = FLCN_ERR_ILWALID_STATE;
        }
        else
        {
            // Grab the PMU HW mutex.
            mutexStatus = mutexAcquire(LW_MUTEX_ID_I2C, timeoutns, &I2CmutexId_GMXXX);
        }
        if (mutexStatus != FLCN_OK)
        {
            status = mutexStatus;
            PMU_BREAKPOINT();
            break;
        }

        //
        // Global/legacy PMU I2C mutex is acquired successfully, now look at
        // per-port mutex register.
        //
        mutexRegister = i2cMutexMaskRd32_HAL(&I2c);
        if (bAcquire)
        {
            //
            // Check that no agent has acquired any of the ports in which the RM
            // is interested.
            //
            if ((mutexRegister & physicalMask) == 0)
            {
                // Opposite order as release.

                // 1. Set the per-port bits as acquired.
                mutexRegister |= physicalMask;
                i2cMutexMaskWr32_HAL(&I2c, mutexRegister);

                //
                // 2. Update internal RM tracking state now that RM actually owns
                // those ports.
                //
                I2CacquiredPhysicalMask_GMXXX |= physicalMask;

                //
                // Mark status as FLCN_OK, as acquisition successful and can fall
                // out of loop!
                //
                status = FLCN_OK;
            }
        }
        else
        {
            // Opposite order as acquire.

            //
            // 1. Update internal RM tracking state now that RM will no longer own
            // those ports.
            //
            I2CacquiredPhysicalMask_GMXXX &= ~physicalMask;

            // 2. Clear the per-port bits as released.
            mutexRegister &= ~physicalMask;
            i2cMutexMaskWr32_HAL(&I2c, mutexRegister);

            //
            // Mark status as FLCN_OK, as acquisition successful and can fall
            // out of loop!
            //
            status = FLCN_OK;
        }


        // Make sure that the mutex is acquired.
        if (LW_U8_MAX == I2CmutexId_GMXXX)
        {
            mutexStatus = FLCN_ERR_ILWALID_STATE;
        }
        else
        {
            // Relase the PMU HW mutex.
            mutexStatus = mutexRelease(LW_MUTEX_ID_I2C, I2CmutexId_GMXXX);
            if (mutexStatus == FLCN_OK)
            {
                I2CmutexId_GMXXX = LW_U8_MAX;
            }
        }
        if (mutexStatus != FLCN_OK)
        {
            PMU_BREAKPOINT();
            //
            // Only return this error if no other errors have been reported so
            // far.  Other errors above are far more useful/interesting than
            // this failure.
            //
            if ((FLCN_ERR_MORE_PROCESSING_REQUIRED == status) ||
                (status == FLCN_OK))
            {
                status = mutexStatus;
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
