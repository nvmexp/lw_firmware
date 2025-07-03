/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2005-2007 by LWPU Corporation.  All rights reserved.  All
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

#ifndef INCLUDED_SMBREG_H
#define INCLUDED_SMBREG_H

#define SMBUS_DEFAULT_HOST_ADRS  0x08
#define SMBUS_MAX_DATA_LOAD      0x20

//----------------------------------------------------------------------------//
//          PCI Address Space
//----------------------------------------------------------------------------//
#define SMBUS_PCI_DDC_BASE_OFFSET            0x10  // cfg 4
#define SMBUS_PCI_CNTRL0_BASE_OFFSET         0x20  // Private
#define SMBUS_PCI_CNTRL1_BASE_OFFSET         0x24  // Private
#define SMBUS_PCI_PM_CAPABILITIES_OFFSET     0x44  // cfg 17
#define SMBUS_PCI_PM_CNTL_STATUS_OFFSET      0x48  // cfg 18
//MCP89 use different BARs for SMB?
//#define SMBUS_MCP89_CNTRL0_BASE_OFFSET          0x18  //cfg 6
//#define SMBUS_MCP89_CNTRL1_BASE_OFFSET          0x1c  //cfg 7

//----------------------------------------------------------------------------//
// SMBUS  Register code
// see https://p4viewer.lwpu.com/get///hw/mcp55n/manuals/dev_mcp_smb.ref
//----------------------------------------------------------------------------//

#define SMBUS_REG_BASE_MASK        0xf00
#define SMBUS_REG_OFFSET_MASK      0x0ff

// Register Code definition

#define SMBUS_REG_DATA_SIZE        0x020
#define SMBUS_REG_PRTCL            0x000
#define SMBUS_REG_STS              0x001
#define SMBUS_REG_ADDR             0x002
#define SMBUS_REG_CMD              0x003
#define SMBUS_REG_DATA_BASE        0x004 // Data Array size = 0x20 Byte
#define SMBUS_REG_BCNT             0x024 // only bit 0-4 , 5-7 = 0
#define SMBUS_REG_ALARM_ADDR       0x025
#define SMBUS_REG_ALARM_DATA_LO    0x026
#define SMBUS_REG_ALARM_DATA_HI    0x027

// Legacy registers (for Smbus1) not useful for now.
#define SMBUS_REG_HST10BA          0x038
#define SMBUS_REG_SLV10BA          0x039
#define SMBUS_REG_HASADDR          0x03a
#define SMBUS_REG_SNPADDR          0x03b
#define SMBUS_REG_STATUS           0x03c // 16 bit
#define SMBUS_REG_CTRL             0x03e // 16 bit

// DDB Registers
#define SMBUS_REG_DDC_IN0      0x200
#define SMBUS_REG_DDC_OUT0     0x201
#define SMBUS_REG_DDC_IN1      0x204
#define SMBUS_REG_DDC_OUT1     0x205

//***************************** bit field definitions ****************

// PRTCL         0x00
#define PRTCL_PROTOCAL_MASK      0x7f // bit 0-6
#define PRTCL_NOT_IN_USE   0x0
#define PRTCL_RSVD         0x1
#define PRTCL_WR_QUICK     0x2
#define PRTCL_RD_QUICK     0x3
#define PRTCL_SND_BYTE     0x4
#define PRTCL_RCV_BYTE     0x5
#define PRTCL_WR_BYTE      0x6
#define PRTCL_RD_BYTE      0x7
#define PRTCL_WR_WORD      0x8
#define PRTCL_RD_WORD      0x9
#define PRTCL_WR_BLK       0xa
#define PRTCL_RD_BLK       0xb
#define PRTCL_PROCESS_CALL 0xc
#define PRTCL_UNSUPPORTED  0xd
#define PRTCL_PEC_EN             0x80 // bit 7

//STS            0x01
#define STS_STATUS_MASK  0x1f // bit 0 - 4
#define STS_STATUS_OK           0x00
#define STS_STATUS_NAK          0x10
#define STS_STATUS_TIMEOUT      0x18
#define STS_STATUS_UNSUP_PRTCL  0x19
#define STS_STATUS_SMBUS_BUSY   0x1a
#define STS_STATUS_PEC_ERROR    0x1f

#define STS_ALARM_ACTIVE 0x40 // bit 6
#define STS_DONE_OK      0x80 // bit 7

//ADDR (All addresses)
#define ADDR_SHIFT      1

#endif  // INCLUDED_SMBREG_H
