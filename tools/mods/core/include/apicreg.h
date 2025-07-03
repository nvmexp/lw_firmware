/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2007 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Registers, values that needed for ApicTest.

#ifndef INCLUDE_APICREG_H
#define INCLUDE_APICREG_H

// APIC control reg inside Mcp Pci-LPC (Leg)
#define LPC_PCI_APIC_CONTROL 0x74 // 8 bits

// Default Base Memory Address Must be 4K aligned

#define APIC_LOCAL_REG_BASE      0xfee00000
#define APIC_IO_REG_BASE         0xfec00000 // to 0xfec00100

#define MAX_ENTRY_OF_IVT 0xff
#define NUM_IRQS         16
#define NUM_AIRQS        24
#define PIC_MASK_0 0x21
#define PIC_MASK_1 0xa1

/*******************************************************************************
                Local Apic register memory mapping (offset)
*******************************************************************************/

// From IA-32 Intel  Architecture
// Software Developer's Manual Volume 3 : System Programming Guide

/*
APIC registers are mapped into the 4-KByte APIC register space. All registers
are 32 bits, 64 bits, or 256 bits in width, and all are aligned on 128-bit
boundaries. All 32-bit registers must be accessed using 128-bit aligned 32-bit
loads or stores. The wider registers (64-bit or 256-bit) must be accessed using
multiple 32-bit loads or stores, with the first access being 128-bit aligned.
If a LOCK prefix is used with a MOV instruction that accesses the APIC address
space, the prefix is ignored; that is, a locking operation does not take place.
*/

// #define Reserved 0x000
// #define Reserved 0x0010

#define APIC_LOCAL_REG_ID        0x020 //Local APIC ID Register Read/Write.
#define APIC_LOCAL_REG_VER       0x030 //Local APIC Version Register Read Only.

// #define Reserved 0xfee00040
// #define Reserved 0xfee00050
// #define Reserved 0xfee00060
// #define Reserved 0xfee00070

#define APIC_LOCAL_REG_TPR       0x080 //Task Priority Register (TPR) Read/Write.
#define APIC_LOCAL_REG_APR       0x090 //Arbitration Priority Register 1 (APR) Read Only.
#define APIC_LOCAL_REG_PPR       0x0A0 //Processor Priority Register (PPR) Read Only.
#define APIC_LOCAL_REG_EOI       0x0B0 //EOI Register Write Only.
// #define Reserved 0xfee000C0
#define APIC_LOCAL_REG_LDR       0x0D0 //Logical Destination Register Read/Write.
#define APIC_LOCAL_REG_DFR       0x0E0 //Destination Format Register Bits 0-27 Read only; bits 28-31Read/Write.
#define APIC_LOCAL_REG_SILWR     0x0F0 //Spurious Interrupt Vector Register Bits 0-8 Read/Write; bits 9-31Read Only.
#define APIC_LOCAL_REG_SILWR_SW_EN 0x00000100

// Interrupt resgister size
#define APIC_LOCAL_REG_INT_SIZE  7

#define APIC_LOCAL_REG_ISR(i)    (0x100+(i)*0x10) //In-Service Register (ISR) Read Only.
// thtough 0x170

#define APIC_LOCAL_REG_TMR(i)    (0x180+(i)*0x10) //Trigger Mode Register (TMR) Read Only.
// through 0x1F0

#define APIC_LOCAL_REG_IRR(i)    (0x200+(i)*0x10) //Interrupt Request Register (IRR) Read Only.
// through 0x270

#define APIC_LOCAL_REG_ESR       0x280 //Error Status Register Read Only.

// #define Reserved  0xfee00290 through 0xfee002F0

#define APIC_LOCAL_REG_ICR_LO    0x300 //Interrupt Command Register (ICR) [0-31] Read/Write.
#define APIC_LOCAL_REG_ICR_HI    0x310 //Interrupt Command Register (ICR) [32-63] Read/Write.
#define APIC_LOCAL_REG_LVT_TR    0x320 //LVT Timer Register Read/Write.
#define APIC_LOCAL_REG_LVT_RSR   0x330 //LVT Thermal Sensor Register 2 Read/Write.
#define APIC_LOCAL_REG_LVT_PMC   0x340 //LVT Performance Monitoring Counters Read/Write.
#define APIC_LOCAL_REG_LVT_LINT0 0x350 //LVT LINT0 Register Read/Write.
#define APIC_LOCAL_REG_LVT_LINT1 0x360 //LVT LINT1 Register Read/Write.
#define APIC_LOCAL_REG_LVT_ER    0x370 //LVT Error Register Read/Write.
#define APIC_LOCAL_REG_ICR       0x380 //Initial Count Register (for Timer) Read/Write.
#define APIC_LOCAL_REG_CCR       0x390 //Current Count Register (for Timer) Read Only.

// #define Reserved 0xfee003A0 through 0xfee003D0

#define APIC_LOCAL_REG_DCR       0x3E0 //Divide Configuration Register (for Timer) Read/Write.
#define APIC_LOCAL_REG_LAST      0x3F0 //Reserved

/*******************************************************************************
                      IO Apic register memory mapping (offset)
*******************************************************************************/
// From 82093AA I/O ADVANCED PROGRAMMABLE INTERRUPT CONTROLLER (IOAPIC)

/*The IOAPIC registers are accessed by an indirect addressing scheme using two
registers (IOREGSEL andIOWIN) that are located in the CPU's memory space (memory
address specified by the APICBASE Register located in the PIIX3). These two
registers are re-locateable (via the APICBASE Register) as shown in Table 3.1.In
the IOAPIC only the IOREGSEL and IOWIN Registers are directly accesable in the
memory address space.
*/

#define APIC_IO_REG_SEL      0x000
#define APIC_IO_REG_DATA     0x010

// Register index

//IOAPIC ID R/W
#define APIC_IO_REG_ID        0x00
#define APIC_IO_REG_ID_ID     27:24
#define APIC_IO_REG_ID_VAL    (1<<24)

// IOAPIC Version RO
#define APIC_IO_REG_VER                0x01
#define APIC_IO_REG_VER_VERSTION       7:0
#define APIC_IO_REG_VER_VERSTION_VAL   0x11
#define APIC_IO_REG_VER_MAX_ENTRY      23:16
#define APIC_IO_REG_VER_MAX_ENTRY_VAL  0x17

//IOAPIC Arbitration ID RO
#define APIC_IO_REG_ARB       0x02
#define APIC_IO_REG_ARB_ID    27:24

//Redirection Table (Entries 0-23) (64 bits each) R/W
// Range from 0x10 - 0xfe
#define APIC_IO_REG_TBL_SIZE        24

// 0x10 - 0x3f
#define APIC_IO_REG_TBL_LO(i)    (0x10+i*2)

#define APIC_IO_REG_TBL_LO_INTVEC   7:0

#define APIC_IO_REG_TBL_LO_DELMODE        10:8
#define APIC_IO_REG_TBL_LO_DELMODE_FIXED  (0<<8)
#define APIC_IO_REG_TBL_LO_DELMODE_LOWEST (1<<8)
#define APIC_IO_REG_TBL_LO_DELMODE_SMI    (2<<8)
#define APIC_IO_REG_TBL_LO_DELMODE_RES1   (3<<8)
#define APIC_IO_REG_TBL_LO_DELMODE_NMI    (4<<8)
#define APIC_IO_REG_TBL_LO_DELMODE_INIT   (5<<8)
#define APIC_IO_REG_TBL_LO_DELMODE_RES2   (6<<8)
#define APIC_IO_REG_TBL_LO_DELMODE_EXTINT (7<<8)

#define APIC_IO_REG_TBL_LO_DESTMOD        11:11
#define APIC_IO_REG_TBL_LO_DESTMOD_PHY    (0<<11)
#define APIC_IO_REG_TBL_LO_DESTMOD_LOG    (1<<11)

#define APIC_IO_REG_TBL_LO_DELIVS         12:12
#define APIC_IO_REG_TBL_LO_DELIVS_SET     (1<<12)

#define APIC_IO_REG_TBL_LO_INTPOL         13:13
#define APIC_IO_REG_TBL_LO_INTPOL_HIGH    (0<<13)
#define APIC_IO_REG_TBL_LO_INTPOL_LOW     (1<<13)

#define APIC_IO_REG_TBL_LO_IRR            14:14
#define APIC_IO_REG_TBL_LO_IRR_SET        (1<<14)

#define APIC_IO_REG_TBL_LO_TRIGMOD        15:15
#define APIC_IO_REG_TBL_LO_TRIGMOD_EDGE   (0<<15)
#define APIC_IO_REG_TBL_LO_TRIGMOD_LEV    (1<<15)

#define APIC_IO_REG_TBL_LO_INT_MASK       16:16
#define APIC_IO_REG_TBL_LO_INT_MASK_CLR   (0<<16)
#define APIC_IO_REG_TBL_LO_INT_MASK_SET   (1<<16)

//if Physical ModeIOREDTBLx[59:56] = APIC ID
//if Logical Mode IOREDTBLx[63:56] = Set of processors
#define APIC_IO_REG_TBL_HI(i)     (0x11+i*2)
#define APIC_IO_REG_TBL_HI_DST    31:24
#define APIC_IO_REG_LAST          APIC_IO_REG_TBL_HI(23)

// Bit fields of each register
// APIC_IO_REG_ID default 0x00170011

#endif
