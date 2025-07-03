/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "dev_gsp_riscv_csr_64.h"
#include "dev_riscv_csr_64_addendum.h"
#include "lw_gsp_riscv_address_map.h"
#include "interrupts.h"
#include "kernel.h"
#include "paging.h"
#include "task.h"

/**
 *
 * @file bootstrap.c
 *
 * @brief The bootstrap module is responsible for setting up the MPU and
 *   switching to virtual mode.
 */

/**
 * @brief This is the main kernel entrypoint.
 *  Exelwtion control comes from the chipmux @see libos_start.
 *
 * @return Does not return.
 */
LIBOS_NORETURN LIBOS_SECTION_KERNEL_BOOTSTRAP LIBOS_NAKED void kernel_bootstrap(
  libos_bootstrap_args_t *args)                            // a0
{
    __asm(

        /*
         *   Disable linker relaxation to ensure code is compiled with PC
         *   relative relocations For details @see libos_start.
         */
        ".option push;"
        ".option norelax;"

        /*
         *  Scrub the first MPU entry, which may be a full identity mapping
         *  left enabled by bootloader, so that it doesn't interfere with the
         *  CACHE_CODE/CACHE_DATA mappings when we re-enable the MPU.
         */
        "csrwi       %[RISCV_CSR_XMPUIDX], 0;"
        "csrwi       %[RISCV_CSR_XMPUVA],  0;"

        /*
         *  Compute the SMPUVA of the kernel code
         *
         *  @note We're using a absolute address relocation here to capture the VA.
         */
        "lui s0, %%hi(code_cached_start);"
        "addi  s0, s0, %%lo(code_cached_start);"
        "ori s0,s0, 1;" /* mark the entry valid */

        /*
         *  Compute the SMPUPA of the kernel code
         *
         *  @note We're using a PC relative relocation here.
         *        Our PC is lwrrently physically addressed, so we'll end up with the
         *        PA for the section start.
         */
        "2:auipc s1, %%pcrel_hi(code_cached_start);"
        "addi  s1, s1, %%pcrel_lo(2b);"

        /*
         * Compute the SMPURNG of the kernel code
         *
         * @note The linker script is setup to define "_size" symbols.
         *       These are absolute address symbols where the address
         *       corresponds to the numeric size.
         *
         */
        "lui s2, %%hi(code_cached_size);"
        "addi  s2, s2, %%lo(code_cached_size);"

        /*
         * Compute the virtual to physical delta
         *
         *  @note The final link produces a single section 'image' so we're
         *    guaranteed that a single offset is sufficient to translate from
         *    VA to PA for global variables and backing stores.
         *
         */
        "sub   a1, s1, s0;"
        "add   a1, a1, 1;" /* remove the valid bit */

        /*
         * Program the kernel's code MPU entry
         *
         *  Access permissions: machine read/execute only.
         */
        "li      t0, %[XMPUIDX_CACHE_CODE];"
        "csrw        %[RISCV_CSR_XMPUIDX],  t0;"
        "csrw        %[RISCV_CSR_XMPUPA],   s1;"
        "csrw        %[RISCV_CSR_XMPURNG],  s2;"
        "li      t1, %[XMPUATTR_CODE];"
        "csrw        %[RISCV_CSR_XMPUATTR], t1;"
        "csrw        %[RISCV_CSR_XMPUVA],   s0;"

        /*
         * Compute the SMPUVA of the kernel data
         *
         *  @note We're using a absolute address relocation here to capture the VA.
         */
        "lui s3, %%hi(kernel_data_cached_start);"
        "addi  s3, s3, %%lo(kernel_data_cached_start);"
        "ori s3,s3, 1;" /* mark the entry valid */

        /*
         * Compute the SMPUPA of the kernel data
         *
         *  @note We're using a PC relative relocation here.  Our PC is lwrrently physically
         * addressed, so we'll end up with the PA for the section start.
         */
        "5:auipc s1, %%pcrel_hi(kernel_data_cached_start);"
        "addi  s1, s1, %%pcrel_lo(5b);"

        /*
         * Compute the SMPURNG of the kernel data
         *
         * @note The linker script is setup to define "_size" symbols.  These are absolute address
         * symbols where the address corresponds to the numeric size.
         */
        "lui s2, %%hi(kernel_data_cached_size);"
        "addi  s2, s2, %%lo(kernel_data_cached_size);"

        /*
         *  Program the kernel's data MPU entry
         *
         *  Access permissions: machine read/write only.
         */
        "li      t0, %[XMPUIDX_CACHE_DATA];"
        "csrw        %[RISCV_CSR_XMPUIDX],  t0;"
        "csrw        %[RISCV_CSR_XMPUPA],   s1;"
        "csrw        %[RISCV_CSR_XMPURNG],  s2;"
        "li      t1, %[XMPUATTR_DATA];"
        "csrw        %[RISCV_CSR_XMPUATTR], t1;"
        "csrw        %[RISCV_CSR_XMPUVA],   s3;"

        /*
         *  Setup a *temporary* indentity map to allow continued access to all memory.
         *  @note We're laying the virtual memory map on top of the physical
         *        memory map during the transition.  This happens because the
         *        MPU gives precendece to lower mappings (code+data are first).
         *
         *        The transition is completed when the PRI mapping is created
         *        during @see kernel_mmu_load_kernel.
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
         */
        "li s2, 0xFFFFFFFFFFFFF000ULL;"
        "li      t0, %[XMPUIDX_SCRATCH];"
        "csrw        %[RISCV_CSR_XMPUIDX],  t0;"
        "csrwi       %[RISCV_CSR_XMPUPA],   0;"
        "csrw        %[RISCV_CSR_XMPURNG],  s2;"
        "li      t1, %[XMPUATTR_IDENTITY];"
        "csrw        %[RISCV_CSR_XMPUATTR], t1;"
        "csrwi       %[RISCV_CSR_XMPUVA],   1;"

        /*
         *  Enable the MPU and update our PC
         *
         *  @note We manually specifiy an absolute relocation to ensure
         *     our PC is no longer in the 0x6180000000000000 aperture.
         */
#if LIBOS_CONFIG_RISCV_S_MODE
        "li    t0,      %[RISCV_CSR_SATP_MODE_LWMPU_MASK];"
        "csrs           %[RISCV_CSR_SATP], t0;"
#else
        "li    t0,      %[RISCV_CSR_MSTATUS_VM_LWMPU_MASK];"
        "csrs           %[RISCV_CSR_MSTATUS], t0;"
#endif
        "lui   t0,      %%hi(.next);"
        "addi  t0, t0,  %%lo(.next);"
        "jalr  x0, t0, 0;"
        ".next:;"

        /*
         *  MPU enabled, allow linker relaxation to absolute addresses
         *  and remove the identity mapping
         */

        ".option pop;"
        "csrwi       %[RISCV_CSR_XMPUVA],   0;"

        /*
         *  Setup the kernel stack and jump to C code
         *
         *  @note a1 still holds the phdr_boot_virtual_to_physical_delta from
         *        above.
         */

        "la sp, kernel_stack+%c[STACK_SIZE];"
        "j kernel_init;"

        ::[STACK_SIZE] "i"(LIBOS_KERNEL_STACK_SIZE),
        [ XMPUIDX_CACHE_CODE ] "i"(LIBOS_MPU_INDEX_CACHE_CODE),
        [ XMPUATTR_CODE ] "i"(
            REF_NUM64(LW_RISCV_CSR_XMPUATTR_COHERENT, 1u) | REF_NUM64(LW_RISCV_CSR_XMPUATTR_CACHEABLE, 1u) |
            REF_NUM64(LW_RISCV_CSR_XMPUATTR_UX, 1u) | REF_NUM64(LW_RISCV_CSR_XMPUATTR_XR, 1u) |
            REF_NUM64(LW_RISCV_CSR_XMPUATTR_XX, 1u) | REF_NUM64(LW_RISCV_CSR_XMPUATTR_WPR, LIBOS_CONFIG_WPR)),
        [ XMPUIDX_CACHE_DATA ] "i"(LIBOS_MPU_INDEX_CACHE_DATA),
        [ XMPUATTR_DATA ] "i"(
            REF_NUM64(LW_RISCV_CSR_XMPUATTR_COHERENT, 1u) | REF_NUM64(LW_RISCV_CSR_XMPUATTR_CACHEABLE, 1u) |
            REF_NUM64(LW_RISCV_CSR_XMPUATTR_XR, 1u) | REF_NUM64(LW_RISCV_CSR_XMPUATTR_XW, 1u) |
            REF_NUM64(LW_RISCV_CSR_XMPUATTR_WPR, LIBOS_CONFIG_WPR)),
        [ XMPUIDX_SCRATCH ] "i"(LIBOS_MPU_INDEX_SCRATCH),
        [ XMPUATTR_IDENTITY ] "i"(
            REF_NUM64(LW_RISCV_CSR_XMPUATTR_COHERENT, 1u) | REF_NUM64(LW_RISCV_CSR_XMPUATTR_CACHEABLE, 1u) |
            REF_NUM64(LW_RISCV_CSR_XMPUATTR_XR, 1u) | REF_NUM64(LW_RISCV_CSR_XMPUATTR_XW, 1u) |
            REF_NUM64(LW_RISCV_CSR_XMPUATTR_XX, 1u) | REF_NUM64(LW_RISCV_CSR_XMPUATTR_WPR, LIBOS_CONFIG_WPR)),
        [ RISCV_CSR_XMPUIDX ] "i"(LW_RISCV_CSR_XMPUIDX), [ RISCV_CSR_XMPUPA ] "i"(LW_RISCV_CSR_XMPUPA),
        [ RISCV_CSR_XMPURNG ] "i"(LW_RISCV_CSR_XMPURNG), [ RISCV_CSR_XMPUATTR ] "i"(LW_RISCV_CSR_XMPUATTR),
        [ RISCV_CSR_XMPUVA ] "i"(LW_RISCV_CSR_XMPUVA),
#if LIBOS_CONFIG_RISCV_S_MODE
        [ RISCV_CSR_SATP ] "i"(LW_RISCV_CSR_SATP),
        [ RISCV_CSR_SATP_MODE_LWMPU_MASK ] "i"(REF_DEF64(LW_RISCV_CSR_SATP_MODE, _LWMPU)));
#else
        [ RISCV_CSR_MSTATUS ] "i"(LW_RISCV_CSR_MSTATUS),
        [ RISCV_CSR_MSTATUS_VM_LWMPU_MASK ] "i"(REF_DEF64(LW_RISCV_CSR_MSTATUS_VM, _LWMPU)));
#endif
    __builtin_unreachable();
}
