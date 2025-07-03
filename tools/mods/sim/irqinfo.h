/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifdef PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#ifndef INCLUDED_IRQINFO_H
#define INCLUDED_IRQINFO_H

#define MASK_INFO_LIST_SIZE 16 // maximum number of entries in regInfoList[]

// supported irq types should match with
// //sw/mods/linuxdrv/v3.2/mods.h
// drivers/unix/rmapi_tegra/mirror/mirror/tegra_top/core/include/linux/mods.h
// //hw/tools/shimlib/ifspec3/core/include/irqinfo.h
#define MODS_IRQ_TYPE_INT  0
#define MODS_IRQ_TYPE_MSI  1
#define MODS_IRQ_TYPE_CPU  2 // for CheetAh
#define MODS_IRQ_TYPE_MSIX 3
#define MODS_IRQ_NUM_TYPES 4
#define MODS_IRQ_TYPE_MASK 0xff

/* Standard PCI device information */
typedef struct PciInfo
{
    LwU016 domain;
    LwU016 bus;
    LwU016 device;
    LwU016 function;
}PciInfo;

/* Structure to define how to mask off an interrupt for given register */
typedef struct MaskInfo
{
    LwU008 maskType;    // type of mask to use, either MODS_MASK_TYPE_IRQ_DISABLE
                        // or MODS_MASK_TYPE_IRQ_DISABLE64. 
    LwU032 offset;      // offset from the start of barAddr
    LwU064 andMask;     // mask to clear bits
    LwU064 orMask;      // mask to set bits
}MaskInfo;

/* Structure to define all necessary information for masking off multiple interrupt
 * sources that feed into a single interrupt.
 */
typedef struct IrqInfo
{
    LwU032 irqNumber;   //
    LwU064 barAddr;     // physical address of the BAR
    LwU032 barSize;     // size in bytes of the BAR
    PciInfo pciDev;
    LwU008 irqType;     // type of IRQ, s/b MODS_IRQ_TYPE_INT
    LwU032 maskInfoCount; // Number of entries in the maskInfoList[]
    MaskInfo maskInfoList[MASK_INFO_LIST_SIZE];  
}IrqInfo;

/* Structure to define how to mask off an interrupt for given register 
 * The assumption is that the interrupts will be masked using the following algorithm 
 * *irqDisableOffset = ((*irqEnabledOffset & andMask) | orMask) 
 */
typedef struct MaskInfo2
{
    LwU008 maskType;    // type of mask to use, either MODS_MASK_TYPE_IRQ_DISABLE
                        // or MODS_MASK_TYPE_IRQ_DISABLE64. 
    LwU032 irqPendingOffset;   // register to read IRQ pending status
    LwU032 irqEnabledOffset;   // register to read the enabled interrupts
    LwU032 irqEnableOffset;    // register to write to enable interrupts
    LwU032 irqDisableOffset;   // register to write to disable interrupts
    LwU064 andMask;     // mask to clear bits
    LwU064 orMask;      // mask to set bits
}MaskInfo2;

typedef struct IrqInfo2
{
    LwU032 irqNumber;   // IRQ number to hook.
    LwU064 barAddr;     // physical address of the Base Address Region (BAR)
    LwU032 barSize;     // number of byte to may within the BAR
    PciInfo pciDev;     // PCI's domain,bus,device,function values
    LwU008 irqType;     // type of IRQ to hook. s/b one of the following
                        // - MODS_IRQ_TYPE_INT (legacy INTA)
                        // - MODS_IRQ_TYPE_MSI
                        // - MODS_IRQ_TYPE_CPU (for CheetAh)
    LwU032 maskInfoCount; // Number of entries in the maskInfoList[]
    MaskInfo2 maskInfoList[MASK_INFO_LIST_SIZE];  //see above
}IrqInfo2;

#endif

