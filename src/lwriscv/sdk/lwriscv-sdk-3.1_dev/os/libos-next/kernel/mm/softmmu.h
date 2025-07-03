#pragma once
#include "kernel.h"
#include <lwmisc.h>

// @note: The implementation of the interface is split between softmmu.s, softmmu_core.c, and bootstrap_softmmu.s

/**
 * @brief Page hashing functions for the three page sizes
 *        supported by our hashed MPU.
 * 
 * These page sizes are configurable set changing the LIBOS_CONFIG_PAGESIZE and LIBOS_CONFIG_PAGESIZE_LOG2
 * in your profile configurations.  You may inspect the libos-config.h generated file to see the computed
 * medium page size. @see LIBOS_CONFIG_PAGESIZE_MEDIUM
 * 
 * @param addr 
 * @return LwU64 
 */
extern LwU64      KernelBootstrapHash1pb(LwU64 addr);
extern LwU64      KernelBootstrapHashMedium(LwU64 addr);
extern LwU64      KernelBootstrapHashSmall(LwU64 addr);


void              KernelSoftmmuFlushPages(LwU64 virtualAddress, LwU64 size);
