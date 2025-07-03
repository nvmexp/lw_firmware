/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2007-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_icgkxxx.c
 * @brief  Contains all Interrupt Control routines specific to Gkxxx.
 *
 * Implementation Notes:
 *    # Within ISRs only use macro DBG_PRINTF_ISR (never DBG_PRINTF).
 *
 *    # Avoid access to any BAR0/PRIV registers while in the ISR. This is to
 *      avoid inlwrring the penalty of accessing the PRIV bus due to its
 *      relatively high-latency.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"
#include "dev_pwr_falcon_csb.h"
#include "dev_pwr_falcon_csb_addendum.h"
#include "dev_pwr_pri.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objic.h"
#include "therm/objtherm.h"
#include "pmu_objtimer.h"
#include "pmu_objpg.h"
#include "pmu_objms.h"
#include "pmu_objgcx.h"
#include "pmu_objgr.h"
#include "pmu_objfb.h"
#include "lwosdebug.h"
#include "lwoswatchdog.h"
#include "config/g_ic_private.h"
#include "config/g_fb_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/*!
 * @brief   Generic handler for interrupts that are not planned to be handled
 *          and are not expected to be fired.
 */
static void s_icServiceIntr1Unhandled_GMXXX(void)
    GCC_ATTRIB_SECTION("imem_resident", "s_icServiceIntr1Unhandled_GMXXX");

static void  s_icServiceIntr1Cmd_GMXXX(void)
    GCC_ATTRIB_SECTION("imem_resident", "s_icServiceIntr1Cmd_GMXXX");

#if PMUCFG_FEATURE_ENABLED(PMU_IC_HOLDOFF_INTERRUPT)
static void  s_icServiceIntr1Holdoff_GMXXX(void)
    GCC_ATTRIB_SECTION("imem_resident", "s_icServiceIntr1Holdoff_GMXXX");
#else // PMUCFG_FEATURE_ENABLED(PMU_IC_HOLDOFF_INTERRUPT)
#define s_icServiceIntr1Holdoff_GMXXX s_icServiceIntr1Unhandled_GMXXX
#endif // PMUCFG_FEATURE_ENABLED(PMU_IC_HOLDOFF_INTERRUPT)

#if PMUCFG_FEATURE_ENABLED(PMU_IC_ELPG_INTERRUPTS)
static void  s_icServiceIntr1Elpg0_GMXXX(void)
    GCC_ATTRIB_SECTION("imem_resident", "s_icServiceIntr1Elpg0_GMXXX");
static void  s_icServiceIntr1Elpg1_GMXXX(void)
    GCC_ATTRIB_SECTION("imem_resident", "s_icServiceIntr1Elpg1_GMXXX");
static void  s_icServiceIntr1Elpg2_GMXXX(void)
    GCC_ATTRIB_SECTION("imem_resident", "s_icServiceIntr1Elpg2_GMXXX");
static void  s_icServiceIntr1Elpg3_GMXXX(void)
    GCC_ATTRIB_SECTION("imem_resident", "s_icServiceIntr1Elpg3_GMXXX");
static void  s_icServiceIntr1Elpg4_GMXXX(void)
    GCC_ATTRIB_SECTION("imem_resident", "s_icServiceIntr1Elpg4_GMXXX");
static void  s_icServiceIntr1Elpg5_GMXXX(void)
    GCC_ATTRIB_SECTION("imem_resident", "s_icServiceIntr1Elpg5_GMXXX");
static void  s_icServiceIntr1Elpg6_GMXXX(void)
    GCC_ATTRIB_SECTION("imem_resident", "s_icServiceIntr1Elpg6_GMXXX");
static void  s_icServiceIntr1Elpg7_GMXXX(void)
    GCC_ATTRIB_SECTION("imem_resident", "s_icServiceIntr1Elpg7_GMXXX");
static void  s_icServiceIntr1Elpg8_GMXXX(void)
    GCC_ATTRIB_SECTION("imem_resident", "s_icServiceIntr1Elpg8_GMXXX");
static void  s_icServiceIntr1Elpg9_GMXXX(void)
    GCC_ATTRIB_SECTION("imem_resident", "s_icServiceIntr1Elpg9_GMXXX");
static void  s_icServiceIntr1ElpgGeneric_GMXXX(LwU8 hwEngId)
    GCC_ATTRIB_SECTION("imem_resident", "s_icServiceIntr1ElpgGeneric_GMXXX");
#else // PMUCFG_FEATURE_ENABLED(PMU_IC_ELPG_INTERRUPTS)
#define s_icServiceIntr1Elpg0_GMXXX s_icServiceIntr1Unhandled_GMXXX
#define s_icServiceIntr1Elpg1_GMXXX s_icServiceIntr1Unhandled_GMXXX
#define s_icServiceIntr1Elpg2_GMXXX s_icServiceIntr1Unhandled_GMXXX
#define s_icServiceIntr1Elpg3_GMXXX s_icServiceIntr1Unhandled_GMXXX
#define s_icServiceIntr1Elpg4_GMXXX s_icServiceIntr1Unhandled_GMXXX
#define s_icServiceIntr1Elpg5_GMXXX s_icServiceIntr1Unhandled_GMXXX
#define s_icServiceIntr1Elpg6_GMXXX s_icServiceIntr1Unhandled_GMXXX
#define s_icServiceIntr1Elpg7_GMXXX s_icServiceIntr1Unhandled_GMXXX
#define s_icServiceIntr1Elpg8_GMXXX s_icServiceIntr1Unhandled_GMXXX
#define s_icServiceIntr1Elpg9_GMXXX s_icServiceIntr1Unhandled_GMXXX
#endif // PMUCFG_FEATURE_ENABLED(PMU_IC_ELPG_INTERRUPTS)

/* ------------------------- Global Variables ------------------------------- */
LwBool IcCtxSwitchReq;

/* ------------------------- Static Variables ------------------------------- */
static IC_SERVICE_ROUTINE IcServiceIntrFT_GMXXX[] =
{
    s_icServiceIntr1Unhandled_GMXXX,    // MCP SMU
    s_icServiceIntr1Cmd_GMXXX,          // Queue interrupts
    s_icServiceIntr1Elpg0_GMXXX,        // PG_ENG_0/LPWR_ENG_0
    s_icServiceIntr1Elpg1_GMXXX,        // PG_ENG_1/LPWR_ENG_1
    s_icServiceIntr1Unhandled_GMXXX,    // BAR0
    s_icServiceIntr1Elpg2_GMXXX,        // PG_ENG_2/LPWR_ENG_2
    s_icServiceIntr1Unhandled_GMXXX,    // RETARGET
    s_icServiceIntr1Unhandled_GMXXX,    // BAU
    s_icServiceIntr1Unhandled_GMXXX,    // UART
    s_icServiceIntr1Holdoff_GMXXX,      // ENGINE HOLDOFF
    s_icServiceIntr1Elpg3_GMXXX,        // PG_ENG_3/LPWR_ENG_3
    s_icServiceIntr1Unhandled_GMXXX,    //
    s_icServiceIntr1Unhandled_GMXXX,    // ZPW
    s_icServiceIntr1Unhandled_GMXXX,    // CSBM
    s_icServiceIntr1Unhandled_GMXXX,    // IDLE
    s_icServiceIntr1Elpg4_GMXXX,        // PG_ENG_4/LPWR_ENG_4
    s_icServiceIntr1Elpg5_GMXXX,        // PG_ENG_5/LPWR_ENG_5
    s_icServiceIntr1Unhandled_GMXXX,    // SCP
    s_icServiceIntr1Unhandled_GMXXX,    // UnUsed
    s_icServiceIntr1Unhandled_GMXXX,    // UnUsed
    s_icServiceIntr1Elpg6_GMXXX,        // PG_ENG_6/LPWR_ENG_6
    s_icServiceIntr1Elpg7_GMXXX,        // PG_ENG_7/LPWR_ENG_7
    s_icServiceIntr1Elpg8_GMXXX,        // PG_ENG_8/LPWR_ENG_8
    s_icServiceIntr1Elpg9_GMXXX         // PG_ENG_9/LPWR_ENG_9
};

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/*!
 * @copydoc s_icServiceIntr1Unhandled_GMXXX
 */
static void
s_icServiceIntr1Unhandled_GMXXX(void)
{
    //
    // If we experience an unexpected interrupt, indicate an error (by halting
    // the PMU).
    //
    // MMINTS-TODO: test this properly and uncomment!
    //
    // PMU_HALT();
}

/*!
 * This function is responsible for signaling the command dispatcher task wake-
 * up and dispatch the new command to the appropriate unit-task.  This function
 * also disables all command-queue interrupts to prevent new interrupts from
 * coming in while the current command is being dispatched.  The interrupt must
 * be re-enabled by the command dispatcher task.
 */
static void
s_icServiceIntr1Cmd_GMXXX(void)
{
    DISPATCH_CMDMGMT  dispatch;

    dispatch.disp2unitEvt = DISP2UNIT_EVT_RM_RPC;

    DBG_PRINTF_ISR(("HEAD_INTRSTAT=0x%x\n",
                    REG_RD32(CSB, LW_CPWR_PMU_CMD_HEAD_INTRSTAT),
                    0, 0, 0));

    //
    // disable all interrupts from queues
    // Note: if queue id 7 is enabled here, using it to flush the debug buffer on RISCV
    // causes an issue (seen only on the fmodel so far) where writing to a queue head
    // no longer raises interrupts at all. Please avoid enabling queue id 7 here
    // or anywhere else.
    //
    REG_WR32_STALL(CSB, LW_CPWR_PMU_CMD_INTREN, 0x0U);

    lwrtosQueueIdSendFromISR(LWOS_QUEUE_ID(PMU, CMDMGMT),
                             &dispatch, sizeof(dispatch));
}

#if PMUCFG_FEATURE_ENABLED(PMU_IC_ELPG_INTERRUPTS)
/*!
 * This function is responsible for signalling the ELPG task to inform it that
 * an interrupt has fired that it needs to handle.
 *
 * @param[in]  ctrlId
 *     PgCtrl ID (SW ID)
 *
 * @param[in]  pElpgIntrStatAddr
 *     The CSB address for the ELPG interrupt stat register to read/write.
 */
static void
s_icServiceIntr1ElpgGeneric_GMXXX
(
    LwU8 hwEngId
)
{
    DISPATCH_LPWR lpwrEvt;
    LwU32         pgStatus;
    LwU8          ctrlId = Pg.hwEngIdxMap[hwEngId];

    pgStatus = REG_RD32(CSB, LW_CPWR_PMU_PG_INTRSTAT((LwU32)hwEngId));
    DBG_PRINTF_ISR(("ELPG %d IT : ELPGSTAT=0x%x\n",
                    ctrlId, pgStatus, 0, 0));

    // normally, bit0 _INTR and at least one other bit should bit set
    if (pgStatus != 0U)
    {
        // clearing the line first
        REG_WR32_STALL(CSB, LW_CPWR_PMU_PG_INTRSTAT((LwU32)hwEngId),
            DRF_SHIFTMASK(LW_CPWR_PMU_PG_INTRSTAT_INTR));

        // send a message to the LPWR task to service the interrupt
        if (PMUCFG_FEATURE_ENABLED(PMUTASK_LPWR))
        {
            lpwrEvt.hdr.eventType = LPWR_EVENT_ID_PG_INTR;
            lpwrEvt.intr.ctrlId   = ctrlId;
            lpwrEvt.intr.intrStat = pgStatus;
            lwrtosQueueIdSendFromISR(LWOS_QUEUE_ID(PMU, LPWR),
                                     &lpwrEvt, sizeof(lpwrEvt));

            if (PMUCFG_FEATURE_ENABLED(PMU_PG_IDLE_FLIPPED_RESET) &&
                ((ctrlId == (LwU32)RM_PMU_LPWR_CTRL_ID_MS_MSCG) ||
                 (ctrlId == (LwU32)RM_PMU_LPWR_CTRL_ID_MS_DIFR_SW_ASR))  &&
                FLD_TEST_DRF(_CPWR_PMU, _PG_INTRSTAT, _PG_ON, _SET, pgStatus))
            {
                Ms.bIdleFlipedIsr = pgCtrlIdleFlipped_HAL(&Pg, ctrlId);
            }
        }
    }
}

/*!
 * Specific handler-routine for handling interrupts from PG_ENG_0/LPWR_ENG_0
 */
static void
s_icServiceIntr1Elpg0_GMXXX(void)
{
    s_icServiceIntr1ElpgGeneric_GMXXX(PG_ENG_IDX_ENG_0);
}

/*!
 * Specific handler-routine for handling interrupts from PG_ENG_1/LPWR_ENG_1
 */
static void
s_icServiceIntr1Elpg1_GMXXX(void)
{
    s_icServiceIntr1ElpgGeneric_GMXXX(PG_ENG_IDX_ENG_1);
}

/*!
 * Specific handler-routine for handling interrupts from PG_ENG_2/LPWR_ENG_2
 */
static void
s_icServiceIntr1Elpg2_GMXXX(void)
{
    s_icServiceIntr1ElpgGeneric_GMXXX(PG_ENG_IDX_ENG_2);
}

/*!
 * Specific handler-routine for handling interrupts from PG_ENG_3/LPWR_ENG_3
 */
static void
s_icServiceIntr1Elpg3_GMXXX(void)
{
    s_icServiceIntr1ElpgGeneric_GMXXX(PG_ENG_IDX_ENG_3);
}

/*!
 * Specific handler-routine for handling interrupts from PG_ENG_4/LPWR_ENG_4
 */
static void
s_icServiceIntr1Elpg4_GMXXX(void)
{
    s_icServiceIntr1ElpgGeneric_GMXXX(PG_ENG_IDX_ENG_4);
}

/*!
 * Specific handler-routine for handling interrupts from PG_ENG_5/LPWR_ENG_5
 */
static void
s_icServiceIntr1Elpg5_GMXXX(void)
{
    s_icServiceIntr1ElpgGeneric_GMXXX(PG_ENG_IDX_ENG_5);
}

/*!
 * Specific handler-routine for handling interrupts from PG_ENG_6/LPWR_ENG_6
 */
static void
s_icServiceIntr1Elpg6_GMXXX(void)
{
    s_icServiceIntr1ElpgGeneric_GMXXX(PG_ENG_IDX_ENG_6);
}

/*!
 * Specific handler-routine for handling interrupts from PG_ENG_7/LPWR_ENG_7
 */
static void
s_icServiceIntr1Elpg7_GMXXX(void)
{
    s_icServiceIntr1ElpgGeneric_GMXXX(PG_ENG_IDX_ENG_7);
}

/*!
 * Specific handler-routine for handling interrupts from PG_ENG_8/LPWR_ENG_8
 */
static void
s_icServiceIntr1Elpg8_GMXXX(void)
{
    s_icServiceIntr1ElpgGeneric_GMXXX(PG_ENG_IDX_ENG_8);
}

/*!
 * Specific handler-routine for handling interrupts from PG_ENG_9/LPWR_ENG_9
 */
static void
s_icServiceIntr1Elpg9_GMXXX(void)
{
    s_icServiceIntr1ElpgGeneric_GMXXX(PG_ENG_IDX_ENG_9);
}

#endif // PMUCFG_FEATURE_ENABLED(PMU_IC_ELPG_INTERRUPTS)

#if PMUCFG_FEATURE_ENABLED(PMU_IC_HOLDOFF_INTERRUPT)
/*!
 * High-level handler-routine for dealing with eng holdoff interrupts.
 */
static void
s_icServiceIntr1Holdoff_GMXXX(void)
{
#if (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))
    DISPATCH_LPWR lpwrEvt;
    LwU32         holdStatus;

    holdStatus = REG_RD32(CSB, LW_CPWR_THERM_ENG_HOLDOFF_STATUS);
    DBG_PRINTF_ISR(("Holdoff status is 0x%x\n", holdStatus, 0, 0, 0));

    // send a message to the LPWR task
    if (PMUCFG_FEATURE_ENABLED(PMUTASK_LPWR) &&
        (holdStatus != 0U))
    {
        //
        // First send MS Aborted event to DI-OS/DI-SCC as precausion
        //
        // If the GCX task is in imem and the lwrrently entring DI-OS/DI-SSC
        // then abort should first be sent to the GCX task since we may need
        // to undo some steps before being able to abort/exit mscg.
        //
        // This even was added as precaution. DeepL1 exit should happen before
        // triggering engine holdoff
        //
        gcxDidleSendMsAbortedEvent_HAL(&Gcx);

        // Now send HOLDOFF_INTR to LPWR task
        lpwrEvt.hdr.eventType = LPWR_EVENT_ID_PG_HOLDOFF_INTR;
        lpwrEvt.intr.intrStat = holdStatus;
        lpwrEvt.intr.ctrlId   = RM_PMU_LPWR_CTRL_ID_ILWALID_ENGINE;
        lwrtosQueueIdSendFromISR(LWOS_QUEUE_ID(PMU, LPWR),
                                 &lpwrEvt, sizeof(lpwrEvt));
    }
#endif
}
#endif // PMUCFG_FEATURE_ENABLED(PMU_IC_HOLDOFF_INTERRUPT)

/* ------------------------- Public Functions ------------------------------- */
/*!
 * Responsible for all initialization or setup pertaining to PMU interrupts.
 * This includes interrupt routing as well as setting the default enable-state
 * for all PMU interrupt-sources (command queues, gptmr, gpios, etc ...).
 *
 * IRQ0/IRQ1 are the two possible interrupt inputs of the falcon. While setting
 * up an interupt it should be mentioned in IRQDEST_TARGET whether it should be
 * targeted to interrupt the falcon on IRQ0 or IRQ1. The general convention
 * followed on most of the falcons is that only the interrupt used for scheduling
 * goes to IRQ1, everything else goes to IRQ0. We have set up our interrupt handler
 * in such a way that IV1 (vector for IRQ1) can only handle one interrupt which would
 * require to run the scheduler and IV0 calls the global interrupt handler function
 * (icService_xxx). The RTOS allows the clients (PMU, DPU, SEC2, etc.) to install
 * their own IV0 handlers but it never exposes the installation of the IV1 handler
 * since it is implemented by the OS itself.
 *
 * Notes:
 *     - The interrupt-enables for both falcon interrupt vectors should be OFF/
 *       disabled prior to calling this function.
 *     - This function does not modify the interrupt-enables for either vector.
 *     - This function MUST be called PRIOR to starting the scheduler.
 */
void
icPreInit_GMXXX(void)
{
    // Ensure the top-level interrupts are enabled and configured first
    icPreInitTopLevel_HAL(&Ic);

    // Enable command-queue interrupts
    REG_WR32(CSB, LW_CPWR_PMU_CMD_INTREN, IC_CMD_INTREN_SETTINGS);

    // Enable the GPIO Bank0 interrupts
    if (PMUCFG_FEATURE_ENABLED(PMU_IC_MISCIO))
    {
        icEnableMiscIOBank0_HAL(&Ic);
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_BIF_XUSB))
    {
        // Enable all XUSB interrupts
        icXusbIntrEnable_HAL(&Ic);
    }

#ifndef UPROC_RISCV
    // Enable the general purpose timer
    REG_WR32(CSB, LW_CMSDEC_FALCON_GPTMRCTL,
        DRF_SHIFTMASK(LW_CMSDEC_FALCON_GPTMRCTL_GPTMREN));
#endif // UPROC_RISCV
}

/*!
 * @brief   Initializes the top-level interrupt sources
 *
 * @details The PMU's interrupt sources are organized into a tree. This function
 *          configures and enables the interrupts at the top-level, as required.
 */
void
icPreInitTopLevel_GMXXX(void)
{
    //
    // Setup interrupt routing
    //
    // Halt, stallreq, swgen0, ctxe, and limitv get routed to host.
    // The rest are routed to falcon on IV0; except gptimer which gets routed
    // to falcon on IV1. The Watchdog is conditionally routed to the host or the falcon
    // depending on if the Watchdog task is enabled.
    //
    // FB falcon, which will be exelwting the entire MCLK switch sequence volta
    // onwards, doesn't have access to the host registers that blocks the host to
    // FB interface. We've come with this WAR to have PMU do the part of the FB stop
    // sequence that FB falcon can't really do at the moment. CTXSW and MTHD
    // interrupts are needed to support this protocol.
    // For more details on the WAR: Bug 1898603.
    //
    // To do: Replace the DRF_NUM defines with FLD_SET_DRF when we move to the CPWR
    // defines at all the applicable places in this function. The defines for choosing
    // the destination are not available for such cases hence leaving them with DRF_NUM.
    // Refraining from adding them as it'd need changes to addendum files of all the
    // chips and would be like adding more to the mess that we already have.
    //
    //
    const LwU32 irqDest =
#if PMUCFG_FEATURE_ENABLED(PMUTASK_WATCHDOG)
             DRF_DEF(_CMSDEC, _FALCON_IRQDEST, _HOST_WDTMR,   _FALCON) |
#else
             DRF_DEF(_CMSDEC, _FALCON_IRQDEST, _HOST_WDTMR,   _HOST)   |
#endif
             DRF_DEF(_CMSDEC, _FALCON_IRQDEST, _HOST_HALT,    _HOST)   |
             DRF_DEF(_CMSDEC, _FALCON_IRQDEST, _HOST_EXTERR,  _HOST)   |
             DRF_DEF(_CMSDEC, _FALCON_IRQDEST, _HOST_SWGEN0,  _HOST)   |
    // Set up the CTXSW and MTHD interrupts if the FB stop WAR is enabled
#if PMUCFG_FEATURE_ENABLED(PMU_CLK_FBFLCN_MCLK_SWITCH_FB_STOP_WAR)
             DRF_DEF(_CMSDEC, _FALCON_IRQDEST, _HOST_CTXSW,   _FALCON)       |
             DRF_DEF(_CMSDEC, _FALCON_IRQDEST, _HOST_MTHD,    _FALCON)       |
             DRF_DEF(_CMSDEC, _FALCON_IRQDEST, _TARGET_CTXSW, _FALCON_IRQ0)  |
             DRF_DEF(_CMSDEC, _FALCON_IRQDEST, _TARGET_MTHD,  _FALCON_IRQ0)  |
#endif
             DRF_DEF(_CMSDEC, _FALCON_IRQDEST, _TARGET_GPTMR,  _FALCON_IRQ1);

    //
    // Enable the top-level Falcon interrupt sources
    //     - GPTMR     : general-purpose timer for OS scheduler tick
    //     - WDTMR     : for Watchdog interrupts for Watchdog task
    //     - HALT      : CPU transitioned from running into halt
    //     - SWGEN0    : for communication with RM via Message Queue
    //     - SWGEN1    : for context switching (enhanced scheduler)
    //     - SECONDARY : for ELPG interrupts
    //     - THERM     : for MSGBOX interrupts
    //     - MISCIO    : for GPIO interrupts
    //     - RTTIMER   : for internal PMU timer interrupts (disabled)
    //     - EXTERR    : for external mem access error interrupt
    //
    const LwU32 irqMset = DRF_DEF(_CMSDEC, _FALCON_IRQMSET, _GPTMR,   _SET) |
#if PMUCFG_FEATURE_ENABLED(PMUTASK_WATCHDOG)
             DRF_DEF(_CMSDEC, _FALCON_IRQMSET, _WDTMR,   _SET) |
#endif
             DRF_DEF(_CMSDEC, _FALCON_IRQMSET, _HALT,    _SET) |
             DRF_DEF(_CMSDEC, _FALCON_IRQMSET, _SWGEN0,  _SET) |
             DRF_NUM(_CMSDEC, _FALCON_IRQMSET, _SECOND,     1) |
#if PMUCFG_FEATURE_ENABLED(PMU_THERM_INTR)
             DRF_NUM(_CMSDEC, _FALCON_IRQMSET, _THERM,      1) |
#endif
#if PMUCFG_FEATURE_ENABLED(PMU_IC_MISCIO)
             DRF_NUM(_CMSDEC, _FALCON_IRQMSET, _MISCIO,     1) |
#endif // PMUCFG_FEATURE_ENABLED(PMU_IC_MISCIO)
#if PMUCFG_FEATURE_ENABLED(PMU_CLK_FBFLCN_MCLK_SWITCH_FB_STOP_WAR)
             DRF_DEF(_CMSDEC, _FALCON_IRQMSET, _CTXSW,   _SET) |
             DRF_DEF(_CMSDEC, _FALCON_IRQMSET, _MTHD,    _SET) |
#endif
             DRF_DEF(_CMSDEC, _FALCON_IRQMSET, _EXTERR,  _SET);

    REG_WR32(CSB, LW_CMSDEC_FALCON_IRQDEST, irqDest);

    //
    // Set up MTHD and CTXSW interrupts to be edge triggered if the FB stop
    // WAR is enabled. For level triggered interrupts IRQSTAT is a direct copy
    // of the interrupt source line. In that case writes to IRQSSET and IRQSCLR
    // to modify the interrupt status will have no effect. Since we want to trigger
    // the interrupts from FB falcon using IRQSSET we set these interrupts up as edge
    // triggered. Although CTXSW is edge triggered by default we're settting it here
    // irrespectively to avoid relying on the HW init value
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_FBFLCN_MCLK_SWITCH_FB_STOP_WAR))
    {
        LwU32 irqMode = REG_RD32(CSB, LW_CMSDEC_FALCON_IRQMODE);

        irqMode = FLD_SET_DRF(_CMSDEC, _FALCON_IRQMODE, _LVL_MTHD, _FALSE, irqMode);
        irqMode = FLD_SET_DRF(_CMSDEC, _FALCON_IRQMODE, _LVL_CTXSW, _FALSE, irqMode);

        REG_WR32(CSB, LW_CMSDEC_FALCON_IRQMODE, irqMode);
    }

    REG_WR32(CSB, LW_CMSDEC_FALCON_IRQMSET, irqMset);

#if !(PMUCFG_FEATURE_ENABLED(PMU_THERM_INTR))
    REG_WR32(CSB, LW_CMSDEC_FALCON_IRQMCLR,
        DRF_NUM(_CMSDEC, _FALCON_IRQMCLR, _THERM, 1));
#endif

    //
    // Enable ECC interrupts if needed. Must go after above configuration (at
    // least IRQDEST/IRQMODE) because the above does direct writes and not RMWs
    //
    icEccIntrEnable_HAL(&Ic);
}

/*!
 * High-level handler routine for dealing with all first-level PMU interrupts
 * (includes command management, ELPG, interrupt retargetting, etc...). For
 * This function will call the appropriate handler-function for each first-level
 * interrupt-source that is lwrrently pending.
 */
void
icServiceIntr1MainIrq_GMXXX(void)
{
    LwU32 irq           = 0;
    LwU32 statusSec     = REG_RD32(CSB, LW_CPWR_PMU_INTR_1);
    LwU32 lwrrStatus    = statusSec;

    DBG_PRINTF_ISR(("LW_CPWR_PMU_INTR_1 0x%x \n", statusSec, 0, 0, 0));

    while ((lwrrStatus != 0U) &&
           (irq < LW_ARRAY_ELEMENTS(IcServiceIntrFT_GMXXX)))
    {
        if ((lwrrStatus & 1U) != 0U)
        {
            IcServiceIntrFT_GMXXX[irq]();
        }

        lwrrStatus = lwrrStatus >> 1U;
        irq++;
    }

    DBG_PRINTF_ISR(("Clear INTR_1\n", 0, 0, 0, 0));
    REG_WR32_STALL(CSB, LW_CPWR_PMU_INTR_1, statusSec);
}

/*!
 * Handler-routine for dealing with all first-level PMU interrupts.  First-
 * level PMU interrupts include (but not limited to):
 *    - Timer Interrupts
 *    - Thermal Interrupts
 *    - Second-Level Interrupts
 *    - Low-Latency Interrupts
 *    - FBIF Interrupts
 *
 * In call cases, processing is delegated to interrupt-specific handler
 * functions to allow this function to main generic.
 *
 * @return  See @ref IcCtxSwitchReq for details.
 */
LwBool
icService_GMXXX(LwU32 pendingIrqMask)
{
    LwU32 clearBits = 0;

    IcCtxSwitchReq = LW_FALSE;

    //
    // Secondary Interrupt
    //
    if (FLD_TEST_DRF_NUM(_CMSDEC_FALCON, _IRQSTAT, _SECOND, 1U, pendingIrqMask))
    {
        //
        // To do: Replace the IRQMCLR defines with IRQSCLR when we move
        // to the CPWR defines at all the applicable places in this function.
        // MSDEC defines are not present for IRQSCLR and adding them now in the
        // addendum files of all the chips would be like adding more to the mess
        // that we already have.
        //
        clearBits = FLD_SET_DRF_NUM(_CMSDEC_FALCON, _IRQMCLR, _SECOND, 1, clearBits);
        icServiceIntr1MainIrq_HAL(&Ic);
    }

    //
    // THERM Interrupt
    //
    if (FLD_TEST_DRF_NUM(_CMSDEC_FALCON, _IRQSTAT, _THERM, 1U, pendingIrqMask))
    {
        clearBits = FLD_SET_DRF_NUM(_CMSDEC_FALCON, _IRQMCLR, _THERM, 1, clearBits);
        if (PMUCFG_FEATURE_ENABLED(PMU_THERM_INTR))
        {
            thermService_HAL(&Therm);
        }
        else
        {
            DBG_PRINTF_ISR(("THERM interrupt\n", 0, 0,0,0));
        }
    }

    //
    // MISC IO Interrupt
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_IC_MISCIO) &&
        FLD_TEST_DRF_NUM(_CMSDEC_FALCON, _IRQSTAT, _MISCIO, 1U, pendingIrqMask))
    {
        clearBits = FLD_SET_DRF_NUM(_CMSDEC_FALCON, _IRQMCLR, _MISCIO, 1, clearBits);
        icServiceMiscIO_HAL(&Ic);
    }

    //
    // Watchdog Timer Interrupt
    //
    if (PMUCFG_FEATURE_ENABLED(PMUTASK_WATCHDOG) &&
        FLD_TEST_DRF_NUM(_CMSDEC_FALCON, _IRQSTAT, _WDTMR, 1U, pendingIrqMask))
    {
        clearBits = FLD_SET_DRF_NUM(_CMSDEC_FALCON, _IRQMCLR, _WDTMR, 1, clearBits);
        lwosWatchdogCallbackExpired();
    }

    // CTXSW and MTHD interrupts are lwrrently used for the FB stop WAR
    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_FBFLCN_MCLK_SWITCH_FB_STOP_WAR))
    {
        if (FLD_TEST_DRF(_CMSDEC, _FALCON_IRQSTAT, _CTXSW, _TRUE, pendingIrqMask))
        {
            clearBits = FLD_SET_DRF_NUM(_CMSDEC_FALCON, _IRQMCLR, _CTXSW, 1, clearBits);
            fbStopPmuFbflcnWar_HAL(&Fb);
        }
        if (FLD_TEST_DRF(_CMSDEC, _FALCON_IRQSTAT, _MTHD, _TRUE, pendingIrqMask))
        {
            clearBits = FLD_SET_DRF_NUM(_CMSDEC_FALCON, _IRQMCLR, _MTHD, 1, clearBits);
            fbStartPmuFbflcnWar_HAL(&Fb);
        }
    }

    // Clear interrupt using a blocking write
    REG_WR32_STALL(CSB, LW_CMSDEC_FALCON_IRQSCLR, clearBits);

    return IcCtxSwitchReq;
}

#ifdef FLCNDBG_ENABLED
/*!
 * @brief Writes ICD command and IBRKPT registers when interrupt handler
 * gets called.
 *
 * This routine writes the ICD command and IBRKPT registers when the
 * interrupt handler gets called in response to the SW interrrupt
 * generated during breakpoint hit.
 */
void
icServiceSwIntr_GMXXX(void)
{
    LwU32 data = 0;
    LwU32 ptimer0Start = 0;

    // Program EMASK of ICD
    data = FLD_SET_DRF(_CMSDEC_FALCON, _ICD_CMD, _OPC, _EMASK, data);
    data = FLD_SET_DRF(_CMSDEC_FALCON, _ICD_CMD, _EMASK_EXC_IBREAK,
                       _TRUE, data);
    REG_WR32(CSB, LW_CMSDEC_FALCON_ICD_CMD, data);

    // We have to wait here for ICD done since there is no bit to poll on
    ptimer0Start = REG_RD32(CSB, LW_CMSDEC_FALCON_PTIMER0);
    while ((REG_RD32(CSB, LW_CMSDEC_FALCON_PTIMER0) - ptimer0Start) <
           FLCN_DEBUG_DEFAULT_TIMEOUT)
    {
        // NOP
    }

    // Read back ICD and check for CMD ERROR
    data = REG_RD32(CSB, LW_CMSDEC_FALCON_ICD_CMD);
    if (FLD_TEST_DRF(_CMSDEC_FALCON, _ICD_CMD, _ERROR, _TRUE, data))
    {
        PMU_HALT();
    }

    // The first break point register
    data = REG_RD32(CSB, LW_CMSDEC_FALCON_IBRKPT1);
    data = FLD_SET_DRF(_CMSDEC_FALCON, _IBRKPT1, _SUPPRESS, _ENABLE, data);
    REG_WR32(CSB, LW_CMSDEC_FALCON_IBRKPT1, data);

    // The second breakpoint register
    data = REG_RD32(CSB, LW_CMSDEC_FALCON_IBRKPT2);
    data = FLD_SET_DRF(_CMSDEC_FALCON, _IBRKPT2, _SUPPRESS, _ENABLE, data);
    REG_WR32(CSB, LW_CMSDEC_FALCON_IBRKPT2, data);

    // Write SW GEN0 interrupt
    REG_WR32(CSB, LW_CMSDEC_FALCON_IRQSSET,
            DRF_SHIFTMASK(LW_CMSDEC_FALCON_IRQSSET_SWGEN0));

    // Write debug info register
    REG_WR32(CSB, LW_CMSDEC_FALCON_DEBUGINFO, FLCN_DEBUG_DEFAULT_VALUE);
}
#endif // FLCNDBG_ENABLED

/*!
 * @brief Enables the Halt Interrupt
 *
 * It need to be enabled to HALT early incase of failure in pmu init
 */
void
icHaltIntrEnable_GMXXX(void)
{
    LwU32 regVal;

    // Clear the Halt Interrupt before unmasking HALT interrupt
    regVal = REG_RD32(CSB, LW_CMSDEC_FALCON_IRQSCLR);
    regVal = FLD_SET_DRF(_CMSDEC, _FALCON_IRQSCLR, _HALT, _SET, regVal);
    REG_WR32_STALL(CSB, LW_CMSDEC_FALCON_IRQSCLR, regVal);

    regVal = REG_RD32(CSB, LW_CMSDEC_FALCON_IRQMSET);
    regVal = FLD_SET_DRF(_CMSDEC, _FALCON_IRQMSET, _HALT, _SET, regVal);
    REG_WR32_STALL(CSB, LW_CMSDEC_FALCON_IRQMSET, regVal);
}

/*!
 * @brief Disables the Halt Interrupt
 *
 * It needs to be disabled before we HALT on detach
 */
void
icHaltIntrDisable_GMXXX(void)
{
    LwU32 regVal = REG_RD32(CSB, LW_CMSDEC_FALCON_IRQMCLR);
    regVal = FLD_SET_DRF(_CMSDEC, _FALCON_IRQMCLR, _HALT, _SET, regVal);
    REG_WR32_STALL(CSB, LW_CMSDEC_FALCON_IRQMCLR, regVal);
}

/*!
 * @brief   Disables all of the PMU's top-level interrupt sources
 */
void
icDisableAllInterrupts_GMXXX(void)
{
    REG_WR32_STALL(CSB, LW_CMSDEC_FALCON_IRQMCLR, LW_U32_MAX);
}

/*!
 * @brief   Disables interrupts for the detached state
 */
void
icDisableDetachInterrupts_GMXXX(void)
{
    // Disable the top-level Falcon interrupt sources except GPTMR and SWGEN0.
    REG_WR32(CSB, LW_CMSDEC_FALCON_IRQMCLR,
        DRF_SHIFTMASK(LW_CMSDEC_FALCON_IRQMCLR_CTXSW) |
        DRF_SHIFTMASK(LW_CMSDEC_FALCON_IRQMCLR_HALT ) |
        DRF_SHIFTMASK(LW_CMSDEC_FALCON_IRQMCLR_EXT  ));
}

/*!
 * @brief   Get the mask of interrupts pending to the PMU itself
 *
 * @return  The mask of pending interrupts
 */
LwU32
icGetPendingUprocInterrupts_GMXXX(void)
{
    LwU32 dest = REG_RD32(CSB, LW_CMSDEC_FALCON_IRQDEST);

    // only handle level 0 interrupts that are routed to falcon
    return REG_RD32(CSB, LW_CMSDEC_FALCON_IRQSTAT) &
                    ~(dest)                        &   // To check for interrupts with dest as falcon
                    ~(dest >> 16U);                    // To check for interrupts on IRQ0
}
