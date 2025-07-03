/* _LWRM_COPYRIGHT_BEGIN_

 *

 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All

 * information contained herein is proprietary and confidential to LWPU

 * Corporation.  Any use, reproduction, or disclosure without the written

 * permission of LWPU Corporation is prohibited.

 *

 * _LWRM_COPYRIGHT_END_

 */



/**

 *

 * @file bootstrap.c

 *

 * @brief The bootstrap module is responsible for setting up the MPU and

 *   switching to virtual mode.

 */

#include <libos-config.h>

#include <manifest_defines.h>

#include <peregrine-headers.h>

#include <libos_xcsr.h>



// MPU layout

#include "libos_mpu_tiny.h"



// We can't use DRF from GAS

#define LW_RISCV_CSR_SATP_MODE_LWMPU_M        0xf000000000000000

#define LW_RISCV_CSR_XMPUATTR_UR_M            0x01

#define LW_RISCV_CSR_XMPUATTR_UW_M            0x02

#define LW_RISCV_CSR_XMPUATTR_UX_M            0x04

#define LW_RISCV_CSR_XMPUATTR_SR_M            0x08

#define LW_RISCV_CSR_XMPUATTR_SW_M            0x10

#define LW_RISCV_CSR_XMPUATTR_SX_M            0x20

#define LW_RISCV_CSR_XMPUATTR_MR_M            0x200

#define LW_RISCV_CSR_XMPUATTR_MW_M            0x400

#define LW_RISCV_CSR_XMPUATTR_MX_M            0x800

#define LW_RISCV_CSR_XMPUATTR_CACHEABLE_M     0x40000       // 18:18

#define LW_RISCV_CSR_XMPUATTR_COHERENT_M      0x80000       // 19:18



#if LIBOS_CONFIG_RISCV_S_MODE

#   define LW_RISCV_CSR_XMPUATTR_XR_M   LW_RISCV_CSR_XMPUATTR_SR_M

#   define LW_RISCV_CSR_XMPUATTR_XW_M   LW_RISCV_CSR_XMPUATTR_SW_M

#   define LW_RISCV_CSR_XMPUATTR_XX_M   LW_RISCV_CSR_XMPUATTR_SX_M

#else

#   define LW_RISCV_CSR_XMPUATTR_XR_M   LW_RISCV_CSR_XMPUATTR_MR_M

#   define LW_RISCV_CSR_XMPUATTR_XW_M   LW_RISCV_CSR_XMPUATTR_MW_M

#   define LW_RISCV_CSR_XMPUATTR_XX_M   LW_RISCV_CSR_XMPUATTR_MX_M

#endif



/**

 * @brief This is the main kernel entrypoint.

 *

 * @return Does not return.

 */

.section .text.bootstrap

.global KernelBootstrap

KernelBootstrap:

        /*

         *   Disable linker relaxation to ensure code is compiled with PC

         *   relative relocations.

         */

        .option push

        .option norelax



        /*

         *  Compute the SMPUVA of the kernel code

         *

         *  @note We're using a absolute address relocation here to capture the VA.

         *  @todo: We could merge these two expressions with ease.

         */

        lui s0, %hi(taskall_code)

        addi  s0, s0, %lo(taskall_code)

        ori s0,s0, 1



        /*

         *  Compute the SMPUPA of the kernel code

         *

         *  @note We're using a PC relative relocation here.

         *        Our PC is lwrrently physically addressed, so we'll end up with the

         *        PA for the section start.

         */

        2:auipc s1, %pcrel_hi(taskall_code)

        addi  s1, s1, %pcrel_lo(2b)



        /*

         * Compute the SMPURNG of the kernel code

         *

         * @note The linker script is setup to define _size symbols.

         *       These are absolute address symbols where the address

         *       corresponds to the numeric size.

         *

         */

        lui s2, %hi(taskall_code_size)

        addi  s2, s2, %lo(taskall_code_size) /* @todo: ASSERT in layout.ld that the lower bits are 0 */



        /*

         * Program the kernel's code MPU entry

         *

         *  Access permissions: machine read/execute only.

         *

         *  @see LIBOS_MPU_INDEX_CACHE_CODE

         */

#define MPU_ATTR_CODE                           \

            LW_RISCV_CSR_XMPUATTR_COHERENT_M  | \

            LW_RISCV_CSR_XMPUATTR_CACHEABLE_M | \

            LW_RISCV_CSR_XMPUATTR_XR_M        | \

            LW_RISCV_CSR_XMPUATTR_UR_M        | \

            LW_RISCV_CSR_XMPUATTR_XX_M        | \

            LW_RISCV_CSR_XMPUATTR_UX_M



        csrwi       LW_RISCV_CSR_XMPUIDX,  LIBOS_MPU_CODE

        csrw        LW_RISCV_CSR_XMPUPA,   s1

        csrw        LW_RISCV_CSR_XMPURNG,  s2

        li      t1, MPU_ATTR_CODE

        csrw        LW_RISCV_CSR_XMPUATTR, t1

        csrw        LW_RISCV_CSR_XMPUVA,   s0



        /*

         * Compute the SMPUVA of the kernel data

         *

         *  @note We're using a absolute address relocation here to capture the VA.

         */

        lui s3, %hi(taskinit_and_kernel_data)

        addi  s3, s3, %lo(taskinit_and_kernel_data)

        ori s3,s3, 1 /* mark the entry valid */



        /*

         * Compute the SMPUPA of the kernel data

         *

         *  @note We're using a PC relative relocation here.  Our PC is lwrrently physically

         * addressed, so we'll end up with hte PA for the section start.

         */

        5:auipc s1, %pcrel_hi(taskinit_and_kernel_data)

        addi  s1, s1, %pcrel_lo(5b)



        /*

         * Compute the SMPURNG of the kernel data

         *

         * @note The linker script is setup to define _size symbols.  These are absolute address

         * symbols where the address corresponds to the numeric size.

         */

        lui s2, %hi(taskinit_and_kernel_data_size)

        addi  s2, s2, %lo(taskinit_and_kernel_data_size)



        /*

         *  Program the kernel's code MPU entry

         *

         *  Access permissions: machine read/write only.

         *

         *  @note This MPU entry is enabled solely for the

         *        (trusted) init task.

         */

#define MPU_ATTR_DATA \

        LW_RISCV_CSR_XMPUATTR_COHERENT_M | \

        LW_RISCV_CSR_XMPUATTR_CACHEABLE_M | \

        LW_RISCV_CSR_XMPUATTR_XR_M | \

        LW_RISCV_CSR_XMPUATTR_XW_M | \

        LW_RISCV_CSR_XMPUATTR_UR_M | \

        LW_RISCV_CSR_XMPUATTR_UW_M



        csrwi       LW_RISCV_CSR_XMPUIDX,  LIBOS_MPU_DATA_INIT_AND_KERNEL

        csrw        LW_RISCV_CSR_XMPUPA,   s1

        csrw        LW_RISCV_CSR_XMPURNG,  s2

        li      t1, MPU_ATTR_DATA

        csrw        LW_RISCV_CSR_XMPUATTR, t1

        csrw        LW_RISCV_CSR_XMPUVA,   s3



        /*

         *  Setup a *temporary* indentity map to allow continued access to memory

         *  to FB.

         *

         *  @note We're laying the virtual memory map on top of the physical

         *        memory map during the transition.  This happens because the

         *        MPU gives precendece to lower mappings (code+data are first).

         *

         *        The transition is completed when the PRI mapping is created

         *        during @see kernel_paging_init.

         *

         *        @see LIBOS_MPU_MMIO_KERNEL uses the same MPU index as the

         *             identity map.

         *

         *

         *  Physical Memory Map                            Virtual Memory Map

         *

         *  0GB ------------------------              0gb ------------------------

         *      |                      |                  |       Unmapped       |

         *      |                      |             256k ------------------------

         *      |      IMEM PA         | 4gb of PA        |      Kernel + Apps   |

         *      |                      |                  |                      |

         *      |                      |                  |                      |

         *  4GB ------------------------                  ------------------------

         *      |                      |

         *      |      DMEM PA         | 4b of PA

         *      |                      |

         *      ------------------------

         *

         *  Care has been taken to ensure that the virtual memory map nestles

         *  within the 4GB IMEM PA reserved region of the physical address space.

         *  This is guaranteed by two things

         *      1. The first linker assigned address is at address @see LIBOS_MAX_IMEM.

         *         (This is about 256kb, and the maximum amount of IMEM that LIBOS can address)

         *

         *      2. @see compile_ldscript in libos.py for generated assert to verify kernel

         *         fits neatly within this region of space (Kernel must fit within first 4G for

         * bootstrap)

         *

         */

#define MPU_ATTR_BOOTSTRAP \

        LW_RISCV_CSR_XMPUATTR_COHERENT_M | \

        LW_RISCV_CSR_XMPUATTR_CACHEABLE_M | \

        LW_RISCV_CSR_XMPUATTR_XR_M | \

        LW_RISCV_CSR_XMPUATTR_XW_M | \

        LW_RISCV_CSR_XMPUATTR_XX_M



        li      s2, 0xFFFFFFFFFFFFF000ULL

        csrwi   LW_RISCV_CSR_XMPUIDX,  LIBOS_MPU_INDEX_BOOTSTRAP_IDENTITY

        csrwi   LW_RISCV_CSR_XMPUPA,   0

        csrw    LW_RISCV_CSR_XMPURNG,  s2

        li      t1, MPU_ATTR_BOOTSTRAP

        csrw    LW_RISCV_CSR_XMPUATTR, t1

        csrwi   LW_RISCV_CSR_XMPUVA,   1



        /*

         *  Enable the MPU and update our PC

         *

         *  @note We manually specifiy an absolute relocation to ensure

         *     our PC is no longer in the 0x6180000000000000 aperture.

         */

#if LIBOS_CONFIG_RISCV_S_MODE

        li      t0,      LW_RISCV_CSR_SATP_MODE_LWMPU_M

        csrs    LW_RISCV_CSR_SATP, t0

#else

        li      t0,      LW_RISCV_CSR_MSTATUS_VM_LWMPU_M

        csrs    LW_RISCV_CSR_MSTATUS, t0

#endif

        lui     t0,      %hi(.next)

        addi    t0, t0,  %lo(.next)

        jalr    x0, t0, 0

        .next:



        /*

         *  MPU enabled, allow linker relaxation to absolute addresses

         *  and remove the identity mapping

         */



        .option pop

        csrwi   LW_RISCV_CSR_XMPUVA,   0



        /*

         *  Setup the kernel stack and jump to C code

         *

         *  @note a0 still holds the PHDRBootVirtualToPhysicalDelta from

         *        above.

         */



        la      sp, KernelSchedulerStackTop             // @note LIBOS_TINY uses the scheduler stack for everything

        j       KernelInit                                    

