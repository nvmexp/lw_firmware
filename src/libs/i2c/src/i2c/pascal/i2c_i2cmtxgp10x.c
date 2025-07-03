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
 * @file   i2c_i2cmtxgp10x.c
 * @brief  Contains all I2C Mutex routines specific to GP10X and later.
 */

/* ------------------------- System Includes -------------------------------- */
#include "dev_pmgr.h"
#include "lwctassert.h"
/* ------------------------- Application Includes --------------------------- */
#include "lib_intfci2c.h"
#include "lib_mutex.h"
/* ------------------------- Type Definitions ------------------------------- */
#define IS_MUTEX_ACQUIRED(port)     (LW_U8_MAX != I2cMutexOwner_GP10X[(portId)])
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
ct_assert(MAX_I2C_PORTS == 16);
/*!
 * The current value of the generated mutex ID with which the FLCN ucode owns the
 * HW mutex.
 *
 * These values is cached statically because callers really don't need to know
 * it, they can just call @ref i2cMutexAcquire_HAL() and trust the return value.
 */
static LwU8  I2cMutexOwner_GP10X[MAX_I2C_PORTS] =
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
FLCN_STATUS i2cMutexAcquire_GP10X(LwU32 portId, LwBool bAcquire, LwU32 timeoutns)
    GCC_ATTRIB_SECTION("imem_libI2c", "i2cMutexAcquire_GP10X");

/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief I2C Mutex Acquire Mechanism
 *
 * This provides proper synchronization between the VBIOS, RM, and FLCN.
 *
 * @note This interface cannot be called when FLCN already owns the mutex.
 * Otherwise, undefined behavior will result.
 *
 * @param[in]   portId    I2C Port
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
 *     If the I2C mutex used to synchronize I2C access between the RM and FLCN
 *     could not be acquired (indicates the bus is 'Busy' and the transaction
 *     may be retried at a later time).
 */
FLCN_STATUS
i2cMutexAcquire_GP10X
(
    LwU32  portId,
    LwBool bAcquire,
    LwU32  timeoutns
)
{
    FLCN_STATUS    status        = FLCN_OK;

    if (bAcquire)
    {
        // Bail out if mutex already acquired.
        if (IS_MUTEX_ACQUIRED(portId))
        {
            return FLCN_ERR_ILWALID_STATE;
        }

        // Acquire PMGR HW mutex.
        status = mutexAcquire(portId + LW_MUTEX_ID_I2C_0, timeoutns,
                              &I2cMutexOwner_GP10X[portId]);
    }
    else
    {
        // Bail out if we failed to acquire the mutex.
        if (!IS_MUTEX_ACQUIRED(portId))
        {
            return FLCN_ERR_ILWALID_STATE;
        }

        // Release the PMGR HW mutex.
        status = mutexRelease(portId + LW_MUTEX_ID_I2C_0, I2cMutexOwner_GP10X[portId]);
        if (FLCN_OK == status)
        {
            I2cMutexOwner_GP10X[portId] = LW_U8_MAX;
        }
    }

    return status;
}
