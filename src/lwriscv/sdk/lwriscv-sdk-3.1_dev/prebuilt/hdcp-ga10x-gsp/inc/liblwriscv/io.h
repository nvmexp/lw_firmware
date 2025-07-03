/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef LIBLWRISCV_IO_H
#define LIBLWRISCV_IO_H

#include <stdint.h>

#if LWRISCV_HAS_PRI
#include <liblwriscv/io_pri.h>
#endif

#if LWRISCV_HAS_CSB_MMIO
#include <liblwriscv/io_csb.h>
#elif LWRISCV_HAS_PRI && LWRISCV_HAS_CSB_OVER_PRI
uint32_t csbRead(uint32_t addr);
void csbWrite(uint32_t addr, uint32_t val);
#endif

#include <liblwriscv/io_local.h>

#if LWRISCV_HAS_DIO_SE || LWRISCV_HAS_DIO_SNIC || LWRISCV_HAS_DIO_FBHUB
#include <liblwriscv/io_dio.h>
#endif

#endif // LIBLWRISCV_IO_H
