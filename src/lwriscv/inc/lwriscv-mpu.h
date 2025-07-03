/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LWRISCV_MPU_H
#define LWRISCV_MPU_H

#include "riscv_csr.h"

//
// Macros to access MPU fields
//

#define LW_RISCV_CSR_MPU_ENTRY_COUNT        (1 << DRF_SIZE(LW_RISCV_CSR_SMPUIDX_INDEX))
#define LW_RISCV_CSR_MPU_ENTRY_MAX          (LW_RISCV_CSR_MPU_ENTRY_COUNT - 1)

// 4 entries at the end of the MPU are reserved for the bootloader
// MMINTS-TODO: use this value in bootloader's mpu.h
#define LW_RISCV_MPU_BOOTLOADER_RSVD        (4)

// Generic mask for addresses - VA, PA, RANGE
#define LW_RISCV_CSR_MPU_PAGE_SIZE          (1 << DRF_SHIFT64(LW_RISCV_CSR_SMPUPA_BASE))
#define LW_RISCV_CSR_MPU_PAGE_MASK          (LW_RISCV_CSR_MPU_PAGE_SIZE - 1)
#define LW_RISCV_CSR_MPU_ADDRESS_MASK       DRF_SHIFTMASK64(LW_RISCV_CSR_SMPUPA_BASE)

#endif // LWRISCV_MPU_H
