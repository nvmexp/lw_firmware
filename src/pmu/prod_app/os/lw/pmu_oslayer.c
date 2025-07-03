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
 * @file  pmu_oslayer.c
 * @brief Top-Level interfaces between the PMU application layer and the RTOS
 *
 * This module is intended to serve as the top-level interface layer for dealing
 * with all application-specific aspects of the OS. This would include handling
 * exceptions that the OS could not handle or even a custom idle-hook could be
 * installed. In it's current state, it is simply provides a hook for unhandled
 * exceptions to save some non-PRIV accessible state before halting.
 *
 * Note: All data-types in this module should be consistent with those used
 *       in the OS.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_pwr_csb.h"
#include "pmu/pmumailboxid.h"
#include "lwrtosHooks.h"
#include "lwrtos.h"
#include "lwos_instrumentation.h"
#include "lwos_ustreamer.h"

#ifdef UPROC_RISCV
#include "drivers/drivers.h"
#include "syslib/syslib.h"
#include "lwriscv/print.h"
#endif // UPROC_RISCV

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "pmu_objpmu.h"
#include "pmu_objlpwr.h"
#include "pmu_oslayer.h"
#include "pmu_objgcx.h"
#include "pmu_objlsf.h"
#include "config/g_ic_private.h"
#include "g_pmurpc.h"
#include "perf/cf/perf_cf.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */

#if defined(DMA_SUSPENSION) && defined(ON_DEMAND_PAGING_BLK)
extern LwrtosTaskHandle OsTaskIdle;
extern LwrtosTaskHandle OsTaskLpwr;
#endif // if defined(DMA_SUSPENSION) && defined(ON_DEMAND_PAGING_BLK)

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * @brief   Interface providing per-build stack size of idle task.
 *
 * @return  Idle task's stack size.
 */
LwU16
vApplicationIdleTaskStackSize(void)
{
    return OSTASK_ATTRIBUTE_IDLE_STACK_SIZE / 4;
}

/*!
 * @brief Called by the RTOS repeatedly from within the IDLE task.
 *
 * Code added to this hook is guaranteed to run continuously (at low-priorty)
 * during periods of idleness. The following constraints must be meet when
 * adding to this function:
 * 1. Do NOT call any functions that could cause the IDLE task to block.
 * 2. Do NOT code with infinite-loops. Though this is lwrrently permissible,
 *    it could become an issue if task-deletion ever becomes necessary.
 */
void
lwrtosHookIdle(void)
{
    if (LWOS_INSTR_IS_ENABLED())
    {
        LWOS_INSTRUMENTATION_FLUSH();
    }

#ifdef USTREAMER
    if (LWOS_USTREAMER_IS_ENABLED())
    {
        lwosUstreamerIdleHook();
    }
#endif

#if PMUCFG_FEATURE_ENABLED(PMU_PG_TASK_MGMT)
    if (lpwrNotifyRtosIdle())
    {
        //
        // Notification send to LPWR task, thus trigger lwrtosYIELD().
        // Skip falc_wait for this iteration.
        //
        lwrtosYIELD();
    }
    else
#endif
    {
#ifdef UPROC_RISCV
        sysWfi();
#else // UPROC_RISCV
        falc_ssetb_i(0); // set CSW F0 flags
        falc_wait(0);    // suspend exelwtion until next interrupt
#endif // UPROC_RISCV
    }
}

void
lwrtosHookError
(
    void *pxTask,
    LwS32 xErrorCode
)
{
    //
    // VR/NJ-TODO: Update RM to read the Mailbox register for the error code,
    // and not EXCI.
    //

    // REG_WR32(CSB, LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_EXCI), xErrorCode);
    PMU_HALT();
}

/*!
 * Extra callback function - ucode-specific complement to lwrtosHookTimerTick.
 * Ilwoked from lwrtosHookTimerTick. Only defined for PMU for now.
 */
void
lwosHookTimerTickSpecHandler(LwU32 ticksNow)
{
    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_MEM_TUNE) &&
        ((ticksNow % LWRTOS_TICK_RATE_HZ) == 0U))
    {
        // Validate watchdog timer every second
        perfCfMemTuneWatchdogTimerValidate_HAL();
    }
}

#ifdef DMA_SUSPENSION
/*!
 * Callback function called by the RTOS when it was unable to context-switch to
 * a task due to an active DMA suspension. The call is intended to both inform
 * the application that the context-switch failed and allow it take / schedule
 * the actions necessary to correct the situation and unsuspend/resume DMA.
 */
void
vApplicationDmaSuspensionNoticeISR(void)
{
    extern volatile LwBool LwosBDmaSuspendNoticeSent;

    if (!LwosBDmaSuspendNoticeSent)
    {
        //
        // When deepidle is enabled, we must:
        //    1. Check if we're in deepidle, and if so
        //    2. Send a message to the Deepidle event-queue to request a wake
        //       (use the FromAtomic variation of the queue send routine).
        //
        if (PMUCFG_FEATURE_ENABLED(PMU_GCX_DIDLE) &&
            Gcx.bDmaSuspended)
        {
            DISPATCH_DIDLE  dispatchDidle;

            dispatchDidle.hdr.eventType     = DIDLE_SIGNAL_EVTID;
            dispatchDidle.sigGen.signalType = DIDLE_SIGNAL_ABORT;

            (void)lwrtosQueueIdSendFromAtomic(LWOS_QUEUE_ID(PMU, GCX),
                                              &dispatchDidle, sizeof(dispatchDidle));
        }

        //
        //
        // When MS Group is supported
        //  1. Check if we're in any feature which belong to MS Group and if so
        //  2. Send a message to the LPWR task event-queue to request a wake
        //
        if ((PMUCFG_FEATURE_ENABLED(PMU_PG_MS)                      &&
             pgIsCtrlSupported(RM_PMU_LPWR_CTRL_ID_MS_MSCG)         &&
             !PG_IS_FULL_POWER(RM_PMU_LPWR_CTRL_ID_MS_MSCG))        ||
            (PMUCFG_FEATURE_ENABLED(PMU_LPWR_CTRL_MS_DIFR_SW_ASR)   &&
             pgIsCtrlSupported(RM_PMU_LPWR_CTRL_ID_MS_DIFR_SW_ASR)  &&
             !PG_IS_FULL_POWER(RM_PMU_LPWR_CTRL_ID_MS_DIFR_SW_ASR)) ||
            (PMUCFG_FEATURE_ENABLED(PMU_LPWR_CTRL_MS_LTC)           &&
             pgIsCtrlSupported(RM_PMU_LPWR_CTRL_ID_MS_LTC)          &&
             !PG_IS_FULL_POWER(RM_PMU_LPWR_CTRL_ID_MS_LTC)))
        {
            DISPATCH_LPWR dispatchLpwr;

            // MMINTS-TODO: expand this check to Falcon chips
#ifdef UPROC_RISCV
            LwrtosTaskHandle taskHandle = lwrtosTaskGetLwrrentTaskHandle();

            if ((taskHandle == OsTaskIdle) || (taskHandle == OsTaskLpwr))
            {
                //
                // Requesting a wakeup from DMA suspension when running in
                // LPWR or Idle task is an error. Neither of these tasks
                // should do any DMA if MSCG is enabled.
                //
                dbgPrintf(LEVEL_ALWAYS, "Attempted to request DMA suspension wakeup from IDLE or LPWR task!\n");

                if (lwrtosIS_KERNEL())
                {
                    kernelVerboseCrash(0);
                }
                else
                {
                    PMU_HALT();
                }
            }
#endif // UPROC_RISCV

            dispatchLpwr.hdr.eventType = LPWR_EVENT_ID_MS_ABORT_DMA_SUSP;
            dispatchLpwr.intr.ctrlId = RM_PMU_LPWR_CTRL_ID_MS_MSCG;

            (void)lwrtosQueueIdSendFromAtomic(LWOS_QUEUE_ID(PMU, LPWR),
                                              &dispatchLpwr, sizeof(dispatchLpwr));
        }

        LwosBDmaSuspendNoticeSent = LW_TRUE;
    }
}
#endif

#if defined(DMA_SUSPENSION) && defined(ON_DEMAND_PAGING_BLK)

/*!
 * @brief  DMA-suspension-aware ODP-safe critical section enter.
 *
 * If a page fault happens when DMA is suspended (for example,
 * during MSCG), a context switch to the a task (for example, LPWR)
 * that can re-enable DMA is required to service the page fault.
 *
 * This is undesirable inside a critical section! So, when entering a
 * critical section, we preventively exit DMA suspension.
 * This way, we avoid having the context switch inside the critical
 * section body, which might be very dangerous (since the critical
 * section's atomicity guarantees might be violated)!
 *
 * For fully-resident tasks (IDLE and LPWR), identified by their task handle,
 * this behaves identically to lwrtosTaskCriticalEnter (with a tiny overhead).
 * Care should be taken to avoid doing any DMA/ODP inside critical sections
 * for these tasks.
 */
void
appTaskCriticalEnter_FUNC(void)
{
    LwrtosTaskHandle taskHandle;
    LwU32            priority = lwrtosTASK_PRIORITY_IDLE;

#ifdef UPROC_RISCV
    if (lwrtosIS_KERNEL())
    {
        //
        // appTaskCriticalEnter should only be used from task code.
        // Error out if in kernel-mode.
        //
        PMU_HALT();
    }
#endif // UPROC_RISCV

    taskHandle = lwrtosTaskGetLwrrentTaskHandle();

    //
    // Enter critical section early to prevent context switches while we
    // do DMA suspension checks and prepare to un-suspend DMA if needed.
    //
    lwrtosTaskCriticalEnter();

    // Optimization check: ignore fully-resident tasks
    if ((taskHandle != OsTaskIdle) && (taskHandle != OsTaskLpwr))
    {
        //
        // Acquire DMA lock regardless of whether DMA is suspended right now.
        // It will be held for the entire duration of the critical section.
        // This is to support use-cases that might yield inside a critical
        // section and expect code before and after the yield to still be atomic
        // (so the yield shouldn't be able to cause DMA to become suspended
        // or re-suspended).
        //
        lwosDmaLockAcquire();

        if (lwosDmaIsSuspended())
        {
            //
            // Set priority to one lower than the DMA_SUSPENDED priority,
            // so that we have a shot at getting scheduled back on as soon
            // as DMA is re-enabled!
            //
            lwrtosLwrrentTaskPriorityGet(&priority);
            lwrtosLwrrentTaskPrioritySet(OSTASK_PRIORITY_DMA_SUSPENDED - 1U);

            // Send the notification that we need DMA to be un-suspended
            vApplicationDmaSuspensionNoticeISR();

            // The yield-loop: wait for DMA to be un-suspended.
            while (lwosDmaIsSuspended())
            {
                lwrtosYIELD();
            }

            // At this point, DMA should be enabled!

            // Restore original priority
            lwrtosLwrrentTaskPrioritySet(priority);
        }
    }
}

/*!
 * @brief  DMA-suspension-aware ODP-safe critical section enter.
 *
 * This is the counterpart to @ref appTaskCriticalEnter_FUNC.
 * It releases the DMA lock and exits the critical section.
 */
void
appTaskCriticalExit_FUNC(void)
{
    LwrtosTaskHandle taskHandle;

#ifdef UPROC_RISCV
    if (lwrtosIS_KERNEL())
    {
        //
        // appTaskCriticalExit should only be used from task code.
        // Error out if in kernel-mode.
        //
        PMU_HALT();
    }
#endif // UPROC_RISCV

    taskHandle = lwrtosTaskGetLwrrentTaskHandle();

    // Optimization check: ignore fully-resident tasks
    if ((taskHandle != OsTaskIdle) && (taskHandle != OsTaskLpwr))
    {
        //
        // Release DMA lock; it needs to be held through the whole
        // critical section.
        // This is to support use-cases that might yield inside a critical
        // section and expect code before and after the yield to still be atomic
        // (so the yield shouldn't be able to cause DMA to become suspended
        // or re-suspended).
        //
        lwosDmaLockRelease();
    }

    // Finally, exit critical section
    lwrtosTaskCriticalExit();
}

/*!
 * @brief  DMA-suspension-aware ODP-safe scheduler suspend.
 *
 * This relies on @ref appTaskCriticalEnter_FUNC. It's absolutely necessary
 * to use this in presence of both suspended DMA and ODP.
 *
 * While yielding to service a pagefault inside a critical section can break
 * the critical section's semantics, if it happens while the scheduler is
 * suspended, a deadlock will occur.
 *
 * Making sure DMA is un-suspended before suspending scheduler is absolutely
 * necessary because of this.
 */
void
appTaskSchedulerSuspend_FUNC(void)
{
    appTaskCriticalEnter();
    {
        lwrtosTaskSchedulerSuspend();
    }
    appTaskCriticalExit();
}
#endif // if defined(DMA_SUSPENSION) && defined(ON_DEMAND_PAGING_BLK)

#ifdef DYNAMIC_FLCN_PRIV_LEVEL
/*!
 * Callback function called by the RTOS to set falcon privilege level
 *
 * @param[in]  privLevelExt  falcon EXT privilege level
 * @param[in]  privLevelCsb  falcon CSB privilege level
 */
void
vApplicationFlcnPrivLevelSet
(
    LwU8 privLevelExt,
    LwU8 privLevelCsb
)
{
    if (PMUCFG_FEATURE_ENABLED(PMU_DYNAMIC_FLCN_PRIV_LEVEL))
    {
        pmuFlcnPrivLevelSet_HAL(&Pmu, privLevelExt, privLevelCsb);
    }
}

#ifndef UPROC_RISCV
/*!
 * Callback function called by the RTOS to reset falcon privilege level
 */
void
vApplicationFlcnPrivLevelReset(void)
{
    if (PMUCFG_FEATURE_ENABLED(PMU_DYNAMIC_FLCN_PRIV_LEVEL))
    {
        pmuFlcnPrivLevelSet_HAL(&Pmu, PmuPrivLevelExtDefault,
                                PmuPrivLevelCsbDefault);
    }
}
#endif // UPROC_RISCV

#endif // DYNAMIC_FLCN_PRIV_LEVEL

#ifdef FLCNDBG_ENABLED
/*!
 * Callback function called by the RTOS when a breakpoint is hit in the ucode
 * and a software interrupt is generated. Lwrrently used by falcon debugger.
 */
void
vApplicationIntOnBreak(void)
{
    LwU32 csw = 0;

    // Exception handler, clear CSW.E;
    falc_rspr(csw, CSW);
    falc_wspr(CSW, csw & 0xff7ffff);

    icServiceSwIntr_HAL();
}
#endif // FLCNDBG_ENABLED

#ifdef DMA_REGION_CHECK
/*!
 * Callback function called by the RTOS to check whether an access using the
 * specified dmaIdx is permitted.
 *
 * @param[in]  dmaIdx    DMA index to check
 *
 * @return     bubble up the return value from @ref lsfIsDmaIdxPermitted_HAL
 */
LwBool
vApplicationIsDmaIdxPermitted
(
    LwU8 dmaIdx
)
{
    return lsfIsDmaIdxPermitted_HAL(&(Lsf.hal), dmaIdx);
}
#endif // DMA_REGION_CHECK

#ifdef USTREAMER
/*!
 * @copydoc lwosUstreamerGetQueueCount
 */
LwU8
lwosUstreamerGetQueueCount(void)
{
    //
    // This need to be kept in sync with how many queues are created at maxium.
    // see LW2080_CTRL_FLCN_USTREAMER_FEATURE__COUNT
    //
    return 2;
}
#endif // USTREAMER
