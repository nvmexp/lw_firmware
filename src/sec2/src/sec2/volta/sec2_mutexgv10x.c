/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   sec2_mutexgv10x.c
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
#include "dev_se_seb.h"
#include "dev_se_seb_addendum.h"

/* ------------------------- Application Includes --------------------------- */
#include "config/g_sec2_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Global Variables ------------------------------- */
MUTEX_LOG2PHY_MAPPING mutexLog2PhyTable[LW_MUTEX_ID__COUNT];
MUTEX_ENGINE_INFO     mutexEngineInfo[MUTEX_ENGINE_COUNT];

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Function Declarations -------------------------- */
/* ------------------------- Static Function Prototypes  -------------------- */
static inline void _sec2MutexEstablishMapping_GV10X(void)
    GCC_ATTRIB_ALWAYSINLINE();

/* ------------------------- Public Functions ------------------------------- */
/*!
 * LS version of @ref _sec2MutexEstablishMapping_GV10X
 */
void
sec2MutexEstablishMapping_GV10X(void)
{
    _sec2MutexEstablishMapping_GV10X();
}

/*!
 * HS version of @ref _sec2MutexEstablishMapping_GV10X
 */
void
sec2MutexEstablishMappingHS_GV10X(void)
{
    _sec2MutexEstablishMapping_GV10X();
}

/* ------------------------- Static Functions ------------------------------- */
/*!
 * Generate required mappings from each logical mutex ID to the corresponding
 * mutex engine and the physical mutex ID which will be used by the mutex lib.
 */
static inline void
_sec2MutexEstablishMapping_GV10X(void)
{
    LwU8 i;
    LwU8 j;
    LwBool bFind;
    PMUTEX_ENGINE_INFO pEI;
    LwU8 pmgrMutexPhysicalToLogical_GV10X[LW_PMGR_MUTEX_REG__SIZE_1];
    LwU8 seHsGrp0MutexPhysicalToLogical_GV10X[LW_SSE_SE_COMMON_MUTEX_MUTEX__SIZE_2];
    LwU8 seHsGrp1MutexPhysicalToLogical_GV10X[LW_SSE_SE_COMMON_MUTEX_MUTEX__SIZE_2];
    LwU8 seLsMutexPhysicalToLogical_GV10X[LW_SSE_SE_COMMON_MUTEX_MUTEX__SIZE_2];

    //
    // Initializing the logical to physical mapping table inside the code instead
    // of putting the table on global data section which has no signature protection
    // lwrrently. The proper way of implementation is to move this info to a DMEM
    // section which is protected by dmlvl at level 3.
    //
    i = 0;
    pmgrMutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_I2C_0;
    pmgrMutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_I2C_1;
    pmgrMutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_I2C_2;
    pmgrMutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_I2C_3;
    pmgrMutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_I2C_4;
    pmgrMutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_I2C_5;
    pmgrMutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_I2C_6;
    pmgrMutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_I2C_7;
    pmgrMutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_I2C_8;
    pmgrMutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_I2C_9;
    pmgrMutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_I2C_A;
    pmgrMutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_I2C_B;
    pmgrMutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_I2C_C;
    pmgrMutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_I2C_D;
    pmgrMutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_I2C_E;
    pmgrMutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_I2C_F;
    pmgrMutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_DISP_SELWRE_WR_SCRATCH;
    pmgrMutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_HBM_IEEE1500;
    pmgrMutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_ILWALID;
    pmgrMutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_ILWALID;
    pmgrMutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_ILWALID;
    pmgrMutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_ILWALID;
    pmgrMutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_ILWALID;
    pmgrMutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_ILWALID;
    pmgrMutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_ILWALID;
    pmgrMutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_ILWALID;
    pmgrMutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_ILWALID;
    pmgrMutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_ILWALID;
    pmgrMutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_ILWALID;
    pmgrMutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_ILWALID;
    pmgrMutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_ILWALID;
    pmgrMutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_ILWALID;
    if (i != LW_PMGR_MUTEX_REG__SIZE_1)
    {
        // Something went wrong with the code above
        SEC2_HALT();
    }

    i = 0;
    seHsGrp0MutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_RESERVED_1;
    seHsGrp0MutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_PLAYREADY;
    seHsGrp0MutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_VPR_SCRATCH;
    seHsGrp0MutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_ACR_ALLOW_ONLY_ONE_ACR_BINARY_AT_ANY_TIME;
    seHsGrp0MutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_HW_SCRUBBER;
    seHsGrp0MutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_WPR_SCRATCH;
    seHsGrp0MutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_COLD_BOOT_GC6_UDE_COMPLETION;
    seHsGrp0MutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_MMU_VPR_WPR_WRITE;
    seHsGrp0MutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_DISP_SELWRE_SCRATCH;
    seHsGrp0MutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_PR_PDUB_PGC6_BSI_SELWRE_SCRATCH_8;
    seHsGrp0MutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_DECODE_TRAPS_WAR_TU10X_BUG_2369597;
    seHsGrp0MutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_APM_RTS;
    seHsGrp0MutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_RESERVED_3;
    seHsGrp0MutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_RESERVED_4;
    seHsGrp0MutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_RESERVED_5;
    seHsGrp0MutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_RESERVED_6;
    if (i != LW_SSE_SE_COMMON_MUTEX_MUTEX__SIZE_2)
    {
        // Something went wrong with the code above
        SEC2_HALT();
    }

    i = 0;
    seHsGrp1MutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_RESERVED_7;
    seHsGrp1MutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_RESERVED_8;
    seHsGrp1MutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_RESERVED_9;
    seHsGrp1MutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_RESERVED_10;
    seHsGrp1MutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_RESERVED_11;
    seHsGrp1MutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_RESERVED_12;
    seHsGrp1MutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_RESERVED_13;
    seHsGrp1MutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_RESERVED_14;
    seHsGrp1MutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_RESERVED_15;
    seHsGrp1MutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_RESERVED_16;
    seHsGrp1MutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_RESERVED_17;
    seHsGrp1MutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_RESERVED_18;
    seHsGrp1MutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_RESERVED_19;
    seHsGrp1MutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_RESERVED_20;
    seHsGrp1MutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_RESERVED_21;
    seHsGrp1MutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_RESERVED_22;
    if (i != LW_SSE_SE_COMMON_MUTEX_MUTEX__SIZE_2)
    {
        // Something went wrong with the code above
        SEC2_HALT();
    }


    i = 0;
    seLsMutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_RESERVED_23;
    seLsMutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_RESERVED_24;
    seLsMutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_RESERVED_25;
    seLsMutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_RESERVED_26;
    seLsMutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_BSI_WRITE;
    seLsMutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_RESERVED_27;
    seLsMutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_RESERVED_28;
    seLsMutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_RESERVED_29;
    seLsMutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_RESERVED_30;
    seLsMutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_RESERVED_31;
    seLsMutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_RESERVED_32;
    seLsMutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_RESERVED_33;
    seLsMutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_RESERVED_34;
    seLsMutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_RESERVED_35;
    seLsMutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_RESERVED_36;
    seLsMutexPhysicalToLogical_GV10X[i++] = LW_MUTEX_ID_RESERVED_37;
    if (i != LW_SSE_SE_COMMON_MUTEX_MUTEX__SIZE_2)
    {
        // Something went wrong with the code above
        SEC2_HALT();
    }


    for (i = 0; i < LW_MUTEX_ID__COUNT; i++)
    {
        bFind = LW_FALSE;
        for (j = 0; ((j < sizeof(seHsGrp0MutexPhysicalToLogical_GV10X)) && !bFind); j++)
        {
            if (seHsGrp0MutexPhysicalToLogical_GV10X[j] == i)
            {
                mutexLog2PhyTable[i].engine = MUTEX_ENGINE_SE_HS_GRP0;
                mutexLog2PhyTable[i].physId = j;
                bFind = LW_TRUE;
                break;
            }
        }
        for (j = 0; ((j < sizeof(seHsGrp1MutexPhysicalToLogical_GV10X)) && !bFind); j++)
        {
            if (seHsGrp1MutexPhysicalToLogical_GV10X[j] == i)
            {
                mutexLog2PhyTable[i].engine = MUTEX_ENGINE_SE_HS_GRP1;
                mutexLog2PhyTable[i].physId = j;
                bFind = LW_TRUE;
                break;
            }
        }
        for (j = 0; ((j < sizeof(seLsMutexPhysicalToLogical_GV10X)) && !bFind); j++)
        {
            if (seLsMutexPhysicalToLogical_GV10X[j] == i)
            {
                mutexLog2PhyTable[i].engine = MUTEX_ENGINE_SE_LS;
                mutexLog2PhyTable[i].physId = j;
                bFind = LW_TRUE;
                break;
            }
        }
        for (j = 0; ((j < sizeof(pmgrMutexPhysicalToLogical_GV10X)) && !bFind); j++)
        {
            if (pmgrMutexPhysicalToLogical_GV10X[j] == i)
            {
                mutexLog2PhyTable[i].engine = MUTEX_ENGINE_PMGR;
                mutexLog2PhyTable[i].physId = j;
                bFind = LW_TRUE;
                break;
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

    pEI = &mutexEngineInfo[MUTEX_ENGINE_SE_HS_GRP0];

    pEI->mutexCount          = LW_SSE_SE_COMMON_MUTEX_MUTEX__SIZE_2;
    pEI->regAddrMutexId      = LW_SSE_SE_COMMON_MUTEX_ID(0);
    pEI->regAddrMutexIdRel   = LW_SSE_SE_COMMON_MUTEX_ID_RELEASE(0);
    pEI->regAddrMutex        = LW_SSE_SE_COMMON_MUTEX_MUTEX(0,0);
    pEI->regMutexIndexStride = LW_SSE_SE_COMMON_MUTEX_MUTEX(0,1) - LW_SSE_SE_COMMON_MUTEX_MUTEX(0,0);
    pEI->accessBus           = MUTEX_BUS_SE;

    pEI = &mutexEngineInfo[MUTEX_ENGINE_SE_HS_GRP1];

    pEI->mutexCount          = LW_SSE_SE_COMMON_MUTEX_MUTEX__SIZE_2;
    pEI->regAddrMutexId      = LW_SSE_SE_COMMON_MUTEX_ID(1);
    pEI->regAddrMutexIdRel   = LW_SSE_SE_COMMON_MUTEX_ID_RELEASE(1);
    pEI->regAddrMutex        = LW_SSE_SE_COMMON_MUTEX_MUTEX(1,0);
    pEI->regMutexIndexStride = LW_SSE_SE_COMMON_MUTEX_MUTEX(1,1) - LW_SSE_SE_COMMON_MUTEX_MUTEX(1,0);
    pEI->accessBus           = MUTEX_BUS_SE;

    pEI = &mutexEngineInfo[MUTEX_ENGINE_SE_LS];

    pEI->mutexCount          = LW_SSE_SE_COMMON_MUTEX_MUTEX__SIZE_2;
    pEI->regAddrMutexId      = LW_SSE_SE_COMMON_MUTEX_ID(2);
    pEI->regAddrMutexIdRel   = LW_SSE_SE_COMMON_MUTEX_ID_RELEASE(2);
    pEI->regAddrMutex        = LW_SSE_SE_COMMON_MUTEX_MUTEX(2,0);
    pEI->regMutexIndexStride = LW_SSE_SE_COMMON_MUTEX_MUTEX(2,1) - LW_SSE_SE_COMMON_MUTEX_MUTEX(2,0);
    pEI->accessBus           = MUTEX_BUS_SE;
}

