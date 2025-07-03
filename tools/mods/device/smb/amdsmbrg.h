/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file smbreg.h
//! \brief Header file with macros for Smbus registers
//!
//! Contains macro definitions for Smbus registers
//!

#ifndef INCLUDED_AMD_SMBREG_H
#define INCLUDED_AMD_SMBREG_H

// AMD SMBus Register Offsets
#define AMD_SMBUS_REG_HSTSTS            0x000
#define AMD_SMBUS_REG_SLVSTS            0x001
#define AMD_SMBUS_REG_HSTCTRL           0x002
#define AMD_SMBUS_REG_HSTCMD            0x003
#define AMD_SMBUS_REG_HSTADDR           0x004
#define AMD_SMBUS_REG_HSTDAT0           0x005
#define AMD_SMBUS_REG_HSTDAT1           0x006
#define AMD_SMBUS_REG_BLKDAT            0x007
#define AMD_SMBUS_REG_SLVCTRL           0x008
#define AMD_SMBUS_REG_SHDWCMD           0x009
#define AMD_SMBUS_REG_SLVEVTA           0x00A
#define AMD_SMBUS_REG_SLVEVTB           0x00B
#define AMD_SMBUS_REG_SLVDATA           0x00C
#define AMD_SMBUS_REG_SLVDATB           0x00D

// Address register (AMD_SMBUS_REG_HSTADDR) bit 0
#define AMD_SMBUS_ADDR_READ         1
#define AMD_SMBUS_ADDR_WRITE        0

// Status register (AMD_SMBUS_REG_HSTSTS)
#define AMD_SMBUS_STATUS_PEC_ERROR           0x10 // bit 4
#define AMD_SMBUS_STATUS_BUS_COLLISION       0x08 // bit 3
#define AMD_SMBUS_STATUS_DEVICE_ERROR        0x04 // bit 2
#define AMD_SMBUS_STATUS_INTERRUPT           0x02 // bit 1
#define AMD_SMBUS_STATUS_HOST_BUSY           0x01 // bit 0

// Control register (AMD_SMBUS_REG_HSTCTRL)
#define AMD_SMBUS_HSTCTRL_START              0x40 // bit 6
#define AMD_SMBUS_HSTCTRL_PROTO_MASK         0x1c // SMBusProtocol field (bits 4:2)
#define AMD_SMBUS_HSTCTRL_PROTO_QUICK        0x00
#define AMD_SMBUS_HSTCTRL_PROTO_BYTE         0x04
#define AMD_SMBUS_HSTCTRL_PROTO_BYTE_DATA    0x08
#define AMD_SMBUS_HSTCTRL_PROTO_WORD_DATA    0x0c
#define AMD_SMBUS_HSTCTRL_PROTO_BLOCK        0x14

// Slv control register (AMD_SMBUS_REG_SLVCTRL)
#define AMD_SMBUS_SLVCTRL_CLEAR_HOST_SEMA    0x20 // bit 5
#define AMD_SMBUS_SLVCTRL_HOST_SEMA          0x10 // bit 4

// Power Management (PM) registers
#define AMD_SMBUS_PORT_PM_IDX              0xcd6
#define AMD_SMBUS_PORT_PM_DATA             0xcd7
// Register PMx00 (DecodeEn)
#define AMD_SMBUS_PM_00_ENABLE_OFFSET    0x00 // PMx00 byte 0 (bits 0:7)
#define AMD_SMBUS_PM_00_ENABLED          0x10 // SmbusAsfIoEn (bit 4 of PMx00 byte 0)
#define AMD_SMBUS_PM_00_ADDR_OFFSET      0x01 // SmbusAsfIoBase (PMx00 byte 1)
#define AMD_SMBUS_PM_00_PORTSEL_OFFSET   0x02 // PMx00 byte 2 (bits 16:23)
#define AMD_SMBUS_PM_00_PORTSEL_MASK     0x18 // SmBus0Sel (bits 3:4 of PMx00 byte 2)
#define AMD_SMBUS_PM_00_PORTSEL_2        0x08

#define AMD_SMBUS_BASE_ADDR_SHIFT     8 // SmbusAsfIoBase is the upper byte of SMBus base address
#define AMD_SMBUS_BASE_ADDR_LOWER  0x20 // Lower byte is 0x00 for default Smbus and 0x20 for ASF

#define AMD_SMBUS_BLOCK_MAX          32

#endif  // INCLUDED_AMD_SMBREG_H
