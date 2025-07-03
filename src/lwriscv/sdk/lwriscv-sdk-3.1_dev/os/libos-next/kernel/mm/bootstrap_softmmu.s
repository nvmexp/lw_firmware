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

 * @file bootstrap_tlb.s

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

#define LW_RISCV_CSR_SATP_MODE_HASH_M         0xe000000000000000

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



#define MPU_ATTR_CODE                           \

        LW_RISCV_CSR_XMPUATTR_CACHEABLE_M | \

        LW_RISCV_CSR_XMPUATTR_XR_M        | \

        LW_RISCV_CSR_XMPUATTR_UR_M        | \

        LW_RISCV_CSR_XMPUATTR_XX_M        | \

        LW_RISCV_CSR_XMPUATTR_UX_M



#define MPU_ATTR_DATA \

        LW_RISCV_CSR_XMPUATTR_CACHEABLE_M | \

        LW_RISCV_CSR_XMPUATTR_XR_M | \

        LW_RISCV_CSR_XMPUATTR_XW_M | \

        LW_RISCV_CSR_XMPUATTR_UR_M | \

        LW_RISCV_CSR_XMPUATTR_UW_M

/**

 * @brief Declare the init stack

 */

.section .hot.data.KernelInitStack

.align 8

KernelInitStack:

   .fill LIBOS_CONFIG_KERNEL_STACK_SIZE, 1, 0

KernelInitStackTop:



.section .text.bootstrap

/**

 * @brief This is the main kernel entrypoint.

 *

 * @return Does not return.

 */



.global _start

_start:

        mv    t6, a0



        /*

         *   Disable linker relaxation to ensure code is compiled with PC

         *   relative relocations.

         */

        .option push

        .option norelax



        /*

         *  Setup a identity mapping of this code

         */

.relocation.code.bootstrap.page:

        auipc a0, %pcrel_hi(_start)

        addi  a0, a0, %pcrel_lo(.relocation.code.bootstrap.page)



        /* Round down to start of page */

        li    t0, LIBOS_CONFIG_PAGESIZE-1

        xori  t0, t0, -1

        and   a0, a0, t0

        mv    t4, a0



#if LIBOS_CONFIG_MPU_HASHED

        jal   ra, KernelBootstrapHashSmall       

        addi  a0, a0, 1

        mv    t3, a0

#else

        li    t3, LIBOS_CONFIG_MPU_BOOTSTRAP

#endif



        csrw  LW_RISCV_CSR_XMPUIDX,  t3

        csrw  LW_RISCV_CSR_XMPUPA,   t4

        li    a0, LIBOS_CONFIG_PAGESIZE

        csrw  LW_RISCV_CSR_XMPURNG,  a0

        li    t1, MPU_ATTR_CODE

        csrw  LW_RISCV_CSR_XMPUATTR, t1

        or    t4, t4, 1

        csrw  LW_RISCV_CSR_XMPUVA,   t4



        /*

         *  Enable the MPU and update our PC

         *

         *  @note We manually specifiy an absolute relocation to ensure

         *     our PC is no longer in the 0x6180000000000000 aperture.

         */



#if LIBOS_CONFIG_MPU_HASHED && LIBOS_CONFIG_RISCV_S_MODE

        /* Program the hashing mode for the MPU */

#define  LIBOS_SMPUHP   (LIBOS_CONFIG_HASHED_MPU_SMALL_S0)   | \

                    (LIBOS_CONFIG_HASHED_MPU_SMALL_S1 << 8)  | \

                    (LIBOS_CONFIG_HASHED_MPU_MEDIUM_S0 << 16) | \

                    (LIBOS_CONFIG_HASHED_MPU_MEDIUM_S1 << 24) | \

                    (LIBOS_CONFIG_HASHED_MPU_1PB_S0 << 32) | \

                    (LIBOS_CONFIG_HASHED_MPU_1PB_S1 << 40) 



        li    t0, LIBOS_SMPUHP

        csrs  LW_RISCV_CSR_SMPUHP, t0



        /* Enable hashed MPU */

        li    t0, LW_RISCV_CSR_SATP_MODE_HASH_M

        csrs  LW_RISCV_CSR_SATP, t0

#elif LIBOS_CONFIG_RISCV_S_MODE

        /* Enable Supervisor mode MPU */

        li    t0,      LW_RISCV_CSR_SATP_MODE_LWMPU_M

        csrs  LW_RISCV_CSR_SATP, t0

#else

        /* Enable Machine mode MPU */

        li    t0,      LW_RISCV_CSR_MSTATUS_VM_LWMPU_M

        csrs  LW_RISCV_CSR_MSTATUS, t0

#endif





        /*

         *  Compute the SMPUVA of the kernel code

         *

         *  @note We're using a absolute address relocation here to capture the VA.

         *  @todo: We could merge these two expressions with ease.

         */

        lui   t5, %hi(taskall_code)

        addi  t5, t5, %lo(taskall_code)

        ori   t5, t5, 1



        /*

         * Program the kernel's code MPU entry

         *

         *  Access permissions: machine read/execute only.

         *

         */

#if LIBOS_CONFIG_MPU_HASHED

        mv    a0, t5

        jal   ra, KernelBootstrapHashMedium        

#else

        li    a0, LIBOS_CONFIG_MPU_TEXT

#endif

        csrw  LW_RISCV_CSR_XMPUIDX,  a0



        /*

         *  @note We're using a PC relative relocation here.

         *        Our PC is lwrrently physically addressed, so we'll end up with the

         *        PA for the section start.

         */

.relocation.code:

        auipc s2, %pcrel_hi(taskall_code)

        addi  s2, s2, %pcrel_lo(.relocation.code)

        csrw  LW_RISCV_CSR_XMPUPA,   s2



        /*

         * @note The linker script is setup to define _size symbols.

         *       These are absolute address symbols where the address

         *       corresponds to the numeric size.

         */

        lui   t0, %hi(taskall_code_size)             

        addi  t0, t0, %lo(taskall_code_size)         

        csrw  LW_RISCV_CSR_XMPURNG,  t0              



        li    t0, MPU_ATTR_CODE

        csrw  LW_RISCV_CSR_XMPUATTR, t0



        csrw  LW_RISCV_CSR_XMPUVA,   t5

      

        lui   t0,      %hi(.translation.virtual)

        addi  t0, t0,  %lo(.translation.virtual)

        jalr  x0, t0, 0

.translation.virtual:



        /*

         *  MPU enabled remove temporary identity mapping of code page

         */

        .option pop

        csrw LW_RISCV_CSR_XMPUIDX,   t3 

        csrwi LW_RISCV_CSR_XMPUVA,   0          





        /*

         * Compute the SMPUVA of the kernel data

         *

         *  @note We're using a absolute address relocation here to capture the VA.

         */

        lui   t5, %hi(taskinit_and_kernel_data)

        addi  t5, t5, %lo(taskinit_and_kernel_data)

        ori   t5, t5, 1 /* mark the entry valid */





        /*

         * Compute the SMPURNG of the kernel data

         *

         * @note The linker script is setup to define _size symbols.  These are absolute address

         * symbols where the address corresponds to the numeric size.

         */

        lui   t4, %hi(taskinit_and_kernel_data_size)

        addi  t4, t4, %lo(taskinit_and_kernel_data_size)



        /*

         *  Program the kernel's data MPU entry

         *

         *  Access permissions: machine read/write only.

         *

         *  @see LIBOS_MPU_INDEX_CACHE_DATA

         */



#if LIBOS_CONFIG_MPU_HASHED        

        mv    a0, t5

        jal   ra, KernelBootstrapHashMedium

#else

        li    a0, LIBOS_CONFIG_MPU_DATA

#endif

        csrw  LW_RISCV_CSR_XMPUIDX,  a0

        csrw  LW_RISCV_CSR_XMPUPA,   a7

        csrw  LW_RISCV_CSR_XMPURNG,  t4

        li    t1, MPU_ATTR_DATA

        csrw  LW_RISCV_CSR_XMPUATTR, t1

        csrw  LW_RISCV_CSR_XMPUVA,   t5







        /*

         *  Create mapping of FB for SoftTLB and Kernel

         */

        li    t2, LIBOS_CONFIG_SOFTTLB_MAPPING_BASE

        mv    a0, t2

#if LIBOS_CONFIG_MPU_HASHED

        jal   ra, KernelBootstrapHash1pb        

        csrw  LW_RISCV_CSR_XMPUIDX,  a0

#else

        li    a0, LIBOS_CONFIG_MPU_IDENTITY

#endif



        csrw  LW_RISCV_CSR_XMPUIDX,  a0

        li    t0, LW_RISCV_AMAP_FBGPA_START

        csrw  LW_RISCV_CSR_XMPUPA,   t0

        li    a0, 1<<LIBOS_CONFIG_HASHED_MPU_1PB_S0

        csrw  LW_RISCV_CSR_XMPURNG,  a0

        li    t1, MPU_ATTR_DATA

        csrw  LW_RISCV_CSR_XMPUATTR, t1

        addi  t2, t2, 1

        csrw  LW_RISCV_CSR_XMPUVA,   t2



        /*

         *  Setup the kernel stack

         */

        la    sp, KernelInitStackTop



        mv    a0, t6

        jal   x0, KernelPartitionEntry



#if LIBOS_CONFIG_MPU_HASHED

.global KernelBootstrapHashSmall

KernelBootstrapHashSmall:

    srli  t0, a0, LIBOS_CONFIG_HASHED_MPU_SMALL_S0-2        // Compute the page hash

    srli  t1, a0, LIBOS_CONFIG_HASHED_MPU_SMALL_S1-2

    xor   t0, t0, t1

    andi  a0, t0, (LIBOS_CONFIG_MPU_INDEX_COUNT-1) &~ 3

    jalr  x0, ra



.global KernelBootstrapHashMedium

KernelBootstrapHashMedium:

    srli  t0, a0, LIBOS_CONFIG_HASHED_MPU_MEDIUM_S0-2        // Compute the page hash

    srli  t1, a0, LIBOS_CONFIG_HASHED_MPU_MEDIUM_S1-2

    xor   t0, t0, t1

    andi  a0, t0, (LIBOS_CONFIG_MPU_INDEX_COUNT-1) &~ 3

    jalr  x0, ra



.global KernelBootstrapHash1pb

KernelBootstrapHash1pb:

    srli  t0, a0, LIBOS_CONFIG_HASHED_MPU_1PB_S0-2        // Compute the page hash

    srli  t1, a0, LIBOS_CONFIG_HASHED_MPU_1PB_S1-2

    xor   t0, t0, t1

    andi  a0, t0, (LIBOS_CONFIG_MPU_INDEX_COUNT-1) &~ 3

    jalr  x0, ra

#endif