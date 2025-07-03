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
 * @file   pmu_mutexgm20x.c
 *
 * This file helps generate the required mapping info or any required function
 * which will be needed by the common HW mutex library so that the library knows
 * how to access all the available HW mutex from PMU.
 */


/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "lwosreg.h"
#include "dev_disp_falcon.h"
#include "dev_disp_falcon_addendum.h"
#include "dev_pwr_csb.h"
#include "dev_pwr_pri_addendum.h"
#include "dev_pmgr.h"
#include "dev_pmgr_addendum.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "lw_mutex.h"
#include "lib_mutex.h"
#include "pmu_objdisp.h"

#include "config/g_mutex_private.h"
#include "config/g_pmu_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Global Variables ------------------------------- */
MUTEX_LOG2PHY_MAPPING mutexLog2PhyTable[LW_MUTEX_ID__COUNT];
MUTEX_ENGINE_INFO     mutexEngineInfo[MUTEX_ENGINE_COUNT];

/*!
 * The table mapping the DPU physical mutex ID to logical mtuex ID
 */
static LwU8 dpuMutexPhysicalToLogical_GM20X[] =
{
    LW_DPU_MUTEX_DEFINES
};

/*!
 * The table mapping the PMU physical mutex ID to logical mtuex ID on
 * displayable GPU
 */
static LwU8 pmuMutexPhysicalToLogical_displable_GM20X[] =
{
    LW_PMU_MUTEX_DISPABLE_DEFINES
};

/*!
 * The table mapping the PMU physical mutex ID to logical mtuex ID on
 * displayless GPU
 */
static LwU8 pmuMutexPhysicalToLogical_displess_GM20X[] =
{
    LW_PMU_MUTEX_DISPLESS_DEFINES
};

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Function Declarations -------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * Generate required mappings from each logical mutex ID to the corresponding
 * mutex engine and the physical mutex ID which will be used by the mutex lib.
 */
void
mutexEstablishMapping_GM20X(void)
{
    LwU8 i;
    LwU8 j;
    LwBool bFind;
    PMUTEX_ENGINE_INFO pEI;
    LwU8 *pPmuPhysToLogTable = (DISP_IS_PRESENT() ?
        pmuMutexPhysicalToLogical_displable_GM20X :
        pmuMutexPhysicalToLogical_displess_GM20X);
    LwU8 pmuPhysToLogTableSize = (DISP_IS_PRESENT() ?
        sizeof(pmuMutexPhysicalToLogical_displable_GM20X) :
        sizeof(pmuMutexPhysicalToLogical_displess_GM20X));

    for (i = 0; i < LW_MUTEX_ID__COUNT; i++)
    {
        bFind = LW_FALSE;
        if (DISP_IS_PRESENT())
        {
            for (j = 0; ((j < sizeof(dpuMutexPhysicalToLogical_GM20X)) && !bFind); j++)
            {
                if (dpuMutexPhysicalToLogical_GM20X[j] == i)
                {
                    mutexLog2PhyTable[i].engine = MUTEX_ENGINE_DPU;
                    mutexLog2PhyTable[i].physId = j;
                    bFind = LW_TRUE;
                }
            }
        }
        for (j = 0; ((j < pmuPhysToLogTableSize) && !bFind); j++)
        {
            if (pPmuPhysToLogTable[j] == i)
            {
                mutexLog2PhyTable[i].engine = MUTEX_ENGINE_PMU;
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

    if (DISP_IS_PRESENT())
    {
        pEI = &mutexEngineInfo[MUTEX_ENGINE_DPU];

        pEI->mutexCount          = LW_PDISP_FALCON_MUTEX__SIZE_1;
        pEI->regAddrMutexId      = LW_PDISP_FALCON_MUTEX_ID;
        pEI->regAddrMutexIdRel   = LW_PDISP_FALCON_MUTEX_ID_RELEASE;
        pEI->regAddrMutex        = LW_PDISP_FALCON_MUTEX(0);
        pEI->regMutexIndexStride = LW_PDISP_FALCON_MUTEX(1) - LW_PDISP_FALCON_MUTEX(0);
        pEI->accessBus           = REG_BUS_FECS;
    }

    pEI = &mutexEngineInfo[MUTEX_ENGINE_PMU];

    pEI->mutexCount          = LW_CPWR_PMU_MUTEX__SIZE_1;
    pEI->regAddrMutexId      = LW_CPWR_PMU_MUTEX_ID;
    pEI->regAddrMutexIdRel   = LW_CPWR_PMU_MUTEX_ID_RELEASE;
    pEI->regAddrMutex        = LW_CPWR_PMU_MUTEX(0);
    pEI->regMutexIndexStride = LW_CPWR_PMU_MUTEX(1) - LW_CPWR_PMU_MUTEX(0);
    pEI->accessBus           = REG_BUS_CSB;
}

