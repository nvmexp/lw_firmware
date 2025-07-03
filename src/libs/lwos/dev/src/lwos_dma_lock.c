/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   lwos_dma_lock.c
 * @brief  Library providing DMA lock / suspension control.
 */

/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include "lwoslayer.h"
#include "lwos_ovl_priv.h"
#include "lwrtosHooks.h"
#ifdef UPROC_RISCV
#include <syslib/syslib.h>
#endif //defined(UPROC_RISCV)

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * @brief Check if DMA is locked
 *
 * Deny DMA suspension requests if DMA is locked.
 * @sa lwosDmaLockAcquire() and lwosDmaLockRelease().
 *
 * Other notes:
 * OS or OS API should use this macro. Tasks should not use it.
 */
#define LWOS_DMA_IS_LOCKED()        (LwosDmaLock != 0)

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Global Variables ------------------------------- */
#ifdef DMA_SUSPENSION
/*!
 * Counter used to track the number of DMA suspension requests lwrrently issued
 * from the application-layer (when DMA-suspension is enabled). A non-zero
 * value indicates that DMA is suspended and specifically that the scheduler
 * may not issue DMA requests for context- switching.
 */
volatile LwU32  LwosDmaSuspend            = 0;
/*!
 * Keeps track of when the application has been sent a DMA suspension notice
 * for each DMA-suspension cycle (LW_FALSE->LW_TRUE->LW_FALSE). Used to prevent
 * the application from being sent a flood of redundant notifications.
 */
volatile LwBool LwosBDmaSuspendNoticeSent = LW_FALSE;
#endif // DMA_SUSPENSION
/*!
 * Counter used to track the number of DMA lock requests lwrrently issued
 * by OS (when DMA-locking is enabled). A non-zero value indicates that DMA
 * is locked and applications can't suspend DMA.
 */
UPROC_STATIC volatile LwU32  LwosDmaLock        = 0;

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
#ifdef DMA_SUSPENSION
/*!
 * Tasks may call this function to inform the OS that DMA operations should be
 * suspended until further notice. This will prevent the OS from context-
 * switching to any tasks which are not already loaded into IMEM. It does NOT
 * suspend context-switching, just DMA.
 *
 * @param[in] bIgnoreLock
 *      !LW_FALSE - Suspend without checking DMA lock state. i.e. Ignore DMA
 *                  lock state.
 *      LW_FALSE  - Suspend only if DMA is not locked.
 *      Refer @lwosDmaLockAcquire() for DMA locking.
 *
 * @pre It is critically important that tasks raise their priority above that of
 *      all other overlay-tasks before calling this function. The application
 *      should provide a priority level that is reserved exclusively for a task
 *      to live in while it has suspended DMA. There must be one and only one
 *      task at this priority level at any given time.
 *
 * Other notes:
 *     -# Though not advised, nested calls to this function are supported. The
 *        only requirement is that calls to this function are perfectly
 *        balanced with later calls to @ref lwosDmaResume. Failure to call the
 *        resume function the exact number of times this function is called
 *        will result in DMA being indefinitely suspended.
 *
 *     -# Though not advised, task can choose to ignore DMA lock provided task
 *        is ready to ignore current DMA transactions going in progress.
 *
 *     -# Suspending DMA will not abort already queued transactions into
 *        Falcon's HW queue. It only ensures that Task and RTOS can't queue new
 *        DMA transactions.
 *
 *     -# It is recommended that the application also implement the DMA
 *        suspension notice to allow it be informed when a context-switch
 *        fails due to suspended DMA. This notice allows the application
 *        to correct the situation and prevent starvation.
 *
 * @return  LW_TRUE  - DMA suspension was successful.
 * @return  LW_FALSE - DMA suspension failed as DMA is locked.
 *
 * @sa lwosDmaResume
 */
LwBool
lwosDmaSuspend(LwBool bIgnoreLock)
{
    LwBool bReturn = LW_TRUE;

    lwrtosENTER_CRITICAL();
    {
        //
        // Fail DMA suspension if -
        // 1) Call don't want to ignore DMA lock state AND
        // 2) DMA is locked
        //
        if (!bIgnoreLock && LWOS_DMA_IS_LOCKED())
        {
            bReturn = LW_FALSE;
        }
        else
        {
            //
            // Ensure that new suspension notice is sent since
            // DMA is getting suspened now, so notice flag
            // should be reset now.
            //
            if (LwosDmaSuspend == 0U)
            {
                LwosBDmaSuspendNoticeSent = LW_FALSE;
#ifdef UPROC_RISCV
                sysFbhubGated(LW_TRUE);
#endif //defined(UPROC_RISCV)
            }
            LwosDmaSuspend++;
        }
    }
    lwrtosEXIT_CRITICAL();

    return bReturn;
}

/*!
 * Counterpart to @ref lwosDmaSuspend. Called when it becomes safe again for
 * the OS to perform DMA. Task should restore their original priority just
 * after calling this function. Calling this function, combined with the call
 * to lower the task priority should be done atomically (i.e. in a
 * critical-section).
 *
 * @sa lwosDmaSuspend
 */
void
lwosDmaResume(void)
{
    lwrtosENTER_CRITICAL();
    {
        if (LwosDmaSuspend == 0U)
        {
            // Prevent LwosDmaSuspend underflow
            OS_HALT();
        }

        LwosDmaSuspend--;
        //
        // Ensure a new suspension notice is sent the next time DMA is
        // suspended after fully resuming DMA (1->0).
        //
        if (LwosDmaSuspend == 0U)
        {
            LwosBDmaSuspendNoticeSent = LW_FALSE;
#ifdef UPROC_RISCV
            sysFbhubGated(LW_FALSE);
#endif //defined(UPROC_RISCV)
        }
    }
    lwrtosEXIT_CRITICAL();

    //
    // It should not be necessary to yield here as the calling task should
    // still be at a priority which is higher than all other overlay tasks.
    // The yield is also performed as part of @ref vTaskPrioritySet.
    //
}

LwBool
lwosDmaIsSuspended(void)
{
    return (LwosDmaSuspend != 0);
}

LwBool
lwosDmaSuspensionNoticeISR(void)
{
    LwBool bSuspended = lwosDmaIsSuspended();

    if (bSuspended)
    {
        vApplicationDmaSuspensionNoticeISR();
    }

    return bSuspended;
}

/*!
 * Tasks may call this function to inform the OS that they need to perform a DMA
 * operation. This will cause the OS to context-switch to schedule whatever high
 * priority task suspended the DMA.
 *
 * This function needs to be called from inside a critical section.
 */
void
lwosDmaExitSuspension(void)
{
    if (lwosDmaIsSuspended())
    {
        vApplicationDmaSuspensionNoticeISR();

        //
        // Exit critical section. Tasks who has suspended DMA needs
        // to process DMA exception.
        //
        lwrtosEXIT_CRITICAL();

        while (lwosDmaIsSuspended())
        {
            lwrtosYIELD();
        }

        // Enter back into critical section.
        lwrtosENTER_CRITICAL();
    }
}
#endif // DMA_SUSPENSION

/*!
 * @brief Lock DMA to avoid DMA suspensions
 *
 * OS and DMA APIs calls this function to make sure that OS will own DMA until
 * further notice. Locking DMA means tasks can't suspend DMA. This is typical
 * sequence to do DMA -
 * 1) Task issued DMA transaction via DMA APIs(DMEM<->FB/SYSMEM)
 *    OR
 *    OS issued DMA transaction (OS is loading task/overlay into IMEM)
 * 2) Acquires DMA lock.
 * 3) If DMA is suspended
 * 3.1) Raises DMA_SUSPEND exception.
 * 3.2) Task who has suspended DMA will service DMA_EXCEPTION. Task will resume
 *      DMA as part of servicing this exception. No task can suspend DMA until
 *      DMA lock is released (Step5).
 * 4) Complete DMA transaction
 * 5) Release DMA Lock. Now tasks can suspend DMA.
 *
 * Other notes:
 * This function should be called only by OS or other OS API. Tasks should not
 * directly call this API.
 *
 * @sa lwosDmaLockRelease
 */
void
lwosDmaLockAcquire_IMPL(void)
{
    lwrtosENTER_CRITICAL();
    if (LwosDmaLock < LW_U32_MAX)
    {
        LwosDmaLock++;
    }
    else
    {
#ifdef SAFERTOS
        lwrtosHookError(lwrtosTaskGetLwrrentTaskHandle(), (LwS32) FLCN_ERR_ILLEGAL_OPERATION);
#else //SAFERTOS
        OS_HALT();
#endif //SAFERTOS
    }
    lwrtosEXIT_CRITICAL();
}

/*!
 * @brief Release DMA lock
 *
 * DMA APIs calls this function to release lock. Tasks can suspend DMA after
 * releasing lock.
 *
 * Other notes:
 * This function should be called only by OS or other OS API. Tasks should not
 * directly call this API.
 *
 * @sa lwosDmaLockAcquire
 */
void
lwosDmaLockRelease_IMPL(void)
{
    lwrtosENTER_CRITICAL();
    if (LwosDmaLock > 0U)
    {
        LwosDmaLock--;
    }
    else
    {
#ifdef SAFERTOS
        lwrtosHookError(lwrtosTaskGetLwrrentTaskHandle(), (LwS32) FLCN_ERR_ILLEGAL_OPERATION);
#else //SAFERTOS
        OS_HALT();
#endif //SAFERTOS
    }
    lwrtosEXIT_CRITICAL();
}

/* ------------------------ Static Functions  ------------------------------- */

