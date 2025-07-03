/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/* Note: We can't use namespaces in this file because its called from both C
 * and C++ files.
 */
#pragma once

#include "types.h"

#define MASK_PARAMS_LIST_SIZE 16 /* maximum number of entries in regInfoList[] */

#define MODS_XP_IRQ_TYPE_INT  0
#define MODS_XP_IRQ_TYPE_MSI  1
#define MODS_XP_IRQ_TYPE_CPU  2 // for CheetAh
#define MODS_XP_IRQ_TYPE_MSIX 3
#define MODS_XP_IRQ_NUM_TYPES 4
#define MODS_XP_IRQ_TYPE_MASK 0xff

#define MODS_XP_MASK_TYPE_IRQ_DISABLE   0
#define MODS_XP_MASK_TYPE_IRQ_DISABLE64 1

/* Standard PCI device information */
typedef struct PciLocation
{
    UINT16 domain;
    UINT16 bus;
    UINT16 device;
    UINT16 function;
} PciLocation;

/* Structure to define how to mask off an interrupt for given register
 * The assumption is that the interrupts will be masked using the following algorithm
 * *irqDisableOffset = ((*irqEnabledOffset & andMask) | orMask)
 */
typedef struct MaskParams
{
    UINT08 maskType;    // type of mask to use, either MODS_XP_MASK_TYPE_IRQ_DISABLE
			// or MODS_XP_MASK_TYPE_IRQ_DISABLE64.
    UINT32 irqPendingOffset;   // register to read IRQ pending status
    UINT32 irqEnabledOffset;   // register to read the enabled interrupts
    UINT32 irqEnableOffset;    // register to write to enable interrupts
    UINT32 irqDisableOffset;   // register to write to disable interrupts
    UINT64 andMask;     // mask to clear bits
    UINT64 orMask;      // mask to set bits
} MaskParams;

typedef struct IrqParams
{
    UINT32 irqNumber;   // IRQ number to hook.
    UINT64 barAddr;     // physical address of the Base Address Region (BAR)
    UINT32 barSize;     // number of byte to may within the BAR
    PciLocation pciDev;     // PCI's domain,bus,device,function values
    UINT08 irqType;     // type of IRQ to hook. s/b one of the following
			// - MODS_XP_IRQ_TYPE_INT (legacy INTA)
			// - MODS_XP_IRQ_TYPE_MSI
			// - MODS_XP_IRQ_TYPE_CPU (for CheetAh)
			// - MODS_XP_IRQ_TYPE_MSIX
    UINT32 irqFlags;    // Flags to be passed to the irq
    UINT32 maskInfoCount; // Number of entries in the maskInfoList[]
    MaskParams maskInfoList[MASK_PARAMS_LIST_SIZE];  //see above
} IrqParams;

