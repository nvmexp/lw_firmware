/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LIBOS_DEFINES_H
#define LIBOS_DEFINES_H

#include "libos-config.h"

#define LIBOS_MPU_PAGESHIFT 12 /* not technically LW_TRUE for GA11x */
#define LIBOS_MPU_PAGESIZE  (1ULL << LIBOS_MPU_PAGESHIFT)
#define LIBOS_MPU_PAGEMASK  (~(LIBOS_MPU_PAGESIZE - 1))

// Set the kernel stack size (stored in the DMEM page)
// @note tests/validation/stack-depth.h does a live validation
//       ACROSS ALL TESTS to ensure this value isn't exceeded.
#define LIBOS_KERNEL_STACK_SIZE 512

// Maximum pagetable window is 64mb
#define LIBOS_MAX_PAGETABLE_SIZE 0x4000000

// Image base address of the phdr_boot section
#define LIBOS_PHDR_BOOT_VA LIBOS_MAX_PAGETABLE_SIZE

//  See code in linker script that guarantees
//  the pointer has no bits set outside of this mask
#define LIBOS_RESOURCE_PTR_MASK U(0xfffffff8)

//
//  Port privleges are non-transferrable and
//  stored in the lower bits of the handle
//
#define LIBOS_RESOURCE_PORT_OWNER_GRANT U(0x00000001)
#define LIBOS_RESOURCE_PORT_SEND_GRANT  U(0x00000002)

//
//  Shuttle privilege
//
#define LIBOS_RESOURCE_SHUTTLE_GRANT U(0x00000004)

//
//  These are attributes of the memory region
//  and do not reflect granted permissions to a task.
//  Note these settings are stored in the lower 10 bits
//  of the memory region's page aligned size field.
//  @see memory.h
//
#if LWRISCV_VERSION < LWRISCV_2_0
#    define LIBOS_MEMORY_MPU_INDEX_MASK ULL(0x3F)
#endif
#define LIBOS_MEMORY_ATTRIBUTE_EXELWTE_MASK ULL(0x040)
#define LIBOS_MEMORY_ATTRIBUTE_WRITE_MASK   ULL(0x080)
#define LIBOS_MEMORY_ATTRIBUTE_KIND         ULL(0x700)

#define LIBOS_MEMORY_ATTRIBUTE_MASK ULL(0x7ff)

#define LIBOS_MEMORY_KIND_UNCACHED  ULL(0x000)
#define LIBOS_MEMORY_KIND_CACHED    ULL(0x100)
#define LIBOS_MEMORY_KIND_PAGED_TCM ULL(0x200)
#define LIBOS_MEMORY_KIND_MMIO      ULL(0x400)
#define LIBOS_MEMORY_KIND_DMA       ULL(0x600)

// PHDR flags include all memory attribute flags above
// as well as the flags below
#define LIBOS_PHDR_FLAG_PREMAP_AT_INIT ULL(0x1000)

// Physical address is set to this value for uninitialized
// INIT_ARG_REGIONs.
#define LIBOS_MEMORY_PHYSICAL_ILWALID ULL(0xFFFFFFFFFFFFFFFF)

#define LIBOS_CONTINUATION_NONE U(0)
#define LIBOS_CONTINUATION_RECV U(1)
#define LIBOS_CONTINUATION_WAIT U(2)

//
//  Task state, will include states for DMA
//  in progress to allow the kernel to switch
//  during active DMA to always ensure RTOS.
//

#define LIBOS_TASK_STATE_NORMAL     U(0)
#define LIBOS_TASK_STATE_PORT_WAIT  U(1)
#define LIBOS_TASK_STATE_TIMER_WAIT U(2)
#define LIBOS_TASK_STATE_TRAPPED    U(4)

// @note PT_LOOS region reserved for OS specific secions (0x60000000)
#define LIBOS_PT_INIT_ARG_REGION                 0x60000000
#define LIBOS_PT_PHYSICAL                        0x60000001
#define LIBOS_PT_KERNEL                          0x60000003
#define LIBOS_PT_MANIFEST_ACLS                   0x60000005
#define LIBOS_PT_MANIFEST_INIT_ARG_MEMORY_REGION 0x60000006
#define LIBOS_PT_LOGGING_STRINGS                 0x60000007

#define LIBOS_MANIFEST_HEADER                 0
#define LIBOS_MANIFEST_PHDR_ACLS              1
#define LIBOS_MANIFEST_INIT_ARG_MEMORY_REGION 2

#endif
