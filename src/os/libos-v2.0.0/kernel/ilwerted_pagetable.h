/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#pragma once
#include "debug/elf.h"
#include "include/libos_ilwerted_pagetable.h"
#include "kernel.h"

void kernel_syscall_init_register_pagetables(LwU64 pagetable_base, LwU64 entries, LwU64 probe_count);
LIBOS_SECTION_IMEM_PINNED libos_pagetable_entry_t *kernel_resolve_address(libos_pasid_t pasid, LwU64 virtual_address);
LIBOS_SECTION_TEXT_COLD void kernel_bootstrap_pagetables(elf64_header *elf, LwU64 elfPhysical);
LIBOS_SECTION_TEXT_COLD void kernel_remap_pagetables(void);
