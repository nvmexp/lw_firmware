#ifndef _ec_smbus_h__included_
#define _ec_smbus_h__included_

/*
INTRODUCTION

The GPU board EC supports standard SMBus protocols as defined in the System
Management Bus (SMBus) Specification Version 2.0.

\\netapp-hq\specs\SMBus\smbus2.0.pdf

The use of standard SMBus protocols aims to ensure that it is easy to develop
software that uses the SMBus host to communicate with the GPU board EC.

SMBus Protocols

Key
    S       Start Condition
    SR      Repeated Start Condition
    RD      Read (bit value of 1)
    WR      Write (bit value of 0)
    A       Acknowledge (ACK, bit value of 0)
    A*      Not Acknowledge (NACK, bit value of 1)
    P       Stop Condition
    ADDR    Slave Address
    CMD     Command Code

Write Byte
    .--------------------------------.
    |S| ADDR |WR|A| CMD |A| DATA |A|P|
    '--------------------------------'

Write Word
    .---------------------------------------------.
    |S| ADDR |WR|A| CMD |A| DATA_L |A| DATA_H |A|P|
    '---------------------------------------------'

Read Byte
    .------------------------------------------------.
    |S| ADDR |WR|A| CMD |A|SR| ADDR |RD|A| DATA |A*|P|
    '------------------------------------------------'

Read Word
    .-------------------------------------------------------------.
    |S| ADDR |WR|A| CMD |A|SR| ADDR |RD|A| DATA_L |A| DATA_H |A*|P|
    '-------------------------------------------------------------'

SMBus Command Summary

    Command Command                         Supported Protocols
    Code    Description                     WB  WW  RB  RW

    * Register Access Commands *

    0x20    Write/Read Slave Address        x       x
    0x30    Write/Read Register Index       x   x   x   x
    0x31    Write/Read Register Data        x   x   x   x
*/

/*
Slave Address:

 7             0
+-+-+-+-+-+-+-+-.
|             |0| SLAVE_ADDR
+-+-+-+-+-+-+-+-'
*/
#define LW_EC_SMBUS_SLAVE_ADDR                          0x00000020 /* RWI1R */

#define LW_EC_SMBUS_SLAVE_ADDR_VALUE                           7:1 /* RWIVF */
#define LW_EC_SMBUS_SLAVE_ADDR_VALUE_INIT               0x00000014 /* RWI-V */

/*
REGISTER ACCESS

A 4 KB register space is accessible through an index/data port.

Register Index:

OFFSET
  This field specifies the byte offset into the 4 KB register space for writes
  and reads. This field is incremented after each byte written to REG_DATA if
  AINCW is enabled and is incremented after each byte read from REG_DATA if
  AINCR is enabled.

AINCW
  This field enables or disables auto-increment of OFFSET after a write..

AINCR
  This field enables or disables auto-increment of OFFSET after a read.

 15            8 7             0
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-.
| | |0 0|                       | REG_INDEX
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-'
*/
#define LW_EC_SMBUS_REG_INDEX                           0x00000030 /* RWI2R */

#define LW_EC_SMBUS_REG_INDEX_OFFSET                          11:0 /* RWIVF */
#define LW_EC_SMBUS_REG_INDEX_OFFSET_INIT               0x00000000 /* RWI-V */

#define LW_EC_SMBUS_REG_INDEX_AINCW                          14:14 /* RWIVF */
#define LW_EC_SMBUS_REG_INDEX_AINCW_DISABLE             0x00000000 /* RWI-V */
#define LW_EC_SMBUS_REG_INDEX_AINCW_ENABLE              0x00000001 /* RW--V */

#define LW_EC_SMBUS_REG_INDEX_AINCR                          15:15 /* RWIVF */
#define LW_EC_SMBUS_REG_INDEX_AINCR_DISABLE             0x00000000 /* RWI-V */
#define LW_EC_SMBUS_REG_INDEX_AINCR_ENABLE              0x00000001 /* RW--V */

/*
Register Data:

 15            8 7             0
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-.
|                               | REG_DATA
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-'
*/
#define LW_EC_SMBUS_REG_DATA                            0x00000031 /* RW-2R */

#define LW_EC_SMBUS_REG_DATA_VALUE                            15:0 /* RW--F */

#endif // _ec_smbus_h__included_
