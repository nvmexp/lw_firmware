/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2007-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LIB_SWI2C_H
#define LIB_SWI2C_H

/*!
 * @file   lib_swi2c.h
 * @brief  Library swi2c header file
 */

/* ------------------------ System includes --------------------------------- */
#include "lib_i2c.h"
#include "flcnifi2c.h"
/* ------------------------ Application includes ---------------------------- */
/* ------------------------ Defines ----------------------------------------- */
/* ------------------------ Types definitions ------------------------------- */
/* ------------------------ External definitions ---------------------------- */
/* ------------------------ Static variables -------------------------------- */
/* ------------------------ Function Prototypes ----------------------------- */

#define BITS_PER_BYTE               8

#define HIGH                        LW_TRUE
#define LOW                         LW_FALSE

/*! Extract the first byte of a 10-bit address. */
#define GET_ADDRESS_10BIT_FIRST(a)  ((LwU8)((((a) >> 8) & 0x6) | 0xF0))

/*! Extract the second byte of a 10-bit address. */
#define GET_ADDRESS_10BIT_SECOND(a) ((LwU8)(((a) >> 1) & 0xFF))

/*! Attaching read to read application interface */
#define I2C_READ(a,b)               i2cRead_HAL(&I2c, a, b)

/*! Abstract which timer we are using to perform delays. */
#define I2C_DELAY(a)                OS_PTIMER_SPIN_WAIT_NS(a)

#define I2C_SET_BIT(v, p, b)        (((b) << (p)) | ((v) & ~(1 << (p))))

/*! The number of nanoseconds we will wait for slave clock stretching.
 *  Previously, this was set to 100us, but proved too
 *  short (see bug 630691) so was increased to 2ms.
 */
#define I2C_STRETCHED_LOW_TIMEOUT_NS 2000000

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

/*! Structure containing a description of the bus as needed by the software
 *  bitbanging implementation.
 */
typedef struct
{
    LwU32 sclOut;      // Bit number for SCL Output
    LwU32 sdaOut;      // Bit number for SDA ooutput

    LwU32 sclIn;       // Bit number for SCL Input
    LwU32 sdaIn;       // Bit number for SDA Input

    LwU32 port;        // Port number of the driving lines
    LwU32 lwrLine;     // Required for isLineHighFunction

    LwU32 regCache;    // Keeps the cache value of registers.
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
} I2C_SW_BUS;

#endif // LIB_SWI2C_H
