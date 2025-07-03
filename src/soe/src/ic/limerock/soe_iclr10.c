/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   soe_iclr10.c
 * @brief  Contains all Interrupt Control routines specific to SOE LR10x.
 */

/* ------------------------ System Includes -------------------------------- */
/* ------------------------ Application Includes --------------------------- */
#include "main.h"
#include "dev_soe_csb.h"
#include "soe_objsoe.h"
#include "soe_cmdmgmt.h"
#include "soe_timer.h"
#include "unit_dispatch.h"
#include "dev_therm.h"

#include "config/g_ic_private.h"
#include "config/g_intr_private.h"
#include "config/g_timer_private.h"

/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
static void _icServiceCmd_LR10(void)
    GCC_ATTRIB_SECTION("imem_resident", "_icServiceCmd_LR10");
/* ------------------------ Global Variables ------------------------------- */
IC_HAL_IFACES IcHal;
LwBool IcCtxSwitchReq;
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Public Functions ------------------------------- */

/*!
 * Responsible for all initialization or setup pertaining to SOE interrupts.
 * This includes interrupt routing as well as setting the default enable-state
 * for all SOE interupt-sources (command queues, gptmr, etc ...).
 *
 * Notes:
 *     - The interrupt-enables for both falcon interrupt vectors should be OFF/
 *       disabled prior to calling this function.
 *     - This function does not modify the interrupt-enables for either vector.
 *     - This function MUST be called PRIOR to starting the scheduler.
 *     - This function should be called to set up the SOE interrupts before
 *       calling @ref soeStartOsTicksTimer to start the OS timer.
 */
void
icPreInit_LR10(void)
{
    LwU32  regVal;
    
    // Setup interrupt routing
    regVal = (DRF_DEF(_CSOE, _FALCON_IRQDEST, _HOST_HALT,          _HOST)          |
              DRF_DEF(_CSOE, _FALCON_IRQDEST, _HOST_EXTERR,        _HOST)          | 
              DRF_DEF(_CSOE, _FALCON_IRQDEST, _HOST_SWGEN0,        _HOST)          | /* Msg queue intr      */
              DRF_DEF(_CSOE, _FALCON_IRQDEST, _HOST_GPTMR,         _FALCON)        | /* Timer (GP) intr     */
              DRF_DEF(_CSOE, _FALCON_IRQDEST, _HOST_EXT_EXTIRQ5,   _FALCON)        | /* Cmd queue intr      */
              DRF_DEF(_CSOE, _FALCON_IRQDEST, _HOST_EXT_EXTIRQ6,   _FALCON)        | /* RT timer intr (OS)  */
              DRF_DEF(_CSOE, _FALCON_IRQDEST, _HOST_EXT_EXTIRQ8,   _FALCON)        | /* SAW/GIN intr        */
              DRF_DEF(_CSOE, _FALCON_IRQDEST, _TARGET_HALT,        _HOST_NORMAL)   | /* HOST gets halts     */
              DRF_DEF(_CSOE, _FALCON_IRQDEST, _TARGET_EXTERR,      _HOST_NORMAL)   |
              DRF_DEF(_CSOE, _FALCON_IRQDEST, _TARGET_SWGEN0,      _HOST_NORMAL)   |
              DRF_DEF(_CSOE, _FALCON_IRQDEST, _TARGET_GPTMR,       _FALCON_IRQ0)   |
              DRF_DEF(_CSOE, _FALCON_IRQDEST, _TARGET_EXT_EXTIRQ5, _FALCON_IRQ0)   |
              DRF_DEF(_CSOE, _FALCON_IRQDEST, _TARGET_EXT_EXTIRQ6, _FALCON_IRQ1)   | /* OS scheduler on IV1 */
              DRF_DEF(_CSOE, _FALCON_IRQDEST, _TARGET_EXT_EXTIRQ8, _FALCON_IRQ0));

    REG_WR32(CSB, LW_CSOE_FALCON_IRQDEST, regVal);


    // Adjust IRQ LEVEL/EDGE settings...
    regVal = REG_RD32(CSB, LW_CSOE_FALCON_IRQMODE);
    regVal = FLD_SET_DRF(_CSOE, _FALCON_IRQMODE, _LVL_EXT_EXTIRQ5, _FALSE, regVal); // command queue interrupt
    regVal = FLD_SET_DRF(_CSOE, _FALCON_IRQMODE, _LVL_EXT_EXTIRQ6, _FALSE, regVal); // RT timer interrupt
    regVal = FLD_SET_DRF(_CSOE, _FALCON_IRQMODE, _LVL_EXT_EXTIRQ8, _TRUE,  regVal); // SAW/GIN interrupt 
    REG_WR32(CSB, LW_CSOE_FALCON_IRQMODE, regVal);

    //
    // Unmask the top-level Falcon interrupt sources
    //     - HALT      : CPU transitioned from running into halt
    //     - EXTERR    : for external mem access error interrupt
    //     - SWGEN0    : for communication with RM via message queue
    //     - GPTMR     : timer (GP)
    //     - EXTIRQ5   : for communication with RM via command queue
    //     - EXTIRQ6   : OS ticks
    //     - EXTIRQ8   : For SAW/GIN interrupts
    //
    regVal = (DRF_DEF(_CSOE, _FALCON_IRQMSET, _HALT,        _SET) |
              DRF_DEF(_CSOE, _FALCON_IRQMSET, _EXTERR,      _SET) |
              DRF_DEF(_CSOE, _FALCON_IRQMSET, _SWGEN0,      _SET) |
              DRF_DEF(_CSOE, _FALCON_IRQMSET, _GPTMR,       _SET) |
              DRF_DEF(_CSOE, _FALCON_IRQMSET, _EXT_EXTIRQ5, _SET) |
              DRF_DEF(_CSOE, _FALCON_IRQMSET, _EXT_EXTIRQ6, _SET) |
              DRF_DEF(_CSOE, _FALCON_IRQMSET, _EXT_EXTIRQ8, _SET));

    REG_WR32(CSB, LW_CSOE_FALCON_IRQMSET, regVal);

    icEnableGinSaw_HAL();
}

/*!
 * Enable SAW/GIN EXTIO interrupts
 */
void
icEnableGinSaw_LR10(void)
{
    // Unmask SAW interrupt (routed into SOE on HUBIRQ0)
    REG_FLD_WR_DRF_DEF_STALL(CSB, _CSOE, _MISC_EXTIO_IRQMSET, _HUBIRQ0, _SET);
}

/*!
 * @brief Unmask command queue interrupt
 */
void
icCmdQueueIntrUnmask_LR10(void)
{
    //
    // For now, we only support one command queue, so we only unmask that one
    // queue head interrupt
    //
    REG_FLD_WR_DRF_DEF_STALL(CSB, _CSOE, _CMD_INTREN, _HEAD_0_UPDATE, _ENABLED);
}

/*!
 * @brief Clear the cmd queue interrupt
 */
void
icCmdQueueIntrClear_LR10(void)
{
    //
    // We only support one cmd queue today. When we add support for more
    // queues, we can clear the interrupt status for all queue heads.
    //
    REG_WR32(CSB, LW_CSOE_CMD_HEAD_INTRSTAT,
             DRF_DEF(_CSOE, _CMD_HEAD_INTRSTAT, _0, _CLEAR));
}

/*!
 * @brief Mask command queue interrupt
 */
void
icCmdQueueIntrMask_LR10(void)
{
    //
    // For now, we only support one command queue, so we only mask that one
    // queue head interrupt. By default, all queue heads are masked, and we
    // only unmask head0 when we initialize. Only that needs to be masked.
    //
    REG_FLD_WR_DRF_DEF_STALL(CSB, _CSOE, _CMD_INTREN, _HEAD_0_UPDATE, _DISABLE);
}

/*!
 * @brief Get the IRQSTAT mask for the context switch timer
 */
LwU32
icOsTickIntrMaskGet_LR10(void)
{
    return DRF_SHIFTMASK(LW_CSOE_FALCON_IRQSTAT_EXT_EXTIRQ6);
}

//
// This is temporary and will be removed or replaced with the new register once cleared
// from HW team. Tracked in bug 3270626.
//
#if !defined(LW_CSOE_MISC_EXTIO_IRQSCLR)
#define LW_CSOE_MISC_EXTIO_IRQSCLR                         0x084c     /* -W-4R */
#define LW_CSOE_MISC_EXTIO_IRQSCLR_HUBIRQ0                 0:0        /* -WXVF */
#define LW_CSOE_MISC_EXTIO_IRQSCLR_HUBIRQ0_SET             0x00000001 /* -W--V */
#endif

/*!
 * Route extirq8 interrupt to SAW/GIN engine
 */
void
icRouteGinSaw_LR10(void)
{
    LwU32 extio;
    
    extio =  REG_RD32(CSB, LW_CSOE_MISC_EXTIO_IRQSTAT);
    if (FLD_TEST_DRF(_CSOE, _MISC_EXTIO_IRQSTAT, _HUBIRQ0, _TRUE, extio))
    {
        // Service and then clear local HUBIRQ0 interrupt.
        intrService_HAL();
        REG_FLD_WR_DRF_DEF_STALL(CSB, _CSOE, _MISC_EXTIO_IRQSCLR, _HUBIRQ0, _SET);
    }
    else
    {
        // Unexpected interrupt on EXTIRQ8, halt for now.
        // TODO: Provide the reason for the halt to the handler
        SOE_HALT();
    }    
}

/*!
 * Handler-routine for dealing with all first-level SOE interrupts.
 *
 * In all cases, processing is delegated to interrupt-specific handler
 * functions to allow this function to remain generic.
 *
 * @return  See @ref _bCtxSwitchReq for details.
 */
LwBool
icService_LR10(void)
{
    LwU32 status;
    LwU32 mask;
    LwU32 dest;

    IcCtxSwitchReq = LW_FALSE;

    mask   = REG_RD32(CSB, LW_CSOE_FALCON_IRQMASK);
    dest   = REG_RD32(CSB, LW_CSOE_FALCON_IRQDEST);

    // only handle level 0 interrupts that are routed to falcon
    status = REG_RD32(CSB, LW_CSOE_FALCON_IRQSTAT)
                 & mask
                 & ~(dest >> 16)
                 & ~(dest);

    DBG_PRINTF_ISR(("---IRQ0: STAT 0x%x, MASK=0x%x, DEST=0x%x\n",
                    status, mask, dest, 0));

    // Timer Interrupt
    if (FLD_TEST_DRF(_CSOE, _FALCON_IRQSTAT, _GPTMR, _TRUE, status))
    {
        timerService_HAL();
    }

    // Cmd Queue Interrupt
    if (FLD_TEST_DRF(_CSOE, _FALCON_IRQSTAT, _EXT_EXTIRQ5, _TRUE, status))
    {
        _icServiceCmd_LR10();
    }

    // SAW/GIN Interrupt (routed in on EXTIRQ8)
    if (FLD_TEST_DRF(_CSOE, _FALCON_IRQSTAT, _EXT_EXTIRQ8, _TRUE, status))
    {
        // EXTIRQ8 has many inputs.  We only care about HUBIRQ0...
        icRouteGinSaw_HAL();
    }


    // clear interrupt using a blocking write
    REG_WR32_STALL(CSB, LW_CSOE_FALCON_IRQSCLR, status);
    return IcCtxSwitchReq;
}

/*!
 * This function is responsible for signaling the command dispatcher task wake-
 * up and dispatch the new command to the appropriate unit-task.  This function
 * also disables all command-queue interrupts to prevent new interrupts from
 * coming in while the current command is being dispatched.  The interrupt must
 * be re-enabled by the command dispatcher task.
 */
static void
_icServiceCmd_LR10(void)
{
    DISPATCH_CMDMGMT dispatch;

    dispatch.disp2unitEvt = DISP2UNIT_EVT_COMMAND;

    // Mask further cmd queue interrupts.
    icCmdQueueIntrMask_HAL();

    lwrtosQueueSendFromISR(SoeCmdMgmtCmdDispQueue, &dispatch,
                           sizeof(dispatch));
}

/*!
 * @brief Unmask the halt interrupt
 */
void
icHaltIntrUnmask_LR10(void)
{
    // Clear the interrupt before unmasking it
    REG_FLD_WR_DRF_DEF_STALL(CSB, _CSOE, _FALCON_IRQSCLR, _HALT, _SET);

    // Now route it to host and unmask it
    REG_FLD_WR_DRF_DEF_STALL(CSB, _CSOE, _FALCON_IRQDEST, _HOST_HALT, _HOST);
    REG_FLD_WR_DRF_DEF_STALL(CSB, _CSOE, _FALCON_IRQMSET, _HALT, _SET);
}

/*!
 * @brief Mask the HALT interrupt so that the SOE can be safely halted
 * without generating an interrupt to RM
 */
void
icHaltIntrMask_LR10(void)
{
    REG_FLD_WR_DRF_DEF_STALL(CSB, _CSOE, _FALCON_IRQMCLR, _HALT, _SET);
}
