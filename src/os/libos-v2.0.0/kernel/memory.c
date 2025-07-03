/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "memory.h"
#include "ilwerted_pagetable.h"
#include "libos_status.h"
#include "lw_gsp_riscv_address_map.h"
#include "riscv-isa.h"

/**
 *  @brief Queries attributes of memory region containing given pointer.
 *
 *  @note These attributes are ilwariant.
 */
LwU32 LIBOS_NORETURN LIBOS_SECTION_TEXT_COLD kernel_syscall_memory_query(
    task_index_t remote_task, // a0
    LwU64        pointer      // a1
)
{
    libos_task_t *remote = kernel_task_resolve_or_return_errorcode(remote_task);

    libos_pagetable_entry_t *ptr = kernel_resolve_address(remote->pasid, pointer & LIBOS_MPU_PAGEMASK);

    // Callwlate permessions
    LwU64 attrs = LIBOS_MEMORY_ACCESS_MASK_READ;

    if (ptr->attributes_and_size & LIBOS_MEMORY_ATTRIBUTE_WRITE_MASK)
        attrs |= LIBOS_MEMORY_ACCESS_MASK_WRITE;

    if (LIBOS_MEMORY_IS_EXELWTE(ptr))
        attrs |= LIBOS_MEMORY_ACCESS_MASK_EXELWTE;

    if (LIBOS_MEMORY_IS_CACHED(ptr))
        attrs |= LIBOS_MEMORY_ACCESS_MASK_CACHED;

    if (LIBOS_MEMORY_IS_PAGED_TCM(ptr))
    {
#if LIBOS_CONFIG_TCM_PAGING
        attrs |= LIBOS_MEMORY_ACCESS_MASK_PAGED_TCM;
#else
        // When TCM paging is disabled, we direct map its backing FB as cached instead
        attrs |= LIBOS_MEMORY_ACCESS_MASK_CACHED;
#endif
    }

    self->state.registers[LIBOS_REG_IOCTL_MEMORY_QUERY_ACCESS_ATTRS] = attrs;

    // Get aperture.
    LwU64 aperture, physical_offset;
    if ((ptr->physical_address >= LW_RISCV_AMAP_FBGPA_START) &&
        ((ptr->physical_address - LW_RISCV_AMAP_FBGPA_START) < LW_RISCV_AMAP_FBGPA_SIZE))
    {
        aperture        = LIBOS_MEMORY_APERTURE_FB;
        physical_offset = ptr->physical_address - LW_RISCV_AMAP_FBGPA_START;
    }
    else if (
        (ptr->physical_address >= LW_RISCV_AMAP_SYSGPA_START) &&
        ((ptr->physical_address - LW_RISCV_AMAP_SYSGPA_START) < LW_RISCV_AMAP_SYSGPA_SIZE))
    {
        aperture        = LIBOS_MEMORY_APERTURE_SYSCOH;
        physical_offset = ptr->physical_address - LW_RISCV_AMAP_SYSGPA_START;
    }
    else if (
        (ptr->physical_address >= LW_RISCV_AMAP_PRIV_START) &&
        ((ptr->physical_address - LW_RISCV_AMAP_PRIV_START) < LW_RISCV_AMAP_PRIV_SIZE))
    {
        aperture        = LIBOS_MEMORY_APERTURE_MMIO;
        physical_offset = ptr->physical_address - LW_RISCV_AMAP_PRIV_START;
    }
    else
        kernel_return_access_errorcode();

    self->state.registers[LIBOS_REG_IOCTL_MEMORY_QUERY_APERTURE]        = aperture;
    self->state.registers[LIBOS_REG_IOCTL_MEMORY_QUERY_PHYSICAL_OFFSET] = physical_offset;

    // Return payload
    self->state.registers[LIBOS_REG_IOCTL_MEMORY_QUERY_ALLOCATION_BASE] = ptr->virtual_address;
    self->state.registers[LIBOS_REG_IOCTL_MEMORY_QUERY_ALLOCATION_SIZE] =
        ptr->attributes_and_size & ~LIBOS_MEMORY_ATTRIBUTE_MASK;

    // Set successful status
    self->state.registers[LIBOS_REG_IOCTL_A0] = LIBOS_OK;
    kernel_return();
}

/*!
 * @brief Ilwalidate data cache lines by virtual address
 *
 *  This call will cause an instruction fault when start is not the VA of a
 *  cached FB address.
 */
LIBOS_SECTION_TEXT_COLD LIBOS_NORETURN void
kernel_syscall_ilwalidate_cache(LwU64 start, LwU64 size)
{
#define RISCV_CACHE_LINE_SIZE 64ULL
    LwU64 start_masked = (start & ~(RISCV_CACHE_LINE_SIZE - 1ULL));
    LwU64 end = start + size;

    for (; start_masked < end; start_masked += RISCV_CACHE_LINE_SIZE)
    {
        csr_write(LW_RISCV_CSR_XDCACHEOP,
                  start_masked |
                  REF_DEF64(LW_RISCV_CSR_XDCACHEOP_ADDR_MODE, _VA) |
                  REF_DEF64(LW_RISCV_CSR_XDCACHEOP_MODE, _ILW_LINE));
    }

    kernel_return();
}

/*!
 * @brief Ilwalidate all data cache lines
 */
LIBOS_SECTION_TEXT_COLD LIBOS_NORETURN void
kernel_syscall_ilwalidate_cache_all(void)
{
    csr_write(LW_RISCV_CSR_XDCACHEOP,
              REF_DEF64(LW_RISCV_CSR_XDCACHEOP_MODE, _ILW_ALL));

    kernel_return();
}

LIBOS_SECTION_IMEM_PINNED libos_pagetable_entry_t *
validate_memory_access_or_return(LwU64 buffer, LwU64 buffer_size, LwBool write)
{
    return validate_memory_pasid_access_or_return(buffer, buffer_size, write, self->pasid);
}

LIBOS_SECTION_IMEM_PINNED libos_pagetable_entry_t *
validate_memory_pasid_access_or_return(LwU64 buffer, LwU64 buffer_size, LwBool write, libos_pasid_t pasid)
{
    libos_pagetable_entry_t *region = kernel_resolve_address(pasid, buffer & LIBOS_MPU_PAGEMASK);

#ifdef LIBOS_FEATURE_ISOLATION
    /*
        Verify that we have the appropriate write permissions
    */
    if (write && !(region->attributes_and_size & LIBOS_MEMORY_ATTRIBUTE_WRITE_MASK))
        kernel_return_access_errorcode();

    /*
        kernelResolve_address guarantees that "buffer" is within the region.
        Therefore subtracting off region start cannot overflow
    */
    LwU64 offset = (LwU64)buffer - region->virtual_address;

    /*
        Callwlate the remaining size available for copy starting at buffer.
        Again, this cannot overflow as the offset was guaranteed to be within
        the buffer (this comes from a trusted source)
    */
    LwU64 size_from_offset = (region->attributes_and_size & ~LIBOS_MEMORY_ATTRIBUTE_MASK) - offset;

    /*
        Compare against the requested buffer size
    */
    if (buffer_size > size_from_offset)
    {
        kernel_return_access_errorcode();
    }

#endif
    return region;
}
