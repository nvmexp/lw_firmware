#include "kernel.h"
#include "pagetable.h"
#include <lwmisc.h>
#include "softmmu.h"
#include "libbit.h"
#include "diagnostics.h"
#include "memorypool.h"

/**
 * @brief Evicts any pages that start in this range.
 * 
 * Should only be called for ranges substancially shorter than the 
 * MPU size.
 * 
 * Contract: Callers must pass a page aligned virtualAddress and size.
 * 
 * @param table 
 * @param virtualAddress 
 * @param size 
 */
#if LIBOS_CONFIG_MPU_HASHED
static void KernelSoftmmuFlushPagesInRangeByHash(LwU64 virtualAddressFirst, LwU64 virtualAddressLast)
{
    LwU64 first = LibosRoundUp(virtualAddressFirst, LIBOS_CONFIG_PAGESIZE);

    while (first <= virtualAddressLast)
    {
        // We need to shoot everything down in this bucket that might have been this VA
        LwU64 idx = KernelBootstrapHashSmall(first);
        for (LwU32 j = 0; j < 4; j++)
        {
            KernelCsrWrite(LW_RISCV_CSR_XMPUIDX, idx+j);

            // Mask out the valid bit and look for an exact match
            if ((KernelCsrRead(LW_RISCV_CSR_XMPUVA) & ~1) == first)  {
                KernelCsrWrite(LW_RISCV_CSR_XMPUVA, 0);
                break;
            }        
        }

        // @see OVERFLOW-ADD-1 
        if (__builtin_add_overflow(first, LIBOS_CONFIG_PAGESIZE, &first))
            break;
    } 

    first = LibosRoundUp(virtualAddressFirst, LIBOS_CONFIG_PAGESIZE_MEDIUM);
    while (first <= virtualAddressLast)
    {
        // We need to shoot everything down in this bucket that might have been this VA
        LwU64 idx = KernelBootstrapHashMedium(first);
        for (LwU32 j = 0; j < 4; j++)
        {
            KernelCsrWrite(LW_RISCV_CSR_XMPUIDX, idx+j);
            // Mask out the valid bit and look for an exact match
            if ((KernelCsrRead(LW_RISCV_CSR_XMPUVA) & ~1) == first)  {
                KernelCsrWrite(LW_RISCV_CSR_XMPUVA, 0);
                break;
            }        
        }
        // @see OVERFLOW-ADD-1 
        if (__builtin_add_overflow(first, LIBOS_CONFIG_PAGESIZE_MEDIUM, &first))
            break;        
    }
}
#endif

/**
 * @brief Evicts any pages that start in this range
 * 
 * Performs a linear search of all MPU buckets to test
 * intersection with search interval.
 * 
 * Used for large ilwalidations.
 * 
 * @param table 
 * @param virtualAddress 
 * @param size 
 */
static void KernelSoftmmuFlushPagesInRangeLinearSearch(LwU64 virtualAddressFirst, LwU64 virtualAddressLast)
{
    for (LwU32 i = 0; i < LIBOS_CONFIG_MPU_INDEX_COUNT; i++)
    {
        KernelCsrWrite(LW_RISCV_CSR_XMPUIDX, i);
        LwU64 pageVa = KernelCsrRead(LW_RISCV_CSR_XMPUVA) &~ 1;

        // Does this MPU mapping start inside of the ilwalidation region
        if (pageVa >= virtualAddressFirst && pageVa <= virtualAddressLast)
            KernelCsrWrite(LW_RISCV_CSR_XMPUVA, 0);
    }
}

/**
 * @brief Evicts any pages that start in this range
 * 
 * Contract: KernelRangeDoesntWrap(virtualAddress, size) &&
 *           KernelAddressPageAligned(virtualAddress) &&
 *           KernelAddressPageAligned(size) &&
 *           size
 * 
 * @param table 
 * @param virtualAddress 
 * @param size 
 */
void KernelSoftmmuFlushPages(LwU64 virtualAddress, LwU64 size)
{
    KernelAssert(KernelRangeDoesntWrap(virtualAddress, size) &&
                 KernelAddressPageAligned(virtualAddress) &&
                 KernelAddressPageAligned(size) && 
                 size);

    LwU64 virtualAddressFirst = virtualAddress;
    LwU64 virtualAddressLast  = virtualAddress + size - 1;

    // Should be a valid non-empty range
    KernelAssert(virtualAddressFirst <= virtualAddressLast);

    // The virtual address should not be inside one of the pinned mappings
    KernelAssert(virtualAddressFirst < LIBOS_CONFIG_SOFTTLB_MAPPING_BASE || virtualAddressFirst >= (LIBOS_CONFIG_SOFTTLB_MAPPING_BASE + LIBOS_CONFIG_SOFTTLB_MAPPING_RANGE));
    KernelAssert(virtualAddressFirst < LIBOS_CONFIG_KERNEL_TEXT_BASE || virtualAddressFirst >= (LIBOS_CONFIG_KERNEL_TEXT_BASE + LIBOS_CONFIG_KERNEL_TEXT_RANGE));
    KernelAssert(virtualAddressFirst < LIBOS_CONFIG_KERNEL_DATA_BASE || virtualAddressFirst >= (LIBOS_CONFIG_KERNEL_DATA_BASE + LIBOS_CONFIG_KERNEL_DATA_RANGE));

#if LIBOS_CONFIG_MPU_HASHED
    /**
     * @brief Is visiting each page in the range faster than just scanning all the MPU buckets?
     * 
     */
    if (((virtualAddressLast - virtualAddressFirst) / LIBOS_CONFIG_PAGESIZE) * 4 < LIBOS_CONFIG_MPU_INDEX_COUNT) {
        KernelSoftmmuFlushPagesInRangeByHash(virtualAddressFirst, virtualAddressLast);
        return;
    }
#endif

    /** 
     * @brief Exhaustive search of all buckets is faster
     * 
     */
    KernelSoftmmuFlushPagesInRangeLinearSearch(virtualAddressFirst, virtualAddressLast);
}

/**
 * @brief Disables all MPU entries except the pinned entries
 * 
 */
void KernelSoftmmuFlushAll()
{
    KernelCsrWrite(LW_RISCV_CSR_SMPUIDX2, 0);
    KernelCsrWrite(LW_RISCV_CSR_SMPUVLD,  (1ULL << LIBOS_CONFIG_MPU_IDENTITY) | 
                                          (1ULL << LIBOS_CONFIG_MPU_TEXT) | 
                                          (1ULL << LIBOS_CONFIG_MPU_DATA));
    KernelCsrWrite(LW_RISCV_CSR_SMPUIDX2, 1);
    KernelCsrWrite(LW_RISCV_CSR_SMPUVLD,  0);    
}