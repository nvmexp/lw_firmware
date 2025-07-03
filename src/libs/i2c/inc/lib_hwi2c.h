/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LIB_HWI2C_H
#define LIB_HWI2C_H

/*!
 * @file   lib_hwi2c.h
 * @brief  Library hwi2c header file
 */

/* ------------------------ System includes --------------------------------- */
#include "flcnifi2c.h"
/* ------------------------ Application includes ---------------------------- */
/* ------------------------ Defines ----------------------------------------- */
/* ------------------------ Types definitions ------------------------------- */
/* ------------------------ External definitions ---------------------------- */
/* ------------------------ Static variables -------------------------------- */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */

/*!
 * The following defines are used to determine how long to wait for another
 * I2C transaction to finish before failing.
 */
#define I2C_HW_IDLE_TIMEOUT_US 20000000

/*!
 * The longest HW I2C transaction: S BYTE*2 S BYTE*4 P, at 1 each for S/P, and
 * 9 for each byte (+ack).
 */
#define I2C_HW_MAX_CYCLES ((1 * 3) + (9 * 6))

/*!
 * We determine the HW operational timeout as the longest operation, plus two
 * long SCL clock stretches.
 */
#define I2C_HW_IDLE_TIMEOUT_NS (1000 * ((I2C_MAX_CYCLE_US * I2C_HW_MAX_CYCLES) + (I2C_SCL_CLK_TIMEOUT_1200US * 2)))

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
 * We don't want I2C to deal with traffic slower than 20 KHz (50 us cycle).
 */
#define I2C_MAX_CYCLE_US 50

/*!
 * @brief  Data structure used for internal communication for a hardware
 *         i2c transaction.
 */
typedef struct
{
    LwU32   portId;                       //!< The I2C port for transaction
    LwU32   bRead;                        //!< Whether transaction ilwolves reading.
    LwU32   cntl;                         //!< Control for a transaction
    LwU32   data;                         //!< Transaction data
    LwU32   bytesRemaining;               //!< Bytes remaining
    LwU32   status;                       //!< Transaction status
    LwU8   *pMessage;                     //!< Message address
    LwBool  bBlockProtocol;               //!< Whether transaction ilwolves block protocol
    LwBool  bStopRequired;                //!< Whether Stop signal must be sent
} I2C_HW_CMD;

#endif // LIB_HWI2C_H
