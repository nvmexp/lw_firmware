/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    perf_cf_controller_util.c
 * @copydoc perf_cf_controller_util.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"
#include "syslib/syslib.h"
#include "g_lwconfig.h"

/* ------------------------ Application Includes ---------------------------- */
#include "perf/cf/perf_cf.h"
#include "perf/cf/perf_cf_controller_util.h"
#include "pmu_objperf.h"
#include "pmu_objclk.h"
#include "pmu_objfb.h"
#include "pmu_objbif.h"
#include "pmu_dpaux.h"
#include "dev_fuse.h"
#include "pmu_objfuse.h"

/* ------------------------ Macros and Defines ------------------------------ */
#define DEFAULT_INC_STEP_SIZE_LHR11            4U
#define DEFAULT_DEC_STEP_SIZE_LHR11            2U
#define INTERMEDIATE_DEC_STEP_SIZE_LHR11       1U
#define DEFAULT_INC_STEP_SIZE_LHR10            1U
#define DEFAULT_DEC_STEP_SIZE_LHR10            1U
#define INTERMEDIATE_DEC_STEP_SIZE_LHR10       1U

// 128B / 2 bytes/channel / 2 datarate
#define PERF_CF_CONTROLLER_MEM_TUNE_TOTAL_ACT_CH_RATE                       32
// 32B  / 2 bytes/channel / 2 datarate
#define PERF_CF_CONTROLLER_MEM_TUNE_READ_ACC_CH_RATE                         8
// BAPM values have 2 problems:
// 1) the bottom 2 bits are dropped.  Easiy solved with a *4
// 2) the value increments based on (ch0 | ch1) - both being true at once results in +1 instead of +2
//    this is accounted for in the thresholds
#define PERF_CF_CONTROLLER_MEM_TUNE_PM_SIGNAL_CORRECTION                     4

//! Timestamps that can only be set in kernel code
sysKERNEL_DATA FLCN_TIMESTAMP  memTuneControllerTimeStamp        = { 0 };
sysKERNEL_DATA FLCN_TIMESTAMP  memTuneControllerMonitorTimeStamp = { 0 };

//! store p0 memory clock that can only be set in kernel code
sysKERNEL_DATA LwU64           memTuneControllerVbiosMaxMclkkHz  = { 0 };

/* ------------------------ Function Prototypes ----------------------------- */
static FLCN_STATUS s_perfCfControllerQueueMemTuneChange(LwU8 tFAW)
        GCC_ATTRIB_SECTION("imem_perfCf", "s_perfCfControllerQueueMemTuneChange");

/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/*!
 * @todo Move to some CLK structure
 */
static PERF_CF_CONTROLLER_MEM_TUNE_MONITOR s_perfCfControllerMemTuneMonitor
    GCC_ATTRIB_SECTION("dmem_perfCf", "s_perfCfControllerMemTuneMonitor");

static OS_TMR_CALLBACK OsCBPerfCfControllerMemTuneMonitor;

/* ------------------------ Static Function Prototypes ---------------------- */
static OsTmrCallbackFunc (s_perfCfControllerMemTuneMonitorCallback)
    GCC_ATTRIB_SECTION("imem_perfCf", "s_perfCfControllerMemTuneMonitorCallback")
    GCC_ATTRIB_USED();
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
perfCfControllerGrpIfaceModel10ObjSetImpl_MEM_TUNE
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_PERF_CF_CONTROLLER_MEM_TUNE *pDescController =
        (RM_PMU_PERF_CF_CONTROLLER_MEM_TUNE *)pBoardObjDesc;
    PERF_CF_CONTROLLER_MEM_TUNE        *pControllerMemTune;
    LwBool                              bFirstConstruct = (*ppBoardObj == NULL);
    FLCN_STATUS                         status          = FLCN_OK;
    LwU8                                tIdx;
    LwBoardObjIdx                       pmIdx;

    status = perfCfControllerGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfCfControllerGrpIfaceModel10ObjSetImpl_MEM_TUNE_exit;
    }
    pControllerMemTune = (PERF_CF_CONTROLLER_MEM_TUNE *)*ppBoardObj;

    if (!bFirstConstruct)
    {
        // We cannot allow subsequent SET calls to change these parameters.
        if ((pControllerMemTune->pmSensorIdx        != pDescController->pmSensorIdx)        ||
            (pControllerMemTune->timerTopologyIdx   != pDescController->timerTopologyIdx)   ||
            (pControllerMemTune->hysteresisCountInc != pDescController->hysteresisCountInc) ||
            (pControllerMemTune->hysteresisCountDec != pDescController->hysteresisCountDec) ||
            (pControllerMemTune->targets.numTargets != pDescController->targetsCtrl.numTargets))
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto perfCfControllerGrpIfaceModel10ObjSetImpl_MEM_TUNE_exit;
        }

        for (tIdx = 0; tIdx < pControllerMemTune->targets.numTargets; tIdx++)
        {

            if (pControllerMemTune->targets.target[tIdx].target.high !=
                pDescController->targetsCtrl.target[tIdx].high)
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_ILWALID_ARGUMENT;
                goto perfCfControllerGrpIfaceModel10ObjSetImpl_MEM_TUNE_exit;
            }

            if (pControllerMemTune->targets.target[tIdx].target.low !=
                pDescController->targetsCtrl.target[tIdx].low)
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_ILWALID_ARGUMENT;
                goto perfCfControllerGrpIfaceModel10ObjSetImpl_MEM_TUNE_exit;
            }

            // We cannot allow subsequent SET calls to change these parameters.
            if (pControllerMemTune->targets.target[tIdx].numSignals !=
                pDescController->targetsInfo.target[tIdx].numSignals)
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_ILWALID_ARGUMENT;
                goto perfCfControllerGrpIfaceModel10ObjSetImpl_MEM_TUNE_exit;
            }

            for (pmIdx = 0; pmIdx < pControllerMemTune->targets.target[tIdx].numSignals; pmIdx++)
            {
                if (pControllerMemTune->targets.target[tIdx].signal[pmIdx].index !=
                    pDescController->targetsInfo.target[tIdx].signalIdx[pmIdx])
                {
                    PMU_BREAKPOINT();
                    status = FLCN_ERR_ILWALID_ARGUMENT;
                    goto perfCfControllerGrpIfaceModel10ObjSetImpl_MEM_TUNE_exit;
                }
            }
        }
    }

    // Set member variables.
    pControllerMemTune->pmSensorIdx        = pDescController->pmSensorIdx;
    pControllerMemTune->timerTopologyIdx   = pDescController->timerTopologyIdx;
    pControllerMemTune->hysteresisCountInc = pDescController->hysteresisCountInc;
    pControllerMemTune->hysteresisCountDec = pDescController->hysteresisCountDec;

    //
    // Always enable filter support.
    // TODO : Need to move this to VBIOS in follow up.
    //
    pControllerMemTune->bFilterEnabled          = LW_TRUE;
    pControllerMemTune->bLhrEnhancementEnabled  = perfCfControllerMemTuneIsLhrEnhancementFuseEnabled();

    for (tIdx = 0; tIdx < pDescController->targetsInfo.numTargets; tIdx++)
    {
        pControllerMemTune->targets.numTargets                  = pDescController->targetsInfo.numTargets;
        pControllerMemTune->targets.target[tIdx].target         = pDescController->targetsCtrl.target[tIdx];
        pControllerMemTune->targets.target[tIdx].numSignals     = pDescController->targetsInfo.target[tIdx].numSignals;

        for (pmIdx = 0; pmIdx < pControllerMemTune->targets.target[tIdx].numSignals; pmIdx++)
        {
            pControllerMemTune->targets.target[tIdx].signal[pmIdx].index =
                pDescController->targetsInfo.target[tIdx].signalIdx[pmIdx];
        }
    }

    // Sanity check POR config params.
    if (LW2080_CTRL_PERF_PERF_CF_CONTROLLER_INFO_TOPOLOGY_IDX_NONE ==
            pControllerMemTune->super.topologyIdx)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto perfCfControllerGrpIfaceModel10ObjSetImpl_MEM_TUNE_exit;
    }

    if ((LW2080_CTRL_BOARDOBJ_IDX_ILWALID ==
            pControllerMemTune->timerTopologyIdx) ||
        !BOARDOBJGRP_IS_VALID(PERF_CF_TOPOLOGY, pControllerMemTune->timerTopologyIdx))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto perfCfControllerGrpIfaceModel10ObjSetImpl_MEM_TUNE_exit;
    }

    if (LW2080_CTRL_BOARDOBJ_IDX_ILWALID ==
            pControllerMemTune->pmSensorIdx)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto perfCfControllerGrpIfaceModel10ObjSetImpl_MEM_TUNE_exit;
    }

    if (0 == pControllerMemTune->hysteresisCountInc)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto perfCfControllerGrpIfaceModel10ObjSetImpl_MEM_TUNE_exit;
    }

    if (0 == pControllerMemTune->hysteresisCountDec)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto perfCfControllerGrpIfaceModel10ObjSetImpl_MEM_TUNE_exit;
    }

    // Only perform on first construct.
    if (bFirstConstruct)
    {
        pControllerMemTune->hysteresisCountDecLwrr = 0;

        /*!
         * Start at hysteresis increment counter = (threshold) - (1 step size)
         * Controller will engage on the first sample when mining workload is run
         * If there is no mining workload for first 8-16 samples, controller will
         * auto decrement hysteresis counter to zero.
         */
        if (perfCfControllerMemTuneIsLhrEnhancementEnabled(pControllerMemTune))
        {
            pControllerMemTune->hysteresisCountIncLwrr =
                (pControllerMemTune->hysteresisCountInc - 1) * DEFAULT_INC_STEP_SIZE_LHR11;

        }
        else
        {
            pControllerMemTune->hysteresisCountIncLwrr =
                (pControllerMemTune->hysteresisCountInc - 1) * DEFAULT_INC_STEP_SIZE_LHR10;
        }

        if (perfCfControllerMemTuneIsLhrEnhancementEnabled(pControllerMemTune))
        {
            PSTATE *pPstate;
            LwU32   mclkDomainIdx;
            LW2080_CTRL_PERF_PSTATE_CLOCK_ENTRY pstateClkEntry;

            // Cache the POR max MCLK frequency.

            // Get the highest performance pstate
            pPstate = PSTATE_GET_BY_HIGHEST_INDEX();
            if (pPstate == NULL)
            {
                status = FLCN_ERR_ILWALID_STATE;
                PMU_BREAKPOINT();
                goto perfCfControllerGrpIfaceModel10ObjSetImpl_MEM_TUNE_exit;
            }

            // Get current memory clock value.
            PMU_ASSERT_OK_OR_GOTO(status,
                clkDomainsGetIndexByApiDomain(LW2080_CTRL_CLK_DOMAIN_MCLK, &mclkDomainIdx),
                perfCfControllerGrpIfaceModel10ObjSetImpl_MEM_TUNE_exit);

            // Get clks for that pstate
            PMU_ASSERT_OK_OR_GOTO(status,
                perfPstateClkFreqGet(pPstate, mclkDomainIdx, &pstateClkEntry),
                perfCfControllerGrpIfaceModel10ObjSetImpl_MEM_TUNE_exit);

            // P0 POR Max frequency + 7% margin for frequency OC.
            pControllerMemTune->porMaxMclkkHz52_12 =
                pstateClkEntry.max.origFreqkHz + ((pstateClkEntry.max.origFreqkHz * 7) / 100);

            // Left shift by 12 to colwert 64.00 -> 52.24
            lw64Lsl(&pControllerMemTune->porMaxMclkkHz52_12,
                    &pControllerMemTune->porMaxMclkkHz52_12,
                    12);
        }
    }

    // Only perform on set control
    if (!bFirstConstruct)
    {
        //
        // Always reset the counter.
        // This functionality is added to allow automated test plans to
        // validate LHR is not engaging for gaming workload.
        // sequence : Set control to reset counter - run benchmark - check counter - repeat
        //
        pControllerMemTune->trrdWarEngageCounter = 0;
    }

perfCfControllerGrpIfaceModel10ObjSetImpl_MEM_TUNE_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpQuery
 */
FLCN_STATUS
perfCfControllerIfaceModel10GetStatus_MEM_TUNE
(
    BOARDOBJ_IFACE_MODEL_10              *pModel10,
    RM_PMU_BOARDOBJ       *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_PERF_CF_CONTROLLER_MEM_TUNE_GET_STATUS *pStatus =
        (RM_PMU_PERF_CF_CONTROLLER_MEM_TUNE_GET_STATUS *)pPayload;
    PERF_CF_CONTROLLER_MEM_TUNE *pControllerMemTune =
        (PERF_CF_CONTROLLER_MEM_TUNE *)pBoardObj;
    FLCN_STATUS     status = FLCN_OK;
    LwU8            tIdx;
    LwBoardObjIdx   pmIdx;
    LwU64           temp;
    LwU32           val    = fuseRead(LW_FUSE_SPARE_BIT_1);

    status = perfCfControllerIfaceModel10GetStatus_SUPER(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfCfControllerIfaceModel10GetStatus_MEM_TUNE_exit;
    }

    if (val != LW_FUSE_SPARE_BIT_1_DATA_ENABLE)
    {
        // Set member variables.
        pStatus->targets.numTargets = pControllerMemTune->targets.numTargets;

        for (tIdx = 0; tIdx < pControllerMemTune->targets.numTargets; tIdx++)
        {
            pStatus->targets.target[tIdx].observed     =
                pControllerMemTune->targets.target[tIdx].observed;
            pStatus->targets.target[tIdx].cntDiffTotal =
                pControllerMemTune->targets.target[tIdx].cntDiffTotal;

            pStatus->targets.target[tIdx].numSignals = pControllerMemTune->targets.target[tIdx].numSignals;
            for (pmIdx = 0; pmIdx < pControllerMemTune->targets.target[tIdx].numSignals; pmIdx++)
            {
                pStatus->targets.target[tIdx].signal[pmIdx] =
                    pControllerMemTune->targets.target[tIdx].signal[pmIdx];
            }
        }

        pStatus->hysteresisCountIncLwrr    = pControllerMemTune->hysteresisCountIncLwrr;
        pStatus->hysteresisCountDecLwrr    = pControllerMemTune->hysteresisCountDecLwrr;
        pStatus->perfms52_12               = pControllerMemTune->perfms52_12;
        pStatus->activateRateSol52_12      = pControllerMemTune->activateRateSol52_12;
        pStatus->accessRateSol52_12        = pControllerMemTune->accessRateSol52_12;
        pStatus->bPcieOverrideEn           = pControllerMemTune->bPcieOverrideEn;
        pStatus->bDispOverrideEn           = pControllerMemTune->bDispOverrideEn;
        pStatus->bTrrdWarEngagePreOverride = pControllerMemTune->bTrrdWarEngagePreOverride;
    }


    temp = perfCfControllerMemTunetFAWLwrrGet(pControllerMemTune) + clkFbflcnTrrdGet_HAL(&Clk) + pControllerMemTune->trrdWarEngageCounter;
    lw64Add_MOD(&temp,
            &pControllerMemTune->mclkkHz52_12,
            &temp);
    pStatus->mclkkHz52_12              = temp;

    pStatus->bTrrdWarEngage            = perfCfControllerMemTuneIsEngagedLwrr(pControllerMemTune);
    pStatus->trrdWarEngageCounter      = pControllerMemTune->trrdWarEngageCounter;

perfCfControllerIfaceModel10GetStatus_MEM_TUNE_exit:
    return status;
}

/*!
 * @copydoc PerfCfControllerExelwte
 */
FLCN_STATUS
perfCfControllerExelwte_MEM_TUNE
(
    PERF_CF_CONTROLLER *pController
)
{
    PERF_CF_CONTROLLER_MEM_TUNE         *pControllerMemTune =
        (PERF_CF_CONTROLLER_MEM_TUNE *)pController;
    PERF_CF_PM_SENSORS_STATUS           *pPmSensorsStatus;
    RM_PMU_PERF_CF_PM_SENSOR_GET_STATUS *pRmPmSensorStatus;
    LwBool                  bTrrdWarEngage  =
        perfCfControllerMemTuneIsEngagedLwrr(pControllerMemTune);   // <! Init to last requested.
    LwU8                    tFAWLwrr        =
        LW2080_CTRL_PERF_PERF_CF_MEM_TUNE_ENGAGE_TFAW_VAL;          // <! Init to default.
    LwU64                   million         = 1000000;
    LwU64                   numChannel;
    LwU64                   channelRate;
    LwU64                   perfms;
    LwUFXP40_24             perfns40_24;
    LwUFXP52_12             observed52_12;
    LwUFXP52_12             rdObserved52_12;
    LwUFXP52_12             wrObserved52_12;
    LwU8                    tIdx;
    LwBoardObjIdx           pmIdx;
    FLCN_STATUS             status          = FLCN_OK;
    LwBool                  bEngage[LW2080_CTRL_PERF_PERF_CF_CONTROLLER_MEM_TUNE_TARGET_MAX]    = {0};
    LwBool                  bDisengage[LW2080_CTRL_PERF_PERF_CF_CONTROLLER_MEM_TUNE_TARGET_MAX] = {0};
    PERF_CF_CONTROLLER_MEM_TUNE_MOVING_AVG_FILTER *pFilter = &pControllerMemTune->filter;

    // Validate controller parameters
    status = perfCfControllersSanityCheck_HAL(&PerfCf);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfCfControllerExelwte_MEM_TUNE_exit;
    }

    // Update the previous sample decision.
    pControllerMemTune->tFAWPrev = pControllerMemTune->tFAWLwrr;

    status = perfCfControllerExelwte_SUPER(pController);
    if (status != FLCN_OK)
    {
        if (FLCN_ERR_STATE_RESET_NEEDED == status)
        {
            pControllerMemTune->hysteresisCountIncLwrr = 0;
            pControllerMemTune->hysteresisCountDecLwrr = 0;

            // Reset tRRD
            bTrrdWarEngage = LW_FALSE;

            // State reset done. Skip this cycle below.
            status = FLCN_OK;
        }

        goto perfCfControllerExelwte_MEM_TUNE_exit;
    }

    //
    // Always increment the counter.
    // Do not include the RESET to catch cases where someone
    // injects periodic reset.
    //
    pControllerMemTune->samplesCounter =
        (((pControllerMemTune->samplesCounter + 1) == LW_U32_MAX) ?
            0 : (pControllerMemTune->samplesCounter + 1));

    // Read current clock frequency

    // Reading clock frequency and colwert from FXP40.24 in GHz to kHz.
    LwU64_ALIGN32_UNPACK(&pControllerMemTune->mclkkHz52_12,
        &pController->pMultData->topologysStatus.topologys[pControllerMemTune->super.topologyIdx].topology.reading);

    lwU64Mul(&pControllerMemTune->mclkkHz52_12, &pControllerMemTune->mclkkHz52_12, &million);

    // Right shift by 12 to colwert 40.24 -> 52.12
    lw64Lsr(&pControllerMemTune->mclkkHz52_12,
            &pControllerMemTune->mclkkHz52_12,
            12);

    // If LHR enhancement is enabled, use the POR max frequency.
    if (perfCfControllerMemTuneIsLhrEnhancementEnabled(pControllerMemTune))
    {
        LwU32 mclkFreqkHz;
        LwU32 porMaxMclkkHz;

        // Right shift by 44 to check LwU32 value overflow
        lw64Lsr(&observed52_12,
                &pControllerMemTune->mclkkHz52_12,
                44);
        if (0 != observed52_12)
        {
            status = FLCN_ERR_ILWALID_STATE;
            PMU_BREAKPOINT();
            goto perfCfControllerExelwte_MEM_TUNE_exit;
        }
        lw64Lsr(&observed52_12,
                &pControllerMemTune->mclkkHz52_12,
                12);
        mclkFreqkHz = (LwU32)observed52_12;

        // Right shift by 44 to check LwU32 value overflow
        lw64Lsr(&observed52_12,
                &pControllerMemTune->porMaxMclkkHz52_12,
                44);
        if (0 != observed52_12)
        {
            status = FLCN_ERR_ILWALID_STATE;
            PMU_BREAKPOINT();
            goto perfCfControllerExelwte_MEM_TUNE_exit;
        }
        lw64Lsr(&observed52_12,
                &pControllerMemTune->porMaxMclkkHz52_12,
                12);
        porMaxMclkkHz = (LwU32)observed52_12;


        if (((porMaxMclkkHz + 25000) <= mclkFreqkHz) &&
            (porMaxMclkkHz <= mclkFreqkHz))
        {

            tFAWLwrr += LW_UNSIGNED_DIV_CEIL((mclkFreqkHz - porMaxMclkkHz), 100000);
            tFAWLwrr = LW_MIN(tFAWLwrr, LW2080_CTRL_PERF_PERF_CF_MEM_TUNE_ENGAGE_TFAW_VAL_MAX);
        }

        // Always use the POR max MCLK frequency for all callwlations.
        pControllerMemTune->mclkkHz52_12 = pControllerMemTune->porMaxMclkkHz52_12;
    }

    // 2. Read time using pTIMER topology
    LwU64_ALIGN32_UNPACK(
        &perfns40_24,
        &pController->pMultData->topologysStatus.topologys[pControllerMemTune->timerTopologyIdx].topology.reading);

    // Divide to go from ns to ms
    lwU64Div(&pControllerMemTune->perfms52_12, &perfns40_24, &million);

    // Right shift by 12 to colwert 40.24 -> 52.12
    lw64Lsr(&pControllerMemTune->perfms52_12,
            &pControllerMemTune->perfms52_12,
            12);

    // For internal callwlation, removing sub-ms precision.
    lw64Lsr(&perfms,
            &pControllerMemTune->perfms52_12,
            12);

    // 3. Read BAPM counters
    pPmSensorsStatus = perfCfControllerMultiplierDataPmSensorsStatusGet(pController->pMultData);
    PMU_ASSERT_OK_OR_GOTO(status,
        (pPmSensorsStatus == NULL) ? FLCN_ERR_ILWALID_STATE : FLCN_OK,
        perfCfControllerExelwte_MEM_TUNE_exit);

    pRmPmSensorStatus =
        &pPmSensorsStatus->pmSensors[pControllerMemTune->pmSensorIdx].pmSensor;

    // Reading the BAPM signal
    for (tIdx = 0; tIdx < pControllerMemTune->targets.numTargets; tIdx++)
    {
        LwU64 cntDiffUnpack;
        LwU64 correction = PERF_CF_CONTROLLER_MEM_TUNE_PM_SIGNAL_CORRECTION;

        // Init
        pControllerMemTune->targets.target[tIdx].cntDiffTotal = 0;

        for (pmIdx = 0; pmIdx < pControllerMemTune->targets.target[tIdx].numSignals; pmIdx++)
        {
            PMU_ASSERT_OK_OR_GOTO(status,
                (LW2080_CTRL_BOARDOBJGRP_MASK_BIT_GET(&(pRmPmSensorStatus->signalsMask.super),
                    pControllerMemTune->targets.target[tIdx].signal[pmIdx].index) == 0) ? FLCN_ERR_ILWALID_STATE : FLCN_OK,
                perfCfControllerExelwte_MEM_TUNE_exit);

            pControllerMemTune->targets.target[tIdx].signal[pmIdx].cntDiff =
                pRmPmSensorStatus->signals[pControllerMemTune->targets.target[tIdx].signal[pmIdx].index].cntDiff;

            // Unpack the count diff.
            LwU64_ALIGN32_UNPACK((&cntDiffUnpack),
                (&pControllerMemTune->targets.target[tIdx].signal[pmIdx].cntDiff));

            // Apply the correction
            lwU64Mul((&cntDiffUnpack), (&cntDiffUnpack), &correction);

            // Add all counters values.
            lw64Add_MOD(&pControllerMemTune->targets.target[tIdx].cntDiffTotal,
                        &pControllerMemTune->targets.target[tIdx].cntDiffTotal,
                        &cntDiffUnpack);
        }
    }

    // Dynamic callwlations

    // Get number of 16-bit channels from fuses.  This should multiply out as:
    // 3090: 6 fbps * 1 fbpas/fbp * 2 subps/fbpa * 2 channels/subp = 24 channels
    // 3080: 5 fbps * 1 fbpas/fbp * 2 subps/fbpa * 2 channels/subp = 20 channels
    // 3070: 4 fbps * 1 fbpas/fbp * 2 subps/fbpa * 2 channels/subp = 16 channels
    // channels = <num_fbps> * <num fbpas/fbp> * <num subp perf fbpa> * <num channels per subp>

    // SOL memory bandwidth
    // <mem bitrate> is the bitrate of the dram, which is 2x memory frequency
    // FBfalcon hides the fact that GDDR6x is PAM4 by reporting a higher memory frequency as if it was PAM2/DDR.
    //
    // Should multiply out as:
    // 3090: 19_500_000_000 Hz * 24 channels * 2 bytes/channel = 936_000_000_000 bytes/second
    // 3080: 19_000_000_000 Hz * 20 channels * 2 bytes/channel = 760_000_000_000 bytes/second
    // 3070: 14_000_000_000 Hz * 16 channels * 2 bytes/channel = 448_000_000_000 bytes/second
    // bandwidth_sol = <mem bitrate> * channels * 2

    // SOL activation callwlated based on 128B reads
    // The real system has peak activate rate around ~80B reads, but we assume the 128B reads ETH uses
    // act_rate_sol          = bandwidth_sol / 128

    numChannel  = (LwU64)fbGetNumChannels_HAL(&Fb);

    PMU_ASSERT_OK_OR_GOTO(status,
        (numChannel == 0) ? FLCN_ERR_ILWALID_STATE : FLCN_OK,
        perfCfControllerExelwte_MEM_TUNE_exit);

    channelRate = PERF_CF_CONTROLLER_MEM_TUNE_TOTAL_ACT_CH_RATE;

    // <mem freq> * <# channels>
    lwU64Mul(&pControllerMemTune->activateRateSol52_12,
             &pControllerMemTune->mclkkHz52_12,
             &numChannel);

    // <mem freq> * <# channels> / 32
    lwU64Div(&pControllerMemTune->activateRateSol52_12,
             &pControllerMemTune->activateRateSol52_12,
             &channelRate);

    // SOL access rate.  Each acess is 32B
    // acc_rate_sol          = bandwidth_sol / 32

    // rd_acc_rate_sol       = <mem freq> * <# channels> / 8
    channelRate = PERF_CF_CONTROLLER_MEM_TUNE_READ_ACC_CH_RATE;

    // <mem freq> * <# channels>
    lwU64Mul(&pControllerMemTune->accessRateSol52_12,
             &pControllerMemTune->mclkkHz52_12,
             &numChannel);

    // <mem freq> * <# channels> / 8
    lwU64Div(&pControllerMemTune->accessRateSol52_12,
             &pControllerMemTune->accessRateSol52_12,
             &channelRate);

    // Callwlate observed value for LW2080_CTRL_PERF_PERF_CF_CONTROLLER_MEM_TUNE_TARGET_TOTAL_ACTIVATES
    tIdx = LW2080_CTRL_PERF_PERF_CF_CONTROLLER_MEM_TUNE_TARGET_TOTAL_ACTIVATES;

    //
    // Left shift counter diff by 24 to colwert 64.00 -> 40.24
    // This is required as we plan to divide this number by
    // time (64) and sol rate (52.12) with expected output in 52.12
    //

    // Check for overflow by shifting right 40 bits
    lw64Lsr(&observed52_12,
            &pControllerMemTune->targets.target[tIdx].cntDiffTotal,
            40);
    if (0 != observed52_12)
    {
        pControllerMemTune->targets.target[tIdx].observed = LW_U32_MAX;

        // Ignore the samples with overflow
        goto perfCfControllerExelwte_MEM_TUNE_exit;
    }

    //
    // Left shift total observed counter value by 24 to colwert 64.00 -> 40.24
    //
    lw64Lsl(&observed52_12,
            &pControllerMemTune->targets.target[tIdx].cntDiffTotal,
            24);

    // act_rate_obs = act_obs / SAMPLING_PERIOD; Answer in 40.24 / 64 = 40.24
    if (perfms != 0)
    {
        lwU64Div(&observed52_12,
                 &observed52_12,
                 &perfms);
    }
    else
    {
        observed52_12 = LW_U64_MAX;
    }

    // act_rate_pct = act_rate_obs / act_rate_sol;
    if (pControllerMemTune->activateRateSol52_12 != 0)
    {
        lwU64Div(&observed52_12,
                 &observed52_12,
                 &pControllerMemTune->activateRateSol52_12);
    }
    else
    {
        observed52_12 = LW_U64_MAX;
    }

    pControllerMemTune->targets.target[tIdx].observed = (LwUFXP20_12)observed52_12;

    // Right shift by 32 to check higher 32 bits
    lw64Lsr(&observed52_12,
            &observed52_12,
            32);
    if (0 != observed52_12)
    {
        pControllerMemTune->targets.target[tIdx].observed = LW_U32_MAX;

        // Ignore the samples with overflow
        goto perfCfControllerExelwte_MEM_TUNE_exit;
    }

    // Filter window update.
    if (perfCfControllerMemTuneIsLhrEnhancementEnabled(pControllerMemTune) &&
        pControllerMemTune->bFilterEnabled)
    {
        // Update this sample target data.
        pFilter->targetData[tIdx].observedSample[pFilter->count] =
                pControllerMemTune->targets.target[tIdx].observed;
    }

    if (pControllerMemTune->targets.target[tIdx].observed >= pControllerMemTune->targets.target[tIdx].target.high)
    {
        bEngage[tIdx] = LW_TRUE;
    }
    else if (pControllerMemTune->targets.target[tIdx].observed < pControllerMemTune->targets.target[tIdx].target.low)
    {
        bDisengage[tIdx] = LW_TRUE;
    }

    // Callwlate observed value for LW2080_CTRL_PERF_PERF_CF_CONTROLLER_MEM_TUNE_TARGET_READ_ACCESS
    tIdx = LW2080_CTRL_PERF_PERF_CF_CONTROLLER_MEM_TUNE_TARGET_READ_ACCESS;

    //
    // Left shift counter diff by 24 to colwert 64.00 -> 40.24
    // This is required as we plan to divide this number by
    // time (64) and sol rate (52.12) with expected output in 52.12
    //

    // Check for overflow by shifting right 40 bits
    lw64Lsr(&observed52_12,
            &pControllerMemTune->targets.target[tIdx].cntDiffTotal,
            40);
    if (0 != observed52_12)
    {
        pControllerMemTune->targets.target[tIdx].observed = LW_U32_MAX;

        // Ignore the samples with overflow
        goto perfCfControllerExelwte_MEM_TUNE_exit;
    }

    //
    // Left shift total observed counter value by 24 to colwert 64.00 -> 40.24
    //
    lw64Lsl(&observed52_12,
            &pControllerMemTune->targets.target[tIdx].cntDiffTotal,
            24);

    // rd_acc_rate_obs = rd_acc_obs / SAMPLING_PERIOD; Answer in 40.24 / 64 = 40.24
    if (perfms != 0)
    {
        lwU64Div(&observed52_12,
                 &observed52_12,
                 &perfms);
    }
    else
    {
        observed52_12 = LW_U64_MAX;
    }

    // rd_acc_rate_pct = rd_acc_rate_obs / rd_acc_rate_sol;
    if (pControllerMemTune->accessRateSol52_12 != 0)
    {
        lwU64Div(&observed52_12,
                 &observed52_12,
                 &pControllerMemTune->accessRateSol52_12);
    }
    else
    {
        observed52_12 = LW_U64_MAX;
    }

    pControllerMemTune->targets.target[tIdx].observed = (LwUFXP20_12)observed52_12;

    // Cache the result of rd_acc_rate_pct
    rdObserved52_12 = observed52_12;

    // Right shift by 32 to check higher 32 bits
    lw64Lsr(&observed52_12,
            &observed52_12,
            32);
    if (0 != observed52_12)
    {
        pControllerMemTune->targets.target[tIdx].observed = LW_U32_MAX;

        // Ignore the samples with overflow
        goto perfCfControllerExelwte_MEM_TUNE_exit;
    }

    // Filter window update.
    if (perfCfControllerMemTuneIsLhrEnhancementEnabled(pControllerMemTune) &&
        pControllerMemTune->bFilterEnabled)
    {
        // Update this sample target data.
        pFilter->targetData[tIdx].observedSample[pFilter->count] =
                pControllerMemTune->targets.target[tIdx].observed;
    }

    if (pControllerMemTune->targets.target[tIdx].observed >= pControllerMemTune->targets.target[tIdx].target.high)
    {
        bEngage[tIdx] = LW_TRUE;
    }
    else if (pControllerMemTune->targets.target[tIdx].observed < pControllerMemTune->targets.target[tIdx].target.low)
    {
        bDisengage[tIdx] = LW_TRUE;
    }

    // Callwlate observed value for LW2080_CTRL_PERF_PERF_CF_CONTROLLER_MEM_TUNE_TARGET_WRITE_ACCESS
    tIdx = LW2080_CTRL_PERF_PERF_CF_CONTROLLER_MEM_TUNE_TARGET_WRITE_ACCESS;

    //
    // Left shift counter diff by 24 to colwert 64.00 -> 40.24
    // This is required as we plan to divide this number by
    // time (64) and sol rate (52.12) with expected output in 52.12
    //

    // Check for overflow by shifting right 40 bits
    lw64Lsr(&observed52_12,
            &pControllerMemTune->targets.target[tIdx].cntDiffTotal,
            40);
    if (0 != observed52_12)
    {
        pControllerMemTune->targets.target[tIdx].observed = LW_U32_MAX;

        // Ignore the samples with overflow
        goto perfCfControllerExelwte_MEM_TUNE_exit;
    }

    //
    // Left shift total observed counter value by 24 to colwert 64.00 -> 40.24
    //
    lw64Lsl(&observed52_12,
            &pControllerMemTune->targets.target[tIdx].cntDiffTotal,
            24);

    // wr_acc_rate_obs = wr_acc_obs / SAMPLING_PERIOD; Answer in 40.24 / 64 = 40.24
    if (perfms != 0)
    {
        lwU64Div(&observed52_12,
                 &observed52_12,
                 &perfms);
    }
    else
    {
        observed52_12 = LW_U64_MAX;
    }

    // wr_acc_rate_pct = wr_acc_rate_obs / wr_acc_rate_sol;
    if (pControllerMemTune->accessRateSol52_12 != 0)
    {
        lwU64Div(&observed52_12,
                 &observed52_12,
                 &pControllerMemTune->accessRateSol52_12);
    }
    else
    {
        observed52_12 = LW_U64_MAX;
    }

    pControllerMemTune->targets.target[tIdx].observed = (LwUFXP20_12)observed52_12;

    // Cache the result of wr_acc_rate_pct
    wrObserved52_12 = observed52_12;

    // Right shift by 32 to check higher 32 bits
    lw64Lsr(&observed52_12,
            &observed52_12,
            32);
    if (0 != observed52_12)
    {
        pControllerMemTune->targets.target[tIdx].observed = LW_U32_MAX;

        // Ignore the samples with overflow
        goto perfCfControllerExelwte_MEM_TUNE_exit;
    }

    // Filter window update.
    if (perfCfControllerMemTuneIsLhrEnhancementEnabled(pControllerMemTune) &&
        pControllerMemTune->bFilterEnabled)
    {
        // Update this sample target data.
        pFilter->targetData[tIdx].observedSample[pFilter->count] =
                pControllerMemTune->targets.target[tIdx].observed;
    }

    // Write is ilwerted case where the expectations are mining does not perform enough write operation.
    if (pControllerMemTune->targets.target[tIdx].observed <= pControllerMemTune->targets.target[tIdx].target.high)
    {
        bEngage[tIdx] = LW_TRUE;
    }
    else if (pControllerMemTune->targets.target[tIdx].observed > pControllerMemTune->targets.target[tIdx].target.low)
    {
        bDisengage[tIdx] = LW_TRUE;
    }

    // Callwlate observed value for LW2080_CTRL_PERF_PERF_CF_CONTROLLER_MEM_TUNE_TARGET_RD_SUB_WR_ACCESS
    tIdx = LW2080_CTRL_PERF_PERF_CF_CONTROLLER_MEM_TUNE_TARGET_RD_SUB_WR_ACCESS;

    if (rdObserved52_12 >= wrObserved52_12)
    {
        LwU64 wrMulFactor = 30;     // 30 %
        LwU64 wrDivFactor = 100;    // 30 %

        // Write observed * 30 / 100
        lwU64Div(&wrObserved52_12,
                 &wrObserved52_12,
                 &wrDivFactor);

        lwU64Mul(&wrObserved52_12,
                 &wrObserved52_12,
                 &wrMulFactor);

        // rd_sub_wr_acc_rate_pct = rd_acc_rate_pct - 0.3 x wr_acc_rate_pct;
        lw64Sub(&observed52_12,
                &rdObserved52_12,
                &wrObserved52_12);
    }
    else
    {
        observed52_12 = 0;
    }


    pControllerMemTune->targets.target[tIdx].observed = (LwUFXP20_12)observed52_12;

    // Right shift by 32 to check higher 32 bits
    lw64Lsr(&observed52_12,
            &observed52_12,
            32);
    if (0 != observed52_12)
    {
        pControllerMemTune->targets.target[tIdx].observed = LW_U32_MAX;

        // Ignore the samples with overflow
        goto perfCfControllerExelwte_MEM_TUNE_exit;
    }

    // Filter window update.
    if (perfCfControllerMemTuneIsLhrEnhancementEnabled(pControllerMemTune) &&
        pControllerMemTune->bFilterEnabled)
    {
        // Update this sample target data.
        pFilter->targetData[tIdx].observedSample[pFilter->count] =
                pControllerMemTune->targets.target[tIdx].observed;
    }

    if (pControllerMemTune->targets.target[tIdx].observed >= pControllerMemTune->targets.target[tIdx].target.high)
    {
        bEngage[tIdx] = LW_TRUE;
    }
    else if (pControllerMemTune->targets.target[tIdx].observed < pControllerMemTune->targets.target[tIdx].target.low)
    {
        bDisengage[tIdx] = LW_TRUE;
    }

    // If all the targets have requested for engage, inc the engage counter
    for (tIdx = 0; tIdx < pControllerMemTune->targets.numTargets; tIdx++)
    {
        if (!bEngage[tIdx])
        {
            break;
        }
    }

    LwU8 incStepSize        = DEFAULT_INC_STEP_SIZE_LHR10;
    LwU8 decStepSize        = DEFAULT_DEC_STEP_SIZE_LHR10;
    LwU8 intermediaStepSize = INTERMEDIATE_DEC_STEP_SIZE_LHR10;

    if (perfCfControllerMemTuneIsLhrEnhancementEnabled(pControllerMemTune))
    {
        incStepSize        = DEFAULT_INC_STEP_SIZE_LHR11;
        decStepSize        = DEFAULT_DEC_STEP_SIZE_LHR11;
        intermediaStepSize = INTERMEDIATE_DEC_STEP_SIZE_LHR11;
    }

    // If index equal to max, none the observation are below high threshold.
    if (tIdx == pControllerMemTune->targets.numTargets)
    {
        //
        // If all observations are above threshold, increment the inc counter and
        // reset the dec counter
        //
        pControllerMemTune->hysteresisCountIncLwrr += incStepSize;
        pControllerMemTune->hysteresisCountDecLwrr =
            ((pControllerMemTune->hysteresisCountDecLwrr > 0) ? (pControllerMemTune->hysteresisCountDecLwrr - decStepSize) : 0);

        if (pControllerMemTune->hysteresisCountIncLwrr >= (pControllerMemTune->hysteresisCountInc * incStepSize))
        {
            bTrrdWarEngage                             = LW_TRUE;
            pControllerMemTune->hysteresisCountIncLwrr = 0;
        }
    }
    //
    // If all the targets did not request for engagement,
    // check for any of them requested for disengage or all of them are
    // somewhere in between their low and high threshold.
    //
    else
    {
        //
        // If any of them are below the low threshold, increment the dec counter and
        // decrement the inc counter.
        //
        for (tIdx = 0; tIdx < pControllerMemTune->targets.numTargets; tIdx++)
        {
            if (bDisengage[tIdx])
            {
                break;
            }
        }

        // If index is not equal to max, at least one of the observation is below the low threshold.
        if (tIdx != pControllerMemTune->targets.numTargets)
        {
            //
            // If all observations are below threshold, increment the dec counter and
            // decrement the inc counter
            //
            pControllerMemTune->hysteresisCountDecLwrr += incStepSize;
            pControllerMemTune->hysteresisCountIncLwrr =
                ((pControllerMemTune->hysteresisCountIncLwrr > 0) ? (pControllerMemTune->hysteresisCountIncLwrr - decStepSize) : 0);

            if (pControllerMemTune->hysteresisCountDecLwrr >= (pControllerMemTune->hysteresisCountDec * incStepSize))
            {
                bTrrdWarEngage                             = LW_FALSE;
                pControllerMemTune->hysteresisCountDecLwrr = 0;
            }
        }
        else
        {
            //
            // If NOT all observations are able to cross the low | high threshold,
            // decrement both counters as we want to observe conselwtive crossing of
            // observe above | below threshold.
            //
            pControllerMemTune->hysteresisCountIncLwrr =
                ((pControllerMemTune->hysteresisCountIncLwrr > 0) ? (pControllerMemTune->hysteresisCountIncLwrr - intermediaStepSize) : 0);
            pControllerMemTune->hysteresisCountDecLwrr =
                ((pControllerMemTune->hysteresisCountDecLwrr > 0) ? (pControllerMemTune->hysteresisCountDecLwrr - intermediaStepSize) : 0);
        }
    }

    if (perfCfControllerMemTuneIsLhrEnhancementEnabled(pControllerMemTune) &&
        pControllerMemTune->bFilterEnabled)
    {
        LwU64       sampleSize64;
        LwUFXP52_12 observed52_12;
        LwU8        i;

        // Increment the counter.
        pFilter->count++;

        // If window was not yet full, check if it is full now.
        if (!pFilter->bWindowFull &&
            (pFilter->count == PERF_CF_CONTROLLER_MEM_TUNE_MOVING_AVG_FILTER_WINDOW_SIZE))
        {
            pFilter->bWindowFull = LW_TRUE;
        }

        // Reset count on reaching window size
        pFilter->count =
            pFilter->count % PERF_CF_CONTROLLER_MEM_TUNE_MOVING_AVG_FILTER_WINDOW_SIZE;

        // Do not run the algorithm unless the filter window is full to avoid false positives.
        if (!pFilter->bWindowFull)
        {
            goto perfCfControllerExelwte_MEM_TUNE_Filter_done;
        }

        for (tIdx = 0; tIdx < pControllerMemTune->targets.numTargets; tIdx++)
        {
            // Reset the total
            observed52_12 = 0;

            // Sum all the samples and average them against the window size
            for (i = 0; i < PERF_CF_CONTROLLER_MEM_TUNE_MOVING_AVG_FILTER_WINDOW_SIZE; i++)
            {
                observed52_12 += pFilter->targetData[tIdx].observedSample[i];
            }

            // Callwlate the avg over the window size. 52.12 / 64 = 52.12
            sampleSize64 = PERF_CF_CONTROLLER_MEM_TUNE_MOVING_AVG_FILTER_WINDOW_SIZE;
            lwU64Div(&observed52_12,
                     &observed52_12,
                     &sampleSize64);

            pFilter->targetData[tIdx].observedAvg = (LwUFXP20_12)observed52_12;

            // Right shift by 32 to check higher 32 bits
            lw64Lsr(&observed52_12,
                    &observed52_12,
                    32);
            if (0 != observed52_12)
            {
                pFilter->targetData[tIdx].observedAvg = LW_U32_MAX;

                status = FLCN_ERR_ILWALID_STATE;

                // We do not expect overflow.
                PMU_BREAKPOINT();

                // Ignore the samples with overflow
                goto perfCfControllerExelwte_MEM_TUNE_exit;
            }
        }

        for (tIdx = 0; tIdx < pControllerMemTune->targets.numTargets; tIdx++)
        {
            // Write is ilwerted case where the expectations are mining does not perform enough write operation.
            if (tIdx == LW2080_CTRL_PERF_PERF_CF_CONTROLLER_MEM_TUNE_TARGET_WRITE_ACCESS)
            {
                if (pFilter->targetData[tIdx].observedAvg > pControllerMemTune->targets.target[tIdx].target.high)
                {
                    break;
                }
            }
            else if (pFilter->targetData[tIdx].observedAvg < pControllerMemTune->targets.target[tIdx].target.high)
            {
                break;
            }
        }

        // If index equal to max, all observations matches engage criteria.
        if (tIdx == pControllerMemTune->targets.numTargets)
        {
            bTrrdWarEngage = LW_TRUE;
        }

perfCfControllerExelwte_MEM_TUNE_Filter_done:
        lwosNOP();
    }

perfCfControllerExelwte_MEM_TUNE_exit:

    // Trigger mode switch if required.
    if (status == FLCN_OK)
    {
        // Update the time-stamp when controller gets chance to run and make decision.
        PMU_HALT_OK_OR_GOTO(status,
            sysPerfMemTuneControllerSetTimestamp((LwU64)PERF_MEM_TUNE_CONTROLLER_TIMESTAMP),
            perfCfControllerExelwte_MEM_TUNE_CHSEQ_exit);

        // Override the tFAW to disengage value if required as per controller decision.
        if (!bTrrdWarEngage)
        {
            tFAWLwrr = LW2080_CTRL_PERF_PERF_CF_MEM_TUNE_DISENGAGE_TFAW_VAL;
        }

        if (pControllerMemTune->tFAWLwrr != tFAWLwrr)
        {
            PMU_ASSERT_OK_OR_GOTO(status,
                s_perfCfControllerQueueMemTuneChange(tFAWLwrr),
                perfCfControllerExelwte_MEM_TUNE_CHSEQ_exit);

            // Cache the last requested mode.
            pControllerMemTune->tFAWLwrr = tFAWLwrr;
        }
    }

    // Increment the counter if WAR is engaged.
    if (perfCfControllerMemTuneIsEngagedLwrr(pControllerMemTune))
    {
        if ((pControllerMemTune->trrdWarEngageCounter + 1) == LW_U32_MAX)
        {
            pControllerMemTune->trrdWarEngageCounter = 0;
        }
        else
        {
            pControllerMemTune->trrdWarEngageCounter += 1;
        }
    }

perfCfControllerExelwte_MEM_TUNE_CHSEQ_exit:
    return status;
}

/*!
 * @copydoc PerfCfControllerSetMultData
 */
FLCN_STATUS
perfCfControllerSetMultData_MEM_TUNE
(
    PERF_CF_CONTROLLER                 *pController,
    PERF_CF_CONTROLLER_MULTIPLIER_DATA *pMultData
)
{
    PERF_CF_CONTROLLER_MEM_TUNE *pControllerMemTune =
        (PERF_CF_CONTROLLER_MEM_TUNE *)pController;
    FLCN_STATUS status = FLCN_OK;

    // Sanity checking.
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pController == NULL) ||
         (pMultData   == NULL)) ? FLCN_ERR_ILWALID_STATE : FLCN_OK,
        perfCfControllerSetMultData_MEM_TUNE_exit);

    // Set the PTIMER topology the memory tuning controller needs.
    if (LW2080_CTRL_BOARDOBJ_IDX_ILWALID !=
            pControllerMemTune->timerTopologyIdx)
    {
        boardObjGrpMaskBitSet(&(pMultData->topologysStatus.mask),
            pControllerMemTune->timerTopologyIdx);
    }

    PMU_ASSERT_OK_OR_GOTO(status,
        perfCfControllerMultiplierDataPmSensorsStatusInit(pMultData),
        perfCfControllerSetMultData_MEM_TUNE_exit);

perfCfControllerSetMultData_MEM_TUNE_exit:
    return status;
}

/*!
 * @copydoc PerfCfControllerLoad
 */
FLCN_STATUS
perfCfControllerLoad_MEM_TUNE
(
    PERF_CF_CONTROLLER *pController
)
{
    PERF_CF_CONTROLLER_MEM_TUNE         *pControllerMemTune = (PERF_CF_CONTROLLER_MEM_TUNE *)pController;
    PERF_CF_PM_SENSORS_STATUS           *pPmSensorsStatus;
    RM_PMU_PERF_CF_PM_SENSOR_GET_STATUS *pRmPmSensorStatus;
    BOARDOBJGRPMASK_E1024                pmSensorSignalMask;
    FLCN_STATUS                          status = FLCN_OK;
    LwU8                                 tIdx;
    LwBoardObjIdx                        pmIdx;

    pPmSensorsStatus = perfCfControllerMultiplierDataPmSensorsStatusGet(pController->pMultData);
    PMU_ASSERT_OK_OR_GOTO(status,
        (pPmSensorsStatus == NULL) ? FLCN_ERR_ILWALID_STATE : FLCN_OK,
        perfCfControllerLoad_MEM_TUNE_exit);

    boardObjGrpMaskBitSet(
        &pPmSensorsStatus->mask,
        pControllerMemTune->pmSensorIdx);

    pRmPmSensorStatus =
        &pPmSensorsStatus->pmSensors[pControllerMemTune->pmSensorIdx].pmSensor;

    boardObjGrpMaskInit_E1024(&pmSensorSignalMask);

    PMU_ASSERT_OK_OR_GOTO(status,
        boardObjGrpMaskImport_E1024(
            &pmSensorSignalMask,
            &pRmPmSensorStatus->signalsMask),
        perfCfControllerLoad_MEM_TUNE_exit);

    for (tIdx = 0; tIdx < pControllerMemTune->targets.numTargets; tIdx++)
    {
        for (pmIdx = 0; pmIdx < pControllerMemTune->targets.target[tIdx].numSignals; pmIdx++)
        {
            boardObjGrpMaskBitSet(&pmSensorSignalMask,
                pControllerMemTune->targets.target[tIdx].signal[pmIdx].index);
        }
    }

    PMU_ASSERT_OK_OR_GOTO(status,
        boardObjGrpMaskExport_E1024(
            &pmSensorSignalMask,
            &pRmPmSensorStatus->signalsMask),
        perfCfControllerLoad_MEM_TUNE_exit);

perfCfControllerLoad_MEM_TUNE_exit:
    return status;
}

/*!
 * @copydoc perfCfControllerMemTuneMonitorLoad
 */
FLCN_STATUS
perfCfControllerMemTuneMonitorLoad(void)
{
    FLCN_STATUS status = FLCN_OK;

    // Enable sanity check only if controller is avtive.
    if (!perfCfControllerMemTuneIsActive_HAL(&PerfCf))
    {
        goto perfCfControllerMemTuneMonitorLoad_exit;
    }

    // Init
    s_perfCfControllerMemTuneMonitor.samplesMisMatchCounter = 0;
    s_perfCfControllerMemTuneMonitor.trrdMisMatchCounterSW  = 0;
    s_perfCfControllerMemTuneMonitor.trrdMisMatchCounterHW  = 0;

    if (!OS_TMR_CALLBACK_WAS_CREATED(&(OsCBPerfCfControllerMemTuneMonitor)))
    {
        status = osTmrCallbackCreate(&(OsCBPerfCfControllerMemTuneMonitor),                 // pCallback
                    OS_TMR_CALLBACK_TYPE_RELWRRENT_TO_EXPECTED_SKIP_MISSED,                 // type
                    OVL_INDEX_IMEM(perfCf),                                                 // ovlImem
                    s_perfCfControllerMemTuneMonitorCallback,                               // pTmrCallbackFunc
                    LWOS_QUEUE(PMU, PERF_CF),                                               // queueHandle
                    PERF_CF_CONTROLLER_MEM_TUNE_MONITOR_ACTIVE_POWER_CALLBACK_PERIOD_US(),  // periodNormalus
                    PERF_CF_CONTROLLER_MEM_TUNE_MONITOR_LOW_POWER_CALLBACK_PERIOD_US(),     // periodSleepus
                    OS_TIMER_RELAXED_MODE_USE_NORMAL,                                       // bRelaxedUseSleep
                    RM_PMU_TASK_ID_PERF_CF);                                                // taskId
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfCfControllerMemTuneMonitorLoad_exit;
        }

        status = osTmrCallbackSchedule(&(OsCBPerfCfControllerMemTuneMonitor));
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfCfControllerMemTuneMonitorLoad_exit;
        }
    }

perfCfControllerMemTuneMonitorLoad_exit:
    return status;
}

/*!
 * @brief Helper interface to confirm whether the memory tuning controller
 *        is engaged (actively throttling the performance).
 *
 * @param[in]   pControllerMemTune  PERF_CF_CONTROLLER_MEM_TUNE object pointer.
 *
 * @return LW_TRUE  If the controller is engaged.
 * @return LW_FALSE Otherwise
 */
LwBool
perfCfControllerMemTuneIsEngaged
(
    PERF_CF_CONTROLLER_MEM_TUNE *pControllerMemTune
)
{
    LwBool bEngaged = LW_FALSE;

    if (pControllerMemTune == NULL)
    {
        PMU_BREAKPOINT();
        goto perfCfControllerMemTuneIsEngaged_exit;
    }
    bEngaged = perfCfControllerMemTuneIsEngagedLwrr(pControllerMemTune);

perfCfControllerMemTuneIsEngaged_exit:
    return bEngaged;
}

/*!
 * @brief Helper interface to get the memory tuning controller
 *        is engage counter.
 *
 * @param[in]   pControllerMemTune  PERF_CF_CONTROLLER_MEM_TUNE object pointer.
 *
 * @return NEngage counter value. Zero if invalid pointer.
 */
LwU32
perfCfControllerMemTuneEngageCounterGet
(
    PERF_CF_CONTROLLER_MEM_TUNE *pControllerMemTune
)
{
    LwU32 engageCounter = 0;

    if (pControllerMemTune == NULL)
    {
        PMU_BREAKPOINT();
        goto perfCfControllerMemTuneEngageCounterGet_exit;
    }
    engageCounter = pControllerMemTune->trrdWarEngageCounter;

perfCfControllerMemTuneEngageCounterGet_exit:
    return engageCounter;
}

/* ------------------------ Private Functions ------------------------------- */
/*!
 * @copydoc OsTmrCallbackFunc
 */
static FLCN_STATUS
s_perfCfControllerMemTuneMonitorCallback
(
    OS_TMR_CALLBACK *pCallback
)
{
    PERF_CF_CONTROLLER          *pController;
    PERF_CF_CONTROLLER_MEM_TUNE *pControllerMemTune;
    LwU32                        samplesCounter;
    FLCN_STATUS                  status = FLCN_ERR_ILWALID_STATE;

    pController = BOARDOBJGRP_OBJ_GET(PERF_CF_CONTROLLER, 8);
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pController != NULL) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        s_perfCfControllerMemTuneMonitorCallback_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pController->super.type == LW2080_CTRL_PERF_PERF_CF_CONTROLLER_TYPE_MEM_TUNE_1X) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        s_perfCfControllerMemTuneMonitorCallback_exit);

    pControllerMemTune = (PERF_CF_CONTROLLER_MEM_TUNE *)pController;
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pControllerMemTune != NULL) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        s_perfCfControllerMemTuneMonitorCallback_exit);

    samplesCounter = perfCfControllerMemTuneSamplesCounterGet(pControllerMemTune);

    // Validate sample counter
    if ((samplesCounter > PERF_CF_CONTROLLER_MEM_TUNE_MONITOR_SAMPLES_THRESH_HIGH) ||
        (samplesCounter < PERF_CF_CONTROLLER_MEM_TUNE_MONITOR_SAMPLES_THRESH_LOW))
    {
        s_perfCfControllerMemTuneMonitor.samplesMisMatchCounter++;
    }
    else
    {
        s_perfCfControllerMemTuneMonitor.samplesMisMatchCounter = 0;

        perfCfControllerMemTuneSamplesCounterReset(pControllerMemTune);
    }

    PMU_ASSERT_OK_OR_GOTO(status,
        ((s_perfCfControllerMemTuneMonitor.samplesMisMatchCounter <=
            PERF_CF_CONTROLLER_MEM_TUNE_MONITOR_SAMPLES_MISMATCH_THRESH) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        s_perfCfControllerMemTuneMonitorCallback_exit);

    // Validate the tRRD settings.
    perfReadSemaphoreTake();
    {
        // Read SW state of tRRD.
        if ((perfCfControllerMemTunetFAWLwrrGet(pControllerMemTune) !=
             perfChangeSeqChangeLasttFAWGet()) &&
            (perfCfControllerMemTunetFAWPrevGet(pControllerMemTune) !=
             perfChangeSeqChangeLasttFAWGet()))
        {
            s_perfCfControllerMemTuneMonitor.trrdMisMatchCounterSW++;
        }
        else
        {
            s_perfCfControllerMemTuneMonitor.trrdMisMatchCounterSW = 0;
        }

        // Read HW state of tRRD.
        status = perfCfControllerMemTuneMonitorSanityCheck_HAL(&PerfCf,
                    perfCfControllerMemTunetFAWLwrrGet(pControllerMemTune),
                    perfCfControllerMemTunetFAWPrevGet(pControllerMemTune),
                    &s_perfCfControllerMemTuneMonitor.clkCntrSample);
        if (status == FLCN_WARN_NOT_QUERIED)
        {
            status = FLCN_OK;
        }
        else if (status == FLCN_ERR_MISMATCHED_TARGET)
        {
            s_perfCfControllerMemTuneMonitor.trrdMisMatchCounterHW++;

            status = FLCN_OK;
        }
        else if (status == FLCN_OK)
        {
            s_perfCfControllerMemTuneMonitor.trrdMisMatchCounterHW = 0;
        }
    }
    perfReadSemaphoreGive();

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_perfCfControllerMemTuneMonitorCallback_exit;
    }

    // Init status
    status = FLCN_ERR_ILWALID_STATE;

    PMU_ASSERT_OK_OR_GOTO(status,
        ((s_perfCfControllerMemTuneMonitor.trrdMisMatchCounterSW <=
            PERF_CF_CONTROLLER_MEM_TUNE_MONITOR_TRRD_MISMATCH_THRESH) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        s_perfCfControllerMemTuneMonitorCallback_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        ((s_perfCfControllerMemTuneMonitor.trrdMisMatchCounterHW <=
            PERF_CF_CONTROLLER_MEM_TUNE_MONITOR_TRRD_MISMATCH_THRESH) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
        s_perfCfControllerMemTuneMonitorCallback_exit);

    // All PASSED!
    status = FLCN_OK;

    // Update the time-stamp when controller watchdog gets chance to run and make decision.
    PMU_HALT_OK_OR_GOTO(status,
        sysPerfMemTuneControllerSetTimestamp((LwU64)PERF_MEM_TUNE_CONTROLLER_MONITOR_TIMESTAMP),
        s_perfCfControllerMemTuneMonitorCallback_exit);

s_perfCfControllerMemTuneMonitorCallback_exit:
    return status;
}

/*!
 * @copydoc PerfCfControllerLoad
 */
static FLCN_STATUS
s_perfCfControllerQueueMemTuneChange(LwU8 tFAW)
{
    DISPATCH_PERF disp2perf = {0};

    // Queue the memory tuning settings change request.
    disp2perf.hdr.eventType =
        PERF_EVENT_ID_PERF_CHANGE_SEQ_QUEUE_MEM_TUNE_CHANGE;
    disp2perf.memTune.tFAW  = tFAW;

    // Queue the request to PERF task.
    return lwrtosQueueIdSendBlocking(LWOS_QUEUE_ID(PMU, PERF),
            &disp2perf, sizeof(disp2perf));
}

