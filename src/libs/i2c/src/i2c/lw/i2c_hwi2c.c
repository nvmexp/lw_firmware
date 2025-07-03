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
 * @file i2c_hwi2c.c
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "i2c_obji2c.h"
#include "lib_intfci2c.h"
#include "config/g_i2c_private.h"
#include "config/i2c-config.h"

/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Global Variables ------------------------------- */

/*! Global command. Only one oustanding transaction is lwrrently allowed.
 */
I2C_HW_CMD i2cHwGlobalCmd = {0};

/* ------------------------- Prototypes ------------------------------------- */
static FLCN_STATUS _i2cHwWrite(I2C_TRANSACTION *pTa, I2C_HW_CMD *pCmd)
    GCC_ATTRIB_SECTION("imem_libI2cHw", "_i2cHwWrite");
static FLCN_STATUS _i2cHwRead(I2C_TRANSACTION *pTa, I2C_HW_CMD *pCmd)
    GCC_ATTRIB_SECTION("imem_libI2cHw", "_i2cHwRead");
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief   Perform a generic I2C transaction using the HW controller
 *
 * @param[in, out] pTa
 *          Transaction data; please see @ref ./inc/flcn_i2c.h.
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
 *          Clock stretching from the slave took too long and the transaction
 *          aborted.  The bus is no longer in a valid state.
 */
FLCN_STATUS
i2cPerformTransactiolwiaHw
(
    I2C_TRANSACTION *pTa
)
{
    FLCN_STATUS status    = FLCN_ERROR;
    I2C_HW_CMD *pCmd      = &i2cHwGlobalCmd;
    LwU32       flags     = pTa->flags;
    LwU32       portId    = DRF_VAL(_RM_FLCN, _I2C_FLAGS, _PORT, flags);
    LwU32       speedMode = DRF_VAL(_RM_FLCN, _I2C_FLAGS, _SPEED_MODE, flags);
    pCmd->bStopRequired = LW_TRUE;

    status = i2cHwSetSpeedMode_HAL(&I2c, portId, speedMode);
    if (status == FLCN_OK)
    {
        if (!pTa->bRead)
        {
            status = _i2cHwWrite(pTa, pCmd);
        }
        else
        {
            status = _i2cHwRead(pTa, pCmd);
        }
    }
    return status;
}

/* ------------------------- Private Functions ------------------------------ */

/*!
 * Hardware i2c write implementation
 *
 * @param[in, out] pTa       The transaction description.
 *
 * @param[in, out] pCmd      The continuous command structure.
 *
 * @return FLCN_ERR_<xyz>
 */
FLCN_STATUS
_i2cHwWrite
(
    I2C_TRANSACTION *pTa,
    I2C_HW_CMD      *pCmd
)
{
    FLCN_STATUS status = i2cHwInitCmdWrite_HAL(&I2c, pTa, pCmd);
    FLCN_STATUS status2 = FLCN_ERROR;

    while ((status == FLCN_OK) &&
           (pCmd->status == FLCN_ERR_I2C_BUSY))
    {
        status = i2cWaitForHwIdle_HAL(&I2c, pCmd->portId, I2C_HW_IDLE_TIMEOUT_NS);
        if (status == FLCN_OK)
        {
           status = i2cHwWriteNext_HAL(&I2c, pCmd);
        }
    }

    if (status == FLCN_OK)
    {
        status = i2cWaitForHwIdle_HAL(&I2c, pCmd->portId, I2C_HW_IDLE_TIMEOUT_NS);
    }

    //
    // If NACK is recieved during RAB/DAB phases HW will issue automatic Stop,
    // no need to issue a stop manually
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

/*!
 * Hardware i2c read implementation
 *
 * @param[in, out] pTa       The transaction description.
 *
 * @param[in, out] pCmd      The continuous command structure.
 *
 * @return FLCN_ERR_<xyz>
 */
FLCN_STATUS
_i2cHwRead
(
    I2C_TRANSACTION *pTa,
    I2C_HW_CMD      *pCmd
)
{
    FLCN_STATUS status = FLCN_OK;

    // Hardware cannot support index sizes greater than 1.
    if (DRF_VAL(_RM_FLCN, _I2C_FLAGS, _INDEX_LENGTH, pTa->flags) > 1)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    status = i2cHwInitCmdReadWithShortIndex_HAL(&I2c, pTa, pCmd);

    while ((status == FLCN_OK) &&
           (pCmd->status == FLCN_ERR_I2C_BUSY))
    {
        status = i2cWaitForHwIdle_HAL(&I2c, pCmd->portId, I2C_HW_IDLE_TIMEOUT_NS);

        if (status == FLCN_OK)
        {
            status = i2cHwReadNext_HAL(&I2c, pTa, pCmd);
        }
    }

    //
    // If NACK is recieved during RAB/DAB phases HW will issue automatic Stop,
    // no need to issue a stop manually
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
       // Error handling and not override error code.
       (void)i2cSendStopViaHw_HAL(&I2c, pCmd->portId);
    }

    return status;
}
