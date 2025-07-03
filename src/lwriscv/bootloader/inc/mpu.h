/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef BOOTLOADER_MPU_H
#define BOOTLOADER_MPU_H

/*!
 * @file   mpu.h
 * @brief  LW-MPU related stuff.
 */

#include <lwmisc.h>
#include "riscv_csr.h"
#include "lwriscv-mpu.h"

// MPU granularity is 12-bit on 1.0, 10-bit on 1.1 and 2.0(?)
#define MPU_GRANULARITY (1 << DRF_BASE(LW_RISCV_CSR_SMPUPA_BASE))
#define MPU_GRANULARITY_MASK (MPU_GRANULARITY - 1)

#define MPU_CALC_BASE(BASE, SIZE) LW_ALIGN_DOWN(BASE, MPU_GRANULARITY)
#define MPU_CALC_SIZE(BASE, SIZE) LW_ALIGN_UP((SIZE) + ((BASE) & MPU_GRANULARITY_MASK), \
                                              MPU_GRANULARITY)

// Total number of MPU regions lwrrently in LW-MPU.
#define MPUIDX_COUNT LW_RISCV_CSR_MPU_ENTRY_COUNT
// First MPU region reserved by the bootloader.
#define MPUIDX_BOOTLOADER_RESERVED (MPUIDX_COUNT-3)

// Identity mapping for image carveout.
#define MPUIDX_IDENTITY_CARVEOUT (MPUIDX_BOOTLOADER_RESERVED)

//Remaining IDENTITY mapping
#define MPUIDX_IDENTITY_ALL (MPUIDX_BOOTLOADER_RESERVED+1)

// Holds boot arguments.
#define MPUIDX_BOOTLOADER_BOOTARGS (MPUIDX_BOOTLOADER_RESERVED - 1)


typedef struct LW_RISCV_MPU_REGION
{
  LwU64 vaBase;
  LwU64 paBase;
  LwU64 range;
  LwU64 attribute;
} LW_RISCV_MPU_REGION,
*PLW_RISCV_MPU_REGION;

typedef enum LW_RISCV_MPU_SETTING_TYPE
{
  // Mbare mode (LW-MPU disabled).
  LW_RISCV_MPU_SETTING_TYPE_MBARE = 0,
  // Manual configuration.
  LW_RISCV_MPU_SETTING_TYPE_MANUAL = 1,
  // Automatic configuration (with possible additional regions).
  LW_RISCV_MPU_SETTING_TYPE_AUTOMATIC = 2,
} LW_RISCV_MPU_SETTING_TYPE;

// Directly inserted into .LWPU.mpu_info
typedef struct LW_RISCV_MPU_INFO
{
  //
  // Use values from LW_RISCV_MPU_SETTING_TYPE enumeration.
  //
  // This defaults to _MBARE.
  //
  LwU32 mpuSettingType;

  //
  // If this field is non-zero, it is the number of mpuRegion entries that follow. This contains
  // the total number of regions for _MANUAL, or the number of additional regions for _AUTOMATIC.
  // For _MBARE, this field must be zero.
  // Examples:
  //   0x00000000 = No manual regions.
  //   0x00000001 = One manual region defined in this section.
  // The total number of regions must not be greater than 0x3E (region 0x3E and 0x3F are used by
  // the bootloader to hold itself).
  //
  // This defaults to 0.
  //
  LwU32 mpuRegionCount;

  // MPU regions follow.
  LW_RISCV_MPU_REGION mpuRegion[];
} LW_RISCV_MPU_INFO,
*PLW_RISCV_MPU_INFO;

/*!
 * @brief Enables LW-MPU, and transfers into the new loader VA.
 *
 * @param   loaderBaseOffset  Offset from bootloader PA to VA.
 * @param   stackBaseOffset   Offset from stack PA to VA.
 */
void mpuEnable(LwSPtr loaderBaseOffset, LwSPtr stackBaseOffset);
void mpuClear(void);
/*!
 * @brief Disables LW-MPU, and transfers into the old loader PA.
 *
 * @param   loaderBaseOffset  Offset from bootloader PA to VA.
 * @param   stackBaseOffset   Offset from stack PA to VA.
 */
void mpuDisable(LwSPtr loaderBaseOffset, LwSPtr stackBaseOffset);

#endif
