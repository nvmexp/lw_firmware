/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    falcon_intrinsics_sim.c
 *
 * This file simulates subset of falcon instructions via RISCV equivalent codes.
 * So existing falcon ucode can be easier ported to RISCV with minimal change.
 */
#if defined(UPROC_RISCV) && !USE_CSB


#include "engine.h"
#include "bar0_defs.h"
#include "falcon.h"
#include "sections.h"
#include "memops.h"

#ifdef PMU_RTOS
#include <dev_pwr_csb.h>
#include <dev_therm.h>
#include "lwctassert.h"
#endif

/*!
 * Number of bit needs to be shifted to colwert CSB offset to PRIV offset.
 * Note that CSB offset and PRIV offset is the same in certain falcon engines,
 * but now there is no this kind of RISCV engine, so we always do the shift
 * for RISCV.
 */

/*
 * PMU is old engine, so there is no offset.
 */
#ifdef PMU_RTOS
#define LW_CSB_OFFS_TO_PRIV_OFFS_BIT 0
#else
#define LW_CSB_OFFS_TO_PRIV_OFFS_BIT 6
#endif

/*
 * On PMU falcon we access THERM using CSB by poking LW_CPWR_THERM. On PMU riscv
 * we use LW_THERM over priv bus. This means we need to map LW_CPWR_THERM into
 * LW_THERM address space. Fortunately for us, PRIV_BASE of THERM is same as
 * CSB offset of THERM so we don't need to do:
 * PRIV_ADDR = CSB_ADDR - CSB_BASE + PRIV_BASE
 */
#ifdef PMU_RTOS
ct_assert(DEVICE_EXTENT(LW_CPWR_THERM) == DEVICE_EXTENT(LW_THERM));
ct_assert(DEVICE_BASE(LW_CPWR_THERM) == DEVICE_BASE(LW_THERM));
#endif

/*!
 * falc_stx is used as CSB write accessor on Falcon. But Only PRIV access is
 * supported on RISCV, so we need to colwert CSB offset to PRIV offset then do
 * a PRIV write.
 */
sysSYSLIB_CODE void riscv_stx( unsigned int *cfgAdr, unsigned int val )
{
#ifdef PMU_RTOS
    if (((LwUPtr)cfgAdr <= DEVICE_EXTENT(LW_CPWR_THERM)) &&
        ((LwUPtr) cfgAdr >= DEVICE_BASE(LW_CPWR_THERM)))
    {
        bar0Write((unsigned long)cfgAdr, val);
    }
    else
#endif
    {
        bar0Write((ENGINE_PRIV_BASE +
                     ((unsigned long)cfgAdr >> LW_CSB_OFFS_TO_PRIV_OFFS_BIT)),
                     val);
    }
}

sysSYSLIB_CODE void riscv_stx_i( unsigned int *cfgAdr, const unsigned int index_imm, unsigned int val )
{
#ifdef PMU_RTOS
    if (((LwUPtr)cfgAdr <= DEVICE_EXTENT(LW_CPWR_THERM)) &&
        ((LwUPtr) cfgAdr >= DEVICE_BASE(LW_CPWR_THERM)))
    {
        bar0Write((unsigned long)cfgAdr + (4 * index_imm), val);
    }
    else
#endif
    {
        bar0Write((ENGINE_PRIV_BASE + (4 * index_imm) +
                     ((unsigned long)cfgAdr >> LW_CSB_OFFS_TO_PRIV_OFFS_BIT)),
                     val);
    }
}

/*!
 * stxb is the blocking-call version of stx, so we use FENCE.IO to ensure the
 * PRIV request is done before return.
 */
sysSYSLIB_CODE void riscv_stxb_i( unsigned int *cfgAdr, const unsigned int index_imm, unsigned int val )
{
    riscv_stx_i(cfgAdr, index_imm, val);
    /* Fence.IO to ensure all PRIV requests have been done */
    SYS_FLUSH_IO();
}

/*!
 * falc_ldx is used as CSB read accessor on Falcon. But Only PRIV access is
 * supported on RISCV, so we need to colwert CSB offset to PRIV offset then do
 * a PRIV read.
 */
sysSYSLIB_CODE unsigned int riscv_ldx( unsigned int *cfgAdr, unsigned int index )
{
#ifdef PMU_RTOS
    if (((LwUPtr)cfgAdr <= DEVICE_EXTENT(LW_CPWR_THERM)) &&
        ((LwUPtr) cfgAdr >= DEVICE_BASE(LW_CPWR_THERM)))
    {
        return bar0Read((unsigned long)cfgAdr);
    }
    else
#endif
    {
        return bar0Read((ENGINE_PRIV_BASE +
                           ((unsigned long)cfgAdr >> LW_CSB_OFFS_TO_PRIV_OFFS_BIT)));
    }
}

sysSYSLIB_CODE unsigned int riscv_ldx_i( unsigned int *cfgAdr, const unsigned int index_imm )
{
#ifdef PMU_RTOS
    if (((LwUPtr)cfgAdr <= DEVICE_EXTENT(LW_CPWR_THERM)) &&
        ((LwUPtr) cfgAdr >= DEVICE_BASE(LW_CPWR_THERM)))
    {
        return bar0Read((unsigned long)cfgAdr + (4 * index_imm));
    }
    else
#endif
    {
        return bar0Read(ENGINE_PRIV_BASE + (4 * index_imm) +
                           ((unsigned long)cfgAdr >> LW_CSB_OFFS_TO_PRIV_OFFS_BIT));
    }
}

/*!
 * ldxb is the blocking-call version of ldx, so we use FENCE.IO to ensure the
 * PRIV request is done before return.
 */
sysSYSLIB_CODE unsigned int riscv_ldxb_i( unsigned int *cfgAdr, const unsigned int index_imm )
{
    unsigned int result = riscv_ldx_i(cfgAdr, index_imm);
    /* SYS_FLUSH.IO to ensure all PRIV requests have been done */
    SYS_FLUSH_IO();
    return (result);
}

#endif
