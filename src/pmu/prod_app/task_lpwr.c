/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2007-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file task_lpwr.c
 *
 * The object code task_lpwr.o should be placed in same overlay (.pg) as
 * pmu_pggkxxx.o or the file which contains the MS ELPG routines.  MSCG, when
 * engaged, suspends DMA between FB and DMEM. Any new DMA of overlay triggers a
 * suspension abort handled by vApplicationDmaSuspensionNotice which sends a
 * command on PgQ. At such a time, task2 needs to be in DMEM to receive the
 * command on LWOS_QUEUE(PMU, PG).
 *
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "lwostimer.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objlpwr.h"
#include "main.h"
#include "main_init_api.h"
#include "cmdmgmt/cmdmgmt.h"
#include "os/pmu_lwos_task.h"

/* ------------------------- Prototypes ------------------------------------- */
static void s_pgEventHandle(DISPATCH_LPWR *pRequest);

lwrtosTASK_FUNCTION(task_lpwr, pvParameters);

/* ------------------------- Macros and Defines ----------------------------- */
/*
 * @brief Timeout to receive events from queue for CPU based PG.
 */
#define PG_CPU_BASED_PG_EVENT_TIMEOUT_TICKS                             (10000)

/* ------------------------- Global Variables ------------------------------- */
#if PMUCFG_FEATURE_ENABLED(PMU_LPWR_DEBUG)
LwU32 LpwrEventInfo[LPWR_EVENT_DEBUG_COUNT];
LwU32 LpwrEventData[LPWR_EVENT_DEBUG_COUNT];
LwU32 LpwrLwrrEventIdx;
#endif // PMUCFG_FEATURE_ENABLED(PMU_LPWR_DEBUG)

/* ------------------------- Static Variables ------------------------------- */
/*!
 * @brief Union of all possible RPC subsurface data that can be handled by the LPWR task.
 */
typedef union
{
    /*!
     * @brief Default size, as defined by configuration
     */
    LwU8 defaultSize[OSTASK_ATTRIBUTE_LPWR_CMD_SCRATCH_SIZE];
} TASK_SURFACE_UNION_LPWR;

/*!
 * @brief   Defines the command buffer for the LPWR task.
 */
PMU_TASK_CMD_BUFFER_DEFINE_SCRATCH_SIZE(LPWR, "dmem_lpwrCmdBuffer");

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief      Initialize the LPWR Task.
 *
 * @return     FLCN_OK on success,
 * @return     descriptive error code otherwise.
 */
FLCN_STATUS
lpwrPreInitTask(void)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32 queueSize = 0U;

    queueSize =  PMUCFG_FEATURE_ENABLED(PMU_PG_MS) ?
                    RM_PMU_PG_QUEUE_SIZE_MS_SUPPORT :
                    RM_PMU_PG_QUEUE_SIZE_DEFAULT;

    //
    // 16 bins of 2^15[ns] == ~32.8[us] ending at ~524[us]
    // 16 bins of 2^18[ns] == ~262[us] last bin starting at ~4.2[ms]
    //
    OSTASK_CREATE(status, PMU, LPWR);
    if (status != FLCN_OK)
    {
        goto lpwrPreInitTask_exit;
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_CALLBACKS_CENTRALIZED))
    {
        // Assure that we have queue slot for each callback.
        queueSize++;    // OsCBApGr
        queueSize++;    // OsCBApDi
    }

    status = lwrtosQueueCreateOvlRes(&LWOS_QUEUE(PMU, LPWR),
                                     queueSize, sizeof(DISPATCH_LPWR));
    if (status != FLCN_OK)
    {
        goto lpwrPreInitTask_exit;
    }

lpwrPreInitTask_exit:
    return status;
}

/*!
 * @brief Entry point for PG task (AKA LPWR task)
 *
 * LPWR task handles different LowPower features like - HW-CG, ELPG, MSCG, RPPG,
 * PSI, AP, GC6 etc.
 */
lwrtosTASK_FUNCTION(task_lpwr, pvParameters)
{
    DISPATCH_LPWR lpwrEvt;
    FLCN_STATUS   status = FLCN_OK;

    // Attach the lpwr loadin overlay
    pgOverlayAttachAndLoad(PG_OVL_ID_MASK_LPWR_LOADIN);
    status = lpwrPostInit();
    if (status != FLCN_OK)
    {
        PMU_HALT();
    }
    pgOverlayDetach(PG_OVL_ID_MASK_LPWR_LOADIN);

#if (PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))
    LWOS_TASK_LOOP(LWOS_QUEUE(PMU, LPWR), &lpwrEvt, status, PMU_TASK_CMD_BUFFER_PTRS_GET(LPWR))
    {
        s_pgEventHandle(&lpwrEvt);
        status = FLCN_OK; // NJ-TODO-CB
    }
    LWOS_TASK_LOOP_END(status);
#else
    lwosVarNOP(PMU_TASK_CMD_BUFFER_PTRS_GET(LPWR));

    for (;;)
    {
        OS_TIMER_WAIT_FOR_QUEUE_OR_CALLBACK_START(
            &Pg.osTimer,             // pOsTime
            LWOS_QUEUE(PMU, LPWR),   // Queue
            &lpwrEvt,                // Pointer to DISPATCH_LPWR structure
            lwrtosMAX_DELAY)         // maxDelay
        {
            s_pgEventHandle(&lpwrEvt);
        }
        OS_TIMER_WAIT_FOR_QUEUE_OR_CALLBACK_STOP(&Pg.osTimer, lwrtosMAX_DELAY);
    }
#endif
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief   Helper call handling events sent to PG task.
 */
static void
s_pgEventHandle
(
    DISPATCH_LPWR *pRequest
)
{
    PG_LOGIC_STATE pgLogicState;

    // Initialize PG logic state
    pgLogicState.eventId = PMU_PG_EVENT_NONE;
    pgLogicState.ctrlId  = RM_PMU_LPWR_CTRL_ID_ILWALID_ENGINE;
    pgLogicState.data    = 0;

    //
    // LPWR event processing
    //
    // This is the top level LPWR event handler that processes commands, events
    // and interrupts to unified PG event. If event is targeted for PG, then it
    // update PG logic state. pgLogicAll() does the further processing of
    // unified PG events.
    //
    lpwrEventHandler(pRequest, &pgLogicState);

    if (PMUCFG_FEATURE_ENABLED(PMU_PG_PMU_ONLY)   &&
        pgLogicState.eventId != PMU_PG_EVENT_NONE &&
        pgIsCtrlSupported(pgLogicState.ctrlId))
    {
        // Update the info about current event
        LPWR_EVENT_DEBUG_INFO_UPDATE(pgLogicState.ctrlId, pgLogicState.eventId, pgLogicState.data);

        pgLogicAll(&pgLogicState);
    }
}
