/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2008-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   task_i2c.h
 * @brief  Common header for all task I2C files.
 *
 * @section References
 * @ref "../../../drivers/resman/arch/lwalloc/common/inc/rmpmucmdif.h"
 *      rmpmucmdif.h
 */

#ifndef _TASK_I2C_H
#define _TASK_I2C_H

#include "flcntypes.h"
#include "unit_api.h"

#include "pmu_dpaux.h"
#include "pmu_obji2c.h"

/*! Perform a long write command. */
typedef struct
{
    //<! Common command structure for all PMU I2C transactions.
    RM_PMU_I2C_CMD_GENERIC  genCmd;
    //<! Offset of the message used in internal use case in PMU.
    void                   *pBuf;
} PMU_I2C_CMD_TX_QUEUE;

/*! Perform a long read command. */
typedef struct
{
    //<! Common command structure for all PMU I2C transactions.
    RM_PMU_I2C_CMD_GENERIC  genCmd;
    //<! Offset of the message used in internal use case in PMU.
    void                   *pBuf;
    //<! The EDDC segment register used in case of EDID read, set to 0 otherwise.
    LwU8                    segment;
} PMU_I2C_CMD_RX_QUEUE;

/*! Container for all I2C commands available, tagged by cmdType. */
typedef union
{
    RM_PMU_I2C_CMD_GENERIC  generic;
    PMU_I2C_CMD_TX_QUEUE    txQueue;
    PMU_I2C_CMD_RX_QUEUE    rxQueue;
} PMU_I2C_CMD;

/*! @brief Command structure for PMU-internal tasks to perform I2C. */
typedef struct
{
    DISP2UNIT_HDR       hdr;        //<! Must be first element of the structure

    PMU_I2C_CMD         cmd;
    LwrtosQueueHandle   hQueue;    //<! Queue in which to place the message response.
} I2C_INT_CMD;

/*!
 * Event type to send notifications to I2C task
 */
#define I2C_EVENT_TYPE_INT_TX    (DISP2UNIT_EVT__COUNT + 0)
#define I2C_EVENT_TYPE_INT_RX    (DISP2UNIT_EVT__COUNT + 1)
#define I2C_EVENT_TYPE_DPAUX     (DISP2UNIT_EVT__COUNT + 2)

/*!
 * Describes commands which may be passed into the pmgr task command queue.
 */
typedef union
{
    DISP2UNIT_HDR       hdr;
    DISP2UNIT_RM_RPC    rpc;

    I2C_INT_CMD internal;   //<! A command from the PMU internal interface.
    DPAUX_CMD   dpaux;      //<! DPAUX command.
} I2C_COMMAND;

FLCN_STATUS i2cPerformTransaction(I2C_PMU_TRANSACTION *pTa);
FLCN_STATUS i2cPerformTransactiolwiaSw(I2C_PMU_TRANSACTION *pTa);
FLCN_STATUS i2cPerformTransactiolwiaHw(I2C_PMU_TRANSACTION *pTa);
FLCN_STATUS i2cRecoverBusViaSw(I2C_PMU_TRANSACTION *pTa);

#endif // _TASK_I2C_H
