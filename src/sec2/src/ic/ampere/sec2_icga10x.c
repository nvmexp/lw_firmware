/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   sec2_icga10x.c
 * @brief  Contains all Interrupt Control routines specific to SEC2 GA10X.
 */

/* ------------------------ System Includes -------------------------------- */
/* ------------------------ Application Includes --------------------------- */
#include "main.h"
#include "dev_sec_csb.h"
#include "sec2_cmdmgmt.h"

#include "config/g_ic_private.h"

/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Public Functions ------------------------------- */

/*!
 * This function is responsible for signaling the command dispatcher task wake-
 * up and dispatch the new command to the appropriate unit-task.  This function
 * also disables all command-queue interrupts to prevent new interrupts from
 * coming in while the current command is being dispatched.  The interrupt must
 * be re-enabled by the command dispatcher task.
 *
 * Following is the lwrrently handling CMD Queue interrupts -
 *    a. HEAD_0 interrupt - Queue event to 'CMD MGMT' task for further processing
 *    b. HEAD_7 interrupt - Queue event to 'ACR' task for GPCCS bootstrapping
 * (Interrupt on other heads are no-op as we use only queue 0 and 7 on GA10X onwards)
 */
void
icServiceCmd_GA10X(void)
{
    DISPATCH_CMDMGMT dispatch1;
    DISPATCH_CMDMGMT dispatch2;
    LwU32            regVal;

    dispatch1.disp2unitEvt = DISP2UNIT_EVT_COMMAND;
    dispatch2.disp2unitEvt = DISP2UNIT_EVT_COMMAND_BOOTSTRAP_GPCCS_FOR_GPCRG;

    regVal = REG_RD32(CSB, LW_CSEC_CMD_HEAD_INTRSTAT);

    if (FLD_IDX_TEST_DRF(_CSEC, _CMD_HEAD_INTRSTAT, _FIELD,
                         SEC2_CMDMGMT_CMD_QUEUE_RM, _UPDATED, regVal))
    {
        // Mask further cmd queue interrupts for HEAD_(RM)
        icCmdQueueIntrMask_HAL(&Ic, SEC2_CMDMGMT_CMD_QUEUE_RM);

        // Send the event to CMDMGMT task for further processing
        lwrtosQueueSendFromISR(Sec2CmdMgmtCmdDispQueue, &dispatch1,
                            sizeof(dispatch1));
    }
    else if (FLD_IDX_TEST_DRF(_CSEC, _CMD_HEAD_INTRSTAT, _FIELD,
                              SEC2_CMDMGMT_CMD_QUEUE_PMU, _UPDATED, regVal))
    {
        // Mask further cmd queue interrupts for HEAD_(PMU)
        icCmdQueueIntrMask_HAL(&Ic, SEC2_CMDMGMT_CMD_QUEUE_PMU);

        // Send the event to ACR task for further processing
        lwrtosQueueSendFromISR(AcrQueue, &dispatch2,
                               sizeof(dispatch2));
    }
}

/*!
 * @brief Unmask command queue interrupt
 */
void
icCmdQueueIntrUnmask_GA10X(LwU8 queueId)
{
    LwU32 regVal = 0;

    // For now, we only support two command queues (_RM and _PMU) GA10x onwards
    if ((queueId != SEC2_CMDMGMT_CMD_QUEUE_RM) &&
        (queueId != SEC2_CMDMGMT_CMD_QUEUE_PMU))
    {
        SEC2_HALT();
    }

    regVal = REG_RD32(CSB, LW_CSEC_CMD_INTREN);

    regVal = FLD_IDX_SET_DRF(_CSEC, _CMD_INTREN, _HEAD_UPDATE,
                             queueId, _ENABLED, regVal);

    REG_WR32_STALL(CSB, LW_CSEC_CMD_INTREN, regVal);
}

/*!
 * @brief Mask command queue interrupt
 */
void
icCmdQueueIntrMask_GA10X(LwU8 queueId)
{
    LwU32 regVal = 0;

    // For now, we only support two command queues (_RM and _PMU) GA10x onwards
    if ((queueId != SEC2_CMDMGMT_CMD_QUEUE_RM) &&
        (queueId != SEC2_CMDMGMT_CMD_QUEUE_PMU))
    {
        SEC2_HALT();
    }

    regVal = REG_RD32(CSB, LW_CSEC_CMD_INTREN);

    regVal = FLD_IDX_SET_DRF(_CSEC, _CMD_INTREN, _HEAD_UPDATE,
                             queueId, _DISABLE, regVal);

    REG_WR32_STALL(CSB, LW_CSEC_CMD_INTREN, regVal);
}

/*!
 * @brief Clear the cmd queue interrupt
 */
void
icCmdQueueIntrClear_GA10X(LwU8 queueId)
{
    LwU32 regVal = 0;

    // For now, we only support two command queues (_RM and _PMU) GA10x onwards
    if ((queueId != SEC2_CMDMGMT_CMD_QUEUE_RM) &&
        (queueId != SEC2_CMDMGMT_CMD_QUEUE_PMU))
    {
        SEC2_HALT();
    }

    regVal = REG_RD32(CSB, LW_CSEC_CMD_HEAD_INTRSTAT);

    regVal = FLD_IDX_SET_DRF(_CSEC, _CMD_HEAD_INTRSTAT, _FIELD,
                             queueId, _CLEAR, regVal);

    REG_WR32_STALL(CSB, LW_CSEC_CMD_HEAD_INTRSTAT, regVal);
}
