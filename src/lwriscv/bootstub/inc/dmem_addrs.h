/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef BOOTSTUB_DMEM_ADDRS_H
#define BOOTSTUB_DMEM_ADDRS_H

#include "riscv_manuals.h"

/*!
 * @file   dmem_addrs.h
 * @brief  Addresses of various things in DMEM.
 */

/* Long story short: RISCV64 does not give sufficient support to 64-bit addressing. Additionally,
 * GCC wants any sort of inter-section interaction to go through relocation or hard-coded
 * addresses. The former can't be supported, and the latter prevents loading the bootstub into
 * arbitrary addresses.
 *
 * So, for the minislwle amount of data that we put in DMEM, we're "hard coding" the addresses.
 *
 * To avoid collisions with the application's own data, we use the carveout reserved at the end
 * of DMEM by the application's build process. The structure is:
 *
 *  ____________
 * |    DMEM    |
 * |____________|<-- (DMEM_BASE + DMEM_SIZE) - DMEM_END_CARVEOUT_SIZE
 * |   Unused   |
 * |____________|<-- (DMEM_BASE + DMEM_SIZE) - DMESG_BUFFER_SIZE - RETURN_FLAG_SIZE - STACK_SIZE - STACK_CANARY_SIZE [DMEM_CONTENTS_BASE]
 * |   Canary   |
 * |____________|<-- (DMEM_BASE + DMEM_SIZE) - DMESG_BUFFER_SIZE - RETURN_FLAG_SIZE - STACK_SIZE
 * |   Stack    |
 * |____________|<-- (DMEM_BASE + DMEM_SIZE) - DMESG_BUFFER_SIZE - RETURN_FLAG_SIZE
 * |  Ret Flag  |
 * |____________|<-- (DMEM_BASE + DMEM_SIZE) - DMESG_BUFFER_SIZE
 * |  DebugBuf  |
 * |____________|<-- (DMEM_BASE + DMEM_SIZE) [DMEM_CONTENTS_BASE + DMEM_CONTENTS_SIZE]
 */

///////////////////////////////////////////////////////////////////////////////
// Sizes

//
// DMEM_SIZE defined in bs-profiles.mk.
//

//
// DMEM_END_CARVEOUT_SIZE defined by application.
//

#ifdef IS_SSP_ENABLED
// sizeof(void*)
#define STACK_CANARY_SIZE (8)
#else // IS_SSP_ENABLED
#define STACK_CANARY_SIZE (0)
#endif  // IS_SSP_ENABLED

//
// STACK_SIZE defined in bs-profiles.mk.
//

//
// Sixteen bytes to ensure stack is aligned correctly.
// Flag is stored in first eight bytes.
//
#define RETURN_FLAG_SIZE 16

//
// DMESG_BUFFER_SIZE defined by application.
//

#define DMEM_CONTENTS_SIZE (STACK_CANARY_SIZE + STACK_SIZE + \
    RETURN_FLAG_SIZE + DMESG_BUFFER_SIZE)

///////////////////////////////////////////////////////////////////////////////
// Bases

//
// DMEM_BASE defined in bs-profiles.mk.
//

#define DMEM_CONTENTS_BASE (DMEM_BASE + DMEM_SIZE - DMEM_CONTENTS_SIZE)

#define DMEM_STACK_CANARY_BASE    (DMEM_CONTENTS_BASE)
#define DMEM_STACK_BASE           (DMEM_STACK_CANARY_BASE    + STACK_CANARY_SIZE)
#define RETURN_FLAG_BASE          (DMEM_STACK_BASE           + STACK_SIZE)
#define DMESG_BUFFER_BASE         (RETURN_FLAG_BASE          + RETURN_FLAG_SIZE)
#define DMEM_CONTENTS_END         (DMESG_BUFFER_BASE         + DMESG_BUFFER_SIZE)

///////////////////////////////////////////////////////////////////////////////
// Checks

//
// We define DMEM_BASE in bs-profiles.mk because the linker cannot use
// LW_RISCV_AMAP_DMEM_START directly (ULL suffix). Here we ensure that the
// values match.
//
#if DMEM_BASE != LW_RISCV_AMAP_DMEM_START
#error "DMEM_BASE doesn't match LW_RISCV_AMAP_DMEM_START!"
#endif // DMEM_BASE != LW_RISCV_AMAP_DMEM_START

// Ensure that the stack canary meets alignment requirements assumed by linker.
#if (DMEM_STACK_CANARY_BASE % STACK_CANARY_SIZE) != 0
#error "Stack canary not aligned to its size (required by linker script)!"
#endif // (DMEM_STACK_CANARY_BASE % STACK_CANARY_SIZE) != 0

// Ensure that the stack meets alignment requirements assumed by bootLoad().
#if ((DMEM_STACK_BASE & 0xF) != 0) || ((STACK_SIZE & 0xF) != 0)
#error "Stack not aligned to 16 bytes (required by bootLoad())!"
#endif // ((DMEM_STACK_BASE & 0xF) != 0) || ((STACK_SIZE & 0xF) != 0)

// Ensure that the return flag meets alignment requirements assumed by start.S.
#if ((RETURN_FLAG_BASE & 0x7) != 0) || ((RETURN_FLAG_SIZE & 0x7) != 0)
#error "Return flag not aligned to 8 bytes (required by start.S)!"
#endif // ((RETURN_FLAG_BASE & 0x7) != 0) || ((RETURN_FLAG_SIZE & 0x7) != 0)

// Ensure that the sizes and bases don't get out of sync with each other.
#if DMEM_CONTENTS_END != (DMEM_BASE + DMEM_SIZE)
#error "Actual and expected sizes of DMEM contents do not match!"
#endif // DMEM_CONTENTS_END != (DMEM_BASE + DMEM_SIZE)

// Ensure that we don't overflow the available carveout.
#if DMEM_CONTENTS_SIZE > DMEM_END_CARVEOUT_SIZE
#error "Bootstub DMEM contents do not fit into the DMEM end-carveout!"
#endif

#endif // BOOTSTUB_DMEM_ADDRS_H
