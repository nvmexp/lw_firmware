/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2007-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "i2c_obji2c.h"
#include "lib_intfci2c.h"
#include "config/g_i2c_private.h"
#include "config/i2c-config.h"
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Global Variables ------------------------------- */

/*! Prototypes to explicitly mark as USED */
static void _i2cDriveAndDelay(I2C_SW_BUS *const pBus, LwU32 pos,LwBool  bValue, LwU32 delayNs)
    GCC_ATTRIB_SECTION("imem_libI2cSw", "_i2cDriveAndDelay");
static FLCN_STATUS _i2cSendStart(I2C_SW_BUS *const pBus)
    GCC_ATTRIB_SECTION("imem_libI2cSw", "_i2cSendStart");
static FLCN_STATUS _i2cSendRestart(I2C_SW_BUS *const pBus)
    GCC_ATTRIB_SECTION("imem_libI2cSw", "_i2cSendRestart");
static FLCN_STATUS _i2cSendStop(I2C_SW_BUS *const pBus)
    GCC_ATTRIB_SECTION("imem_libI2cSw", "_i2cSendStop");
static FLCN_STATUS _i2cWriteByte(I2C_SW_BUS *const pBus, LwU8 *const pData, const LwBool ignored)
    GCC_ATTRIB_SECTION("imem_libI2cSw", "_i2cWriteByte");
static FLCN_STATUS _i2cReadByte(I2C_SW_BUS *const pBus, LwU8 *const pData, const LwBool bLast)
    GCC_ATTRIB_SECTION("imem_libI2cSw", "_i2cReadByte");
static FLCN_STATUS _i2cProcessByte(I2C_SW_BUS *const pBus,LwU8 *const pData, const LwBool bLast, LwBool bRead)
    GCC_ATTRIB_SECTION("imem_libI2cSw", "_i2cProcessByte");
static FLCN_STATUS _i2cSendAddress(const I2C_TRANSACTION*const pTa, I2C_SW_BUS *const pBus, const LwBool bRead)
    GCC_ATTRIB_SECTION("imem_libI2cSw", "_i2cSendAddress");
static void _i2cBusAndSpeedInit(I2C_SW_BUS *pBus,const LwU8 speedMode, LwU32 portId)
    GCC_ATTRIB_SECTION("imem_libI2cSw", "_i2cBusAndSpeedInit");
static LwBool _i2cIsLineHigh(I2C_SW_BUS *pBus)
    GCC_ATTRIB_USED()
    GCC_ATTRIB_SECTION("imem_libI2cSw", "_i2cIsLineHigh");
static LwBool _i2cWaitSclReleasedTimedOut(I2C_SW_BUS *pBus)
    GCC_ATTRIB_SECTION("imem_libI2cSw", "_i2cWaitSclReleasedTimedOut");
/*!
 * Drive the SCL/SDA pin defined by pos bit to High/Low as per bValue
 *
 * @param[in] pBus      The Bus whose lines we need to drive
 *
 * @param[in] pos       The bit position of SCL/SDA lines
 *
 * @param[in] bValue    Whether to drive lines High/Low
 */
static void
_i2cDriveAndDelay
(
    I2C_SW_BUS *const pBus,
    LwU32             pos,
    LwBool            bValue,
    LwU32             delayNs
)
{
    i2cDrive_HAL(&I2c, pBus, pos, bValue);
    if (delayNs != 0)
    {
        I2C_DELAY(delayNs);
    }
}

/*!
 * @brief   Check the physical status of a line for a particular I2C bus.
 *
 * @param[in] pLine
 *          An integer representing an I2C line as determined previously from
 *          i2cGetScl(port) or i2cGetSda(port).  Note that pLine is really an
 *          integer, but since this is used as a callback it must meet the
 *          function type specification.
 *
 * @returns The state of the I2C line, with LW_FALSE representing low and
 *          LW_TRUE representing high.  The type is determined by the functional
 *          type specification of the callback for which this function is used.
 */

static LwBool
_i2cIsLineHigh
(
   I2C_SW_BUS *pBus
)
{
    LwBool status = LW_TRUE;

    if(pBus->lwrLine == pBus->sclIn)
    {
        status = I2C_READ(pBus, pBus->sclIn);
    }
    else if(pBus->lwrLine == pBus->sdaIn)
    {
        status = I2C_READ(pBus, pBus->sdaIn);
    }

    return status;
}


/*!
 * @brief   Send an initial start signal along the bus.
 *
 * @param[in] pBus
 *          The bus along which to send the start signal.
 *
 * @returns FLCN_OK
 *
 * @section Preconditions
 *  - The bus must not be busy.
 *  - Both SCL and SDA must be high.
 *  - If the most recent bus activity was a stop signal, then the sum of tR and
 *    tBuf must have elapsed since that event.
 *
 * @section Postconditions
 *  - A start signal will have been sent along the bus.
 *  - SCL will be high and SDA will be low.
 *  - At least tF + tHdSta will have elapsed since the start signal was sent.
 */
static FLCN_STATUS
_i2cSendStart
(
    I2C_SW_BUS *const pBus
)
{
    // A start signal is to pull down SDA while SCL is high.
    _i2cDriveAndDelay(pBus, pBus->sdaOut, LOW, pBus->tF + pBus->tHdSta);

    return FLCN_OK;
}

/*!
 * @brief   Send an restart signal along the bus.
 *
 * @param[in] pBus
 *          The bus along which to send the restart signal.
 *
 * @return  FLCN_OK
 *          The operation completed successfully.
 *
 * @return  FLCN_ERR_TIMEOUT
 *          Clock stretching from the slave took too long and the operation
 *          aborted.  The bus is no longer in a valid state.
 *
 * @section Preconditions
 *  - SCL must be high.
 *  - The most recent bus activity must have been a byte + ack transfer.
 *  - The sum of tR and tHigh must have elapsed since the most recent event.
 *
 * @section Postconditions
 *
 * If FLCN_OK were to be returned:
 *  - A restart signal will have been sent along the bus.
 *  - SCL will be high and SDA will be low.
 *  - At least tF + tHdSta elapsed since the start signal was sent.
 *
 * If FLCN_ERR_TIMEOUT were to be returned:
 *  - No postconditions; the bus will not be in a valid state.
 */
static FLCN_STATUS
_i2cSendRestart
(
    I2C_SW_BUS *const pBus
)
{
    //
    // Prior to a restart, we need to prepare the bus.  After the next clock
    // cycle, we need both SCL and SDA to be high for tR + tSuSta.
    //

    _i2cDriveAndDelay(pBus, pBus->sclOut, LOW, pBus->tF);

    _i2cDriveAndDelay(pBus, pBus->sdaOut, HIGH, pBus->tR + pBus->tLow);

    _i2cDriveAndDelay(pBus, pBus->sclOut, HIGH, pBus->tR);

    // Wait for SCL to be released by the slave.
    if (_i2cWaitSclReleasedTimedOut(pBus))
    {
        return FLCN_ERR_TIMEOUT;

    }
    I2C_DELAY(pBus->tSuSta);

    // A start signal is to pull down SDA while SCL is high.
    _i2cDriveAndDelay(pBus, pBus->sdaOut, LOW, pBus->tF + pBus->tHdSta);

    return FLCN_OK;
}


/*!
 * @brief   Send a stop signal along the bus.
 *
 * @param[in] pBus
 *          The bus along which to send the stop signal.
 *
 * @return  FLCN_OK
 *          The operation completed successfully.
 *
 * @return  FLCN_ERR_TIMEOUT
 *          Clock stretching from the slave took too long and the operation
 *          aborted.  The bus is no longer in a valid state.
 *
 * @section Preconditions
 *  - SCL must be high.
 *  - The most recent bus activity must have been a byte + ack transfer.
 *  - The sum of tR and tHigh must have elapsed since SCL was released (by both
 *    master and slave) for the latest ack bit.
 *
 * @section Postconditions
 *
 * If FLCN_OK were to be returned, then:
 *  - A stop signal will have been sent along the bus.
 *  - Both SCL and SDA will be high.
 *  - At least tR + tBuf time will have elapsed.
 *
 * If FLCN_ERR_TIMEOUT were to be returned, then:
 *  - No postconditions; the bus will not be in a valid state.
 */
static FLCN_STATUS
_i2cSendStop
(
    I2C_SW_BUS *const pBus
)
{
    FLCN_STATUS  error = FLCN_OK;

    //
    // Prior to a stop, we need to prepare the bus.  After the next clock
    // cycle, we need SCL high and SDA low for tR + tSuSto.
    //

    _i2cDriveAndDelay(pBus, pBus->sclOut, LOW, pBus->tF + pBus->tHdDat);

    // delay for SDA going low (tF) and additional tLow to guarantee
    // that the length for SCL being low isn't shorter than required
    // from the spec, but can subtract the time already spent with
    // the clock low (tHdDat).
    _i2cDriveAndDelay(pBus, pBus->sdaOut, LOW, pBus->tF + pBus->tLow - pBus->tHdDat);

    _i2cDriveAndDelay(pBus, pBus->sclOut, HIGH, pBus->tR);

    // Wait for SCL to be released by the slave.
    if (_i2cWaitSclReleasedTimedOut(pBus))
    {
        // can't return early, bus must get back to a valid state
        error = FLCN_ERR_TIMEOUT;
    }
    I2C_DELAY(pBus->tSuSto);

    // A stop signal is to release SDA while SCL is high.
    _i2cDriveAndDelay(pBus, pBus->sdaOut, HIGH, pBus->tR + pBus->tBuf);

    return error;
}

/*!
 * @brief   Read/Write a byte from/to the bus.
 *
 * @param[in] pBus
 *          The bus along which to write the byte of data.
 *
 * @param[in] pData
 *      RD: The result of the read.  Written only if the full byte is read.
 *      WR: Pointer to the byte of data to transfer.  This is a pointer to have
 *          the same function type as _i2cReadByte.
 *
 * @param[in] bLast
 *      RD: Whether this is the last byte in a transfer.  The last byte should
 *          be followed by a nack instead of an ack.
 *      WR: Unused parameter to keep the same function type as _i2cReadByte.
 *
 * @param[in] bRead
 *          Defines the direction of the transfer.
 *
 * @return  FLCN_OK
 *          The operation completed successfully.
 *
 * @return  FLCN_ERR_TIMEOUT
 *          Clock stretching from the slave took too long and the operation
 *          aborted.  The bus is no longer in a valid state.
 *
 * @return  FLCN_ERR_I2C_NACK_BYTE
 *          The slave did not acknowledge the byte transfer.
 *
 * @section Preconditions
 *  - SCL must be high.
 *  - The most recent bus activity must have been either a start or a byte +
 *    ack transfer.
 *  - If the most recent bus activity was a start, then tF + tHdSta must have
 *    elapsed.
 *  - If the most recent bus activity was a byte + ack transfer, then tR +
 *    tHigh must have elapsed.
 *
 * @section Postconditions (RD)
 *
 * If FLCN_OK were to be returned, then:
 *  - The byte will have been read and stored in pData.
 *  - If pLast was true, then a nack will have been sent; if false, an ack
 *    will have been sent.
 *  - SCL will be high and SDA will be low.
 *
 * If FLCN_ERR_TIMEOUT were to be returned, then:
 *  - The bus will not be in a valid state.
 *
 * @section Postconditions (WR)
 *
 * Regardless of return value:
 *  - The data contained in pData will remain unchanged.
 *
 * If FLCN_OK were to be returned, then:
 *  - The byte will have been transferred and the ack made by the slave.
 *  - SCL will be high and SDA will be low..
 *
 * If FLCN_ERR_I2C_NACK_BYTE were to be returned, then:
 *  - The byte will have been transferred and a nack will have been received
 *    from the slave.
 *  - Both SCL and SDA will be high.
 *
 * If FLCN_ERR_TIMEOUT were to be returned, then:
 *  - The bus will not be in a valid state.
 */
static FLCN_STATUS
_i2cProcessByte
(
          I2C_SW_BUS *const pBus,
          LwU8       *const pData,
    const LwBool            bLast,
          LwBool            bRead
)
{
    LwU8 data;
    LwU8 i;

    data = bRead ? 0 : *pData;

    for (i = 0; i < BITS_PER_BYTE; i++)
    {
        // Read/Write and store each bit with the most significant coming first.
        if (bRead)
        {
            _i2cDriveAndDelay(pBus, pBus->sclOut, LOW,
                pBus->tF + pBus->tSuDat);

            // Don't mask the transmission.
            _i2cDriveAndDelay(pBus, pBus->sdaOut, HIGH,
                pBus->tR + pBus->tHdDat);
        }
        else
        {
            _i2cDriveAndDelay(pBus, pBus->sclOut, LOW,
                pBus->tF + pBus->tHdDat);

            _i2cDriveAndDelay(pBus, pBus->sdaOut,
                ((data << i) & 0x80) ? HIGH : LOW, pBus->tR + pBus->tSuDat);
        }

        // Note that here we do NOT have any delay for Write.
        _i2cDriveAndDelay(pBus, pBus->sclOut, HIGH, bRead ? pBus->tR : 0);

        // Wait for SCL to be released by the slave.
        if (_i2cWaitSclReleasedTimedOut(pBus))
        {
            if (!bRead)
            {
                _i2cDriveAndDelay(pBus, pBus->sdaOut, HIGH,
                    pBus->tR + pBus->tSuDat);
            }

            return FLCN_ERR_TIMEOUT;
        }
        I2C_DELAY(pBus->tHigh);

        if (bRead)
        {
            // Read the data from the slave.
            data <<= 1;
            data |= I2C_READ(pBus, pBus->sdaIn);
        }
    }

    // Read the acknowledge bit from the slave, where low indicates an ack.
    _i2cDriveAndDelay(pBus, pBus->sclOut, LOW, pBus->tF + pBus->tHdDat);

    // Release SDA so as not to interfere with the slave's transmission.
    _i2cDriveAndDelay(pBus, pBus->sdaOut, bRead ? (bLast ? HIGH : LOW) : HIGH,
        pBus->tR + pBus->tSuDat);

    _i2cDriveAndDelay(pBus, pBus->sclOut, HIGH, pBus->tR);

    // Wait for SCL to be released by the slave.
    if (_i2cWaitSclReleasedTimedOut(pBus))
    {
        if (bRead)
        {
            _i2cDriveAndDelay(pBus, pBus->sdaOut, HIGH,
                pBus->tR + pBus->tSuDat);
        }
        return FLCN_ERR_TIMEOUT;
    }
    I2C_DELAY(pBus->tHigh);

    if (bRead)
    {
       *pData = data;
        return FLCN_OK;
    }
    else
    {
        return I2C_READ(pBus, pBus->sdaIn) ? FLCN_ERR_I2C_NACK_BYTE : FLCN_OK;
    }
}

static FLCN_STATUS
_i2cWriteByte
(
          I2C_SW_BUS *const pBus,
          LwU8       *const pData,
    const LwBool            bIgnored
)
{
    return _i2cProcessByte(pBus, pData, bIgnored, LW_FALSE);
}

/*!
 * @brief   Read a byte from the I2C bus.
 *
 * @param[in] pBus
 *          The bus along which to write the byte of data.
 *
 * @param[out] pData
 *          The result of the read.  Written only if the full byte is read.
 *
 * @param[in] bLast
 *          Whether this is the last byte in a transfer.  The last byte should
 *          be followed by a nack instead of an ack.
 *
 * @return  FLCN_OK
 *          The operation completed successfully.
 *
 * @return  FLCN_ERR_TIMEOUT
 *          Clock stretching from the slave took too long and the operation
 *          aborted.  The bus is no longer in a valid state.
 *
 * @section Preconditions
 *  - SCL must be high.
 *  - The most recent bus activity must have been either a start or a byte +
 *    ack transfer.
 *  - If the most recent bus activity was a start, then tF + tHdSta must have
 *    elapsed.
 *  - If the most recent bus activity was a byte + ack transfer, then tR +
 *    tHigh must have elapsed.
 *
 * @section Postconditions
 *
 * If FLCN_OK were to be returned, then:
 *  - The byte will have been read and stored in pData.
 *  - If pLast was true, then a nack will have been sent; if false, an ack
 *    will have been sent.
 *  - SCL will be high and SDA will be low.
 *
 * If FLCN_ERR_TIMEOUT were to be returned, then:
 *  - The bus will not be in a valid state.
 */
static FLCN_STATUS
_i2cReadByte
(
          I2C_SW_BUS *const pBus,
          LwU8       *const pData,
    const LwBool            bLast
)
{
    return _i2cProcessByte(pBus, pData, bLast, LW_TRUE);
}

/*!
 * @brief   Send an address (if any) using the transaction's specified format.
 *
 * @param[in] pTa
 *          The transaction information, including the addressMode and address.
 *
 * @param[in] pBus
 *          The bus information.
 *
 * @param[in] bRead
 *          Whether to send a read or write bit.  Note that this is not
 *          necessarily the direction of the transaction, as certain preludes
 *          (for example, sending the index) may need to be in a different
 *          direction.
 *
 * @return  FLCN_OK
 *          The address was sent successfully, or none was specified.
 *
 * @return  FLCN_ERR_I2C_NACK_ADDRESS
 *          No device acknowledged the address.
 *
 * @return  FLCN_ERR_ILWALID_ARGUMENT
 *          The addressMode within pTa was not a valid value so no address was
 *          sent.
 *
 * @return  FLCN_ERR_TIMEOUT
 *          Clock stretching from the slave took too long and the operation
 *          aborted.  The bus is no longer in a valid state.
 *
 * @section Preconditions
 *  - SCL must be high.
 *  - The most recent bus activity must have been a start or restart signal.
 *  - The sum tF + tHdSta must have elapsed since the last (re)start.
 *
 * @section Postconditions
 *
 * If FLCN_OK were to be returned, then:
 *  - If addressMode were to indicate to send an address, then the address will
 *    have been sent and acknowledged correctly.
 *  - If addressMode were to indicate to do nothing, then no bus operation will
 *    have been performed.
 *  - Both SCL and SDA will be high.
 *
 * If FLCN_ERR_I2C_NACK_ADDRESS were to be returned, then:
 *  - The address will have been sent correctly but no ack was received.
 *  - SCL will be high and SDA will be low.
 *
 * If FLCN_ERR_ILWALID_ARGUMENT were to be returned, then:
 *  - The pTa->addressMode field will have been invalid.
 *  - No bus operation will have been performed.
 *
 * If timeout were to be returned, then:
 *  - The bus will not be in a valid state.
 */
static FLCN_STATUS
_i2cSendAddress
(
    const   I2C_TRANSACTION *const  pTa,
            I2C_SW_BUS      *const  pBus,
    const   LwBool                  bRead
)
{
    FLCN_STATUS error   = FLCN_OK;
    LwU32       address = pTa->i2cAddress;
    LwU8        byte;

    switch (DRF_VAL(_RM_FLCN, _I2C_FLAGS, _ADDRESS_MODE, pTa->flags))
    {
        case LW_RM_FLCN_I2C_FLAGS_ADDRESS_MODE_NO_ADDRESS:
        {
            break;
        }

        case LW_RM_FLCN_I2C_FLAGS_ADDRESS_MODE_7BIT:
        {
            //
            // The address + R/W bit transfer for 7-bit addressing is simply
            // the address followed by the R/W bit.  Lwrrently the addresses
            // passed by the RM are already shifted by one to the left.
            //
            byte  = address | bRead;
            error = _i2cWriteByte(pBus, &byte, LW_FALSE);
            break;
        }

        case LW_RM_FLCN_I2C_FLAGS_ADDRESS_MODE_10BIT:
        {
            //
            // The address + R/W bit transfer is a bit odd in 10-bit
            // addressing and the format is dependent on the R/W bit itself.
            // Due to backwards compatibility, the first byte is b11110xx0,
            // with 'xx' being the two most significant address bits.
            //
            // Technically the least significant bit indicates a write, but
            // must always be so for a ten-bit address.  If the R/W bit
            // actually is a write, then transmission simply continues after
            // the address.  If the R/W bit actually is a read, then a _second_
            // address transmission follows.  This second address is a resend
            // of the first byte of the 10-bit address with a read bit.
            //
            // If this is confusing (it is,) I highly recommend reading the
            // the specification.
            //

            //
            // First, transfer the address using the 10-bit format.
            //
            byte  = GET_ADDRESS_10BIT_FIRST(address);
            error = _i2cWriteByte(pBus, &byte, LW_FALSE);

            if (error == FLCN_OK)
            {
                byte  = GET_ADDRESS_10BIT_SECOND(address);
                error = _i2cWriteByte(pBus, &byte, LW_FALSE);

                if (error == FLCN_OK)
                {
                    //
                    // Now, if the transaction is a read, we send a restart and then
                    // the first byte with a read bit.
                    //
                    if (bRead)
                    {
                        error = _i2cSendRestart(pBus);

                        if (error == FLCN_OK)
                        {
                            byte  = GET_ADDRESS_10BIT_FIRST(address) | 1;
                            error = _i2cWriteByte(pBus, &byte, LW_FALSE);
                        }
                    }
                }
            }
            break;
        }
        default:
        {
            error = FLCN_ERR_ILWALID_ARGUMENT;
            break;
        }
    }

    if (error == FLCN_ERR_I2C_NACK_BYTE)
    {
        error = FLCN_ERR_I2C_NACK_ADDRESS;
    }
    return error;
}

/*!
 * @brief   Determine the worst-case delay needed between line transitions.
 *
 * @param[out] pBus Fill in the delay timings.
 * @param[in] speedMode
 *          The bitrate for communication.  See @ref rmdpucmdif.h "the command
 *          interface" for more details.
 * @param[in] portId
 *          Port ID of the port.
 */
static void
_i2cBusAndSpeedInit
(
    I2C_SW_BUS     *pBus,
    const LwU8      speedMode,
    LwU32           portId
)
{
    i2cInitSwi2c_HAL(&I2c, pBus);
    pBus->port = portId;

    if (speedMode == LW_RM_FLCN_I2C_FLAGS_SPEED_MODE_100KHZ)
    {
        pBus->tF     = I2C_ST_F;
        pBus->tR     = I2C_ST_R;
        pBus->tSuDat = I2C_ST_SUDAT;
        pBus->tHdDat = I2C_ST_HDDAT;
        pBus->tHigh  = I2C_ST_HIGH;
        pBus->tSuSto = I2C_ST_SUSTO;
        pBus->tHdSta = I2C_ST_HDSTA;
        pBus->tSuSta = I2C_ST_SUSTA;
        pBus->tBuf   = I2C_ST_BUF;
        pBus->tLow   = I2C_ST_LOW;
    }
    else if (speedMode == LW_RM_FLCN_I2C_FLAGS_SPEED_MODE_400KHZ)
    {
        pBus->tF     = I2C_FAST_F;
        pBus->tR     = I2C_FAST_R;
        pBus->tSuDat = I2C_FAST_SUDAT;
        pBus->tHdDat = I2C_FAST_HDDAT;
        pBus->tHigh  = I2C_FAST_HIGH;
        pBus->tSuSto = I2C_FAST_SUSTO;
        pBus->tHdSta = I2C_FAST_HDSTA;
        pBus->tSuSta = I2C_FAST_SUSTA;
        pBus->tBuf   = I2C_FAST_BUF;
        pBus->tLow   = I2C_FAST_LOW;
    }
    else
    {
        // Halt since unknown speed received
        OS_HALT();
    }
}

/*!
 * @brief   Wait for SCL to be released by the slave.
 *
 * @param[in]   pBus    The bus information.
 *
 * @return  LW_TRUE     if waiting for SCL to get released timed out
 * @return  LW_FALSE    if SCL got released by the slave
 */
static LwBool
_i2cWaitSclReleasedTimedOut
(
    I2C_SW_BUS *pBus
)
{
    return  (!OS_PTIMER_SPIN_WAIT_NS_COND((LwBool(*)(void *))(_i2cIsLineHigh),
                (void *)pBus,\
                I2C_STRETCHED_LOW_TIMEOUT_NS));
}

/*!
 * @brief   Perform a generic I2C transaction using the SW bitbanging controls
 *          of the GPU.
 *
 * @param[in, out] pTa
 *          Transaction data; please see @ref ./inc/task4_i2c.h "task4_i2c.h".
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
 *          The transaciton pTa was detected to be invalid.
 *
 * @return  FLCN_ERR_TIMEOUT
 *          Clock stretching from the slave took too long and the transaction
 *          aborted.  The bus is no longer in a valid state.
 */
FLCN_STATUS
i2cPerformTransactiolwiaSw
(
    I2C_TRANSACTION *pTa
)
{
    FLCN_STATUS         error = FLCN_OK;
    FLCN_STATUS         error2;
    I2C_SW_BUS          bus;
    LwU8                junk;
    LwU16               i;
    LwU32               flags = pTa->flags;
    LwU32               portId = DRF_VAL(_RM_FLCN, _I2C_FLAGS, _PORT, flags);
    LwU32               indexLength = DRF_VAL(_RM_FLCN, _I2C_FLAGS, _INDEX_LENGTH,
                                            flags);
    LwU8                messageSize = pTa->messageLength;

    _i2cBusAndSpeedInit(&bus, DRF_VAL(_RM_FLCN, _I2C_FLAGS, _SPEED_MODE, flags), portId);

    //
    // If the command says we should begin with a start signal, send it.
    //
    if (FLD_TEST_DRF(_RM_FLCN, _I2C_FLAGS, _START, _SEND, flags))
    {
        error = _i2cSendStart(&bus);
    }

    //
    // Send the address, if any.  The direction depends on whether or not we're
    // sending an index.  If we are, then we need to write the index.  If not,
    // then use the direction of the transaction.
    //
    if (error == FLCN_OK)
    {
        error = _i2cSendAddress(pTa, &bus, pTa->bRead &&
                DRF_VAL(_RM_FLCN, _I2C_FLAGS, _INDEX_LENGTH, flags) == 0);
    }

    //
    // Send the indices, if any.
    //
    if ((error == FLCN_OK) && indexLength)
    {
        // Send the first index, if any.
        for (i = 0; (error == FLCN_OK) && (i < indexLength); i++)
        {
            error = _i2cWriteByte(&bus, &pTa->index[i], LW_FALSE);
        }

        // If a restart is needed between phrases, send it.
        if ((error == FLCN_OK) &&
            FLD_TEST_DRF(_RM_FLCN, _I2C_FLAGS, _RESTART, _SEND, flags))
        {
            error = _i2cSendRestart(&bus);

            //
            // We know we are sending the message next, so send the message
            // direction.
            //
            if (error == FLCN_OK)
            {
                error = _i2cSendAddress(pTa, &bus, pTa->bRead);
            }
        }
    }

    // If a block transaction read/write the size first!
    if ((error == FLCN_OK) &&
         FLD_TEST_DRF(_RM_FLCN, _I2C_FLAGS, _BLOCK_PROTOCOL, _ENABLED, flags))
    {
        error = _i2cProcessByte(&bus, &messageSize, LW_FALSE, pTa->bRead);

        // If a read, ensure that device returned the expected size!
        if ((error == FLCN_OK) && (pTa->bRead) &&
            (messageSize != pTa->messageLength))
        {
            error = FLCN_ERR_I2C_SIZE;
        }
    }

    //
    // Perform the main transaction.
    //
    for (i = 0; (error == FLCN_OK) && (i < pTa->messageLength);
         i++)
    {
        // The last data byte requires special handling
        error = _i2cProcessByte(&bus, &pTa->pMessage[i],
                                (i == (pTa->messageLength - 1)), pTa->bRead);
    }

    //
    // Bug 719104 : The target device may NACK the last byte written as
    // per I2C standard protocol. It should not be treated as an error.
    //
    if ((error == FLCN_ERR_I2C_NACK_BYTE) && !pTa->bRead &&
        (pTa->messageLength != 0) && (i == pTa->messageLength))
    {
        error = FLCN_OK;
    }

    //
    // Always send a stop, even if the command does not specify or we had an
    // earlier failure.  We must clean up after ourselves!
    //
    error2 = _i2cSendStop(&bus);

    //
    // Bug 785366: Verify that the i2c bus is in a valid state. The data
    // line should be high.
    //

    bus.lwrLine = bus.sdaIn;
    if (!_i2cIsLineHigh(&bus))
    {
         bus.lwrLine = bus.sclIn;
        // another device may be pulling the line low.

        // clock the bus - by reading in a byte.
        (void)_i2cReadByte(&bus, &junk, LW_TRUE);

        // Attempt the Stop again.
        error2 = _i2cSendStop(&bus);
    }

    // Don't let a stop failure mask an earlier one.
    if (error == FLCN_OK)
    {
        error = error2;
    }

    return error;
}


/*!
 * @brief Recover the i2c bus from an unexpected HW-state.
 *
 * This function is to be called when the target I2C bus for a particular
 * transaction is not in an expected state. This is lwrrently limited to cases
 * where a slave device is perpetually pulling SDA low for some unknown reason.
 * It may however be extended in the future to cover various other unexpected
 * bus states. A "recovered" bus is defined by the state where both SCL and SDA
 * are released (pulled high).
 *
 * @param[in] pTa
 *     The transaction information. Used by this function for port and speed
 *     information.
 *
 * @return FLCN_OK
 *     Both SCL and SDA are pulled high. There is no contention on the bus.
 *
 * @return FLCN_ERR_I2C_BUS_ILWALID
 *     The bus remains in a bad/unusable state, even after the recovery
 *     attempt. Recovery failed.
 */
FLCN_STATUS
i2cRecoverBusViaSw
(
    I2C_TRANSACTION *pTa
)
{
    FLCN_STATUS status = FLCN_OK;
    I2C_SW_BUS  bus;
    LwU8        junk;
    LwU32       flags  = pTa->flags;
    LwU32       portId = DRF_VAL(_RM_FLCN, _I2C_FLAGS, _PORT, flags);
    LwBool      bIsBusReady = LW_FALSE;

    _i2cBusAndSpeedInit(&bus, DRF_VAL(_RM_FLCN, _I2C_FLAGS, _SPEED_MODE, flags), portId);

    //
    // In all observed cases where recovery is necessary, performing a single-
    // byte read followed by sending a stop bit is sufficient to recover the
    // bus. It may be required to define a more elaborate recovery sequence in
    // the future where multiple recovery steps are performed and are perhaps
    // even based on the current "wedged" bus state.
    //
    _i2cReadByte(&bus, &junk, LW_TRUE);
    _i2cSendStop(&bus);

    status = i2cIsBusReady_HAL(&I2c, portId, &bIsBusReady);
    if (status != FLCN_OK)
    {
        return status;
    }

    if (!bIsBusReady)
    {
        status = FLCN_ERR_I2C_BUS_ILWALID;
    }

    return status;
}

