/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2007-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file task4_i2c.c
 * @brief A bit-banging implementation of I2C supporting all transactions
 *        compliant with the official Phillips specification.
 */

#include "pmusw.h"
#include "dev_pwr_csb.h"
#include "pmu_obji2c.h"
#include "pmu_objtimer.h"
#include "dbgprintf.h"
#include "task_i2c.h"
#include "flcnifi2c.h"

#define BITS_PER_BYTE 8

/*! Alias describing how to set an I2C line low. */
#define LOW     LW_FALSE

/*! Alias describing how to set an I2C line high. */
#define HIGH    LW_TRUE

/*! Read the actual I2C line status. */
#define READ(line) ((REG_RD32(CSB, LW_CPWR_PMU_GPIO_INPUT) >> (line)) & 0x1)


/*! Extract the first byte of a 10-bit address. */
#define GET_ADDRESS_10BIT_FIRST(a) ((LwU8)((((a) >> 8) & 0x6) | 0xF0))

/*! Extract the second byte of a 10-bit address. */
#define GET_ADDRESS_10BIT_SECOND(a) ((LwU8)(((a) >> 1) & 0xFF))


/*! Abstract which timer we are using to perform delays. */
#define DELAY(a) OS_PTIMER_SPIN_WAIT_NS(a)

/*!
 * @brief The number of nanoseconds we will wait for secondary clock stretching
 *
 * Previously, this was set to 200us, but proved too be short so we
 * increased it to 2ms and then to 10ms.
 * Reference bug 630691   : 200us to 2ms
 * Reference bug 200555847: 2ms to 10ms
 *
 * RM SW I2C uses 10ms timeout value for clock stretching. Using same
 * value for PMU SW I2C to avoid any future changes in this value.
 */
#define STRETCHED_LOW_TIMEOUT_NS 10000000

// Standard timings (100 kHz) in nanoseconds from I2C spec except as noted.
#define I2C_ST_F            300
#define I2C_ST_R           1000
#define I2C_ST_SUDAT       1800    // spec is (min) 250, but borrow from tHDDAT
#define I2C_ST_HDDAT       1900    // spec is (max) 3450, but loan to tSUDAT
#define I2C_ST_HIGH        4000
#define I2C_ST_SUSTO       4000
#define I2C_ST_HDSTA       4000
#define I2C_ST_SUSTA       4700
#define I2C_ST_BUF         4700
#define I2C_ST_LOW         4700    // I2C_ST_SUDAT + I2C_ST_R + I2C_ST_HDDAT


// Fast timings (400 kHz) in nanoseconds from I2C spec except as noted.
#define I2C_FAST_F          300
#define I2C_FAST_R          300
#define I2C_FAST_SUDAT      200    // spec is (min) 100, but borrow from tHDDAT
#define I2C_FAST_HDDAT      800    // spec is (max) 900, but loan to tSUDAT
#define I2C_FAST_HIGH       600
#define I2C_FAST_SUSTO      600
#define I2C_FAST_HDSTA      600
#define I2C_FAST_SUSTA      600
#define I2C_FAST_BUF       1300
#define I2C_FAST_LOW       1300    // I2C_ST_SUDAT + I2C_ST_R + I2C_ST_HDDAT

/*!
 * Macro to set fast mode timing
 */
#define I2C_TIMING_FAST(timing, speed)                                         \
    LW_UNSIGNED_ROUNDED_DIV(((timing) * RM_PMU_I2C_SPEED_400KHZ), (speed))

/*!
 * Structure containing a description of the bus as needed by the software
 * bitbanging implementation.
 */
typedef struct
{
    LwS32 scl;      //!< The GPIO bit for SCL.
    LwS32 sda;      //!< The GPIO bit for SDA.

    //
    // The following timings are used as stand-ins for I2C spec timings, so
    // that different speed modes may share the same code.
    //
    LwU16 tF;
    LwU16 tR;
    LwU16 tSuDat;
    LwU16 tHdDat;
    LwU16 tHigh;
    LwU16 tSuSto;
    LwU16 tHdSta;
    LwU16 tSuSta;
    LwU16 tBuf;
    LwU16 tLow;
} I2C_PMU_SW_BUS;

static FLCN_STATUS s_i2cSendStart  (const I2C_PMU_SW_BUS *pBus);
static FLCN_STATUS s_i2cSendRestart(const I2C_PMU_SW_BUS *pBus);
static FLCN_STATUS s_i2cSendStop   (const I2C_PMU_SW_BUS *pBus);
static FLCN_STATUS s_i2cProcessByte(const I2C_PMU_SW_BUS *pBus, LwU8 *const pData, LwBool bLast, LwBool bRead);
static FLCN_STATUS s_i2cReadByte   (const I2C_PMU_SW_BUS *pBus, LwU8 *const pData, LwBool bLast);
static FLCN_STATUS s_i2cWriteByte  (const I2C_PMU_SW_BUS *pBus, LwU8 *const pData, LwBool bIgnored);
static FLCN_STATUS s_i2cSendAddress(const I2C_PMU_TRANSACTION *pTa, const I2C_PMU_SW_BUS *pBus, LwBool bRead);
static LwBool      s_i2cIsLineHigh (LwS32 *pLine) GCC_ATTRIB_USED();
static void        s_i2cDriveAndDelay(LwU32 line, LwBool bHigh, LwU32 delayNs);
static LwBool      s_i2cWaitSclReleasedTimedOut(const I2C_PMU_SW_BUS *pBus);

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
s_i2cIsLineHigh
(
    LwS32 *pLine
)
{
    return READ(*pLine);
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
s_i2cSendStart
(
    const I2C_PMU_SW_BUS *pBus
)
{
    // A start signal is to pull down SDA while SCL is high.
    s_i2cDriveAndDelay(pBus->sda, LOW, pBus->tF + pBus->tHdSta);

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
 *          Clock stretching from the secondary took too long and the operation
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
s_i2cSendRestart
(
    const I2C_PMU_SW_BUS *pBus
)
{
    //
    // Prior to a restart, we need to prepare the bus.  After the next clock
    // cycle, we need both SCL and SDA to be high for tR + tSuSta.
    //

    s_i2cDriveAndDelay(pBus->scl, LOW, pBus->tF);

    s_i2cDriveAndDelay(pBus->sda, HIGH, pBus->tR + pBus->tLow);

    s_i2cDriveAndDelay(pBus->scl, HIGH, pBus->tR);

    // Wait for SCL to be released by the secondary.
    if (s_i2cWaitSclReleasedTimedOut(pBus))
    {
        return FLCN_ERR_TIMEOUT;
    }
    DELAY(pBus->tSuSta);

    // A start signal is to pull down SDA while SCL is high.
    s_i2cDriveAndDelay(pBus->sda, LOW, pBus->tF + pBus->tHdSta);

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
 *          Clock stretching from the secondary took too long and the operation
 *          aborted.  The bus is no longer in a valid state.
 *
 * @section Preconditions
 *  - SCL must be high.
 *  - The most recent bus activity must have been a byte + ack transfer.
 *  - The sum of tR and tHigh must have elapsed since SCL was released (by both
 *    primary and secondary) for the latest ack bit.
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
s_i2cSendStop
(
    const I2C_PMU_SW_BUS *pBus
)
{
    FLCN_STATUS  status = FLCN_OK;

    //
    // Prior to a stop, we need to prepare the bus.  After the next clock
    // cycle, we need SCL high and SDA low for tR + tSuSto.
    //

    s_i2cDriveAndDelay(pBus->scl, LOW, pBus->tF + pBus->tHdDat);

    // delay for SDA going low (tF) and additional tLow to guarantee
    // that the length for SCL being low isn't shorter than required
    // from the spec, but can subtract the time already spent with
    // the clock low (tHdDat).
    s_i2cDriveAndDelay(pBus->sda, LOW, pBus->tF + pBus->tLow - pBus->tHdDat);

    s_i2cDriveAndDelay(pBus->scl, HIGH, pBus->tR);

    // Wait for SCL to be released by the secondary.
    if (s_i2cWaitSclReleasedTimedOut(pBus))
    {
        // can't return early, bus must get back to a valid state
        status = FLCN_ERR_TIMEOUT;
    }
    DELAY(pBus->tSuSto);

    // A stop signal is to release SDA while SCL is high.
    s_i2cDriveAndDelay(pBus->sda, HIGH, pBus->tR + pBus->tBuf);

    return status;
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
 *          the same function type as s_i2cReadByte.
 *
 * @param[in] bLast
 *      RD: Whether this is the last byte in a transfer.  The last byte should
 *          be followed by a nack instead of an ack.
 *      WR: Unused parameter to keep the same function type as s_i2cReadByte.
 *
 * @param[in] bRead
 *          Defines the direction of the transfer.
 *
 * @return  FLCN_OK
 *          The operation completed successfully.
 *
 * @return  FLCN_ERR_TIMEOUT
 *          Clock stretching from the secondary took too long and the operation
 *          aborted.  The bus is no longer in a valid state.
 *
 * @return  FLCN_ERR_I2C_NACK_BYTE
 *          The secondary did not acknowledge the byte transfer.
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
 *  - The byte will have been transferred and the ack made by the secondary.
 *  - SCL will be high and SDA will be low..
 *
 * If FLCN_ERR_I2C_NACK_BYTE were to be returned, then:
 *  - The byte will have been transferred and a nack will have been received
 *    from the secondary.
 *  - Both SCL and SDA will be high.
 *
 * If FLCN_ERR_TIMEOUT were to be returned, then:
 *  - The bus will not be in a valid state.
 */
static FLCN_STATUS
s_i2cProcessByte
(
    const I2C_PMU_SW_BUS *pBus,
    LwU8         *const pData,
    LwBool              bLast,
    LwBool              bRead
)
{
    LwU8    data;
    LwU8    i;

    data = bRead ? 0 : *pData;

    for (i = 0; i < BITS_PER_BYTE; i++)
    {
        // Read/Write each bit starting with the most significant.
        if (bRead)
        {
            s_i2cDriveAndDelay(pBus->scl, LOW, pBus->tF + pBus->tSuDat);

            // Don't mask the transmission.
            s_i2cDriveAndDelay(pBus->sda, HIGH, pBus->tR + pBus->tHdDat);
        }
        else
        {
            s_i2cDriveAndDelay(pBus->scl, LOW, pBus->tF + pBus->tHdDat);

            s_i2cDriveAndDelay(pBus->sda, ((data << i) & 0x80) ? HIGH : LOW,
                              pBus->tR + pBus->tSuDat);
        }

        // Note that here we do NOT have any delay for Write.
        s_i2cDriveAndDelay(pBus->scl, HIGH, bRead ? pBus->tR : 0);

        // Wait for SCL to be released by the secondary.
        if (s_i2cWaitSclReleasedTimedOut(pBus))
        {
            if (!bRead)
            {
                s_i2cDriveAndDelay(pBus->sda, HIGH, pBus->tR + pBus->tSuDat);
            }

            return FLCN_ERR_TIMEOUT;
        }
        DELAY(pBus->tHigh);

        if (bRead)
        {
            // Read the data from the secondary.
            data <<= 1;
            data |= READ(pBus->sda);
        }
    }

    //
    // Read the acknowledge bit from the secondary, where low indicates an ack.
    //

    s_i2cDriveAndDelay(pBus->scl, LOW, pBus->tF + pBus->tHdDat);

    // Release SDA so as not to interfere with the secondary's transmission.
    s_i2cDriveAndDelay(pBus->sda, bRead ? (bLast ? HIGH : LOW) : HIGH,
                      pBus->tR + pBus->tSuDat);

    s_i2cDriveAndDelay(pBus->scl, HIGH, pBus->tR);

    // Wait for SCL to be released by the secondary.
    if (s_i2cWaitSclReleasedTimedOut(pBus))
    {
        if (bRead)
        {
            s_i2cDriveAndDelay(pBus->sda, HIGH, pBus->tR + pBus->tSuDat);
        }

        return FLCN_ERR_TIMEOUT;
    }
    DELAY(pBus->tHigh);

    if (bRead)
    {
        *pData = data;

        return FLCN_OK;
    }
    else
    {
        return READ(pBus->sda) ? FLCN_ERR_I2C_NACK_BYTE : FLCN_OK;
    }
}

static FLCN_STATUS
s_i2cReadByte
(
    const I2C_PMU_SW_BUS *pBus,
    LwU8         *const pData,
    LwBool              bLast
)
{
    return s_i2cProcessByte(pBus, pData, bLast, LW_TRUE);
}

static FLCN_STATUS
s_i2cWriteByte
(
    const I2C_PMU_SW_BUS *pBus,
    LwU8         *const pData,
    LwBool              bIgnored
)
{
    return s_i2cProcessByte(pBus, pData, bIgnored, LW_FALSE);
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
 *          Clock stretching from the secondary took too long and the operation
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
 * If FLCN_ERR_TIMEOUT were to be returned, then:
 *  - The bus will not be in a valid state.
 */
static FLCN_STATUS
s_i2cSendAddress
(
    const I2C_PMU_TRANSACTION *pTa,
    const I2C_PMU_SW_BUS      *pBus,
          LwBool               bRead
)
{
    FLCN_STATUS    error   = FLCN_OK;
    LwU32          address = pTa->pGenCmd->i2cAddress;
    LwU8           byte;

    switch (DRF_VAL(_RM_FLCN, _I2C_FLAGS, _ADDRESS_MODE, pTa->pGenCmd->flags))
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
            error = s_i2cWriteByte(pBus, &byte, LW_FALSE);
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
            error = s_i2cWriteByte(pBus, &byte, LW_FALSE);

            if (error == FLCN_OK)
            {
                byte  = GET_ADDRESS_10BIT_SECOND(address);
                error = s_i2cWriteByte(pBus, &byte, LW_FALSE);

                if (error == FLCN_OK)
                {
                    //
                    // If the transaction is a read, we send a restart and
                    // then the first byte with a read bit.
                    //
                    if (bRead)
                    {
                        error = s_i2cSendRestart(pBus);

                        if (error == FLCN_OK)
                        {
                            byte  = GET_ADDRESS_10BIT_FIRST(address) | 1;
                            error = s_i2cWriteByte(pBus, &byte, LW_FALSE);
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
 *          The bitrate for communication.  See @ref rmpmucmdif.h "the command
 *          interface" for more details.
 */
static void
s_i2cBusAndSpeedInit
(
    I2C_PMU_SW_BUS *pBus,
    const LwU8      speedMode,
    LwU32           portId
)
{
    pBus->scl = i2cGetSclGpioBit_HAL(&I2c, portId);
    pBus->sda = i2cGetSdaGpioBit_HAL(&I2c, portId);

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

#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_I2C_200KHZ))
    else if (speedMode == LW_RM_FLCN_I2C_FLAGS_SPEED_MODE_200KHZ)
    {
        pBus->tF     = I2C_FAST_F;
        pBus->tR     = I2C_TIMING_FAST(I2C_FAST_R,     RM_PMU_I2C_SPEED_200KHZ);
        pBus->tSuDat = I2C_TIMING_FAST(I2C_FAST_SUDAT, RM_PMU_I2C_SPEED_200KHZ);
        pBus->tHdDat = I2C_TIMING_FAST(I2C_FAST_HDDAT, RM_PMU_I2C_SPEED_200KHZ);
        pBus->tHigh  = I2C_TIMING_FAST(I2C_FAST_HIGH,  RM_PMU_I2C_SPEED_200KHZ);
        pBus->tSuSto = I2C_TIMING_FAST(I2C_FAST_SUSTO, RM_PMU_I2C_SPEED_200KHZ);
        pBus->tHdSta = I2C_TIMING_FAST(I2C_FAST_HDSTA, RM_PMU_I2C_SPEED_200KHZ);
        pBus->tSuSta = I2C_TIMING_FAST(I2C_FAST_SUSTA, RM_PMU_I2C_SPEED_200KHZ);
        pBus->tBuf   = I2C_TIMING_FAST(I2C_FAST_BUF,   RM_PMU_I2C_SPEED_200KHZ);
        pBus->tLow   = I2C_TIMING_FAST(I2C_FAST_LOW,   RM_PMU_I2C_SPEED_200KHZ);
    }
#endif // PMUCFG_FEATURE_ENABLED(PMU_PMGR_I2C_200KHZ)

#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_I2C_300KHZ))
    else if (speedMode == LW_RM_FLCN_I2C_FLAGS_SPEED_MODE_300KHZ)
    {
        pBus->tF     = I2C_FAST_F;
        pBus->tR     = I2C_TIMING_FAST(I2C_FAST_R,     RM_PMU_I2C_SPEED_300KHZ);
        pBus->tSuDat = I2C_TIMING_FAST(I2C_FAST_SUDAT, RM_PMU_I2C_SPEED_300KHZ);
        pBus->tHdDat = I2C_TIMING_FAST(I2C_FAST_HDDAT, RM_PMU_I2C_SPEED_300KHZ);
        pBus->tHigh  = I2C_TIMING_FAST(I2C_FAST_HIGH,  RM_PMU_I2C_SPEED_300KHZ);
        pBus->tSuSto = I2C_TIMING_FAST(I2C_FAST_SUSTO, RM_PMU_I2C_SPEED_300KHZ);
        pBus->tHdSta = I2C_TIMING_FAST(I2C_FAST_HDSTA, RM_PMU_I2C_SPEED_300KHZ);
        pBus->tSuSta = I2C_TIMING_FAST(I2C_FAST_SUSTA, RM_PMU_I2C_SPEED_300KHZ);
        pBus->tBuf   = I2C_TIMING_FAST(I2C_FAST_BUF,   RM_PMU_I2C_SPEED_300KHZ);
        pBus->tLow   = I2C_TIMING_FAST(I2C_FAST_LOW,   RM_PMU_I2C_SPEED_300KHZ);
    }
#endif // PMUCFG_FEATURE_ENABLED(PMU_PMGR_I2C_300KHZ)

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
}

/*!
 * @brief   Perform a generic I2C transaction using the SW bitbanging controls
 *          of the GPU.
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
 *          The transaciton pTa was detected to be invalid.
 *
 * @return  FLCN_ERR_TIMEOUT
 *          Clock stretching from the secondary took too long and the transaction
 *          aborted.  The bus is no longer in a valid state.
 */
FLCN_STATUS
i2cPerformTransactiolwiaSw
(
    I2C_PMU_TRANSACTION *pTa
)
{
    LwU32           flags       = pTa->pGenCmd->flags;
    LwU32           indexLength = DRF_VAL(_RM_FLCN, _I2C_FLAGS, _INDEX_LENGTH,
                                            flags);
    LwU8            messageSize = pTa->messageLength;
    FLCN_STATUS     error       = FLCN_OK;
    I2C_PMU_SW_BUS  bus;
    FLCN_STATUS     error2;
    LwU8            junk;
    LwU16           i;

    s_i2cBusAndSpeedInit(&bus, DRF_VAL(_RM_FLCN, _I2C_FLAGS, _SPEED_MODE, flags),
                              DRF_VAL(_RM_FLCN, _I2C_FLAGS, _PORT,       flags));

    //
    // If the command says we should begin with a start signal, send it.
    //
    if (FLD_TEST_DRF(_RM_FLCN, _I2C_FLAGS, _START, _SEND, flags))
    {
        error = s_i2cSendStart(&bus);
    }

    //
    // Send the address, if any.  The direction depends on whether or not we're
    // sending an index.  If we are, then we need to write the index.  If not,
    // then use the direction of the transaction.
    //
    if (error == FLCN_OK)
    {
        error = s_i2cSendAddress(pTa, &bus, pTa->bRead &&
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
            error = s_i2cWriteByte(&bus, &pTa->pGenCmd->index[i], LW_FALSE);
        }

        // If a restart is needed between phrases, send it.
        if ((error == FLCN_OK) &&
            FLD_TEST_DRF(_RM_FLCN, _I2C_FLAGS, _RESTART, _SEND, flags))
        {
            error = s_i2cSendRestart(&bus);

            //
            // We know we are sending the message next, so send the message
            // direction.
            //
            if (error == FLCN_OK)
            {
                error = s_i2cSendAddress(pTa, &bus, pTa->bRead);
            }
        }
    }

    // If a block transaction read/write the size first!
    if ((error == FLCN_OK) &&
         FLD_TEST_DRF(_RM_FLCN, _I2C_FLAGS, _BLOCK_PROTOCOL, _ENABLED, flags))
    {
        error = s_i2cProcessByte(&bus, &messageSize, LW_FALSE, pTa->bRead);

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
    for (i = 0; (error == FLCN_OK) && (i < pTa->messageLength); i++)
    {
        // The last data byte requires special handling
        error = s_i2cProcessByte(&bus, &pTa->pMessage[i],
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
    error2 = s_i2cSendStop(&bus);
    if (error2 != FLCN_OK)
    {
        // Stop failure.
    }

    //
    // Bug 785366: Verify that the i2c bus is in a valid state. The data
    // line should be high.
    //
    if (!s_i2cIsLineHigh(&(bus.sda)))
    {
        //
        // another device may be pulling the line low.
        // Stop failure Data line Low.
        //

        // clock the bus - by reading in a byte.
        (void)s_i2cReadByte(&bus, &junk, LW_TRUE);

        // Attempt the Stop again.
        error2 = s_i2cSendStop(&bus);
        if (!s_i2cIsLineHigh(&(bus.sda)))
        {
            // Stop still can't restore Data line.
        }
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
 * where a secondary device is perpetually pulling SDA low for some unknown reason.
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
    I2C_PMU_TRANSACTION *pTa
)
{
    I2C_PMU_SW_BUS  bus;
    LwU32           flags  = pTa->pGenCmd->flags;
    LwU32           portId = DRF_VAL(_RM_FLCN, _I2C_FLAGS, _PORT, flags);
    LwU8            junk;

    s_i2cBusAndSpeedInit(&bus, DRF_VAL(_RM_FLCN, _I2C_FLAGS, _SPEED_MODE, flags),
                        portId);

    //
    // In all observed cases where recovery is necessary, performing a single-
    // byte read followed by sending a stop bit is sufficient to recover the
    // bus. It may be required to define a more elaborate recovery sequence in
    // the future where multiple recovery steps are performed and are perhaps
    // even based on the current "wedged" bus state.
    //
    s_i2cReadByte(&bus, &junk, LW_TRUE);
    s_i2cSendStop(&bus);

    return i2cIsBusReady_HAL(&I2c, portId) ?
        FLCN_OK : FLCN_ERR_I2C_BUS_ILWALID;
}

/*!
 * @brief   Drive an I2C line and wait specified time in ns for it to settle.
 *
 * @param[in]   line        Line index to drive.
 * @param[in]   pValue      Respective register address.
 * @param[in]   delayNs     Time [ns] for line to settle.
 */
static void
s_i2cDriveAndDelay
(
    LwU32   line,
    LwBool  bHigh,
    LwU32   delayNs
)
{
    REG_WR32(CSB,
             bHigh ? LW_CPWR_PMU_GPIO_OUTPUT_SET : LW_CPWR_PMU_GPIO_OUTPUT_CLEAR,
             BIT(line));
    if (delayNs != 0)
    {
        DELAY(delayNs);
    }
}

/*!
 * @brief   Wait for SCL to be released by the secondary.
 *
 * @param[in]   pBus    The bus information.
 *
 * @return  LW_TRUE     if waiting for SCL to get released timed out
 * @return  LW_FALSE    if SCL got released by the secondary
 */
static LwBool
s_i2cWaitSclReleasedTimedOut
(
    const I2C_PMU_SW_BUS *pBus
)
{
    return !OS_PTIMER_SPIN_WAIT_NS_COND(s_i2cIsLineHigh, &(pBus->scl),
                                        STRETCHED_LOW_TIMEOUT_NS);
}
