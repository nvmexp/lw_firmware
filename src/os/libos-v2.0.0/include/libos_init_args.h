
/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#pragma once

// @todo: Remove this once chips_a/drivers/resman/arch/lwalloc/common/inc/riscvifriscv.h has been
//        switched to directly include this file.
#if !defined(RISCVIFRISCV_H) && !defined(LIBOS_INIT_H_)
#define LIBOS_INIT_H_

#define LIBOS_MAGIC 0x51CB7F1D

typedef struct
{
    LwU64 index;
    LwU64 id8;
} libos_manifest_init_arg_memory_region_t;

#define LIBOS_MEMORY_REGION_INIT_ARGUMENTS_MAX 4096

typedef LwU64 LibosAddress;

typedef enum {
    LIBOS_MEMORY_REGION_NONE,
    LIBOS_MEMORY_REGION_CONTIGUOUS,
    LIBOS_MEMORY_REGION_RADIX3
} LibosMemoryRegionKind;

typedef enum {
    LIBOS_MEMORY_REGION_LOC_NONE,
    LIBOS_MEMORY_REGION_LOC_SYSMEM,
    LIBOS_MEMORY_REGION_LOC_FB
} LibosMemoryRegionLoc;

#define LIBOS_MEMORY_REGION_RADIX_PAGE_SIZE 4096
#define LIBOS_MEMORY_REGION_RADIX_PAGE_LOG2 12
typedef struct
{
    LibosAddress          id8;  // Id tag.
    LibosAddress          pa;   // Physical address.
    LibosAddress          size; // Size of memory area.
    LwU8                  kind; // See LibosMemoryRegionKind above.
    LwU8                  loc;  // See LibosMemoryRegionLoc above.
} LibosMemoryRegionInitArgument;

#endif
