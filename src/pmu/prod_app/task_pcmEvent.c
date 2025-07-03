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
 * @file   task_pcmEvent.c
 * @brief  PMU Preventative and Corrective Maintenance Task (Event Based)
 *
 * For a variety of reasons including location, flexibility, and determinism,
 * the PMU is an ideal location for dealing with various HW issues (from both
 * preventative and corrective standpoints).
 *
 * Adding to the existing PCM task which exelwtes WAR routines at every
 * pre-determined interval (Periodic PCM Task), this PCM task is for WARs
 * to occur on request. (Event-based PCM Task) This was created to handle
 * WAR routines that are seldom called and don't need to live in resident
 * section.
 *
 * @section PCMEVT Task Overlay Strategy
 *
 * PCMEVT provides regular, periodic work-item callback functionality.  These
 * periodic callbacks can run with very small sampling periods (on the order of
 * 100us), which means that the callback code will often need to be resident in
 * IMEM.  This often IMEM residency can cause thrashing with other tasks,
 * especially those associated with power management which suspend DMA.  Thus,
 * the goal of the PCMEVT task is to minimize its high-frequency code size
 * footprint.
 *
 * To accomplish this, the PCMEVT code is split into the following overlays:
 *
 * 1. pcmevt - Main pcmevt task code.  The barebones code to run the task.  This
 * overlay should be tiny.
 *
 * 2. pcmevtLibHighFreq - Overlay for code which exelwtes at high frequency
 * (period on order of 10ms or less).  This is mainly the actual work-item
 * callback functions.  Size of this overlay should be kept as small as possible.
 *
 * 3. pcmevtLibLowFreq - Overlay for code which exelwtes at low frequency.  This
 * is mainly all the work-item management code (enabling, disabling, pausing,
 * etc.) as well as low-frequency callbacks.  Controlling the size of this
 * overlay is much less critical.
 */

/* ------------------------- System Includes ------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes -------------------------- */
#include "main.h"
#include "main_init_api.h"
#include "cmdmgmt/cmdmgmt.h"
#include "pmu_objpcm.h"
#include "pmu_objpmu.h"
#include "dbgprintf.h"
#include "clk/pmu_clkcntr.h"

#include "g_pmurpc.h"

/* ------------------------- Types Definitions ----------------------------- */
/* ------------------------- Defines --------------------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/*!
 * OS timer callback manager used to manage both incoming commands from the
 * queue and relwrring callbacks needed to process and enabled PCM work-items.
 */
#if (!PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))
OS_TIMER PcmOsTimer;
#endif

/* ------------------------- Prototypes ------------------------------------ */
static void s_pcmevtInit(void)
    GCC_ATTRIB_SECTION("imem_init", "s_pcmevtInit");
lwrtosTASK_FUNCTION(task_pcmEvent, pvParameters);

/* ------------------------- Public Functions ------------------------------ */
/*!
 * @brief      Create and initialize the PCM task.
 *
 * @return     FLCN_OK on success
 * @return     descriptive error code otherwise
 */
FLCN_STATUS
pcmPreInitTask(void)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       queueSize = 2U;

    OSTASK_CREATE(status, PMU, PCMEVT);
    if (status != FLCN_OK)
    {
        goto pcmPreInitTask_exit;
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))
    {
        PMU_HALT();
    }

    status = lwrtosQueueCreateOvlRes(&LWOS_QUEUE(PMU, PCMEVT),
                                     queueSize, sizeof(DISP2UNIT_RM_RPC));
    if (status != FLCN_OK)
    {
        goto pcmPreInitTask_exit;
    }

    s_pcmevtInit();

pcmPreInitTask_exit:
    return status;
}

/*!
 * Entry point for the PCM Event task. This task does not receive any
 * parameters and ever returns.
 */
lwrtosTASK_FUNCTION(task_pcmEvent, pvParameters)
{
    DISP2UNIT_RM_RPC dispatch;

    //
    // Infinite-loop to process all incoming commands and relwrring timer
    // callbacks.
    //
#if (PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))
    PMU_HALT();
#else
    for (;;)
    {
        OS_TIMER_WAIT_FOR_QUEUE_OR_CALLBACK_START(
            &PcmOsTimer, LWOS_QUEUE(PMU, PCMEVT), &dispatch, lwrtosMAX_DELAY);
        {
            lwosNOP();
        }
        OS_TIMER_WAIT_FOR_QUEUE_OR_CALLBACK_STOP(&PcmOsTimer, lwrtosMAX_DELAY);
    }
#endif
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * Initilaizes PCM Event task SW state.
 */
static void
s_pcmevtInit(void)
{

#if (!PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))
    extern LwrtosTaskHandle OsTaskPcmEvent;

    osTimerInitTracked(OSTASK_TCB(PCMEVT), &PcmOsTimer,
                       PCM_OS_TIMER_ENTRY_NUM_ENTRIES);
#endif

    // Schedule the callback for clock counter feature.
    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CNTR_PCM_CALLBACK))
    {
        clkCntrSchedPcmCallback();
    }
}
