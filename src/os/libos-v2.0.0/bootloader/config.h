/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef CONFIG_H_
#define CONFIG_H_

#include "lwriscv-version.h"

// Feature toggles are only applicable if a target platform is specified
#ifdef LWRISCV_VERSION
    // Stack Smashing Protection
    #define LIBOS_BOOTLOADER_CONFIG_SSP             (LWRISCV_VERSION >= LWRISCV_2_0)

    // Signed bootstrapping support
    #define LIBOS_BOOTLOADER_CONFIG_SIGNED_BOOT     (LWRISCV_VERSION == LWRISCV_2_0)

    // Separation Kernel
    #define LIBOS_BOOTLOADER_CONFIG_LWRISCV_SEPKERN (LWRISCV_VERSION >= LWRISCV_2_0)

    // Supervisor Mode
    #define LIBOS_BOOTLOADER_CONFIG_RISCV_S_MODE    (LWRISCV_VERSION >= LWRISCV_2_0)

    #if defined(LIBOS_EXOKERNEL_GH100)
        #define LIBOS_BOOTLOADER_CONFIG_USES_BOOTER    0
    #else
        #define LIBOS_BOOTLOADER_CONFIG_USES_BOOTER    0 // TODO: Change to 1 when BOOTER is enabled.
    #endif

    #define LIBOS_BOOTLOADER_CONFIG_USE_INIT_ARGS      !LIBOS_BOOTLOADER_CONFIG_USES_BOOTER
    #define LIBOS_BOOTLOADER_CONFIG_COPY_UCODE         !LIBOS_BOOTLOADER_CONFIG_USES_BOOTER

    // Separation Kernel Accounting
    #if LIBOS_BOOTLOADER_CONFIG_LWRISCV_SEPKERN
        #define LIBOS_BOOTLOADER_CONFIG_STACK_SIZE     0x1400
        #define LIBOS_BOOTLOADER_CONFIG_IMEM_OFFSET    0x1000
        #define LIBOS_BOOTLOADER_CONFIG_IMEM_SIZE      (0x16000 - LIBOS_BOOTLOADER_CONFIG_IMEM_OFFSET)
        #define LIBOS_BOOTLOADER_CONFIG_DMEM_OFFSET    0x1000
        #define LIBOS_BOOTLOADER_CONFIG_DMEM_SIZE      (0x10000 - LIBOS_BOOTLOADER_CONFIG_DMEM_OFFSET)
    #else
        #define LIBOS_BOOTLOADER_CONFIG_STACK_SIZE     0x400
        #define LIBOS_BOOTLOADER_CONFIG_IMEM_OFFSET    0
        #define LIBOS_BOOTLOADER_CONFIG_IMEM_SIZE      0x10000
        #define LIBOS_BOOTLOADER_CONFIG_DMEM_OFFSET    0
        #define LIBOS_BOOTLOADER_CONFIG_DMEM_SIZE      0x10000
    #endif
#endif // LWRISCV_VERSION

#endif
