/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   sec2_mutexgp10x.c
 *
 * This file helps generate the required mapping info or any required function
 * which will be needed by the common HW mutex library so that the library knows
 * how to access all the available HW mutex from SEC2.
 */

/* ------------------------- System Includes -------------------------------- */
#include "sec2sw.h"
#include "lw_mutex.h"
#include "dev_pmgr.h"
#include "dev_pmgr_addendum.h"
#include "dev_sec_addendum.h"

/* ------------------------- Application Includes --------------------------- */
#include "sec2_objsec2.h"

#include "config/g_sec2_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Global Variables ------------------------------- */
MUTEX_LOG2PHY_MAPPING mutexLog2PhyTable[LW_MUTEX_ID__COUNT];
MUTEX_ENGINE_INFO     mutexEngineInfo[MUTEX_ENGINE_COUNT];

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Function Declarations -------------------------- */
/* ------------------------- Static Function Prototypes  -------------------- */
static inline void _sec2MutexEstablishMapping_GP10X(void)
    GCC_ATTRIB_ALWAYSINLINE();

/* ------------------------- Public Functions ------------------------------- */
/*!
 * LS version of @ref _sec2MutexEstablishMapping_GP10X
 */
void
sec2MutexEstablishMapping_GP10X(void)
{
    _sec2MutexEstablishMapping_GP10X();
}

/*!
 * HS version of @ref _sec2MutexEstablishMapping_GP10X
 */
void
sec2MutexEstablishMappingHS_GP10X(void)
{
    _sec2MutexEstablishMapping_GP10X();
}

/* ------------------------- Static Functions ------------------------------- */
/*!
 * Generate required mappings from each logical mutex ID to the corresponding
 * mutex engine and the physical mutex ID which will be used by the mutex lib.
 */
static inline void
_sec2MutexEstablishMapping_GP10X(void)
{
    LwU8 i;
    LwU8 j;
    LwBool bFind;
    PMUTEX_ENGINE_INFO pEI;
    LwU8 pmgrMutexPhysicalToLogical_GP10X[LW_PMGR_MUTEX_REG__SIZE_1];
    LwU8 sec2MutexPhysicalToLogical_GP10X[LW_CSEC_MUTEX__SIZE_1];

    //
    // Initializing the logical to physical mapping table inside the code instead
    // of putting the table on global data section which has no signature protection
    // lwrrently. The proper way of implementation is to move this info to a DMEM
    // section which is protected by dmlvl at level 3.
    //
    i = 0;
    pmgrMutexPhysicalToLogical_GP10X[i++] = LW_MUTEX_ID_I2C_0;
    pmgrMutexPhysicalToLogical_GP10X[i++] = LW_MUTEX_ID_I2C_1;
    pmgrMutexPhysicalToLogical_GP10X[i++] = LW_MUTEX_ID_I2C_2;
    pmgrMutexPhysicalToLogical_GP10X[i++] = LW_MUTEX_ID_I2C_3;
    pmgrMutexPhysicalToLogical_GP10X[i++] = LW_MUTEX_ID_I2C_4;
    pmgrMutexPhysicalToLogical_GP10X[i++] = LW_MUTEX_ID_I2C_5;
    pmgrMutexPhysicalToLogical_GP10X[i++] = LW_MUTEX_ID_I2C_6;
    pmgrMutexPhysicalToLogical_GP10X[i++] = LW_MUTEX_ID_I2C_7;
    pmgrMutexPhysicalToLogical_GP10X[i++] = LW_MUTEX_ID_I2C_8;
    pmgrMutexPhysicalToLogical_GP10X[i++] = LW_MUTEX_ID_I2C_9;
    pmgrMutexPhysicalToLogical_GP10X[i++] = LW_MUTEX_ID_I2C_A;
    pmgrMutexPhysicalToLogical_GP10X[i++] = LW_MUTEX_ID_I2C_B;
    pmgrMutexPhysicalToLogical_GP10X[i++] = LW_MUTEX_ID_I2C_C;
    pmgrMutexPhysicalToLogical_GP10X[i++] = LW_MUTEX_ID_I2C_D;
    pmgrMutexPhysicalToLogical_GP10X[i++] = LW_MUTEX_ID_I2C_E;
    pmgrMutexPhysicalToLogical_GP10X[i++] = LW_MUTEX_ID_I2C_F;
    pmgrMutexPhysicalToLogical_GP10X[i++] = LW_MUTEX_ID_DISP_SELWRE_WR_SCRATCH;
    pmgrMutexPhysicalToLogical_GP10X[i++] = LW_MUTEX_ID_SEC2_EMEM_ACCESS;
    pmgrMutexPhysicalToLogical_GP10X[i++] = LW_MUTEX_ID_ILWALID;
    pmgrMutexPhysicalToLogical_GP10X[i++] = LW_MUTEX_ID_ILWALID;
    pmgrMutexPhysicalToLogical_GP10X[i++] = LW_MUTEX_ID_ILWALID;
    pmgrMutexPhysicalToLogical_GP10X[i++] = LW_MUTEX_ID_ILWALID;
    pmgrMutexPhysicalToLogical_GP10X[i++] = LW_MUTEX_ID_ILWALID;
    pmgrMutexPhysicalToLogical_GP10X[i++] = LW_MUTEX_ID_ILWALID;
    pmgrMutexPhysicalToLogical_GP10X[i++] = LW_MUTEX_ID_ILWALID;
    pmgrMutexPhysicalToLogical_GP10X[i++] = LW_MUTEX_ID_ILWALID;
    pmgrMutexPhysicalToLogical_GP10X[i++] = LW_MUTEX_ID_ILWALID;
    pmgrMutexPhysicalToLogical_GP10X[i++] = LW_MUTEX_ID_ILWALID;
    pmgrMutexPhysicalToLogical_GP10X[i++] = LW_MUTEX_ID_ILWALID;
    pmgrMutexPhysicalToLogical_GP10X[i++] = LW_MUTEX_ID_ILWALID;
    pmgrMutexPhysicalToLogical_GP10X[i++] = LW_MUTEX_ID_ILWALID;
    pmgrMutexPhysicalToLogical_GP10X[i++] = LW_MUTEX_ID_ILWALID;
    if (i != LW_PMGR_MUTEX_REG__SIZE_1)
    {
        // Something went wrong with the code above
        SEC2_HALT();
    }

    i = 0;
    sec2MutexPhysicalToLogical_GP10X[i++] = LW_MUTEX_ID_ILWALID;
    sec2MutexPhysicalToLogical_GP10X[i++] = LW_MUTEX_ID_PLAYREADY;
    sec2MutexPhysicalToLogical_GP10X[i++] = LW_MUTEX_ID_VPR_SCRATCH;
    sec2MutexPhysicalToLogical_GP10X[i++] = LW_MUTEX_ID_ACR_ALLOW_ONLY_ONE_ACR_BINARY_AT_ANY_TIME;
    sec2MutexPhysicalToLogical_GP10X[i++] = LW_MUTEX_ID_BSI_WRITE;
    sec2MutexPhysicalToLogical_GP10X[i++] = LW_MUTEX_ID_WPR_SCRATCH;
    sec2MutexPhysicalToLogical_GP10X[i++] = LW_MUTEX_ID_COLD_BOOT_GC6_UDE_COMPLETION;
    sec2MutexPhysicalToLogical_GP10X[i++] = LW_MUTEX_ID_MMU_VPR_WPR_WRITE;
    sec2MutexPhysicalToLogical_GP10X[i++] = LW_MUTEX_ID_DISP_SELWRE_SCRATCH;
    sec2MutexPhysicalToLogical_GP10X[i++] = LW_MUTEX_ID_PR_PDUB_PGC6_BSI_SELWRE_SCRATCH_8;
    sec2MutexPhysicalToLogical_GP10X[i++] = LW_MUTEX_ID_ILWALID;
    sec2MutexPhysicalToLogical_GP10X[i++] = LW_MUTEX_ID_ILWALID;
    sec2MutexPhysicalToLogical_GP10X[i++] = LW_MUTEX_ID_ILWALID;
    sec2MutexPhysicalToLogical_GP10X[i++] = LW_MUTEX_ID_ILWALID;
    sec2MutexPhysicalToLogical_GP10X[i++] = LW_MUTEX_ID_ILWALID;
    sec2MutexPhysicalToLogical_GP10X[i++] = LW_MUTEX_ID_ILWALID;
    if (i != LW_CSEC_MUTEX__SIZE_1)
    {
        // Something went wrong with the code above
        SEC2_HALT();
    }

    for (i = 0; i < LW_MUTEX_ID__COUNT; i++)
    {
        bFind = LW_FALSE;
        for (j = 0; ((j < sizeof(sec2MutexPhysicalToLogical_GP10X)) && !bFind); j++)
        {
            if (sec2MutexPhysicalToLogical_GP10X[j] == i)
            {
                mutexLog2PhyTable[i].engine = MUTEX_ENGINE_SEC2;
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
        if (!bFind)
        {
            mutexLog2PhyTable[i].engine = MUTEX_ENGINE_COUNT;
            mutexLog2PhyTable[i].physId = LW_MUTEX_PHYSICAL_ID_ILWALID;
        }
    }

    pEI = &mutexEngineInfo[MUTEX_ENGINE_PMGR];

    pEI->mutexCount          = LW_PMGR_MUTEX_REG__SIZE_1;
    pEI->regAddrMutexId      = LW_PMGR_MUTEX_ID_ACQUIRE;
    pEI->regAddrMutexIdRel   = LW_PMGR_MUTEX_ID_RELEASE;
    pEI->regAddrMutex        = LW_PMGR_MUTEX_REG(0);
    pEI->regMutexIndexStride = LW_PMGR_MUTEX_REG(1) - LW_PMGR_MUTEX_REG(0);
    pEI->accessBus           = MUTEX_BUS_BAR0;

    pEI = &mutexEngineInfo[MUTEX_ENGINE_SEC2];

    pEI->mutexCount          = LW_CSEC_MUTEX__SIZE_1;
    pEI->regAddrMutexId      = LW_CSEC_MUTEX_ID;
    pEI->regAddrMutexIdRel   = LW_CSEC_MUTEX_ID_RELEASE;
    pEI->regAddrMutex        = LW_CSEC_MUTEX(0);
    pEI->regMutexIndexStride = LW_CSEC_MUTEX(1) - LW_CSEC_MUTEX(0);
    pEI->accessBus           = MUTEX_BUS_CSB;
}
