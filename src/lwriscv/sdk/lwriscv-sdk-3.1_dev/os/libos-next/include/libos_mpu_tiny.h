#pragma once



#define LIBOS_MPU_CODE                       0

#define LIBOS_MPU_DATA_INIT_AND_KERNEL       1

#define LIBOS_MPU_INDEX_BOOTSTRAP_IDENTITY   2 /* this entry isn't used again outside boot */

#define LIBOS_MPU_MMIO_KERNEL                2

#define LIBOS_MPU_USER_INDEX                 3



#ifndef ASSEMBLY

/**

 * @brief KernelBootstrap a mapping into the MPU/TLB

 *

 * @note This call may only be used in init before the scheduler

 *       and paging subsystems have been enabled.

 *

 *       All addresses and sizes must be a multiple of

 *       LIBOS_CONFIG_MPU_PAGESIZE.

 *

 * @param va

 *     Virtual address of start of mapping. No collision detection

 *     will be performed.

 * @param pa

 *     Physical address to map

 * @param size

 *     Length of mapping.

 * @param attributes

 *     Processor specific MPU attributes or TLB attributes for mapping.

 */

void LibosBootstrapMmap(LwU64 index, LwU64 va, LwU64 pa, LwU64 size, LwU64 attributes);



void LibosInitHookMpu();

LwU32 LibosInitDaemon();

#endif