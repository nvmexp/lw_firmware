/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef DMEM_ADDRS_H
#define DMEM_ADDRS_H

#include "riscv_manuals.h"

/*!
 * @file   dmem_addrs.h
 * @brief  Addresses of various things in DMEM.
 */

/* Long story short: RISCV64 does not give sufficient support to 64-bit addressing. Additionally,
 * GCC wants any sort of inter-section interaction to go through relocation or hard-coded
 * addresses. The former can't be supported, and the latter prevents loading the bootloader into
 * arbitrary addresses.
 *
 * So, for the minislwle amount of data that we put in DMEM, we're "hard coding" the addresses.
 * The structure is:
 *  ____________
 * |    DMEM    |
 * |____________|<-- (DMEM_BASE + DMEM_SIZE) - DMESG_BUFFER_SIZE - DMEM_DMA_BUFFER_SIZE - DMEM_DMA_ZERO_BUFFER_SIZE - STACK_SIZE - STACK_CANARY_SIZE
 * |   Stack    |
 * |  Canary    |
 * |____________|<-- (DMEM_BASE + DMEM_SIZE) - DMESG_BUFFER_SIZE - DMEM_DMA_BUFFER_SIZE - DMEM_DMA_ZERO_BUFFER_SIZE - STACK_SIZE
 * |   Stack    |
 * |____________|<-- (DMEM_BASE + DMEM_SIZE) - DMESG_BUFFER_SIZE - DMEM_DMA_BUFFER_SIZE - DMEM_DMA_ZERO_BUFFER_SIZE
 * |    DMA     |
 * | 0-Buffer   |
 * |____________|<-- (DMEM_BASE + DMEM_SIZE) - DMESG_BUFFER_SIZE - DMEM_DMA_BUFFER_SIZE
 * | DMA Buffer |
 * |____________|<-- (DMEM_BASE + DMEM_SIZE) - DMESG_BUFFER_SIZE
 * |  DmesgBuf  |
 * |____________|<-- (DMEM_BASE + DMEM_SIZE)
 */

#ifdef IS_SSP_ENABLED
// stack canary is void*. Hence hardcoding the size to 8 bytes.
#define STACK_CANARY_SIZE (8)
#else // IS_SSP_ENABLED
#define STACK_CANARY_SIZE (0)
#endif  // IS_SSP_ENABLED

//
// Check to verify tha DMEM_BASE matches LW_RISCV_AMAP_DMEM_START
// when SSP is enabld DMEM_BASE is defined by at the compile command line.
// The value has to be set to equal to LW_RISCV_AMAP_DMEM_START. They are
// being defined in the makefile since, for SSP, linker also uses the
// DMEM_BASE and it is unable to use the LW_RISCV_AMAP_DMEM_START as it is
// given. The check below ensures that DMEM_BASE is always equal in value to
// LW_RISCV_AMAP_DMEM_START. In order for this check to work, both the dmem_addrs.h
// and lw_riscv_address_map.h file should be included. Hence adding this check
// here.
//
#if DMEM_BASE != LW_RISCV_AMAP_DMEM_START
#error "DMEM_BASE doesn't match LW_RISCV_AMAP_DMEM_START!"
#endif // DMEM_BASE != LW_RISCV_AMAP_DMEM_START

#define DMEM_CONTENTS_SIZE (DMESG_BUFFER_SIZE + DMEM_DMA_BUFFER_SIZE +\
                            DMEM_DMA_ZERO_BUFFER_SIZE + STACK_SIZE + STACK_CANARY_SIZE)
#define DMEM_CONTENTS_BASE (DMEM_BASE + DMEM_SIZE - DMEM_CONTENTS_SIZE)

#define DMEM_STACK_CANARY_BASE    (DMEM_CONTENTS_BASE)
#define DMEM_STACK_BASE           (DMEM_STACK_CANARY_BASE    + STACK_CANARY_SIZE)
#define DMEM_DMA_ZERO_BUFFER_BASE (DMEM_STACK_BASE           + STACK_SIZE)
#define DMEM_DMA_BUFFER_BASE      (DMEM_DMA_ZERO_BUFFER_BASE + DMEM_DMA_ZERO_BUFFER_SIZE)
#define DMESG_BUFFER_BASE         (DMEM_DMA_BUFFER_BASE      + DMEM_DMA_BUFFER_SIZE)

#if DMESG_BUFFER_BASE != DMEM_BASE + DMEM_SIZE - DMESG_BUFFER_SIZE
#error "DMESG_BUFFER_BASE doesn't match DMEM_BASE + DMEM_SIZE - DMESG_BUFFER_SIZE!"
#endif // DMESG_BUFFER_BASE != DMEM_BASE + DMEM_SIZE - DMESG_BUFFER_SIZE

#endif
