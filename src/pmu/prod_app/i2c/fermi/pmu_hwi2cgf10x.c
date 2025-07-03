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
 * @file pmu_hwi2cgf10x.c
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_pwr_csb.h"
#include "dev_pmgr.h"
#include "dev_pmgr_addendum.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmu.h"
#include "pmu_objtimer.h"
#include "pmu_obji2c.h"
#include "dbgprintf.h"
#include "task_i2c.h"
#include "config/g_i2c_private.h"
#include "flcnifi2c.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */

/*!
 * The following defines are used to determine how long to wait for another
 * I2C transaction to finish before failing.
 */
#define I2C_HW_IDLE_TIMEOUT_US      20000000

/*!
 * The I2C specification does not specify any timeout conditions for clock
 * stretching, i.e. any device can hold down SCL as long as it likes so this
 * value needs to be adjusted on case by case basis.
 */
#define I2C_SCL_CLK_TIMEOUT_1200US  1200

#define I2C_SCL_CLK_TIMEOUT_400KHZ (I2C_SCL_CLK_TIMEOUT_100KHZ * 4)
#define I2C_SCL_CLK_TIMEOUT_300KHZ (I2C_SCL_CLK_TIMEOUT_100KHZ * 3)
#define I2C_SCL_CLK_TIMEOUT_200KHZ (I2C_SCL_CLK_TIMEOUT_100KHZ * 2)
#define I2C_SCL_CLK_TIMEOUT_100KHZ (I2C_SCL_CLK_TIMEOUT_1200US / 10)

/*!
 * We don't want PMU I2C to deal with traffic slower than 20 KHz (50 us cycle).
 */
#define I2C_MAX_CYCLE_US 50

/*!
 * The longest HW I2C transaction: S BYTE*2 S BYTE*4 P, at 1 each for S/P, and
 * 9 for each byte (+ack).
 */
#define I2C_HW_MAX_CYCLES ((1 * 3) + (9 * 6))

/*!
 * We determine the HW operational timeout as the longest operation, plus two
 * long SCL clock stretches.
 */
#define I2C_HW_IDLE_TIMEOUT_NS (1000 * \
    ((I2C_MAX_CYCLE_US * I2C_HW_MAX_CYCLES) + (I2C_SCL_CLK_TIMEOUT_1200US * 2)))

#define I2C_EDDC_SEGMENT_POINTER_ADDR   0x60
/* ------------------------- Prototypes ------------------------------------- */
static FLCN_STATUS  s_i2cHwStatusToI2cStatus_GMXXX(LwU32 hwStatus);
static void         s_i2cHwReadPrep_GMXXX(I2C_HW_CMD *pCmd, LwU32 bytesRemaining);
static void         s_i2cHwShortWrite_GMXXX(I2C_PMU_TRANSACTION *pTa);

/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- Private Functions ------------------------------ */

/*!
 * Set the speed of the HW I2C controller on a given port.
 *
 * @param[in] port          The port identifying the controller.
 *
 * @param[in] speedMode     The speed mode to run at.
 */
void
i2cHwSetSpeedMode_GMXXX
(
    LwU32 port,
    LwU32 speedMode
)
{

    LwU32 timing = DRF_DEF(_PMGR, _I2C_TIMING, _IGNORE_ACK, _DISABLE) |
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

#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_I2C_200KHZ))
        case LW_RM_FLCN_I2C_FLAGS_SPEED_MODE_200KHZ:
            timing = FLD_SET_DRF(_PMGR, _I2C_TIMING, _SCL_PERIOD, _200KHZ,
                                 timing);
            timing = FLD_SET_DRF_NUM(_PMGR, _I2C_TIMING, _TIMEOUT_CLK_CNT,
                                     I2C_SCL_CLK_TIMEOUT_200KHZ, timing);
            break;
#endif // PMUCFG_FEATURE_ENABLED(PMU_PMGR_I2C_200KHZ)

#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_I2C_300KHZ))
        case LW_RM_FLCN_I2C_FLAGS_SPEED_MODE_300KHZ:
            timing = FLD_SET_DRF(_PMGR, _I2C_TIMING, _SCL_PERIOD, _300KHZ,
                                 timing);
            timing = FLD_SET_DRF_NUM(_PMGR, _I2C_TIMING, _TIMEOUT_CLK_CNT,
                                     I2C_SCL_CLK_TIMEOUT_300KHZ, timing);
            break;
#endif // PMUCFG_FEATURE_ENABLED(PMU_PMGR_I2C_300KHZ)

#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_I2C_400KHZ))
        case LW_RM_FLCN_I2C_FLAGS_SPEED_MODE_400KHZ:
            timing = FLD_SET_DRF(_PMGR, _I2C_TIMING, _SCL_PERIOD, _400KHZ,
                                 timing);
            timing = FLD_SET_DRF_NUM(_PMGR, _I2C_TIMING, _TIMEOUT_CLK_CNT,
                                     I2C_SCL_CLK_TIMEOUT_400KHZ, timing);
            break;
#endif // PMUCFG_FEATURE_ENABLED(PMU_PMGR_I2C_400KHZ)

    }

    REG_WR32(BAR0, LW_PMGR_I2C_TIMING(port), timing);
}

/*!
 * @brief Wait for the HW I2C controller to idle, or until an internal timeout.
 *
 * @param[in] port The port of the HW I2C controller.
 *
 * @return FLCN_OK, on success.
 * @return FLCN_ERR_TIMEOUT, if the timeout is exceeded.
 *
 * Note: Even if the command times out in hardware, FLCN_OK will be
 *       returned.  The idea for this function is to prevent collisions on the
 *       bus, not to report the status of a previous command.
 */
FLCN_STATUS
i2cHwWaitForIdle_GMXXX
(
    LwU32 port
)
{
    FLCN_STATUS status = FLCN_OK;

    if (!PMU_REG32_POLL_NS(
             LW_PMGR_I2C_CNTL(port),
             DRF_SHIFTMASK(LW_PMGR_I2C_CNTL_CYCLE),
             DRF_DEF(_PMGR, _I2C_CNTL, _CYCLE, _DONE),
             I2C_HW_IDLE_TIMEOUT_NS,
             PMU_REG_POLL_MODE_BAR0_EQ))
    {
        status = FLCN_ERR_TIMEOUT;
        goto i2cHwWaitForIdle_GMXXX_exit;
    }

i2cHwWaitForIdle_GMXXX_exit:
    return status;
}

/*!
 * Colwert a HW status to an FLCN_I2C_ERR_<xyz>.
 *
 * @param[in] hwStatus      The status from hardware.
 *
 * @return FLCN_STATUS      FLCN_I2C_ERR_<xyz>.
 */
static FLCN_STATUS
s_i2cHwStatusToI2cStatus_GMXXX
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

/*!
 * Send a start in manual mode.
 *
 * @param[in] portId            The port identifier.
 *
 * @return FLCN_STATUS
 */
FLCN_STATUS
i2cSendStartViaHw_GMXXX
(
    LwU32 portId
)
{
    LwU32 cntl = DRF_DEF(_PMGR, _I2C_CNTL, _GEN_START, _YES) |
                 DRF_DEF(_PMGR, _I2C_CNTL, _INTR_WHEN_DONE, _YES);
 
    REG_WR32(BAR0, LW_PMGR_I2C_CNTL(portId), cntl);

    return i2cHwWaitForIdle_HAL(&I2c, portId);
}

/*!
 * Write a byte in manual mode.
 *
 * @param[in] portId            The port identifier.
 *
 * @param[in] data              The byte to write.
 *
 * @return FLCN_STATUS
 */
FLCN_STATUS
i2cWriteByteViaHw_GMXXX
(
    LwU32 portId,
    LwU32 data
)
{
    LwU32 cntl = DRF_DEF(_PMGR, _I2C_CNTL, _CMD, _WRITE) |
                 DRF_DEF(_PMGR, _I2C_CNTL, _INTR_WHEN_DONE, _YES)   |
                 DRF_NUM(_PMGR, _I2C_CNTL, _BURST_SIZE, 1);

    REG_WR32(BAR0, LW_PMGR_I2C_DATA(portId), data);
    REG_WR32(BAR0, LW_PMGR_I2C_CNTL(portId), cntl);

    return i2cHwWaitForIdle_HAL(&I2c, portId);
}

/*!
 * Send a stop in manual mode.
 *
 * @param[in] portId            The port identifier.
 *
 * @return FLCN_STATUS
 */
FLCN_STATUS
i2cSendStopViaHw_GMXXX
(
    LwU32 portId
)
{
    LwU32 cntl = DRF_DEF(_PMGR, _I2C_CNTL, _GEN_STOP, _YES) |
                 DRF_DEF(_PMGR, _I2C_CNTL, _INTR_WHEN_DONE, _YES);


    REG_WR32(BAR0, LW_PMGR_I2C_CNTL(portId), cntl);

    return i2cHwWaitForIdle_HAL(&I2c, portId);
}


/*!
 * Send an address in manual mode.
 *
 * @param[in] pTa               The transaction description.
 *
 * @param[in] bSending          The read/write bit to use with the address.
 *
 * @return FLCN_STATUS
 */
FLCN_STATUS
i2cSendAddressViaHw_GMXXX
(
    I2C_PMU_TRANSACTION *pTa,
    LwU32                bSending
)
{
    LwU32       flags   = pTa->pGenCmd->flags;
    LwU32       portId  = DRF_VAL(_RM_FLCN, _I2C_FLAGS, _PORT, flags);
    LwU32       address = pTa->pGenCmd->i2cAddress;
    FLCN_STATUS status;

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
            status = i2cWriteByteViaHw_HAL(&I2c, portId, (((address >> 9) << 1) & 0xF7) |
                     0xF0 | !bSending);

            if (status == FLCN_OK)
            {
                status = i2cWriteByteViaHw_HAL(&I2c, portId, (address >> 1) & 0xFF);
            }
            return status;
    }
}

/*!
 * Send a self-contained write command.
 *
 * @param[in,out] pTa           The transaction description.
 *
 * @return void
 */
static void
s_i2cHwShortWrite_GMXXX
(
    I2C_PMU_TRANSACTION *pTa
)
{
    LwU32  flags  = pTa->pGenCmd->flags;
    LwU32  portId = DRF_VAL(_RM_FLCN, _I2C_FLAGS, _PORT, flags);
    LwU32  addr   = 0;
    LwU32  cntl   = 0;
    LwU32  data   = 0;
    LwS32  i      = 0;

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
            addr = DRF_NUM(_PMGR, _I2C_ADDR, _DAB,
                           pTa->pGenCmd->i2cAddress >> 1) |
                   DRF_DEF(_PMGR, _I2C_ADDR, _TEN_BIT, _DISABLE);
            break;

        case LW_RM_FLCN_I2C_FLAGS_ADDRESS_MODE_10BIT:
            addr = DRF_NUM(_PMGR, _I2C_ADDR, _DAB,
                           pTa->pGenCmd->i2cAddress >> 1) |
                   DRF_DEF(_PMGR, _I2C_ADDR, _TEN_BIT, _ENABLE);
            break;
    }

    if (DRF_VAL(_RM_FLCN, _I2C_FLAGS, _INDEX_LENGTH, flags) == 1)
    {
        cntl = FLD_SET_DRF(_PMGR, _I2C_CNTL, _GEN_RAB, _YES, cntl);
        addr = FLD_SET_DRF_NUM(_PMGR, _I2C_ADDR, _RAB, pTa->pGenCmd->index[0],
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

    REG_WR32(BAR0, LW_PMGR_I2C_ADDR(portId), addr);
    REG_WR32(BAR0, LW_PMGR_I2C_DATA(portId), data);
    REG_WR32(BAR0, LW_PMGR_I2C_CNTL(portId), cntl);
}

/*!
 * Start off a write.
 *
 * @param[in, out] pTa      The transaction description.
 *
 * @param[out]     pCmd     The continuous command structure.
 *
 * @return FLCN_STATUS      Status of type FLCN_ERR_I2C_<xyz>
 */
FLCN_STATUS
i2cHwInitCmdWrite_GMXXX
(
    I2C_PMU_TRANSACTION *pTa,
    I2C_HW_CMD          *pCmd
)
{
    LwU32       flags       = pTa->pGenCmd->flags;
    LwU32       portId      = DRF_VAL(_RM_FLCN, _I2C_FLAGS, _PORT, flags);
    LwU32       indexLength = DRF_VAL(_RM_FLCN, _I2C_FLAGS, _INDEX_LENGTH, flags);
    FLCN_STATUS status      = FLCN_ERROR;
    LwU32       i;

    pCmd->portId    = portId;
    pCmd->bRead     = LW_FALSE;
    pCmd->pMessage  = pTa->pMessage;
    pCmd->status    = FLCN_ERR_I2C_BUSY;

    if ((DRF_VAL(_RM_FLCN, _I2C_FLAGS, _INDEX_LENGTH, flags) <= 1) &&
        (pTa->messageLength < LW_PMGR_I2C_CNTL_BURST_SIZE_MAXIMUM))
    {
        s_i2cHwShortWrite_GMXXX(pTa);
        pCmd->cntl = 0;
        pCmd->data = 0;
        pCmd->bytesRemaining = 0;
        pCmd->bStopRequired = LW_FALSE;

        return FLCN_OK;
    }

    // Long write.
    // Perform the initial operations we need before sending the message.
    status = i2cSendStartViaHw_HAL(&I2c, portId);

    if (status == FLCN_OK)
    {
        status = i2cSendAddressViaHw_HAL(&I2c, pTa, LW_TRUE);
    }

    for (i = 0; (status == FLCN_OK) && (i < indexLength); i++)
    {
        status = i2cWriteByteViaHw_HAL(&I2c, portId, pTa->pGenCmd->index[i]);
    }

    if ((status == FLCN_OK) &&
        FLD_TEST_DRF(_RM_FLCN, _I2C_FLAGS, _RESTART, _SEND, flags))
    {
        status = i2cSendStartViaHw_HAL(&I2c, portId);
        if (status == FLCN_OK)
        {
            (void)i2cSendAddressViaHw_HAL(&I2c, pTa, LW_TRUE);
        }
    }

    // Issue the first byte of the message.
    if (pTa->messageLength == 0)
    {
        pCmd->data = 0;
        pCmd->cntl = DRF_DEF(_PMGR, _I2C_CNTL, _GEN_STOP, _YES) |
                     DRF_DEF(_PMGR, _I2C_CNTL, _INTR_WHEN_DONE, _YES);
        pCmd->bytesRemaining = 0;
        pCmd->bStopRequired  = LW_FALSE;
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

            REG_WR32(BAR0, LW_PMGR_I2C_DATA(portId), pCmd->data);
            REG_WR32(BAR0, LW_PMGR_I2C_CNTL(portId), pCmd->cntl);

            //
            // Wait for write to complete.  Ignoring the status for now because
            // these writes below are always happening, even if everything above
            // failed!
            //
            (void)i2cHwWaitForIdle_HAL(&I2c, portId);
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
    REG_WR32(BAR0, LW_PMGR_I2C_DATA(portId), pCmd->data);
    REG_WR32(BAR0, LW_PMGR_I2C_CNTL(portId), pCmd->cntl);

    if (pCmd->bytesRemaining > 0)
    {
        pCmd->data = *pCmd->pMessage;
        pCmd->pMessage++;
    }

    // Why are we ignoring status?

    return FLCN_OK;
}

FLCN_STATUS
i2cHwWriteNext_GMXXX
(
    I2C_HW_CMD *pCmd
)
{
    LwU32       cntl   = REG_RD32(BAR0, LW_PMGR_I2C_CNTL(pCmd->portId));
    FLCN_STATUS status = FLCN_ERROR;

    status = s_i2cHwStatusToI2cStatus_GMXXX(DRF_VAL(_PMGR, _I2C_CNTL, _STATUS, cntl));

    // Get the next command running ASAP to reduce latency.
    if ((status == FLCN_OK) && (pCmd->bytesRemaining > 0))
    {
        REG_WR32(BAR0, LW_PMGR_I2C_DATA(pCmd->portId), pCmd->data);
        REG_WR32(BAR0, LW_PMGR_I2C_CNTL(pCmd->portId), pCmd->cntl);
        pCmd->bytesRemaining--;
    }

    if (status == FLCN_OK)
    {
        if (pCmd->bytesRemaining > 0)
        {
            pCmd->data = *pCmd->pMessage;
            pCmd->pMessage++;
        }
        else
        {
            pCmd->status = status;
        }
    }

    return status;
}

/*!
 * Prepare next read command in sequence.
 *
 * @param[in,out]   pCmd            The command data.
 *
 * @param[in]       bytesRemaining  The number of bytes remaining.
 *
 * @return void
 */
static void
s_i2cHwReadPrep_GMXXX
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
 * @param[in, out] pTa        The transaction description.
 *
 * @param[in, out] pCmd       The continuous command structure.
 *
 * @params[in] genStop        To issue Stop explicitly
 *
 * @params[in] bIndexPresent  To issue RAB or Not
 *
 * @params[in] bytesRemaining Bytes remaining to be read   
 */
void
i2cHwInitCmdReadWithShortIndex_GMXXX
(
    I2C_PMU_TRANSACTION *pTa,
    I2C_HW_CMD          *pCmd,
    LwBool               bIndexPresent,
    LwBool               genStop,
    LwU32                bytesRemaining
)
{
    LwU32 flags = pTa->pGenCmd->flags;
    LwU32 cntl = 0;
    LwU32 data = 0;
    LwU32 addr = 0;
    LwU32 bytesToRead = LW_U32_MAX;
    LwU32 portId = DRF_VAL(_RM_FLCN, _I2C_FLAGS, _PORT, flags);
    LwU32 address = pTa->pGenCmd->i2cAddress;
    LwU32 bSendAddress = LW_FALSE;

    //
    // Determine initial command.
    //
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

    if (FLD_TEST_DRF(_RM_FLCN, _I2C_FLAGS, _INDEX_LENGTH, _ONE, flags) && 
                            bIndexPresent)
    {
        cntl = FLD_SET_DRF(_PMGR, _I2C_CNTL, _GEN_RAB, _YES, cntl);
        addr = FLD_SET_DRF_NUM(_PMGR, _I2C_ADDR, _RAB,
                               pTa->pGenCmd->index[0], addr);
    }
    else // preconditions imply 'indexLength' == 0
    {
        cntl = FLD_SET_DRF(_PMGR, _I2C_CNTL, _GEN_RAB, _NO, cntl);
    }

    //
    // If block protocol, the size is the first byte, so we'll need to read
    // expected size + 1 bytes total.
    //
    if (FLD_TEST_DRF(_RM_FLCN, _I2C_FLAGS, _BLOCK_PROTOCOL, _ENABLED, flags))
    {
        bytesRemaining++;
    }

    if ((bytesRemaining <= LW_PMGR_I2C_CNTL_BURST_SIZE_MAXIMUM) || genStop)
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
        REG_WR32(BAR0, LW_PMGR_I2C_ADDR(portId), addr);
    }

    REG_WR32(BAR0, LW_PMGR_I2C_CNTL(portId), cntl);

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
    pCmd->status = FLCN_ERR_I2C_BUSY;
    if (FLD_TEST_DRF(_RM_FLCN, _I2C_FLAGS, _BLOCK_PROTOCOL, _ENABLED, flags))
    {
        pCmd->bBlockProtocol = LW_TRUE;
    }
    else
    {
        pCmd->bBlockProtocol = LW_FALSE;
    }
}

// Reads the byte and initialize next command 
FLCN_STATUS
i2cHwReadNext_GMXXX
(
    I2C_PMU_TRANSACTION *pTa,
    I2C_HW_CMD          *pCmd,
    LwBool               bReadNext
)
{
    LwU32       cntl   = REG_RD32(BAR0, LW_PMGR_I2C_CNTL(pCmd->portId));
    LwS32       i      = LW_S32_MAX;
    FLCN_STATUS status = FLCN_ERROR;
    LwU8  messageSize;

    status = s_i2cHwStatusToI2cStatus_GMXXX(DRF_VAL(_PMGR, _I2C_CNTL, _STATUS, cntl));

    if (status == FLCN_OK)
    {
        pCmd->data = REG_RD32(BAR0, LW_PMGR_I2C_DATA(pCmd->portId));

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
                status = FLCN_ERR_I2C_SIZE;
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

    if (status == FLCN_OK)
    {
        if ((pCmd->bytesRemaining > 0) && bReadNext)
        {
            s_i2cHwReadPrep_GMXXX(pCmd, pCmd->bytesRemaining);

            REG_WR32(BAR0, LW_PMGR_I2C_CNTL(pCmd->portId), pCmd->cntl);
        }
        else // pCmd->bytesRemaining == 0
        {
            pCmd->status = FLCN_OK;
        }
    }

    return status;
}

/*!
 * @brief Wait for the HW I2C controller to idle, or until an internal timeout.
 *
 * @param[in] port The port of the HW I2C controller.
 *
 * @return FLCN_OK
 *     upon success
 *
 * @return FLCN_ERR_I2C_BUSY
 *     if the timeout is exceeded.
 *
 * Note: Even if the command times out in hardware, FLCN_OK will be returned.
 *       The idea for this function is to prevent collisions on the bus, not
 *       to report the status of a previous command.
 */
FLCN_STATUS
i2cWaitForHwIdle_GMXXX
(
    LwU32 port
)
{
    FLCN_TIMESTAMP  startTime;
    LwU32           cntlReg = LW_PMGR_I2C_CNTL(port);
    LwU32           cntl;
    LwU32           elapsedTime = 0;

    osPTimerTimeNsLwrrentGet(&startTime);
    cntl = REG_RD32(BAR0, cntlReg);

    while ((!FLD_TEST_DRF(_PMGR, _I2C_CNTL, _CYCLE, _DONE, cntl)) &&
           (!FLD_TEST_DRF(_PMGR, _I2C_CNTL, _STATUS, _TIMEOUT, cntl)) &&
           (elapsedTime < I2C_HW_IDLE_TIMEOUT_US))
    {
        lwrtosYIELD();
        cntl = REG_RD32(BAR0, cntlReg);
        elapsedTime = osPTimerTimeNsElapsedGet(&startTime);
    }

    return (elapsedTime < I2C_HW_IDLE_TIMEOUT_US) ?
                FLCN_OK : FLCN_ERR_I2C_BUSY;
}

/*!
 * Reset the HW engine and wait for it to idle.
 *
 * @param[in] port              The port identifier.
 *
 * @return FLCN_OK
 *     upon success
 *
 * @return FLCN_ERR_I2C_BUSY
 *     if the timeout is exceeded.
 */
FLCN_STATUS
i2cHwReset_GMXXX
(
    LwU32 port
)
{
    LwU32 cntl = DRF_DEF(_PMGR, _I2C_CNTL, _CMD, _RESET);

    REG_WR32(BAR0, LW_PMGR_I2C_CNTL(port), cntl);

    return i2cWaitForHwIdle_HAL(&I2c, port);
}

/*!
 * Issue I2C_WRITEBYTE_WAIT for EDDC Segment
 * 
 * Sequence :   |S| eddc_segment_addr[7:1] |wr|A| segment_value |A|
 * 
 * @param[in] portId            The port identifier.
 *
 * @return FLCN_OK
 *     upon success
 *
 * @return FLCN_ERR_I2C_BUSY
 *     if the timeout is exceeded.
 */
FLCN_STATUS
i2cHwWriteByteWaitEddcSegment_GMXXX
(
    I2C_PMU_TRANSACTION *pTa
)
{
    LwU32  flags  = pTa->pGenCmd->flags;
    LwU32  portId = DRF_VAL(_RM_FLCN, _I2C_FLAGS, _PORT, flags);
    LwU32  cntl   = 0;
    LwU32  data   = 0;
    LwU32  addr   = 0;

    cntl = DRF_DEF(_PMGR, _I2C_CNTL, _GEN_START, _YES)                |
           DRF_DEF(_PMGR, _I2C_CNTL, _GEN_STOP, _NO)                  |
           DRF_DEF(_PMGR, _I2C_CNTL, _CMD, _WRITE)                    |
           DRF_NUM(_PMGR, _I2C_CNTL, _BURST_SIZE, 1)                  |
           DRF_DEF(_PMGR, _I2C_CNTL, _GEN_RAB, _NO)                   |
           DRF_DEF(_PMGR, _I2C_CNTL, _INTR_WHEN_DONE, _YES)           |
           DRF_DEF(_PMGR, _I2C_CNTL, _GEN_NACK, _NO);

    switch (DRF_VAL(_RM_FLCN, _I2C_FLAGS, _ADDRESS_MODE, flags))
    {
        default:
        case LW_RM_FLCN_I2C_FLAGS_ADDRESS_MODE_NO_ADDRESS:
            return FLCN_ERROR;
            break;

        case LW_RM_FLCN_I2C_FLAGS_ADDRESS_MODE_7BIT:
            addr = DRF_NUM(_PMGR, _I2C_ADDR, _DAB,
                           I2C_EDDC_SEGMENT_POINTER_ADDR >> 1) |
                   DRF_DEF(_PMGR, _I2C_ADDR, _TEN_BIT, _DISABLE);
            break;

        case LW_RM_FLCN_I2C_FLAGS_ADDRESS_MODE_10BIT:
            addr = DRF_NUM(_PMGR, _I2C_ADDR, _DAB,
                           I2C_EDDC_SEGMENT_POINTER_ADDR >> 1) |
                   DRF_DEF(_PMGR, _I2C_ADDR, _TEN_BIT, _ENABLE);
            break;
    }

    // Put the segment pointer value
    data = pTa->segment;

    REG_WR32(BAR0, LW_PMGR_I2C_ADDR(portId), addr);
    REG_WR32(BAR0, LW_PMGR_I2C_DATA(portId), data);
    REG_WR32(BAR0, LW_PMGR_I2C_CNTL(portId), cntl);

    return i2cWaitForHwIdle_HAL(&I2c, portId);
}
