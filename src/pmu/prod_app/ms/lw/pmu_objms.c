/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2011-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_objms.c
 * @brief  Memory System object providing ms related methods
 *
 * MS is superset of FB. Covers HUB MMU/XBAR/NISO/PA. Covers programming
 * unit2FB blockers, BAR blocker, gate/ungate memory clocks, interacts with
 * display unit to engage/disengage force_fill mode for CG.
 *
 * FB object covers features directly related to the framebuffer such as
 * FB start/stop, init training. Anything outside of FB/L2 but related to
 * the broader memory system should go in MS object.
 */

/* ------------------------ System includes -------------------------------- */
#include "pmusw.h"

/* ------------------------ Application includes --------------------------- */
#include "main.h"
#include "main_init_api.h"
#include "pmu_objhal.h"
#include "pmu_objlpwr.h"
#include "pmu_objms.h"
#include "pmu_objgcx.h"
#include "pmu_objfifo.h"
#include "pmu_objtimer.h"
#include "pmu_objdisp.h"
#include "pmu_objdi.h"
#include "clk3/clk3.h"
#include "pmu_swasr.h"
#include "pmu_cg.h"
#include "pmu_disp.h"
#include "pmu_objpmu.h"
#include "rmpbicmdif.h"
#include "dbgprintf.h"
#include "ctrl/ctrl2080/ctrl2080fb.h"

/* ------------------------ Application includes --------------------------- */
/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Global variables ------------------------------- */
OBJMS Ms;

/* ------------------------ Prototypes --------------------------------------*/
static LwU32       s_msMaxSupportedDramFreqKhzGet(void)
    GCC_ATTRIB_SECTION("imem_lpwr", "s_msMaxSupportedDramFreqKhzGet");

static void        s_msEiClientInit(void)
    GCC_ATTRIB_SECTION("imem_lpwrLoadin", "s_msEiClientInit");

static void        s_msDfprClientInit(void)
    GCC_ATTRIB_SECTION("imem_lpwrLoadin", "s_msDfprClientInit");

static void        s_msPrivBlockerInit(void)
    GCC_ATTRIB_SECTION("imem_lpwrLoadin", "s_msPrivBlockerInit");

static void        s_msClockGatingInit(void)
        GCC_ATTRIB_SECTION("imem_lpwrLoadin", "s_msClockGatingInit");

static FLCN_STATUS s_msLtcEntry(void)
                   GCC_ATTRIB_NOINLINE();
static FLCN_STATUS s_msLtcExit(void)
                   GCC_ATTRIB_NOINLINE();

static FLCN_STATUS s_msDifrSwAsrEntry(void)
                   GCC_ATTRIB_NOINLINE();
static FLCN_STATUS s_msDifrSwAsrExit(void)
                   GCC_ATTRIB_NOINLINE();

static FLCN_STATUS s_msDifrSwAsrEntrySeq(LwU8 *pStepId);
static FLCN_STATUS s_msDifrSwAsrExitSeq(LwU8 stepId, LwBool bAbort);

static FLCN_STATUS s_msDifrCgEntry(void)
                   GCC_ATTRIB_NOINLINE();
static FLCN_STATUS s_msDifrCgExit(void)
                   GCC_ATTRIB_NOINLINE();

static FLCN_STATUS s_msDifrCgEntrySeq(LwU8 *pStepId);
static FLCN_STATUS s_msDifrCgExitSeq(LwU8 stepId, LwBool bAbort);

static FLCN_STATUS s_msMscgEntry(void)
                   GCC_ATTRIB_NOINLINE();
static FLCN_STATUS s_msMscgExit(void)
                   GCC_ATTRIB_NOINLINE();
static FLCN_STATUS s_msMscgReset(void)
                   GCC_ATTRIB_NOINLINE();

static FLCN_STATUS s_msMscgEntrySeq(LwU8 *pStepId);
static FLCN_STATUS s_msMscgExitSeq(LwU8 stepId, LwBool bAbort);

/* ------------------------ Macros ----------------------------------------- */

/*!
 * Macro to define the ISO only Wakeup Event
 */
#define MS_ISO_WAKEUP_MASK                \
        (PG_WAKEUP_EVENT_MS_ISO_BLOCKER | \
         PG_WAKEUP_EVENT_MS_ISO_STUTTER)

/*!
 * Clock Gating Mask for MS Group Features
 */
#define MS_CG_MASK_SW_ASR (LW2080_CTRL_CLK_DOMAIN_MCLK)
#define MS_CG_MASK_CG     (CLK_DOMAIN_MASK_GPC | CLK_DOMAIN_MASK_XBAR | CLK_DOMAIN_MASK_LTC)

/* ------------------------ Local Functions  ------------------------------- */
/* ------------------------ Public Functions  ------------------------------ */

/*!
 * Construct the MS object.  This sets up the HAL interface used by the
 * MS module.
 *
 * @return  FLCN_OK     On success
 */
FLCN_STATUS
constructMs(void)
{
    // Add actions here to be performed prior to pg task is scheduled
    return FLCN_OK;
}

/*!
 * @brief MS initialization post init
 *
 * Initialization of MS related structures immediately after scheduling
 * the pg task in scheduler.
 */
FLCN_STATUS
msPostInit(void)
{
    LwU32 railId;

    // All members are by default initialize to Zero.
    memset(&Ms, 0, sizeof(Ms));

    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_GRP_MS))
    {
        // Initialize Non-Zero members
        Ms.abortTimeoutNs = MS_DEFAULT_ABORT_TIMEOUT0_NS;

        // This field is needed only for current aware PSI
        if (PMUCFG_FEATURE_ENABLED(PMU_PSI_MULTIRAIL) &&
            PMUCFG_FEATURE_ENABLED(PMU_PSI_FLAVOUR_LWRRENT_AWARE))
        {
            // Initialize the coefficient for all the rails
            for (railId = 0; railId < RM_PMU_PSI_RAIL_ID__COUNT; railId++)
            {
                Ms.dynamicLwrrentCoeff[railId] = LW_U16_MAX;
            }
        }

        //
        // MS_LTC is CheetAh specific feature and does not need SW ASR
        // SW ASR is used by MS Group
        // We need to skip this using proper Group APIs.
        //
        Ms.pSwAsr = lwosCallocType(OVL_INDEX_DMEM(lpwr), 1, OBJSWASR);
        if (Ms.pSwAsr == NULL)
        {
            PMU_BREAKPOINT();
            return FLCN_ERR_NO_FREE_MEM;
        }

        // Cache for Clock Gating registers
        Ms.pClkGatingRegs = lwosCallocType(OVL_INDEX_DMEM(lpwr), 1, CLKGATINGREGS);
        if (Ms.pClkGatingRegs == NULL)
        {
            PMU_BREAKPOINT();
            return FLCN_ERR_NO_FREE_MEM;
        }
    }

    // Allocate the structure for MS_RPG
    if (PMUCFG_FEATURE_ENABLED(PMU_MS_RPG))
    {
        if (Ms.pMsRpg == NULL)
        {
            Ms.pMsRpg = lwosCallocType(OVL_INDEX_DMEM(lpwr), 1, MS_RPG);
            if (Ms.pMsRpg == NULL)
            {
                PMU_BREAKPOINT();
                return FLCN_ERR_NO_FREE_MEM;
            }
        }
    }

    // Allocate the structure for DIFR Layer2/Layer3
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_DIFR))
    {
        if (Ms.pMsDifr == NULL)
        {
            Ms.pMsDifr = lwosCallocType(OVL_INDEX_DMEM(lpwr), 1, MS_DIFR);
            if (Ms.pMsDifr == NULL)
            {
                PMU_BREAKPOINT();
                return FLCN_ERR_NO_FREE_MEM;
            }
        }
    }
    return FLCN_OK;
}

/*!
 * @brief Initialize MSCG parameters
 */
void
msMscgInit
(
    RM_PMU_RPC_STRUCT_LPWR_LOADIN_PG_CTRL_INIT *pParams
)
{
    OBJPGSTATE *pMsState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_MS_MSCG);

    // Initialize PG interfaces for MS
    pMsState->pgIf.lpwrEntry = s_msMscgEntry;
    pMsState->pgIf.lpwrExit  = s_msMscgExit;
    pMsState->pgIf.lpwrReset = s_msMscgReset;

    //
    // TODO-pankumar: We need to move below data out of MSCG and
    // move it to LPWR MS Group initialization code
    //
    // Check whether abortTimeout has valid value before updating it.
    if (pParams->params.ms.abortTimeoutUs != 0)
    {
        Ms.abortTimeoutNs  = pParams->params.ms.abortTimeoutUs * 1000;
    }

    Ms.psiSettleTimeNs     = pParams->params.ms.psiSettleTimeUs * 1000;

    //
    // If Multirail psi is supported, update the flag for
    // MSCG coupled PSI exit sequential with MSCG exit sequence
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_PSI_MULTIRAIL))
    {
        Ms.bPsiSequentialExit = pParams->params.ms.bMsPsiSequentialExit;
    }

    // Initialize MS FSM parameters

    // MS dont need "power gating" delays
    pgCtrlPgDelaysDisable_HAL(&Pg, RM_PMU_LPWR_CTRL_ID_MS_MSCG);

    // Initialize the Holdoff Mask
    msHoldoffMaskInit_HAL(&Ms);

    // Initialize the Idle Mask
    msMscgIdleMaskInit_HAL(&Ms);

    // Check the display; dispInit must be called before this function.
    if (!PMUCFG_ENGINE_ENABLED(DISP)    ||
        !DISP_IS_PRESENT())
    {
        pMsState->supportMask = LPWR_SF_MASK_CLEAR(MS, DISPLAY, pMsState->supportMask);
    }

    if (!LPWR_CTRL_IS_SF_SUPPORTED(pMsState, MS, DISPLAY))
    {
        // If display is disabled, always disable ISO and DWB.
        pMsState->supportMask = LPWR_SF_MASK_CLEAR(MS, ISO_STUTTER, pMsState->supportMask);
        pMsState->supportMask = LPWR_SF_MASK_CLEAR(MS, DWB, pMsState->supportMask);
    }

    // Override entry conditions by updating entry equation of MSCG.
    msEntryConditionConfig_HAL(&Ms, RM_PMU_LPWR_CTRL_ID_MS_MSCG);

    // Enable holdoff interrupts
    pgEnableHoldoffIntr_HAL(&Pg, pMsState->holdoffMask, LW_TRUE);

    // Init XVE Blocker for MSCG if unified infra is not enabled
    if (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_UNIFIED_PRIV_BLOCKER_CTRL))
    {
        s_msPrivBlockerInit();
    }
}

/*!
 * @brief MS Group Common Initialization
 */
void
msGrpPostInit(void)
{
    // Enable MS Wakeup interrupts
    msInitAndEnableIntr_HAL(&Ms);

    // Initialize the Active FBIO Mask
    msInitActiveFbios_HAL(&Ms);

    // Initialize the FB Params
    msInitFbParams_HAL(&Ms);

    // Initialize force fill mode
    msDispForceFillInit_HAL(&Ms);

    // Initialize Clock Gating Mask for MS Group
    s_msClockGatingInit();

    // If EI is enabled, update the MS Group SFM Bit mask for EI
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_CTRL_EI))
    {
        s_msEiClientInit();
    }

    // if DPFR is enabled, update the MS Group Mutual Exclusion
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_CTRL_DFPR))
    {
        s_msDfprClientInit();
    }
}

/*!
 * @brief Initialize MS parameters
 */
void
msLtcInit
(
    RM_PMU_RPC_STRUCT_LPWR_LOADIN_PG_CTRL_INIT *pParams
)
{
    OBJPGSTATE *pMsLtcState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_MS_LTC);

    // Initialize PG interfaces for MS-LTC
    pMsLtcState->pgIf.lpwrEntry = s_msLtcEntry;
    pMsLtcState->pgIf.lpwrExit  = s_msLtcExit;

    // Set Idle and Holdoff masks
    msLtcIdleMaskInit_HAL(&Ms);
    msLtcHoldoffMaskInit_HAL(&Ms);

    // Configure entry conditions of MS-LTC
    msLtcEntryConditionConfig_HAL(&Ms);

    // Initialize and Enable MISCIO interrupts
    msLtcInitAndEnableIntr_HAL(&Ms);

    // Enable holdoff interrupts
    pgEnableHoldoffIntr_HAL(&Pg, pMsLtcState->holdoffMask, LW_TRUE);
}

/*!
 * @brief Event/Interrupt Handler for MSCG
 *
 * This event handler is responsible for translating raw MSCG events to the
 * PG event format which other PG Logics are built upon.
 *
 * @param[in]   pLpwrEvt        The source of all PG events
 * @param[out]  pPgLogicState   Pointer to PG Logic state
 *
 * @return      FLCN_OK                                 On success
 * @return      FLCN_ERR_ILWALID_ARGUMENT               Invalid argument for given event
 * @return      FLCN_ERR_QUEUE_TASK_ILWALID_EVENT_TYPE  Otherwise
 */
FLCN_STATUS
msProcessInterrupts
(
    DISPATCH_LPWR   *pLpwrEvt,
    PG_LOGIC_STATE  *pPgLogicState
)
{
    DISP_RM_ONESHOT_CONTEXT* pRmOneshotCtx = DispContext.pRmOneshotCtx;
    FLCN_STATUS              status        = FLCN_OK;
    LwU32                    eventId       = PMU_PG_EVENT_NONE;
    LwU32                    data          = 0;
    LwU32                    lpwrCtrlId    = 0;
    LwBool                   bIsoOnlyWakeup = LW_FALSE;

    switch (pLpwrEvt->hdr.eventType)
    {
        case LPWR_EVENT_ID_MS_WAKEUP_INTR:
        {
            // Colwert MS_WAKEUP_INTR to WAKEUP event
            eventId = PMU_PG_EVENT_WAKEUP;
            data    = msColwertIntrsToWakeUpMask_HAL(&Ms, pLpwrEvt->intr.intrStat);

            // If its a ISO only Wakeup, set the boolean
            if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_CTRL_MS_DIFR_SW_ASR)    &&
                pgIsCtrlSupported(RM_PMU_LPWR_CTRL_ID_MS_DIFR_SW_ASR))
            {
                // Check if we only have ISO Wakeup request only
                if ((data != 0) &&
                    (data & (~MS_ISO_WAKEUP_MASK)) == 0)
                {
                    bIsoOnlyWakeup = LW_TRUE;
                }
            }

            //
            // Re-enable the HW wake-up interrupts if MS Group Feature
            // are not doing the state transitions. Otherwise set the pending bit.
            //
            if (!lpwrGrpCtrlIsInTransition(RM_PMU_LPWR_GRP_CTRL_ID_MS))
            {
                msEnableIntr_HAL(&Ms);
            }
            else
            {
                Ms.bWakeEnablePending = LW_TRUE;
            }

            break;
        }

        case LPWR_EVENT_ID_MS_ABORT_DMA_SUSP:
        {
            eventId = PMU_PG_EVENT_WAKEUP;
            data    = PG_WAKEUP_EVENT_MS_DMA_SUSP_ABORT;

            break;
        }

        case LPWR_EVENT_ID_MS_DISP_INTR:
        {
            if (pLpwrEvt->intr.intrStat == RM_PMU_DISP_INTR_SV_1)
            {
                eventId = PMU_PG_EVENT_DISALLOW;
                data    = PG_CTRL_DISALLOW_REASON_MASK(SV_INTR);

                // Set SV1 serviced flag
                Ms.bSv1Serviced = LW_TRUE;
            }
            else if (pLpwrEvt->intr.intrStat == RM_PMU_DISP_INTR_SV_3)
            {
                // Skip queuing up ALLOW event, if SV1 was not able to queue DISALLOW
                if (Ms.bSv1Serviced)
                {
                    eventId = PMU_PG_EVENT_ALLOW;
                    data    = PG_CTRL_DISALLOW_REASON_MASK(SV_INTR);

                    // Reset SV1 serviced flag
                    Ms.bSv1Serviced = LW_FALSE;
                }

                //
                // There can be a case where we get SV3 before OSM state change
                // command to disable OSM. In HW, the OSM will get disabled in
                // SV2 itself. So we need to disable NISO MSCG i.e. enable
                // ISO_STUTTER explicitly here.
                //
                if (PMUCFG_FEATURE_ENABLED(PMU_MS_OSM) &&
                    pRmOneshotCtx != NULL)
                {
                    if (dispRmOneshotStateUpdate_HAL(&Disp))
                    {
                        // Enable ISO_STUTTER sub feature if OSM is not active
                        pgCtrlSubfeatureMaskRequest(RM_PMU_LPWR_CTRL_ID_MS_MSCG,
                                                    LPWR_CTRL_SFM_CLIENT_ID_OSM,
                                                    LW_U32_MAX);
                    }
                }
            }
            else
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_ILWALID_ARGUMENT;
            }

            break;
        }

#if PMUCFG_FEATURE_ENABLED(PMU_MS_OSM)
        case LPWR_EVENT_ID_MS_OSM_NISO_START:
        {
            if ((pRmOneshotCtx != NULL) && pRmOneshotCtx->bActive &&
                 pRmOneshotCtx->bNisoMsActive)
            {

                //
                // If OSM is active and NISO start event is received then
                // we need to disable the ISO_STUTTER from MSCG sequence.
                // Clear the ISO_STUTTER sub feature from LW_U32_MAX mask
                // and then send.
                //
                pgCtrlSubfeatureMaskRequest(RM_PMU_LPWR_CTRL_ID_MS_MSCG,
                                            LPWR_CTRL_SFM_CLIENT_ID_OSM,
                                            LPWR_SF_MASK_CLEAR(MS, ISO_STUTTER, LW_U32_MAX));
            }
            break;
        }

        case LPWR_EVENT_ID_MS_OSM_NISO_END:
        {
            if (pRmOneshotCtx != NULL)
            {
                //
                // If we received NISO_END interrupt then we need to enable
                // the ISO_STUTTER in MSCG sequence. Pass the mask as
                // LW_U32_MAX to enable the subfeature.
                //
                // There might be a scenario that ISO_STUTTER is already
                // enabled. In such scenario there will be no change in
                // subfeature mask and this API will return a LW_FALSE.
                //
                // If there is a valid change in subfeature mask, this API will
                // return a LW_TRUE, then ack will be send by SFM API itself.
                //
                if (!pgCtrlSubfeatureMaskRequest(RM_PMU_LPWR_CTRL_ID_MS_MSCG,
                                                 LPWR_CTRL_SFM_CLIENT_ID_OSM, LW_U32_MAX))
                {
                    // Send the PBI ack immediately for OSM1.0
                    if (PMUCFG_FEATURE_ENABLED(PMU_MS_OSM_1) &&
                        pRmOneshotCtx->bPbiAckPending)
                    {
                        pmuPbiSendAck_HAL(&Pmu, LW_PBI_COMMAND_STATUS_SUCCESS);

                        // Clear ACK pending variable
                        pRmOneshotCtx->bPbiAckPending = LW_FALSE;
                    }
                }
            }
            break;
        }
#endif // PMUCFG_FEATURE_ENABLE(PMU_MS_OSM)

        default:
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_QUEUE_TASK_ILWALID_EVENT_TYPE;
        }
    }

    // Send or colwert the event as per need
    if (eventId != PMU_PG_EVENT_NONE)
    {
        FOR_EACH_INDEX_IN_MASK(32, lpwrCtrlId, lpwrGrpCtrlMaskGet(RM_PMU_LPWR_GRP_CTRL_ID_MS))
        {
            //
            // If LpwrCtrl is already in FULL Power Mode, do not send the event
            // further, Just ignore it, if the current event is Wakeup only event
            //
            // We should not ignore the Allow/disallow events
            //
            if ((eventId == PMU_PG_EVENT_WAKEUP) &&
                PG_IS_FULL_POWER(lpwrCtrlId))
            {
                continue;
            }

            // If its a ISO only Wakeup, then do not wake DIFR SW_ASR FSM
            if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_CTRL_MS_DIFR_SW_ASR)  &&
                pgIsCtrlSupported(RM_PMU_LPWR_CTRL_ID_MS_DIFR_SW_ASR) &&
                (lpwrCtrlId == RM_PMU_LPWR_CTRL_ID_MS_DIFR_SW_ASR)    &&
                (bIsoOnlyWakeup))
            {
                continue;
            }

            pgCtrlEventColwertOrSend(pPgLogicState, lpwrCtrlId, eventId, data);
        }
        FOR_EACH_INDEX_IN_MASK_END;
    }

    return status;
}

/*!
 * @brief Check if a message is pending in the LWOS_QUEUE(PMU, LPWR)
 */
LwBool
msWakeupPending(void)
{
    FLCN_STATUS   status;
    LwU32         numItems;

    status = lwrtosQueueGetLwrrentNumItems(LWOS_QUEUE(PMU, LPWR), &numItems);

    if (status != FLCN_OK)
    {
        PMU_HALT();
    }

    return numItems > 0;
}

/*!
 * @brief Compute remaining timeout
 *
 * @param[in]  timeout        original timeout value
 * @param[in]  pStartTime     the start time for the sequence
 * @param[out] pTimeoutLeftNs timeout left
 *
 * @return LW_TRUE      if timing out
 *         LW_FALSE     otherwise
 */
LwBool
msCheckTimeout
(
    LwU32           timeoutNs,
    FLCN_TIMESTAMP *pStartTimeNs,
    LwS32          *pTimeoutLeftNs
)
{
    *pTimeoutLeftNs = timeoutNs - osPTimerTimeNsElapsedGet(pStartTimeNs);
    return (*pTimeoutLeftNs <= 0);
}

/*!
 * @brief Gate the clocks
 */
void
msClkGate
(
    LwU8 ctrlId
)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(ctrlId);

    //
    // GR-ELCG <-> OSM CG WAR
    // Disable GR-ELCG before gating clock from OSM
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_CG_GR_ELCG_OSM_WAR))
    {
        lpwrCgElcgDisable(PMU_ENGINE_GR, LPWR_CG_ELCG_CTRL_REASON_MS);
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_MS_CG_STOP_CLOCK) &&
        LPWR_CTRL_IS_SF_ENABLED(pPgState, MS, CG_STOP_CLOCK))
    {
        msStopClockGate_HAL(&Ms);
    }
    else
    {
        lpwrCgPowerdown_HAL(&Lpwr, Ms.cgMask, LPWR_CG_TARGET_MODE_GATED);
    }
}

/*!
 * @brief Ungate the clocks
 */
void
msClkUngate
(
    LwU8 ctrlId
)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(ctrlId);

    if (PMUCFG_FEATURE_ENABLED(PMU_MS_CG_STOP_CLOCK) &&
        LPWR_CTRL_IS_SF_ENABLED(pPgState, MS, CG_STOP_CLOCK))
    {
        msStopClockUngate_HAL(&Ms);
    }
    else
    {
        // Restore all clocks to Cached Modes.
        lpwrCgPowerup_HAL(&Lpwr, Ms.cgMask, LPWR_CG_TARGET_MODE_ORG);
    }

    //
    // GR-ELCG <-> OSM CG WAR
    // Re-enable GR-ELCG before gating clock from OSM
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_CG_GR_ELCG_OSM_WAR))
    {
        lpwrCgElcgEnable(PMU_ENGINE_GR, LPWR_CG_ELCG_CTRL_REASON_MS);
    }
}

/*!
 * @brief Initialization of DIFR SW ASR FSM
 */
void
msDifrSwAsrInit()
{
    OBJPGSTATE *pDifrSwAsrState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_MS_DIFR_SW_ASR);

    // Initialize FSM Sequence interfaces for DIFR SW-ASR
    pDifrSwAsrState->pgIf.lpwrEntry = s_msDifrSwAsrEntry;
    pDifrSwAsrState->pgIf.lpwrExit  = s_msDifrSwAsrExit;

    // Initialize IdleMask for DIFR_SWA_SR
    msDifrIdleMaskInit_HAL(&Ms, RM_PMU_LPWR_CTRL_ID_MS_DIFR_SW_ASR);

    // Initiailze the holdoff Mask
    msDifrSwAsrHoldoffMaskInit();

    // Update the entry conditions for DIFR_SW_ASR.
    msEntryConditionConfig_HAL(&Ms, RM_PMU_LPWR_CTRL_ID_MS_DIFR_SW_ASR);

    // Enable holdoff interrupts
    pgEnableHoldoffIntr_HAL(&Pg, pDifrSwAsrState->holdoffMask, LW_TRUE);
}

/*!
 * @brief Initialization of DIFR SW ASR FSM
 */
void
msDifrCgInit()
{
    OBJPGSTATE *pDifrCgState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_MS_DIFR_CG);

    // Initialize FSM Sequence interfaces for DIFR SW-ASR
    pDifrCgState->pgIf.lpwrEntry = s_msDifrCgEntry;
    pDifrCgState->pgIf.lpwrExit  = s_msDifrCgExit;

    // Initialize IdleMask for DIFR_CG
    msDifrIdleMaskInit_HAL(&Ms, RM_PMU_LPWR_CTRL_ID_MS_DIFR_CG);

    // Update the entry conditions for DIFR_CG
    msEntryConditionConfig_HAL(&Ms, RM_PMU_LPWR_CTRL_ID_MS_DIFR_CG);
}

/*!
 * @brief Initializes the Holdoff mask for DIFR SW-ASR FSM
 */
void
msDifrSwAsrHoldoffMaskInit(void)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_MS_DIFR_SW_ASR);
    LwU32       hm       = 0;

    if (FIFO_IS_ENGINE_PRESENT(GR))
    {
        hm |= BIT(GET_FIFO_ENG(PMU_ENGINE_GR));
    }
    if (FIFO_IS_ENGINE_PRESENT(CE0))
    {
        hm |= BIT(GET_FIFO_ENG(PMU_ENGINE_CE0));
    }
    if (FIFO_IS_ENGINE_PRESENT(CE1))
    {
        hm |= BIT(GET_FIFO_ENG(PMU_ENGINE_CE1));
    }
    if (FIFO_IS_ENGINE_PRESENT(CE2))
    {
        hm |= BIT(GET_FIFO_ENG(PMU_ENGINE_CE2));
    }
    if (FIFO_IS_ENGINE_PRESENT(CE3))
    {
        hm |= BIT(GET_FIFO_ENG(PMU_ENGINE_CE3));
    }
    if (FIFO_IS_ENGINE_PRESENT(CE4))
    {
        hm |= BIT(GET_FIFO_ENG(PMU_ENGINE_CE4));
    }
    if (FIFO_IS_ENGINE_PRESENT(CE5))
    {
        hm |= BIT(GET_FIFO_ENG(PMU_ENGINE_CE5));
    }
    //  LWDEC = BSP  (reusing MSVLD code for LWDEC)
    if (FIFO_IS_ENGINE_PRESENT(BSP))
    {
        hm |= BIT(GET_FIFO_ENG(PMU_ENGINE_BSP));
    }
    if (FIFO_IS_ENGINE_PRESENT(DEC1))
    {
        hm |= BIT(GET_FIFO_ENG(PMU_ENGINE_DEC1));
    }
    if (FIFO_IS_ENGINE_PRESENT(DEC2))
    {
        hm |= BIT(GET_FIFO_ENG(PMU_ENGINE_DEC2));
    }
#if PMUCFG_FEATURE_ENABLED(PMU_HOST_ENGINE_EXPANSION)
    if (FIFO_IS_ENGINE_PRESENT(DEC3))
    {
        hm |= BIT(GET_FIFO_ENG(PMU_ENGINE_DEC3));
    }
#endif
    if (FIFO_IS_ENGINE_PRESENT(ENC0))
    {
        hm |= BIT(GET_FIFO_ENG(PMU_ENGINE_ENC0));
    }
    if (FIFO_IS_ENGINE_PRESENT(ENC1))
    {
        hm |= BIT(GET_FIFO_ENG(PMU_ENGINE_ENC1));
    }
    if (FIFO_IS_ENGINE_PRESENT(ENC2))
    {
        hm |= BIT(GET_FIFO_ENG(PMU_ENGINE_ENC2));
    }
    if (FIFO_IS_ENGINE_PRESENT(SEC2))
    {
        hm |= BIT(GET_FIFO_ENG(PMU_ENGINE_SEC2));
    }
#if PMUCFG_FEATURE_ENABLED(PMU_HOST_ENGINE_EXPANSION)
    if (FIFO_IS_ENGINE_PRESENT(OFA0))
    {
        hm |= BIT(GET_FIFO_ENG(PMU_ENGINE_OFA0));
    }
    if (FIFO_IS_ENGINE_PRESENT(JPG0))
    {
        hm |= BIT(GET_FIFO_ENG(PMU_ENGINE_JPG0));
    }
#endif
    pPgState->holdoffMask = hm;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief Context save interface for LPWR_ENG(MS_LTC)
 *
 * @return FLCN_OK      Success.
 * @return FLCN_ERROR   Otherwise
 */
static FLCN_STATUS
s_msLtcEntry(void)
{
    FLCN_STATUS status = FLCN_ERROR;

    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_CTRL_MS_LTC))
    {
        status = msLtcEntry_HAL(&Gr);
    }

    return status;
}

/*!
 * @brief Context restore interface for LPWR_ENG(MS_LTC)
 *
 * @return FLCN_OK      Success.
 * @return FLCN_ERROR   Otherwise
 */
static FLCN_STATUS
s_msLtcExit(void)
{
    FLCN_STATUS status = FLCN_ERROR;

    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_CTRL_MS_LTC))
    {
        status = msLtcExit_HAL(&Gr);
    }

    return status;
}

/*!
 * @brief Get the Max DRAM clock in Khz on which MSCG is supported
 */
LwU32
s_msMaxSupportedDramFreqKhzGet(void)
{
    OBJSWASR *pSwAsr  = MS_GET_SWASR();
    LwU32     mclkKHz = 0;

    switch (pSwAsr->dramType)
    {
        // For GDDR5/GDDR5X/HBM we can accommodate a maximum of 1GHZ when enabling MSCG
        case LW2080_CTRL_FB_INFO_RAM_TYPE_GDDR6:
        case LW2080_CTRL_FB_INFO_RAM_TYPE_GDDR5X:
        case LW2080_CTRL_FB_INFO_RAM_TYPE_GDDR5:
        {
            mclkKHz = RM_PMU_PG_MS_MAX_MCLK_FREQ_AT_WHICH_MSCG_ALLOWED_GDDR5_KHZ;
            break;
        }
        case LW2080_CTRL_FB_INFO_RAM_TYPE_SDDR3:
        {
            mclkKHz = RM_PMU_PG_MS_MAX_MCLK_FREQ_AT_WHICH_MSCG_ALLOWED_SDDR3_KHZ;
            break;
        }
        case LW2080_CTRL_FB_INFO_RAM_TYPE_SDDR4:
        {
            mclkKHz = RM_PMU_PG_MS_MAX_MCLK_FREQ_AT_WHICH_MSCG_ALLOWED_SDDR4_KHZ;
            break;
        }
        default:
        {
            PMU_BREAKPOINT();
        }
    }
    return mclkKHz;
}

/*!
 * @brief Initialize the MS SFM for EI Client
 *
 */
void
s_msEiClientInit(void)
{
    OBJPGSTATE *pPgState;
    LwU32       msEiSfmInitMask;
    LwU8        lpwrCtrlId;

    msEiSfmInitMask = RM_PMU_LPWR_CTRL_SFM_VAL_DEFAULT;

    //
    // Keep the default state of RPG as disabled in SFM enabledMask for SFM EI Client.
    //
    // This mask will be updated at run time with EI Entry/exit notification
    // in the function s_lpwrEiNotificationMsRpgHandler
    //
    msEiSfmInitMask = LPWR_SF_MASK_CLEAR(MS, RPG, msEiSfmInitMask);

    // Keep the Clock Slow down feature disabled for EI Client
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_CLK_SLOWDOWN))
    {
        msEiSfmInitMask = LPWR_SF_MASK_CLEAR(MS, CLK_SLOWDOWN_SYS, msEiSfmInitMask);
        msEiSfmInitMask = LPWR_SF_MASK_CLEAR(MS, CLK_SLOWDOWN_LWD, msEiSfmInitMask);
    }

    // Loop over all the MS Group LPWR_CTRLs and set the SFM bit mask for EI
    FOR_EACH_INDEX_IN_MASK(32, lpwrCtrlId, lpwrGrpCtrlMaskGet(RM_PMU_LPWR_GRP_CTRL_ID_MS))
    {
        pPgState = GET_OBJPGSTATE(lpwrCtrlId);

        // Set the default value of enableMask for EI SFM Client
        pPgState->enabledMaskClient[LPWR_CTRL_SFM_CLIENT_ID_EI_NOTIFICATION] = msEiSfmInitMask;
    }
    FOR_EACH_INDEX_IN_MASK_END;
}

/*!
 * @brief Initialize the MS Group Mutual Exclusion with DFPR
 *
 */
void
s_msDfprClientInit(void)
{
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_GRP_MUTUAL_EXCLUSION) &&
        pgIsCtrlSupported(RM_PMU_LPWR_CTRL_ID_DFPR)           &&
        LPWR_ARCH_IS_SF_SUPPORTED(DFPR_COUPLED_DIFR_SW_ASR))
    {
        //
        // DIFR_SW_ASR (Layer 2) and DIFR_CG (Layer 3) are kept disabled
        // until DFPR is engaged. Initialize mutual exclusion policy settings
        // to keep these FSM disabled after boot.
        //
        lpwrGrpCtrlMutualExclusion(RM_PMU_LPWR_GRP_CTRL_ID_MS,
                                   LPWR_GRP_CTRL_CLIENT_ID_DFPR,
                                   MS_DIFR_FSM_MASK);
    }
}

/*!
 * @brief Perform MS Subfeature specific actions.
 *
 * Note: This function must be called from the
 * pgCtrlSubfeatureMaskUpdate() API only.
 */
void
msSfmHandler
(
    LwU8 ctrlId
)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(ctrlId);
    LwBool      bAddIsoStutter;

    // If there is any ack pending for OSM, send the ack.
    if (PMUCFG_FEATURE_ENABLED(PMU_MS_OSM))
    {
        DISP_RM_ONESHOT_CONTEXT *pRmOneshotCtx = DispContext.pRmOneshotCtx;

        // If PBI ACK pending variable is set, send PBI ACK to RM
        if ((PMUCFG_FEATURE_ENABLED(PMU_MS_OSM_1)) &&
            pRmOneshotCtx                          &&
            pRmOneshotCtx->bPbiAckPending)
        {
            pmuPbiSendAck_HAL(&Pmu, LW_PBI_COMMAND_STATUS_SUCCESS);

            // Clear ACK pending variable
            pRmOneshotCtx->bPbiAckPending = LW_FALSE;
        }

        //
        // If RM has sent request to disable OSM, send async message to
        // RM to unblock it.
        //
        lpwrOsmAckSend();
    }

    //
    // Call the isostutter actions if ISO_STUTTER sub feature
    // is modified.
    //
    if (LPWR_IS_SF_MASK_SET(MS, ISO_STUTTER, pPgState->requestedMask) !=
        LPWR_IS_SF_MASK_SET(MS, ISO_STUTTER, pPgState->enabledMask))
    {
        //
        // If in the featureMask ISO_STUTTER is
        // 1. Enabled -> Add iso stutter back to MSCG entry condition.
        // 2. Disabled -> Remove iso stutter from MSCG entry condition
        //
        bAddIsoStutter = LPWR_IS_SF_MASK_SET(MS, ISO_STUTTER,
                                             pPgState->requestedMask);
        //Update IsoStutter override
        msIsoStutterOverride_HAL(&Ms, ctrlId, bAddIsoStutter);
    }

    // Update the dependencies of RPG
    if (PMUCFG_FEATURE_ENABLED(PMU_MS_RPG) &&
        LPWR_CTRL_IS_SF_SUPPORTED(pPgState, MS, RPG))
    {
        // If RPG is not enabled, clear the L2_CACHE_OPS as well
        if (!LPWR_IS_SF_MASK_SET(MS, RPG, pPgState->requestedMask))
        {
            pPgState->requestedMask = LPWR_SF_MASK_CLEAR(MS, L2_CACHE_OPS, pPgState->requestedMask);
        }
    }

    // Update the dependencies of FB_TRAINING. Disable FB_WR_TRAINING if FB_TRAINING is disabled
    if (!LPWR_IS_SF_MASK_SET(MS, FB_TRAINING, pPgState->requestedMask))
    {
        pPgState->requestedMask = LPWR_SF_MASK_CLEAR(MS, FB_WR_TRAINING, pPgState->requestedMask);
    }

    // Update the priv blocker enabled Mask
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_UNIFIED_PRIV_BLOCKER_CTRL))
    {
        //
        // Resetting the priv blocker enabled mask and setting it
        // based on each SFM bit.
        //
        // Note: We can optimize this code here, but to avoid more IMEM
        // usages, i am following this appraoch. Later, we will revisit this
        // again.
        //
        pPgState->privBlockerEnabledMask = 0;

        if (LPWR_IS_SF_MASK_SET(MS, SEC2, pPgState->requestedMask))
        {
            pPgState->privBlockerEnabledMask |= BIT(RM_PMU_PRIV_BLOCKER_ID_SEC2);
        }

        if (LPWR_IS_SF_MASK_SET(MS, GSP, pPgState->requestedMask))
        {
            pPgState->privBlockerEnabledMask |= BIT(RM_PMU_PRIV_BLOCKER_ID_GSP);
        }

        if (LPWR_IS_SF_MASK_SET(MS, XVE, pPgState->requestedMask))
        {
            pPgState->privBlockerEnabledMask |= BIT(RM_PMU_PRIV_BLOCKER_ID_XVE);
        }
    }
}

/*!
 * @brief Update the MS setting based on MCLK
 *
 */
void
msMclkAction
(
    LwBool *pBMsEnable,
    LwBool *pBWrTrainingReq
)
{
    OBJSWASR                            *pSwAsr           = MS_GET_SWASR();
    FLCN_STATUS                          status           = FLCN_OK;
    LW2080_CTRL_CLK_CLK_DOMAIN_LIST_ITEM mclkDomain;

    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfVf)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libClkReadAnyTask)
    };

    memset(&mclkDomain, 0, sizeof(mclkDomain));

    // Set MCLK Domain id
    mclkDomain.clkDomain = LW2080_CTRL_CLK_DOMAIN_MCLK;

    // Read the MCLK domain
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        status = clkReadDomain_AnyTask(&mclkDomain);
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }

    //
    // Enable the WR training only if
    // 1. System has GDDR5/5x/6 memory types &
    // 2. Current Mclk frequency needs it to be enabled.
    //
    if (pSwAsr->bIsGDDRx &&
        (mclkDomain.clkFreqKHz > RM_PMU_PG_MS_MCLK_MAX_FREQ_WITHOUT_WR_TRAINING_MAXWELL_KHZ))
    {
        *pBWrTrainingReq = LW_TRUE;
    }

    // Return True/Flase if MSCG can be enabled on current DRAM clock or not.
    if (mclkDomain.clkFreqKHz < s_msMaxSupportedDramFreqKhzGet())
    {
        *pBMsEnable = LW_TRUE;
    }
}

/*!
 * @brief Execute the MS Idle Flip reset sequence
 *
 * @param[in]   ctrlId       LPWR_CTRL Id
 * @param[out] *pAbortReason Abort reason
 *
 * @returns FLCN_OK         if reset is successful
 *          FLCN_ERROR      otherwise
 */
FLCN_STATUS
msIdleFlipReset
(
    LwU8   ctrlId,
    LwU16 *pAbortReason
)
{
    // Issue the Idle flip reset if PMU only traffic caused it
    if ((PMUCFG_FEATURE_ENABLED(PMU_PG_IDLE_FLIPPED_RESET)) &&
        ((FLCN_OK != msIdleFlippedReset_HAL(&Ms, ctrlId)) ||
         (Ms.bIdleFlipedIsr)))
    {
        *pAbortReason = MS_ABORT_REASON_FB_NOT_IDLE;
        return FLCN_ERROR;
    }

    return FLCN_OK;
}

/*!
 * @brief Execute the MS DMA Entry Sequence
 */
FLCN_STATUS
msDmaSequenceEntry
(
    LwU16  *pAbortReason
)
{
    FLCN_STATUS status = FLCN_OK;
    //
    // Suspend DMA if it's not locked. It will prevent code from swapping in
    // during MSCG. Also bump our priority to guarantee that the PG task gets
    // rescheduled when the scheduler code loops back around after overlay
    // load failure. Suspending DMA and raising priority should be an atomic
    // operation to guarantee that the PG task will service DMA_EXCEPTION on
    // priority.
    //
    appTaskCriticalEnter();
    {
        // Send DMA if its not locked.
        if (!lwosDmaSuspend(LW_FALSE))
        {
            *pAbortReason = MS_ABORT_REASON_DMA_SUSPENSION_FAILED;
            status = FLCN_ERROR;

            goto msDmaSequenceEntry_exit;
        }

        // Set the task priority
        lwrtosLwrrentTaskPrioritySet(OSTASK_PRIORITY_DMA_SUSPENDED);

        // Send DMA suspend notification to PG Log
        pgLogNotifyMsDmaSuspend();

        // Mark that DMA is suspended by MS Feature
        Ms.bDmaSuspended = LW_TRUE;
    }
msDmaSequenceEntry_exit:
    appTaskCriticalExit();

    return status;
}

/*!
 * @brief Execute the MS DMA Exit Sequence
 */
void
msDmaSequenceExit(void)
{
    appTaskCriticalEnter();
    {
        //
        // if DMA was suspended by MS features, then only
        // resume the DMA
        //
        if (Ms.bDmaSuspended)
        {
            // Resume DMA
            lwosDmaResume();

            // Restore the task priority
            lwrtosLwrrentTaskPrioritySet(OSTASK_PRIORITY(LPWR));

            // Send DMA resume notification to PG Log
            pgLogNotifyMsDmaResume();

            // Mark that DMA is not suspended by MS Feature
            Ms.bDmaSuspended = LW_FALSE;
        }

    }
    appTaskCriticalExit();
}

/*!
 * @brief Handler to do clock counter related operation
 *
 * @param[in]  ctrlId   LPWR_CTRL Id doing the clock gate/ungate
 * @param[in]  eventId  Clock Gating or Clock Ungating event
 */
void
msClkCntrActionHandler
(
    LwU8  ctrlId,
    LwU8  msClkGateEventId
)
{
    OBJPGSTATE *pPgState      = GET_OBJPGSTATE(ctrlId);
    LwU8        clientId      = LW2080_CTRL_CLK_CLIENT_ID_MAX_NUM;
    LwU32       clkDomainMask = LW2080_CTRL_CLK_DOMAIN_UNDEFINED;

    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CNTR))
    {
        //
        // if SW-ASR and Clock gating is not enabled, return.
        // SW-ASR Gates the DRAM(MCLK) Clock
        // CG Gates the GPC/LTC/XBAR Clocks
        //
        if (!LPWR_CTRL_IS_SF_ENABLED(pPgState, MS, SW_ASR) &&
            !LPWR_CTRL_IS_SF_ENABLED(pPgState, MS, CG))
        {
            return;
        }

        if (ctrlId == RM_PMU_LPWR_CTRL_ID_MS_MSCG)
        {
            clientId = LW2080_CTRL_CLK_CLIENT_ID_MSCG;

            // MSCG gates all the clock - DRAM/GPC/XBAR/LTC
            clkDomainMask = MS_GET_CG_MASK();
        }
        else if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_CTRL_MS_DIFR_SW_ASR) &&
                 (ctrlId == RM_PMU_LPWR_CTRL_ID_MS_DIFR_SW_ASR))
        {
            clientId = LW2080_CTRL_CLK_CLIENT_ID_DIFR_SW_ASR;

            // Override the mask for DIFR_SW_ASR which only gates MCLK
            clkDomainMask = MS_GET_CG_MASK() & MS_CG_MASK_SW_ASR;
        }
        else if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_CTRL_MS_DIFR_CG) &&
                 (ctrlId == RM_PMU_LPWR_CTRL_ID_MS_DIFR_CG))
        {
            clientId = LW2080_CTRL_CLK_CLIENT_ID_DIFR_CG;

            // Override the mask for DIFR_CG which only gates GPC/XBAR/LTC Clocks
            clkDomainMask = MS_GET_CG_MASK() & MS_CG_MASK_CG;
        }
        else
        {
            PMU_BREAKPOINT();
        }

        // Remove the clock domains which are not supported
        clkDomainMask = clkDomainMask & MS_GET_CG_MASK();

        switch (msClkGateEventId)
        {
            case MS_SEQ_ENTRY:
            {
                // Pass the clock domain mask which are gated by MS
                clkCntrAccessSync(clkDomainMask, clientId,
                                  CLK_CNTR_ACCESS_DISABLED);
                break;
            }

            case MS_SEQ_ABORT:
            {
                //
                // In case of MS abort, only re-enable the counters,
                // no need to trigger callback as MS was not engaged.
                //
                clkCntrAccessSync(clkDomainMask, clientId,
                                  CLK_CNTR_ACCESS_ENABLED_SKIP_CALLBACK);
                break;
            }

            case MS_SEQ_EXIT:
            {
                //
                // Re-enable counters and trigger callback on MS exit after
                // successful MS cycles
                //

                clkCntrAccessSync(clkDomainMask, clientId,
                                  CLK_CNTR_ACCESS_ENABLED_TRIGGER_CALLBACK);
                break;
            }
        }
    }
}

/*!
 * @brief Init the XVE Priv Blocker Settings
 *
 */
void
s_msPrivBlockerInit(void)
{
    // Enable XVE Priv Blocker Timout
    msPrivBlockerXveTimeoutInit_HAL(&Ms);

    // Program and enable allow (Allowlist) ranges for all priv blockers.
    msProgramAllowRanges_HAL(&Ms);

    // Program and enable disallow (Denylist) ranges for all priv blockers.
    msProgramDisallowRanges_HAL(&Ms);
}

/*!
 * @brief Context save interface for LPWR_ENG(DIFR_SW_ASR)
 *
 * Entry Sequence steps:
 *
 * 1. DMA Suspension
 * 2. Idle Flip Reset
 * 3. Clock Counter Action Handler
 * 4. Host Sequence
 * 5. Priv Blocker Sequence
 * 6. Holdoff Sequence
 * 7. Niso Blocker Sequence
 * 8. L2 Cache Operation.
 * 9. SW_ASR Entry Seqeunce
 *
 * @return FLCN_OK      Success.
 * @return FLCN_ERROR   Otherwise
 */
static FLCN_STATUS
s_msDifrSwAsrEntry(void)
{
    LwU8        stepId      = MS_DIFR_SW_ASR_SEQ_STEP_ID__START;
    FLCN_STATUS status      = FLCN_OK;

    // Start DIFR SW_ASR Entry Sequence
    status = s_msDifrSwAsrEntrySeq(&stepId);
    if ((status != FLCN_OK))
    {
        // Abort the DIFR SW-ASR Sequence
        s_msDifrSwAsrExitSeq(stepId, LW_TRUE);
    }
    return status;
}

/*!
 * @brief Context restore interface for LPWR_ENG(DIFR_SW_ASR)
 *
 * Exit Sequence steps:
 *
 * 1. SW_ASR Exit Sequence
 * 2. L2 Cache Operation
 * 3. Niso Blocker Exit Sequence
 * 4. Holdff Exit Sequence
 * 5. Priv Blocker Exit Sequence
 * 6. Host Exit Sequence
 * 7. Clock Counter Action Handler
 * 8. DMA Resume
*
 * @return FLCN_OK      Success.
 * @return FLCN_ERROR   Otherwise
 */
static FLCN_STATUS
s_msDifrSwAsrExit(void)
{
    return s_msDifrSwAsrExitSeq(MS_DIFR_SW_ASR_SEQ_STEP_ID__END, LW_FALSE);;
}

/*!
 * @brief Context save interface for LPWR_ENG(DIFR_CG)
 *
 * Entry Sequence Steps:
 * 1. Clock Counter Action Handler
 * 2. SMC ARB Disable
 * 3. ISO Blocker Entry Sequence
 * 4. L2 Auto FLush Disbale
 * 5. Wait for Subunit to report Idle
 * 6. GPC/LTC Clock Gating Sequence
 * 7. MS-RPPG Entry Sequence
 * 8. MS PSI Entry Sequence
 * 9. Clock SlowDown Engage Sequece
 *
 * @return FLCN_OK      Success.
 * @return FLCN_ERROR   Otherwise
 */
static FLCN_STATUS
s_msDifrCgEntry(void)
{
    LwU8        stepId      = MS_DIFR_CG_SEQ_STEP_ID__START;
    FLCN_STATUS status      = FLCN_OK;

    // Start DIFR CG Entry Sequence
    status = s_msDifrCgEntrySeq(&stepId);
    if ((status != FLCN_OK))
    {
        // Abort the DIFR CG Sequence
        s_msDifrCgExitSeq(stepId, LW_TRUE);
    }
    return status;
}

/*!
 * @brief Context restore interface for LPWR_ENG(DIFR_CG)
 *
 * Exit Sequence steps:
 *
 * 1. Clock SlowDown Exit
 * 2. PSI Exit Sequence
 * 3. MS-RPPG Exit Sequence
 * 4. GPC/LTC Clock Ungating Sequence
 * 5. L2 Auto FLush Enable
 * 6. ISO Blocker Exit Sequence
 * 7. SMC Arb Enable
 * 8. Clock Counter Action Handler
 *
 * @return FLCN_OK      Success.
 * @return FLCN_ERROR   Otherwise
 */
static FLCN_STATUS
s_msDifrCgExit(void)
{
    return s_msDifrCgExitSeq(MS_DIFR_CG_SEQ_STEP_ID__END, LW_FALSE);;
}

/*!
 * @brief Entry Sequence for DIFR CG
 *
 * @param[out]  pStepId Send back the last execture step of DIFR_CG Entry
 *
 * @return FLCN_OK      Success.
 * @return FLCN_ERROR   Otherwise
 */
FLCN_STATUS
s_msDifrCgEntrySeq
(
    LwU8    *pStepId
)
{
    FLCN_TIMESTAMP blockStartTimeNs;
    LwS32          timeoutLeftNs;
    LwU16          abortReason = 0;
    LwU8           ctrlId      = RM_PMU_LPWR_CTRL_ID_MS_DIFR_CG;
    FLCN_STATUS    status      = FLCN_OK;
    OBJPGSTATE    *pMsState    = GET_OBJPGSTATE(ctrlId);
    LwBool         bIdleCheck;

    // Set the timeout for Sequence abort
    timeoutLeftNs = MS_DEFAULT_ABORT_TIMEOUT1_NS;

    //
    // execute the entry sequence untill
    // 1. Sequence is completed or
    // 2. We have an abort
    //
    while (*pStepId <= MS_DIFR_CG_SEQ_STEP_ID__END)
    {
        //
        // Each step which needs idle check must set this
        // variable
        //
        bIdleCheck = LW_FALSE;

        switch (*pStepId)
        {
            case MS_DIFR_CG_SEQ_STEP_ID_CLK:
            {
                // Perform the clock related operation
                msClkCntrActionHandler(ctrlId, MS_SEQ_ENTRY);

                bIdleCheck = LW_TRUE;
                break;
            }

            case MS_DIFR_CG_SEQ_STEP_ID_SMCARB:
            {
                //
                // From GA10X, we need to disable the SMCARB Free running timestamp
                // so that XBAR does not report busy.
                //
                msSmcarbTimestampDisable_HAL(&Ms, LW_TRUE);
                break;
            }

            case MS_DIFR_CG_SEQ_STEP_ID_ISO_BLOCKER:
            {
                osPTimerTimeNsLwrrentGet(&blockStartTimeNs);

                // Execute the ISO Blockers Entry Sequence
                status = msIsoBlockerEntry_HAL(&Ms, ctrlId, &blockStartTimeNs,
                                               &timeoutLeftNs, &abortReason);
                bIdleCheck = LW_TRUE;
                break;
            }

            case MS_DIFR_CG_SEQ_STEP_ID_L2_CACHE_OPS:
            {
                if (LPWR_CTRL_IS_SF_ENABLED(pMsState, MS, L2_CACHE_OPS))
                {
                    // Disable L2 cache flush as we are not going to flush L2 cache.
                    msDisableL2AutoFlush_HAL(&Ms);
                }
                break;
            }

            case MS_DIFR_CG_SEQ_STEP_ID_SUB_UNIT:
            {
                status = msWaitForSubunitsIdle_HAL(&Ms, ctrlId, &blockStartTimeNs,
                                                   &timeoutLeftNs);
                if (status != FLCN_OK)
                {
                    abortReason = MS_ABORT_REASON_FB_NOT_IDLE;
                }
                bIdleCheck = LW_TRUE;
                break;
            }

            case MS_DIFR_CG_SEQ_STEP_ID_CG:
            {
                // Gate clocks and disable prive ring
                if (LPWR_CTRL_IS_SF_ENABLED(pMsState, MS, CG))
                {
                    msClkGate(ctrlId);
                    msDisablePrivAccess_HAL(&Ms, LW_TRUE);
                }
                break;
            }

            case MS_DIFR_CG_SEQ_STEP_ID_SRAM_SEQ:
            {
                // Engage MSCG coupled SRAM Power Gating (RPPG or RPG)
                if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_SEQ))
                {
                    msSramPowerGatingEngage_HAL(&Ms, ctrlId, LW_TRUE);
                }
                break;
            }

            case MS_DIFR_CG_SEQ_STEP_ID_PSI:
            {
                // Engage MS coupled PSI (Multrail PSI Support)
                if (PMUCFG_FEATURE_ENABLED(PMU_PSI_CTRL_MS) &&
                    (!msWakeupPending()))
                {
                    msPsiSeq(ctrlId, MS_PSI_ENTRY_STEPS_MASK_CORE_SEQ);
                }
                break;
            }

            case MS_DIFR_CG_SEQ_STEP_ID_CLK_SLOWDOWN:
            {
                // Engage the Clock SlowDown
                if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_CLK_SLOWDOWN))
                {
                    msClkSlowDownEngage_HAL(&Lpwr, ctrlId, LW_TRUE);
                }
                break;
            }
        }

        // Check if step returned error or Memory subsystem is idle or not
        if ((status != FLCN_OK) ||
            (bIdleCheck         &&
             !msIsIdle_HAL(&Ms, ctrlId, &abortReason)))
        {
            // Set the Abort Reason and StepId
            pMsState->stats.abortReason = (*pStepId << 16) | abortReason;

            // Set the status explicity
            status = FLCN_ERROR;

            // Exit out of while loop
            break;
        }

        // Move to next step
        *pStepId = *pStepId + 1;
    }

    return status;
}

/*!
 * @brief Exit Sequence for MSCG
 *
 * @param[in]  stepId  Start of MSCG Exit Sequence
 * @param[in]  bAbort  LW_TURE -> MSCG Entry was aborted
 *                     LW_FALSE -> MSCG Entry was not aborted
 *
 * @return FLCN_OK      Success.
 * @return FLCN_ERROR   Otherwise
 */
FLCN_STATUS
s_msDifrCgExitSeq
(
    LwU8 stepId,
    LwBool bAbort
)
{
    LwU8           ctrlId   = RM_PMU_LPWR_CTRL_ID_MS_DIFR_CG;
    OBJPGSTATE    *pMsState = GET_OBJPGSTATE(ctrlId);

    while (stepId >= MS_DIFR_CG_SEQ_STEP_ID__START)
    {
        switch (stepId)
        {
            case MS_DIFR_CG_SEQ_STEP_ID_CLK_SLOWDOWN:
            {
                // Disengage the Clock Slowdown
                if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_CLK_SLOWDOWN))
                {
                    msClkSlowDownEngage_HAL(&Lpwr, ctrlId, LW_FALSE);
                }
                break;
            }

            case MS_DIFR_CG_SEQ_STEP_ID_PSI:
            {
                // Multirail psi exit path.
                if (PMUCFG_FEATURE_ENABLED(PMU_PSI_CTRL_MS) &&
                    PMUCFG_FEATURE_ENABLED(PMU_PSI_MULTIRAIL))
                {
                    msPsiSeq(ctrlId, MS_PSI_MULTIRAIL_EXIT_STEPS);
                }
                break;
            }

            case MS_DIFR_CG_SEQ_STEP_ID_SRAM_SEQ:
            {
                // Disenage MSCG coupled SRAM Power Gating (RPPG or RPG)
                if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_SEQ))
                {
                    msSramPowerGatingEngage_HAL(&Ms, ctrlId, LW_FALSE);
                }
                break;
            }

            case MS_DIFR_CG_SEQ_STEP_ID_CG:
            {
                // Enable Priv ring and ungate clocks.
                if (LPWR_CTRL_IS_SF_ENABLED(pMsState, MS, CG))
                {
                    msDisablePrivAccess_HAL(&Ms, LW_FALSE);
                    msClkUngate(ctrlId);
                }
                break;
            }

            case MS_DIFR_CG_SEQ_STEP_ID_L2_CACHE_OPS:
            {
                if (LPWR_CTRL_IS_SF_ENABLED(pMsState, MS, L2_CACHE_OPS))
                {
                    msRestoreL2AutoFlush_HAL(&Ms);
                }
                break;
            }

            case MS_DIFR_CG_SEQ_STEP_ID_ISO_BLOCKER:
            {
                msIsoBlockerExit_HAL(&Ms, ctrlId);
                break;
            }

            case MS_DIFR_CG_SEQ_STEP_ID_SMCARB:
            {
                // Restore the SMCARB Timestamp
                msSmcarbTimestampDisable_HAL(&Ms, LW_FALSE);
                break;
            }

            case MS_DIFR_CG_SEQ_STEP_ID_CLK:
            {
                // Perform the clock related operation
                if (bAbort)
                {
                    msClkCntrActionHandler(ctrlId, MS_SEQ_ABORT);
                }
                else
                {
                    msClkCntrActionHandler(ctrlId, MS_SEQ_EXIT);
                }

                break;
            }
        }

        // Move to next step
        stepId--;
    }

    return FLCN_OK;
}

/*!
 * @brief MS Coupled PSI Sequence
 *
 * @param[in] ctrlId        LPWR_CTRL Id
 * @param[in] msPsiStepMask Mask of PSI Sequence
 */
FLCN_STATUS
msPsiSeq
(
    LwU8  ctrlId,
    LwU32 msPsiStepMask
)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(ctrlId);
    LwU8        psiCtrlId = RM_PMU_PSI_CTRL_ID_MS;
    LwU32 railEnabledMask = BIT(RM_PMU_PSI_RAIL_ID_LWVDD);
    FLCN_STATUS    status = FLCN_OK;

    // If PSI is not enabled in SFM or PSI Ctrl is not supported, return
    if ((!LPWR_CTRL_IS_SF_ENABLED(pPgState, MS, PSI)) ||
        (!PSI_IS_CTRL_SUPPORTED(psiCtrlId)))
    {
        return status;
    }

    // If Multirail PSI is supported, update the railMask and exit step mask
    if (PMUCFG_FEATURE_ENABLED(PMU_PSI_MULTIRAIL))
    {
        // Get the current railEnabledMask for MS PSI for multirail support.
        railEnabledMask = psiCtrlRailEnabledMaskGet(psiCtrlId);
    }

    switch (msPsiStepMask)
    {
        // Engage the PSI
        case MS_PSI_ENTRY_STEPS_MASK_CORE_SEQ:
        {
            status = psiEngagePsi_HAL(&Psi, psiCtrlId, railEnabledMask,
                                      msPsiStepMask);
            break;
        }

        //
        // Disengage the PSI
        //
        // Trigger MS coupled PSI disengage (No Multirail Support)
        //
        // 1) PSI dis-engage sequence is parallelized with MSCG exit
        // 2) Do not check for psiEnable while disengaging PSI. PMU uses
        //    non-blocking call to disengage PSI in which we first disable PSI
        //    and then wake MSCG.
        //
        // Bug #1285311 for the details.
        // Legacy parallel PSI exit path.
        //
        // In case we are doing the parallel Exit of PSI, we need to
        // cache the time.
        //
        // First stage of PSI writes to GPIO_6_OUTPUT_CNTL register to
        // dis-engage PSI. Cache dis-engage sequence start timestamp.
        //
        case MS_PSI_EXIT_STEPS_MASK_CORE_SEQ_STAGE_1:
        {
            psiDisengagePsi_HAL(&Psi, psiCtrlId, railEnabledMask, msPsiStepMask);
            osPTimerTimeNsLwrrentGet(&Ms.psiDisengageStartTimeNs);
            break;
        }

        case MS_PSI_EXIT_STEPS_MASK_CORE_SEQ_STAGE_2:
        {
            LwS32 psiWaitTimeNs;

            psiDisengagePsi_HAL(&Psi, psiCtrlId, railEnabledMask, msPsiStepMask);

            //
            // Callwlate Effective Wait Time.
            //
            // Wait Time = PSI Settle Time -
            //             Elapse time from starting of PSI dis-engage sequence
            //
            psiWaitTimeNs = osPTimerTimeNsElapsedGet(&Ms.psiDisengageStartTimeNs);

            psiWaitTimeNs = Ms.psiSettleTimeNs - psiWaitTimeNs;

            if (psiWaitTimeNs > 0)
            {
                OS_PTIMER_SPIN_WAIT_NS(psiWaitTimeNs);
            }
            break;
        }

#if PMUCFG_FEATURE_ENABLED(PMU_PSI_MULTIRAIL)
        case MS_PSI_MULTIRAIL_EXIT_STEPS:
        {
            psiDisengagePsi_HAL(&Psi, psiCtrlId, railEnabledMask, msPsiStepMask);
            break;
        }
#endif
    }

    return status;
}

/*!
 * @brief Entry Sequence for DIFR SW_ASR
 *
 * @param[out]  pStepId Send back the last execture step of MSCG Entry
 *
 * @return FLCN_OK      Success.
 * @return FLCN_ERROR   Otherwise
 */
FLCN_STATUS
s_msDifrSwAsrEntrySeq
(
    LwU8    *pStepId
)
{
    FLCN_TIMESTAMP blockStartTimeNs;
    LwS32          timeoutLeftNs;
    LwU16          abortReason;
    LwU8           ctrlId   = RM_PMU_LPWR_CTRL_ID_MS_DIFR_SW_ASR;
    FLCN_STATUS    status   = FLCN_OK;
    OBJPGSTATE    *pPgState = GET_OBJPGSTATE(ctrlId);
    LwBool         bIdleCheck;

    // Set the timeout for Sequence abort
    timeoutLeftNs = Ms.abortTimeoutNs;

    //
    // execute the entry sequence untill
    // 1. Sequence is completed or
    // 2. We have an abort
    //
    while (*pStepId <= MS_DIFR_SW_ASR_SEQ_STEP_ID__END)
    {
        //
        // Each step which needs idle check must set this
        // variable
        //
        bIdleCheck = LW_FALSE;

        switch (*pStepId)
        {
            case MS_DIFR_SW_ASR_SEQ_STEP_ID_DMA:
            {
                status = msDmaSequenceEntry(&abortReason);
                bIdleCheck = LW_TRUE;
                break;
            }

            case MS_DIFR_SW_ASR_SEQ_STEP_ID_CLK:
            {
                // Perform the clock related operation
                msClkCntrActionHandler(ctrlId, MS_SEQ_ENTRY);
                break;
            }

            case MS_DIFR_SW_ASR_SEQ_STEP_ID_IDLE_FLIP_RESET:
            {
                status = msIdleFlipReset(ctrlId, &abortReason);
                bIdleCheck = LW_TRUE;
                break;
            }

            case MS_DIFR_SW_ASR_SEQ_STEP_ID_HOST_SEQ:
            {
                status = msHostFlushBindEntry_HAL(&Ms, &abortReason);
                if (status != FLCN_OK)
                {
                    break;
                }

                if (msIsHostMemOpPending_HAL(&Ms))
                {
                    abortReason = MS_ABORT_REASON_HOST_MEMOP_PENDING;
                    status = FLCN_ERROR;
                    break;
                }
                bIdleCheck = LW_TRUE;
                break;
            }

            case MS_DIFR_SW_ASR_SEQ_STEP_ID_PRIV_BLOCKER:
            {
                // Start the Timer for Blocker Action
                osPTimerTimeNsLwrrentGet(&blockStartTimeNs);

                if (!msPrivBlockerEntry_HAL(&Ms, ctrlId, &blockStartTimeNs,
                                            &timeoutLeftNs, &abortReason))
                {
                    status = FLCN_ERROR;
                }
                bIdleCheck = LW_TRUE;
                break;
            }

            case MS_DIFR_SW_ASR_SEQ_STEP_ID_HOLDOFF:
            {
                 status = lpwrHoldoffMaskSet_HAL(&Lpwr, LPWR_HOLDOFF_CLIENT_ID_MS_DIFR_SW_ASR,
                                                 pPgState->holdoffMask);
                if (status != FLCN_OK)
                {
                    abortReason = MS_ABORT_REASON_POLL_ENGSTAT_TIMEOUT;
                    break;
                }

                if (msCheckTimeout(Ms.abortTimeoutNs, &blockStartTimeNs, &timeoutLeftNs))
                {
                    abortReason = MS_ABORT_REASON_HOLDOFF_TIMEOUT;
                    status = FLCN_ERROR;
                    break;
                }

                // Check non-stall intr for each engine once hold off are engaged.
                if (msIsEngineIntrPending_HAL(&Ms))
                {
                    abortReason = MS_ABORT_REASON_NON_STALLING_INTR;
                    status = FLCN_ERROR;
                }
                bIdleCheck = LW_TRUE;
                break;
            }

            case MS_DIFR_SW_ASR_SEQ_STEP_ID_NISO_BLOCKER:
            {
                // Execute the Niso Blocker Entry Sequence
                status = msNisoBlockerEntry_HAL(&Ms, ctrlId, &blockStartTimeNs,
                                                &timeoutLeftNs, &abortReason);
                break;
            }

            case MS_DIFR_SW_ASR_SEQ_STEP_ID_L2_CACHE_OPS:
            {
                if (LPWR_CTRL_IS_SF_ENABLED(pPgState, MS, L2_CACHE_OPS))
                {
                    msDifrSwAsrLtcEntry_HAL(&Ms, ctrlId);
                }
                bIdleCheck = LW_TRUE;
                break;
            }

            case MS_DIFR_SW_ASR_SEQ_STEP_ID_SW_ASR:
            {
                // Execute the SW ASR Entry sequence if SW-ASR is enabled in Ms.supportMask.
                if (LPWR_CTRL_IS_SF_ENABLED(pPgState, MS, SW_ASR))
                {
                    msSwAsrEntry_HAL(&Ms, ctrlId);
                }
                break;
            }
        }

        // Check if step returned error or Memory subsystem is idle or not
        if ((status != FLCN_OK) ||
            (bIdleCheck         &&
             !msIsIdle_HAL(&Ms, ctrlId, &abortReason)))
        {
            // Set the Abort Reason and StepId
            pPgState->stats.abortReason = (*pStepId << 16) | abortReason;

            // Set the status explicity
            status = FLCN_ERROR;

            // Exit out of while loop
            break;
        }

        // Move to next step
        *pStepId = *pStepId + 1;
    }

    return status;
}

/*!
 * @brief Exit Sequence for DIFR SW_ASR
 *
 * @param[in]  stepId  Start of MSCG Exit Sequence
 * @param[in]  bAbort  LW_TURE -> MSCG Entry was aborted
 *                     LW_FALSE -> MSCG Entry was not aborted
 *
 * @return FLCN_OK      Success.
 * @return FLCN_ERROR   Otherwise
 */
FLCN_STATUS
s_msDifrSwAsrExitSeq
(
    LwU8 stepId,
    LwBool bAbort
)
{
    LwU8           ctrlId   = RM_PMU_LPWR_CTRL_ID_MS_DIFR_SW_ASR;
    OBJPGSTATE    *pPgState = GET_OBJPGSTATE(ctrlId);

    while (stepId >= MS_DIFR_SW_ASR_SEQ_STEP_ID__START)
    {
        switch (stepId)
        {
            case MS_DIFR_SW_ASR_SEQ_STEP_ID_SW_ASR:
            {
                // Exit SW-ASR
                if (LPWR_CTRL_IS_SF_ENABLED(pPgState, MS, SW_ASR))
                {
                    msSwAsrExit_HAL(&Ms, ctrlId);
                }
                break;
            }

            case MS_DIFR_SW_ASR_SEQ_STEP_ID_L2_CACHE_OPS:
            {
                // Restore L2 cache settings
                if (LPWR_CTRL_IS_SF_ENABLED(pPgState, MS, L2_CACHE_OPS))
                {
                    msDifrSwAsrLtcExit_HAL(&Ms, ctrlId);
                }
                break;
            }

            case MS_DIFR_SW_ASR_SEQ_STEP_ID_NISO_BLOCKER:
            {
                msNisoBlockerExit_HAL(&Ms, ctrlId);
                break;
            }

            case MS_DIFR_SW_ASR_SEQ_STEP_ID_HOLDOFF:
            {
                lpwrHoldoffMaskSet_HAL(&Lpwr, LPWR_HOLDOFF_CLIENT_ID_MS_DIFR_SW_ASR,
                                       LPWR_HOLDOFF_MASK_NONE);
                break;
            }

            case MS_DIFR_SW_ASR_SEQ_STEP_ID_PRIV_BLOCKER:
            {
                // Execute rest of Priv Blocker Sequence
                msPrivBlockerExit_HAL(&Ms, ctrlId);
                break;
            }

            case MS_DIFR_SW_ASR_SEQ_STEP_ID_HOST_SEQ:
            {
                msHostFlushBindExit_HAL(&Ms);
                break;
            }

            case MS_DIFR_SW_ASR_SEQ_STEP_ID_CLK:
            {
                // Perform the clock related operation
                if (bAbort)
                {
                    msClkCntrActionHandler(ctrlId, MS_SEQ_ABORT);
                }
                else
                {
                    msClkCntrActionHandler(ctrlId, MS_SEQ_EXIT);
                }

                break;
            }

            case MS_DIFR_SW_ASR_SEQ_STEP_ID_DMA:
            {
                msDmaSequenceExit();
                break;
            }
        }

        // Move to next step
        stepId--;
    }

    return FLCN_OK;
}

/*!
 * @brief Entry Sequence interface for LPWR_ENG(MSCG)
 *
 * @return FLCN_OK      Success.
 * @return FLCN_ERROR   Otherwise
 */
static FLCN_STATUS
s_msMscgEntry(void)
{
    LwU8        stepId = MS_MSCG_SEQ_STEP_ID__START;
    FLCN_STATUS status = FLCN_OK;

    // Start MSCG Entry Sequence
    status = s_msMscgEntrySeq(&stepId);
    if ((status != FLCN_OK))
    {
        // Abort the MSCG Sequence
        s_msMscgExitSeq(stepId, LW_TRUE);
    }

    return status;
}

/*!
 * @brief Exit Sequence interface for LPWR_ENG(MSCG)
 *
 * @return FLCN_OK      Success.
 * @return FLCN_ERROR   Otherwise
 */
static FLCN_STATUS
s_msMscgExit(void)
{
    return s_msMscgExitSeq(MS_MSCG_SEQ_STEP_ID__END, LW_FALSE);
}

/*!
 * Handle Engine Reset for MSCG
 *
 * @returns FLCN_OK
 */
FLCN_STATUS
s_msMscgReset(void)
{
     return FLCN_OK;
}

/*!
 * @brief Entry Sequence for MSCG
 *
 * @param[out]  pStepId Send back the last execture step of MSCG Entry
 *
 * @return FLCN_OK      Success.
 * @return FLCN_ERROR   Otherwise
 */
FLCN_STATUS
s_msMscgEntrySeq
(
    LwU8    *pStepId
)
{
    FLCN_TIMESTAMP blockStartTimeNs;
    LwS32          timeoutLeftNs;
    LwU16          abortReason = 0;
    LwU8           ctrlId      = RM_PMU_LPWR_CTRL_ID_MS_MSCG;
    FLCN_STATUS    status      = FLCN_OK;
    OBJPGSTATE    *pMsState    = GET_OBJPGSTATE(ctrlId);
    LwBool         bIdleCheck;

    // Set the timeout for Sequence abort
    timeoutLeftNs = Ms.abortTimeoutNs;

    //
    // execute the entry sequence untill
    // 1. Sequence is completed or
    // 2. We have an abort
    //
    while (*pStepId <= MS_MSCG_SEQ_STEP_ID__END)
    {
        //
        // Each step which needs idle check must set this
        // variable
        //
        bIdleCheck = LW_FALSE;

        switch (*pStepId)
        {
            case MS_MSCG_SEQ_STEP_ID_DMA:
            {
                status = msDmaSequenceEntry(&abortReason);
                bIdleCheck = LW_TRUE;
                break;
            }

            case MS_MSCG_SEQ_STEP_ID_CLK:
            {
                // Perform the clock related operation
                msClkCntrActionHandler(ctrlId, MS_SEQ_ENTRY);
                break;
            }

            case MS_MSCG_SEQ_STEP_ID_IDLE_FLIP_RESET:
            {
                status = msIdleFlipReset(ctrlId, &abortReason);
                bIdleCheck = LW_TRUE;
                break;
            }

            case MS_MSCG_SEQ_STEP_ID_SMCARB:
            {
                //
                // From GA10X, we need to disable the SMCARB Free running timestamp
                // so that XBAR does not report busy.
                //
                msSmcarbTimestampDisable_HAL(&Ms, LW_TRUE);
                break;
            }

            case MS_MSCG_SEQ_STEP_ID_HOST_SEQ:
            {
                status = msHostFlushBindEntry_HAL(&Ms, &abortReason);
                if (status != FLCN_OK)
                {
                    break;
                }

                if (msIsHostMemOpPending_HAL(&Ms))
                {
                    abortReason = MS_ABORT_REASON_HOST_MEMOP_PENDING;
                    status = FLCN_ERROR;
                    break;
                }

                //
                // This step is from Pascal and on Turing GPU as per new HOST
                // sequence.
                // Details are in the bug: 200254181 & 1893989
                //
                if (!msIsHostIdle_HAL(&Ms))
                {
                    abortReason = MS_ABORT_REASON_HOST_NOT_IDLE;
                    status = FLCN_ERROR;
                }
                bIdleCheck = LW_TRUE;
                break;
            }

            case MS_MSCG_SEQ_STEP_ID_PRIV_BLOCKER:
            {
                // Start the Timer for Blocker Action
                osPTimerTimeNsLwrrentGet(&blockStartTimeNs);

                if (!msPrivBlockerEntry_HAL(&Ms, ctrlId, &blockStartTimeNs,
                                            &timeoutLeftNs, &abortReason))
                {
                    status = FLCN_ERROR;
                }
                break;
            }

            case MS_MSCG_SEQ_STEP_ID_HOLDOFF:
            {
                 status = lpwrHoldoffMaskSet_HAL(&Lpwr, LPWR_HOLDOFF_CLIENT_ID_MS_MSCG,
                                                 pMsState->holdoffMask);
                if (status != FLCN_OK)
                {
                    abortReason = MS_ABORT_REASON_POLL_ENGSTAT_TIMEOUT;
                    break;
                }

                if (msCheckTimeout(Ms.abortTimeoutNs, &blockStartTimeNs, &timeoutLeftNs))
                {
                    abortReason = MS_ABORT_REASON_HOLDOFF_TIMEOUT;
                    status = FLCN_ERROR;
                    break;
                }

                // Check non-stall intr for each engine once hold off are engaged.
                if (msIsEngineIntrPending_HAL(&Ms))
                {
                    abortReason = MS_ABORT_REASON_NON_STALLING_INTR;
                    status = FLCN_ERROR;
                }
                bIdleCheck = LW_TRUE;
                break;
            }

            case MS_MSCG_SEQ_STEP_ID_NISO_BLOCKER:
            {
                // Execute the Niso Blocker Entry Sequence
                status = msNisoBlockerEntry_HAL(&Ms, ctrlId, &blockStartTimeNs,
                                                &timeoutLeftNs, &abortReason);
                break;
            }

            case MS_MSCG_SEQ_STEP_ID_ISO_BLOCKER:
            {
                //
                // Reset the start timer for the following steps
                // as it uses a differnt, non-programmable timeout
                // MS_DEFAULT_ABORT_TIMEOUT1_NS.
                //
                timeoutLeftNs = MS_DEFAULT_ABORT_TIMEOUT1_NS;
                osPTimerTimeNsLwrrentGet(&blockStartTimeNs);

                // Execute the ISO Blockers Entry Sequence
                status = msIsoBlockerEntry_HAL(&Ms, ctrlId, &blockStartTimeNs,
                                               &timeoutLeftNs, &abortReason);
                bIdleCheck = LW_TRUE;
                break;
            }

            case MS_MSCG_SEQ_STEP_ID_L2_CACHE_OPS:
            {
                if (PMUCFG_FEATURE_ENABLED(PMU_MSCG_L2_CACHE_OPS) &&
                    LPWR_CTRL_IS_SF_ENABLED(pMsState, MS, L2_CACHE_OPS))
                {
                    status = msL2FlushAndIlwalidate_HAL(&Ms, ctrlId, &blockStartTimeNs,
                                                        &timeoutLeftNs);
                    if (status != FLCN_OK)
                    {
                        abortReason = MS_ABORT_REASON_L2_FLUSH_TIMEOUT;
                    }
                }
                else
                {
                    // Disable L2 cache flush if MSCG is not planning to flush L2 cache.
                    msDisableL2AutoFlush_HAL(&Ms);
                }
                break;
            }

            case MS_MSCG_SEQ_STEP_ID_SUB_UNIT:
            {
                status = msWaitForSubunitsIdle_HAL(&Ms, ctrlId, &blockStartTimeNs,
                                                   &timeoutLeftNs);
                if (status != FLCN_OK)
                {
                    abortReason = MS_ABORT_REASON_FB_NOT_IDLE;
                }
                bIdleCheck = LW_TRUE;
                break;
            }

            case MS_MSCG_SEQ_STEP_ID_SW_ASR:
            {
                // Execute the SW ASR Entry sequence if SW-ASR is enabled in Ms.supportMask.
                if (LPWR_CTRL_IS_SF_ENABLED(pMsState, MS, SW_ASR))
                {
                    msSwAsrEntry_HAL(&Ms, ctrlId);
                }
                break;
            }

            case MS_MSCG_SEQ_STEP_ID_CG:
            {
                // Gate clocks and disable prive ring
                if (LPWR_CTRL_IS_SF_ENABLED(pMsState, MS, CG))
                {
                    msClkGate(ctrlId);
                    msDisablePrivAccess_HAL(&Ms, LW_TRUE);
                }
                break;
            }

            case MS_MSCG_SEQ_STEP_ID_SRAM_SEQ:
            {
                // Engage MSCG coupled SRAM Power Gating (RPPG or RPG)
                if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_SEQ))
                {
                    msSramPowerGatingEngage_HAL(&Ms, ctrlId, LW_TRUE);
                }
                break;
            }

            case MS_MSCG_SEQ_STEP_ID_PSI:
            {
                //
                // Engage MS coupled PSI (No Multrail PSI Support)
                //
                // MSCG coupled PSI requires special handling like -
                // 1) Ensure that there is no request pending in LWOS_QUEUE(PMU, PG) before
                //    engaging PSI
                // 2) PMU will keep GPIO mutex when GPU is in MSCG
                //
                // Bug #1285311 for the details.
                //
                // This handling will not be needed only for PrePascal
                // GPUs.
                //
                if (PMUCFG_FEATURE_ENABLED(PMU_PSI_MULTIRAIL))
                {
                    if (PMUCFG_FEATURE_ENABLED(PMU_PSI_CTRL_MS)      &&
                        PSI_IS_CTRL_SUPPORTED(RM_PMU_PSI_CTRL_ID_MS) &&
                        LPWR_CTRL_IS_SF_ENABLED(pMsState, MS, PSI)   &&
                        (!msWakeupPending()))
                    {
                        // Get the current railEnabledMask for MS PSI.
                        LwU32 railEnabledMask = psiCtrlRailEnabledMaskGet(RM_PMU_PSI_CTRL_ID_MS);

                        // Send this railMask to engage the PSI on given rails.
                        psiEngage(RM_PMU_PSI_CTRL_ID_MS, railEnabledMask,
                                MS_PSI_ENTRY_STEPS_MASK_CORE_SEQ);
                    }
                }
                else
                {
                    if (PMUCFG_FEATURE_ENABLED(PMU_PSI_CTRL_MS)       &&
                        PSI_IS_CTRL_SUPPORTED(RM_PMU_PSI_CTRL_ID_MS)  &&
                        LPWR_CTRL_IS_SF_ENABLED(pMsState, MS, PSI)    &&
                        (!msWakeupPending()))
                    {
                        psiEngage(RM_PMU_PSI_CTRL_ID_MS, BIT(RM_PMU_PSI_RAIL_ID_LWVDD),
                                  MS_PSI_ENTRY_STEPS_MASK_CORE_SEQ);
                    }
                }
                break;
            }

            case MS_MSCG_SEQ_STEP_ID_CLK_SLOWDOWN:
            {
                // Engage the Clock SlowDown
                if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_CLK_SLOWDOWN))
                {
                    msClkSlowDownEngage_HAL(&Ms, ctrlId, LW_TRUE);
                }
                break;
            }
        }

        // Check if step returned error or Memory subsystem is idle or not
        if ((status != FLCN_OK) ||
            (bIdleCheck         &&
             !msIsIdle_HAL(&Ms, ctrlId, &abortReason)))
        {
            // Set the Abort Reason and StepId
            pMsState->stats.abortReason = (*pStepId << 16) | abortReason;

            // Set the status explicity
            status = FLCN_ERROR;

            // Exit out of while loop
            break;
        }

        // Move to next step
        *pStepId = *pStepId + 1;
    }

    return status;
}

/*!
 * @brief Exit Sequence for MSCG
 *
 * @param[in]  stepId  Start of MSCG Exit Sequence
 * @param[in]  bAbort  LW_TURE -> MSCG Entry was aborted
 *                     LW_FALSE -> MSCG Entry was not aborted
 *
 * @return FLCN_OK      Success.
 * @return FLCN_ERROR   Otherwise
 */
FLCN_STATUS
s_msMscgExitSeq
(
    LwU8 stepId,
    LwBool bAbort
)
{
    LwU8           ctrlId   = RM_PMU_LPWR_CTRL_ID_MS_MSCG;
    OBJPGSTATE    *pMsState = GET_OBJPGSTATE(ctrlId);

    while (stepId >= MS_MSCG_SEQ_STEP_ID__START)
    {
        switch (stepId)
        {
            case MS_MSCG_SEQ_STEP_ID_CLK_SLOWDOWN:
            {
                // Disengage the Clock Slowdown
                if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_CLK_SLOWDOWN))
                {
                    msClkSlowDownEngage_HAL(&Ms, ctrlId, LW_FALSE);
                }
                break;
            }

            case MS_MSCG_SEQ_STEP_ID_PSI:
            {

                // Multirail psi exit path.
                if (PMUCFG_FEATURE_ENABLED(PMU_PSI_MULTIRAIL) &&
                    Ms.bPsiSequentialExit)
                {
                    if (PMUCFG_FEATURE_ENABLED(PMU_PSI_CTRL_MS)       &&
                        PSI_IS_CTRL_SUPPORTED(RM_PMU_PSI_CTRL_ID_MS)  &&
                        LPWR_CTRL_IS_SF_ENABLED(pMsState, MS, PSI))
                    {
                        LwU32 railEnabledMask = psiCtrlRailEnabledMaskGet(RM_PMU_PSI_CTRL_ID_MS);

                        psiDisengage(RM_PMU_PSI_CTRL_ID_MS, railEnabledMask,
                                     MS_PSI_MULTIRAIL_EXIT_STEPS);
                    }
                }
                else
                {
                    // Legacy parallel PSI exit path.
                    if (PMUCFG_FEATURE_ENABLED(PMU_PSI_CTRL_MS)      &&
                        PSI_IS_CTRL_SUPPORTED(RM_PMU_PSI_CTRL_ID_MS) &&
                        psiIsCtrlEngaged(RM_PMU_PSI_CTRL_ID_MS, RM_PMU_PSI_RAIL_ID_LWVDD))
                    {
                        //
                        // First stage of PSI writes to GPIO_6_OUTPUT_CNTL register to
                        // dis-engage PSI. Cache dis-engage sequence start timestamp.
                        //
                        psiDisengage(RM_PMU_PSI_CTRL_ID_MS, BIT(RM_PMU_PSI_RAIL_ID_LWVDD),
                                     MS_PSI_EXIT_STEPS_MASK_CORE_SEQ_STAGE_1);
                        osPTimerTimeNsLwrrentGet(&Ms.psiDisengageStartTimeNs);
                    }
                }

                break;
            }

            case MS_MSCG_SEQ_STEP_ID_SRAM_SEQ:
            {
                // Disenage MSCG coupled SRAM Power Gating (RPPG or RPG)
                if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_SEQ))
                {
                    msSramPowerGatingEngage_HAL(&Ms, ctrlId, LW_FALSE);
                }
                break;
            }

            case MS_MSCG_SEQ_STEP_ID_CG:
            {
                // Enable Priv ring and ungate clocks.
                if (LPWR_CTRL_IS_SF_ENABLED(pMsState, MS, CG))
                {
                    msDisablePrivAccess_HAL(&Ms, LW_FALSE);
                    msClkUngate(ctrlId);
                }
                break;
            }

            case MS_MSCG_SEQ_STEP_ID_SW_ASR:
            {
                // Exit SW-ASR
                if (LPWR_CTRL_IS_SF_ENABLED(pMsState, MS, SW_ASR))
                {
                    msSwAsrExit_HAL(&Ms, ctrlId);
                }
                break;
            }

            case MS_MSCG_SEQ_STEP_ID_L2_CACHE_OPS:
            {
                // Restore L2 cache auto flush settings
                if (!PMUCFG_FEATURE_ENABLED(PMU_MSCG_L2_CACHE_OPS) ||
                    !LPWR_CTRL_IS_SF_ENABLED(pMsState, MS, L2_CACHE_OPS))
                {
                    msRestoreL2AutoFlush_HAL(&Ms);
                }
                break;
            }

            case MS_MSCG_SEQ_STEP_ID_ISO_BLOCKER:
            {
                msIsoBlockerExit_HAL(&Ms, ctrlId);

                //
                // Execute 2nd stage of PSI dis-engage sequence (No Multirail support).
                //
                // 2nd stage ensures that GPIO register has flushed successfully and
                // release the GPIO mutex. It waits for regulator/voltage settle time.
                //
                // Do not check for psiEnable while disengaging PSI. PMU uses non-blocking
                // call to disengage PSI in which we first disable PSI and then wake MSCG
                // which ultimately disengages and disables PSI.
                //
                if (!PMUCFG_FEATURE_ENABLED(PMU_PSI_MULTIRAIL) ||
                    !Ms.bPsiSequentialExit)
                {
                    if (PMUCFG_FEATURE_ENABLED(PMU_PSI_CTRL_MS)      &&
                        PSI_IS_CTRL_SUPPORTED(RM_PMU_PSI_CTRL_ID_MS) &&
                        psiIsCtrlEngaged(RM_PMU_PSI_CTRL_ID_MS, RM_PMU_PSI_RAIL_ID_LWVDD))
                    {
                        LwS32 psiWaitTimeNs;

                        psiDisengage(RM_PMU_PSI_CTRL_ID_MS, BIT(RM_PMU_PSI_RAIL_ID_LWVDD),
                                     MS_PSI_EXIT_STEPS_MASK_CORE_SEQ_STAGE_2);
                        //
                        // Callwlate Effective Wait Time.
                        //
                        // Wait Time = PSI Settle Time -
                        //             Elapse time from starting of PSI dis-engage sequence
                        //
                        psiWaitTimeNs = osPTimerTimeNsElapsedGet(&Ms.psiDisengageStartTimeNs);

                        psiWaitTimeNs = Ms.psiSettleTimeNs - psiWaitTimeNs;

                        if (psiWaitTimeNs > 0)
                        {
                            OS_PTIMER_SPIN_WAIT_NS(psiWaitTimeNs);
                        }
                    }
                }
                break;
            }

            case MS_MSCG_SEQ_STEP_ID_NISO_BLOCKER:
            {
                msNisoBlockerExit_HAL(&Ms, ctrlId);
                break;
            }

            case MS_MSCG_SEQ_STEP_ID_HOLDOFF:
            {
                lpwrHoldoffMaskSet_HAL(&Lpwr, LPWR_HOLDOFF_CLIENT_ID_MS_MSCG,
                                       LPWR_HOLDOFF_MASK_NONE);
                break;
            }

            case MS_MSCG_SEQ_STEP_ID_PRIV_BLOCKER:
            {
                //
                // SEC2 combined PRIV and FB blocker WAR: Disengage the combined
                // PRIV+FB blocker before disabling method holdoff for SEC2
                // (bug 200113462). - This is applicable only for PASCAL
                //
                msSec2PrivFbBlockersDisengage_HAL(&Ms);

                // Execute rest of Priv Blocker Sequence
                msPrivBlockerExit_HAL(&Ms, ctrlId);
                break;
            }

              case MS_MSCG_SEQ_STEP_ID_HOST_SEQ:
            {
                msHostFlushBindExit_HAL(&Ms);
                break;
            }

            case MS_MSCG_SEQ_STEP_ID_SMCARB:
            {
                // Restore the SMCARB Timestamp
                msSmcarbTimestampDisable_HAL(&Ms, LW_FALSE);
                break;
            }

            case MS_MSCG_SEQ_STEP_ID_CLK:
            {
                // Perform the clock related operation
                if (bAbort)
                {
                    msClkCntrActionHandler(ctrlId, MS_SEQ_ABORT);
                }
                else
                {
                    msClkCntrActionHandler(ctrlId, MS_SEQ_EXIT);
                }

                break;
            }

            case MS_MSCG_SEQ_STEP_ID_DMA:
            {
                if ((bAbort) &&
                    (LPWR_CTRL_IS_SF_ENABLED(pMsState, MS, L2_CACHE_OPS)))
                {
                    msWaitForL2FlushFinish_HAL(&Ms);
                }

                msDmaSequenceExit();
                break;
            }
        }

        // Move to next step
        stepId--;
    }

    return FLCN_OK;
}

/*!
 * @brief Initialize the MS Group Clock
 * gating Mask
 */
void
s_msClockGatingInit(void)
{
    LPWR_CG *pLpwrCg = Lpwr.pLpwrCg;

    //
    // If any MS Group Feature supports Clock Gating, initialize the
    // Clock Gating Mask
    //
    if (lpwrGrpCtrlIsSfSupported(RM_PMU_LPWR_GRP_CTRL_ID_MS,
                                 RM_PMU_LPWR_CTRL_FEATURE_ID_MS_CG) &&
        (pLpwrCg != NULL))
    {
        // Set the Mask for GPC/XBAR/LTC.
        Ms.cgMask = pLpwrCg->clkDomainSupportMask;

        // If MS Stop clock feature supported, initailize the stop clock profile
        if (PMUCFG_FEATURE_ENABLED(PMU_MS_CG_STOP_CLOCK))
        {
            msStopClockInit_HAL(&Ms, Ms.cgMask);
        }

        // Init DI Clock gating mask if DI is supported.
        if (PMUCFG_FEATURE_ENABLED(PMU_GCX_DIDLE))
        {
            Di.cgMask = pLpwrCg->clkDomainSupportMask & (~Ms.cgMask);
        }
    }
}
