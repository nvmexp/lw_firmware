/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    cmdq.c
 * @brief   Kernel-level driver for hardware command queues.
 *
 * This driver owns queue interrupts and all command/message queue registers.
 * It is assumed that all functions are called within kernel context with
 * interrupts/preemption disabled.
 *
 * MK TODO:
 * - It should be hardened, so only owner can handle their queues
 * - Try to remove SafeRTOS dependency
 * - Make it engine-independent somehow once we need it for SEC2
 * - Check IcCtxSwitchReq (possibly in kernel) and context switch if needed
 */

/* ------------------------ LW Includes ------------------------------------ */
#include <lwmisc.h>
#include <lwtypes.h>

/* ------------------------ SafeRTOS Includes ------------------------------ */
#include <lwrtos.h>
#include <sections.h>
#include <engine.h>
#include <lwriscv/print.h>

/* ------------------------ Module Includes -------------------------------- */
#include "drivers/drivers.h"

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

sysKERNEL_DATA static LwrtosTaskHandle cmdqListeners[ENGINE_REG(_QUEUE_HEAD__SIZE_1)] = { NULL, };

//------------------------------------------------------------------------------
// Code
//------------------------------------------------------------------------------

/**
 * @brief cmdqIrqHandler Interrupt handler for CMD_HEAD_* interrupts
 */
sysKERNEL_CODE void
cmdqIrqHandler(void)
{
    LwU64 cmdqIdx;
    LwU32 cmdqHead    = csbRead(ENGINE_REG(_CMD_HEAD_INTRSTAT));
    LwU32 cmdqIrqMask = csbRead(ENGINE_REG(_CMD_INTREN));

    cmdqHead &= cmdqIrqMask;

    if (cmdqHead == 0)
    {
        return;
    }

    csbWrite(ENGINE_REG(_CMD_HEAD_INTRSTAT), cmdqHead);
    for (cmdqIdx = 0; cmdqIdx < ENGINE_REG(_QUEUE_HEAD__SIZE_1); ++cmdqIdx)
    {
        if (cmdqHead & BIT(cmdqIdx))
        {
            // Mask out that queue - we sent notification, or we don't care about it anyway
            cmdqIrqMask = cmdqIrqMask & ~BIT(cmdqIdx);
            if (cmdqListeners[cmdqIdx] != NULL)
            {
                LwBool csrq;
                FLCN_STATUS ret;

                ret = lwrtosTaskNotifySendFromISR(cmdqListeners[cmdqIdx],
                                                  lwrtosTaskNOTIFICATION_SET_BITS, BIT(cmdqIdx),
                                                  &csrq);
                if (ret != FLCN_OK)
                {
                    dbgPrintf(LEVEL_ERROR,
                              "Failed to notify task. Removing handler for queue %llu\n",
                              cmdqIdx);
                    // Remove handler
                    cmdqListeners[cmdqIdx] = NULL;
                }

                IcCtxSwitchReq = csrq;
            }
        }
    }
    // Mask whatever we handled, must be unmasked manually
    csbWrite(ENGINE_REG(_CMD_INTREN), cmdqIrqMask);
}

sysKERNEL_CODE FLCN_STATUS
cmdqRegisterNotifier(LwU64 cmdqIdx)
{
    if (cmdqIdx >= ENGINE_REG(_QUEUE_HEAD__SIZE_1))
    {
        return FLCN_ERR_ILWALID_INDEX;
    }

    if (cmdqListeners[cmdqIdx] != NULL)
    {
        return FLCN_ERR_NO_FREE_MEM;
    }

    cmdqListeners[cmdqIdx] = lwrtosTaskGetLwrrentTaskHandle();

    dbgPrintf(LEVEL_DEBUG, "Registered queue %llu notifier for task %p\n",
           cmdqIdx, cmdqListeners[cmdqIdx]);

    return FLCN_OK;
}

sysKERNEL_CODE FLCN_STATUS
cmdqUnregisterNotifier(LwU64 cmdqIdx)
{
    if (cmdqIdx >= ENGINE_REG(_QUEUE_HEAD__SIZE_1))
    {
        return FLCN_ERR_ILWALID_INDEX;
    }

    if (cmdqListeners[cmdqIdx] == NULL)
    {
        return FLCN_ERR_OBJECT_NOT_FOUND;
    }

    cmdqDisableNotifier(cmdqIdx);

    dbgPrintf(LEVEL_DEBUG, "Unregistered queue %llu notifier for task %p\n",
              cmdqIdx, cmdqListeners[cmdqIdx]);

    cmdqListeners[cmdqIdx] = NULL;

    return FLCN_OK;
}

sysKERNEL_CODE FLCN_STATUS
cmdqEnableNotifier(LwU64 cmdqIdx)
{
    if (cmdqIdx >= ENGINE_REG(_QUEUE_HEAD__SIZE_1))
    {
        return FLCN_ERR_ILWALID_INDEX;
    }

    if (cmdqListeners[cmdqIdx] == NULL)
    {
        return FLCN_ERR_OBJECT_NOT_FOUND;
    }

    LwU32 xMask = csbRead(ENGINE_REG(_CMD_INTREN));

    xMask = xMask | BIT(cmdqIdx);
    csbWrite(ENGINE_REG(_CMD_INTREN), xMask);

    dbgPrintf(LEVEL_TRACE, "Enabled queue %llu notifier for task %p\n", cmdqIdx,
              cmdqListeners[cmdqIdx]);

    return FLCN_OK;
}

sysKERNEL_CODE FLCN_STATUS
cmdqDisableNotifier(LwU64 cmdqIdx)
{
    LwU32 mask;

    if (cmdqIdx >= ENGINE_REG(_QUEUE_HEAD__SIZE_1))
    {
        return FLCN_ERR_ILWALID_INDEX;
    }

    if (cmdqListeners[cmdqIdx] == NULL)
    {
        return FLCN_ERR_OBJECT_NOT_FOUND;
    }

    mask = csbRead(ENGINE_REG(_CMD_INTREN));

    mask = mask & ~BIT(cmdqIdx);
    csbWrite(ENGINE_REG(_CMD_INTREN), mask);

    dbgPrintf(LEVEL_TRACE, "Disabled queue %llu notifier for task %p\n", cmdqIdx,
              cmdqListeners[cmdqIdx]);

    return FLCN_OK;
}
