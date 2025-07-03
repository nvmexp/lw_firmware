/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef LIBLWRISCV_MEMUTILS_LEGACY_H
#define LIBLWRISCV_MEMUTILS_LEGACY_H


/*
 * This file is to maintain support for legacy clients which use the old deprecated
 * function names. Please do not use these functions in new code. Existing code should
 * migrate to using the new function names if possible.
 */

typedef riscv_mem_target_t RISCV_MEM_TARGET;

#define riscvPaToFbifAperture(pa, p_target, p_offset) \
    memutils_riscv_pa_to_fbif_aperture((pa), (p_target), (p_offset))

#define riscvPaToTargetOffset(pa, p_target, p_offset) \
    memutils_riscv_pa_to_target_offset((pa), (p_target), (p_offset))

#define memAddrInRange(check_addr, region_start, region_end) \
    memutils_mem_addr_in_range((check_addr), (region_start), (region_end))

#define memHasOverlap(first_base, first_size, second_base, second_size) \
    memutils_mem_has_overlap((first_base), (first_size), (second_base), (second_size))

#endif // LIBLWRISCV_MEMUTILS_LEGACY_H
