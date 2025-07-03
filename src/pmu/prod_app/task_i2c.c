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
 * @file task_i2c.c
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_pmgr.h"
#include "dev_pwr_csb.h"

/* ------------------------- Application Includes --------------------------- */

#include "task_i2c.h"
#include "main.h"
#include "main_init_api.h"
#include "cmdmgmt/cmdmgmt.h"
#include "os/pmu_lwos_task.h"
#include "pmu_i2capi.h"
#include "pmu_obji2c.h"
#include "pmu_dpaux.h"
#include "flcnifi2c.h"

#include "g_pmurpc.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/*!
 * @brief   Defines the command buffer for the PERF_CF task.
 */
PMU_TASK_CMD_BUFFER_DEFINE(I2C, "dmem_i2cCmdBuffer");

/* ------------------------- Macros and Defines ----------------------------- */

/*!
 * I2C mutex timeout value, specified in ns.  Current value is 200ms.
 */
#define I2C_MUTEX_ACQUIRE_TIMEOUT_NS   200000000

/* ------------------------- Prototypes ------------------------------------- */



static FLCN_STATUS  s_i2cDecodeCommand(I2C_PMU_TRANSACTION *pTa, I2C_INT_CMD *pInt);
static FLCN_STATUS  s_i2cDispatchInternal(I2C_INT_CMD *pInt);

lwrtosTASK_FUNCTION(task_i2c, pvParameters);

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief      Initialize the I2C Task.
 *
 * @return     FLCN_OK on success,
 * @return     descriptive error code otherwise.
 */
FLCN_STATUS
i2cPreInitTask(void)
{
    FLCN_STATUS status = FLCN_OK;

    OSTASK_CREATE(status, PMU, I2C);
    if (status != FLCN_OK)
    {
        goto i2cPreInitTask_exit;
    }

    status = lwrtosQueueCreateOvlRes(&LWOS_QUEUE(PMU, I2C), 2, sizeof(I2C_COMMAND));
    if (status != FLCN_OK)
    {
        goto i2cPreInitTask_exit;
    }

i2cPreInitTask_exit:
    return status;
}

/*! Main function for the I2C task, which receives and dispatches commands. */
lwrtosTASK_FUNCTION(task_i2c, pvParameters)
{
    I2C_COMMAND cmd;

    i2cInitI2cGpios_HAL(&I2c);

    LWOS_TASK_LOOP_NO_CALLBACKS(LWOS_QUEUE(PMU, I2C), &cmd, status, PMU_TASK_CMD_BUFFER_PTRS_GET(I2C))
    {
        status = FLCN_ERR_QUEUE_TASK_ILWALID_EVENT_TYPE;

        switch (cmd.hdr.eventType)
        {
            case DISP2UNIT_EVT_RM_RPC:
                status = pmuRpcProcessUnitI2c(&(cmd.rpc));
                break;

            case I2C_EVENT_TYPE_INT_TX:
            case I2C_EVENT_TYPE_INT_RX:
                if (PMUCFG_FEATURE_ENABLED(PMU_I2C_INT))
                {
                    status = s_i2cDispatchInternal(&(cmd.internal));
                }
                break;

            case I2C_EVENT_TYPE_DPAUX:
                if (PMUCFG_FEATURE_ENABLED(PMU_DPAUX_SUPPORT))
                {
                    dpauxCommand(&(cmd.dpaux));
                    status = FLCN_OK;
                }
                break;
        }
    }
    LWOS_TASK_LOOP_END(status);
}

/*!
 * @brief  Dispatch a request to perfrom an I2C transaction to the correct
 *         implementation.
 *
 * @param[in,out]  pTa
 *          The transaction information.  The field 'bSwImpl' is used to
 *          determine the implementation.
 *
 * @return The value returned by the selected implementation.
 *
 * @return 'FLCN_ERR_I2C_BUSY'
 *     If the I2C mutex used to synchronize I2C access between the RM and PMU
 *     could not be acquired (indicates the bus is 'Busy' and the transaction
 *     may be retried at a later time).
 *
 * @return 'FLCN_ERR_ILWALID_ARGUMENT'
 *     Invalid input.
 *
 */
FLCN_STATUS
i2cPerformTransaction
(
    I2C_PMU_TRANSACTION *pTa
)
{
    FLCN_STATUS status      = FLCN_ERROR;
    FLCN_STATUS pmuStatus   = FLCN_ERROR;
    LwBool      bSwImpl     = LW_TRUE;
    LwBool      bWasBb      = LW_TRUE;
    LwU32       flags       = pTa->pGenCmd->flags;
    LwU32       indexLength = DRF_VAL(_RM_FLCN, _I2C_FLAGS, _INDEX_LENGTH, flags);
    LwU32       portId      = DRF_VAL(_RM_FLCN, _I2C_FLAGS, _PORT, flags);

    if (PMUCFG_FEATURE_ENABLED(PMU_SELWRITY_HARDENING))
    {
        // sanity check on portId, indexLength and messageLength
        if ((portId >= MAX_I2C_PORTS)            ||
            (indexLength > RM_PMU_I2C_INDEX_MAX) ||
            (pTa->messageLength > RM_PMU_I2C_MAX_MSG_SIZE))
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto i2cPerformTransaction_exit;
        }
    }

    bSwImpl = FLD_TEST_DRF(_RM_FLCN, _I2C_FLAGS, _FLAVOR, _SW, flags);

    pmuStatus = i2cMutexAcquire_HAL(&I2c,
                    portId,                        // portId
                    LW_TRUE,                       // bAcquire
                    I2C_MUTEX_ACQUIRE_TIMEOUT_NS); // timeoutns
    {
        if (pmuStatus != FLCN_OK)
        {
            // Mutex busy.
            return FLCN_ERR_I2C_BUSY;
        }

        //
        // When just starting an i2c transaction and just after successfully
        // acquiring the I2C mutex, it is never expected that the bus will be
        // busy.  If it is busy, consider it as being in an invalid state and
        // attempt to recover it.
        //
        if (!i2cIsBusReady_HAL(&I2c, portId))
        {
            // set the i2c mode to bit-banging mode (should never fail!)
            status = i2cSetMode_HAL(&I2c, portId, LW_TRUE, &bWasBb);
            PMU_HALT_COND(status == FLCN_OK);

            // now attempt to recover the bus
            status = i2cRecoverBusViaSw(pTa);
            i2cRestoreMode_HAL(&I2c, portId, bWasBb);

            // bail-out if we couldn't recover it, nothing else to do
            if (status != FLCN_OK)
            {
                goto i2cPerformTransaction_done;
            }
        }

        //
        // With the bus ready, set the desired operating mode and perform the
        // transaction.
        //
        status = i2cSetMode_HAL(&I2c, portId, bSwImpl, &bWasBb);
        if (status == FLCN_OK)
        {
            if (bSwImpl)
            {
                status = i2cPerformTransactiolwiaSw(pTa);
            }
            else if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_I2C_HW))
            {
                status = i2cPerformTransactiolwiaHw(pTa);
            }
            else
            {
                PMU_HALT();
            }
            i2cRestoreMode_HAL(&I2c, portId, bWasBb);
        }
    }

i2cPerformTransaction_done:
    pmuStatus = i2cMutexAcquire_HAL(&I2c,
                    portId,                        // portId
                    LW_FALSE,                      // bAcquire
                    I2C_MUTEX_ACQUIRE_TIMEOUT_NS); // timeoutns
    //
    // If this release fails, all subsequent PMU I2C will fail.  So, just hang
    // the PMU so these failures are detected.
    //
    PMU_HALT_COND(pmuStatus == FLCN_OK);

i2cPerformTransaction_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief  Decode a RM PMU Command into a transaction, using message memory
 *         space if appropriate.
 *
 * @param[out] pTa The transaction specified by the command.
 * @param[in] pCmd The command to decode.
 *
 * @return Error code so far.
 */
static FLCN_STATUS
s_i2cDecodeCommand
(
    I2C_PMU_TRANSACTION *pTa,
    I2C_INT_CMD         *pInt
)
{
    PMU_I2C_CMD *pCmd = &(pInt->cmd);

    pTa->pGenCmd = &(pCmd->generic);

    switch (pInt->hdr.eventType)
    {
        case I2C_EVENT_TYPE_INT_TX:
            pTa->bRead         = LW_FALSE;
            pTa->pMessage      = pCmd->txQueue.pBuf;
            pTa->messageLength = pCmd->txQueue.genCmd.bufSize;
            break;

        case I2C_EVENT_TYPE_INT_RX:
            pTa->bRead         = LW_TRUE;
            pTa->pMessage      = pCmd->rxQueue.pBuf;
            pTa->messageLength = pCmd->rxQueue.genCmd.bufSize;
            pTa->segment       = pCmd->rxQueue.segment;
            break;

        default:
            // Invalid I2C cmd.
            return FLCN_ERR_ILWALID_FUNCTION;
    }

    // If a request for block protocol, ensure that size can be supported.
    if (FLD_TEST_DRF(_RM_FLCN, _I2C_FLAGS, _BLOCK_PROTOCOL, _ENABLED,
                     pCmd->generic.flags) &&
        ((pTa->messageLength < RM_PMU_I2C_BLOCK_PROTOCOL_MIN) ||
         (pTa->messageLength > RM_PMU_I2C_BLOCK_PROTOCOL_MAX)))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    return FLCN_OK;
}

/*!
 * @brief   Dispatch a command from another PMU task.
 *
 * @param[in]   pInt    The internally authored packet describing what to do.
 */
static FLCN_STATUS
s_i2cDispatchInternal
(
    I2C_INT_CMD *pInt
)
{
    I2C_INT_MESSAGE     msg;
    I2C_PMU_TRANSACTION ta;
    FLCN_STATUS         status;

    status = s_i2cDecodeCommand(&ta, pInt);
    if (status == FLCN_OK)
    {
        status = i2cPerformTransaction(&ta);
    }

    msg.errorCode = status;

    return lwrtosQueueSend(pInt->hQueue, &msg, sizeof(msg), 0);
}
