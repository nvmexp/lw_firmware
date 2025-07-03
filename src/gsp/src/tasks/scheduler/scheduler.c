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
 * @file    scheduler.c
 * @brief   Scheduler task (main).
 */

/* ------------------------ System Includes -------------------------------- */
/* ------------------------ Register Includes ------------------------------ */
#include <engine.h>

/* ------------------------ SafeRTOS Includes ------------------------------ */
#include <lwrtos.h>
#include <sections.h>

/* ------------------------ RISC-V system library  -------------------------- */
#include <syslib/syslib.h>
#include <lwriscv/print.h>

/* ------------------------ Module Includes -------------------------------- */
#include "unit_dispatch.h"
#include "tasks.h"
#include "rmflcncmdif.h"

#define tprintf(level,...)    dbgPrintf(level, "scheduler: " __VA_ARGS__)
#define IRQ_MASK_ALL    (~0ULL)

#define IRQ_DEMO_BIT    12
#define IRQ_DEMO_MASK   (1ULL << IRQ_DEMO_BIT)


// This is part of command management task accessible by other tasks.
// It will be removed once new messaging is in place.
extern void cmdQueueSweep(PRM_FLCN_QUEUE_HDR pHdr);

static void handleSchedulerCommand(LwU8 cmdType, const RM_GSP_COMMAND *pCmd)
{
    FLCN_STATUS ret;
    RM_FLCN_MSG_GSP gspMsg;

    // By default just send ACK
    gspMsg.hdr = pCmd->hdr;

    tprintf(LEVEL_TRACE, "Scheduler request, cmd: %x, sending ACK.\n", cmdType);

    ret = lwrtosQueueSendBlocking(rmMsgRequestQueue, &gspMsg, sizeof(gspMsg));
    if (ret != FLCN_OK)
    {
        tprintf(LEVEL_ERROR, "Failed to send message to queue..%x\n", ret);
    }
}

static void handleSchedulerIrq(LwU32 irq)
{
    tprintf(LEVEL_TRACE, "Doorbell irq %d\n", irq);
}

lwrtosTASK_FUNCTION(schedulerMain, pvParameters)
{
    DISP2UNIT_CMD irqNotification;
    FLCN_STATUS ret;

    tprintf(LEVEL_INFO, "started.\n");

    // Request notification interrupts, for demo use single top bit
    irqNotification.eventType = DISP2UNIT_EVT_SIGNAL;
    irqNotification.cmdQueueId = 0;
    irqNotification.pCmd = 0;

    ret = sysNotifyRegisterTaskNotifier(IRQ_DEMO_MASK, schedulerRequestQueue,
                                        &irqNotification,
                                        sizeof(irqNotification));
    if (ret != FLCN_OK)
    {
        sysTaskExit(dbgStr(LEVEL_CRIT, "Scheduler: failed requesting doorbell notifications."), ret);
    }
    ret = sysNotifyEnableIrq(IRQ_DEMO_MASK, IRQ_MASK_ALL);
    if (ret != FLCN_OK)
    {
        sysTaskExit(dbgStr(LEVEL_CRIT, "Scheduler: failed enabling doorbel notifications."), ret);
    }

    while (LW_TRUE)
    {
        DISP2UNIT_CMD disp2Unit;
        FLCN_STATUS ret = -1;

        ret = lwrtosQueueReceive(schedulerRequestQueue, &disp2Unit, sizeof(disp2Unit), lwrtosMAX_DELAY);
        if (ret == FLCN_OK)
        {
            if (disp2Unit.eventType == DISP2UNIT_EVT_COMMAND)
            {
                RM_GSP_COMMAND cmd;

                memcpy((LwU8 *)&cmd, (LwU8 *)disp2Unit.pCmd,
                       sizeof(RM_GSP_COMMAND));

                handleSchedulerCommand(cmd.cmd.scheduler.cmdType, &cmd);
                cmdQueueSweep(&(((RM_FLCN_CMD *)disp2Unit.pCmd)->cmdGen.hdr));
            } else if (disp2Unit.eventType == DISP2UNIT_EVT_SIGNAL)
            {
                // interrupt
                while (LW_TRUE)
                {
                    LwU64 groups = sysNotifyQueryGroups();

                    if (groups == 0)
                    {
                        break;
                    } else
                    {
                        LwU64 bits, j;
                        bits = sysNotifyQueryIrqs(IRQ_DEMO_BIT);
                        if (bits)
                        {
                            for (j=0; j<64; ++j)
                            {
                                if ((1ULL << j) & bits)
                                {
                                    handleSchedulerIrq(j);
                                }
                            }
                            sysNotifyAckIrq(IRQ_DEMO_MASK, bits);

                            //
                            // need to reenable all, because driver disables
                            // whole group on interrupt
                            //
                            sysNotifyEnableIrq(IRQ_DEMO_MASK, IRQ_MASK_ALL);
                        }
                    }
                }
                sysNotifyProcessingDone();
            }
        }
        else
        {
            sysTaskExit(dbgStr(LEVEL_CRIT, "Scheduler: Failed waiting on queue."), ret);
        }
    }
}
