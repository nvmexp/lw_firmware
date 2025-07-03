/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   i2c_i2cmtxgm20x.c
 * @brief  Contains all I2C Mutex routines specific to GM20X.
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "flcncmn.h"
#ifdef DPU_RTOS
#include "dev_disp_falcon.h"
#endif
#include "lib_intfci2c.h"
#include "lib_mutex.h"
/* ------------------------- Type Definitions ------------------------------- */
#define IS_MUTEX_ACQUIRED()     (LW_U8_MAX != I2cFlcnMutexOwner_GM20X)
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/*!
 * The current mask of physical I2C ports which the falcon ucode owns via the
 * scratch register.
 *
 * This value is cached statically because callers really dont need to know it,
 * they can just call @ref i2cMutexAcquire_HAL() and trust the return value.
 */
static LwU32 I2CacquiredPhysicalMask_GM20X = 0;

/*!
 * The current value of the generated mutex ID with which the FLCN ucode owns the
 * HW mutex.
 *
 * This value is cached statically because callers really don't need to know it,
 * they can just call @ref i2cMutexAcquire_HAL() and trust the return value.
 */
static LwU8  I2cFlcnMutexOwner_GM20X = LW_U8_MAX;

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
static FLCN_STATUS _i2cGetGlobalLockState_GM20X(LwU32 *pMutexRegister)
    GCC_ATTRIB_SECTION("imem_libI2c", "_i2cGetGlobalLockState_GM20X");

static FLCN_STATUS _i2cSetGlobalLockState_GM20X(LwU32 mutexRegister)
    GCC_ATTRIB_SECTION("imem_libI2c", "_i2cSetGlobalLockState_GM20X");

FLCN_STATUS i2cMutexAcquire_GM20X(LwU32 portId, LwBool bAcquire, LwU32 timeoutns)
    GCC_ATTRIB_SECTION("imem_libI2c", "i2cMutexAcquire_GM20X");

/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief I2C Mutex Acquire Mechanism
 *
 * This provides proper synchronization between the VBIOS, RM, FLCN.
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
i2cMutexAcquire_GM20X
(
    LwU32  portId,
    LwBool bAcquire,
    LwU32  timeoutns
)
{
    LwU32           physicalMask  = BIT(portId);
    LwU32           mutexRegister = 0;
    FLCN_STATUS     status        = FLCN_OK;

    // Sanity-check the input request.
    if (bAcquire)
    {
        // Make sure that the per-port mutex is not already acquired!
        if ((physicalMask & I2CacquiredPhysicalMask_GM20X) != 0)
        {
            return FLCN_ERR_ILWALID_STATE;
        }
    }
    else
    {
        // Make sure that the per-port mutex is acquired!
        if ((physicalMask & I2CacquiredPhysicalMask_GM20X) != physicalMask)
        {
            return FLCN_ERR_ILWALID_STATE;
        }
    }

    //
    // Acquire the FLCN I2C mutex which
    // is used to synchronize accesses to the per-port mutex register.
    //
    if (IS_MUTEX_ACQUIRED())
    {
        return FLCN_ERR_ILWALID_STATE;
    }

    status = mutexAcquire(LW_MUTEX_ID_I2C, timeoutns, &I2cFlcnMutexOwner_GM20X);
    if (FLCN_OK != status)
    {
        return status;
    }

    //
    // Global/legacy FLCN I2C mutex is acquired successfully, now look at
    // per-port mutex register.
    //
    status = _i2cGetGlobalLockState_GM20X(&mutexRegister);
    if (FLCN_OK != status)
    {
        goto i2cMutexAcquire_GM20X_exit;
    }

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
            status = _i2cSetGlobalLockState_GM20X(mutexRegister);
            if (FLCN_OK != status)
            {
                goto i2cMutexAcquire_GM20X_exit;
            }

            //
            // 2. Update internal tracking state now that falcon actually owns
            // those ports.
            //
            I2CacquiredPhysicalMask_GM20X |= physicalMask;
        }
    }
    else
    {
        // Opposite order as acquire.

        //
        // 1. Update internal tracking state now that falcon will no longer own
        // those ports.
        //
        I2CacquiredPhysicalMask_GM20X &= ~physicalMask;

        // 2. Clear the per-port bits as released.
        mutexRegister &= ~physicalMask;
        status = _i2cSetGlobalLockState_GM20X(mutexRegister);
        if (FLCN_OK != status)
        {
            goto i2cMutexAcquire_GM20X_exit;
        }
    }

i2cMutexAcquire_GM20X_exit:

    // Bail out if we failed to acquire the mutex.
    if (!IS_MUTEX_ACQUIRED())
    {
        return FLCN_ERR_ILWALID_STATE;
    }

    // Release the FLCN HW mutex.
    status = mutexRelease(LW_MUTEX_ID_I2C, I2cFlcnMutexOwner_GM20X);
    if (FLCN_OK == status)
    {
        I2cFlcnMutexOwner_GM20X = LW_U8_MAX;
    }

    return status;
}

/*!
 * @brief Get internal saved per-port mutex mask value.
 *
 * @param[in]   mutexRegister   pointer to per-port mutex mask value
 *
 * @return FLCN_OK
 *     Upon successful to get saved mask value.
 */
static FLCN_STATUS
_i2cGetGlobalLockState_GM20X(LwU32 *pMutexRegister)
{
#ifdef DPU_RTOS
    *pMutexRegister = DISP_REG_RD32(LW_PDISP_FALCON_MAILBOX_REG(15));
    return FLCN_OK;
#else
    return FLCN_ERR_NOT_SUPPORTED;
#endif
}

/*!
 * @brief Set internal saved per-port mutex mask value.
 *
 * @param[in]   mutexRegister   per-port mutex mask value
 *
 * @return FLCN_OK
 *     Upon successful to save mask value.
 */
static FLCN_STATUS
_i2cSetGlobalLockState_GM20X(LwU32 mutexRegister)
{
#ifdef DPU_RTOS
    DISP_REG_WR32(LW_PDISP_FALCON_MAILBOX_REG(15), mutexRegister);
    return FLCN_OK;
#else
    return FLCN_ERR_NOT_SUPPORTED;
#endif
}
