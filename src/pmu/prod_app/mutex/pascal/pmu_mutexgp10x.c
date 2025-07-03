/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_mutexgp10x.c
 *
 * This file helps generate the required mapping info or any required function
 * which will be needed by the common HW mutex library so that the library knows
 * how to access all the available HW mutex from PMU.
 */


/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "lw_mutex.h"
#include "lwosreg.h"
#include "dev_pwr_csb.h"
#include "dev_pwr_pri_addendum.h"
#include "dev_pmgr.h"
#include "dev_pmgr_addendum.h"
#include "dev_sec_pri.h"
#include "dev_sec_addendum.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objmutex.h"

#include "config/g_mutex_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Global Variables ------------------------------- */
MUTEX_LOG2PHY_MAPPING mutexLog2PhyTable[LW_MUTEX_ID__COUNT];
MUTEX_ENGINE_INFO     mutexEngineInfo[MUTEX_ENGINE_COUNT];

/*!
 * The table mapping the PMU physical mutex ID to logical mtuex ID
 */
static LwU8 pmuMutexPhysicalToLogical_GP10X[] =
{
    LW_PMU_MUTEX_DEFINES
};

/*!
 * The table mapping the PMGR physical mutex ID to logical mtuex ID
 */
static LwU8 pmgrMutexPhysicalToLogical_GP10X[] =
{
    LW_PMGR_MUTEX_DEFINES
};

/*!
 * The table mapping the SEC2 physical mutex ID to logical mtuex ID
 */
static LwU8 sec2MutexPhysicalToLogical_GP10X[] =
{
    LW_SEC2_MUTEX_DEFINES
};

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Function Declarations -------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * Generate required mappings from each logical mutex ID to the corresponding
 * mutex engine and the physical mutex ID which will be used by the mutex lib.
 */
void
mutexEstablishMapping_GP10X(void)
{
    LwU8 i;
    LwU8 j;
    LwBool bFind;
    PMUTEX_ENGINE_INFO pEI;

    for (i = 0; i < LW_MUTEX_ID__COUNT; i++)
    {
        bFind = LW_FALSE;
        for (j = 0; ((j < sizeof(pmuMutexPhysicalToLogical_GP10X)) && !bFind); j++)
        {
            if (pmuMutexPhysicalToLogical_GP10X[j] == i)
            {
                mutexLog2PhyTable[i].engine = MUTEX_ENGINE_PMU;
                mutexLog2PhyTable[i].physId = j;
                bFind = LW_TRUE;
            }
        }
        for (j = 0; ((j < sizeof(pmgrMutexPhysicalToLogical_GP10X)) && !bFind); j++)
        {
            if (pmgrMutexPhysicalToLogical_GP10X[j] == i)
            {
                mutexLog2PhyTable[i].engine = MUTEX_ENGINE_PMGR;
                mutexLog2PhyTable[i].physId = j;
                bFind = LW_TRUE;
            }
        }
        for (j = 0; ((j < sizeof(sec2MutexPhysicalToLogical_GP10X)) && !bFind); j++)
        {
            if (sec2MutexPhysicalToLogical_GP10X[j] == i)
            {
                mutexLog2PhyTable[i].engine = MUTEX_ENGINE_SEC2;
                mutexLog2PhyTable[i].physId = j;
                bFind = LW_TRUE;
            }
        }
        if (!bFind)
        {
            mutexLog2PhyTable[i].engine = MUTEX_ENGINE_COUNT;
            mutexLog2PhyTable[i].physId = LW_MUTEX_PHYSICAL_ID_ILWALID;
        }
    }

    pEI = &mutexEngineInfo[MUTEX_ENGINE_PMU];

    pEI->mutexCount          = LW_CPWR_PMU_MUTEX__SIZE_1;
    pEI->regAddrMutexId      = LW_CPWR_PMU_MUTEX_ID;
    pEI->regAddrMutexIdRel   = LW_CPWR_PMU_MUTEX_ID_RELEASE;
    pEI->regAddrMutex        = LW_CPWR_PMU_MUTEX(0);
    pEI->regMutexIndexStride = LW_CPWR_PMU_MUTEX(1) - LW_CPWR_PMU_MUTEX(0);
    pEI->accessBus           = REG_BUS_CSB;

    pEI = &mutexEngineInfo[MUTEX_ENGINE_PMGR];

    pEI->mutexCount          = LW_PMGR_MUTEX_REG__SIZE_1;
    pEI->regAddrMutexId      = LW_PMGR_MUTEX_ID_ACQUIRE;
    pEI->regAddrMutexIdRel   = LW_PMGR_MUTEX_ID_RELEASE;
    pEI->regAddrMutex        = LW_PMGR_MUTEX_REG(0);
    pEI->regMutexIndexStride = LW_PMGR_MUTEX_REG(1) - LW_PMGR_MUTEX_REG(0);
    pEI->accessBus           = REG_BUS_FECS;

    pEI = &mutexEngineInfo[MUTEX_ENGINE_SEC2];

    pEI->mutexCount          = LW_PSEC_MUTEX__SIZE_1;
    pEI->regAddrMutexId      = LW_PSEC_MUTEX_ID;
    pEI->regAddrMutexIdRel   = LW_PSEC_MUTEX_ID_RELEASE;
    pEI->regAddrMutex        = LW_PSEC_MUTEX(0);
    pEI->regMutexIndexStride = LW_PSEC_MUTEX(1) - LW_PSEC_MUTEX(0);
    pEI->accessBus           = REG_BUS_FECS;
}
