/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2010,2015 by LWPU Corporation.  All rights reserved.  All
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

#ifndef INCLUDED_INTEL_SMBREG_H
#define INCLUDED_INTEL_SMBREG_H

#define INTEL_SMBUS_MAX_DATA_LOAD      0x20

//----------------------------------------------------------------------------//
//          PCI Address Space
//----------------------------------------------------------------------------//
#define INTEL_SMBUS_PCI_DDC_BASE_OFFSET            0x10  // cfg 4
#define INTEL_SMBUS_PCI_CNTRL0_BASE_OFFSET         0x20  // Private
#define INTEL_SMBUS_PCI_CNTRL1_BASE_OFFSET         0x24  // Private
#define INTEL_SMBUS_PCI_PM_CAPABILITIES_OFFSET     0x44  // cfg 17
#define INTEL_SMBUS_PCI_PM_CNTL_STATUS_OFFSET      0x48  // cfg 18

//----------------------------------------------------------------------------//
// INTEL_SMBUS  Register code
//----------------------------------------------------------------------------//

#define INTEL_SMBUS_REG_BASE_MASK        0xf00
#define INTEL_SMBUS_REG_OFFSET_MASK      0x0ff
#define INTEL_SMBUS_STS_CLEAR            0x0fe
#define INTEL_SMBUS_STS_BUSY             0x001

// Register Code definition

#define INTEL_SMBUS_REG_DATA_SIZE        0x020
#define INTEL_SMBUS_REG_STS              0x000
#define INTEL_SMBUS_REG_CNT              0x002
#define INTEL_SMBUS_REG_CMD              0x003
#define INTEL_SMBUS_REG_ADDR             0x004
#define INTEL_SMBUS_REG_HST_D0           0x005
#define INTEL_SMBUS_REG_HST_D1           0x006
#define INTEL_SMBUS_REG_HST_BLOCK_DB     0x007
#define INTEL_SMBUS_REG_PEC              0x008
#define INTEL_SMBUS_REG_RCV_SLVA         0x009
#define INTEL_SMBUS_REG_SLV_DATA         0x00A
#define INTEL_SMBUS_REG_SLV_DATB         0x00B
#define INTEL_SMBUS_REG_AUX_STS          0x00C
#define INTEL_SMBUS_REG_AUX_CTL          0x00D
#define INTEL_SMBUS_REG_SMLINK_PIN_CTL   0x00E
#define INTEL_SMBUS_REG_BCNT             0x005 // only bit 0-4 , 5-7 = 0

// DDB Registers
#define INTEL_SMBUS_REG_DDC_IN0      0x200
#define INTEL_SMBUS_REG_DDC_OUT0     0x201
#define INTEL_SMBUS_REG_DDC_IN1      0x204
#define INTEL_SMBUS_REG_DDC_OUT1     0x205

//***************************** bit field definitions ****************

// PRTCL         0x00
#define INTEL_PRTCL_PROTOCOL_MASK      0x03 // bit 0-6
#define INTEL_PRTCL_NOT_IN_USE   0x0
#define INTEL_PRTCL_RSVD         0x1
#define INTEL_PRTCL_WR_QUICK     0x2
#define INTEL_PRTCL_RD_QUICK     0x3
#define INTEL_PRTCL_SND_BYTE     0x4
#define INTEL_PRTCL_RCV_BYTE     0x5
#define INTEL_PRTCL_WR_BYTE      0x6
#define INTEL_PRTCL_RD_BYTE      0x7
#define INTEL_PRTCL_WR_WORD      0x8
#define INTEL_PRTCL_RD_WORD      0x9
#define INTEL_PRTCL_WR_BLK       0xa
#define INTEL_PRTCL_RD_BLK       0xb
#define INTEL_PRTCL_PROCESS_CALL 0xc
#define INTEL_PRTCL_UNSUPPORTED  0xd
#define INTEL_PRTCL_PEC_EN             0x80 // bit 7
#define INTEL_PRTCL_START              0x40 // bit 6
#define INTEL_PRTCL_BYTE               0x04 // bit 7
#define INTEL_PRTCL_BYTE_DATA          0x08 // bit 7
#define INTEL_PRTCL_WORD_DATA          0x0C // bit 7
#define INTEL_PRTCL_PROC               0x18 // bit 7
#define INTEL_PRTCL_BLOCK              0x14 // bit 7

//STS            0x01
#define INTEL_STS_STATUS_MASK  0x1f // bit 0 - 4
#define INTEL_STS_STATUS_OK           0x00
#define INTEL_STS_STATUS_NAK          0x10
#define INTEL_STS_STATUS_TIMEOUT      0x18
#define INTEL_STS_STATUS_UNSUP_PRTCL  0x19
#define INTEL_STS_STATUS_SMBUS_BUSY   0x1a
#define INTEL_STS_STATUS_PEC_ERROR    0x1f

#define INTEL_STS_ALARM_ACTIVE 0x40 // bit 6
#define INTEL_STS_DONE_OK      0x02 // bit 7
#define INTEL_STS_BUS_ERR      0x01 // bit 7

#define INTEL_AUXCTL_32B_BUF_EN  0x02 // bit 7

//ADDR (All addresses)
#define INTEL_ADDR_SHIFT      1
#define INTEL_ADDR_WRITE      0
#define INTEL_ADDR_READ       1

#endif  // INCLUDED_INTEL_SMBREG_H
