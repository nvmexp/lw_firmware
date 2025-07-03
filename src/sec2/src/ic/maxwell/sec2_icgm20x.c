/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   sec2_icgm20x.c
 * @brief  Contains all Interrupt Control routines specific to SEC2 GM20x.
 */

/* ------------------------ System Includes -------------------------------- */
#include "lwostimer.h"

/* ------------------------ Application Includes --------------------------- */
#include "main.h"
#include "sec2_objic.h"
#include "sec2_objsec2.h"
#include "dev_sec_csb.h"
#include "sec2_cmdmgmt.h"
#include "sec2_chnmgmt.h"

#include "config/g_ic_private.h"

/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
static void _icServiceCtxSw_GM20X(void)
    GCC_ATTRIB_SECTION("imem_resident", "_icServiceCtxSw_GM20X");
static void _icServiceMthd_GM20X(void)
    GCC_ATTRIB_SECTION("imem_resident", "_icServiceMthd_GM20X");
static void _icServiceWdTmr_GM20X(void)
    GCC_ATTRIB_SECTION("imem_resident", "_icServiceWdTmr_GM20X");

/* ------------------------ Global Variables ------------------------------- */
IC_HAL_IFACES IcHal;
LwBool IcCtxSwitchReq;

/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * Handler-routine for dealing with all first-level SEC2 interrupts.
 *
 * In all cases, processing is delegated to interrupt-specific handler
 * functions to allow this function to remain generic.
 *
 * @return  See @ref _bCtxSwitchReq for details.
 */
LwBool
icService_GM20X(void)
{
    LwU32 status;
    LwU32 mask;
    LwU32 dest;

    IcCtxSwitchReq = LW_FALSE;

    mask   = REG_RD32(CSB, LW_CSEC_FALCON_IRQMASK);
    dest   = REG_RD32(CSB, LW_CSEC_FALCON_IRQDEST);

    // only handle level 0 interrupts that are routed to falcon
    status = REG_RD32(CSB, LW_CSEC_FALCON_IRQSTAT)
                 & mask
                 & ~(dest >> 16)
                 & ~(dest);

    DBG_PRINTF_ISR(("---IRQ0: STAT 0x%x, MASK=0x%x, DEST=0x%x\n",
                    status, mask, dest, 0));

    // Cmd Queue Interrupt
    if (FLD_TEST_DRF(_CSEC, _FALCON_IRQSTAT, _EXT_EXTIRQ5, _TRUE, status))
    {
        icServiceCmd_HAL();
    }

    // Watchdog timer interrupt
    if (FLD_TEST_DRF(_CSEC, _FALCON_IRQSTAT, _WDTMR, _TRUE, status))
    {
        _icServiceWdTmr_GM20X();
    }

    if (FLD_TEST_DRF(_CSEC, _FALCON_IRQSTAT, _MTHD, _TRUE, status) &&
        FLD_TEST_DRF(_CSEC, _FALCON_IRQSTAT, _CTXSW, _TRUE, status))
    {
        //
        // Host can send a ctxsw as soon as a method has left host and started
        // across the method interface. It is possible that we see both mthd
        // and ctxsw pending at the same time. The correct behavior is to give
        // priority to methods and process/drain all methods for the current
        // context before working on a ctxsw request. Hence, in the case where
        // we receive both interrupts together, we won't handle the ctxsw
        // interrupt now. We will mask it when we process the mthd interrupt
        // below and handle it when we unmask the interrupt after method
        // processing has finished. There are some dislwssions with HW about
        // this topic on bug 1786345.
        //
        status = FLD_SET_DRF(_CSEC, _FALCON_IRQSTAT, _CTXSW, _FALSE, status);
    }

    // Method interface interrupt
    if (FLD_TEST_DRF(_CSEC, _FALCON_IRQSTAT, _MTHD, _TRUE, status))
    {
        _icServiceMthd_GM20X();
    }

    // Context switch interrupt
    if (FLD_TEST_DRF(_CSEC, _FALCON_IRQSTAT, _CTXSW, _TRUE, status))
    {
        _icServiceCtxSw_GM20X();
    }

    // clear interrupt using a blocking write
    REG_WR32_STALL(CSB, LW_CSEC_FALCON_IRQSCLR, status);
    return IcCtxSwitchReq;
}

/*!
 * This function is responsible for signaling the command dispatcher task wake-
 * up and dispatch the new command to the appropriate unit-task.  This function
 * also disables all command-queue interrupts to prevent new interrupts from
 * coming in while the current command is being dispatched.  The interrupt must
 * be re-enabled by the command dispatcher task.
 *
 * Following is the lwrrently handling CMD Queue interrupts -
 *    a. HEAD_0 interrupt - Queue event to 'CMD MGMT' task for further processing
 * (Interrupt on other heads are no-op as we use only queue 0 on GM20X to GA100)
 */
void
icServiceCmd_GM20X(void)
{
    DISPATCH_CMDMGMT dispatch;
    LwU32            regVal;

    dispatch.disp2unitEvt = DISP2UNIT_EVT_COMMAND;

    regVal = REG_RD32(CSB, LW_CSEC_CMD_HEAD_INTRSTAT);

    if (FLD_TEST_DRF(_CSEC, _CMD_HEAD_INTRSTAT, _0, _UPDATED , regVal))
    {
        // Mask further cmd queue interrupt for HEAD_0
        icCmdQueueIntrMask_HAL(&Ic, SEC2_CMDMGMT_CMD_QUEUE_RM);

        lwrtosQueueSendFromISR(Sec2CmdMgmtCmdDispQueue, &dispatch,
                            sizeof(dispatch));
    }
}

/*!
 * This function is responsible for signaling the channel management task to
 * wake up and save off the new context. This function also masks method and
 * ctxsw interrupts to prevent new interrupts from coming in while the current
 * interrupt is being handled. The interrupts must be unmasked by the channel
 * management task.
 */
static void
_icServiceCtxSw_GM20X(void)
{
    DISPATCH_CHNMGMT dispatch;

    dispatch.eventType = DISP2CHNMGMT_EVT_CTXSW;

    // Mask further mthd and ctxsw interrupts
    REG_WR32(CSB, LW_CSEC_FALCON_IRQMCLR,
             (DRF_DEF(_CSEC, _FALCON_IRQMCLR, _MTHD, _SET) |
              DRF_DEF(_CSEC, _FALCON_IRQMCLR, _CTXSW, _SET)));

    lwrtosQueueSendFromISR(Sec2ChnMgmtCmdDispQueue, &dispatch,
                           sizeof(dispatch));
}

/*!
 * This function is responsible for signaling the channel management task to
 * service the watchdog timer
 */
static void
_icServiceWdTmr_GM20X(void)
{
    DISPATCH_CHNMGMT dispatch;

    dispatch.eventType = DISP2CHNMGMT_EVT_WDTMR;

    lwrtosQueueSendFromISR(Sec2ChnMgmtCmdDispQueue, &dispatch,
                           sizeof(dispatch));
}

/*!
 * This function is responsible for signaling the channel management task to
 * wake up and dispatch the new methods to the appropriate unit-task. This
 * function also masks method and ctxsw interrupts to prevent new interrupts
 * from coming in while the current interrupt is being handled. The interrupts
 * must be unmasked by the channel management task.
 */
static void
_icServiceMthd_GM20X(void)
{
    DISPATCH_CHNMGMT dispatch;

    dispatch.eventType = DISP2CHNMGMT_EVT_METHOD;

    // Mask further mthd and ctxsw interrupts
    REG_WR32(CSB, LW_CSEC_FALCON_IRQMCLR,
             (DRF_DEF(_CSEC, _FALCON_IRQMCLR, _MTHD,  _SET) |
              DRF_DEF(_CSEC, _FALCON_IRQMCLR, _CTXSW, _SET)));

    lwrtosQueueSendFromISR(Sec2ChnMgmtCmdDispQueue, &dispatch,
                           sizeof(dispatch));
}

/*!
 * @brief Unmask the halt interrupt
 */
void
icHaltIntrUnmask_GM20X(void)
{
    // Clear the interrupt before unmasking it
    REG_FLD_WR_DRF_DEF_STALL(CSB, _CSEC, _FALCON_IRQSCLR, _HALT, _SET);

    // Now route it to host and unmask it
    REG_FLD_WR_DRF_DEF_STALL(CSB, _CSEC, _FALCON_IRQDEST, _HOST_HALT, _HOST);
    REG_FLD_WR_DRF_DEF_STALL(CSB, _CSEC, _FALCON_IRQMSET, _HALT, _SET);
}

/*!
 * @brief Mask the HALT interrupt so that the SEC2 can be safely halted
 * without generating an interrupt to RM
 */
void
icHaltIntrMask_GM20X(void)
{
    REG_FLD_WR_DRF_DEF_STALL(CSB, _CSEC, _FALCON_IRQMCLR, _HALT, _SET);
}

/*!
 * @brief Unmask host interface interrupts (ctxsw and method interrupts)
 */
void
icHostIfIntrUnmask_GM20X(void)
{
    REG_WR32(CSB, LW_CSEC_FALCON_IRQMSET,
             (DRF_DEF(_CSEC, _FALCON_IRQMSET, _MTHD,  _SET) |
              DRF_DEF(_CSEC, _FALCON_IRQMSET, _CTXSW, _SET)));
}
