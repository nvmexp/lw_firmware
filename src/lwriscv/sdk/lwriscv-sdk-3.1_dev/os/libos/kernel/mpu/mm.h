#pragma once
#include "libos.h"

// Setup in bootstrap.c
#define LIBOS_MPU_INDEX_BOOTSTRAP_IDENTITY LIBOS_MPU_MMIO_KERNEL /* this entry isn't used again outside boot */

void LIBOS_SECTION_TEXT_COLD KernelSyscallBootstrapMmap(LwU64 index, LwU64 va, LwU64 pa, LwU64 size, LwU64 attributes);

void KerneMpuLoad();
