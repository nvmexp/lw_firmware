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
 * @file   dpu_mutex0205.c
 *
 * This file helps generate the required mapping info or any required function
 * which will be needed by the common HW mutex library so that the library knows
 * how to access all the available HW mutex from DPU.
 */

/* ------------------------- System Includes -------------------------------- */
#include "dpusw.h"
#include "lw_mutex.h"
#include "dispflcn_regmacros.h"

/* ------------------------- Application Includes --------------------------- */
#include "lwosreg.h"
#include "config/g_dpu_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Global Variables ------------------------------- */
MUTEX_LOG2PHY_MAPPING mutexLog2PhyTable[LW_MUTEX_ID__COUNT];
MUTEX_ENGINE_INFO     mutexEngineInfo[MUTEX_ENGINE_COUNT];

/*!
 * The table mapping the DPU physical mutex ID to logical mtuex ID
 */
static LwU8 dpuMutexPhysicalToLogical_v02_05[] =
{
    LW_DPU_MUTEX_DEFINES
};

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Function Declarations -------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * Generate required mappings from each logical mutex ID to the corresponding
 * mutex engine and the physical mutex ID which will be used by the mutex lib.
 */
void
dpuMutexEstablishMapping_v02_05(void)
{
    LwU8 i;
    LwU8 j;
    LwBool bFind;
    PMUTEX_ENGINE_INFO pEI;

    for (i = 0; i < LW_MUTEX_ID__COUNT; i++)
    {
        bFind = LW_FALSE;
        for (j = 0; ((j < sizeof(dpuMutexPhysicalToLogical_v02_05)) && !bFind); j++)
        {
            if (dpuMutexPhysicalToLogical_v02_05[j] == i)
            {
                mutexLog2PhyTable[i].engine = MUTEX_ENGINE_DPU;
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

    pEI = &mutexEngineInfo[MUTEX_ENGINE_DPU];

    pEI->mutexCount          = DFLCN_DRF(MUTEX__SIZE_1);
    pEI->regAddrMutexId      = DFLCN_DRF(MUTEX_ID);
    pEI->regAddrMutexIdRel   = DFLCN_DRF(MUTEX_ID_RELEASE);
    pEI->regAddrMutex        = DFLCN_DRF(MUTEX(0));
    pEI->regMutexIndexStride = DFLCN_DRF(MUTEX(1)) - DFLCN_DRF(MUTEX(0));
    pEI->accessBus           = REG_BUS_CSB;

}

