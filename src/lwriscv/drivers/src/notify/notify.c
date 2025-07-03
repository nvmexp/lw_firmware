/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
/* ------------------------ LW Includes ------------------------------------ */
#include <lwmisc.h>
#include <lwtypes.h>

/* ------------------------ SafeRTOS Includes ------------------------------ */
#include <lwrtos.h>
#include <sections.h>
#include <engine.h>

#include <dev_ctrl.h>

/* ------------------------ Module Includes -------------------------------- */
#include <lwriscv/print.h>
#include "drivers/drivers.h"
#include "drivers/mpu.h"

//------------------------------------------------------------------------------
// External symbols
//------------------------------------------------------------------------------

//
// This variable is used by SafeRTOS to notify us that some task should preempt us.
//
extern volatile LwBool IcCtxSwitchReq;

//------------------------------------------------------------------------------
// Variables
//------------------------------------------------------------------------------

// Just to limit mem footprint, max number of entities that can register notify
// (task can register only one, and that thing ammends it)
#define MAX_HOOKS_TASK              10
#define IRQ_MASK_ALL                (~0U)

// Task notifications
// Notifications are sent either via custom message (if queue is set)
// or via notification mechanism (if queue is null)
typedef struct
{
    LwrtosTaskHandle task;
    LwrtosQueueHandle queue; // if not set, then notification is sent via
    LwU64 groupMask;
    void *pMsg; // pointer to message in USER address space, will be sent to queue
    LwU32 msgSize; // size of message
    LwU32 flags;
} NotifyEntryTask;

#define NOTIFY_FLAG_MORE_WORK       0x1 // more work for task pending
#define NOTIFY_FLAG_MUTED           0x2 // task muted to avoid irq storm

// mask for leaves allocated by various entities
// It can be used to enable/disable interrupts in bulk
sysKERNEL_DATA static LwU64 assignedGroups;

sysKERNEL_DATA static NotifyEntryTask notifyTasks[MAX_HOOKS_TASK];

// We assume we can use LwU64 as masks in interface for now
ct_assert(LW_CTRL_GSP_NOTIFY_TOP__SIZE_1 == 2);

//------------------------------------------------------------------------------
// Code
//------------------------------------------------------------------------------
static sysKERNEL_CODE NotifyEntryTask *notifyFindTask(LwrtosTaskHandle task)
{
    int i;

    if (task == NULL)
    {
        return NULL;
    }

    for (i = 0; i < MAX_HOOKS_TASK; ++i)
    {
        if (notifyTasks[i].task == task)
        {
            return &notifyTasks[i];
        }
    }

    return NULL;
}

static sysKERNEL_CODE LwU64 notifyQueryTop(void)
{
    LwU64 res;

    res = bar0Read(LW_CTRL_GSP_NOTIFY_TOP(1));
    res <<= 32;
    res |= bar0Read(LW_CTRL_GSP_NOTIFY_TOP(0));

    return res;
}

static FLCN_STATUS notifySendNotification(NotifyEntryTask *pEntry)
{
    FLCN_STATUS ret;
    LwBool bIsLwrrentTask;

    if (pEntry == NULL)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // task will get notification, mute it
    pEntry->flags |= NOTIFY_FLAG_MUTED;

    // Also at moment there is no more work (because we just notified it)
    pEntry->flags &= ~NOTIFY_FLAG_MORE_WORK;

    // dispatch message
    if (pEntry->queue) // dispatch to queue
    {
        // We have to map / unmap task if it's not current
        bIsLwrrentTask = (pEntry->task == lwrtosTaskGetLwrrentTaskHandle());
        if (!bIsLwrrentTask)
        {
            mpuTaskMap(pEntry->task);
        }

        // MK: TODO: check if queue (still) belongs to task - we can't do it now

        dbgPrintf(LEVEL_TRACE, "Dispatching irq to task %p queue %p\n",
                  pEntry->task, pEntry->queue);
        ret = lwrtosQueueSendFromISRWithStatus(pEntry->queue, pEntry->pMsg,
                                               pEntry->msgSize);
        if (ret != FLCN_OK)
        {
            // Remove handler
            dbgPrintf(LEVEL_ERROR,
                      "Failed to notify task %p queue %p: %x. Removing task notifications. \n",
                      pEntry->task, pEntry->queue, ret);
            notifyUnregisterTaskNotifier(pEntry->task);
        }

        // Note: can task handle changes if queue requests context switch?
        if (!bIsLwrrentTask)
        {
            mpuTaskUnmap(pEntry->task);
        }
    }
    else // dispatch via notification
    {
        LwBool csrq;

        dbgPrintf(LEVEL_TRACE, "Dispatching irq to task %p\n", pEntry->task);
        ret = lwrtosTaskNotifySendFromISR(pEntry->task,
                                          lwrtosTaskNOTIFICATION_SET_BITS,
                                          0x1,
                                          &csrq);
        if (ret != FLCN_OK)
        {
            // Remove handler
            dbgPrintf(LEVEL_ERROR,
                      "Failed to notify task %p queue %p. Removing task notifications. \n",
                      pEntry->task, pEntry->queue);
            notifyUnregisterTaskNotifier(pEntry->task);
        }

        IcCtxSwitchReq = csrq;
    }

    return ret;
}


sysKERNEL_CODE
void notifyInit(LwU64 kernelReservedGroups)
{
    int i;

    // Disable and zero all interrupts at notify tree
    for (i=0; i<LW_CTRL_GSP_NOTIFY_LEAF_EN_CLEAR__SIZE_1; ++i)
    {
        bar0Write(LW_CTRL_GSP_NOTIFY_LEAF_EN_CLEAR(i), IRQ_MASK_ALL);
        bar0Write(LW_CTRL_GSP_NOTIFY_LEAF(i), IRQ_MASK_ALL);
    }

    // enable notifications
    // MK TODO: this probably should not happen under normal cirlwmstance
    bar0Write(LW_CTRL_GSP_DOORBELL_CFG(0), (1U << 31) | 0xFFF);

    // allocate / reserve kernel groups (if any)
    // MK TODO: we need a bit of code to support kernel side notifications
    assignedGroups = kernelReservedGroups;

    memset(notifyTasks, 0, sizeof(notifyTasks));

    // Enable extio line
    bar0Write(LW_PGSP_MISC_EXTIO_IRQMSET, bar0Read(LW_PGSP_MISC_EXTIO_IRQMSET) |
              0x8);

    dbgPrintf(LEVEL_INFO, "Notify initialized.\n");
}

sysKERNEL_CODE void notifyExit(void)
{
    int i;

    // clear all asssigned interrupts
    for (i=0; i<64; ++i)
    {
        if (assignedGroups & (1ULL << i))
        {
            bar0Write(LW_CTRL_GSP_NOTIFY_LEAF_EN_CLEAR(2*i), IRQ_MASK_ALL);
            bar0Write(LW_CTRL_GSP_NOTIFY_LEAF_EN_CLEAR(2*i + 1), IRQ_MASK_ALL);
        }
    }
}

sysKERNEL_CODE void
notifyIrqHandlerHook(void)
{
    LwU64 intr_top;
    int i;

    intr_top = notifyQueryTop();

    dbgPrintf(LEVEL_TRACE, "Notify irq... 0x%llx\n", intr_top);

    while (intr_top)
    {
        // this should never happen
        if (intr_top & (~assignedGroups))
        {
            dbgPrintf(LEVEL_CRIT,
                      "Notify irq received on group that was not assigned.\n");
            appHalt();
            return;
        }

        // check task dispatches
        for (i=0; i<MAX_HOOKS_TASK; ++i)
        {
            NotifyEntryTask *pEntry = &notifyTasks[i];

            // No task
            if (pEntry->task == NULL)
            {
                continue;
            }

            // No interrupt for given task
            if ((pEntry->groupMask & intr_top) == 0)
            {
                continue;
            }

            // Mask *all* active interrupts for this task
            notifyDisableIrq(pEntry->task, (pEntry->groupMask & intr_top), ~0ULL);

            // Don't send notification if task is muted
            if (pEntry->flags & NOTIFY_FLAG_MUTED)
            {
                pEntry->flags |= NOTIFY_FLAG_MORE_WORK;
                continue;
            }

            notifySendNotification(pEntry);
        }

        // clear notification irq, poll interrupt state
        bar0Write(LW_PGSP_MISC_EXTIO_IRQSCLR, 0x8);

        // More interrupts?
        intr_top = notifyQueryTop();
    }
}

sysKERNEL_CODE FLCN_STATUS
notifyRegisterTaskNotifier(LwrtosTaskHandle task, LwU64 groupMask,
                           LwrtosQueueHandle queue, void *pMsg, LwU64 msgSize)
{
    int i;
    NotifyEntryTask *pEntry = NULL;

    if (task == NULL)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // already registred
    if (notifyFindTask(task) != NULL)
    {
        return FLCN_ERR_ILWALID_STATE;
    }

    // cant share the same group
    if (assignedGroups & groupMask)
    {
        return FLCN_ERR_ILWALID_STATE;
    }

    for (i=0; i<MAX_HOOKS_TASK; ++i)
    {
        if (notifyTasks[i].task == NULL)
        {
            pEntry = &notifyTasks[i];
            break;
        }
    }

    if (!pEntry)
    {
        return FLCN_ERR_NO_FREE_MEM;
    }

    pEntry->task = task;
    // MK TODO: validate queue (and check if it belongs to task) once
    // RTOS permits that.
    pEntry->queue = queue;
    pEntry->groupMask = groupMask;
    pEntry->pMsg = pMsg;
    pEntry->msgSize = msgSize;

    // Lock groups
    assignedGroups |= groupMask;

    dbgPrintf(LEVEL_DEBUG,
              "Registred task notifier, task %p queue %p mask %llx\n", task,
              queue, groupMask);

    return FLCN_OK;
}

sysKERNEL_CODE FLCN_STATUS notifyUnregisterTaskNotifier(LwrtosTaskHandle task)
{
    NotifyEntryTask *pEntry = notifyFindTask(task);

    if (!pEntry)
    {
        return FLCN_ERR_ILWALID_STATE;
    }

    notifyReleaseGroup(task, pEntry->groupMask);
    pEntry->task = NULL;

    return FLCN_OK;
}

FLCN_STATUS notifyProcessingDone(LwrtosTaskHandle task)
{
    NotifyEntryTask *pEntry = notifyFindTask(task);

    if (pEntry == NULL)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    pEntry->flags &= ~NOTIFY_FLAG_MUTED;

    if ((pEntry->flags & NOTIFY_FLAG_MORE_WORK))
    {
        return notifySendNotification(pEntry);
    }
    return FLCN_OK;
}

sysKERNEL_CODE LwU64 notifyQueryIrqs(LwrtosTaskHandle task, LwU8 groupNo)
{
    NotifyEntryTask *pEntry = notifyFindTask(task);
    LwU64 result = 0;

    if (pEntry == NULL)
    {
        return 0;
    }

    if (groupNo >= 64)
    {
        return 0;
    }

    // no peeking at others permitted
    if ((pEntry->groupMask & (1ULL << groupNo)) == 0)
    {
        return 0;
    }

    result = bar0Read(LW_CTRL_GSP_NOTIFY_LEAF(2 * groupNo + 1));
    result <<= 32;
    result |= bar0Read(LW_CTRL_GSP_NOTIFY_LEAF(2 * groupNo));

    return result;
}

sysKERNEL_CODE LwU64 notifyQueryGroups(LwrtosTaskHandle task)
{
    NotifyEntryTask *pEntry = notifyFindTask(task);
    LwU64 activeGroups = 0, groupMask;
    int i;

    if ((pEntry == NULL) || (pEntry->groupMask == 0))
    {
        return 0;
    }

    groupMask = pEntry->groupMask;

    for (i=0; i<64; ++i)
    {
        if ((groupMask & 0x1) == 0)
        {
            groupMask >>= 1;
            continue;
        }

        if (groupMask & (1ULL << i))
        {
            if ((bar0Read(LW_CTRL_GSP_NOTIFY_LEAF(2*i)) != 0) ||
                (bar0Read(LW_CTRL_GSP_NOTIFY_LEAF(2*i + 1)) != 0))
            {
                activeGroups |= (1ULL << i);
            }
        }
    }

    return activeGroups;
}


sysKERNEL_CODE FLCN_STATUS
notifyRequestGroup(LwrtosTaskHandle task, LwU64 groupMask)
{
    NotifyEntryTask *pEntry = notifyFindTask(task);

    if (pEntry == NULL)
    {
        return FLCN_ERR_ILWALID_STATE;
    }

    // remove stuff we already own
    groupMask = groupMask & (~pEntry->groupMask);

    // check if we can take ownership
    if (groupMask & assignedGroups)
    {
        return FLCN_ERR_NO_FREE_MEM;
    }

    assignedGroups |= groupMask;
    pEntry->groupMask |= groupMask;

    return FLCN_OK;
}

sysKERNEL_CODE FLCN_STATUS
notifyReleaseGroup(LwrtosTaskHandle task, LwU64 groupMask)
{
    NotifyEntryTask *pEntry = notifyFindTask(task);

    if (pEntry == NULL)
    {
        return FLCN_ERR_ILWALID_STATE;
    }

    // we can't release bits that weren't set by us
    if (groupMask & (~pEntry->groupMask))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // bits to clear
    groupMask &= pEntry->groupMask;

    notifyDisableIrq(task, groupMask, 0xFFFFFFFFFFFFFFFFULL);
    assignedGroups = assignedGroups & (~groupMask);
    pEntry->groupMask = pEntry->groupMask & (~groupMask);

    return FLCN_OK;
}

sysKERNEL_CODE FLCN_STATUS
notifyEnableIrq(LwrtosTaskHandle task, LwU64 groupMask, LwU64 irqMask)
{
    NotifyEntryTask *pEntry = notifyFindTask(task);
    int i = 0;

    if (pEntry == NULL)
    {
        return FLCN_ERR_ILWALID_STATE;
    }

    if (groupMask & (~pEntry->groupMask))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    if (irqMask == 0)
    {
        return FLCN_OK;
    }

    for (i=0; i<64; ++i)
    {
        if (groupMask & (1ULL << i))
        {
            bar0Write(LW_CTRL_GSP_NOTIFY_LEAF_EN_SET(2*i), LwU64_LO32(irqMask));
            bar0Write(LW_CTRL_GSP_NOTIFY_LEAF_EN_SET(2*i + 1), LwU64_HI32(irqMask));
        }
    }

    return FLCN_OK;
}

sysKERNEL_CODE FLCN_STATUS
notifyDisableIrq(LwrtosTaskHandle task, LwU64 groupMask, LwU64 irqMask)
{
    NotifyEntryTask *pEntry = notifyFindTask(task);
    int i = 0;

    if (pEntry == NULL)
    {
        return FLCN_ERR_ILWALID_STATE;
    }

    if (groupMask & (~pEntry->groupMask))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    if (irqMask == 0)
    {
        return FLCN_OK;
    }

    for (i=0; i<64; ++i)
    {
        if (groupMask & (1ULL << i))
        {
            bar0Write(LW_CTRL_GSP_NOTIFY_LEAF_EN_CLEAR(2*i), LwU64_LO32(irqMask));
            bar0Write(LW_CTRL_GSP_NOTIFY_LEAF_EN_CLEAR(2*i + 1), LwU64_HI32(irqMask));
        }
    }

    return FLCN_OK;
}

sysKERNEL_CODE FLCN_STATUS
notifyAckIrq(LwrtosTaskHandle task,LwU64 groupMask, LwU64 irqMask)
{
    NotifyEntryTask *pEntry = notifyFindTask(task);
    int i = 0;

    if (pEntry == NULL)
    {
        return FLCN_ERR_ILWALID_STATE;
    }

    if (groupMask & (~pEntry->groupMask))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    if (irqMask == 0)
    {
        return FLCN_OK;
    }

    for (i=0; i<64; ++i)
    {
        if (groupMask & (1ULL << i))
        {
            bar0Write(LW_CTRL_GSP_NOTIFY_LEAF(2*i), LwU64_LO32(irqMask));
            bar0Write(LW_CTRL_GSP_NOTIFY_LEAF(2*i + 1), LwU64_HI32(irqMask));
        }
    }

    return FLCN_OK;
}
