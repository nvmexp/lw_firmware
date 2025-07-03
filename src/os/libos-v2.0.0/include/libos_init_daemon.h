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

#include "debug/elf.h"
#include "include/libos_elf_acl.h"
#include "include/libos_init_args.h"

void *lib_init_heap_alloc(LwU64 length);
LwU64 lib_init_heap_get_free(void);
void lib_init_premap_log(LibosMemoryRegionInitArgument *init_args, LwU64 sysGpaStartAddr);
void lib_init_map_log(void);
void lib_init_pagetable_map(elf64_header *elf, LwU32 phdr_index, LwU64 physical_address, LwU64 physical_size);
void lib_init_pagetable_initialize(elf64_header *elf, LwU64 elfPhysical);
libos_manifest_init_arg_memory_region_t *
lib_init_phdr_init_arg_memory_regions(const elf64_header *elf, LwU64 *count);
void lib_init_pagetable_insert(libos_pasid_t pasid, LwU64 virtual_address, LwU64 size, LwU64 attributes, LwU64 physical_address);

#ifdef GSPRM_ASAN_ENABLE
LwU64 lib_init_shadow_ranges_get_size_estimate(libos_pasid_t pasid);
void lib_init_shadow_ranges_register(libos_pasid_t pasid, LwU64 virtual_address, LwU64 memsz);
void lib_init_shadow_ranges_register_phdr(libos_pasid_t pasid, elf64_header *elf, LwU32 phdr_index, LwU64 size);
void lib_init_shadow_ranges_allocate_and_map(libos_pasid_t pasid);
#endif