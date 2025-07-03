/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#pragma once
#include <sdk/lwpu/inc/lwtypes.h>

//
//  Memory structures
//
typedef LwU8 libos_pasid_t;

// Standard pagetable entries cover 4kb
#define LIBOS_PAGETABLE_SEARCH_GRANULARITY_SMALL      0x1000ULL
#define LIBOS_PAGETABLE_MAX_ADDRESS_GRANULARITY_SMALL 0x100000000ULL

// Huge pagetable entries cover 48bits.
// @note The actual size of mapping can be any size, but no more than one huge mapping
//       map exist in any 2**48 region.
#define LIBOS_PAGETABLE_SEARCH_GRANULARITY_HUGE 0x0001000000000000ULL

typedef struct
{
    // This is the 'page' that was being searched for
    LwU64 key_virtual_address;

    // This is the base VA of the mapping containing the page
    // (because of the limited MPU table - we support arbitrarily large mappings)
    LwU64 virtual_address;

    // Physical address corresponding to the base of the mapping
    LwU64 physical_address;

    //     63                                       14 13..10  9          0
    //   ____________________________________________________________________
    //   |      Size                                  | kind |   mpu-index  |
    //   --------------------------------------------------------------------
    //      LIBOS_MEMORY_MPU_INDEX_MASK - GA100/TU10X
    //         Store the last MPU index this non-paged region was mapped into.
    //         Used since these chips will issue LACC faults on MPU resident
    //         entries to denote downstream failures (ECC, PRI, etc)
    //      LIBOS_MEMORY_ATTRIBUTE_KIND
    //        LIBOS_MEMORY_KIND_UNCACHED        - Uncached FB/SYSCOH (Coherent)
    //        LIBOS_MEMORY_KIND_CACHED          - Cached FB/SYSCOH
    //        LIBOS_MEMORY_KIND_PAGED_TCM       - Paged TCM (FB)
    //        LIBOS_MEMORY_KIND_MMIO            - Uncached and non-coherent
    //        LIBOS_MEMORY_KIND_DMA             - Accessible to DMA only (coherent)
    //      LIBOS_MEMORY_ATTRIBUTE_EXELWTE_MASK
    //        Memory is exelwtable.
    // @see libos_defines.h for defines (shared with linker script)
    LwU64 attributes_and_size;
} libos_pagetable_entry_t;

//
//  PTE hash
//
static inline LwU64 murmur(LwU64 address)
{
    LwU64 h = address;
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccdL;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53L;
    h ^= h >> 33;
    return h;
}
