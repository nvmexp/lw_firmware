
/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   interface.h
 * @brief  SOE HS bootloader platform-specific interface.
*/

#ifndef BOOT_PLATFORM_INTERFACE_H
#define BOOT_PLATFORM_INTERFACE_H

#include "lwuproc.h"

/**
 * @brief Initialize BAR0 on this platform
 * 
 */
void soeBar0InitTmout_PlatformHS(void)
    GCC_ATTRIB_SECTION("imem_hs", "soeBar0InitTmout_PlatformHS")
    GCC_ATTRIB_NOINLINE()
    GCC_ATTRIB_USED();

/**
 * @brief Verify CHIP ID for this platform.
 *        eg. On LR10, make sure we are running on LR10
 * 
 * @return LwBool True if check succeeds/passes, false on failure.
 */
LwBool soeVerifyChipId_PlatformHS(void)
    GCC_ATTRIB_SECTION("imem_hs", "soeVerifyChipId_PlatformHS")
    GCC_ATTRIB_NOINLINE()
    GCC_ATTRIB_USED();

/**
 * @brief Read the SW revocation revision (Fuse) for this HS
 * @return - SW Fuse revision for this HS binary
 */
LwU32 soeGetRevocationUcodeVersionSW_PlatformHS(void)
    GCC_ATTRIB_SECTION("imem_hs", "soeGetRevocationUcodeVersionSW_PlatformHS")
    GCC_ATTRIB_NOINLINE()
    GCC_ATTRIB_USED();

/**
 * @brief Read a 32-bit value from BAR0.
 * 
 * @param addr      Address to read from
 * @return LwU32    Value read from BAR0
 */
LwU32 soeBar0RegRd32_PlatformHS(LwU32 addr)
    GCC_ATTRIB_SECTION("imem_hs", "soeBar0RegRd32_PlatformHS")
    GCC_ATTRIB_NOINLINE()
    GCC_ATTRIB_USED();

void hsInstallExceptionHandler(void)
    GCC_ATTRIB_SECTION("imem_hs", "hsInstallExceptionHandler")
    GCC_ATTRIB_NOINLINE();

#endif // BOOT_PLATFORM_INTERFACE_H
