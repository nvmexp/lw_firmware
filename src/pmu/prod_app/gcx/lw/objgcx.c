/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2009-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   objgcx.c
 * @brief  Top-level display object for deepidle state and HAL infrastructure.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "main_init_api.h"
#include "pmu_objhal.h"
#include "pmu_objclk.h"
#include "pmu_objgcx.h"
#include "pmu_objpg.h"
#include "pmu_disp.h"
#include "pmu_oslayer.h"
#include "dbgprintf.h"

#include "config/g_gcx_private.h"
#if(DIDLE_MOCK_FUNCTIONS_GENERATED)
#include "config/g_gcx_mock.c"
#endif

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
OBJGCX Gcx;

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief This function performs all preinitialization tasks for GCX task
 * like attaching overlays
 */
void
gcxPreInit(void)
{
    //
    // Permanently attach .clkLibCntr to synchronize when gating GPCCLK.
    // We don't need clkLibCntr overlay for GC5.
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CNTR) &&
        !PMUCFG_FEATURE_ENABLED(PMU_DIDLE_OS))
    {
        OSTASK_ATTACH_IMEM_OVERLAY_TO_TASK(OSTASK_TCB(GCX),
                                           OVL_INDEX_IMEM(clkLibCntr));
        OSTASK_ATTACH_IMEM_OVERLAY_TO_TASK(OSTASK_TCB(GCX),
                                           OVL_INDEX_IMEM(clkLibCntrSwSync));
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_VOLTAGE_SAMPLE_CALLBACK))
    {
        OSTASK_ATTACH_IMEM_OVERLAY_TO_TASK(OSTASK_TCB(GCX),
                                           OVL_INDEX_IMEM(libClkVolt));
    }

    // Permanently attach .libDi
    if (PMUCFG_FEATURE_ENABLED(PMU_DI_SEQUENCE_COMMON))
    {
        OSTASK_ATTACH_IMEM_OVERLAY_TO_TASK(OSTASK_TCB(GCX),
                                           OVL_INDEX_IMEM(libDi));
    }
}

/*!
 * Construct the DIDLE object.  This sets up the HAL interface used by the
 * DIDLE module.
 *
 * @return  FLCN_OK     On success
 */
FLCN_STATUS
constructGcx(void)
{
    static OBJGC4 gc4;

    //
    // Following are Initialized to 0 by default being a global declaration
    //
    // Gcx.bInitialized = LW_FALSE;
    // Gcx.pSsc         = NULL;
    //

    // Need to be moved to better place
    Gcx.pGc4 = &gc4;

    //
    // Following are Initialized to 0 by default being a static declaration
    //
    // Gcx.pGc4->bInDeepL1                   = LW_FALSE;
    // Gcx.pGc4->bInDIdle                    = LW_FALSE;
    // Gcx.pGc4->bDmaSuspended               = LW_FALSE;
    //

    return FLCN_OK;
}

/*!
 * PG Logic to DI Notifcation Logic for GC4/5
 *
 * Sends a notification from PG to DI task about current state of MSCG
 *
 * @param[in] bEnter True indicates we have engaged MSCG and False
 *                   indicates we have disengaged MSCG
 *
 * @returns FLCN_OK
 */
void
didleNotifyMscgChange
(
    LwBool bEngage
)
{
    DISPATCH_DIDLE  dispatchDI;
    OBJGC4         *pGc4               = GCX_GET_GC4(Gcx);
    static LwBool   bNotifyOnMscgEntry = LW_FALSE;

    if (bEngage) // MSCG is engaged
    {
        //
        // We have completed MSCG entry. Notify to GCX task so that it can
        // engage DIOS/DISSC. Send notification to GCX task if either DIOS
        // or DISSC needs it.
        //

        //
        // A) Check if DIOS needs MSCG entry notification
        // Notify DIOS only when:
        //      1) DIOS is armed
        //      2) DeepL1 is engaged
        //
        if ((PMUCFG_FEATURE_ENABLED(PMU_DIDLE_OS)) &&
            (pGc4 != NULL)                         &&
            (pGc4->bArmed)                         &&
            (pGc4->bInDeepL1))
        {
            bNotifyOnMscgEntry = LW_TRUE;
        }

        // Send notification only if its required
        if (bNotifyOnMscgEntry)
        {
            dispatchDI.hdr.eventType     = DIDLE_SIGNAL_EVTID;
            dispatchDI.sigGen.signalType = DIDLE_SIGNAL_MSCG_ENGAGED;

            //
            // PG task should not wait for GCX queue to become empty.
            // This will delay MSCG exit.
            //
            (void)lwrtosQueueIdSend(LWOS_QUEUE_ID(PMU, GCX),
                                    &dispatchDI, sizeof(dispatchDI), 1);
        }
    }
    else
    {
        // Update flag
        pGc4->bMscgEngaged = LW_FALSE;

        // By this time we should be out of DI-OS.
        if (PMUCFG_FEATURE_ENABLED(PMU_DIDLE_OS))
        {
            PMU_HALT_COND(DidleLwrState != DIDLE_STATE_IN_OS);
        }

        if (bNotifyOnMscgEntry)
        {
            dispatchDI.hdr.eventType     = DIDLE_SIGNAL_EVTID;
            dispatchDI.sigGen.signalType = DIDLE_SIGNAL_MSCG_DISENGAGED;

            // Wait only for 1 tick to make sure that even gets pushed-in.
            (void)lwrtosQueueIdSend(LWOS_QUEUE_ID(PMU, GCX),
                                      &dispatchDI, sizeof(dispatchDI), 1);

            bNotifyOnMscgEntry = LW_FALSE;
        }
    }
}

/*
 * @Brief Attaches the GCX & libSyncGpio overlay to PG task.
 */
void
gcxAttachGC45toPG(void)
{
    //
    // WAR to fix race condition while attaching overlays to PG from GCX
    // since PG may try to attach overlays at the same time. This WAR
    // should be removed when GCX task is migrated to PG. This will NOT work if
    // PG priority <= GCX priority
    //

    appTaskCriticalEnter();
    {
        OSTASK_ATTACH_IMEM_OVERLAY_TO_TASK(OSTASK_TCB(LPWR), OSTASK_OVL_IMEM(GCX));

        if (PMUCFG_FEATURE_ENABLED(PMU_DI_SEQUENCE_COMMON))
        {
            OSTASK_ATTACH_IMEM_OVERLAY_TO_TASK(OSTASK_TCB(LPWR), OVL_INDEX_IMEM(libDi));
        }

        if (PMUCFG_FEATURE_ENABLED(PMU_PGISLAND_SCI_PMGR_GPIO_SYNC) &&
            !Pg.bSyncGpioAttached)
        {
            OSTASK_ATTACH_IMEM_OVERLAY_TO_TASK(OSTASK_TCB(LPWR), OVL_INDEX_IMEM(libSyncGpio));
            Pg.bSyncGpioAttached = LW_TRUE;
        }
    }
    appTaskCriticalExit();
}

/*
 * @Brief Detaches the GCX  & libSyncGpio overlay from PG task.
 */
void
gcxDetachGC45fromPG(void)
{
    appTaskCriticalEnter();
    {
        OSTASK_DETACH_IMEM_OVERLAY_FROM_TASK(OSTASK_TCB(LPWR), OSTASK_OVL_IMEM(GCX));

        if (PMUCFG_FEATURE_ENABLED(PMU_DI_SEQUENCE_COMMON))
        {
            OSTASK_DETACH_IMEM_OVERLAY_FROM_TASK(OSTASK_TCB(LPWR), OVL_INDEX_IMEM(libDi));
        }

        if (PMUCFG_FEATURE_ENABLED(PMU_PGISLAND_SCI_PMGR_GPIO_SYNC) &&
            Pg.bSyncGpioAttached)
        {
            OSTASK_DETACH_IMEM_OVERLAY_FROM_TASK(OSTASK_TCB(LPWR), OVL_INDEX_IMEM(libSyncGpio));
            Pg.bSyncGpioAttached = LW_FALSE;
        }
    }
    appTaskCriticalExit();
}
