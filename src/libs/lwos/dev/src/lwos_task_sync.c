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
 * @file    lwos_task_sync.c
 * @details TODO.
 */

/* ------------------------ System Includes --------------------------------- */
#include "lwoslayer.h"
#include "lwos_task_sync.h"

/* ------------------------ Types definitions ------------------------------- */
/* ------------------------ Macro definitions ------------------------------- */
/* ------------------------ External definitions ---------------------------- */
/* ------------------------ Static variables -------------------------------- */
/*!
 * @brief   Number of RTOS tasks that are yet to complete the post-init code.
 */
static LwU8 LwosTaskSyncCount = 0;

/*!
 * @brief   Mutex to wait on until all tasks have completed the post-init code.
 */
static LwrtosSemaphoreHandle LwosTaskSyncMutex = NULL;

/* ------------------------ Public variables -------------------------------- */
/* ------------------------ Static Function Prototypes  --------------------- */
/* ------------------------ Public Functions  ------------------------------- */
/*!
 * @brief   Initialize the task-sync feature.
 *
 * @return  FLCN_OK                 On success.
 * @return  FLCN_ERR_NO_FREE_MEM    On failure to allocate remaphore.
 */
FLCN_STATUS
lwosTaskSyncInitialize(void)
{
    FLCN_STATUS status = FLCN_OK;

    lwrtosSemaphoreCreateBinaryTaken(&LwosTaskSyncMutex, OVL_INDEX_OS_HEAP);
    if (LwosTaskSyncMutex == NULL)
    {
        status = FLCN_ERR_NO_FREE_MEM;
        OS_BREAKPOINT();
    }

    return status;
}

/*!
 * @brief   Updates the task-sync feature's state on task creation.
 *
 * @return  FLCN_OK                 On success.
 * @return  FLCN_ERR_ILWALID_STATE  On detection of a potential overflow.
 */
FLCN_STATUS
lwosTaskCreated(void)
{
    FLCN_STATUS status = FLCN_OK;

    if (LwosTaskSyncCount < LW_U8_MAX)
    {
        // Critical section not required since scheduler is not running.
        LwosTaskSyncCount++;
    }
    else
    {
        status = FLCN_ERR_ILWALID_STATE;
        OS_BREAKPOINT();
    }

    return status;
}

/*!
 * @brief   Updates the task-sync feature's state on task init completion.
 */
void
lwosTaskInitDone(void)
{
    FLCN_STATUS status = FLCN_OK;
    LwBool      bGive  = LW_FALSE;

    // Critical section required since scheduler is running.
    lwrtosENTER_CRITICAL();
    {
        if (LwosTaskSyncCount == 0)
        {
            status = FLCN_ERR_ILWALID_STATE;
            goto lwosTaskInitDone_exit;
        }
        LwosTaskSyncCount--;
        if (LwosTaskSyncCount == 0)
        {
            bGive = LW_TRUE;
        }

lwosTaskInitDone_exit:
        lwosNOP();
    }
    lwrtosEXIT_CRITICAL();

    if (bGive)
    {
        status = lwrtosSemaphoreGive(LwosTaskSyncMutex);
    }

    if (status != FLCN_OK)
    {
        //
        // No error propagation from here. We will not carry problem into
        // the task's main loop. NJ-TODO: Report an error globaly.
        //
        OS_HALT();
    }
}

/*!
 * @brief   Waits until all tasks report that the initialization is done.
 *
 * @return  FLCN_OK     Always succeeds.
 */
FLCN_STATUS
lwosTaskInitWaitAll(void)
{
    // Account for the IDLE task.
    lwosTaskInitDone();

    // Account for the task that waits (CMDMGMT).
    lwosTaskInitDone();

    //
    // NJ-TODO: We do not want to wait forever. Change this to a meaningfull
    //          timeout. Also do not wait if some task has failed allready.
    //
    lwrtosSemaphoreTakeWaitForever(LwosTaskSyncMutex);

    // Allow waiting task to normally issue @ref lwosTaskInitDone.
    LwosTaskSyncCount++;

    // For now just succeeds.
    return FLCN_OK;
}

/* ------------------------ Private Functions ------------------------------- */
