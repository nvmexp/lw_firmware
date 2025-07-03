/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    i2cdev.h
 * @copydoc i2cdev.c
 */

#ifndef I2CDEV_H
#define I2CDEV_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "boardobj/boardobjgrp.h"
#include "pmu/pmuifcmn.h"
#include "boardobj/boardobjgrp_iface_model_10.h"
#include "boardobj/boardobj_iface_model_10.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct I2C_DEVICE I2C_DEVICE, I2C_DEVICE_BASE;

/* ------------------------- Macros ---------------------------------------- */
/*!
 *  Macro to locate I2C_DEVICE BOARDOBJGRP.
 *  @copydoc BOARDOBJGRP_OBJ_GET
 */
#define BOARDOBJGRP_DATA_LOCATION_I2C_DEVICE                                  \
    (&(Pmgr.i2cDevices.super))

/*!
 * Retrieve a pointer to a I2C_DEVICE using the I2C Device Table index.
 *
 * @param[in]  deviceIdx    I2C Device Table index
 */
#define I2C_DEVICE_GET(deviceIdx)                                             \
    (BOARDOBJGRP_OBJ_GET(I2C_DEVICE, (deviceIdx)))

/*!
 * Data buffer size for I2C transaction data.
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_ILLUM))
#define I2C_DATA_BUFFER_SIZE_BYTES      24
#else
#define I2C_DATA_BUFFER_SIZE_BYTES       4
#endif

/* ------------------------- Types Definitions ----------------------------- */
struct I2C_DEVICE
{
    BOARDOBJ      super;
    LwU16         i2cAddress; //<! I2C secondary address
    LwU8          i2cPort;    //<! Physical I2C port where the secondary resides

    /*!
     * The RM and PMU must synchronize their I2C accesses using a PMU mutex. If
     * the RM grabs the mutex before the PMU, the PMU must wait/retry. In these
     * cases, the I2C interfaces return pmuI2cErrorBusy instead of waiting.
     * This value specifies the number of times the I2C interfaces will try to
     * grab the PMU I2C mutex.
     */
    LwU8          i2cRetryCount;

    /*!
     * Cached flags value for the internal I2C APIs. See @ref pmuifi2c.h
     * for more information.
     */
    LwU32         i2cFlags;

    /*!
     * I2C devices may store and/or present data in little- or big- endian
     * format. For example, a 16-bit big-endian device is one which begins its
     * transmission with the most-significant byte appearing on the bus first
     * (while little-endian is just the opposite). This variable reflects
     * whether or not the device is big- or little- endian.
     */
    LwBool        bIsBigEndian;

    /*!
     *
     * When this byte is LW_TRUE, any indexed read/write with size > 2 will use
     * the block read/write protocol.  If LW_FALSE or for 1-2 byte transactions,
     * the size byte will be omitted.
     */
    LwBool        bBlockProtocol;
};

/*!
 * Per client buffer shared with the I2C code to communicate read/write values.
 * Safeguards from passing pointers of automatic (stack) variables to I2C task.
 */
typedef struct I2C_CONTEXT
{
    LwU8 data[I2C_DATA_BUFFER_SIZE_BYTES];
} I2C_CONTEXT;

/*!
 * Construct a new I2C device object.
 *
 * @param[in]  ppDev       Pointer to pre-allocated device structure to populate
 * @param[in]  pDesc       @ref RM_PMU_BOARDOBJ pointer
 * @param[in]  i2cAddress  @ref I2C_DEVICE::i2cAddress
 * @param[in]  i2cPort     @ref I2C_DEVICE::i2cPort
 *
 * @return FLCN_OK
 *     Upon successful construction of the I2C device
 */
typedef FLCN_STATUS I2cDevConstruct(BOARDOBJ **ppDev, RM_PMU_BOARDOBJ *pDesc, LwU16 i2cAddress, LwU8 i2cPort, LwU32 i2cFlags);

/*!
 * @brief 8-bit register read
 *
 * @param[in]   pDev    I2C-device object for the I2C device to read
 * @param[in]   index   Index or command-code to use when issuing the command
 * @param[out]  pValue  Written with the 8-bit read-value
 * @param[in]   hQueue
 *     Event-queue used when communicating with the I2C driver/task.
 *
 * @return FLCN_OK
 *     Read completed successfully
 *
 * @return FLCN_ERR_TIMEOUT
 *     If the I2C task did not acknowledge the request before the timeout
 *     interval expired
 *
 * @return other
 *     Returns the error-code given by the I2C task in its acknowlegment to the
 *     I2C request see (taskR_i2c.c::i2c)
 */
#define I2cDevReadReg8(fname) FLCN_STATUS (fname)(I2C_DEVICE *pDev, LwrtosQueueHandle hQueue, LwU32 index, LwU8 *pValue)

/*!
 * @brief 16-bit register read
 *
 * @param[in]   pDev    I2C-device object for the I2C device to read
 * @param[in]   index   Index or command-code to use when issuing the command
 * @param[out]  pValue  Written with the 16-bit read-value
 * @param[in]   hQueue
 *     Event-queue used when communicating with the I2C driver/task.
 *
 * @return FLCN_OK
 *     Read completed successfully
 *
 * @return FLCN_ERR_TIMEOUT
 *     If the I2C task did not acknowledge the request before the timeout
 *     interval expired
 *
 * @return other
 *     Returns the error-code given by the I2C task in its acknowlegment to the
 *     I2C request see (taskR_i2c.c::i2c)
 */
#define I2cDevReadReg16(fname) FLCN_STATUS (fname)(I2C_DEVICE *pDev, LwrtosQueueHandle hQueue, LwU32 index, LwU16 *pValue)

/*!
 * @brief 32-bit register read
 *
 * @param[in]   pDev    I2C-device object for the I2C device to read
 * @param[in]   index   Index or command-code to use when issuing the command
 * @param[out]  pValue  Written with the 32-bit read-value
 * @param[in]   hQueue
 *     Event-queue used when communicating with the I2C driver/task.
 *
 * @return FLCN_OK
 *     Read completed successfully
 *
 * @return FLCN_ERR_TIMEOUT
 *     If the I2C task did not acknowledge the request before the timeout
 *     interval expired
 *
 * @return other
 *     Returns the error-code given by the I2C task in its acknowlegment to the
 *     I2C request see (taskR_i2c.c::i2c)
 */
#define I2cDevReadReg32(fname) FLCN_STATUS (fname)(I2C_DEVICE *pDev, LwrtosQueueHandle hQueue, LwU32 index, LwU32 *pValue)

/*!
 * @brief 8-bit register write
 *
 * @param[in]  pDev   I2C-device object for the I2C device to write
 * @param[in]  index  Index or command-code to use when issuing the command
 * @param[in]  value  8-bit value to write to the device
 * @param[in]  hQueue
 *     Event-queue used when communicating with the I2C driver/task.
 *
 * @return FLCN_OK
 *     Write completed successfully
 *
 * @return FLCN_ERR_TIMEOUT
 *     If the I2C task did not acknowledge the request before the timeout
 *     interval expired
 *
 * @return other
 *     Returns the error-code given by the I2C task in its acknowlegment to the
 *     I2C request see (taskR_i2c.c::i2c)
 */
#define I2cDevWriteReg8(fname) FLCN_STATUS (fname)(I2C_DEVICE *pDev, LwrtosQueueHandle hQueue, LwU32 index, LwU8 value)

/*!
 * @brief 16-bit register write
 *
 * @param[in]  pDev   I2C-device object for the I2C device to write
 * @param[in]  index  Index or command-code to use when issuing the command
 * @param[in]  value  16-bit value to write to the device
 * @param[in]  hQueue
 *     Event-queue used when communicating with the I2C driver/task.
 *
 * @return FLCN_OK
 *     Write completed successfully
 *
 * @return FLCN_ERR_TIMEOUT
 *     If the I2C task did not acknowledge the request before the timeout
 *     interval expired
 *
 * @return other
 *     Returns the error-code given by the I2C task in its acknowlegment to the
 *     I2C request see (taskR_i2c.c::i2c)
 */
#define I2cDevWriteReg16(fname) FLCN_STATUS (fname)(I2C_DEVICE *pDev, LwrtosQueueHandle hQueue, LwU32 index, LwU16 value)

/*!
 * @brief 32-bit register write
 *
 * @param[in]  pDev   I2C-device object for the I2C device to write
 * @param[in]  index  Index or command-code to use when issuing the command
 * @param[in]  value  32-bit value to write to the device
 * @param[in]  hQueue
 *     Event-queue used when communicating with the I2C driver/task.
 *
 * @return FLCN_OK
 *     Write completed successfully
 *
 * @return FLCN_ERR_TIMEOUT
 *     If the I2C task did not acknowledge the request before the timeout
 *     interval expired
 *
 * @return other
 *     Returns the error-code given by the I2C task in its acknowlegment to the
 *     I2C request see (taskR_i2c.c::i2c)
 */
#define I2cDevWriteReg32(fname) FLCN_STATUS (fname)(I2C_DEVICE *pDev, LwrtosQueueHandle hQueue, LwU32 index, LwU32 value)

/* ------------------------- Defines --------------------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
I2cDevReadReg8   (i2cDevReadReg8)
    GCC_ATTRIB_SECTION("imem_libi2c", "i2cDevReadReg8");
I2cDevReadReg16  (i2cDevReadReg16)
    GCC_ATTRIB_SECTION("imem_libi2c", "i2cDevReadReg16");
I2cDevReadReg32  (i2cDevReadReg32)
    GCC_ATTRIB_SECTION("imem_libi2c", "i2cDevReadReg32");
I2cDevWriteReg8  (i2cDevWriteReg8)
    GCC_ATTRIB_SECTION("imem_libi2c", "i2cDevWriteReg8");
I2cDevWriteReg16 (i2cDevWriteReg16)
    GCC_ATTRIB_SECTION("imem_libi2c", "i2cDevWriteReg16");
I2cDevWriteReg32 (i2cDevWriteReg32)
    GCC_ATTRIB_SECTION("imem_libi2c", "i2cDevWriteReg32");

FLCN_STATUS i2cDevRWIndex(I2C_DEVICE *pDev, LwrtosQueueHandle hQueue, LwU32 index, LwU8 *pMessage, LwU8 messageSize, LwBool bRead)
    GCC_ATTRIB_SECTION("imem_libi2c", "i2cDevRWIndex");

/*!
 * Board Object interfaces.
 */
BoardObjGrpIfaceModel10CmdHandler     (i2cDeviceBoardObjGrpIfaceModel10Set)
    GCC_ATTRIB_SECTION("imem_pmgrLibBoardObj", "i2cDeviceBoardObjGrpIfaceModel10Set");
BoardObjGrpIfaceModel10SetEntry (i2cDeviceIfaceModel10SetEntry)
    GCC_ATTRIB_SECTION("imem_pmgrLibConstruct", "i2cDeviceIfaceModel10SetEntry");
BoardObjGrpIfaceModel10ObjSet         (i2cDevGrpIfaceModel10ObjSet_SUPER)
    GCC_ATTRIB_SECTION("imem_pmgrLibConstruct", "i2cDevGrpIfaceModel10ObjSet_SUPER");

/* ------------------------- Child Class Includes -------------------------- */
#include "pmgr/i2cdev_ina219.h"
#include "pmgr/i2cdev_stm32f051k8u6tr.h"
#include "pmgr/i2cdev_stm32l031g6u6tr.h"

#endif // I2CDEV_H
