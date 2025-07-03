/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <lwriscv/status.h>

#include <liblwriscv/io.h>
#include <liblwriscv/memutils.h>

#if LWRISCV_HAS_FBIF
#include <liblwriscv/fbif.h>

LWRV_STATUS
memutils_riscv_pa_to_fbif_aperture
(
    uintptr_t pa,
    fbif_transcfg_target_t *p_target,
    uint64_t *p_offset
)
{
    riscv_mem_target_t mem_target;
    LWRV_STATUS status;

    if (p_target == NULL)
    {
        return LWRV_ERR_ILWALID_ARGUMENT;
    }

    // Obtain the memory target and offset for the provided physical address.
    status = memutils_riscv_pa_to_target_offset(pa, &mem_target, p_offset);
    if(status != LWRV_OK)
    {
        goto out;
    }

    // Translate the memory target to its corresponding FBIF target.
    switch (mem_target)
    {
        case RISCV_MEM_TARGET_FBGPA:
            *p_target = FBIF_TRANSCFG_TARGET_LOCAL_FB;
            break;

        case RISCV_MEM_TARGET_SYSGPA:
            //
            // Just assume coherent here as this value is meant to be
            // passed to dmaPa() anyway.
            //
            *p_target = FBIF_TRANSCFG_TARGET_COHERENT_SYSTEM;
            break;

        default:
            status = LWRV_ERR_OUT_OF_RANGE;
            break;
    }

out:
    return status;
}
#endif // LWRISCV_HAS_FBIF

LWRV_STATUS
memutils_riscv_pa_to_target_offset
(
    uintptr_t pa,
    riscv_mem_target_t *p_target,
    uint64_t *p_offset
)
{
    LWRV_STATUS status = LWRV_OK;
    riscv_mem_target_t target;
    uint64_t offset;

    // Determine which memory region pa resides in.
    if (memutils_mem_addr_in_range(pa, LW_RISCV_AMAP_IMEM_START, LW_RISCV_AMAP_IMEM_END))
    {
        target = RISCV_MEM_TARGET_IMEM;
        offset = pa - LW_RISCV_AMAP_IMEM_START;
    }
    else if (memutils_mem_addr_in_range(pa, LW_RISCV_AMAP_DMEM_START, LW_RISCV_AMAP_DMEM_END))
    {
        target = RISCV_MEM_TARGET_DMEM;
        offset = pa - LW_RISCV_AMAP_DMEM_START;
    }
#ifdef LW_RISCV_AMAP_FBGPA_START
    else if (memutils_mem_addr_in_range(pa, LW_RISCV_AMAP_FBGPA_START, LW_RISCV_AMAP_FBGPA_END))
    {
        target = RISCV_MEM_TARGET_FBGPA;
        offset = pa - LW_RISCV_AMAP_FBGPA_START;
    }
#endif // LW_RISCV_AMAP_FBGPA_START
#ifdef LW_RISCV_AMAP_SYSGPA_START
    else if (memutils_mem_addr_in_range(pa, LW_RISCV_AMAP_SYSGPA_START, LW_RISCV_AMAP_SYSGPA_END))
    {
        target = RISCV_MEM_TARGET_SYSGPA;
        offset = pa - LW_RISCV_AMAP_SYSGPA_START;
    }
#endif // LW_RISCV_AMAP_SYSGPA_START
    else
    {
        // Not a supported memory region.
        status = LWRV_ERR_OUT_OF_RANGE;
        goto out;
    }

    // Return target/offset information as requested.
    if (p_target != NULL)
    {
        *p_target = target;
    }

    if (p_offset != NULL)
    {
        *p_offset = offset;
    }

out:
    return status;
}
