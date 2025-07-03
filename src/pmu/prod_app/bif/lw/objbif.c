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
 * @file   objbif.c
 * @brief  Container-object for PMU BIF routines.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "lwostask.h"

/* ------------------------- Application Includes --------------------------- */
#include "main_init_api.h"
#include "cmdmgmt/cmdmgmt.h"
#include "pmu_objbif.h"
#include "pmu_objclk.h"
#include "pmu_objperf.h"
#include "g_pmurmifrpc.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
OS_TMR_CALLBACK OsCBBifXusbIsochTimeout;
//
// Initiate callback to engage Pex Power Savings feature when the system enters
// P3 for the first time
//
static LwU8 initiateCallback = 0;
/* ------------------------- Macros and Defines ----------------------------- */

/*!
 * The number of limits the power policies will be modifying.
 */
#define BIF_GEN_SPEED_PERF_LIMITS_NUM                                          1

// Timeout duration for PMU2XUSB isochronous query after which callback triggers
#define BIF_XUSB_ISOCH_QUERY_CALLBACK_PERIOD_US                              200

/* ------------------------- Private Function Prototypes -------------------- */
static FLCN_STATUS perfGetLinkSpeed(LwU32 pstateIdx, RM_PMU_BIF_LINK_SPEED *pLinkSpeed)
    GCC_ATTRIB_SECTION("imem_libBif", "perfGetLinkSpeed");
static OsTmrCallbackFunc (s_bifXusbIsochTimeoutCallbackExelwte)
    GCC_ATTRIB_USED();

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief   BIF engine constructor.
 *
 * @return  FLCN_OK     On success
 * @return  Others      Error return value from failing child function 
 */
FLCN_STATUS
constructBif(void)
{
    return lwrtosSemaphoreCreateBinaryGiven(&BifResident.switchLock, OVL_INDEX_OS_HEAP);
}

/*!
 * @brief   BIF post-init function.
 */
FLCN_STATUS
bifPostInit(void)
{
    FLCN_STATUS status = FLCN_OK;

    if (PMUCFG_FEATURE_ENABLED(PMU_BIF_PMU_PERF_LIMIT))
    {
        OSTASK_OVL_DESC ovlDescList[] = {
            OSTASK_OVL_DESC_DEFINE_PERF_PERF_LIMITS_CLIENT_INIT
            OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, bifPcieLimitClient)
        };

        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            status = perfLimitsClientInit(&Bif.pPerfLimits,
                                          OVL_INDEX_DMEM(bifPcieLimitClient),
                                          BIF_GEN_SPEED_PERF_LIMITS_NUM,
                                          LW_TRUE);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
            }
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    }

    return status;
}

/*!
 * @brief   Submits the requested Perf Limits to the PMU arbiter
 */
FLCN_STATUS
bifSetPmuPerfLimits
(
    LW2080_CTRL_PERF_PERF_LIMIT_ID perfLimitId,
    LwBool                         bClearLimit,
    LwU32                          lockSpeed
)
{
    FLCN_STATUS         status  = FLCN_OK;
    PERF_LIMITS_CLIENT *pClient = Bif.pPerfLimits;

    OSTASK_OVL_DESC ovlDescList[] = {
            // Required by perfLimitsClientEntryGet and perfLimitsClientArbitrateAndApply
            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libPerfLimitClient)
             // Required for clkDomainsGetIdxByApiDomain
            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfVf)
            OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, bifPcieLimitClient)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        PERF_LIMITS_CLIENT_SEMAPHORE_TAKE(pClient);
        {
            PERF_LIMITS_CLIENT_ENTRY *pEntry = perfLimitsClientEntryGet(pClient, perfLimitId);
            if (pEntry == NULL)
            {
                goto bifSetPmuPerfLimits_exit;
            }

            // Handle the client request
            if (bClearLimit)
            {
                // Clear perf limit if requested
                PERF_LIMIT_CLIENT_CLEAR_LIMIT(pEntry, perfLimitId);
            }
            else
            {
                // Find the clock domain index for PCIE.
                LwU32 pcieClkDomainIdx;

                PMU_HALT_OK_OR_GOTO(status,
                    clkDomainsGetIndexByApiDomain(LW2080_CTRL_CLK_DOMAIN_PCIEGENCLK, &pcieClkDomainIdx),
                    bifSetPmuPerfLimits_exit);

                // Set the requested perf limit
                PERF_LIMITS_CLIENT_FREQ_LIMIT(pEntry, perfLimitId, pcieClkDomainIdx, lockSpeed);
            }

            // Fire off a synchronous arbitration request to PMU arbiter
            pClient->hdr.applyFlags = LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_NONE;
            status = perfLimitsClientArbitrateAndApply(pClient);

bifSetPmuPerfLimits_exit:
            lwosNOP();
        }
        PERF_LIMITS_CLIENT_SEMAPHORE_GIVE(pClient);
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @brief BIF Handler for each Perf change step
 */
FLCN_STATUS
bifPerfChangeStep(LwU32 pstateIdx, LwU8 pcieIdx)
{
    LwU32 pciExpressCaps;
    FLCN_STATUS status = FLCN_OK;
    RM_PMU_BIF_LINK_SPEED pexLinkSpeedTarget = RM_PMU_BIF_LINK_SPEED_GEN1PCIE;
    PSTATE   *pPstate = PSTATE_GET(pstateIdx);

    Bif.pstateNum = pPstate->pstateNum;
    Bif.lwrrPstatePcieIdx = pcieIdx;

    //
    // If ASPM not supported for a particular board,
    // it's expected that pcieIdx would be OxFF
    //
    if (pcieIdx == 0xFF)
    {
        goto bifPerfChangeStep_exit;
    }

    if (pcieIdx >= RM_PMU_BIF_PCIE_ENTRY_COUNT_MAX)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto bifPerfChangeStep_exit;
    }

    pciExpressCaps = Bif.vbiosData.pcieGpuData.entry[pcieIdx].pcieExpressCaps;

    PMU_ASSERT_OK_OR_GOTO(status,
        perfGetLinkSpeed(pstateIdx, &pexLinkSpeedTarget),
        bifPerfChangeStep_exit);

    if (Bif.vbiosData.pcieGpuData.entry[pcieIdx].bPcieEnabled)
    {
        // Update ASPM L0s/L1 GPU Mask
        bifEnableL0sMask_HAL(&Bif, FLD_TEST_DRF(_RM_PMU, _BIF_PCIECAPS, _GPU_L0S_MASK, _ENABLED, pciExpressCaps));
        bifEnableL1Mask_HAL(&Bif,
                            FLD_TEST_DRF(_RM_PMU, _BIF_PCIECAPS, _GPU_L1_MASK, _ENABLED, pciExpressCaps),
                            pexLinkSpeedTarget);
    }

    // PCIE L1 Entry Threshold may be varied for perf/power balance
    if (FLD_TEST_DRF(_RM_PMU, _BIF_PCIECAPS, _OVERRIDE_L1_THRESHOLD, _ENABLED, pciExpressCaps))
    {
        bifWriteL1Threshold_HAL(&Bif,
                                Bif.vbiosData.pcieGpuData.entry[pcieIdx].l1Threshold,
                                Bif.vbiosData.pcieGpuData.entry[pcieIdx].l1ThresholdFrac,
                                pexLinkSpeedTarget);
    }

    // Update PCIE L0s Threshold value if override is enabled
    if (RM_PMU_BIF_PCIE_ENTRY_DATA_GET(&Bif, pcieIdx, bL0sThresholdOverride))
    {
        bifWriteL0sThreshold_HAL(&Bif, RM_PMU_BIF_PCIE_ENTRY_DATA_GET(&Bif, pcieIdx, l0sThreshold));
    }

    //
    // Enable L1 substates. The call below checks if current pstate
    // supports L1.1 and L1.2
    //
    bifL1SubstatesEnable_HAL(&Bif, LW_TRUE, pstateIdx, pcieIdx, LW_TRUE);

    // L1 PLL-Power Down enable/disable as per current P-state
    bifEnableL1PllPowerDown_HAL(&Bif,
        FLD_TEST_DRF(_RM_PMU, _BIF_PCIECAPS, _L1_PLL_PD, _ALLOWED, pciExpressCaps));

    // L1 Clock Power Mgmt enable/disable as per current P-state
    bifEnableL1ClockPwrMgmt_HAL(&Bif,
        FLD_TEST_DRF(_RM_PMU, _BIF_PCIECAPS, _L1_CPM, _ALLOWED, pciExpressCaps));

    // Update LTR latency
    if (Bif.vbiosData.pcieGpuData.entry[pcieIdx].bLtrEnabled &&
        FLD_TEST_DRF(_RM_PMU, _BIF_CAPS, _LTR_REGKEY_OVERRIDE, _DISABLED, pciExpressCaps))
    {
        bifUpdateLtrValue_HAL(&Bif,
                              Bif.vbiosData.pcieGpuData.entry[pcieIdx].ltrSnoopLatencyValue,
                              Bif.vbiosData.pcieGpuData.entry[pcieIdx].ltrSnoopLatencyScale,
                              Bif.vbiosData.pcieGpuData.entry[pcieIdx].ltrNoSnoopLatencyValue,
                              Bif.vbiosData.pcieGpuData.entry[pcieIdx].ltrNoSnoopLatencyScale);
    }

    if ((Bif.bPexPowerSavingsEnabled == LW_TRUE) &&
        (Bif.pstateNum == PSTATE_NUM_P3)         &&
        (Bif.bPowerSrcDc == LW_TRUE))
    {
        if (initiateCallback < 1)
        {
            initiateCallback++;
            if (PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))
            {
                // Initiate callback
                bifEngageAspmAggressivelyInitiateCallback_HAL(&Bif, LW_FALSE);
            }
        }
    }

bifPerfChangeStep_exit:
    return status;
}

//
// Returns whether or not L1.1 is allowed in the current P-state
//
LwBool bifIsL11Allowed(LwU8 pcieIdx, LwBool bIsPerfChangeInprogress)
{
    if (bIsPerfChangeInprogress)
    {
        return LW_FALSE;
    }

    return RM_PMU_BIF_PCIE_ENTRY_DATA_GET(&Bif, pcieIdx, bL11Enabled);
}

//
// Returns whether or not L1.2 is allowed in the current P-state
//
LwBool bifIsL12Allowed(LwU8 pcieIdx, LwBool bIsPerfChangeInprogress)
{
    if (bIsPerfChangeInprogress)
    {
        return LW_FALSE;
    }

    return RM_PMU_BIF_PCIE_ENTRY_DATA_GET(&Bif, pcieIdx, bL12Enabled);
}

//
// Returns the mask of disabled lwlinks
//
LwU32 bifGetDisabledLwlinkMask(void)
{
    return Bif.lwlinkDisableMask;
}

/*!
 * @brief   Exelwtes generic BIF_CFG_INIT RPC.
 *
 * @param[in/out]   pParams     Pointer to RM_PMU_RPC_STRUCT_BIF_CFG_INIT
 */
FLCN_STATUS
pmuRpcBifCfgInit
(
    RM_PMU_RPC_STRUCT_BIF_CFG_INIT *pParams
)
{
    FLCN_STATUS status             = FLCN_OK;
    Bif.bifCaps                    = pParams->bifCaps;
    Bif.bRtlSim                    = pParams->bRtlSim;
    Bif.l1ssRegs                   = pParams->l1ssRegs;
    Bif.platformParams             = pParams->platformParams;
    Bif.vbiosData.pcieGpuData      = pParams->pcieGpuData;
    Bif.vbiosData.pciePlatformData = pParams->pciePlatformData;
    Bif.bUsbcDebugMode             = pParams->bUsbcDebugMode;
    Bif.bMobile                    = pParams->bMobile;
    Bif.bMarginingRegkeyEnabled    = pParams->bMarginingRegkeyEnabled;
    Bif.bDisableHigherGenSpeedsDuringGenSpeedSwitch = pParams->bDisableHigherGenSpeedsDuringGenSpeedSwitch;
    Bif.linkReadyTimeoutNs         = BIF_COLWERT_MS_TO_NS(pParams->linkReadyTimeoutMs);
    Bif.linkReadyTimeoutNs         = LW_MIN(Bif.linkReadyTimeoutNs, LW_RM_PMU_BIF_LINK_READY_TIMEOUT_MAX_SUPPORTED_NS);
    Bif.bPexPowerSavingsEnabled    = pParams->bPexPowerSavingsEnabled;
    Bif.lowerLimitDiffPercentage   = pParams->lowerLimitDiffPercentage;
    Bif.upperLimitDiffPercentage   = pParams->upperLimitDiffPercentage;
    Bif.lowerLimitNewL1Residency   = pParams->lowerLimitNewL1Residency;
    Bif.upperLimitNewL1Residency   = pParams->upperLimitNewL1Residency;

    if (PMUCFG_FEATURE_ENABLED(PMU_BIF_ASPM_SPLIT_CONTROL))
    {
        Bif.bEnableAspmAtLoad      = pParams->bEnableAspmAtLoad;
    }

    if ((Bif.lowerLimitDiffPercentage == 0) || (Bif.upperLimitDiffPercentage == 0) ||
        (Bif.lowerLimitNewL1Residency == 0) || (Bif.upperLimitNewL1Residency == 0))
    {
        Bif.bPexPowerSavingsEnabled = LW_FALSE;
    }

    if (Bif.linkReadyTimeoutNs == 0)
    {
        Bif.linkReadyTimeoutNs = LW_RM_PMU_BIF_LTSSM_LINK_READY_TIMEOUT_NS;
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_SELWRITY_HARDENING))
    {
        status = bifCheckL1Threshold_HAL(&Bif);
        if (status != FLCN_OK)
        {
            goto pmuRpcBifCfgInit_exit;
        }
    }

    // Next CL will initialize this based on input parameter
    Bif.bMarginingEnabledFromSW    = LW_TRUE;

    // Initialize to no links configured.
    Bif.lwlinkDisableMask     = 0;
    Bif.lwlinkLpwrEnableMask  = 0;
    Bif.lwlinkLpwrSetMask     = 0;
    Bif.lwlinkVbiosIdx        = BIF_LWLINK_VBIOS_INDEX_UNSET;
    Bif.bLwlinkSuspend        = LW_FALSE;
    Bif.bLwlinkGlobalLpEnable = LW_FALSE;

    Bif.bIsochReplyReceived = LW_FALSE;
    Bif.bInitialized        = LW_TRUE;
    Bif.bLoaded             = LW_FALSE;

    status = bifUpdateTlcBaseOffsets_HAL(&Bif);

    if (PMUCFG_FEATURE_ENABLED(PMU_BIF_LANE_MARGINING))
    {
        bifInitMarginingIntr_HAL(&Bif);
    }

pmuRpcBifCfgInit_exit:
    return status;
}

FLCN_STATUS
pmuRpcBifPmuDeinit
(
    RM_PMU_RPC_STRUCT_BIF_PMU_DEINIT *pParams
)
{
    if (Bif.bPexPowerSavingsEnabled)
    {
        if (PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))
        {
            bifEngageAspmAggressivelyInitiateCallback_HAL(&Bif, pParams->bCancelCallback);
        }
    }

    return FLCN_OK;
}

/*!
 * @brief   Exelwtes generic BIF_LOAD RPC.
 *
 * @param[in/out]   pParams     Pointer to RM_PMU_RPC_STRUCT_BIF_LOAD
 */
FLCN_STATUS
pmuRpcBifLoad
(
    RM_PMU_RPC_STRUCT_BIF_LOAD *pParams
)
{
    FLCN_STATUS status = FLCN_OK;

    if (!Bif.bInitialized)
    {
        status = FLCN_ERR_ILWALID_STATE;
        goto _pmuRpcBifLoad_exit;
    }

    Bif.bLoaded = pParams->bLoad;

    if (!(Bif.bLoaded))
    {
        //
        // It's still a valid case when Bif.bLoaded is false say when PERF module
        // is not loaded
        //
        goto _pmuRpcBifLoad_exit;
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_BIF_ASPM_SPLIT_CONTROL))
    {
        // Setup HW for enabling L1-PLL PD, L1-CPM
        bifL1LpwrFeatureSetup_HAL(&Bif);

        // Check if we want to enable L1 at GPU load
        if (Bif.bEnableAspmAtLoad)
        {
            //
            // Try to enable L1 irrespective of what pcie-platform table says for
            // current gen speed. bifEnableL1Mask will not enable L1 in case of
            // other restrictions like chipset, regkey etc.
            //
            bifEnableL1Mask_HAL(&Bif, LW_FALSE, RM_PMU_BIF_LINK_SPEED_ILWALID);
        }
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_BIF_XUSB))
    {
        // Query the ISOCH state on XUSB, register and schedule timeout callback
        status = bifQueryXusbIsochActivity_HAL(&Bif);
        if (!OS_TMR_CALLBACK_WAS_CREATED(&OsCBBifXusbIsochTimeout))
        {
            osTmrCallbackCreate(&OsCBBifXusbIsochTimeout,    // pCallback
                OS_TMR_CALLBACK_TYPE_ONE_SHOT,               // type
                OVL_INDEX_ILWALID,                           // ovlImem
                s_bifXusbIsochTimeoutCallbackExelwte,        // pTmrCallbackFunc
                LWOS_QUEUE(PMU, BIF),                        // queueHandle
                BIF_XUSB_ISOCH_QUERY_CALLBACK_PERIOD_US,     // periodNormalus
                BIF_XUSB_ISOCH_QUERY_CALLBACK_PERIOD_US,     // periodSleepus
                OS_TIMER_RELAXED_MODE_USE_NORMAL,            // bRelaxedUseSleep
                RM_PMU_TASK_ID_BIF);                         // taskId
        }
        else
        {
            // Update callback period
            osTmrCallbackUpdate(&OsCBBifXusbIsochTimeout,
                                BIF_XUSB_ISOCH_QUERY_CALLBACK_PERIOD_US,
                                BIF_XUSB_ISOCH_QUERY_CALLBACK_PERIOD_US,
                                OS_TIMER_RELAXED_MODE_USE_NORMAL,
                                OS_TIMER_UPDATE_USE_BASE_LWRRENT);
        }
        osTmrCallbackSchedule(&OsCBBifXusbIsochTimeout);
        if (status != FLCN_OK)
        {
            //
            // Ilwestigation in the @ref lwbugs/3162574 clarified that:
            //
            //      ISOCH query is either not supported or has failed as
            // _PMU2XUSB_ACK line status is not _LOW. We must unlock the PCIE
            // gen speed that is set to max at BIF load by the RM if the PMU
            // does not receive the expected response from XUSB.
            //      Possible side effect might be on the side of ISOCH traffic
            // where the user might expect a slight glitch at boot. It is not
            // a valid end user usecase as no user should expect ISOCH traffic
            // right at boot.
            //
            status = FLCN_OK;
        }
    }

_pmuRpcBifLoad_exit:
    return status;
}

/*!
 * @brief   Exelwtes generic BIF_BUS_SPEED_CHANGE RPC.
 *
 * @param[in/out]   pParams     Pointer to RM_PMU_RPC_STRUCT_BIF_BUS_SPEED_CHANGE
 */
FLCN_STATUS
pmuRpcBifBusSpeedChange
(
    RM_PMU_RPC_STRUCT_BIF_BUS_SPEED_CHANGE *pParams
)
{
    FLCN_STATUS status = FLCN_OK;

    if (!Bif.bInitialized)
    {
        return FLCN_ERR_ILWALID_STATE;
    }

    // Attach and load overlay.
    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(libBif));

    status = bifChangeBusSpeed_HAL(&Bif, pParams->speed, &(pParams->speed),
                                   &(pParams->errorId));

    // Release overlay.
    OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(libBif));

    return status;
}

/*!
 * @brief   Exelwtes generic BIF_POWER_SOURCE_EVENT_CHANGE_NOTIFY RPC.
 *
 * @param[in] pParams Pointer to RM_PMU_RPC_STRUCT_BIF_POWER_SOURCE_EVENT_CHANGE_NOTIFY
 */
FLCN_STATUS
pmuRpcBifPowerSourceEventChangeNotify
(
    RM_PMU_RPC_STRUCT_BIF_POWER_SOURCE_EVENT_CHANGE_NOTIFY *pParams
)
{
    //
    // Todo by anaikwade: This is a temporary fix to resolve
    // regression (Bug 3462550).  To be fixed as a part of Bug 3416431.
    // Adding it here to remind that we need to have this check
    // in much cleaner way here as well.
    //
    if (Bif.bPexPowerSavingsEnabled == LW_FALSE)
    {
        return FLCN_OK;
    }

    Bif.bPowerSrcDc = pParams->bPowerSrcDc;
    LwU8 pcieIdx    = Bif.lwrrPstatePcieIdx;

    if (Bif.bPowerSrcDc == LW_FALSE)
    {
        bifUpdateL1Threshold_HAL(&Bif,
            Bif.vbiosData.pcieGpuData.entry[pcieIdx].l1Threshold,
            Bif.vbiosData.pcieGpuData.entry[pcieIdx].l1ThresholdFrac);
    }

    return FLCN_OK;
}

/*!
 * @brief   Exelwtes generic BIF_ERROR_COUNT_GET RPC.
 *
 * @param[in/out]   pParams     Pointer to RM_PMU_RPC_STRUCT_BIF_ERROR_COUNT_GET
 */
FLCN_STATUS
pmuRpcBifErrorCountGet
(
    RM_PMU_RPC_STRUCT_BIF_ERROR_COUNT_GET *pParams
)
{
    if (!Bif.bInitialized)
    {
        return FLCN_ERR_ILWALID_STATE;
    }

    pParams->linkSpeedSwitchingErrorCount = Bif.linkSpeedSwitchingErrorCount;

    return FLCN_OK;
}

/*!
 * @brief   Exelwtes generic BIF_L1_MASK_STATUS_FOR_UPSTREAM RPC.
 *
 * @param[in]   pParams     Pointer to RM_PMU_RPC_STRUCT_BIF_L1_MASK_STATUS_FOR_UPSTREAM
 */
FLCN_STATUS
pmuRpcBifL1MaskStatusForUpstream
(
    RM_PMU_RPC_STRUCT_BIF_L1_MASK_STATUS_FOR_UPSTREAM *pParams
)
{
    //
    // Warning: Tightly coupled to client in the interest of saving RPC communication time
    // Note that this RPC call is made only when chipset side L1 mask is disabled
    // Therefore, if this call is made, value of BIF's L1 mask is entirely dependent
    // on l1 mask for upstream port because-
    // Bif's overall L1 mask = (l1MaskForChipset || l1MaskForUpstreamPort)
    //
    if (pParams->bL1MaskEnabled)
    {
        Bif.bifCaps = FLD_SET_DRF(_RM_PMU, _BIF_CAPS, _L1_MASK, _ENABLED, Bif.bifCaps);
    }
    else
    {
        Bif.bifCaps = FLD_SET_DRF(_RM_PMU, _BIF_CAPS, _L1_MASK, _DISABLED, Bif.bifCaps);
    }
    return FLCN_OK;
}

/*!
 * @brief   Exelwtes generic BIF_LWLINK_LINK_DISABLE RPC.
 *
 * @param[in/out]   pParams     Pointer to RM_PMU_RPC_STRUCT_BIF_LWLINK_LINK_DISABLE
 */
FLCN_STATUS
pmuRpcBifLwlinkLinkDisable
(
    RM_PMU_RPC_STRUCT_BIF_LWLINK_LINK_DISABLE *pParams
)
{
    if (!Bif.bInitialized)
    {
        return FLCN_ERR_ILWALID_STATE;
    }

    Bif.lwlinkDisableMask = pParams->disabledLinks;

    return FLCN_OK;
}

/*!
 * @brief   Exelwtes generic BIF_LWLINK_LPWR_LINK_ENABLE RPC.
 *
 * @param[in/out]   pParams     Pointer to RM_PMU_RPC_STRUCT_BIF_LWLINK_LPWR_LINK_ENABLE
 */
FLCN_STATUS
pmuRpcBifLwlinkLpwrLinkEnable
(
    RM_PMU_RPC_STRUCT_BIF_LWLINK_LPWR_LINK_ENABLE *pParams
)
{
    if (!Bif.bInitialized)
    {
        return FLCN_ERR_ILWALID_STATE;
    }

    return bifLwlinkLpwrLinkEnable_HAL(&Bif, pParams->changeMask, pParams->newLinks);
}

/*!
 * @brief   Exelwtes generic BIF_LWLINK_TLC_LPWR_SET RPC.
 *
 * @param[in/out]   pParams     Pointer to RM_PMU_RPC_STRUCT_BIF_LWLINK_TLC_LPWR_SET
 */
FLCN_STATUS
pmuRpcBifLwlinkTlcLpwrSet
(
    RM_PMU_RPC_STRUCT_BIF_LWLINK_TLC_LPWR_SET *pParams
)
{
    if (!Bif.bInitialized)
    {
        return FLCN_ERR_ILWALID_STATE;
    }

    return bifLwlinkTlcLpwrSet_HAL(&Bif, pParams->vbiosIdx);
}

/*!
 * @brief   Exelwtes generic BIF_LWLINK_TLC_LPWR_LINK_INIT RPC.
 *
 * @param[in/out]   pParams     Pointer to RM_PMU_RPC_STRUCT_BIF_LWLINK_TLC_LPWR_LINK_INIT
 */
FLCN_STATUS
pmuRpcBifLwlinkTlcLpwrLinkInit
(
    RM_PMU_RPC_STRUCT_BIF_LWLINK_TLC_LPWR_LINK_INIT *pParams
)
{
    if (!Bif.bInitialized)
    {
        return FLCN_ERR_ILWALID_STATE;
    }

    return bifLwlinkTlcLpwrLinkInit_HAL(&Bif, pParams->linkIdx);
}

/*!
 * @brief   Exelwtes generic BIF_LWLINK_PWR_MGMT_TOGGLE RPC.
 *
 * @param[in/out]   pParams     Pointer to RM_PMU_RPC_STRUCT_BIF_LWLINK_PWR_MGMT_TOGGLE
 */
FLCN_STATUS
pmuRpcBifLwlinkPwrMgmtToggle
(
    RM_PMU_RPC_STRUCT_BIF_LWLINK_PWR_MGMT_TOGGLE *pParams
)
{
    if (!Bif.bInitialized)
    {
        return FLCN_ERR_ILWALID_STATE;
    }

    return bifLwlinkPwrMgmtToggle_HAL(&Bif, pParams->linkIdx, pParams->bEnable);
}

/*!
 * @brief   Exelwtes generic BIF_LWLINK_LPWR_SUSPEND RPC.
 *
 * @param[in/out]   pParams pointer to RM_PMU_RPC_STRUCT_BIF_LWLINK_LPWR_SUSPEND
 */
FLCN_STATUS
pmuRpcBifLwlinkLpwrSuspend
(
    RM_PMU_RPC_STRUCT_BIF_LWLINK_LPWR_SUSPEND *pParams
)
{
    if (!Bif.bInitialized)
    {
        return FLCN_ERR_ILWALID_STATE;
    }

    return bifLwlinkLpwrSuspend_HAL(&Bif, pParams->bSuspend);
}

/*!
 * @brief   Exelwtes generic BIF_LWLINK_HSHUB_UPDATE RPC.
 *
 * @param[in/out]   pParams     Pointer to RM_PMU_RPC_STRUCT_BIF_LWLINK_HSHUB_UPDATE
 */
FLCN_STATUS
pmuRpcBifLwlinkHshubUpdate
(
    RM_PMU_RPC_STRUCT_BIF_LWLINK_HSHUB_UPDATE *pParams
)
{
    return bifLwlinkHshubUpdate_HAL(&Bif,
                                    pParams->hshubConfig0,
                                    pParams->hshubConfig1,
                                    pParams->hshubConfig2,
                                    pParams->hshubConfig6);
}

/*!
 * @brief   Exelwtes generic BIF_SET_EOM_PARAMETERS RPC.
 *
 * @param[in/out] pParams  Pointer to RM_PMU_RPC_STRUCT_BIF_SET_EOM_PARAMETERS
 */
FLCN_STATUS
pmuRpcBifSetEomParameters
(
    RM_PMU_RPC_STRUCT_BIF_SET_EOM_PARAMETERS *pParams
)
{
    //
    // As confirmed by MODS team on Bug 3255516, comment#9
    // *BIF_SET_EOM_PARAMETERS is valid only for pre-Hopper
    //
    // Also, eomBerEyeSel and eomPamEyeSel are redundant pre-Hopper
    //
    return bifSetEomParameters_HAL(&Bif,
                                   pParams->eomMode,
                                   pParams->eomNblks,
                                   pParams->eomNerrs,
                                   0, 0);
}

/*!
 * @brief Runs the EOM sequence and returns EOM values in rpc buffer
 *
 * @param[in/out] pParams  Pointer to RM_PMU_RPC_STRUCT_BIF_GET_EOM_STATUS
 */
FLCN_STATUS
pmuRpcBifGetEomStatus
(
    RM_PMU_RPC_STRUCT_BIF_GET_EOM_STATUS *pParams
)
{
    return bifGetEomStatus_HAL(&Bif, pParams->eomMode, pParams->eomNblks,
               pParams->eomNerrs, pParams->eomBerEyeSel, pParams->eomPamEyeSel,
               pParams->laneMask, pParams->eomStatus);
}

/*!
 * @brief   Exelwtes generic BIF_GET_UPHY_DLN_CFG_SPACE RPC.
 *
 * @param[in/out]   pParams pointer to
 *                  RM_PMU_RPC_STRUCT_BIF_GET_UPHY_DLN_CFG_SPACE
 */
FLCN_STATUS
pmuRpcBifGetUphyDlnCfgSpace
(
    RM_PMU_RPC_STRUCT_BIF_GET_UPHY_DLN_CFG_SPACE *pParams
)
{
    FLCN_STATUS status;
    LwU16 regVal = 0;

    status = bifGetUphyDlnCfgSpace_HAL(&Bif, pParams->regAddress,
                                       pParams->laneSelectMask, &regVal);
    pParams->regValue = regVal;

    return status;
}

/*!
 * @brief   Exelwtes generic BIF_SERVICE_MULTIFUNCTION_STATE RPC.
 *
 * @param[in/out]   pParams pointer to
 *                  RM_PMU_RPC_STRUCT_BIF_SERVICE_MULTIFUNCTION_STATE
 */
FLCN_STATUS
pmuRpcBifServiceMultifunctionState
(
    RM_PMU_RPC_STRUCT_BIF_SERVICE_MULTIFUNCTION_STATE *pParams
)
{
    FLCN_STATUS status;

    status = bifServiceMultifunctionState_HAL(&Bif, pParams->cmd);

    return status;
}

/*!
 * @brief   Enables or disable poison error data reporting
 *
 * @param[in]   pParams @ref RM_PMU_RPC_STRUCT_BIF_PCIE_POISON_CONTROL_ERR_DATA_CONTROL
 *
 * @return  @ref FLCN_OK    Always
 */
FLCN_STATUS
pmuRpcBifPciePoisonControlErrDataControl
(
    RM_PMU_RPC_STRUCT_BIF_PCIE_POISON_CONTROL_ERR_DATA_CONTROL *pParams
)
{
    bifPciePoisonControlErrDataControl_HAL(&Bif, pParams->bReportEnable);
    return FLCN_OK;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief           Get highest link speed supported on a pstate
 *                  considering all constraints.
 *
 * @param[in]       pstateIdx   Pstate index
 * @param[out]      pLinkSpeed  Highest link speed supported
 *
 * @return FLCN_OK
 *      The requested operation completed successfully.
 */
static FLCN_STATUS
perfGetLinkSpeed
(
    LwU32                 pstateIdx,
    RM_PMU_BIF_LINK_SPEED *pLinkSpeed
)
{
    LwU32     pcieClkDomainIdx;
    PSTATE   *pPstate = PSTATE_GET(pstateIdx);
    FLCN_STATUS status = FLCN_OK;
    LW2080_CTRL_PERF_PSTATE_CLOCK_ENTRY clkFreqInfo;

    // Sanity check that the PSTATE index is valid.
    if (pPstate == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto perfGetLinkSpeed_exit;
    }

    // Initialize with minimum supported link speed
    *pLinkSpeed = RM_PMU_BIF_LINK_SPEED_GEN1PCIE;

    // Find the clock domain index for PCIE.
    PMU_ASSERT_OK_OR_GOTO(status,
        clkDomainsGetIndexByApiDomain(LW2080_CTRL_CLK_DOMAIN_PCIEGENCLK, &pcieClkDomainIdx),
        perfGetLinkSpeed_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        perfPstateClkFreqGet(pPstate, pcieClkDomainIdx, &clkFreqInfo),
        perfGetLinkSpeed_exit);

    //
    // Following if-then blocks basically boils down to -
    // *pLinkSpeed = min(Bif.bifCaps, clkFreqInfo.max.freqkHz)
    //
    if (FLD_TEST_DRF(_RM_PMU, _BIF_CAPS, _GEN5PCIE, _SUPPORTED, Bif.bifCaps))
    {
        *pLinkSpeed = clkFreqInfo.max.freqkHz;
    }
    else if (FLD_TEST_DRF(_RM_PMU, _BIF_CAPS, _GEN4PCIE, _SUPPORTED, Bif.bifCaps))
    {
        if (clkFreqInfo.max.freqkHz != RM_PMU_BIF_LINK_SPEED_GEN5PCIE)
        {
            *pLinkSpeed = clkFreqInfo.max.freqkHz;
        }
        else
        {
            *pLinkSpeed = RM_PMU_BIF_LINK_SPEED_GEN4PCIE;
        }
    }
    else if (FLD_TEST_DRF(_RM_PMU, _BIF_CAPS, _GEN3PCIE, _SUPPORTED, Bif.bifCaps))
    {
        if ((clkFreqInfo.max.freqkHz != RM_PMU_BIF_LINK_SPEED_GEN5PCIE) &&
            (clkFreqInfo.max.freqkHz != RM_PMU_BIF_LINK_SPEED_GEN4PCIE))
        {
            *pLinkSpeed = clkFreqInfo.max.freqkHz;
        }
        else
        {
            *pLinkSpeed = RM_PMU_BIF_LINK_SPEED_GEN3PCIE;
        }
    }
    else if (FLD_TEST_DRF(_RM_PMU, _BIF_CAPS, _GEN2PCIE, _SUPPORTED, Bif.bifCaps))
    {
        if ((clkFreqInfo.max.freqkHz != RM_PMU_BIF_LINK_SPEED_GEN5PCIE) &&
            (clkFreqInfo.max.freqkHz != RM_PMU_BIF_LINK_SPEED_GEN4PCIE) &&
            (clkFreqInfo.max.freqkHz != RM_PMU_BIF_LINK_SPEED_GEN3PCIE))
        {
            *pLinkSpeed = clkFreqInfo.max.freqkHz;
        }
        else
        {
            *pLinkSpeed = RM_PMU_BIF_LINK_SPEED_GEN2PCIE;
        }
    }
    else
    {
        *pLinkSpeed = RM_PMU_BIF_LINK_SPEED_GEN1PCIE;
    }

perfGetLinkSpeed_exit:
    return status;
}

/*!
 * @ref      OsTmrCallbackFunc
 *
 * @brief    Exelwtes bif timer callback function.
 *
 * Scheduled from @ref pmuRpcBifLoad().
 */
static FLCN_STATUS
s_bifXusbIsochTimeoutCallbackExelwte
(
    OS_TMR_CALLBACK *pCallback
)
{
    if (!Bif.bIsochReplyReceived)
    {
        //
        // Send a notifier event to RM indicating that PMU has 'not' received
        // response to ISOCH query in specified time. This can happen if -
        //   a. XUSB function is not up yet
        //   b. XUSB function is in a bad state or in reset
        // RM uses event to clear RM pcie genspeed PERF_LIMIT preset during RM bifStateLoad
        //
        PMU_RM_RPC_STRUCT_BIF_XUSB_ISOCH rpc;
        FLCN_STATUS                      status;

        // RC-TODO, need to propery handle status.
        PMU_RM_RPC_EXELWTE_BLOCKING(status, BIF, XUSB_ISOCH, &rpc);

        // Also clear the timed out ISOCH query posted in the PMU2XUSB message box
        bifClearPmuToXusbMsgbox_HAL(&Bif);
    }

    return FLCN_OK; // NJ-TODO-CB
}
