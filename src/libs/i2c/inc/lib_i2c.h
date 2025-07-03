/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LIB_I2C_H
#define LIB_I2C_H

/*!
 * @file lib_i2c.h
 */
/* ------------------------- Application Includes --------------------------- */
#include "flcntypes.h"

/* ------------------------- Macros and Defines ----------------------------- */
#define MAX_BR04_I2C_PORTS              1
#define MAX_GPU_I2C_PORTS               15
#define MAX_I2C_PORTS                   (MAX_GPU_I2C_PORTS+MAX_BR04_I2C_PORTS)

/*!
 * I2C mutex timeout value, specified in ns.  Current value is 1000ms.
 */
#define I2C_MUTEX_ACQUIRE_TIMEOUT_NS                1000000000

//! Maximum size of index.
#define LW_I2C_INDEX_MAX                            4

/*! Set if the command should begin with a START.  For a transactional
 *  interface (highly recommended), this should always be _YES.
 */
#define LW_I2C_FLAGS_START                          0:0
#define LW_I2C_FLAGS_START_NONE                       0
#define LW_I2C_FLAGS_START_SEND                       1

/*!
 *  Indicate whether to send a repeated start between the index and
 *  message phrases.
 *
 *  This flag will send a restart between each index and message.  This should
 *  be set for reads, but rarely (if ever) for writes.
 *
 *  A RESTART is required when switching directions; this is called a combined
 *  format.  These are typically used in indexed read commands, where an index
 *  is written to the device to indicate what register(s) to read, and then
 *  the register is read.  Almost always, indexed writes do not require a
 *  restart, though some devices will accept them.  However, this flag should
 *  be used for writes in the rare case where a restart should be sent between
 *  the last index and the message.
 */
#define LW_I2C_FLAGS_RESTART                        1:1
#define LW_I2C_FLAGS_RESTART_NONE                     0
#define LW_I2C_FLAGS_RESTART_SEND                     1

/*! Set if the command should conclude with a STOP.  For a transactional
 *  interface (highly recommended), this should always be _SEND.
 */
#define LW_I2C_FLAGS_STOP                           2:2
#define LW_I2C_FLAGS_STOP_NONE                        0
#define LW_I2C_FLAGS_STOP_SEND                        1

/*! The slave addressing mode: 7-bit (most common) or 10-bit.  It is possible
 *  but not recommended to send no address at all using _NONE.
 */
#define LW_I2C_FLAGS_ADDRESS_MODE                   4:3
#define LW_I2C_FLAGS_ADDRESS_MODE_NO_ADDRESS          0
#define LW_I2C_FLAGS_ADDRESS_MODE_7BIT                1
#define LW_I2C_FLAGS_ADDRESS_MODE_10BIT               2

//! The length of the index.  If length is 0, no index will be sent.
#define LW_I2C_FLAGS_INDEX_LENGTH                   7:5
#define LW_I2C_FLAGS_INDEX_LENGTH_ZERO                0
#define LW_I2C_FLAGS_INDEX_LENGTH_ONE                 1
#define LW_I2C_FLAGS_INDEX_LENGTH_TWO                 2
#define LW_I2C_FLAGS_INDEX_LENGTH_THREE               3
#define LW_I2C_FLAGS_INDEX_LENGTH_MAXIMUM             4

/*! The flavor to use: software bit-bang or hardware controller.  The hardware
 *  controller is faster, but is not necessarily available or capable.
 */
#define LW_I2C_FLAGS_FLAVOR                         8:8
#define LW_I2C_FLAGS_FLAVOR_HW                        0
#define LW_I2C_FLAGS_FLAVOR_SW                        1

/*! The target speed at which to drive the transaction at.
 *
 *  Note: The lib reserves the right to lower the speed mode if the I2C master
 *  implementation cannot handle the speed given.
 */
#define LW_I2C_FLAGS_SPEED_MODE                    11:9
#define LW_I2C_FLAGS_SPEED_MODE_3KHZ         0x00000000
#define LW_I2C_FLAGS_SPEED_MODE_10KHZ        0x00000001
#define LW_I2C_FLAGS_SPEED_MODE_33KHZ        0x00000002
#define LW_I2C_FLAGS_SPEED_MODE_100KHZ       0x00000003
#define LW_I2C_FLAGS_SPEED_MODE_200KHZ       0x00000004
#define LW_I2C_FLAGS_SPEED_MODE_300KHZ       0x00000005
#define LW_I2C_FLAGS_SPEED_MODE_400KHZ       0x00000006

//! The physical port id describing which I2C bus to use.
#define LW_I2C_FLAGS_PORT                         16:12

/*!
 * Block Reads/Writes: There are two different protocols for reading/writing >2
 * byte sets of data to/from a slave device.  The SMBus specification section
 * 5.5.7 defines "Block Reads/Writes" in which the first byte of the payload
 * specifies the size of the data to be read/written s.t. payload_size =
 * data_size + 1.  However, many other devices depend on the master to already
 * know the size of the data being accessed (i.e. SW written with knowledge of
 * the device's I2C register spec) and skip this overhead.  This second behavior
 * is actually the default behavior of all the lib's I2C interfaces.
 *
 * Setting this bit will enable the block protocol for reads and writes for size
 * >2.
 */
#define LW_I2C_FLAGS_BLOCK_PROTOCOL               17:17
#define LW_I2C_FLAGS_BLOCK_PROTOCOL_DISABLED 0x00000000
#define LW_I2C_FLAGS_BLOCK_PROTOCOL_ENABLED  0x00000001

/*!
 * The direction determines, read or write request.
 */
#define LW_I2C_FLAGS_DIR                          18:18
#define LW_I2C_FLAGS_DIR_WRITE               0x00000000
#define LW_I2C_FLAGS_DIR_READ                0x00000001

/*!
 * @brief  Data structure used to communicate between the I2C management
 *         subsystem and the I2C transaction subsystems without revealing too
 *         much detail as to how the commands are structured.
 */
typedef struct
{
    LwU8    index[LW_I2C_INDEX_MAX];      //!< Optional indexing data; aka register address
    LwU32   flags;                        //!< The I2C flag for transaction
    LwU8    *pMessage;                    //!< The message (required.) Input or output
    LwU16   i2cAddress;                   //!< The I2C device address
    LwU16   messageLength;                //!< The length of the message
    LwBool  bRead;                        //!< Whether transaction ilwolves reading
} I2C_TRANSACTION, *PI2C_TRANSACTION;

#endif // LIB_I2C_H
