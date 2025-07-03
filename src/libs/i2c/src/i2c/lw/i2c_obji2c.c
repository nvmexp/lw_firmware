/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   i2c_obji2c.c
 * @brief  Container-object for FLCN i2c routines. Contains generic non-HAL
 *         interrupt-routines plus logic required to hook-up chip-specific
 *         interrupt HAL-routines.
 */

/* ------------------------ System includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "i2c_obji2c.h"
#include "lib_intfci2c.h"
#include "config/g_i2c_private.h"
#include "config/i2c-config.h"
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
#if defined(GSPLITE_RTOS) || defined(GSP_RTOS)
#define I2C_ATTACH_AND_LOAD_IMEM_OVERLAY(ovlIdxImem)
#define I2C_DETACH_IMEM_OVERLAY(ovlIdxImem)
#else
#define I2C_ATTACH_AND_LOAD_IMEM_OVERLAY(ovlIdxImem)    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(ovlIdxImem)
#define I2C_DETACH_IMEM_OVERLAY(ovlIdxImem)             OSTASK_DETACH_IMEM_OVERLAY(ovlIdxImem)
#endif

/* ------------------------- Private Function Definitions ------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief  Dispatch a request to perform an I2C transaction to the hardware
 *         i2c implementation.
 *
 * @param[in,out]  pTa           The transaction information.
 *
 * @return The value returned by the selected implementation.
 *
 * @return 'FLCN_ERR_I2C_BUSY'
 *     If the I2C mutex used to synchronize I2C access between the RM,
 *     FLCN could not be acquired (indicates the bus is 'Busy' and the transaction
 *     may be retried at a later time).
 */
FLCN_STATUS
i2cPerformTransaction
(
    I2C_TRANSACTION *pTa
)
{
    FLCN_STATUS status      = FLCN_ERROR;
    FLCN_STATUS mutexStatus = FLCN_ERROR;
    LwU32       portId      = 0;
    LwBool      bWasBb      = LW_TRUE;
#ifndef HDCP_TEGRA_BUILD
    portId      = DRF_VAL(_RM_FLCN, _I2C_FLAGS, _PORT, pTa->flags);
    LwBool      bSwImpl     = LW_FALSE;
    LwBool      bIsBusReady = LW_FALSE;
#endif
#ifdef COMPILE_SWI2C
    bSwImpl = libIntfcI2lwseSwI2c();
#endif

    mutexStatus = i2cMutexAcquire_HAL(&I2c,
                                      portId,                        // portId
                                      LW_TRUE,                       // bAcquire
                                      I2C_MUTEX_ACQUIRE_TIMEOUT_NS); // timeoutns


    if (mutexStatus != FLCN_OK)
    {
        // Failed to acquire i2c mutex
        return FLCN_ERR_I2C_BUSY;
    }

    //
    // When just starting an i2c transaction and just after successfully
    // acquiring the I2C mutex, it is never expected that the bus will be
    // busy. If it is busy, consider it as being in an invalid state and
    // attempt to recover it.
    //
#ifndef HDCP_TEGRA_BUILD

    status = i2cIsBusReady_HAL(&I2c, portId, &bIsBusReady);
    if (status != FLCN_OK)
    {
        goto i2cPerformTransaction_done;
    }

    if (!bIsBusReady)
    {
#ifdef COMPILE_SWI2C
        // Bus recovery feature is enabled for v0207
        if (bSwImpl)
        {
            // set the i2c mode to bit-banging mode (should never fail!)
            I2C_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(libI2cHw));
            status = i2cSetMode_HAL(&I2c, portId, LW_TRUE, &bWasBb);
            OS_ASSERT(status == FLCN_OK);
            I2C_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(libI2cHw));

            // Now attempt to recover the buss
            I2C_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(libI2cSw));
            status = i2cRecoverBusViaSw(pTa);
            OS_ASSERT(status == FLCN_OK);
            I2C_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(libI2cSw));

            // TODO: add check and test CTS with Pascal.
            i2cRestoreMode_HAL(&I2c, portId, bWasBb);

            if (status != FLCN_OK)
            {
                goto i2cPerformTransaction_done;
            }
        }
#else
        status = FLCN_ERR_I2C_BUSY;
        goto i2cPerformTransaction_done;
#endif
    }

    //
    // With the bus ready, set the desired operating mode and perform the
    // transaction.
    //
    I2C_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(libI2cHw));
    status = i2cSetMode_HAL(&I2c, portId, bSwImpl, &bWasBb);
    I2C_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(libI2cHw));
#else
    status = mutexStatus;
#endif
    if (status == FLCN_OK)
    {
#ifdef COMPILE_SWI2C
        if (bSwImpl)
        {
            I2C_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(libI2cSw));
            status = i2cPerformTransactiolwiaSw(pTa);
            I2C_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(libI2cSw));
        }
        else
#endif

#ifndef HDCP_TEGRA_BUILD
        {
            I2C_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(libI2cHw));
            status = i2cPerformTransactiolwiaHw(pTa);
            I2C_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(libI2cHw));
        }
#else
        {
           status = i2cPerformTransactiolwiaTegraHw(pTa);
        }
#endif

        if (status != FLCN_ERR_I2C_NACK_BYTE)
        {
            // No need to restore mode when HW issue automatic Stop.
            status = i2cRestoreMode_HAL(&I2c, portId, bWasBb);
        }
    }
#ifndef HDCP_TEGRA_BUILD
i2cPerformTransaction_done:
#endif
    mutexStatus = i2cMutexAcquire_HAL(&I2c,
                                      portId,                        // portId
                                      LW_FALSE,                      // bAcquire
                                      I2C_MUTEX_ACQUIRE_TIMEOUT_NS); // timeoutns

    //
    // Don't override original error and return mutex release error if hit mutex
    // problem with success transaction.
    //
    return (status==FLCN_OK) ? mutexStatus : status;
}

