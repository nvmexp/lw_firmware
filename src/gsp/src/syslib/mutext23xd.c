/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   mutext23xd.c
 *
 * This file helps generate the required mapping info or any required function
 * which will be needed by the common HW mutex library so that the library knows
 * how to access all the available HW mutex from GSP.
 */

/* ------------------------- System Includes -------------------------------- */
#include "gspsw.h"
#include "dev_pmgr_addendum.h"
#include "dev_gsp_addendum.h"
#include "lw_mutex.h"

/* ------------------------- Application Includes --------------------------- */
#include "lwosreg.h"

#include "config/g_mutex_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Global Variables ------------------------------- */
sysTASK_DATA MUTEX_LOG2PHY_MAPPING mutexLog2PhyTable[LW_MUTEX_ID__COUNT];
sysTASK_DATA MUTEX_ENGINE_INFO     mutexEngineInfo[MUTEX_ENGINE_COUNT];

/*!
 * The table mapping the PMGR physical mutex ID to logical mtuex ID
 */
sysTASK_DATA static LwU8 pmgrMutexPhysicalToLogical_T23XD[] =
{
    LW_PMGR_MUTEX_DEFINES
};

/*!
 * The table mapping the GSP physical mutex ID to logical mtuex ID
 */
sysTASK_DATA static LwU8 gspMutexPhysicalToLogical_T23XD[] =
{
    LW_GSP_MUTEX_DEFINES
};

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Function Declarations -------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * Generate required mappings from each logical mutex ID to the corresponding
 * mutex engine and the physical mutex ID which will be used by the mutex lib.
 */
sysSYSLIB_CODE void
mutexEstablishMapping_T23XD(void)
{
    LwU8 i;
    LwU8 j;
    LwBool bFind;
    PMUTEX_ENGINE_INFO pEI;

    //TODO 200586859: Need to have CHEETAH implementation here
    for (i = 0; i < LW_MUTEX_ID__COUNT; i++)
    {
        bFind = LW_FALSE;
        for (j = 0; ((j < sizeof(gspMutexPhysicalToLogical_T23XD)) && !bFind); j++)
        {
            if (gspMutexPhysicalToLogical_T23XD[j] == i)
            {
                mutexLog2PhyTable[i].engine = MUTEX_ENGINE_DPU; // Reuse DPU mutex ID
                mutexLog2PhyTable[i].physId = j;
                bFind = LW_TRUE;
            }
        }
        for (j = 0; ((j < sizeof(pmgrMutexPhysicalToLogical_T23XD)) && !bFind); j++)
        {
            if (pmgrMutexPhysicalToLogical_T23XD[j] == i)
            {
                mutexLog2PhyTable[i].engine = MUTEX_ENGINE_PMGR;
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

    pEI->mutexCount          = LW_PGSP_MUTEX__SIZE_1;
    pEI->regAddrMutexId      = LW_PGSP_MUTEX_ID;
    pEI->regAddrMutexIdRel   = LW_PGSP_MUTEX_ID_RELEASE;
    pEI->regAddrMutex        = LW_PGSP_MUTEX(0);
    pEI->regMutexIndexStride = LW_PGSP_MUTEX(1) - LW_PGSP_MUTEX(0);
    // TODO commen out due to lack of BAR0 bus on cheetah
    // pEI->accessBus           = REG_BUS_BAR0;

    //TODO 200586859: comment out due to lack of dev_pmgr.h
    // pEI = &mutexEngineInfo[MUTEX_ENGINE_PMGR];

    // pEI->mutexCount          = LW_PMGR_MUTEX_REG__SIZE_1;
    // pEI->regAddrMutexId      = LW_PMGR_MUTEX_ID_ACQUIRE;
    // pEI->regAddrMutexIdRel   = LW_PMGR_MUTEX_ID_RELEASE;
    // pEI->regAddrMutex        = LW_PMGR_MUTEX_REG(0);
    // pEI->regMutexIndexStride = LW_PMGR_MUTEX_REG(1) - LW_PMGR_MUTEX_REG(0);
    // pEI->accessBus           = REG_BUS_BAR0;
}
