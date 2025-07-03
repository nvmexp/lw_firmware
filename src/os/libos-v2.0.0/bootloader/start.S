/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "lw_gsp_riscv_address_map.h"
#include "config.h"

#define STACK_POINTER (LW_RISCV_AMAP_DMEM_START + LIBOS_BOOTLOADER_CONFIG_STACK_SIZE)

    .section .start
    .globl start
start:
    // Set initial stack pointer
    li sp, STACK_POINTER
    j main
