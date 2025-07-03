
#include "kernel.h"
#include "diagnostics.h"
#include <lwmisc.h>

/**
 * @brief Returns the kernel identity mapping for a physical address
 * 
 * The kernel maintains an identity mapping of the LW_RISCV_AMAP_FBGPA
 * aperture.
 * 
 * While not ideal, it is required by softmmu.s to access the pagetables
 * which may be straddled across memory.  On first glance, it might
 * seem like we could have a per MemoryPool identity mapping, but
 * these mappings would need to be pinned in the MPU for turing/hopper/ada.
 * Our design requires at most 3 pinned MPU entries at specific virtual addresses
 * to ensure softmmu.s doesn't accidentally evict them.
 * 
 * This mapping is also use for the ObjectPool to ensure we minimize 
 * the number of softmmu.s eviction calls.  
 * 
 * @param fbAddress 
 * @return void* 
 */
void * KernelPhysicalIdentityMap(LwU64 fbAddress) {
    // The address must be within the FBPGA aperture
    KernelAssert(fbAddress >= LW_RISCV_AMAP_FBGPA_START && 
                 fbAddress < LW_RISCV_AMAP_FBGPA_END);
    
    // The address must also fit within the SOFTTLB mapping
    KernelAssert((fbAddress - LW_RISCV_AMAP_FBGPA_START) < LIBOS_CONFIG_SOFTTLB_MAPPING_RANGE);

    return (void *)(fbAddress - LW_RISCV_AMAP_FBGPA_START + LIBOS_CONFIG_SOFTTLB_MAPPING_BASE);
}

/**
 * @brief Lookup the physical address for a SOFTTLB mapping 
 * 
 * @param addr 
 * @return LwU64 
 */
LwU64 KernelPhysicalIdentityReverseMap(void * addr) {
    // Address must be in the SOFTTLB aperture
    KernelAssert((LwU64)addr >= LIBOS_CONFIG_SOFTTLB_MAPPING_BASE && 
                 ((LwU64)addr - LIBOS_CONFIG_SOFTTLB_MAPPING_BASE) < LIBOS_CONFIG_SOFTTLB_MAPPING_BASE);

    return ((LwU64)addr - LIBOS_CONFIG_SOFTTLB_MAPPING_BASE + LW_RISCV_AMAP_FBGPA_START);
}