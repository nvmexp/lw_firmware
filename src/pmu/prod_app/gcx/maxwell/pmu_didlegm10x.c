/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_didlegm10x.c
 * @brief  HAL-interface for the GM10x Deepidle Engine
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_disp.h"
#include "dev_trim.h"
#include "dev_trim_addendum.h"
#include "dev_pwr_csb.h"
#include "dev_pwr_csb_addendum.h"
#include "dev_pwr_pri.h"
#include "dev_gc6_island.h"
#include "dev_lw_xp.h"
#include "dev_lw_xve.h"
#include "dev_ihub.h"

/* ------------------------- Application Includes --------------------------- */
#include "dbgprintf.h"
#include "lwuproc.h"
#include "flcntypes.h"
#include "lwos_dma.h"
#include "pmu_didle.h"
#include "pmu_objgcx.h"
#include "pmu_objpmu.h"
#include "pmu_objdisp.h"
#include "pmu_objpcm.h"
#include "pmu_objms.h"
#include "pmu_objdi.h"
#include "pmu_objic.h"
#include "pmu_disp.h"
#include "pmu_swasr.h"

#include "config/g_gcx_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */

#define      DIDLE_SSC_US_TO_4NS_OFFSET                               (1000/4)
#define      DIDLE_GC5_THERM_I2CS_ADDR_MASK                           (0xFF)
#define      FALCON_HALT_INSTR_SIZE                                   2

//
// These are temporary defines till HW display manual changes gets in TOT
//
#define LW_PDISP_SOR_PLL_PU_BGAP_DELAY                                       0x00000014 /*       */
#define LW_PDISP_SOR_PLL_PU_PLL_LOCK_DELAY                                   0x000000c8 /*       */

// DeepL1 exit delay required while in DI-SSC(Lwrrently it is set to 300us)
#define DEEP_IDLE_SSC_DEEP_L1_EXIT_DELAY                             0x1FA4

#ifndef LW_PTRIM_SYS_XTAL4X_CTRL_SETUP_3_2
#define LW_PTRIM_SYS_XTAL4X_CTRL_SETUP_3_2                             3:2
#define LW_PTRIM_SYS_XTAL4X_CTRL_SETUP_7_6                             7:6
#endif

#define NAPLL_LOCK_DELAY_US                                          40

// Compile time asserts to protect against assumptions.
ct_assert(DRF_BASE(LW_CPWR_THERM_VID0_PWM_PERIOD) == DRF_BASE(LW_PGC6_SCI_VID_CFG0_PERIOD));
ct_assert(DRF_EXTENT(LW_CPWR_THERM_VID0_PWM_PERIOD) == DRF_EXTENT(LW_PGC6_SCI_VID_CFG0_PERIOD));
ct_assert(DRF_BASE(LW_CPWR_THERM_VID0_PWM_BIT_SPREAD) == DRF_BASE(LW_PGC6_SCI_VID_CFG0_BIT_SPREAD));
ct_assert(DRF_EXTENT(LW_CPWR_THERM_VID0_PWM_BIT_SPREAD) == DRF_EXTENT(LW_PGC6_SCI_VID_CFG0_BIT_SPREAD));
ct_assert(DRF_BASE(LW_CPWR_THERM_VID0_PWM_MIN_SPREAD) == DRF_BASE(LW_PGC6_SCI_VID_CFG0_MIN_SPREAD));
ct_assert(DRF_EXTENT(LW_CPWR_THERM_VID0_PWM_MIN_SPREAD) == DRF_EXTENT(LW_PGC6_SCI_VID_CFG0_MIN_SPREAD));
ct_assert(DRF_BASE(LW_CPWR_THERM_VID1_PWM_HI) == DRF_BASE(LW_PGC6_SCI_VID_CFG1_HI));
ct_assert(DRF_EXTENT(LW_CPWR_THERM_VID1_PWM_HI) == DRF_EXTENT(LW_PGC6_SCI_VID_CFG1_HI));

/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Function Declarations -------------------------- */
static LwU8 s_gcxDidleGpuPowerDown_GM10X(LwU8 targetState, LwU8 *nextState, RM_PMU_DIDLE_STAT *pDidleStats);
static void s_gcxDidleGpuPowerUp_GM10X(LwU8 targetState, RM_PMU_DIDLE_STAT *pDidleStats);

/* ------------------------- Global Variables ------------------------------- */
RM_PMU_DIDLE_STAT DidleStatisticsSSC;
LwU32             gc5SwExitStartTimeLo;

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * Puts the GPU into the GC4 state
 *
 * @param[in]     targetState   Desired end state on successful completion
 *                              of the entry steps
 *
 * @return targetState if the transition into SSC/GC4 was completed,
 *         otherwise the next desired state of the next request in the queue
 *         is returned.
 */
LwU8
gcxGC4Enter_GM10X
(
    LwU8                targetState
)
{
    LwU8                nextState;
    OBJGC4             *pGc4 = GCX_GET_GC4(Gcx);
    DISP2UNIT_RM_RPC    lwrCommand;
    LwU8                lwrCmdStatus;
    LwU8                lwrEventType;
    RM_PMU_DIDLE_STAT  *pDidleStats = NULL;
    LwU8                status;

    // start assuming that the recent command will get gpu into Deepidle
    pGc4->cmdStatus = RM_PMU_GCX_CMD_STATUS_OK;

    // Return early if Common DI sequence is not supported
    if (!PMUCFG_FEATURE_ENABLED(PMU_DI_SEQUENCE_COMMON))
    {
        pGc4->cmdStatus = RM_PMU_GCX_CMD_STATUS_FAIL;
        return DidleLwrState;
    }

    if (targetState == DIDLE_STATE_IN_OS)
    {
        DidleLwrState = DIDLE_STATE_ENTERING_OS;
        pDidleStats   = &DidleStatisticsNH;
    }
    else
    {
        //
        // Invalid target state. Return the current state to indicate that we
        // did not complete the state change and no additional state changes
        // are pending.
        //
        pGc4->cmdStatus = RM_PMU_GCX_CMD_STATUS_FAIL;
        return DidleLwrState;
    }
    pDidleStats->attempts++;

    // Make sure Azalia is disarmed only if GPU is not Display less
    if (!Gcx.bIsGpuDisplayless &&
        !diSeqIsAzaliaInactive_HAL(&Di))
    {
        pGc4->cmdStatus = RM_PMU_GCX_CMD_STATUS_BUSY;
    }

    //
    // For non displayless GPUs (i.e. GPUs with Display HW) make sure that
    // DISPCLK is driven from alternate path if Display PLL Power Down
    // is not supported.
    //
    // This is "fail safe" check. For practical use-cases RM make sure that
    // display clocks are running from alternate path before arming DIOS. For
    // some test scenarios its possible that DISPCLK is driven from VCO path
    // if test is forcing dispclk. There two APIs that can change DISPCLK
    // frequency:
    // 1) Perf Sub system APIs :
    //    These APIs checks DIOS state before changing DISPCLK source to VCO
    //    path. They disables DIOS before enabling DISPPLL.
    // 2) Clock APIs :
    //    These APIs doesnt check DIOS state.
    //
    // 99% tools/tests uses Perf APIs to change DISPCLK frequency. This is
    // "fail-safe" mechanism added to avoid crashes in test that are using
    // "Clock APIs".
    //
    if (!Gcx.bIsGpuDisplayless                               &&
        !diSeqIsDispclkOnAltPath_HAL(&Di))
    {
        pGc4->cmdStatus = RM_PMU_GCX_CMD_STATUS_BUSY;
    }

    // If we can't go into Deepidle right now, return
    if (pGc4->cmdStatus != RM_PMU_GCX_CMD_STATUS_OK)
    {
        return DidleLwrState;
    }

    nextState = DidleLwrState;

    //
    // Save off the request context information for the command that kicked
    // off Deepidle entry. We may be getting commands off the queue that
    // will overwrite the original dispatch structure. Hence save off the
    // content that we are interested in.
    //
    lwrCommand   = pGc4->dispatch.rpc;
    lwrEventType = pGc4->dispatch.hdr.eventType;

    //
    // If you hit this PMU ASSERT, a command is triggering GC4 entry.
    // This is not expected. Contact LPWR team
    //
    PMU_HALT_COND(!pGc4->bDisarmResponsePending);

    DBG_PRINTF_DIDLE(("DI: Current Command Response %u \n", bLwrCtxRequiresResponse, 0, 0, 0));
    // Assume the command will succeed.
    lwrCmdStatus = RM_PMU_GCX_CMD_STATUS_OK;

    status = s_gcxDidleGpuPowerDown_GM10X(targetState, &nextState, pDidleStats);
    // Check if the PowerDown Sequence Failed
    if (status == FLCN_ERROR)
    {
        return DIDLE_STATE_ARMED_OS;
    }
    //
    // Check if any abort event(returned by didleNextStateGet)
    // caused to result in ILWALID_STATE, then return the nextState
    //
    else if (status == FLCN_ERR_ILWALID_STATE)
    {
        return nextState;
    }
    else if (status == FLCN_OK)
    {
        // update successful entries
        pDidleStats->entries++;
    }

    // Update the cached Deepidle state
    pGc4->bInDIdle = LW_TRUE;

    return DIDLE_STATE_ARMED_OS;
}

/*!
 * @brief Bring the GPU out of GC4
 *
 * @param[in]     targetState   Desired end state on successful completion
 *                              of the exit steps
 * @param[in]     startSubState Exit substate at which to start. If we are
 *                              re-exitomg GC4 while in the middle of
 *                              entering, startSubState will be set to the
 *                              substate necessary to reverse the enter
 *                              process.
 *
 * @return targetState if the transition out of GC4 was completed,
 *         otherwise the next desired state of the next request in the queue
 *         is returned.
 */
LwU8
gcxGC4Exit_GM10X
(
    LwU8 targetState,
    LwU8 startSubState
)
{
    RM_PMU_DIDLE_STAT  *pDidleStats = NULL;
    OBJGC4             *pGc4        = GCX_GET_GC4(Gcx);

    // Return early if Common DI sequence is not supported
    if (!PMUCFG_FEATURE_ENABLED(PMU_DI_SEQUENCE_COMMON))
    {
        pGc4->cmdStatus = RM_PMU_GCX_CMD_STATUS_FAIL;
        return DidleLwrState;
    }

    // Validate target state
    if ((targetState != DIDLE_STATE_INACTIVE) &&
        (targetState != DIDLE_STATE_ARMED_OS))
    {
        //
        // Invalid target state. Return the current state to indicate that we
        // did not complete the state change.
        //
        pGc4->cmdStatus = RM_PMU_GCX_CMD_STATUS_FAIL;
        DBG_PRINTF_DIDLE(("DI: ERROR: Invalid EXIT target state: %d\n", targetState, 0, 0, 0));
        return DidleLwrState;
    }

    if ((DIDLE_STATE_IN_OS == DidleLwrState) ||
             (DIDLE_STATE_ENTERING_OS == DidleLwrState))
    {
        pDidleStats = &DidleStatisticsNH;
    }
    else
    {
        // Cannot figure out which Deepidle mode we're in.
        pGc4->cmdStatus = RM_PMU_GCX_CMD_STATUS_FAIL;
        DBG_PRINTF_DIDLE(("DI: ERROR: Cannot determine Deepidle mode\n", 0, 0, 0, 0));
        return DidleLwrState;
    }

    // start assuming that the recent command will get gpu into Deepidle
    pGc4->cmdStatus = RM_PMU_GCX_CMD_STATUS_OK;

    s_gcxDidleGpuPowerUp_GM10X(targetState, pDidleStats);

    // Update the current state
    DidleLwrState = targetState;

    // Update the cached Deepidle state
    pGc4->bInDIdle = LW_FALSE;

    // update statistics
    pDidleStats->exits++;

    return targetState;
}

/*!
 * @brief Checks whether GC4 entry is possible and returns the next valid state.
 *
 * @return the next valid end state resulting from the event or command. Internal
 *         states of *_ENTERING_* are not returned as they are intermediate
 *         states and not end states.
 */
LwU8
gcxGC4NextStateGet_GM10X(void)
{
    LwU8   nextState = DidleLwrState;

    //
    // If Deepidle has not yet been initialized, do not allow the
    // state to change.
    //
    if (!(DIDLE_IS_INITIALIZED(Gcx)))
    {
        return nextState;
    }

    if ((Gcx.pGc4->bInDeepL1)                   &&
        (DidleLwrState == DIDLE_STATE_ARMED_OS) &&
        (PG_IS_ENGAGED(RM_PMU_LPWR_CTRL_ID_MS_MSCG)))
    {
        //
        // For Deepidle NH, validate that there are no active heads before
        // allowing entry into Deepidle.
        //
        // No need to check for head inactivity on disp-less-chips. Rather
        // its dangerous to access disp registers on these chips.
        //
        if ((Gcx.bIsGpuDisplayless) ||
            (PMUCFG_FEATURE_ENABLED(PMU_DI_SEQUENCE_COMMON) &&
             diSeqAreHeadsInactive_HAL(&Di)))
        {
            nextState = DIDLE_STATE_IN_OS;
        }
    }

    return nextState;
}

/*!
 * @brief Set the GPIO based on DI pexSleepState
 *
 * @param[in]  pexSleepState    PEX sleep state - DI_PEX_SLEEP_STATE_xyz
 * @paran[in]  bEnable          LW_TRUE  Enable the GPIO
 *                              LW_FALSE Disable the GPIO
 */
void
gcxDidleSetGpio_GM10X
(
    LwU32  *pIrqEn,
    LwU32   pexSleepState,
    LwBool  bEnable
)
{
    switch (pexSleepState)
    {
        case DI_PEX_SLEEP_STATE_DEEP_L1:
        {
            bEnable ? PMU_SETB(pIrqEn, LW_CPWR_PMU_GPIO_INTR_RISING_EN_XVXP_DEEP_IDLE)
                    : PMU_CLRB(pIrqEn, LW_CPWR_PMU_GPIO_INTR_RISING_EN_XVXP_DEEP_IDLE);
            break;
        }
        case DI_PEX_SLEEP_STATE_L1_1:
        {
            bEnable ? PMU_SETB(pIrqEn, LW_PPWR_PMU_GPIO_1_INTR_RISING_EN_XP2PMU_STATE_IS_L1_1)
                    : PMU_CLRB(pIrqEn, LW_PPWR_PMU_GPIO_1_INTR_RISING_EN_XP2PMU_STATE_IS_L1_1);
            break;
        }
        case DI_PEX_SLEEP_STATE_L1_2:
        {
            bEnable ? PMU_SETB(pIrqEn, LW_PPWR_PMU_GPIO_1_INTR_RISING_EN_XP2PMU_STATE_IS_L1_2)
                    : PMU_CLRB(pIrqEn, LW_PPWR_PMU_GPIO_1_INTR_RISING_EN_XP2PMU_STATE_IS_L1_2);
            break;
        }
        default:
            break;
    }
}

/*!
 * Helper function to enable or disable deepL1/L1.1/L1.2 interrupts.
 * From Maxwell on, XP can goto any of Power Saving states, deepL1,
 * L1.1, L1.2, So, All the interrupts related to these power states
 * are enabled lwrrently
 *
 * @param[in]  bEnable     LW_TRUE - Enable rising and falling edge interrupt
 *                         LW_FALSE - Disable rising and falling edge interrupt
 */
void
gcxDidleDeepL1SetInterrupts_GM10X
(
    LwBool bEnable
)
{
    LwU32 regVal;
    LwU32 regVal1;

    if (bEnable)
    {
        appTaskCriticalEnter();
        {
            //
            // Clear the history of deepL1/L1.1/L1.2 Interrupts
            // Case 1: If current GPIO state is Low (not in any of these states deepL1, L1.1, L1.2)
            //    Clear history of both  respective Raising and Falling Edge Interrupts
            // Case 2: If current GPIO state is High (In one of these states deepL1, L1.1, L1.2)
            //    Clear history of respective Falling Edge Interrupts
            //

            // deepL1 Interrupts
            regVal = REG_RD32(CSB, LW_CPWR_PMU_GPIO_INPUT);
            if (FLD_TEST_DRF(_CPWR, _PMU_GPIO_INPUT, _XVXP_DEEP_IDLE, _FALSE, regVal))
            {
                regVal = REG_RD32(CSB, LW_CPWR_PMU_GPIO_INTR_RISING);
                regVal = FLD_SET_DRF(_CPWR, _PMU_GPIO_INTR_RISING,
                                        _XVXP_DEEP_IDLE, _PENDING, regVal);
                REG_WR32(CSB, LW_CPWR_PMU_GPIO_INTR_RISING, regVal);
            }

            regVal = REG_RD32(CSB, LW_CPWR_PMU_GPIO_INTR_FALLING);
            regVal = FLD_SET_DRF(_CPWR, _PMU_GPIO_INTR_FALLING,
                                 _XVXP_DEEP_IDLE, _PENDING, regVal);
            REG_WR32(CSB, LW_CPWR_PMU_GPIO_INTR_FALLING, regVal);

            // L1.1,L1.2 Interrupts
            regVal = REG_RD32(CSB, LW_CPWR_PMU_GPIO_1_INPUT);
            regVal1 = REG_RD32(CSB, LW_CPWR_PMU_GPIO_1_INTR_RISING);

            if (FLD_TEST_DRF(_CPWR, _PMU_GPIO_1_INPUT, _XP2PMU_STATE_IS_L1_1, _FALSE, regVal))
            {
                regVal1 = FLD_SET_DRF(_CPWR, _PMU_GPIO_1_INTR_RISING,
                                     _XP2PMU_STATE_IS_L1_1, _PENDING, regVal1);
            }

            if (FLD_TEST_DRF(_CPWR, _PMU_GPIO_1_INPUT, _XP2PMU_STATE_IS_L1_2, _FALSE, regVal))
            {
                regVal1 = FLD_SET_DRF(_CPWR, _PMU_GPIO_1_INTR_RISING,
                                        _XP2PMU_STATE_IS_L1_2, _PENDING, regVal1);
            }

            REG_WR32(CSB, LW_CPWR_PMU_GPIO_1_INTR_RISING, regVal1);

            regVal = REG_RD32(CSB, LW_CPWR_PMU_GPIO_1_INTR_FALLING);
            regVal = FLD_SET_DRF(_CPWR, _PMU_GPIO_1_INTR_FALLING,
                                 _XP2PMU_STATE_IS_L1_1, _PENDING, regVal);
            regVal = FLD_SET_DRF(_CPWR, _PMU_GPIO_1_INTR_FALLING,
                                 _XP2PMU_STATE_IS_L1_2, _PENDING, regVal);
            REG_WR32(CSB, LW_CPWR_PMU_GPIO_1_INTR_FALLING, regVal);
        }
        appTaskCriticalExit();
    }

    // Set the rising and falling  edge interrupt for deepL1
    gcxDidleDeepL1SetInterrupt_HAL(&Gcx, LW_TRUE, bEnable, DI_PEX_SLEEP_STATE_DEEP_L1);
    gcxDidleDeepL1SetInterrupt_HAL(&Gcx, LW_FALSE, bEnable, DI_PEX_SLEEP_STATE_DEEP_L1);

    // Set the rising and falling  edge interrupt for L1.1, L1.2
    gcxDidleDeepL1SetInterrupt_HAL(&Gcx, LW_TRUE, bEnable, DI_PEX_SLEEP_STATE_L1_1);
    gcxDidleDeepL1SetInterrupt_HAL(&Gcx, LW_TRUE, bEnable, DI_PEX_SLEEP_STATE_L1_2);
    gcxDidleDeepL1SetInterrupt_HAL(&Gcx, LW_FALSE, bEnable, DI_PEX_SLEEP_STATE_L1_1);
    gcxDidleDeepL1SetInterrupt_HAL(&Gcx, LW_FALSE, bEnable, DI_PEX_SLEEP_STATE_L1_2);
}

/*!
 * @brief Set Deep-L1 interrupts.
 *
 * The function enable or disable the DEEP_IDLE rising or falling edge
 * interrupts.
 *
 * @param[in]   bRising         Rising edge if LW_TRUE, falling edge if LW_FALSE
 * @param[in]   bEnable         Enable if LW_TRUE, disable if LW_FALSE
 * @param[in]   pexSleepState   PEX sleep state - DI_PEX_SLEEP_STATE_xyz
 */
void
gcxDidleDeepL1SetInterrupt_GM10X
(
    LwBool bRising,
    LwBool bEnable,
    LwU32  pexSleepState
)
{
    if (DI_PEX_SLEEP_STATE_DEEP_L1 == pexSleepState)
    {
        LwU32 regVal;

        appTaskCriticalEnter();
        {
            if (bRising)
            {
                regVal = REG_RD32(CSB, LW_CPWR_PMU_GPIO_INTR_RISING_EN);
                bEnable ?
                    PMU_SETB(&regVal,
                             LW_CPWR_PMU_GPIO_INTR_RISING_EN_XVXP_DEEP_IDLE) :
                    PMU_CLRB(&regVal,
                             LW_CPWR_PMU_GPIO_INTR_RISING_EN_XVXP_DEEP_IDLE);
                REG_WR32_STALL(CSB, LW_CPWR_PMU_GPIO_INTR_RISING_EN, regVal);
            }
            else
            {
                regVal = REG_RD32(CSB, LW_CPWR_PMU_GPIO_INTR_FALLING_EN);
                bEnable ?
                    PMU_SETB(&regVal,
                             LW_CPWR_PMU_GPIO_INTR_FALLING_EN_XVXP_DEEP_IDLE) :
                    PMU_CLRB(&regVal,
                             LW_CPWR_PMU_GPIO_INTR_FALLING_EN_XVXP_DEEP_IDLE);
                REG_WR32_STALL(CSB, LW_CPWR_PMU_GPIO_INTR_FALLING_EN, regVal);
            }
        }
        appTaskCriticalExit();
    }
    else
    {
        gcxDidleL1SubstatesSetInterrupt_HAL(&Gcx, bRising, bEnable, pexSleepState);
    }
}

/*!
 * @brief Set L1 substates interrupts.
 *
 * The function enable or disable the L1 substates rising or falling edge
 * interrupts.
 *
 * @param[in]   bRising         Rising edge if LW_TRUE, falling edge if LW_FALSE
 * @param[in]   bEnable         Enable if LW_TRUE, disable if LW_FALSE
 * @param[in]   pexSleepState   PEX sleep state - DI_PEX_SLEEP_STATE_xyz
 */
void
gcxDidleL1SubstatesSetInterrupt_GM10X
(
    LwBool bRising,
    LwBool bEnable,
    LwU32  pexSleepState
)
{
    LwU32 regVal;

    appTaskCriticalEnter();
    {
        if (bRising)
        {
            regVal = REG_RD32(CSB, LW_CPWR_PMU_GPIO_1_INTR_RISING_EN);
            gcxDidleSetGpio_HAL(&Gcx, &regVal, pexSleepState, bEnable);
            REG_WR32_STALL(CSB, LW_CPWR_PMU_GPIO_1_INTR_RISING_EN, regVal);
        }
        else
        {
            regVal = REG_RD32(CSB, LW_CPWR_PMU_GPIO_1_INTR_FALLING_EN);
            gcxDidleSetGpio_HAL(&Gcx, &regVal, pexSleepState, bEnable);
            REG_WR32_STALL(CSB, LW_CPWR_PMU_GPIO_1_INTR_FALLING_EN, regVal);
        }
    }
    appTaskCriticalExit();
}

/*!
 * @brief Sends MSCG aborted notification to DI-OS/DI-SSC
 *
 * This is helper function that sends MSCG aborted notification event from ISR
 * to DI-OS and DI-SSC. It will notify GCX task when ISR got MSCG abort OR
 * MSCG dis-engage interrupts.
 *
 * GCX task will abort DI-OS/DI-SSC as response to MSCG_ABORTED event.
 */
void
gcxDidleSendMsAbortedEvent_GM10X(void)
{
    DISPATCH_DIDLE dispatch;

    if (PMUCFG_FEATURE_ENABLED(PMU_DIDLE_OS))
    {
        if ((PG_IS_ENGAGED(RM_PMU_LPWR_CTRL_ID_MS_MSCG)) &&
            (DidleLwrState != DIDLE_STATE_INACTIVE))
        {
            dispatch.hdr.eventType = DIDLE_SIGNAL_EVTID;
            dispatch.sigGen.signalType = DIDLE_SIGNAL_MSCG_ABORTED;
            lwrtosQueueIdSendFromISR(LWOS_QUEUE_ID(PMU, GCX),
                                     &dispatch, sizeof(dispatch));
        }
    }
}

/* ------------------------- Private Functions ------------------------------ */

/*!
 * @brief Actual Power Down Sequence of GPU to GC5
 *
 * In between each step, check to see if we need to abort the transition
 * due to an incoming request. If so, exit and let the task function handle
 * getting us into the next state.
 *
 * @param[in]     targetState            Desired end state on successful completion
 *                                       of the entry steps
 * @param[in,out] nextState              Holds the Next State obtained from didleNextStateGet
 *
 * @param[in,out] pDidleStats            Pointer to statistics.
 *
 * @return        FLCN_OK                Entire Sequence is successful
 *                FLCN_ERROR             Sequence failure in Between
 *                FLCN_ERR_ILWALID_STATE  An event(returned by didleNextStateGet) oclwred and
 *                                       cannot continue PowerDown Sequence in this state.
 */
LwU8
s_gcxDidleGpuPowerDown_GM10X
(
    LwU8               targetState,
    LwU8               *nextState,
    RM_PMU_DIDLE_STAT  *pDidleStats
)
{
    OBJGC4       *pGc4   = GCX_GET_GC4(Gcx);
    DI_SEQ_CACHE *pCache = DI_SEQ_GET_CAHCE();
    LwBool        bAbortEntry = LW_FALSE;
    LwU32         entryStartTimeLow;
    LwU32         entryStartTimeHi;
    LwU32         entryEndTimeLow;
    LwU32         entryEndTimeHi;
    LwU32         totalEntryLatencyNs;
    LwU64         tempTimeU64;

    //
    // Check for MSCG state before starting power down sequence
    //
    // Its possible that MSCG got disengage at this point. Thus check the state
    // of MSCG. This is sequence -
    // 1) All pre-conditions for engaging GC5 are met. didleNextStateGet()
    //    callwlates next state as _IN_OS or _ENTERING_SSC
    // 2) MSCG wake-up interrupt came. GCX task is still at priority 1 thus it
    //    will get preempt.
    // 3) Control comes to PG task. PG execute MSCG.exit sequence.
    // 4) Control comes back to GCX task. GCX task priority will get raised to
    //    MAX_POSSIBLE.
    // 5) GC5 entry sequence starts.
    //
    if (!PG_IS_ENGAGED(RM_PMU_LPWR_CTRL_ID_MS_MSCG))
    {
        DI_SEQ_ABORT_REASON_UPDATE_NAME(LEGACY);
        return FLCN_ERROR;
    }

    //
    // clear the SCI interrupt status and BSI sequence status if
    // preconditions are true.
    //
    if (diSeqIsHostIdle_HAL(&Di) &&
        !diSeqIsIntrPendingGpu_HAL(&Di, pCache))
    {
        diSeqCopyClearSciPendingIntrs_HAL(&Di);
        diSeqBsiSequenceStatusClear_HAL(&Di);
    }
    else
    {
        return FLCN_ERROR;
    }

    // Capture the start time for profiling
    diSnapshotSciPtimer_HAL(&Di, &entryStartTimeLow, &entryStartTimeHi);

    // Initialize endTime
    entryEndTimeLow = entryStartTimeLow;

    DBG_PRINTF_DIDLE(("DI: GC5 PowerDown lwrr %u, tar %u\n", DidleLwrState, targetState, 0, 0));

    // Initialize sub state to _START
    DidleSubstate = DI_SEQ_STEP_INIT;

    // Process each step until we enter into DI state
    while ((DidleLwrState != DIDLE_STATE_IN_OS) &&
           (DidleLwrState != DIDLE_STATE_IN_SSC))
    {
        DBG_PRINTF_DIDLE(("GC5: lwrr state %u Substate %u\n", DidleLwrState, DidleSubstate, 0, 0));

        //
        // Execute next Sub State
        // nextSubState = (DidleSubstate + 1)
        //
        if ((DidleSubstate + 1) < DI_SEQ_STEP_LAST)
        {
            if (diSeqEnter_HAL(&Di, DidleSubstate + 1) == FLCN_OK)
            {
                DidleSubstate++;
            }
            else
            {
                bAbortEntry = LW_TRUE;
            }
        }
        else
        {
            //
            // The final step is turning off sppll and switching all
            // sys clks to xtal. Entry and exit for this step should be atomic
            // and there should not be context switches now if we have come
            // this far.
            //
            appTaskCriticalEnter();
            {
                if (diSeqEnter_HAL(&Di, DI_SEQ_STEP_LAST) == FLCN_OK)
                {
                    //
                    // We have completed last step without any aborts. Put Legacy
                    // DI state machine to either IN_OS or IN_SSC, depending on
                    // which we were armed for.
                    //
                    DidleLwrState = pGc4->bArmed ? DIDLE_STATE_IN_OS:
                                                   DIDLE_STATE_IN_SSC;

                    // Capture the DI entry end time for profiling
                    diSnapshotSciPtimer_HAL(&Di, &entryEndTimeLow, &entryEndTimeHi);

                    // Handoff control to SCI and halt PMU
                    diSeqIslandHandover_HAL(&Di);

                    // Capture the DI exit start time for profiling
                    diSnapshotSciPtimer_HAL(&Di, &gc5SwExitStartTimeLo, &entryEndTimeHi);
                    pCache->diExitStartTimeLo = gc5SwExitStartTimeLo;

                    // Immediately start exiting last step
                    diSeqExit_HAL(&Di, DI_SEQ_STEP_LAST);
                }
                else
                {
                    bAbortEntry = LW_TRUE;
                }
            }
            appTaskCriticalExit();
        }

        // We are still entering and theres possiblity of abort.
        if ((DidleLwrState != DIDLE_STATE_IN_OS) &&
            (DidleLwrState != DIDLE_STATE_IN_SSC))
        {
            //
            // Check for aborts due to interrupts after doing the substate.
            // Make sure that MSCG has not disengaged in every entry substate.
            // We foresee this happening if RTOS decides to fetch an overlay
            // from FB in ARMED_OS state and we don't send the disengage event
            // since it will be overkill to bombard the GCx task with messages
            // while DI-OS is armed (always at the supported pstate). We don't
            // foresee this situation to arise with DI-SSC, however keep this
            // check anyway to catch an abnormal condition.
            //
            if (diSeqIsIntrPendingGpu_HAL(&Di, pCache) ||
                diSeqIsIntrPendingSci_HAL(&Di, pCache))
            {
                bAbortEntry = LW_TRUE;
            }

            if (!PG_IS_ENGAGED(RM_PMU_LPWR_CTRL_ID_MS_MSCG))
            {
                DI_SEQ_ABORT_REASON_UPDATE_NAME(LEGACY);
                bAbortEntry = LW_TRUE;
            }

            *nextState = targetState;

            // Check to see if we need to back out due to a step failing.
            if (bAbortEntry)
            {
                DBG_PRINTF_DIDLE(("GC5: Pending Intr/MSCG disengaging %u\n", DidleLwrState, 0, 0, 0));
                //
                // If one of the steps failed and we need to back out,
                // figure out what the backed out state should be.
                //

                //
                // Copy clear the SCI pending interrupts so that we can send up the prev
                // intr status in case of an abort.
                //
                diSeqCopyClearSciPendingIntrs_HAL(&Di);

                return FLCN_ERROR;
            }

            //
            // If we don't need to back out due to a step failure, check to see
            // if the next incoming request will require a state change.
            //
            if (*nextState == targetState)
            {
                *nextState = didleNextStateGet(0);
            }

            //
            // If no state change is requred or we're already transitioning into
            // the next state, continue.  Otherwise, figure out whether we can
            // continue or need to abort.
            //
            if ((*nextState != DidleLwrState) &&
                (*nextState != targetState))
            {
                DBG_PRINTF_DIDLE(("GC5: Aborting entry seq %u\n", DidleLwrState, 0, 0, 0));

                //
                // Copy clear the SCI pending interrupts so that we can send up the prev
                // intr status in case of an abort.
                //
                diSeqCopyClearSciPendingIntrs_HAL(&Di);

                return FLCN_ERR_ILWALID_STATE;
            }

            //
            // If the last command should be ignored, return a failure response
            // because the outer function will not get this command's context if another
            // command comes in
            //
            if (pGc4->bDisarmResponsePending)
            {
                didleMessageDisarmOs(pGc4->cmdStatus);
            }
        }
    }  // while entering

    PMU_HALT_COND((DidleLwrState == DIDLE_STATE_IN_OS) ||
               (DidleLwrState == DIDLE_STATE_IN_SSC));

    //
    // Since time-spans are less than 4.29 secs, only the lower 32-bits of the
    // timestamps need to be compared. Subtracting these values will yield
    // the correct result. Also consider entry time taken by SCI.
    //
    totalEntryLatencyNs = (entryEndTimeLow - entryStartTimeLow) + (pCache->sciEntryLatencyUs * 1000);

    // Update Maximum entry latency
    if (totalEntryLatencyNs > pDidleStats->maxEntryLatencyNs)
    {
        pDidleStats->maxEntryLatencyNs = totalEntryLatencyNs;
    }

    //
    // RM expects the residency time in nano seconds. So colwerting
    // to nano seconds.
    //
    if (pCache->residentTimeUs < (LW_U32_MAX / 1000))
    {
        tempTimeU64 = (LwU64)(pCache->residentTimeUs * 1000);
        lw64Add((LwU64 *)&pDidleStats->residentTimeNs,
                (LwU64 *)&pDidleStats->residentTimeNs, &tempTimeU64);
    }
    else
    {
        // Reset on overflow.
        pCache->residentTimeUs = 0;
    }

    return FLCN_OK;
}

/*!
 * @brief Execute the GPU powerup sequence to bring GPU out of GC5
 *
 * @param[in]     targetState   Desired end state on successful completion
 *                              of the exit steps
 * @param[in,out] pDidleStats   Pointer to statistics.
 */
void
s_gcxDidleGpuPowerUp_GM10X
(
    LwU8                targetState,
    RM_PMU_DIDLE_STAT  *pDidleStats
)
{
    DI_SEQ_CACHE *pCache             = DI_SEQ_GET_CAHCE();
    LwU8          bSuccessfulEntry   = LW_TRUE;
    LwU32         totalExitLatencyNs = 0;
    LwU32         endTimeLo;
    LwU32         endTimeHi;

    DBG_PRINTF_DIDLE(("GC5: didleGPUPowerUp LwrrState %u targetState %u substate %u\n", DidleLwrState, targetState, startSubState, 0));

    // To exit deepidle, run through the states in reverse order
    while (DidleSubstate > DI_SEQ_STEP_INIT)
    {
        DBG_PRINTF_DIDLE(("GC5 exit: lwrr state %u Substate %u\n", DidleLwrState, DidleSubstate, 0, 0));

        //
        // Exit from current sub-state and update state variable if we exited
        // successfully  from current sub-state
        //
        diSeqExit_HAL(&Di, DidleSubstate);
        DidleSubstate--;
    }

    if ((Di.abortReasonMask & DI_SEQ_ABORT_REASON_MASK_SCI_SM) != 0)
    {
        bSuccessfulEntry = LW_FALSE;
    }

    // DI-OS should not use the following code.
    if (DI_IS_FEATURE_SUPPORTED(GPU_READY, Di.supportMask))
    {
        diSeqGpuReadyEventSend(bSuccessfulEntry, Di.abortReasonMask);
    }

    if (DidleLwrState == DIDLE_STATE_IN_OS)
    {
        // Capture the end time for profiling
        diSnapshotSciPtimer_HAL(&Di, &endTimeLo, &endTimeHi);

        //
        // Since time-spans are less than 4.29 secs, only the lower 32-bits of the
        // timestamps need to be compared. Subtracting these values will yield
        // the correct result. Also consider addition time taken by SCI.
        //
        totalExitLatencyNs = (endTimeLo - gc5SwExitStartTimeLo) +
                             (pCache->sciExitLatencyUs * 1000);

        // Update Maximum exit latency
        if (totalExitLatencyNs > pDidleStats->maxExitLatencyNs)
        {
            pDidleStats->maxExitLatencyNs = totalExitLatencyNs;
        }
    }
}
