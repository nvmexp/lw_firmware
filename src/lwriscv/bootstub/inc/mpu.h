/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef BOOTSTUB_MPU_H
#define BOOTSTUB_MPU_H

/*!
 * @file   mpu.h
 * @brief  LW-MPU interfaces.
 */

#include <lwtypes.h>


// Forward declaration (see mpu.c for definition).
typedef struct LW_RISCV_MPU_INFO LW_RISCV_MPU_INFO;


/*!
 * @brief Validates and applies the provided MPU settings.
 *
 * @param[in] pMpuInfo      A pointer to the desired MPU settings.
 * @param[in] pBootstubBase The base address of the bootstub.
 * @param[in] bootstubSize  The total size of the bootstub.
 *
 * @return
 *      LW_TRUE     if successful.
 *      LW_FALSE    if the provided MPU settings are invalid.
 *
 * Validates the contents of pMpuInfo and, if LW-MPU mode is requested,
 * initializes and enables the MPU accordingly. Also creates identity mappings
 * for use by the bootstub as applicable.
 */
LwBool mpuInit(const LW_RISCV_MPU_INFO *pMpuInfo, const void *pBootstubBase,
    LwU64 bootstubSize);

#endif // BOOTSTUB_MPU_H
