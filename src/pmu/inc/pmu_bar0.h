/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef PMU_BAR0_H
#define PMU_BAR0_H
#ifndef UTF_REG_MOCKING

/*!
 * @file pmu_bar0.h
 */

#include "regmacros.h"
#if defined(UPROC_RISCV)
    #include "engine.h"
    #include "syslib/syslib.h"
#endif
#include "hwproject.h"

//
// EMEM aperture macros for Falcon4+ falcon2ext BAR0 access.
//
// Diagram from the manuals:
//
// 0x1FFFFFFF +-------------------------------+
//            |    FECS (PRI hub) Blocking    |
//            +-------------------------------+
//            |   FECS (PRI hub) NonBlocking  |
//            +-------------------------------+  EMEM Aperture(256MB)
//            |         HOST Blocking         |
//            +-------------------------------+
//            |           (unused)            |
// 0x10000000 +-------------------------------+
//
// Note that HOST only supports blocking access, so bit 26 is only relevant for
// for PRI hub (FECS) path. However, starting from GA10x+, all PRI accesses from
// the PMU will go through the PRI hub (FECS) path even if bit 27 is not set;
// and bit 26 will solely control blocking-ness. Hence, for code which requests
// the host path (referred to as BAR0 path in the PMU ucode), we will explicitly
// always set bit 26 to indicate a blocking access. That way, old code that
// specifies BAR0 as the path to PRI will get same behavior with respect to the
// blocking-ness on GA10x+.
//
#define USE_EMEM_APERTURE(addr)          ((addr) | BIT32(28))
#define USE_FECS_BLOCKING(addr)          (USE_FECS(addr) | USE_BLOCKING(addr))

//
// GPU BAR0/Priv Access Macros {BAR0_*}
//
#ifdef UPROC_RISCV
#define USE_FECS(addr)             ((addr))
#define USE_BLOCKING(addr)         ((addr))
#ifdef PMU_BAREMODE_PARTITION
//
// Redefine for the case of a bare-mode RISCV partition. We don't redefine
// nonposted writes for now because sysBar0WriteNonPosted can be redefined
// in one of the partition's .c files.
//
#define BAR0_REG_RD32(addr)        bar0ReadPA(addr)
#define PRI_REG_RD32(addr)         bar0ReadPA(addr)
#define PRI_REG_WR32(addr, data)   bar0WritePA(addr, data)
#else // PMU_BAREMODE_PARTITION
#define BAR0_REG_RD32(addr)        bar0Read(addr)
#define PRI_REG_RD32(addr)         bar0Read(addr)
#define PRI_REG_WR32(addr, data)   bar0Write(addr, data)
#endif // PMU_BAREMODE_PARTITION
#define BAR0_REG_WR32(addr, data)  sysBar0WriteNonPosted(addr, data)
#define FECS_REG_WR32_STALL(addr, data) sysBar0WriteNonPosted(addr, data)
#else
#define USE_FECS(addr)                   ((addr) | BIT32(27))
#define USE_BLOCKING(addr)               ((addr) | BIT32(26))
#define PRI_REG_RD32(addr)                                                     \
    (*(const volatile LwU32 *)USE_EMEM_APERTURE(addr))

#define PRI_REG_WR32(addr, data)                                               \
    do                                                                         \
    {                                                                          \
        *(volatile LwU32 *)USE_EMEM_APERTURE(addr) = (data);                   \
    }                                                                          \
    while (LW_FALSE)

/*!
 * @brief Read a 32-bit register on the BAR0 bus
 * 
 * @param[in]  addr    The address to access
 * 
 * @return The value which was read
 */
#define BAR0_REG_RD32(addr)                                                    \
    PRI_REG_RD32(USE_BLOCKING(addr))

/*!
 * @brief Write a 32-bit register on the BAR0 bus
 * 
 * @param[in]  addr    The address to access
 * @param[in]  data    The value to write
 */
#define BAR0_REG_WR32(addr, data)                                              \
    PRI_REG_WR32(USE_BLOCKING(addr), data)

#define FECS_REG_WR32_STALL(addr, data)                                        \
    PRI_REG_WR32(USE_FECS_BLOCKING(addr), data)

#endif

//
// GPU Config-Space Access through BAR0 Macros {BAR0_CFG_*}
//

/*!
 * @brief Read a 32-bit config-space register on the BAR0 bus
 * 
 * @param[in]  addr    The address to access
 * 
 * @return The value which was read
 */
#define BAR0_CFG_REG_RD32(addr)                                                \
    BAR0_REG_RD32(DEVICE_BASE(LW_PCFG) + (addr))

/*!
 * @brief Write a 32-bit config-space register on the BAR0 bus
 * 
 * @param[in]  addr    The address to access
 * @param[in]  data    The value to write
 */
#define BAR0_CFG_REG_WR32(addr, data)                                          \
    BAR0_REG_WR32(DEVICE_BASE(LW_PCFG) + (addr), (data))

/*!
 * @brief Read a 32-bit register from XTL on the BAR0 bus
 *
 * @param[in]  addr    The address to access
 *
 * @return The value which was read
 */
#define XTL_REG_RD32(addr) \
    BAR0_REG_RD32(DEVICE_BASE(LW_XTL) + (addr))

/*!
 * @brief Write a 32-bit register to XTL on the BAR0 bus
 *
 * @param[in]  addr    The address to access
 * @param[in]  data    The value to write
 */
#define XTL_REG_WR32(addr, value) \
    BAR0_REG_WR32(DEVICE_BASE(LW_XTL) + (addr), (value))

/*!
 * @brief Read a 32-bit register from XPL
 *
 * @param[in] path    FECS or BAR0
 * @param[in] readReg The address to access
 *
 * @return The value which was read
 */
#define XPL_REG_RD32(path, readReg) \
    REG_RD32(path, LW_XPL_BASE_ADDRESS + readReg)

/*!
 * @brief Write a 32-bit register to XPL
 *
 * @param[in] path     FECS or BAR0
 * @param[in] writeReg The address to access
 * @param[in] value    The value to write
 */
#define XPL_REG_WR32(path, writeReg, value) \
    REG_WR32(path, LW_XPL_BASE_ADDRESS + writeReg, value)

//
// GPU BAR0/Priv through FECS Access Macros {FECS_*}
//

//
// For speed, FECS can be used to access certain BAR0 registers. This path is
// only available on GF11x+ and, again, only for certain registers as FECS does
// not have access to every register-range in the GPU. Only Host has absolute
// visibility. The request itself is made just like any other BAR0 access on
// GF11x+. The only difference is that in addition to setting Bit28 (to direct
// the request to ext2priv), Bit27 must also be set to further direct it to
// FECS instead of Host.
//
#define FECS_REG_RD32(addr)                                                    \
    PRI_REG_RD32(USE_FECS(addr))

#define FECS_REG_WR32(addr, data)                                              \
    PRI_REG_WR32(USE_FECS(addr), data)

//
// GPU Config-Space Access through FECS Macros {FECS_CFG_*}
//

#define FECS_CFG_REG_RD32(addr)                                                \
    FECS_REG_RD32(DEVICE_BASE(LW_PCFG) + (addr))

#define FECS_CFG_REG_WR32(addr, data)                                          \
    FECS_REG_WR32(DEVICE_BASE(LW_PCFG) + (addr), (data))

#define FECS_CFG_REG_WR32_STALL(addr, data)                                    \
    FECS_REG_WR32_STALL(DEVICE_BASE(LW_PCFG) + (addr), (data))

#endif // UTF_REG_MOCKING
#endif // PMU_BAR0_H
