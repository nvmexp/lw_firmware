/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef ACRUTILS_H
#define ACRUTILS_H

#include <lwtypes.h>
#include <lwmisc.h>

#ifndef UPROC_RISCV
/*!
 * @brief   ACR BAR0/CSB register access macros.
 *
 * @note    Replaced all CSB non blocking call with blocking calls.
 */
#define ACR_CSB_REG_RD32(addr)                      ACR_CSB_REG_RD32_STALL(addr)
#define ACR_CSB_REG_WR32(addr, data)                ACR_CSB_REG_WR32_STALL(addr, data)
#define ACR_CSB_REG_RD32_STALL(addr)                falc_ldxb_i_with_halt(addr)
#define ACR_CSB_REG_WR32_STALL(addr, data)          falc_stxb_i_with_halt(addr, data)

//!< Use BAR0 primary to write registers if DMEM Aperture is not present.
#if PMUCFG_FEATURE_ENABLED(PMU_ACR_DMEM_APERT_ENABLED)
# define ACR_BAR0_REG_RD32(addr)                  acrBar0RegReadDmemApert_HAL(&Acr, addr)
# define ACR_BAR0_REG_WR32(addr, data)            acrBar0RegWriteDmemApert_HAL(&Acr, addr, data)
#else // PMUCFG_FEATURE_ENABLED(PMU_ACR_DMEM_APERT_ENABLED)
# define ACR_BAR0_REG_RD32(addr)                  acrBar0RegRead_HAL(&Acr, addr)
# define ACR_BAR0_REG_WR32(addr, data)            acrBar0RegWrite_HAL(&Acr, addr, data)
#endif // PMUCFG_FEATURE_ENABLED(PMU_ACR_DMEM_APERT_ENABLED)

#define ACR_REG_RD32(bus,addr)                      ACR_##bus##_REG_RD32(addr)
#define ACR_REG_WR32(bus,addr,data)                 ACR_##bus##_REG_WR32(addr, data)
#define ACR_REG_RD32_STALL(bus,addr)                ACR_##bus##_REG_RD32_STALL(addr)
#define ACR_REG_WR32_STALL(bus,addr,data)           ACR_##bus##_REG_WR32_STALL(addr,data)
#endif // UPROC_RISCV

#endif // ACRUTILS_H
