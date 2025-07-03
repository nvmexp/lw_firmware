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
 * @file   i2c_i2cgm20x.c
 * @brief  Contains all I2C specific routines specific to GM20X and later.
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "flcncmn.h"
#include "i2c_obji2c.h"
#include "lib_intfci2c.h"
#include "dev_pmgr.h"
#include "config/g_i2c_private.h"
#include "config/i2c-config.h"

/* ------------------------- Types Definitions ----------------------------- */
#define GET_BIT(v, p)       (((v) >> (p)) & 1)
#define SET_BIT(v, p, b)    (((b) << (p)) | ((v) & ~(1 << (p))))

/* ------------------------- External Definitions -------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Private Function Definitions ------------------- */
static FLCN_STATUS _i2cHwStatusToI2cStatus(LwU32 hwStatus)
    GCC_ATTRIB_SECTION("imem_libI2cHw", "_i2cHwStatusToI2cStatus");

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief Is the bus associated with 'port' ready for a bus transaction?
 *
 * Check to see if SCL and SDA is lwrrently high (ie. not being driven low).
 * If either (or both) lines are low, consider the bus as "busy" and therefore
 * not ready to accept the next transaction.
 *
 * @param[in]  port         The physical port for the bus to consider
 * @param[in]  pbIsBusReady Flag Pointer to tell if Bus ready.
 *             LW_TRUE      Both SCL and SDA are high. The master may commence the next transaction.
 *             LW_FALSE     One (or both) of the I2C lines is being pulled/driven low.
 *
 * @return FLCN_OK if succeed else error.
 */
FLCN_STATUS
i2cIsBusReady_GM20X
(
    LwU32   port,
    LwBool *pbIsBusReady
)
{
    FLCN_STATUS flcnStatus  = FLCN_OK;
    LwU32       reg32       = 0;

    *pbIsBusReady = LW_FALSE;

    PMGR_REG_RD32_ERR_EXIT(LW_PMGR_I2C_OVERRIDE(port), &reg32);

    // Ensure that neither line is being driven low (by master or slave(s))
    *pbIsBusReady = FLD_TEST_DRF(_PMGR, _I2C_OVERRIDE, _SCLPAD_IN, _ONE, reg32) &&
                    FLD_TEST_DRF(_PMGR, _I2C_OVERRIDE, _SDAPAD_IN, _ONE, reg32);

ErrorExit:
    return flcnStatus;
}

/*!
 * Reset the HW engine and wait for it to idle.
 *
 * @param[in]  portId  The port identifier.
 *
 * @return FLCN_OK upon success
 * @return FLCN_ERR_I2C_BUSY if the timeout is exceeded.
 */
FLCN_STATUS
i2cHwReset_GM20X
(
    LwU32 portId
)
{
    FLCN_STATUS flcnStatus = FLCN_OK;
    LwU32       cntl = DRF_DEF(_PMGR, _I2C_CNTL, _CMD, _RESET);

    PMGR_REG_WR32_ERR_EXIT(LW_PMGR_I2C_CNTL(portId), cntl);

    flcnStatus = i2cWaitForHwIdle_HAL(&I2c, portId, I2C_HW_IDLE_TIMEOUT_US);

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief Wait for the HW I2C controller to idle, or until an internal timeout.
 *
 * @param[in]  port  The port of the HW I2C controller.
 *
 * @return FLCN_OK upon success
 * @return FLCN_ERR_I2C_BUSY if the timeout is exceeded.
 *
 * Note: Even if the command times out in hardware, FLCN_OK will be returned.
 *       The idea for this function is to prevent collisions on the bus, not
 *       to report the status of a previous command.
 */
FLCN_STATUS
i2cWaitForHwIdle_GM20X
(
    LwU32 port,
    LwU32 timeout
)
{
    FLCN_TIMESTAMP startTime;
    LwU32 cntlReg       = LW_PMGR_I2C_CNTL(port);
    LwU32 cntl          = 0;
    LwU32 elapsedTime   = 0;
    FLCN_STATUS flcnStatus;

    osPTimerTimeNsLwrrentGet(&startTime);

    PMGR_REG_RD32_ERR_EXIT(cntlReg, &cntl);

    while ((!FLD_TEST_DRF(_PMGR, _I2C_CNTL, _CYCLE, _DONE, cntl)) &&
           (!FLD_TEST_DRF(_PMGR, _I2C_CNTL, _STATUS, _TIMEOUT, cntl)) &&
           (elapsedTime < timeout))
    {
        lwrtosYIELD();
        PMGR_REG_RD32_ERR_EXIT(cntlReg, &cntl);

        elapsedTime = osPTimerTimeNsElapsedGet(&startTime);
    }

    flcnStatus =  (elapsedTime < timeout) ? FLCN_OK : FLCN_ERR_I2C_BUSY;

ErrorExit:
    return flcnStatus;
}

/*!
 * Restore the previous operating mode.
 *
 * @param[in] portId    The port identifier.
 * @param[in] bWasBb    The previous operating mode.
 *                      (was it bitbanging i.e SwI2C ?)
 *
 * @return FLCN_OK upon success else error.
 */
FLCN_STATUS
i2cRestoreMode_GM20X
(
    LwU32 portId,
    LwU32 bWasBb
)
{
    FLCN_STATUS flcnStatus = FLCN_OK;
    LwU32 override = DRF_DEF(_PMGR, _I2C_OVERRIDE, _PMU, _DISABLE) |
                     DRF_DEF(_PMGR, _I2C_OVERRIDE, _SCL, _OUT_ONE) |
                     DRF_DEF(_PMGR, _I2C_OVERRIDE, _SDA, _OUT_ONE);

    if (bWasBb)
    {
        override = FLD_SET_DRF(_PMGR, _I2C_OVERRIDE, _SIGNALS, _ENABLE,
                               override);
    }
    else // !bWasBb
    {
        override = FLD_SET_DRF(_PMGR, _I2C_OVERRIDE, _SIGNALS, _DISABLE,
                               override);
    }

    PMGR_REG_WR32_ERR_EXIT(LW_PMGR_I2C_OVERRIDE(portId), override);

ErrorExit:
    return flcnStatus;
}

/*!
 * Set the new operating mode, and return the previous mode.
 *
 * @param[in]  portId   The port identifier.
 * @param[in]  bSwI2c   The target mode (LW_TRUE = bit-bang, LW_FALSE = HWI2C).
 * @param[out] pbWasSwI2c
 *    Optional (may be NULL) pointer written with the current I2C operating-
 *    mode when requested (LW_TRUE = SW bit-bang). Ignored when NULL.
 *
 * @return 'FLCN_OK'
 *      if the operation completed successfully.
 *
 * @return 'FLCN_ERR_I2C_BUSY'
 *      if the operation timed out waiting for the HW controller.
 *
 * @return 'FLCN_ERR_I2C_BUSY'
 *      if the current mode cannot be exited safely.
 *
 * @note
 *     This function does not deal with HW polling (which is broken, see bug
 *     671708) or stereo vision.
 *
 * @note
 *     This function does not check the current bus state before setting the
 *     mode. In cases where it matters, the caller is responsible for ensuring
 *     the bus is in a good state before calling this function.
 */
FLCN_STATUS
i2cSetMode_GM20X
(
    LwU32   portId,
    LwBool  bSwI2c,
    LwBool *pbWasSwI2c
)
{
    FLCN_STATUS flcnStatus = FLCN_OK;
    LwU32       override;

    // if requested, return/save the current operating-mode
    if (pbWasSwI2c != NULL)
    {
        LwU32 val = 0;

        PMGR_REG_RD32_ERR_EXIT(LW_PMGR_I2C_OVERRIDE(portId), &val);
        *pbWasSwI2c = FLD_TEST_DRF(_PMGR, _I2C_OVERRIDE, _SIGNALS, _ENABLE, val);
    }

    //
    // Set the I2C override register for the port to allow the FLCN to master
    // the bus when SW bit-banging mode is requested. Make sure it is disabled
    // when HWI2C mode is requested.
    //
    override = DRF_DEF(_PMGR, _I2C_OVERRIDE, _PMU    , _DISABLE) |
               DRF_DEF(_PMGR, _I2C_OVERRIDE, _SDA    , _OUT_ONE) |
               DRF_DEF(_PMGR, _I2C_OVERRIDE, _SCL    , _OUT_ONE);

    if(bSwI2c)
    {
       override = FLD_SET_DRF(_PMGR, _I2C_OVERRIDE, _SIGNALS, _ENABLE , override);
    }
    else
    {
        override = FLD_SET_DRF(_PMGR, _I2C_OVERRIDE, _SIGNALS, _DISABLE, override);
    }

    PMGR_REG_WR32_ERR_EXIT(LW_PMGR_I2C_OVERRIDE(portId), override);

    //
    // When entering HWI2C-mode, we need to reset the controller.  Since we
    // don't necessarily do so every time we enter HWI2C-mode, reset it
    // regardless of the previous mode.
    //
    if (!bSwI2c)
    {
        return i2cHwReset_HAL(&I2c, portId);
    }

ErrorExit:
    return flcnStatus;
}

/*!
 * Set the speed of the HW I2C controller on a given port.
 *
 * @param[in] portId        The port identifying the controller.
 * @param[in] speedMode     The speed mode to run at.
 */
FLCN_STATUS
i2cHwSetSpeedMode_GM20X
(
    LwU32 portId,
    LwU32 speedMode
)
{
    FLCN_STATUS flcnStatus = FLCN_OK;
    LwU32       timing = DRF_DEF(_PMGR, _I2C_TIMING, _IGNORE_ACK, _DISABLE) |
                         DRF_DEF(_PMGR, _I2C_TIMING, _TIMEOUT_CHECK, _ENABLE);

    switch (speedMode)
    {
        // Default should not be hit if above layers work correctly.
        default:

        case LW_RM_FLCN_I2C_FLAGS_SPEED_MODE_100KHZ:
            timing = FLD_SET_DRF(_PMGR, _I2C_TIMING, _SCL_PERIOD, _100KHZ,
                                 timing);
            timing = FLD_SET_DRF_NUM(_PMGR, _I2C_TIMING, _TIMEOUT_CLK_CNT,
                                     I2C_SCL_CLK_TIMEOUT_100KHZ, timing);
            break;

        case LW_RM_FLCN_I2C_FLAGS_SPEED_MODE_200KHZ:
            //
            // Using hardcoded value of LW_PMGR_I2C_TIMING_SCL_PERIOD_200KHZ.
            // Will replace it with macro, once it is added for all Maxwell
            // dev_pmgr ref manuals.
            //
            timing = timing | 0x80;
            timing = FLD_SET_DRF_NUM(_PMGR, _I2C_TIMING, _TIMEOUT_CLK_CNT,
                                     I2C_SCL_CLK_TIMEOUT_200KHZ, timing);
            break;

        case LW_RM_FLCN_I2C_FLAGS_SPEED_MODE_300KHZ:
            //
            // Using hardcoded value of LW_PMGR_I2C_TIMING_SCL_PERIOD_300KHZ.
            // Will replace it with marco, once it is added for all Maxwell
            // dev_pmgr ref manuals.
            //
            timing = timing | 0x50;
            timing = FLD_SET_DRF_NUM(_PMGR, _I2C_TIMING, _TIMEOUT_CLK_CNT,
                                     I2C_SCL_CLK_TIMEOUT_300KHZ, timing);
            break;

        case LW_RM_FLCN_I2C_FLAGS_SPEED_MODE_400KHZ:
            timing = FLD_SET_DRF(_PMGR, _I2C_TIMING, _SCL_PERIOD, _400KHZ,
                                 timing);
            timing = FLD_SET_DRF_NUM(_PMGR, _I2C_TIMING, _TIMEOUT_CLK_CNT,
                                     I2C_SCL_CLK_TIMEOUT_400KHZ, timing);
            break;
    }

    PMGR_REG_WR32_ERR_EXIT(LW_PMGR_I2C_TIMING(portId), timing);

ErrorExit:
    return flcnStatus;
}

/*!
 * Send a start in manual mode.
 *
 * @param[in] portId            The port identifier.
 *
 * @return FLCN_OK upon success
 * @return FLCN_ERR_I2C_BUSY if the timeout is exceeded.
 */
FLCN_STATUS
i2cSendStartViaHw_GM20X
(
    LwU32 portId
)
{
    FLCN_STATUS flcnStatus = FLCN_OK;
    LwU32       cntl = DRF_DEF(_PMGR, _I2C_CNTL, _GEN_START, _YES) |
                       DRF_DEF(_PMGR, _I2C_CNTL, _INTR_WHEN_DONE, _YES);

    PMGR_REG_WR32_ERR_EXIT(LW_PMGR_I2C_CNTL(portId), cntl);

    flcnStatus = i2cWaitForHwIdle_HAL(&I2c, portId, I2C_HW_IDLE_TIMEOUT_NS);

ErrorExit:
    return flcnStatus;
}

/*!
 * Write a byte in manual mode.
 *
 * @param[in] portId            The port identifier.
 * @param[in] data              The byte to write.
 *
 * @return FLCN_OK upon success
 * @return FLCN_ERR_I2C_BUSY if the timeout is exceeded.
 */
FLCN_STATUS
i2cWriteByteViaHw_GM20X
(
    LwU32 portId,
    LwU32 data
)
{
    FLCN_STATUS flcnStatus = FLCN_OK;
    LwU32       cntl = DRF_DEF(_PMGR, _I2C_CNTL, _CMD, _WRITE) |
                       DRF_DEF(_PMGR, _I2C_CNTL, _INTR_WHEN_DONE, _YES) |
                       DRF_NUM(_PMGR, _I2C_CNTL, _BURST_SIZE, 1);

    PMGR_REG_WR32_ERR_EXIT(LW_PMGR_I2C_DATA(portId), data);
    PMGR_REG_WR32_ERR_EXIT(LW_PMGR_I2C_CNTL(portId), cntl);

    flcnStatus = i2cWaitForHwIdle_HAL(&I2c, portId, I2C_HW_IDLE_TIMEOUT_NS);

ErrorExit:
    return flcnStatus;
}

/*!
 * Send a stop in manual mode.
 *
 * @param[in] portId            The port identifier.
 *
 * @return FLCN_OK
 *     upon success
 *
 * @return FLCN_ERR_I2C_BUSY if the timeout is exceeded.
 */
FLCN_STATUS
i2cSendStopViaHw_GM20X
(
    LwU32 portId
)
{
    FLCN_STATUS flcnStatus = FLCN_OK;
    LwU32       cntl = DRF_DEF(_PMGR, _I2C_CNTL, _GEN_STOP, _YES) |
                       DRF_DEF(_PMGR, _I2C_CNTL, _INTR_WHEN_DONE, _YES);

    PMGR_REG_WR32_ERR_EXIT(LW_PMGR_I2C_CNTL(portId), cntl);

    flcnStatus = i2cWaitForHwIdle_HAL(&I2c, portId, I2C_HW_IDLE_TIMEOUT_NS);

ErrorExit:
    return flcnStatus;
}

/*!
 * Send an address in manual mode.
 *
 * @param[in] pTa               The transaction description.
 * @param[in] bSending          The read/write bit to use with the address.
 *
 * @return FLCN_OK upon success
 * @return FLCN_ERR_I2C_BUSY if the timeout is exceeded.
 */
FLCN_STATUS
i2cSendAddressViaHw_GM20X
(
    I2C_TRANSACTION *pTa,
    LwBool          bSending
)
{
    LwU32       flags   = pTa->flags;
    LwU32       portId  = DRF_VAL(_RM_FLCN, _I2C_FLAGS, _PORT, flags);
    LwU32       address = pTa->i2cAddress;
    FLCN_STATUS flcnStatus = FLCN_OK;

    switch (DRF_VAL(_RM_FLCN, _I2C_FLAGS, _ADDRESS_MODE, flags))
    {
        default:

        case LW_RM_FLCN_I2C_FLAGS_ADDRESS_MODE_NO_ADDRESS:
            return FLCN_OK;

        case LW_RM_FLCN_I2C_FLAGS_ADDRESS_MODE_7BIT:
            return i2cWriteByteViaHw_HAL(&I2c, portId, (address | !bSending) & 0xFF);

        case LW_RM_FLCN_I2C_FLAGS_ADDRESS_MODE_10BIT:
            //
            // For 10-bits addressing, the 6 MSB bits on the 2 bytes are
            // b11110AAR; 10-address is 1111_0AAR_AAAA_AAAA
            //
            flcnStatus = i2cWriteByteViaHw_HAL(&I2c, portId, (((address >> 9) << 1) & 0xF7) |
                     0xF0 | !bSending);

            if (flcnStatus == FLCN_OK)
            {
                flcnStatus = i2cWriteByteViaHw_HAL(&I2c, portId, (address >> 1) & 0xFF);
            }
            return flcnStatus;
    }
}

/*!
 * Send a self-contained write command.
 *
 * @param[in,out] pTa           The transaction description.
 *
 * @return FLCN_OK if succeed else error.
 */
FLCN_STATUS
i2cHwShortWrite_GM20X
(
    I2C_TRANSACTION *pTa
)
{
    FLCN_STATUS flcnStatus = FLCN_OK;
    LwU32       flags  = pTa->flags;
    LwU32       portId = DRF_VAL(_RM_FLCN, _I2C_FLAGS, _PORT, flags);
    LwU32       addr = 0;
    LwU32       cntl = 0;
    LwU32       data = 0;
    LwU32       i    = 0;

    // Note: GEN_RAB done later; STATUS and CYCLE are outputs.
    cntl = DRF_DEF(_PMGR, _I2C_CNTL, _GEN_START, _YES)                |
           DRF_DEF(_PMGR, _I2C_CNTL, _GEN_STOP, _YES)                 |
           DRF_DEF(_PMGR, _I2C_CNTL, _CMD, _WRITE)                    |
           DRF_DEF(_PMGR, _I2C_CNTL, _INTR_WHEN_DONE, _YES)           |
           DRF_NUM(_PMGR, _I2C_CNTL, _BURST_SIZE, pTa->messageLength) |
           DRF_DEF(_PMGR, _I2C_CNTL, _GEN_NACK, _NO);

    switch (DRF_VAL(_RM_FLCN, _I2C_FLAGS, _ADDRESS_MODE, flags))
    {
        default:
        case LW_RM_FLCN_I2C_FLAGS_ADDRESS_MODE_NO_ADDRESS:
            break;

        case LW_RM_FLCN_I2C_FLAGS_ADDRESS_MODE_7BIT:
            addr = DRF_NUM(_PMGR, _I2C_ADDR, _DAB, pTa->i2cAddress >> 1)|
                   DRF_DEF(_PMGR, _I2C_ADDR, _TEN_BIT, _DISABLE);
            break;

        case LW_RM_FLCN_I2C_FLAGS_ADDRESS_MODE_10BIT:
            addr = DRF_NUM(_PMGR, _I2C_ADDR, _DAB, pTa->i2cAddress >> 1)|
                   DRF_DEF(_PMGR, _I2C_ADDR, _TEN_BIT, _ENABLE);
            break;
    }

    if (DRF_VAL(_RM_FLCN, _I2C_FLAGS, _INDEX_LENGTH, flags) == 1)
    {
        cntl = FLD_SET_DRF(_PMGR, _I2C_CNTL, _GEN_RAB, _YES, cntl);
        addr = FLD_SET_DRF_NUM(_PMGR, _I2C_ADDR, _RAB, pTa->index[0],
                               addr);
    }
    else // preconditions imply 'indexLength' == 0
    {
        cntl = FLD_SET_DRF(_PMGR, _I2C_CNTL, _GEN_RAB, _NO, cntl);
    }

    for (i = 0; i < pTa->messageLength; i++)
    {
        data |= *pTa->pMessage << (i * 8);
        pTa->pMessage++;
    }

    PMGR_REG_WR32_ERR_EXIT(LW_PMGR_I2C_ADDR(portId), addr);
    PMGR_REG_WR32_ERR_EXIT(LW_PMGR_I2C_DATA(portId), data);
    PMGR_REG_WR32_ERR_EXIT(LW_PMGR_I2C_CNTL(portId), cntl);

ErrorExit:
    return flcnStatus;
}

/*!
 * Start off a write.
 *
 * @param[in, out] pTa      The transaction description.
 * @param[out]     pCmd     The continuous command structure.
 *
 * @return FLCN_OK upon success
 * @return FLCN_ERR_I2C_BUSY if the timeout is exceeded.
 */
FLCN_STATUS
i2cHwInitCmdWrite_GM20X
(
    I2C_TRANSACTION    *pTa,
    I2C_HW_CMD         *pCmd
)
{
    LwU32       flags       = pTa->flags;
    LwU32       portId      = DRF_VAL(_RM_FLCN, _I2C_FLAGS, _PORT, flags);
    LwU32       indexLength = DRF_VAL(_RM_FLCN, _I2C_FLAGS, _INDEX_LENGTH, flags);
    FLCN_STATUS flcnStatus  = FLCN_ERROR;
    LwU32       i           = 0;

    pCmd->portId = portId;
    pCmd->bRead = LW_FALSE;
    pCmd->pMessage = pTa->pMessage;
    pCmd->status = FLCN_ERR_I2C_BUSY;

    if ((DRF_VAL(_RM_FLCN, _I2C_FLAGS, _INDEX_LENGTH, flags) <= 1) &&
        (pTa->messageLength < LW_PMGR_I2C_CNTL_BURST_SIZE_MAXIMUM))
    {
        flcnStatus = i2cHwShortWrite_HAL(&I2c, pTa);
        pCmd->cntl = 0;
        pCmd->data = 0;
        pCmd->bytesRemaining = 0;
        pCmd->bStopRequired = LW_FALSE;

        return flcnStatus;
    }

    // Long write case - Perform the initial operations we need before sending the message.
    flcnStatus = i2cSendStartViaHw_HAL(&I2c, portId);

    if (flcnStatus == FLCN_OK)
    {
        flcnStatus = i2cSendAddressViaHw_HAL(&I2c, pTa, LW_TRUE);
    }

    for (i = 0; (flcnStatus == FLCN_OK) && (i < indexLength); i++)
    {
        flcnStatus = i2cWriteByteViaHw_HAL(&I2c, portId, pTa->index[i]);
    }

    if ((flcnStatus == FLCN_OK) &&
        FLD_TEST_DRF(_RM_FLCN, _I2C_FLAGS, _RESTART, _SEND, flags))
    {
        flcnStatus = i2cSendStartViaHw_HAL(&I2c, portId);
        if (flcnStatus == FLCN_OK)
        {
            flcnStatus = i2cSendAddressViaHw_HAL(&I2c, pTa, LW_TRUE);
        }
    }

    // Issue the first byte of the message.
    if (pTa->messageLength == 0)
    {
        pCmd->data = 0;
        pCmd->cntl = DRF_DEF(_PMGR, _I2C_CNTL, _GEN_STOP, _YES) |
                     DRF_DEF(_PMGR, _I2C_CNTL, _INTR_WHEN_DONE, _YES);
        pCmd->bytesRemaining = 0;
        pCmd->bStopRequired = LW_FALSE;
    }
    else // pTa->messageLength != 0
    {
        if (FLD_TEST_DRF(_RM_FLCN, _I2C_FLAGS, _BLOCK_PROTOCOL, _ENABLED, flags))
        {
            // Actually write the size
            pCmd->data = pTa->messageLength;
            pCmd->cntl = DRF_DEF(_PMGR, _I2C_CNTL, _CMD           , _WRITE) |
                         DRF_DEF(_PMGR, _I2C_CNTL, _INTR_WHEN_DONE, _YES)   |
                         DRF_NUM(_PMGR, _I2C_CNTL, _BURST_SIZE    , 1);

            PMGR_REG_WR32_ERR_EXIT(LW_PMGR_I2C_DATA(portId), pCmd->data);
            PMGR_REG_WR32_ERR_EXIT(LW_PMGR_I2C_CNTL(portId), pCmd->cntl);

            //
            // Wait for write to complete.  Ignoring the status for now because
            // these writes below are always happening, even if everything above
            // failed!
            //
            flcnStatus = i2cWaitForHwIdle_HAL(&I2c, portId, I2C_HW_IDLE_TIMEOUT_NS);
        }

        pCmd->data = *pTa->pMessage;
        pCmd->cntl = DRF_DEF(_PMGR, _I2C_CNTL, _CMD           , _WRITE) |
                     DRF_DEF(_PMGR, _I2C_CNTL, _INTR_WHEN_DONE, _YES)   |
                     DRF_NUM(_PMGR, _I2C_CNTL, _BURST_SIZE    , 1);

        if (pTa->messageLength == 1)
        {
            pCmd->cntl = FLD_SET_DRF(_PMGR, _I2C_CNTL, _GEN_STOP, _YES,
                                     pCmd->cntl);
            pCmd->bStopRequired = LW_FALSE;
        }
        else // pTa->messageLength > 1
        {
            pCmd->cntl = FLD_SET_DRF(_PMGR, _I2C_CNTL, _GEN_STOP, _NO,
                                     pCmd->cntl);
            pCmd->bStopRequired = LW_TRUE;
        }
        pCmd->bytesRemaining = pTa->messageLength - 1;
        pCmd->pMessage++;
    }

    PMGR_REG_WR32_ERR_EXIT(LW_PMGR_I2C_DATA(portId), pCmd->data);
    PMGR_REG_WR32_ERR_EXIT(LW_PMGR_I2C_CNTL(portId), pCmd->cntl);

    if (pCmd->bytesRemaining > 0)
    {
        pCmd->data = *pCmd->pMessage;
        pCmd->pMessage++;
    }

ErrorExit:
    return flcnStatus;
}

/*!
 * Write next byte.
 *
 * @param[in, out] pCmd      The continuous command structure.
 *
 * @return The FLCN_ERR_<xyz> equivalent status.
 */
FLCN_STATUS
i2cHwWriteNext_GM20X
(
    I2C_HW_CMD *pCmd
)
{
    FLCN_STATUS flcnStatus  = FLCN_ERROR;
    LwU32       cntl        = 0;

    PMGR_REG_RD32_ERR_EXIT(LW_PMGR_I2C_CNTL(pCmd->portId), &cntl);

    flcnStatus = _i2cHwStatusToI2cStatus(DRF_VAL(_PMGR, _I2C_CNTL, _STATUS, cntl));
    // Get the next command running ASAP to reduce latency.
    if ((flcnStatus == FLCN_OK) && (pCmd->bytesRemaining > 0))
    {
        PMGR_REG_WR32_ERR_EXIT(LW_PMGR_I2C_DATA(pCmd->portId), pCmd->data);
        PMGR_REG_WR32_ERR_EXIT(LW_PMGR_I2C_CNTL(pCmd->portId), pCmd->cntl);
        pCmd->bytesRemaining--;
    }

    if (flcnStatus == FLCN_OK)
    {
        if (pCmd->bytesRemaining > 0)
        {
            pCmd->data = *pCmd->pMessage;
            pCmd->pMessage++;
        }
        else
        {
            pCmd->status = flcnStatus;
        }
    }

ErrorExit:
    return flcnStatus;
}

/*!
 * Prepare next read command in sequence.
 *
 * @param[in,out]   pCmd            The command data.
 * @param[in]       bytesRemaining  The number of bytes remaining.
 */
void
i2cHwReadPrep_GM20X
(
    I2C_HW_CMD *pCmd,
    LwU32       bytesRemaining
)
{
    LwU32 bytesToRead = 0;

    bytesToRead = LW_MIN(bytesRemaining,
                         LW_PMGR_I2C_CNTL_BURST_SIZE_MAXIMUM);

    pCmd->cntl = FLD_SET_DRF_NUM(_PMGR, _I2C_CNTL, _BURST_SIZE,
                                 bytesToRead, pCmd->cntl);

    if (bytesRemaining <= LW_PMGR_I2C_CNTL_BURST_SIZE_MAXIMUM)
    {
        pCmd->cntl = FLD_SET_DRF(_PMGR, _I2C_CNTL, _GEN_STOP, _YES,
                                 pCmd->cntl);
        pCmd->bStopRequired = LW_FALSE;
    }
}

/*!
 * Send a read with a 0/1-byte index.
 *
 * @param[in, out] pTa      The transaction description.
 * @param[in, out] pCmd     The continuous command structure.
 *
 * @return FLCN_OK if succeed else error.
 */
FLCN_STATUS
i2cHwInitCmdReadWithShortIndex_GM20X
(
    I2C_TRANSACTION    *pTa,
    I2C_HW_CMD         *pCmd
)
{
    FLCN_STATUS flcnStatus  = FLCN_OK;
    LwU32       flags       = pTa->flags;
    LwU32       cntl        = 0;
    LwU32       data        = 0;
    LwU32       addr        = 0;
    LwU32       bytesToRead = LW_U32_MAX;
    LwU32       portId      = DRF_VAL(_RM_FLCN, _I2C_FLAGS, _PORT, flags);
    LwU32       address     = pTa->i2cAddress;
    LwBool      bSendAddress    = LW_FALSE;
    LwU32       bytesRemaining  = pTa->messageLength;
    LwU8       *pMessage    = pTa->pMessage;

    // Determine initial command.

    // Note: GEN_RAB and BURST_SIZE done later; STATUS and CYCLE are outputs.
    cntl = DRF_DEF(_PMGR, _I2C_CNTL, _GEN_START, _YES)      |
           DRF_DEF(_PMGR, _I2C_CNTL, _GEN_STOP, _NO)        |
           DRF_DEF(_PMGR, _I2C_CNTL, _CMD, _READ)           |
           DRF_DEF(_PMGR, _I2C_CNTL, _INTR_WHEN_DONE, _YES) |
           DRF_DEF(_PMGR, _I2C_CNTL, _GEN_NACK, _NO);

    switch (DRF_VAL(_RM_FLCN, _I2C_FLAGS, _ADDRESS_MODE, flags))
    {
        default:

        case LW_RM_FLCN_I2C_FLAGS_ADDRESS_MODE_NO_ADDRESS:
            bSendAddress = LW_FALSE;
            break;

        case LW_RM_FLCN_I2C_FLAGS_ADDRESS_MODE_7BIT:
            bSendAddress = LW_TRUE;
            addr = DRF_NUM(_PMGR, _I2C_ADDR, _DAB, address >> 1) |
                   DRF_DEF(_PMGR, _I2C_ADDR, _TEN_BIT, _DISABLE);
            break;

        case LW_RM_FLCN_I2C_FLAGS_ADDRESS_MODE_10BIT:
            bSendAddress = LW_TRUE;
            addr = DRF_NUM(_PMGR, _I2C_ADDR, _DAB, address >> 1) |
                   DRF_DEF(_PMGR, _I2C_ADDR, _TEN_BIT, _ENABLE);
            break;
    }

    if (DRF_VAL(_RM_FLCN, _I2C_FLAGS, _INDEX_LENGTH, flags) == 1)
    {
        cntl = FLD_SET_DRF(_PMGR, _I2C_CNTL, _GEN_RAB, _YES, cntl);
        addr = FLD_SET_DRF_NUM(_PMGR, _I2C_ADDR, _RAB,
                               pTa->index[0], addr);
    }
    else // preconditions imply 'indexLength' == 0
    {
        cntl = FLD_SET_DRF(_PMGR, _I2C_CNTL, _GEN_RAB, _NO, cntl);
    }

    //
    // If block protocol, the size is the first byte, so we'll need to read
    // expected size + 1 bytes total.
    //
    if (FLD_TEST_DRF(_RM_FLCN, _I2C_FLAGS, _BLOCK_PROTOCOL, _ENABLED,
                     flags))
    {
        bytesRemaining++;
    }

    if (bytesRemaining <= LW_PMGR_I2C_CNTL_BURST_SIZE_MAXIMUM)
    {
        cntl = FLD_SET_DRF(_PMGR, _I2C_CNTL, _GEN_STOP, _YES, cntl);
        pCmd->bStopRequired = LW_FALSE;
    }
    else // bytesRemaining > LW_PMGR_I2C_CNTL_BURST_SIZE_MAXIMUM
    {
        cntl = FLD_SET_DRF(_PMGR, _I2C_CNTL, _GEN_STOP, _NO, cntl);
        pCmd->bStopRequired = LW_TRUE;
    }

    // Value bytesRemaining does not decrease until the data is actually read.
    bytesToRead = LW_MIN(bytesRemaining, LW_PMGR_I2C_CNTL_BURST_SIZE_MAXIMUM);

    cntl = FLD_SET_DRF_NUM(_PMGR, _I2C_CNTL, _BURST_SIZE, bytesToRead, cntl);

    //
    // Command submission.
    //

    if (bSendAddress)
    {
        PMGR_REG_WR32_ERR_EXIT(LW_PMGR_I2C_ADDR(portId), addr);
    }

    PMGR_REG_WR32_ERR_EXIT(LW_PMGR_I2C_CNTL(portId), cntl);

    //
    // Initialize next command.
    //

    cntl = FLD_SET_DRF(_PMGR, _I2C_CNTL, _GEN_START, _NO, cntl);
    cntl = FLD_SET_DRF(_PMGR, _I2C_CNTL, _GEN_RAB, _NO, cntl);

    pCmd->portId = portId;
    pCmd->bRead = LW_TRUE;
    pCmd->cntl = cntl;
    pCmd->data = data;
    pCmd->bytesRemaining = bytesRemaining;
    pCmd->pMessage = pMessage;
    pCmd->status = FLCN_ERR_I2C_BUSY;

    if (FLD_TEST_DRF(_RM_FLCN, _I2C_FLAGS, _BLOCK_PROTOCOL, _ENABLED,
                        flags))
    {
        pCmd->bBlockProtocol = LW_TRUE;
    }
    else
    {
        pCmd->bBlockProtocol = LW_FALSE;
    }

ErrorExit:
    return flcnStatus;
}

/*!
 * Read next byte.
 *
 * @param[in, out] pTa      The transaction description.
 * @param[in, out] pCmd     The continuous command structure.
 *
 * @return The FLCN_ERR_<xyz> equivalent status.
 */
FLCN_STATUS
i2cHwReadNext_GM20X
(
    I2C_TRANSACTION *pTa,
    I2C_HW_CMD      *pCmd
)
{
    FLCN_STATUS flcnStatus  = FLCN_ERROR;
    LwU32       cntl        = 0;
    LwS32       i           = LW_S32_MAX;
    LwU8        messageSize;

    PMGR_REG_RD32_ERR_EXIT(LW_PMGR_I2C_CNTL(pCmd->portId), &cntl);
    flcnStatus = _i2cHwStatusToI2cStatus(DRF_VAL(_PMGR, _I2C_CNTL, _STATUS, cntl));
    if (flcnStatus == FLCN_OK)
    {
        PMGR_REG_RD32_ERR_EXIT(LW_PMGR_I2C_DATA(pCmd->portId), &pCmd->data);

        // Migrate "data" from dword into respective bytes.
        i = LW_MIN(pCmd->bytesRemaining,
                   LW_PMGR_I2C_CNTL_BURST_SIZE_MAXIMUM) - 1;

        //
        // Block Protocol - read the size byte which should be the first byte in
        // the payload/message.
        //
        if (pCmd->bBlockProtocol)
        {
            messageSize = (pCmd->data >> (i * 8)) & 0xFF;

            if (messageSize != pTa->messageLength)
            {
                flcnStatus = FLCN_ERR_I2C_SIZE;
            }

            pCmd->bBlockProtocol = LW_FALSE;
            i--;
            pCmd->bytesRemaining--;
        }


        for (; i >= 0; i--)
        {
            *pCmd->pMessage = (pCmd->data >> (i * 8)) & 0xFF;
            pCmd->pMessage++;
            pCmd->bytesRemaining--;
        }
    }

    if (flcnStatus == FLCN_OK)
    {
        if (pCmd->bytesRemaining > 0)
        {
            i2cHwReadPrep_HAL(&I2c, pCmd, pCmd->bytesRemaining);

            PMGR_REG_WR32_ERR_EXIT(LW_PMGR_I2C_CNTL(pCmd->portId), pCmd->cntl);
        }
        else // pCmd->bytesRemaining == 0
        {
            pCmd->status = FLCN_OK;
        }
    }

ErrorExit:
    return flcnStatus;
}

/*!
 * Colwert a HW status to an FLCN_ERR_<xyz>.
 *
 * @param[in] hwStatus      The status from hardware.
 *
 * @return The FLCN_ERR_<xyz> equivalent status.
 */
static FLCN_STATUS
_i2cHwStatusToI2cStatus
(
    LwU32 hwStatus
)
{
    FLCN_STATUS status = FLCN_ERROR;

    if (hwStatus == LW_PMGR_I2C_CNTL_STATUS_OKAY)
    {
        status = FLCN_OK;
    }
    else if (hwStatus == LW_PMGR_I2C_CNTL_STATUS_NO_ACK)
    {
        status = FLCN_ERR_I2C_NACK_BYTE;
    }
    else if (hwStatus == LW_PMGR_I2C_CNTL_STATUS_TIMEOUT)
    {
        status = FLCN_ERR_TIMEOUT;
    }

    return status;
}

#if defined(DPU_RTOS) || defined(GSP_RTOS)
/*!
 * Drive the SCL/SDA pin defined by pos bit to High/Low as per bValue
 *
 * @param[in] pBus     The Bus whose lines we need to drive
 * @param[in] pos      The bit position of SCL/SDA lines
 * @param[in] bValue   Whether to drive lines High/Low
 */
void
i2cDrive_GM20X
(
    I2C_SW_BUS *const pBus,
    LwU32       pos,
    LwBool      bValue
)
{
    pBus->regCache  = FLD_SET_DRF(_PMGR, _I2C_OVERRIDE, _SIGNALS, _ENABLE, 
                                  pBus->regCache);
    pBus->regCache  = SET_BIT(pBus->regCache, pos, bValue);

    // BAR0 access always succeed for preVolta, the func is STUB for Volta+.
    PMGR_REG_WR32(LW_PMGR_I2C_OVERRIDE(pBus->port), pBus->regCache);
}

/*!
 * Read the SDA/SCL lines at bit position pos
 *
 * @param[in] pBus      The Bus whose lines we need to read
 * @param[in] pos       The bit position of SCL/SDA lines to read
 *
 * @return 'LW_TRUE' if line is High, else 'LW_FALSE'.
 */
LwBool
i2cRead_GM20X
(
    I2C_SW_BUS *const pBus,
    LwU32       pos
)
{
    // BAR0 access always succeed for preVolta, the func is STUB for Volta+.
    LwU32 reg32 = PMGR_REG_RD32(LW_PMGR_I2C_OVERRIDE(pBus->port));

    return (LwBool)GET_BIT(reg32, pos);
}
#endif

/*!
 * Initialize the I2C lines
 *
 * @param[in] pBus      The Bus whose lines we need to initialize
 */
void
i2cInitSwi2c_GM20X
(
    I2C_SW_BUS *const pBus
)
{
    pBus->sclOut = DRF_BASE(LW_PMGR_I2C_OVERRIDE_SCL);
    pBus->sdaOut = DRF_BASE(LW_PMGR_I2C_OVERRIDE_SDA);

    pBus->sclIn = DRF_BASE(LW_PMGR_I2C_OVERRIDE_SCLPAD_IN);
    pBus->sdaIn = DRF_BASE(LW_PMGR_I2C_OVERRIDE_SDAPAD_IN);

    pBus->regCache = 0;
    pBus->regCache = SET_BIT(pBus->regCache, pBus->sclOut, 1);
    pBus->regCache = SET_BIT(pBus->regCache, pBus->sdaOut, 1);
    pBus->regCache = SET_BIT(pBus->regCache, DRF_BASE(LW_PMGR_I2C_OVERRIDE_SIGNALS), 1);
    pBus->lwrLine  = pBus->sclIn;
};

