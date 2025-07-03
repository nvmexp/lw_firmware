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
 * @file   i2cdev.c
 * @brief  Represents a physical I2C device on the board.
 *
 * This module views a physical I2C device as any I2C device with a unique
 * secondary port/address pair. Note that some devices respond to more than one
 * secondary address. Such devices may be represented logically using multiple
 * objects of this type (one instance per address).
 *
 * @extends BOARDOBJ
 */

/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "dbgprintf.h"
#include "pmu_objpmgr.h"
#include "pmu_i2capi.h"
#include "pmgr/i2cdev.h"
#include "flcnifi2c.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "rmpmusupersurfif.h"

/* ------------------------- Type Definitions ------------------------------- */
/*!
 * Enumeration of all clients using i2cDev[Read|Write]Reg[8|16|32] interfaces.
 */
typedef enum
{
    I2C_CONTEXT_ID_PMGR = 0,
    I2C_CONTEXT_ID_THERM,
    I2C_CONTEXT_ID_PERF,
#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_MULT_DATA_PWR_CHANNEL))
    I2C_CONTEXT_ID_PERF_CF,
#endif
#if (PMUCFG_FEATURE_ENABLED(PMU_PSI_I2C_INTERFACE))
    I2C_CONTEXT_ID_LPWR,
#endif
    I2C_CONTEXT__COUNT,
} I2C_CONTEXT_ID;

/* ------------------------- Macros and Defines ----------------------------- */

/*!
 * Default I2C retry count. See @ref I2C_DEVICE::i2cRetryCount.
 */
#define  LW_PMU_I2CDEV_RETRY_COUNT                                           255

/* ------------------------- Global variables ------------------------------- */
I2C_CONTEXT   I2cContext[I2C_CONTEXT__COUNT];

/* ------------------------- Prototypes ------------------------------------- */
static I2C_CONTEXT *s_i2cContextLookup(void)
    GCC_ATTRIB_SECTION("imem_libi2c", "s_i2cContextLookup");

/* ------------------------- Public Functions ------------------------------- */

/*!
 * I2C_DEVICE handler for the RM_PMU_BOARDOBJ_CMD_GRP_SET interface.
 *
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
i2cDeviceBoardObjGrpIfaceModel10Set
(
   PMU_DMEM_BUFFER *pBuffer
)
{
    return BOARDOBJGRP_IFACE_MODEL_10_SET_AUTO_DMA(E32,      // _grpType
        I2C_DEVICE,                                 // _class
        pBuffer,                                    // _pBuffer
        boardObjGrpIfaceModel10SetHeader,                 // _hdrFunc
        i2cDeviceIfaceModel10SetEntry,                    // _entryFunc
        all.pmgr.i2cDeviceGrpSet,                   // _ssElement
        OVL_INDEX_DMEM(i2cDevice),                  // _ovlIdxDmem
        BoardObjGrpVirtualTablesNotImplemented);    // _ppObjectVtables
}

/*!
 * @copydoc BoardObjGrpIfaceModel10SetEntry
 */
FLCN_STATUS
i2cDeviceIfaceModel10SetEntry
(
    BOARDOBJGRP_IFACE_MODEL_10     *pModel10,
    BOARDOBJ       **ppBoardObj,
    RM_PMU_BOARDOBJ *pBuf
)
{
    switch (pBuf->type)
    {
       case RM_PMU_I2C_DEVICE_TYPE_POWER_SENSOR_INA209:
       case RM_PMU_I2C_DEVICE_TYPE_POWER_SENSOR_INA219:
       case RM_PMU_I2C_DEVICE_TYPE_POWER_SENSOR_INA3221:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_I2C_DEVICE_INA219);
            return i2cDevGrpIfaceModel10ObjSet_INA219(pModel10, ppBoardObj,
                sizeof(I2C_DEVICE), pBuf);

        case RM_PMU_I2C_DEVICE_TYPE_STM32F051K8U6TR:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_I2C_DEVICE_STM32F051K8U6TR);
            return i2cDevGrpIfaceModel10ObjSet_STM32F051K8U6TR(pModel10, ppBoardObj,
                sizeof(I2C_DEVICE), pBuf);

        case RM_PMU_I2C_DEVICE_TYPE_STM32L031G6U6TR:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_I2C_DEVICE_STM32L031G6U6TR);
            return i2cDevGrpIfaceModel10ObjSet_STM32L031G6U6TR(pModel10, ppBoardObj,
                sizeof(I2C_DEVICE), pBuf);
    }

    //
    // Reaching this point indicates that the device does not require a
    // special constructor.  Use the default.
    //
    return i2cDevGrpIfaceModel10ObjSet_SUPER(pModel10, ppBoardObj, sizeof(I2C_DEVICE), pBuf);
}

/*!
 * Super-class implementation.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
i2cDevGrpIfaceModel10ObjSet_SUPER
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_PMGR_I2C_DEVICE_BOARDOBJ_SET *pSet =
        (RM_PMU_PMGR_I2C_DEVICE_BOARDOBJ_SET *)pBoardObjDesc;
    I2C_DEVICE                          *pI2cDev;
    FLCN_STATUS                          status;

    status = boardObjGrpIfaceModel10ObjSet(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        return status;
    }

    pI2cDev = (I2C_DEVICE *)*ppBoardObj;

    // Set member variables.
    pI2cDev->i2cAddress      = pSet->i2cAddress;
    pI2cDev->i2cPort         = pSet->i2cPort;
    pI2cDev->i2cFlags        = pSet->i2cFlags;
    pI2cDev->i2cRetryCount   = LW_PMU_I2CDEV_RETRY_COUNT;

    return status;
}

/*!
 * @copydoc I2cDevReadReg8
 *
 * Implemented using the SMBus Read-Byte Protocol (5.5.5).
 */
FLCN_STATUS
i2cDevReadReg8
(
    I2C_DEVICE         *pDev,
    LwrtosQueueHandle   hQueue,
    LwU32               index,
    LwU8               *pValue
)
{
    return i2cDevRWIndex(pDev, hQueue, index, pValue, 1, LW_TRUE);
}

/*!
 * @copydoc I2cDevReadReg16
 *
 * Implemented using the SMBus Read-Word Protocol (5.5.5).
 */
FLCN_STATUS
i2cDevReadReg16
(
    I2C_DEVICE         *pDev,
    LwrtosQueueHandle   hQueue,
    LwU32               index,
    LwU16              *pValue
)
{
    FLCN_STATUS status;
    status = i2cDevRWIndex(pDev, hQueue, index, (LwU8 *)pValue, 2, LW_TRUE);
    if ((status == FLCN_OK) && (pDev->bIsBigEndian))
    {
        SWAP(LwU8, ((LwU8*)pValue)[0], ((LwU8*)pValue)[1]);
    }
    return status;
}

/*!
 * @copydoc I2cDevReadReg32
 *
 * Implemented using the SMBus Block Read Protocol (5.5.7) or
 * another non-spec transaction type which doesn't include the size first.
 */
FLCN_STATUS
i2cDevReadReg32
(
    I2C_DEVICE         *pDev,
    LwrtosQueueHandle   hQueue,
    LwU32               index,
    LwU32              *pValue
)
{
    FLCN_STATUS status;

    status = i2cDevRWIndex(pDev, hQueue, index, (LwU8 *)pValue, 4, LW_TRUE);
    if ((status == FLCN_OK) && (pDev->bIsBigEndian))
    {
        SWAP(LwU8, ((LwU8 *)pValue)[0], ((LwU8 *)pValue)[3]);
        SWAP(LwU8, ((LwU8 *)pValue)[1], ((LwU8 *)pValue)[2]);
    }
    return status;
}

/*!
 * @copydoc I2cDevWriteReg8
 *
 * Implemented using the SMBus Write-Byte Protocol (5.5.4).
 */
FLCN_STATUS
i2cDevWriteReg8
(
    I2C_DEVICE         *pDev,
    LwrtosQueueHandle   hQueue,
    LwU32               index,
    LwU8                value
)
{
    return i2cDevRWIndex(pDev, hQueue, index, (LwU8 *)&value, 1, LW_FALSE);
}

/*!
 * @copydoc I2cDevWriteReg16
 *
 * Implemented using the SMBus Write-Word Protocol (5.5.4).
 */
FLCN_STATUS
i2cDevWriteReg16
(
    I2C_DEVICE         *pDev,
    LwrtosQueueHandle   hQueue,
    LwU32               index,
    LwU16               value
)
{
    if (pDev->bIsBigEndian)
    {
        SWAP(LwU8, ((LwU8*)&value)[0], ((LwU8*)&value)[1]);
    }
    return i2cDevRWIndex(pDev, hQueue, index, (LwU8 *)&value, 2, LW_FALSE);
}

/*!
 * @copydoc I2cDevWriteReg32
 *
 * Implemented using either the SMBus Block-Write Protocol (5.5.7) or other
 * another non-spec transaction type which doesn't include the size first.
 */
FLCN_STATUS
i2cDevWriteReg32
(
    I2C_DEVICE         *pDev,
    LwrtosQueueHandle   hQueue,
    LwU32               index,
    LwU32               value
)
{
    if (pDev->bIsBigEndian)
    {
        SWAP(LwU8, ((LwU8 *)&value)[0], ((LwU8 *)&value)[3]);
        SWAP(LwU8, ((LwU8 *)&value)[1], ((LwU8 *)&value)[2]);
    }
    return i2cDevRWIndex(pDev, hQueue, index, (LwU8 *)&value, 4, LW_FALSE);
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief   Variable-length indexed I2C read / write
 *
 * R/W an arbitrarily-sized buffer from/to a device. Note that the buffer is
 * transmitted byte-by-byte. The caller is responsible for dealing with any
 * device-level endianness characteristics that may exist after calling this
 * function.
 *
 * @param[in]   pDev        I2C-device object for the I2C device to R/W
 * @param[in]   hQueue
 *     Event-queue used when communicating with the I2C driver/task.
 * @param[in]   index       Index or command-code to use when issuing the command
 * @param[in]   pMessage    Pointer to byte-array holding the transferred data
 * @param[in]   messageSize The number of bytes of the array to transfer
 * @param[in]   bRead       The direction of the transfer
 *
 * @return FLCN_OK
 *     R/W completed successfully
 *
 * @return FLCN_ERR_TIMEOUT
 *     If the I2C task did not acknowledge the request before the timeout
 *     interval expired
 *
 * @return FLCN_ERR_ILWALID_STATE
 *     Ilwoked from the task that does not have allocated I2C context.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *     Unsupported message size.
 *
 * @return other
 *     Returns the error-code given by the I2C task in its acknowlegment to the
 *     I2C request see (taskR_i2c.c::i2c)
 */
FLCN_STATUS
i2cDevRWIndex
(
    I2C_DEVICE         *pDev,
    LwrtosQueueHandle   hQueue,
    LwU32               index,
    LwU8               *pMessage,
    LwU8                messageSize,
    LwBool              bRead
)
{
    FLCN_STATUS     status   = FLCN_ERR_I2C_BUSY;
    I2C_CONTEXT    *pContext = s_i2cContextLookup();
    LwU32           tries    = 0;
    LwU32           flags    = pDev->i2cFlags;
    I2C_INT_MESSAGE msg;

    if (pContext == NULL)
    {
        status = FLCN_ERR_ILWALID_STATE;
        goto i2cDevRWIndex_exit;
    }

    if (messageSize > sizeof(I2C_CONTEXT))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto i2cDevRWIndex_exit;
    }

    if (!bRead)
    {
        memcpy(pContext->data, pMessage, messageSize);
    }

    flags = bRead ? FLD_SET_DRF(_RM_FLCN, _I2C_FLAGS, _DIR, _READ, flags) :
                    FLD_SET_DRF(_RM_FLCN, _I2C_FLAGS, _DIR, _WRITE, flags);

    if ((pDev->bBlockProtocol) &&
        (messageSize >= RM_PMU_I2C_BLOCK_PROTOCOL_MIN))
    {
        flags = FLD_SET_DRF(_RM_FLCN, _I2C_FLAGS, _BLOCK_PROTOCOL, _ENABLED, flags);
    }

    while ((tries < pDev->i2cRetryCount) &&
           (status == FLCN_ERR_I2C_BUSY))
    {
        //
        // Queue-up the I2C RW-request with the I2C task (note that this is a
        // non-blocking call).
        //
        status = i2c(pDev->i2cPort,    // portId
                     flags,            // flags
                     pDev->i2cAddress, // address
                     index,            // index
                     messageSize,      // size
                     pContext->data,   // pMessage
                     hQueue);          // hQueue

        if (status != FLCN_OK)
        {
            continue;
        }

        //
        // Now wait for the I2C task to acknowledge completion of the request by
        // waiting for a response in the shared I2C queue.
        //
        if (FLCN_OK != lwrtosQueueReceive(hQueue, &msg, sizeof(msg),
                                          lwrtosMAX_DELAY))
        {
            status = FLCN_ERR_TIMEOUT;
            continue;
        }
        status = msg.errorCode;
        tries++;
    }

    if (bRead)
    {
        memcpy(pMessage, pContext->data, messageSize);
    }

i2cDevRWIndex_exit:
    return status;
}

/*!
 * @brief   Helper to look-up the I2C context of lwrrently running task.
 *
 * @return  Pointer to the I2C context buffer.
 */
static I2C_CONTEXT *
s_i2cContextLookup(void)
{
    I2C_CONTEXT *pContext = NULL;

    switch (osTaskIdGet())
    {
        case RM_PMU_TASK_ID_PMGR:
            pContext = &I2cContext[I2C_CONTEXT_ID_PMGR];
            break;
        case RM_PMU_TASK_ID_THERM:
            pContext = &I2cContext[I2C_CONTEXT_ID_THERM];
            break;
        case RM_PMU_TASK_ID_PERF:
            pContext = &I2cContext[I2C_CONTEXT_ID_PERF];
            break;
#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_MULT_DATA_PWR_CHANNEL))
        case RM_PMU_TASK_ID_PERF_CF:
            pContext = &I2cContext[I2C_CONTEXT_ID_PERF_CF];
            break;
#endif
#if (PMUCFG_FEATURE_ENABLED(PMU_PSI_I2C_INTERFACE))
        case RM_PMU_TASK_ID_LPWR_LP:
            pContext = &I2cContext[I2C_CONTEXT_ID_LPWR];
            break;
#endif
        default:
            PMU_BREAKPOINT();
            break;
    }

    return pContext;
}
