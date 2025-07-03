/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   lpwr_loadin_rpc.c
 * @brief  Manage RPCs for LowPower Loadin unit
 */

/* ------------------------- System Includes ------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes -------------------------- */
#include "pmu_objhal.h"
#include "dbgprintf.h"
#include "pmu_objlpwr.h"
#include "pmu_objpg.h"
#include "pmu_objgr.h"
#include "pmu_objms.h"
#include "pmu_objdi.h"
#include "pmu_objap.h"
#include "pmu_objei.h"
#include "pmu_gc6.h"
#include "dmemovl.h"
#include "g_pmuifrpc.h"

// Wrapper for the generated LPWR_LOADIN RPC source file.
#include "rpcgen/g_pmurpc_lpwr_loadin.c"

#define LW_GC6_RTD3_PWM_VALUE      (0)
/* ------------------------- Prototypes -------------------------------------*/
/* ------------------------- Private Functions ----------------------------- */

/*!
 * @brief Exelwtes PRE_INIT RPC in PMU
 *
 * @param[in]   pParams     Pointer to RM_PMU_RPC_STRUCT_LPWR_LOADIN_PRE_INIT
 *
 * @return      FLCN_OK     On success
 */
static FLCN_STATUS
s_pmuRpcLpwrLoadinPreInit
(
    RM_PMU_RPC_STRUCT_LPWR_LOADIN_PRE_INIT *pParams
)
{
    LPWR_GRP *pLpwrGrp = LPWR_GET_GRP();
    LwU32     idx      = 0;

    // Initialize LPWR VBIOS data with default values
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_WITH_PS35))
    {
        lpwrVbiosDataInit();

        Pg.bNoPstateVbios = pParams->bNoPstateVbios;
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_CENTRALISED_CALLBACK))
    {
        Pg.pCallback->basePeriodUs = pParams->basePeriodMs * 1000;
    }

    Pg.archSfSupportMask = pParams->archSfSupportMask;

    // Initialize LPWR group info
    for (idx = 0; idx < RM_PMU_LPWR_GRP_CTRL_ID__COUNT; idx++)
    {
        pLpwrGrp->grpCtrl[idx].lpwrCtrlMask = pParams->grpCtrlMask[idx];
    }

    return FLCN_OK;
}

/*!
 * @brief Exelwtes POST_INIT RPC in PMU
 *
 * @param[in]   pParams     Pointer to RM_PMU_RPC_STRUCT_LPWR_LOADIN_POST_INIT
 *
 * @return      FLCN_OK     On success
 */
static FLCN_STATUS
s_pmuRpcLpwrLoadinPostInit
(
    RM_PMU_RPC_STRUCT_LPWR_LOADIN_POST_INIT *pParams
)
{
    LwU32       railIdx;
    FLCN_STATUS status;

    status = lpwrCgPostInitRpcHandler(pParams->bIstCgSupport);
    if (status != FLCN_OK)
    {
        goto s_pmuRpcLpwrLoadinPostInit_exit;
    }

    // Initialize voltage info
    for (railIdx = 0; railIdx < PG_VOLT_RAIL_IDX_MAX; railIdx++)
    {
        Pg.voltRail[railIdx].voltRailIdx     = pParams->voltRail[railIdx].voltRailIdx;
        Pg.voltRail[railIdx].sleepVfeIdx     = pParams->voltRail[railIdx].sleepVfeIdx;
        Pg.voltRail[railIdx].sleepVoltDevIdx = pParams->voltRail[railIdx].sleepVoltDevIdx;
    }

    //
    // TODO-pankumar: We need to work and find some appropriate place
    // to do the LPWR Group initialization
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_GRP_GR))
    {
        grLpwrCtrlPostInit();
    }

    // If MS Grp is enabled, init the MS Group related initialization
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_GRP_MS) &&
        lpwrGrpIsCtrlSupported(RM_PMU_LPWR_GRP_CTRL_ID_MS))
    {
        msGrpPostInit();
    }

    //
    // If EI is supported, then Enable the EI Notification
    // for LPWR Client
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_CTRL_EI) &&
        pgIsCtrlSupported(RM_PMU_LPWR_CTRL_ID_EI))
    {
        status = lpwrRequestEiNotificationEnable(RM_PMU_LPWR_EI_CLIENT_ID_PMU_LPWR,
                                                 LW_TRUE);
    }

    // If FG-RPPG is supported, initialize and enable it
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_FG_RPPG) &&
        pParams->bFgRppgSupported)
    {
        lpwrSeqSramFgRppgInit_HAL(&Lpwr);
    }

s_pmuRpcLpwrLoadinPostInit_exit:
    return status;
}

/*!
 * @brief Exelwtes VBIOS_IDX_INIT RPC in PMU
 *
 * @param[in]   pParams     Pointer to RM_PMU_RPC_STRUCT_LPWR_LOADIN_VBIOS_IDX_INIT
 *
 * @return      FLCN_OK     On success
 */
static FLCN_STATUS
s_pmuRpcLpwrLoadilwbiosIdxInit
(
    RM_PMU_RPC_STRUCT_LPWR_LOADIN_VBIOS_IDX_INIT *pParams
)
{
    Lpwr.pVbiosData->idx = pParams->idxData;

    return FLCN_OK;
}

/*!
 * @brief Exelwtes VBIOS_GR_INIT RPC in PMU
 *
 * @param[in]   pParams     Pointer to RM_PMU_RPC_STRUCT_LPWR_LOADIN_VBIOS_GR_INITGR_VBIOS_DATA_SEND
 *
 * @return      FLCN_OK     On success
 */
static FLCN_STATUS
s_pmuRpcLpwrLoadilwbiosGrInit
(
    RM_PMU_RPC_STRUCT_LPWR_LOADIN_VBIOS_GR_INIT *pParams
)
{
    FLCN_STATUS status = FLCN_OK;

    if (PMUCFG_FEATURE_ENABLED(PMU_SELWRITY_HARDENING))
    {
        // Sanity check the index values in grData
        if ((pParams->grData.adaptiveGrIndexLwrrent >= RM_PMU_LPWR_VBIOS_AP_ENTRY_COUNT_MAX) ||
            (pParams->grData.adaptiveGrIndexAc >= RM_PMU_LPWR_VBIOS_AP_ENTRY_COUNT_MAX) ||
            (pParams->grData.adaptiveGrIndexDc >= RM_PMU_LPWR_VBIOS_AP_ENTRY_COUNT_MAX))
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto s_pmuRpcLpwrLoadilwbiosGrInit_exit;
        }
    }

    Lpwr.pVbiosData->gr = pParams->grData;

s_pmuRpcLpwrLoadilwbiosGrInit_exit:
    return status;
}

/*!
 * @brief Exelwtes VBIOS_MS_INIT RPC in PMU
 *
 * @param[in]   pParams     Pointer to RM_PMU_RPC_STRUCT_LPWR_LOADIN_VBIOS_MS_INIT
 *
 * @return      FLCN_OK     On success
 */
static FLCN_STATUS
s_pmuRpcLpwrLoadilwbiosMsInit
(
    RM_PMU_RPC_STRUCT_LPWR_LOADIN_VBIOS_MS_INIT *pParams
)
{
    FLCN_STATUS status = FLCN_OK;

    if (PMUCFG_FEATURE_ENABLED(PMU_SELWRITY_HARDENING))
    {
        // Sanity check the index values in grData
        if ((pParams->msData.adaptiveMscgIndexLwrrent >= RM_PMU_LPWR_VBIOS_AP_ENTRY_COUNT_MAX) ||
            (pParams->msData.adaptiveMscgIndexAc >= RM_PMU_LPWR_VBIOS_AP_ENTRY_COUNT_MAX) ||
            (pParams->msData.adaptiveMscgIndexDc >= RM_PMU_LPWR_VBIOS_AP_ENTRY_COUNT_MAX))
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto s_pmuRpcLpwrLoadilwbiosMsInit_exit;
        }
    }

    Lpwr.pVbiosData->ms = pParams->msData;

s_pmuRpcLpwrLoadilwbiosMsInit_exit:
    return status;
}

/*!
 * @brief Exelwtes VBIOS_DI_INIT RPC in PMU
 *
 * @param[in]   pParams     Pointer to RM_PMU_RPC_STRUCT_LPWR_LOADIN_VBIOS_DI_INIT
 *
 * @return      FLCN_OK     On success
 */
static FLCN_STATUS
s_pmuRpcLpwrLoadilwbiosDiInit
(
    RM_PMU_RPC_STRUCT_LPWR_LOADIN_VBIOS_DI_INIT *pParams
)
{
    Lpwr.pVbiosData->di = pParams->diData;

    return FLCN_OK;
}

/*!
 * @brief Exelwtes VBIOS_AP_INIT RPC in PMU
 *
 * @param[in]   pParams     Pointer to RM_PMU_RPC_STRUCT_LPWR_LOADIN_VBIOS_AP_INIT
 *
 * @return      FLCN_OK     On success
 */
static FLCN_STATUS
s_pmuRpcLpwrLoadilwbiosApInit
(
    RM_PMU_RPC_STRUCT_LPWR_LOADIN_VBIOS_AP_INIT *pParams
)
{
    if (PMUCFG_FEATURE_ENABLED(PMU_AP))
    {
        Lpwr.pVbiosData->ap = pParams->apData;
    }

    return FLCN_OK;
}

/*!
 * @brief Exelwtes VBIOS_GR_RG_INIT RPC in PMU
 *
 * @param[in]   pParams     Pointer to RM_PMU_RPC_STRUCT_LPWR_LOADIN_VBIOS_GR_RG_INIT
 *
 * @return      FLCN_OK     On success
 */
static FLCN_STATUS
s_pmuRpcLpwrLoadilwbiosGrRgInit
(
    RM_PMU_RPC_STRUCT_LPWR_LOADIN_VBIOS_GR_RG_INIT *pParams
)
{
    Lpwr.pVbiosData->grRg = pParams->grRgData;

    return FLCN_OK;
}

/*!
 * @brief Exelwtes VBIOS_EI_INIT RPC in PMU
 *
 * @param[in]   pParams     Pointer to RM_PMU_RPC_STRUCT_LPWR_LOADIN_VBIOS_EI_INIT
 *
 * @return      FLCN_OK     On success
 */
static FLCN_STATUS
s_pmuRpcLpwrLoadilwbiosEiInit
(
    RM_PMU_RPC_STRUCT_LPWR_LOADIN_VBIOS_EI_INIT *pParams
)
{
    Lpwr.pVbiosData->ei = pParams->eiData;

    return FLCN_OK;
}

/*!
 * @brief Exelwtes VBIOS_DIFR_INIT RPC in PMU
 *
 * @param[in]   pParams     Pointer to RM_PMU_RPC_STRUCT_LPWR_LOADIN_VBIOS_DIFR_INIT
 *
 * @return      FLCN_OK     On success
 */
static FLCN_STATUS
s_pmuRpcLpwrLoadilwbiosDifrInit
(
    RM_PMU_RPC_STRUCT_LPWR_LOADIN_VBIOS_DIFR_INIT *pParams
)
{
   *(Lpwr.pVbiosData->pDifr) = pParams->difrData;

    return FLCN_OK;
}

/*!
 * @brief Exelwtes PGCTRL initialization RPC in PMU
 *
 * RPC initializes PgCtrl -
 * 1) SW initialization
 * 2) Call controller APIs for further initialization
 * 3) HW initialization
 *
 * @param[in]   pParams     Pointer to RM_PMU_RPC_STRUCT_LPWR_LOADIN_PGCTRL_INIT
 *
 * @return      FLCN_OK                     On success
 * @return      FLCN_ERR_ILWALID_STATE      Duplicate initialization request
 * @return      FLCN_ERR_NO_FREE_MEM        No free space in DMEM or in DMEM overlay
 * @return      FLCN_ERR_NOT_SUPPORTED      HW PG_ENG not supported
 */
static FLCN_STATUS
s_pmuRpcLpwrLoadinPgCtrlInit
(
    RM_PMU_RPC_STRUCT_LPWR_LOADIN_PG_CTRL_INIT *pParams
)
{
    FLCN_STATUS status   = FLCN_OK;
    LwU32       ctrlId   = pParams->ctrlId;
    OBJPGSTATE *pPgState;

    status = pgCtrlInit(pParams);
    if (status != FLCN_OK)
    {
        goto s_pmuRpcLpwrLoadinPgCtrlInit_exit;
    }

    pPgState                       = GET_OBJPGSTATE(ctrlId);
    pParams->hwEngIdx              = pPgState->hwEngIdx;
    pParams->supportMask           = pPgState->supportMask;
    pParams->bLwmulativeWakeupMask = pPgState->bLwmulativeWakeupMask;
    pParams->engHoldoffMask        = pPgState->holdoffMask;

    // Fill stats dmem offset details.
    status = lwosResVAddrToPhysMemOffsetMap(&pPgState->stats, &pParams->statsDmemOffset);
    if (status != FLCN_OK)
    {
        goto s_pmuRpcLpwrLoadinPgCtrlInit_exit;
    }

s_pmuRpcLpwrLoadinPgCtrlInit_exit:

    return status;
}

/*!
 * @brief Initiliazes Priv blockers
 *
 * @param[in]   pParams     Pointer to RM_PMU_RPC_STRUCT_LPWR_LOADIN_PRIV_BLOCKER_INIT
 *
 * @return      FLCN_OK                 On success
 * @return      FLCN_ERR_NOT_SUPPORTED  Priv blocker not supported
 */
static FLCN_STATUS
s_pmuRpcLpwrLoadinPrivBlockerInit
(
    RM_PMU_RPC_STRUCT_LPWR_LOADIN_PRIV_BLOCKER_INIT *pParams
)
{
    FLCN_STATUS status = FLCN_OK;

    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_UNIFIED_PRIV_BLOCKER_CTRL))
    {
        status = lpwrPrivBlockerInit(pParams->blockerSupportMask);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();

            goto s_pmuRpcLpwrLoadinPrivBlockerInit_exit;
        }

    }
    else if (PMUCFG_FEATURE_ENABLED(PMU_PG_SEC2))
    {
        // Priv Blocker Allowlist ranges for MS
        msProgramPrivBlockerAllowRanges_HAL(&Ms, pParams->blockerSupportMask);

        // Priv Blocker Disallow list ranges for MS
        msProgramPrivBlockerDisallowRanges_HAL(&Ms, pParams->blockerSupportMask);
    }

s_pmuRpcLpwrLoadinPrivBlockerInit_exit:
    return status;
}

/*!
 * @brief Construct and initialize OBJPSI
 *
 * @param[in]   pParams     Pointer to RM_PMU_RPC_STRUCT_LPWR_LOADIN_PSI_INIT
 *
 * @return      FLCN_OK     On success
 */
static FLCN_STATUS
s_pmuRpcLpwrLoadinPsiInit
(
    RM_PMU_RPC_STRUCT_LPWR_LOADIN_PSI_INIT *pParams
)
{
    if (PMUCFG_FEATURE_ENABLED(PMU_PSI_FLAVOUR_LWRRENT_AWARE))
    {
         Psi.lwrrPollPeriodNormalUs = pParams->lwrrPollPeriodNormalUs;
         Psi.lwrrPollPeriodSleepUs  = pParams->lwrrPollPeriodSleepUs;
    }

    return FLCN_OK;
}

/*!
 * @brief Construct and initialize PSI_RAIL
 *
 * @param[in]   pParams     Pointer to RM_PMU_RPC_STRUCT_LPWR_LOADIN_PSI_RAIL_INIT
 *
 * @return      FLCN_OK                     On success
 * @return      FLCN_ERR_ILWALID_STATE      Duplicate initialization request
 * @return      FLCN_ERR_RPC_ILWALID_INPUT  Invalid input
 * @return      FLCN_ERR_NO_FREE_MEM        No free space in DMEM or in DMEM overlay.
 */
static FLCN_STATUS
s_pmuRpcLpwrLoadinPsiRailInit
(
    RM_PMU_RPC_STRUCT_LPWR_LOADIN_PSI_RAIL_INIT *pParams
)
{
    LwU32       railId   = pParams->railId;
    PSI_RAIL   *pPsiRail = PSI_GET_RAIL(railId);
    FLCN_STATUS status   = FLCN_OK;

    //
    // Skip the initialization if PSI rail is already supported. This might
    // be duplicate call.
    //
    if (PSI_IS_RAIL_SUPPORTED(railId))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_STATE;

        goto s_pmuRpcLpwrLoadinPsiRailInit_exit;
    }

    // Return early on invalid input
    if ((pParams->vrInterfaceMode == RM_PMU_PSI_RAIL_VR_INTERFACE_GPIO) &&
        (pParams->gpioPin >= LW_U8_MAX))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_RPC_ILWALID_INPUT;

        goto s_pmuRpcLpwrLoadinPsiRailInit_exit;
    }

    //
    // Allocate space for PSI Rail. Return if there is no free space.
    // Avoid the duplicate allocations.
    //
    if (pPsiRail == NULL)
    {
        pPsiRail = lwosCallocType(OVL_INDEX_DMEM(lpwr), 1, PSI_RAIL);
        if (pPsiRail == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_NO_FREE_MEM;

            goto s_pmuRpcLpwrLoadinPsiRailInit_exit;
        }

        Psi.pPsiRail[railId] = pPsiRail;

        if (PMUCFG_FEATURE_ENABLED(PMU_PSI_I2C_INTERFACE))
        {
            PSI_RAIL_I2C_INFO *pPsiRailI2cInfo =
                                     lwosCallocType(OVL_INDEX_DMEM(lpwr), 1,
                                                    PSI_RAIL_I2C_INFO);
            if (pPsiRailI2cInfo == NULL)
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_NO_FREE_MEM;

                goto s_pmuRpcLpwrLoadinPsiRailInit_exit;
            }

            pPsiRail->pI2cInfo = pPsiRailI2cInfo;
        }
    }
    pPsiRail->lwrrentOwnerId     = RM_PMU_PSI_CTRL_ID__ILWALID;
    pPsiRail->settleTimeUs       = pParams->settleTimeUs;
    pPsiRail->gpioPin            = pParams->gpioPin;
    pPsiRail->flavor             = pParams->flavor;
    pPsiRail->voltRailIdx        = pParams->voltRailIdx;
    pPsiRail->railDependencyMask = pParams->railDependencyMask;

    if (PSI_FLAVOUR_IS_ENABLED(LWRRENT_AWARE, railId))
    {
        pPsiRail->iCrossmA = pParams->iCrossmA;

        //
        // Since the given rail supports the current aware PSI and
        // if current callwlation callback is not scheduled, then
        // we need to schedule it.
        //
        // If there are other PSI rails which also supports current
        // aware PSI, then callwlation for those rails will be
        // taken care in callback it self.
        //
        if (PMUCFG_FEATURE_ENABLED(PMU_PSI_PMGR_SLEEP_CALLBACK) &&
           !Psi.bCallbackScheduled)
        {
            psiSchedulePmgrCallbackMultiRail(LW_TRUE);
            Psi.bCallbackScheduled = LW_TRUE;
        }

    }

    // If given PSI rail supports the I2c Based VR, fill the I2C related details
    if (PMUCFG_FEATURE_ENABLED(PMU_PSI_I2C_INTERFACE) &&
        pParams->vrInterfaceMode == RM_PMU_PSI_RAIL_VR_INTERFACE_I2C)
    {
        // Update the VR Interface Mode
        pPsiRail->vrInterfaceMode    = pParams->vrInterfaceMode;

        // Update the I2c device Index from PSI Rail VBIOS Table
        pPsiRail->pI2cInfo->i2cDevIdx = pParams->dcbEntryIndexForVr;

        // Update the I2C Device Type - For now Hard coding the device type
        pPsiRail->pI2cInfo->i2cDevType = RM_PMU_I2C_DEVICE_TYPE_POWER_CONTRL_MP28XX;

        // Update the PSI Engage/Disenagge command for the rail.
        psiRailI2cCommandInit(railId);
    }

    // PSI Rail construction completed. Update the rail support bit.
    Psi.railSupportMask |= BIT(railId);

s_pmuRpcLpwrLoadinPsiRailInit_exit:

    return status;
}

/*!
 * @brief Construct and initialize PSI_CTRL
 *
 * @param[in]   pParams     Pointer to RM_PMU_RPC_STRUCT_LPWR_LOADIN_PSI_CTRL_INIT
 *
 * @return      FLCN_OK                     On success
 * @return      FLCN_ERR_ILWALID_STATE      Duplicate initialization request
 * @return      FLCN_ERR_NO_FREE_MEM        No free space in DMEM or in DMEM overlay.
 */
static FLCN_STATUS
s_pmuRpcLpwrLoadinPsiCtrlInit
(
    RM_PMU_RPC_STRUCT_LPWR_LOADIN_PSI_CTRL_INIT *pParams
)
{
    LwU32                 ctrlId        = pParams->ctrlId;
    PSI_CTRL             *pPsiCtrl      = PSI_GET_CTRL(ctrlId);
    RM_PMU_PSI_CTRL_STAT *pPsiCtrlStats = NULL;
    FLCN_STATUS           status        = FLCN_OK;
    LwU32                 railId;

    //
    // Skip the initialization if PsiCtrl is already supported. This might
    // be duplicate call.
    //
    if (PSI_IS_CTRL_SUPPORTED(ctrlId))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_STATE;

        goto s_pmuRpcLpwrLoadinPsiCtrlInit_exit;
    }

    //
    // Allocate space for PSI Ctrl. Return if there is no free space.
    // Avoid the duplicate allocations.
    //
    if (pPsiCtrl == NULL)
    {
        pPsiCtrl = lwosCallocType(OVL_INDEX_DMEM(lpwr), 1, PSI_CTRL);
        if (pPsiCtrl == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_NO_FREE_MEM;

            goto s_pmuRpcLpwrLoadinPsiCtrlInit_exit;
        }

        Psi.pPsiCtrl[ctrlId] = pPsiCtrl;
    }

    // Update the rail enabled mask
    if (PMUCFG_FEATURE_ENABLED(PMU_PSI_MULTIRAIL))
    {
        pPsiCtrl->railEnabledMask = Psi.railSupportMask &
                                    pParams->railEnabledMask;
    }

    if ((PMUCFG_FEATURE_ENABLED(PMU_PSI_CTRL_MS)) &&
        (ctrlId == RM_PMU_PSI_CTRL_ID_MS))
    {
        pPsiCtrl->mutexAcquireTimeoutUs =
                        PSI_MUTEX_ACQUIRE_TIMEOUT_MSCG_COUPLED_US;

    }
    else if ((PMUCFG_FEATURE_ENABLED(PMU_PSI_CTRL_DI)) &&
             (ctrlId == RM_PMU_PSI_CTRL_ID_DI))
    {
        pPsiCtrl->mutexAcquireTimeoutUs =
                        PSI_MUTEX_ACQUIRE_TIMEOUT_DI_COUPLED_US;
    }
    else
    {
        pPsiCtrl->mutexAcquireTimeoutUs =
                        PSI_MUTEX_ACQUIRE_TIMEOUT_DEFAULT_US;
    }

    FOR_EACH_INDEX_IN_MASK(32, railId, Psi.railSupportMask)
    {
        pPsiCtrlStats = pPsiCtrl->pRailStat[railId];

        if (pPsiCtrlStats == NULL)
        {
            pPsiCtrlStats = lwosCallocResidentType(1, RM_PMU_PSI_CTRL_STAT);
            if (pPsiCtrlStats == NULL)
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_NO_FREE_MEM;

                goto s_pmuRpcLpwrLoadinPsiCtrlInit_exit;
            }

            pPsiCtrl->pRailStat[railId] = pPsiCtrlStats;
        }

        // Return rail stat offset.
        status = lwosResVAddrToPhysMemOffsetMap(pPsiCtrlStats, &pParams->railStatsDmemOffset[railId]);
        if (status != FLCN_OK)
        {
            goto s_pmuRpcLpwrLoadinPsiCtrlInit_exit;
        }

        // Initialize sleep current to invalid for current aware PSI
        if (PMUCFG_FEATURE_ENABLED(PMU_PSI_FLAVOUR_LWRRENT_AWARE))
        {
            pPsiCtrl->pRailStat[railId]->iSleepmA = PSI_SLEEP_LWRRENT_ILWALID_MA;
        }
    }
    FOR_EACH_INDEX_IN_MASK_END;

    // Initialize pstate PSI related Settings
    if ((PMUCFG_FEATURE_ENABLED(PMU_LPWR_WITH_PS35))  &&
        (PMUCFG_FEATURE_ENABLED(PMU_PSI_CTRL_PSTATE)) &&
        (ctrlId == RM_PMU_PSI_CTRL_ID_PSTATE))
    {
        status = lpwrLpPstatePostInit();
        if (status != FLCN_OK)
        {
            goto s_pmuRpcLpwrLoadinPsiCtrlInit_exit;
        }
    }

    // Update Psi control support
    Psi.ctrlSupportMask |= BIT(ctrlId);

s_pmuRpcLpwrLoadinPsiCtrlInit_exit:

    return status;
}

/*!
 * @brief Initializes the Adaptive Power module
 *
 * @param[in]   pParams     Pointer to RM_PMU_RPC_STRUCT_LPWR_LOADIN_AP_INIT
 *
 * @return      FLCN_OK     On success
 */
static FLCN_STATUS
s_pmuRpcLpwrLoadinApInit
(
    RM_PMU_RPC_STRUCT_LPWR_LOADIN_AP_INIT *pParams
)
{
    return apInit(pParams);
}

/*!
 * @brief Initialize and enable APCTRL command
 *
 * @param[in]   pParams     Pointer to RM_PMU_RPC_STRUCT_LPWR_LOADIN_AP_CTRL_INIT_AND_ENABLE
 *
 * @return      FLCN_OK                     On success
 * @return      FLCN_ERR_ILWALID_STATE      Duplicate initialization request
 * @return      FLCN_ERR_RPC_ILWALID_INPUT  Invalid input
 * @return      FLCN_ERR_NO_FREE_MEM        No free space in DMEM or in DMEM overlay
 * @return      FLCN_ERR_NOT_SUPPORTED      Parent is not supported
 */
static FLCN_STATUS
s_pmuRpcLpwrLoadinApCtrlInitAndEnable
(
    RM_PMU_RPC_STRUCT_LPWR_LOADIN_AP_CTRL_INIT_AND_ENABLE *pParams
)
{
    FLCN_STATUS status = FLCN_OK;

    // Initialize ApCtrl. Bail out if something goes wrong.
    status = apCtrlInit(pParams);
    if (status != FLCN_OK)
    {
        goto s_pmuRpcLpwrLoadinApCtrlInitAndEnable_exit;
    }

    // Fill stats dmem offset details.
    status = lwosResVAddrToPhysMemOffsetMap(Ap.pApCtrl[pParams->ctrlId]->pStat, &pParams->statsDmemOffset);
    if (status != FLCN_OK)
    {
        goto s_pmuRpcLpwrLoadinApCtrlInitAndEnable_exit;
    }

    // Enable ApCtrl. Bail out if something goes wrong.
    status = apCtrlEnable(pParams->ctrlId, AP_CTRL_ENABLE_MASK(RM));
    if (status != FLCN_OK)
    {
        goto s_pmuRpcLpwrLoadinApCtrlInitAndEnable_exit;
    }

s_pmuRpcLpwrLoadinApCtrlInitAndEnable_exit:

    return status;
}

/*!
 * @brief Initializes SEQ_SRAM_CTRL in PMU
 *
 * @param[in]   pParams    Pointer to RM_PMU_RPC_STRUCT_LPWR_LOADIN_SEQ_SRAM_CTRL_INIT
 *
 * @return      FLCN_OK                     On success
 * @return      FLCN_ERR_ILWALID_STATE      Duplicate initialization request
 * @return      FLCN_ERR_NO_FREE_MEM        No free space in DMEM or in DMEM overlay
 */
static FLCN_STATUS
s_pmuRpcLpwrLoadinSeqSramCtrlInit
(
    RM_PMU_RPC_STRUCT_LPWR_LOADIN_SEQ_SRAM_CTRL_INIT *pParams
)
{
    LwU8                    ctrlId         = pParams->ctrlId;
    LPWR_SEQ_CTRL          *pSramSeqCtrl   = NULL;
    FLCN_STATUS             status         = FLCN_OK;

    status = lpwrSeqSramCtrlInit(ctrlId);
    if (status == FLCN_OK)
    {
        pSramSeqCtrl = LPWR_SEQ_GET_SRAM_CTRL(ctrlId);

        // Fill the stats dmem offset details.
        status = lwosResVAddrToPhysMemOffsetMap(pSramSeqCtrl->pStats,
                                                &pParams->statsDmemOffset);
    }

    return status;
}

/*!
 * @brief Initializes PgLog in PMU
 * @param[in]   pParams    Pointer to RM_PMU_RPC_STRUCT_LPWR_LOADIN_PG_LOG_INIT
 *
 * @return      FLCN_OK    On success
 */
static FLCN_STATUS
s_pmuRpcLpwrLoadinPgLogInit
(
    RM_PMU_RPC_STRUCT_LPWR_LOADIN_PG_LOG_INIT *pParams
)
{
    FLCN_STATUS status = FLCN_OK;

    pgLogInit(pParams);

    // Fill rpc OUT data with PgLog offset.
    status = lwosResVAddrToPhysMemOffsetMap(&PgLog.putOffset, &pParams->fbPutAddrDmemOffset);
    if (status != FLCN_OK)
    {
        goto s_pmuRpcLpwrLoadinPgLogInit_exit;
    }

    pParams->fbPutAddrVal        = PgLog.putOffset;

s_pmuRpcLpwrLoadinPgLogInit_exit:

    return status;
}

/*!
 * @brief Exelwtes rpc to Init Gc6
 * @param[in]   pParams    Pointer to RM_PMU_RPC_STRUCT_LPWR_LOADIN_PGISLAND_GC6_INIT
 *
 * @return      status     FLCN_STATUS
 */
static FLCN_STATUS
s_pmuRpcLpwrLoadinPgislandGc6Init
(
    RM_PMU_RPC_STRUCT_LPWR_LOADIN_PGISLAND_GC6_INIT *pParams
)
{
    FLCN_STATUS status = FLCN_OK;

    //
    // Lower Task Prioty for the copy to avoid startvations of
    // other tasks while copy is happening
    //
    lwrtosLwrrentTaskPrioritySet(1);

    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(libGC6));

    if (pParams->gc6Init.gc6ExitMode == GCX_GC6_BSI_DRIVEN_SUPPORTED)
    {
        // Sanity check the copy request
        status = pgIslandBsiRamCopyCheck_HAL(&Pg, Pg.dmaReadBuffer,
                                    pParams->gc6Init.gc6BsiInitParams.size,
                                    pParams->gc6Init.gc6BsiInitParams.bsiBootPhaseInfo);
        if (status == FLCN_OK)
        {
            pgIslandBsiRamCopy_HAL(&Pg, Pg.dmaReadBuffer,
                                    pParams->gc6Init.gc6BsiInitParams.size,
                                    pParams->gc6Init.gc6BsiInitParams.bsiBootPhaseInfo);
        }
    }

    // Remove delay from SCI delay table if RTD3 AUX switch is not supported
    if (!pParams->gc6Init.bAuxSwitchSupported)
    {
        pgGc6Rtd3VoltRemoveSciDelay_HAL();
    }

    OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(libGC6));

    // Restore task priority
    lwrtosLwrrentTaskPrioritySet(OSTASK_PRIORITY(LPWR));

    return status;
}

/*!
 * @brief Exelwtes rpc to stores GPIO pin mask
 * @param[in]   pParams    Pointer to RM_PMU_RPC_STRUCT_LPWR_LOADIN_PGISLAND_SCI_PMGR_GPIO_SYNC
 *
 * @return      FLCN_OK    On success
 */
static FLCN_STATUS
s_pmuRpcLpwrLoadinPgislandSciPmgrGpioSync
(
    RM_PMU_RPC_STRUCT_LPWR_LOADIN_PGISLAND_SCI_PMGR_GPIO_SYNC *pParams
)
{
    //
    // This command stores GPIO pin mask in GCX object
    // "set" bit in this mask will denote the SCI GPIO pins
    // that needs to be in sync with PMGR GPIO
    //
    Pg.gpioPinMask = pParams->gpioPinMask;

    return FLCN_OK;
}

/*!
 * @brief Exelwtes rpc to stores GPIO pin mask
 * @param[in]   pParams    Pointer to RM_PMU_RPC_STRUCT_LPWR_LOADIN_PGISLAND_SCI_PMGR_GPIO_SYNC
 *
 * @return      FLCN_OK    On success
 */
static FLCN_STATUS
s_pmuRpcLpwrLoadinPgislandGc6ExitCompleteNotify
(
    RM_PMU_RPC_STRUCT_LPWR_LOADIN_PGISLAND_GC6_EXIT_COMPLETE_NOTIFY *pParams
)
{
    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(libGC6));

    // Sanity check the copy request
    pgIslandClearInterrupts_HAL(&Pg);

    OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(libGC6));

    return FLCN_OK;
}

/*!
 * @brief Exelwtes the GC6 entry sequence which detached PMU
 * @param[in]   pParams    Pointer to RM_PMU_RPC_STRUCT_LPWR_LOADIN_GC6_PMU_DETACH
 *
 * @return      FLCN_OK    On success
 */
static FLCN_STATUS
s_pmuRpcLpwrLoadinGc6PmuDetach
(
    RM_PMU_RPC_STRUCT_LPWR_LOADIN_GC6_PMU_DETACH *pParams
)
{
    // Attach and Load required Overlays
    pgOverlaySyncGpioAttachAndLoad();
#if PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_MULTI_FAN)
    pgGc6SmartFanOvlAttachAndLoad();
#endif

    pgGc6PmuDetach_HAL(&Pg, &pParams->gc6Detach);

    // Detach Overlays
    pgOverlaySyncGpioDetach();

    return FLCN_OK;
}

/*!
 * @brief Exelwtes RPC to get GPU Idle Mask
 *
 * @param[in]   pParams  Pointer to RM_PMU_RPC_STRUCT_LPWR_LOADIN_GPU_IDLE_MASK_GET
 *
 * @return      FLCN_OK  On success
 */
static FLCN_STATUS
s_pmuRpcLpwrLoadinGpuIdleMaskGet
(
    RM_PMU_RPC_STRUCT_LPWR_LOADIN_GPU_IDLE_MASK_GET *pParams
)
{
    lpwrGpuIdleMaskGet_HAL(&Lpwr, pParams->idleMask);

    return FLCN_OK;
}
