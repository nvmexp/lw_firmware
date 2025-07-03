/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef ACRUTILS_RISCV_H
#define ACRUTILS_RISCV_H


#include "lwuproc.h"
#include <liblwriscv/g_lwriscv_config.h>
#include <liblwriscv/io_local.h>
#include <liblwriscv/io_csb.h>
#include <liblwriscv/io_pri.h>

#define ACR_PRGNLCL_REG_RD32(addr)                  localRead(addr)
#define ACR_PRGNLCL_REG_WR32(addr, data)            localWrite(addr, data)

#define ACR_BAR0_REG_RD32(addr)                     acrBar0RegRd32Riscv(addr)
#define ACR_BAR0_REG_WR32(addr, data)               acrBar0RegWr32Riscv(addr, data)
#define ACR_BAR0_REG_WR32_NON_BLOCKING(addr, data)  acrBar0RegWr32Riscv(addr, data)


GCC_ATTRIB_ALWAYSINLINE()
static inline
LwU32
acrBar0RegRd32Riscv(LwU32 addr)
{
    return priRead(addr);
}

GCC_ATTRIB_ALWAYSINLINE()
static inline
void
acrBar0RegWr32Riscv(LwU32 addr, LwU32 data)
{
    priWrite(addr, data);
}

#endif // ACRUTILS_RISCV_H

