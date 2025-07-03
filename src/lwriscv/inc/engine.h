/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef ENGINE_H
#define ENGINE_H

/*!
 * @file    engine.h
 * @brief   This file hides header differences between different RISC-V engines.
 */

#include <memmap.h>

// Include RISCV engine headers
#include "riscv_manuals.h"

#include <dev_falcon_v4.h>

#if !defined(SOE_RTOS)
#include <dev_fbif_v4.h>
#endif

#include <dev_riscv_pri.h>


#if USE_CSB
#define ENGINE_BUS(X)           LW_C ## X
#define csbWrite(X, value)      intioWrite(LW_RISCV_AMAP_INTIO_START + (X), value)
#define csbRead(X)              intioRead(LW_RISCV_AMAP_INTIO_START + (X))
#define csbWritePA(X, value)    intioWrite(LW_RISCV_AMAP_INTIO_START + (X), value)
#define csbReadPA(X)            intioRead(LW_RISCV_AMAP_INTIO_START + (X))

#else // USE_CSB
#define ENGINE_BUS(X)           LW_P ## X
#define csbWrite(X, value)      bar0Write(X, value)
#define csbRead(X)              bar0Read(X)
#define csbWritePA(X, value)    bar0WritePA(X, value)
#define csbReadPA(X)            bar0ReadPA(X)

#endif // USE_CSB

// GSP defines
#if defined(GSP_RTOS)
#ifdef RUN_ON_SEC
#define ENGINE_REG(X) ENGINE_BUS(SEC ## X)
#else // RUN_ON_SEC
#define ENGINE_REG(X) ENGINE_BUS(GSP ## X)
#endif // RUN_ON_SEC
// PMU defines
#elif defined(PMU_RTOS)
#define ENGINE_REG(X) ENGINE_BUS(PWR ## X)
// Correct weirdly named registers
#define LW_PPWR_QUEUE_HEAD                        LW_PPWR_PMU_QUEUE_HEAD
#define LW_PPWR_QUEUE_TAIL                        LW_PPWR_PMU_QUEUE_TAIL
#define LW_PPWR_QUEUE_HEAD__SIZE_1                LW_PPWR_PMU_QUEUE_HEAD__SIZE_1
#define LW_PPWR_QUEUE_TAIL__SIZE_1                LW_PPWR_PMU_QUEUE_TAIL__SIZE_1
#define LW_PPWR_SCP_CTL_STAT                      LW_PPWR_PMU_SCP_CTL_STAT
#define LW_PPWR_SCP_CTL_STAT_DEBUG_MODE           LW_PPWR_PMU_SCP_CTL_STAT_DEBUG_MODE
#define LW_PPWR_SCP_CTL_STAT_DEBUG_MODE_DISABLED  LW_PPWR_PMU_SCP_CTL_STAT_DEBUG_MODE_DISABLED

// SEC2 defines
#elif defined(SEC2_RTOS)
#define ENGINE_REG(X) ENGINE_BUS(SEC ## X)
// SOE defines
#elif defined(SOE_RTOS)
#define ENGINE_REG(X) ENGINE_BUS(SOE ## X)
#endif // defined(GSP_RTOS), defined(PMU_RTOS), defined(SEC_RTOS), defined(SOE_RTOS)

// MMINTS-TODO: get this into addendum headers
#define LW_PRGNLCL_FBIF_REGIONCFG_T(i) (3+(4*(i))) : (0+(4*(i)))

/* ------------------------ MMIO access ------------------------------------- */
#ifndef __ASSEMBLER__
#include <lwtypes.h>

#if USE_CBB // CBB access functions

// VG  TODO: implement me - here to cause errors when we try to access bar0 on cheetah

#else // USE_CBB - PRI access functions

// @todo  yitianc: add a third condition where PRI CBB are both unavaialble, where we use DIO
#ifdef LW_RISCV_AMAP_PRIV_START
static LW_FORCEINLINE void
bar0Write(LwU32 priAddr, LwU32 value)
{
    *(volatile LwU32*)(enginePRIV_VA_BASE + priAddr) = value;
}

static LW_FORCEINLINE LwU32
bar0Read(LwU32 priAddr)
{
    return *(volatile LwU32*)(enginePRIV_VA_BASE + priAddr);
}

static LW_FORCEINLINE void
bar0WritePA(LwU32 priAddr, LwU32 value)
{
    *(volatile LwU32*)(LW_RISCV_AMAP_PRIV_START + priAddr) = value;
}

static LW_FORCEINLINE LwU32
bar0ReadPA(LwU32 priAddr)
{
    return *(volatile LwU32*)(LW_RISCV_AMAP_PRIV_START + priAddr);
}
#else // LW_RISCV_AMAP_PRIV_START

#include <flcnretval.h>
#include <address_map_new.h>

extern FLCN_STATUS dioReadReg(LwU32 address, LwU32* pData);
extern FLCN_STATUS dioWriteReg(LwU32 address, LwU32 data);

static LW_FORCEINLINE void
bar0Write(LwU32 priAddr, LwU32 value)
{
    dioWriteReg(LW_ADDRESS_MAP_GPU_BASE + priAddr, value);
}

static LW_FORCEINLINE LwU32
bar0Read(LwU32 priAddr)
{
    LwU32 value = 0;
    dioReadReg(LW_ADDRESS_MAP_GPU_BASE + priAddr, &value);
    return value;
}

#define bar0WritePA(priAddr, value) bar0Write(priAddr, value)

#define bar0ReadPA(priAddr) bar0Read(priAddr)

#endif // LW_RISCV_AMAP_PRIV_START

#endif // USE_CBB

static LW_FORCEINLINE void
intioWrite(LwU32 addr, LwU32 value)
{
    *(volatile LwU32*)((LwUPtr) addr) = value;
}

static LW_FORCEINLINE LwU32
intioRead(LwU32 addr)
{
    return *(volatile LwU32*)((LwUPtr) addr);
}
#endif // __ASSEMBLER__

#endif // ENGINE_H
