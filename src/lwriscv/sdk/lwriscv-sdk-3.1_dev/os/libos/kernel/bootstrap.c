/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "lwmisc.h"
#include "lwtypes.h"
#include "kernel.h"
#include "task.h"

#include "mm.h"

/**
 *
 * @file bootstrap.c
 *
 * @brief The bootstrap module is responsible for setting up the MPU and
 *   switching to virtual mode.
 */

/**
 * @brief This is the main kernel entrypoint.
 *
 * @return Does not return.
 */
LIBOS_NORETURN LIBOS_SECTION_TEXT_STARTUP LIBOS_NAKED void KernelBootstrap
(
    // The initial arguments from SK/bootloader or initial partition switch
    // arguments from another partition will be set as arguments for the init
    // task.
    // Or, the subsequent partition switch arguments from another partition will
    // be set as return values for the resumeTask.
    // @see KernelInit and LibosPartitionSwitchInternal
    LwU64 param1,
    LwU64 param2,
    LwU64 param3,
    LwU64 param4,
    LwU64 param5
)
{
    __asm(

        /*
         *   Disable linker relaxation to ensure code is compiled with PC
         *   relative relocations.
         */
        ".option push;"
        ".option norelax;"

        /*
         *  Compute the SMPUVA of the kernel code
         *
         *  @note We're using a absolute address relocation here to capture the VA.
         *  @todo: We could merge these two expressions with ease.
         */
        "lui s0, %%hi(taskall_code);"
        "addi  s0, s0, %%lo(taskall_code);"
        "ori s0,s0, 1;"

        /*
         *  Compute the SMPUPA of the kernel code
         *
         *  @note We're using a PC relative relocation here.
         *        Our PC is lwrrently physically addressed, so we'll end up with the
         *        PA for the section start.
         */
        "2:auipc s1, %%pcrel_hi(taskall_code);"
        "addi  s1, s1, %%pcrel_lo(2b);"

        /*
         * Compute the SMPURNG of the kernel code
         *
         * @note The linker script is setup to define "_size" symbols.
         *       These are absolute address symbols where the address
         *       corresponds to the numeric size.
         *
         */
        "lui s2, %%hi(taskall_code_size);"
        "addi  s2, s2, %%lo(taskall_code_size);" /* @todo: ASSERT in layout.ld that the lower bits are 0 */

#ifdef LIBOS_FEATURE_PAGING
        /*
         * Compute the virtual to physical delta
         *
         *  @note The final link produces a single section 'image' so we're
         *    guaranteed that a single offset is sufficient to translate from
         *    VA to PA for global variables and backing stores.
         *
         */
        "sub   a5, s1, s0;"
        "add   a5, a5, 1;" /* remove the valid bit */
#endif

        /*
         * Program the kernel's code MPU entry
         *
         *  Access permissions: machine read/execute only.
         *
         *  @see LIBOS_MPU_INDEX_CACHE_CODE
         */
        "csrwi       %[RISCV_CSR_XMPUIDX],  %[MPU_INDEX_CODE];"
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
        "lui s3, %%hi(taskinit_and_kernel_data);"
        "addi  s3, s3, %%lo(taskinit_and_kernel_data);"
        "ori s3,s3, 1;" /* mark the entry valid */

        /*
         * Compute the SMPUPA of the kernel data
         *
         *  @note We're using a PC relative relocation here.  Our PC is lwrrently physically
         * addressed, so we'll end up with hte PA for the section start.
         */
        "5:auipc s1, %%pcrel_hi(taskinit_and_kernel_data);"
        "addi  s1, s1, %%pcrel_lo(5b);"

        /*
         * Compute the SMPURNG of the kernel data
         *
         * @note The linker script is setup to define "_size" symbols.  These are absolute address
         * symbols where the address corresponds to the numeric size.
         */
        "lui s2, %%hi(taskinit_and_kernel_data_size);"
        "addi  s2, s2, %%lo(taskinit_and_kernel_data_size);"

        /*
         *  Program the kernel's code MPU entry
         *
         *  Access permissions: machine read/write only.
         *
         *  @see LIBOS_MPU_INDEX_CACHE_DATA
         */
        "csrwi       %[RISCV_CSR_XMPUIDX],  %[MPU_INDEX_DATA];"
        "csrw        %[RISCV_CSR_XMPUPA],   s1;"
        "csrw        %[RISCV_CSR_XMPURNG],  s2;"
        "li      t1, %[XMPUATTR_DATA];"
        "csrw        %[RISCV_CSR_XMPUATTR], t1;"
        "csrw        %[RISCV_CSR_XMPUVA],   s3;"

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
         *         fits neatly within this region of space ("Kernel must fit within first 4G for
         * bootstrap")
         *
         */
        "li s2, 0xFFFFFFFFFFFFF000ULL;"
        "csrwi       %[RISCV_CSR_XMPUIDX],  %[MPU_INDEX_BOOTSTRAP_IDENTITY];"
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
         *  @note a5 still holds the PHDRBootVirtualToPhysicalDelta from
         *        above.
         */

        "la sp, KernelStack+%c[STACK_SIZE];"
        "j KernelInit;"

        ::[STACK_SIZE] "i"(LIBOS_CONFIG_KERNEL_STACK_SIZE),
        [ MPU_INDEX_CODE ] "i"(LIBOS_MPU_CODE),
        [ MPU_INDEX_DATA ] "i"(LIBOS_MPU_DATA_INIT_AND_KERNEL),
        [ MPU_INDEX_BOOTSTRAP_IDENTITY ] "i" (LIBOS_MPU_INDEX_BOOTSTRAP_IDENTITY),
        [ XMPUATTR_CODE ] "i"(
            REF_NUM64(LW_RISCV_CSR_XMPUATTR_COHERENT, 1u) | REF_NUM64(LW_RISCV_CSR_XMPUATTR_CACHEABLE, 1u) |
            REF_NUM64(LW_RISCV_CSR_XMPUATTR_XR, 1u) | REF_NUM64(LW_RISCV_CSR_XMPUATTR_UR, 1u) |
            REF_NUM64(LW_RISCV_CSR_XMPUATTR_XX, 1u) | REF_NUM64(LW_RISCV_CSR_XMPUATTR_UX, 1u)),
        [ XMPUATTR_DATA ] "i"(
            REF_NUM64(LW_RISCV_CSR_XMPUATTR_COHERENT, 1u) | REF_NUM64(LW_RISCV_CSR_XMPUATTR_CACHEABLE, 1u) |
            REF_NUM64(LW_RISCV_CSR_XMPUATTR_XR, 1u) | REF_NUM64(LW_RISCV_CSR_XMPUATTR_XW, 1u) |
            REF_NUM64(LW_RISCV_CSR_XMPUATTR_UR, 1u) | REF_NUM64(LW_RISCV_CSR_XMPUATTR_UW, 1u)),
        [ XMPUATTR_IDENTITY ] "i"(
            REF_NUM64(LW_RISCV_CSR_XMPUATTR_COHERENT, 1u) | REF_NUM64(LW_RISCV_CSR_XMPUATTR_CACHEABLE, 1u) |
            REF_NUM64(LW_RISCV_CSR_XMPUATTR_XR, 1u) | REF_NUM64(LW_RISCV_CSR_XMPUATTR_XW, 1u) |
            REF_NUM64(LW_RISCV_CSR_XMPUATTR_XX, 1u)),
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
