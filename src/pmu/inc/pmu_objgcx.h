/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2009-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_OBJGCX_H
#define PMU_OBJGCX_H

/*!
 * @file pmu_OBJGCX.h
 */

/* ------------------------ System includes -------------------------------- */
#include "flcntypes.h"

/* ------------------------ Application includes --------------------------- */
#include "pmu_didle.h"
#include "lwostimer.h"
#include "config/g_gcx_hal.h"

/* ------------------------- Macros and Defines ---------------------------- */
// Didle Task Queue Size with GC5 enabled
#define PMU_GCX_QUEUE_SIZE_GC5                  16

/* ------------------------ Types definitions ------------------------------ */
/*!
 * GC4 object definition (Also used by GC3)
 */
typedef struct
{
    LwU8                    DidleLwrState;
    LwBool                  bDisarmResponsePending;
    LwU8                    cmdStatus;

    // Indicates whether we are lwrrently in the Deep L1 state
    LwBool                  bInDeepL1;

    // MSCG status engaged/disengaged
    LwBool                  bMscgEngaged;

    //
    // GC4 armed status
    // AGADRE-TODO: Remove this variable when the DidleLwrState is moved in
    //              a global structure.
    //
    LwBool                  bArmed;


    // Holds the current state of DeepIdle i.e Active/Inactive
    LwBool                  bInDIdle;

    DISPATCH_DIDLE          dispatch;
} OBJGC4;

/*!
 * DIDLE object Definition
 */
typedef struct
{
    // Indicates whether or not RM has sent initialization information
    LwBool      bInitialized;

    // Indicates whether or not we suspended dma operations lwrrently
    LwBool      bDmaSuspended;

    // Stores whether current GPU is displayless
    LwBool      bIsGpuDisplayless;

    OBJGC4     *pGc4;
} OBJGCX;

// Macro to get the GC4 object from GCX object
#define GCX_GET_GC4(gcx)    (gcx).pGc4

/* ------------------------ External definitions --------------------------- */
extern OBJGCX              Gcx;
extern RM_PMU_DIDLE_STAT   DidleStatisticsNH;
extern RM_PMU_DIDLE_STAT   DidleStatisticsSSC;
extern LwU8                DidleLwrState;
extern LwU8                DidleSubstate;

/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
void        gcxPreInit(void)
    GCC_ATTRIB_SECTION("imem_init", "gcxPreInit");

void didleNotifyMscgChange(LwBool)
    GCC_ATTRIB_SECTION("imem_libMs", "didleNotifyMscgChange")
    GCC_ATTRIB_NOINLINE();
void gcxDiosAttemptToEngage(void)
    GCC_ATTRIB_SECTION("imem_perfmon", "gcxDiosAttemptToEngage")
    GCC_ATTRIB_NOINLINE();

void gcxAttachGC45toPG  (void);
void gcxDetachGC45fromPG(void);

void        didleResetStatistics  (RM_PMU_DIDLE_STAT *);
extern void didleMessageDisarmOs  (LwU8);
extern LwU8 didleNextStateGet     (LwU32);

#endif // PMU_OBJGCX_H
