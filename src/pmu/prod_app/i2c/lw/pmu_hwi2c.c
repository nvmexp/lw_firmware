/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2007-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file pmu_hwi2c.c
 */

/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include "pmusw.h"
#include "dev_pwr_csb.h"
#include "dev_pmgr.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmu.h"
#include "pmu_objtimer.h"
#include "pmu_obji2c.h"
#include "task_i2c.h"
#include "flcnifi2c.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Global Variables ------------------------------- */

/*! Global command. Only one oustanding transaction is lwrrently allowed. */
I2C_HW_CMD i2cHwGlobalCmd = {0};

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
static FLCN_STATUS s_i2cHwRead(I2C_PMU_TRANSACTION *pTa, I2C_HW_CMD *pCmd);
static FLCN_STATUS s_i2cHwWrite(I2C_PMU_TRANSACTION *pTa, I2C_HW_CMD *pCmd);
static FLCN_STATUS s_i2cHwReadGeneric(I2C_PMU_TRANSACTION *pTa, I2C_HW_CMD *pCmd);
static FLCN_STATUS s_i2cSendByteCmd(I2C_PMU_TRANSACTION *pTa, LwBool bSendStop, LwU32  offset);
static FLCN_STATUS s_i2cHwReadChunks(I2C_PMU_TRANSACTION *pTa, I2C_HW_CMD *pCmd, LwBool isDDC);

/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief   Perform a generic I2C transaction using the HW controller
 *
 * @param[in, out] pTa
 *          Transaction data; please see @ref ./inc/task_i2c.h "task_i2c.h".
 *
 * @return  FLCN_OK
 *          The transaction completed successfully.
 *
 * @return  FLCN_ERR_I2C_NACK_ADDRESS
 *          No device acknowledged the address.
 *
 * @return  FLCN_ERR_I2C_NACK_BYTE
 *          No device acknowledged one of the message bytes.
 *
 * @return  FLCN_ERR_ILWALID_ARGUMENT
 *          The transaction pTa was detected to be invalid.
 *
 * @return  FLCN_ERR_TIMEOUT
 *          Clock stretching from the secondary took too long and the transaction
 *          aborted.  The bus is no longer in a valid state.
 */
FLCN_STATUS
i2cPerformTransactiolwiaHw
(
    I2C_PMU_TRANSACTION *pTa
)
{
    LwU32       flags     = pTa->pGenCmd->flags;
    I2C_HW_CMD *pCmd      = &i2cHwGlobalCmd;
    LwU32       portId    = DRF_VAL(_RM_FLCN, _I2C_FLAGS, _PORT, flags);
    LwU32       speedMode = DRF_VAL(_RM_FLCN, _I2C_FLAGS, _SPEED_MODE, flags);
    FLCN_STATUS status;

    pCmd->bStopRequired = LW_TRUE;

    i2cHwSetSpeedMode_HAL(&I2c, portId, speedMode);

    if (!pTa->bRead)
    {
        status = s_i2cHwWrite(pTa, pCmd);
    }
    else
    {
        status = s_i2cHwRead(pTa, pCmd);
    }

    return status;
}


/* ------------------------- Private Functions ------------------------------ */
static FLCN_STATUS
s_i2cHwWrite
(
    I2C_PMU_TRANSACTION *pTa,
    I2C_HW_CMD          *pCmd
)
{
    FLCN_STATUS status  = i2cHwInitCmdWrite_HAL(&I2c, pTa, pCmd);
    FLCN_STATUS status2 = FLCN_ERROR;

    while ((status == FLCN_OK) && (pCmd->status == FLCN_ERR_I2C_BUSY))
    {
        status = i2cHwWaitForIdle_HAL(&I2c, pCmd->portId);
        if (status == FLCN_OK)
        {
            status = i2cHwWriteNext_HAL(&I2c, pCmd);
        }
    }

    if (status == FLCN_OK)
    {
        status = i2cHwWaitForIdle_HAL(&I2c, pCmd->portId);
    }

    //
    // If a NACK is received during DAB/RAB phases HW will issue a stop by
    // itself hence no need to again issue a stop
    //
    if (status == FLCN_ERR_I2C_NACK_BYTE)
    {
        pCmd->bStopRequired = LW_FALSE;
    }
    else if (status == FLCN_ERR_TIMEOUT)
    {
        pCmd->bStopRequired = LW_TRUE;
    }

    if (pCmd->bStopRequired)
    {
        status2 = i2cSendStopViaHw_HAL(&I2c, pCmd->portId);
        if (status == FLCN_OK)
        {
            status = status2;
        }
    }

    return status;
}

static FLCN_STATUS
s_i2cSendByteCmd
(
    I2C_PMU_TRANSACTION *pTa,
    LwBool               bSendStop,
    LwU32                offset
)
{
    LwU32       flags       = pTa->pGenCmd->flags;
    LwU32       portId      = DRF_VAL(_RM_FLCN, _I2C_FLAGS, _PORT, flags);
    LwU32       indexLength = DRF_VAL(_RM_FLCN, _I2C_FLAGS, _INDEX_LENGTH, flags);
    FLCN_STATUS status      = FLCN_ERROR;

    if (indexLength != 1)
    {
        return  FLCN_ERR_ILWALID_ARGUMENT;
    }

    // Perform the initial operations we need before sending the message.
    status = i2cSendStartViaHw_HAL(&I2c, portId);

    // send address
    if (status == FLCN_OK)
    {
        status = i2cSendAddressViaHw_HAL(&I2c, pTa, LW_TRUE);
    }

    // send the index length 
    if (status == FLCN_OK)
    {
        status = i2cWriteByteViaHw_HAL(&I2c, portId, offset);
    }

    // send the stop bit
    if ((status == FLCN_OK) && bSendStop)
    {
        (void)i2cSendStopViaHw_HAL(&I2c, portId);
    }

    return status;
}

/* 
 * This function does i2c reads in 4 bytes chunk only. 
 * This can handle >4 bytes read but does so in 4 bytes transaction explicitly specifying
 * offset in each loop.
 *
 *@params[in] isDDC            Issue stop after Cmd Byte
 */ 
static FLCN_STATUS
s_i2cHwReadChunks
(
    I2C_PMU_TRANSACTION *pTa,
    I2C_HW_CMD          *pCmd,
    LwBool               isDDC
)
{
    LwU32       bytesRemaining = pTa->messageLength;
    LwU32       offset         = 0;
    LwBool      start          = LW_TRUE;
    FLCN_STATUS status         = FLCN_OK;

    pCmd->pMessage = pTa->pMessage;

    // Do transactions in 4 bytes chunk
    do {
        if (FLD_TEST_DRF(_RM_FLCN, _I2C_FLAGS, _INDEX_LENGTH, _ONE, pTa->pGenCmd->flags))
        {
            if (start)
            {
                offset = pTa->pGenCmd->index[0];
                start  = LW_FALSE;
            }
            else
            {
                offset += LW_PMGR_I2C_CNTL_BURST_SIZE_MAXIMUM;
            }
            status = s_i2cSendByteCmd(pTa, isDDC, offset);
        }

        if (status == FLCN_OK)
        {
            i2cHwInitCmdReadWithShortIndex_HAL(&I2c, pTa, pCmd, LW_FALSE, LW_TRUE ,bytesRemaining);
        }

        if (status == FLCN_OK)
        {
            status = i2cHwWaitForIdle_HAL(&I2c, pCmd->portId);
            if (status == FLCN_OK)
            {
                status = i2cHwReadNext_HAL(&I2c, pTa, pCmd, LW_FALSE);
            }
        }
        bytesRemaining = pCmd->bytesRemaining;
    } while ((bytesRemaining > 0) && (status == FLCN_OK));

    return status;
}


static FLCN_STATUS
s_i2cHwReadGeneric
(
    I2C_PMU_TRANSACTION *pTa,
    I2C_HW_CMD          *pCmd
)
{
    LwU32       bytesRemaining = pTa->messageLength;
    FLCN_STATUS status         = FLCN_OK;

    pCmd->pMessage = pTa->pMessage;

    // Hardware cannot support index sizes greater than 1.
    if (DRF_VAL(_RM_FLCN, _I2C_FLAGS, _INDEX_LENGTH, pTa->pGenCmd->flags) > 1)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    i2cHwInitCmdReadWithShortIndex_HAL(&I2c, pTa, pCmd, LW_TRUE, LW_FALSE, bytesRemaining);

    while ((status == FLCN_OK) && (pCmd->status == FLCN_ERR_I2C_BUSY))
    {
        status = i2cHwWaitForIdle_HAL(&I2c, pCmd->portId);
        if (status == FLCN_OK)
        {
            status = i2cHwReadNext_HAL(&I2c, pTa, pCmd, LW_TRUE);
        }
    }

    return status;
}

static FLCN_STATUS
s_i2cHwRead
(
    I2C_PMU_TRANSACTION *pTa,
    I2C_HW_CMD          *pCmd
)
{
    LwU32       flags  = pTa->pGenCmd->flags;
    FLCN_STATUS status = FLCN_OK;

    // Hardware cannot support index sizes greater than 1.
    if (DRF_VAL(_RM_FLCN, _I2C_FLAGS, _INDEX_LENGTH, flags) > 1)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // For GP100, switch to HW I2C WAR to use new HW Sequence, 
    // This WAR is limited to Non DDC devices as of now.
    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_I2C_WAR_BUG_200125155))
    {
        if (FLD_TEST_DRF(_RM_FLCN, _I2C_FLAGS, _DDC_DEVICE, _NO, flags))
        {
            status = s_i2cHwReadChunks(pTa, pCmd, LW_FALSE);
        }
        else
        {
            // For DDC we have switched to SW I2C on GP100
            // entering here should give the error
            return FLCN_ERROR;
        }
    }
    else
    {
        //
        // If the segment pointer is not 0 this implies this is an EDDC transaction
        // First write the segment pointer and then proceed with read
        // Note: EDDC will not be supported for HwReadchunks since stop is issued
        //       in between read, this would reset the segment pointer to 0.
        //
        if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_I2C_EDDC_SUPPORT_HW_I2C) && 
            (pTa->segment != 0))
        {
            status = i2cHwWriteByteWaitEddcSegment_HAL(&I2c, pTa);
        }
        if (status == FLCN_OK)
        {
            status = s_i2cHwReadGeneric(pTa, pCmd);
        }
    }

    //
    // If a NACK is received during DAB/RAB phases HW will issue a stop by
    // itself hence no need to again issue a stop
    //
    if (status == FLCN_ERR_I2C_NACK_BYTE)
    {
        pCmd->bStopRequired = LW_FALSE;
    }
    else if (status == FLCN_ERR_TIMEOUT)
    {
        pCmd->bStopRequired = LW_TRUE;
    }

    if (pCmd->bStopRequired)
    {
        (void)i2cSendStopViaHw_HAL(&I2c, pCmd->portId);
    }

    return status;
}
