/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_OBJFIFO_H
#define PMU_OBJFIFO_H

/*!
 * @file pmu_objfifo.h
 */

/* ------------------------ System includes -------------------------------- */
#include "flcntypes.h"
#include "pmu_oslayer.h"
#include "main.h"
/* ------------------------ Forward Declartion ----------------------------- */
typedef struct PMU_ENGINE_LOOKUP_TABLE PMU_ENGINE_LOOKUP_TABLE;

#define PMU_ENGINE_MISSING       (0xFF)
#define PMU_PBDMA_MASK_ILWALID   (0)
#define PMU_RUNLIST_MISSING      (0xFF)

/* ------------------------ Application includes --------------------------- */
#include "config/g_fifo_hal.h"

/* ------------------------ Types definitions ------------------------------ */
#define FIFO_PREEMPT_PENDING_TIMEOUT_US     (1000)
#define FIFO_MAX_PREEMPT_RETRY_COUNT        (5)
#define FIFO_PREEMPT_WAIT_TIMEOUT_NS        (50000)

// FIFO mutex
#define FIFO_MUTEX_ACQUIRE_TIMEOUT_NS       (10000) // 10us
#define fifoMutexAcquire(tk)        mutexAcquire(LW_MUTEX_ID_FIFO,             \
                                            FIFO_MUTEX_ACQUIRE_TIMEOUT_NS, tk)

#define fifoMutexRelease(tk)        mutexRelease(LW_MUTEX_ID_FIFO, tk)
//
// PMU Engine Index - HW independent engine indexes.
//
enum
{
    PMU_ENGINE_GR  = 0,
    PMU_ENGINE_GR0 = PMU_ENGINE_GR,
    PMU_ENGINE_VP,
    PMU_ENGINE_PPP,
    PMU_ENGINE_BSP,
    PMU_ENGINE_DEC0 = PMU_ENGINE_BSP,
    PMU_ENGINE_CE0,
    PMU_ENGINE_CE1,
    PMU_ENGINE_CE2,
    PMU_ENGINE_ENC0,
    PMU_ENGINE_ENC1,
    PMU_ENGINE_SEC2,
    PMU_ENGINE_CE3,
    PMU_ENGINE_CE4,
    PMU_ENGINE_CE5,
    PMU_ENGINE_ENC2,
    PMU_ENGINE_DEC1,
    PMU_ENGINE_DEC2,
    PMU_ENGINE_GR1,
// Adding this check to restrict DMEM usages on pre Ampere chips
#if PMUCFG_FEATURE_ENABLED(PMU_HOST_ENGINE_EXPANSION)
    PMU_ENGINE_DEC3,
    PMU_ENGINE_JPG0,
    PMU_ENGINE_JPG1,
    PMU_ENGINE_JPG2,
    PMU_ENGINE_JPG3,
    PMU_ENGINE_OFA0,
#endif
    PMU_ENGINE_ILWALID,
};

/*!
* @brief Hepler macros define start and count for PMU_ENGINE_xyz Ids
*/
#define PMU_ENGINE_ID__START    PMU_ENGINE_GR
#define PMU_ENGINE_ID__COUNT    PMU_ENGINE_ILWALID

#define GET_FIFO_ENG(x)                (Fifo.pmuEngTbl[x].fifoEngId)
#define GET_ENG_RUNLIST(x)             (Fifo.pmuEngTbl[x].runlistId)
#if PMUCFG_FEATURE_ENABLED(PMU_HOST_ENGINE_EXPANSION)
#define GET_ENG_RUNLIST_PRI_BASE(x)    (Fifo.pmuEngTbl[x].runlistPriBase)
#define GET_ENG_RUNLIST_ENG_ID(x)      (Fifo.pmuEngTbl[x].rlEngId)
#else
#define GET_ENG_INTR(x)                (Fifo.pmuEngTbl[x].intrId)
#define GET_ENG_PBDMA_MASK(x)          (Fifo.pmuEngTbl[x].pbdmaIdMask)
#endif

#define FIFO_CHECK_ENGINE(pmuEng, fifoEng)                                    \
    (GET_FIFO_ENG(PMU_ENGINE_##pmuEng) == fifoEng)

#define FIFO_IS_ENGINE_PRESENT(pmuEng)                                        \
    (GET_FIFO_ENG(PMU_ENGINE_##pmuEng) != PMU_ENGINE_MISSING)

/* ------------------------ External definitions --------------------------- */
/*!
 * Mapping of PMU_ENGINE ID to FIFO_ENGINE_ID, PBDMA_ID Mask and RUNLIST_ID
 */
typedef struct
{
#if PMUCFG_FEATURE_ENABLED(PMU_HOST_ENGINE_EXPANSION)
    LwU32 runlistPriBase;
    LwU16 rlEngId;
#else
    LwU16 pbdmaIdMask;
    LwU8  intrId;
#endif
    LwU8  fifoEngId;
    LwU8  runlistId;
} PMU_ENGINE_TABLE;

struct PMU_ENGINE_LOOKUP_TABLE
{
    LwU8  typeEnum;
    LwU8  pmuEngineId;
    LwU8  instId;
};

/*!
 * FIFO object Definition
 */
typedef struct
{
    PMU_ENGINE_TABLE  pmuEngTbl[PMU_ENGINE_ILWALID];
} OBJFIFO;

extern OBJFIFO Fifo;

/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
LwBool fifoIsGrCeEngine(LwU8 ceEngId)
       GCC_ATTRIB_SECTION("imem_lpwrLoadin", "fifoIsGrCeEngine");

#endif // PMU_OBJFIFO_H
