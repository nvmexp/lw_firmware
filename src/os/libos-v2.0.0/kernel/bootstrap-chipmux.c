/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "kernel.h"
#include <drivers/common/inc/hwref/hopper/gh100/dev_gsp.h>
#include <drivers/common/inc/hwref/hopper/gh100/dev_master.h>
#include <drivers/common/inc/hwref/hopper/gh100/lw_gsp_riscv_address_map.h>

/*!
 * @file bootstrap-chipmux.c
 * @brief The chipmux is responsible for validating the exelwtion
 *  environment and selecting the appropriate kernel.
 *
 *  The kernel loaded for each version of the Peregrine IP core is
 *  specifically tuned to ensure no uncessary code or branching
 *  is loaded into TCM.
 *
 *  Tasks may be shared between multiple architectures.
 *
 * @note Hardware hasn't been reving the peregrine core version
 *      so we must look at the PMC_BOOT_0 register to determine
 *      which processor we're running on (!).
 */

/*!
 * @brief This is the main firmware entrypoint.
 *  It is guaranteed to be at offset 0 of the final linked firmware
 *
 * \param void
 *
 * \return Does not return.
 */
LIBOS_SECTION_TEXT_STARTUP LIBOS_NAKED LIBOS_NORETURN void libos_start(
  libos_bootstrap_args_t *args)  // a0
{
    __asm(
        /*
            Kernel entry point code runs with a PC address somewhere within either
              FB(0x618000xxxxxxxxxx) or IMEM(0).  We must ensure the linker generates PC
              relative addressing for bootstrap code.

            Assumptions: Code compiled with -mcmodel=medlow / -mcmodel=medany
            .option norelax forces PC relative addressing for this function.

            We use RISC-V's medium memory model, which ensures that all code and data sections are
           within 2GB. The compiler is required to emit an auipc instruction to compute the address
           relative to the PC pointer.

            Normally, the linker is allowed to relax auipc's to lui's (or sometimes just a straight
           load). This happens because the linker sees that we're compiling an ELF with no
           relocations.

            Since the MPU isn't yet enabled, we're running at somewhere inside 0x618000xxxxxxxxxx

         */
        ".option push;"
        ".option norelax;"

        /*
            Read PMC_BOOT42 to determine which chip we're on.
            FUTURE: Hardware didn't bump the architecture version between changes to the IP cores.
                    Ideally, there'd be a falcon register to tell us this.
         */
        "li t0, %[RISCV_PA_FOR_PRIV_START] + %[PMC_BOOT_42_OFFSET];"
        "lw t0, 0(t0);"

        /*
            Extract the CHIP_ID field
         */
        "srli t0, t0, %[PMC_BOOT_42_CHIP_ID_SHIFT];" /* Shift down to the architecture field */
        "andi t0, t0, %[PMC_BOOT_42_CHIP_ID_MASK];"  /* Shift out the remaining data         */

        /*
            Boot to the Tu10x kernel
         */
        "li t1,       %[PMC_BOOT_42_CHIP_ID_TU102];"
        "beq t0, t1, .boot_turing;"
        "li t1,       %[PMC_BOOT_42_CHIP_ID_TU104];"
        "beq t0, t1, .boot_turing;"
        "li t1,       %[PMC_BOOT_42_CHIP_ID_TU105];"
        "beq t0, t1, .boot_turing;"
        "li t1,       %[PMC_BOOT_42_CHIP_ID_TU106];"
        "beq t0, t1, .boot_turing;"
        "li t1,       %[PMC_BOOT_42_CHIP_ID_TU107];"
        "beq t0, t1, .boot_turing;"
        "li t1,       %[PMC_BOOT_42_CHIP_ID_TU116];"
        "beq t0, t1, .boot_turing;"
        "li t1,       %[PMC_BOOT_42_CHIP_ID_TU117];"
        "bne t0, t1, .skip_turing;"
        ".boot_turing:;"
        "call tu10x_kernel_bootstrap;"
        ".skip_turing:;"

        /*
            Boot to the Ga100 kernel
         */
        "li t2,       %[PMC_BOOT_42_CHIP_ID_GA101];"
        "beq t0, t2, .boot_ampere;"
        "li t2,       %[PMC_BOOT_42_CHIP_ID_GA100];"
        "bne t0, t2, .skip_ga100;"
        ".boot_ampere:;"
        "call ga100_kernel_bootstrap;"
        ".skip_ga100:;"

        /*
            Boot to the Ga102 kernel
         */
        "li t2,       %[PMC_BOOT_42_CHIP_ID_GA102];"
        "beq t0, t2, .boot_ampere_10x;"
        "li t2,       %[PMC_BOOT_42_CHIP_ID_GA102F];"
        "beq t0, t2, .boot_ampere_10x;"
        "li t2,       %[PMC_BOOT_42_CHIP_ID_GA103];"
        "beq t0, t2, .boot_ampere_10x;"
        "li t2,       %[PMC_BOOT_42_CHIP_ID_GA104];"
        "beq t0, t2, .boot_ampere_10x;"
        "li t2,       %[PMC_BOOT_42_CHIP_ID_GA106];"
        "beq t0, t2, .boot_ampere_10x;"
        "li t2,       %[PMC_BOOT_42_CHIP_ID_GA107];"
        "bne t0, t2, .skip_ga10x;"
        ".boot_ampere_10x:;"
        "call ga10x_kernel_bootstrap;"
        ".skip_ga10x:;"

        /*
            Boot to the GH100 kernel
         */
        "li t2,       %[PMC_BOOT_42_CHIP_ID_GH100];"
        "bne t0, t2, .skip_gh100;"
        "call t0, gh100_kernel_bootstrap;"
        ".skip_gh100:;"

        /*
            (!) Unsupported chip
         */
        ".unknown_chip:;"
        "j .unknown_chip;"

        ".option pop;"
        :: [RISCV_PA_FOR_PRIV_START] "i"(LW_RISCV_AMAP_PRIV_START),
        [ PMC_BOOT_0_ARCHITECTURE_SHIFT ] "i"(REF_SHIFT64(LW_PMC_BOOT_0_ARCHITECTURE)),
        [ PMC_BOOT_0_ARCHITECTURE_MASK ] "i"(REF_MASK64(LW_PMC_BOOT_0_ARCHITECTURE)),
        [ PMC_BOOT_0_ARCHITECTURE_TU100 ] "i"(LW_PMC_BOOT_0_ARCHITECTURE_TU100),
        [ PMC_BOOT_0_ARCHITECTURE_GA100 ] "i"(LW_PMC_BOOT_0_ARCHITECTURE_GA100),
        [ PMC_BOOT_42_OFFSET ] "i"(LW_PMC_BOOT_42),
        [ PMC_BOOT_42_CHIP_ID_SHIFT ] "i"(REF_SHIFT64(LW_PMC_BOOT_42_CHIP_ID)),
        [ PMC_BOOT_42_CHIP_ID_MASK ] "i"(REF_MASK64(LW_PMC_BOOT_42_CHIP_ID)),
        [ PMC_BOOT_42_CHIP_ID_TU102 ] "i"(LW_PMC_BOOT_42_CHIP_ID_TU102),
        [ PMC_BOOT_42_CHIP_ID_TU104 ] "i"(LW_PMC_BOOT_42_CHIP_ID_TU104),
        [ PMC_BOOT_42_CHIP_ID_TU105 ] "i"(LW_PMC_BOOT_42_CHIP_ID_TU105),
        [ PMC_BOOT_42_CHIP_ID_TU106 ] "i"(LW_PMC_BOOT_42_CHIP_ID_TU106),
        [ PMC_BOOT_42_CHIP_ID_TU107 ] "i"(LW_PMC_BOOT_42_CHIP_ID_TU107),
        [ PMC_BOOT_42_CHIP_ID_TU116 ] "i"(LW_PMC_BOOT_42_CHIP_ID_TU116),
        [ PMC_BOOT_42_CHIP_ID_TU117 ] "i"(LW_PMC_BOOT_42_CHIP_ID_TU117),
        [ PMC_BOOT_42_CHIP_ID_GA100 ] "i"(LW_PMC_BOOT_42_CHIP_ID_GA100),
        [ PMC_BOOT_42_CHIP_ID_GA101 ] "i"(LW_PMC_BOOT_42_CHIP_ID_GA101),
        [ PMC_BOOT_42_CHIP_ID_GA102 ] "i"(LW_PMC_BOOT_42_CHIP_ID_GA102),
        [ PMC_BOOT_42_CHIP_ID_GA102F ] "i"(LW_PMC_BOOT_42_CHIP_ID_GA102F),
        [ PMC_BOOT_42_CHIP_ID_GA103 ] "i"(LW_PMC_BOOT_42_CHIP_ID_GA103),
        [ PMC_BOOT_42_CHIP_ID_GA104 ] "i"(LW_PMC_BOOT_42_CHIP_ID_GA104),
        [ PMC_BOOT_42_CHIP_ID_GA106 ] "i"(LW_PMC_BOOT_42_CHIP_ID_GA106),
        [ PMC_BOOT_42_CHIP_ID_GA107 ] "i"(LW_PMC_BOOT_42_CHIP_ID_GA107),
        [ PMC_BOOT_42_CHIP_ID_GH100 ] "i"(LW_PMC_BOOT_42_CHIP_ID_GH100));
}
