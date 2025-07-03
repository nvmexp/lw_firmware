/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef LIBLWRISCV_MEMUTILS_H
#define LIBLWRISCV_MEMUTILS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <lwriscv/status.h>

#if LWRISCV_HAS_FBIF
#include <liblwriscv/fbif.h>
#endif // LWRISCV_HAS_FBIF

/**
 * @brief Memory targets supported by memutils_riscv_pa_to_target_offset()
 *
 * @typedef-title riscv_mem_target_t
 */
typedef enum
{
    RISCV_MEM_TARGET_IMEM,
    RISCV_MEM_TARGET_DMEM,
    RISCV_MEM_TARGET_FBGPA,
    RISCV_MEM_TARGET_SYSGPA,
} riscv_mem_target_t;


#if LWRISCV_HAS_FBIF

/**
 * @brief Colwerts a RISCV PA to an FBIF target/offset pair. Target is one of
 * the external memory types which Peregrine can access via FBIF (FB or SYSMEM)
 * and offset is the offset within the physical address range of that memory type.
 *
 * @param[in]  pa       The RISCV PA to colwert.
 *
 * @param[out] p_target  The target aperture (e.g. COHERENT_SYSTEM) that the PA
 *                      resides within.
 *
 * @param[out] p_offset  The offset of the PA within the target aperture.
 *
 * @return LWRV_OK       if successful.
 * LWRV_ERR_OUT_OF_RANGE if PA is not within the range of a valid memory aperture (SYSMEM or FB).
 */
LWRV_STATUS memutils_riscv_pa_to_fbif_aperture(uintptr_t pa, fbif_transcfg_target_t *p_target,
    uint64_t *p_offset);

#endif // LWRISCV_HAS_FBIF

/**
 * @brief Colwerts a RISCV PA to a global target/offset pair. Target is one of
 * the available memory types which Peregrine can access (IMEM, DMEM, FB, SYSMEM)
 * and offset is the offset within the physical address range of that memory type.
 *
 * @param[in]  pa       The RISCV PA to colwert.
 *
 * @param[out] p_target  The memory target (e.g. SYSMEM) that the PA resides
 *                      within (optional).
 *
 * @param[out] p_offset  The offset of the PA within the memory target
 *                      (optional).
 *
 * @return LWRV_OK       if successful.
 * LWRV_ERR_OUT_OF_RANGE if PA does not fall inside any of the supported memory targets.
 */
LWRV_STATUS memutils_riscv_pa_to_target_offset(uintptr_t pa, riscv_mem_target_t *p_target,
    uint64_t *p_offset);

///////////////////////////////////////////////////////////////////////////////

/*!
 * @brief Checks whether an address is contained within a memory region.
 *
 * @param[in]    check_addr      The address to check.
 * @param[in]    region_start    The start address of the memory region.
 * @param[in]    region_end      The end address of the memory region.
 *
 * @return true if the check address lies within the memory region.
 *         false otherwise.
 *
 * @note The byte at region_end is not included in the memory region.
 */
static inline bool
memutils_mem_addr_in_range
(
    uintptr_t check_addr,
    uintptr_t region_start,
    uintptr_t region_end
)
{
    return ((check_addr >= region_start) && (check_addr < region_end));
}

/*!
 * @brief Checks whether two memory regions overlap.
 *
 * @param[in]    first_base      Base address of the first region.
 * @param[in]    first_size      Size in bytes of the first region.
 * @param[in]    second_base     Base address of the second region.
 * @param[in]    second_size     Size in bytes of the second region.
 *
 * @return
 *    true  if the memory regions overlap.
 *    false if the memory regions do not overlap.
 *
 * @note This DOES NOT check for integer overflow!
 */
static inline bool
memutils_mem_has_overlap
(
    uintptr_t first_base,
    size_t first_size,
    uintptr_t second_base,
    size_t second_size
)
{
    return ((second_base < (first_base  + first_size)) &&
            (first_base  < (second_base + second_size)));
}

#if LWRISCV_FEATURE_LEGACY_API 
/*
 * Maintain legacy support for clients which use the old function names.
 * Include at the end so that it may access the types in this fiile and typedef them.
 */
#include <liblwriscv/legacy/memutils_legacy.h>
#endif //LWRISCV_FEATURE_LEGACY_API

#endif //LIBLWRISCV_MEMUTILS_H
