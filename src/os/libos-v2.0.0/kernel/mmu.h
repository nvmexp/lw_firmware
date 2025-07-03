/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LIBOS_MMU_H
#define LIBOS_MMU_H

#include "kernel.h"
#include "ilwerted_pagetable.h"

// kernel_mmu_page_in returns this on failure
#define LIBOS_ILWALID_TCM_OFFSET (0xFFFFFFFF)

extern LwU64 cached_phdr_boot_virtual_to_physical_delta;

typedef struct
{
    LwU64 start;
    LwU64 end;
} libos_wpr_info_t;

extern libos_wpr_info_t LIBOS_SECTION_DMEM_PINNED(wpr);

#define WPR_OVERLAP(pa, size)  (((pa) < wpr.end) && (((pa) + (size)) > wpr.start))

void LIBOS_SECTION_TEXT_COLD kernel_mmu_load(void);
void kernel_mmu_load_kernel(const libos_bootstrap_args_t *boot_args,
                            LwS64 phdr_boot_virtual_to_physical_delta);
void kernel_mmu_clear_mpu(void);
void kernel_mmu_prepare_for_processor_suspend(void);

LIBOS_SECTION_TEXT_COLD void kernel_mmu_program_mpu_entry(LwU64 idx, LwU64 va, LwU64 pa, LwU64 size, LwU64 attr);
LIBOS_SECTION_TEXT_COLD void kernel_mmu_mark_mpu_entry_ro(LwU64 idx);
LIBOS_SECTION_TEXT_COLD void kernel_mmu_clear_mpu_entry(LwU64 idx);
LIBOS_SECTION_TEXT_COLD void kernel_mmu_map_pagetable(LwU64 va, LwU64 pa, LwU64 size);

LIBOS_SECTION_IMEM_PINNED LIBOS_NORETURN void kernel_datafault(void);
LIBOS_SECTION_IMEM_PINNED LIBOS_NORETURN void kernel_codefault(void);

LwU64 kernel_mmu_page_in(libos_pagetable_entry_t *memory, LwU64 bad_addr, LwU64 xcause, LwBool skip_read);

// indices into mpu_attributes_base[] table
#define MPU_ATTRIBUTES_COHERENT   0
#define MPU_ATTRIBUTES_INCOHERENT 1

#endif
