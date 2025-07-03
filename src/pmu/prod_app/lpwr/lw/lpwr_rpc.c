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
 * @file   lpwr_rpc.c
 * @brief  Manage RPCs for LowPower module
 */

/* ------------------------- System Includes ------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes -------------------------- */
#include "pmu_objhal.h"
#include "dbgprintf.h"
#include "pmu_objlpwr.h"
#include "pmu_objap.h"
#include "pmu_objgr.h"
#include "pmu_objms.h"
#include "pmu_objdi.h"
#include "pmu_disp.h"
#include "pmu_objdisp.h"
#include "pmu_objdfpr.h"

#include "g_pmuifrpc.h"

// Wrapper for the generated FAN RPC source file.
#include "rpcgen/g_pmurpc_lpwr.c"

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief Execute power source change RPC in PMU
 *
 * @param[in]   pParams     Pointer to RM_PMU_RPC_STRUCT_LPWR_PWR_SRC_CHANGE
 * @param[out]  pLwstom     Void pointer used to send back data to caller.
 *                          Caller sends pointer to PG_LOGIC_STATE. Parameter
 *                          not used in this RPC.
 *
 * @return      FLCN_OK     On success
 */
static FLCN_STATUS
s_pmuRpcLpwrPwrSrcChange
(
    RM_PMU_RPC_STRUCT_LPWR_PWR_SRC_CHANGE  *pParams,
    void                                   *pLwstom
)
{
    FLCN_STATUS status = FLCN_OK;

    if (pParams->bPwrSrcDc != Lpwr.bPwrSrcDc)
    {
        if (PMUCFG_FEATURE_ENABLED(PMU_AP_CTRL_GR) &&
            AP_IS_CTRL_SUPPORTED(RM_PMU_AP_CTRL_ID_GR))
        {
            status = apCtrlEventHandlerPwrSrc(RM_PMU_AP_CTRL_ID_GR,
                                              pParams->bPwrSrcDc);
        }

        if (PMUCFG_FEATURE_ENABLED(PMU_AP_CTRL_MSCG) &&
            AP_IS_CTRL_SUPPORTED(RM_PMU_AP_CTRL_ID_MSCG))
        {
            status = apCtrlEventHandlerPwrSrc(RM_PMU_AP_CTRL_ID_MSCG,
                                              pParams->bPwrSrcDc);
        }

        Lpwr.bPwrSrcDc = pParams->bPwrSrcDc;
    }

    return status;
}

/*!
 * @brief Execute Sleep Aware Callback mode RPC in PMU
 *
 * @param[in]   pParams     Pointer to RM_PMU_RPC_STRUCT_LPWR_SAC_MODE
 * @param[out]  pLwstom     Void pointer used to send back data to caller.
 *                          Caller sends pointer to PG_LOGIC_STATE. Parameter
 *                          not used in this RPC.
 *
 * @return      FLCN_OK     On success
 */
static FLCN_STATUS
s_pmuRpcLpwrSacMode
(
    RM_PMU_RPC_STRUCT_LPWR_SAC_MODE *pParams,
    void                            *pLwstom
)
{
    lpwrCallbackSacArbiter(LPWR_SAC_CLIENT_ID_RM, pParams->bSleep);

    return FLCN_OK;
}

/*!
 * @brief Execute PgCtrl allow RPC
 *
 * @param[in]   pParams     Pointer to RM_PMU_RPC_STRUCT_LPWR_PG_CTRL_ALLOW
 * @param[out]  pLwstom     Void pointer used to send back data to caller.
 *                          Caller sends pointer to PG_LOGIC_STATE.
 *
 * @return      FLCN_OK                 On success
 * @return      FLCN_ERR_NOT_SUPPORTED  PgCtrl is not supported
 * @return      FLCN_ERR_ILWALID_STATE  PgCtrl is already allowed by RM
 */
static FLCN_STATUS
s_pmuRpcLpwrPgCtrlAllow
(
    RM_PMU_RPC_STRUCT_LPWR_PG_CTRL_ALLOW  *pParams,
    void                                  *pLwstom
)
{
    LwU32           ctrlId        = pParams->ctrlId;
    PG_LOGIC_STATE *pPgLogicState = (PG_LOGIC_STATE *)pLwstom;
    OBJPGSTATE     *pPgState;
    FLCN_STATUS     status = FLCN_OK;

    if (pgIsCtrlSupported(ctrlId))
    {
        pPgState = GET_OBJPGSTATE(ctrlId);

        // Colwert RM RPC to ALLOW event
        if (pPgState->bRmDisallowed)
        {
            pPgLogicState->ctrlId   = ctrlId;
            pPgLogicState->eventId  = PMU_PG_EVENT_ALLOW;
            pPgLogicState->data     = PG_CTRL_ALLOW_REASON_MASK(RM);

            pPgState->bRmDisallowed = LW_FALSE;

            if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_GRP_MUTUAL_EXCLUSION))
            {
                if (lpwrGrpIsLpwrCtrlMember(RM_PMU_LPWR_GRP_CTRL_ID_GR, ctrlId))
                {
                    lpwrGrpCtrlRmClientHandler(RM_PMU_LPWR_GRP_CTRL_ID_GR);
                }
                else if (lpwrGrpIsLpwrCtrlMember(RM_PMU_LPWR_GRP_CTRL_ID_MS, ctrlId))
                {
                    lpwrGrpCtrlRmClientHandler(RM_PMU_LPWR_GRP_CTRL_ID_MS);
                }
            }
        }
        else
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_STATE;
        }
    }
    else
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_NOT_SUPPORTED;
    }

    return status;
}

/*!
 * @brief Execute PgCtrl disallow RPC
 *
 * @param[in]   pParams     Pointer to RM_PMU_RPC_STRUCT_LPWR_PG_CTRL_DISALLOW
 * @param[out]  pLwstom     Void pointer used to send back data to caller.
 *                          Caller sends pointer to PG_LOGIC_STATE.
 *
 * @return      FLCN_OK                 On success
 * @return      FLCN_ERR_NOT_SUPPORTED  PgCtrl is not supported
 * @return      FLCN_ERR_ILWALID_STATE  PgCtrl is already disallowed by RM
 */
static FLCN_STATUS
s_pmuRpcLpwrPgCtrlDisallow
(
    RM_PMU_RPC_STRUCT_LPWR_PG_CTRL_DISALLOW  *pParams,
    void                                     *pLwstom
)
{
    LwU32           ctrlId        = pParams->ctrlId;
    PG_LOGIC_STATE *pPgLogicState = (PG_LOGIC_STATE *)pLwstom;
    OBJPGSTATE     *pPgState;
    FLCN_STATUS     status = FLCN_OK;

    if (pgIsCtrlSupported(ctrlId))
    {
        pPgState = GET_OBJPGSTATE(ctrlId);

        //
        // Colwert RM RPC to DISALLOW event. Ack will be sent after disallow
        // is actually done.
        //
        if (!pPgState->bRmDisallowed)
        {
            pPgLogicState->ctrlId   = ctrlId;
            pPgLogicState->eventId  = PMU_PG_EVENT_DISALLOW;
            pPgLogicState->data     = PG_CTRL_DISALLOW_REASON_MASK(RM);

            pPgState->bRmDisallowed = LW_TRUE;

            if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_GRP_MUTUAL_EXCLUSION))
            {
                if (lpwrGrpIsLpwrCtrlMember(RM_PMU_LPWR_GRP_CTRL_ID_GR, ctrlId))
                {
                    lpwrGrpCtrlRmClientHandler(RM_PMU_LPWR_GRP_CTRL_ID_GR);
                }
                else if (lpwrGrpIsLpwrCtrlMember(RM_PMU_LPWR_GRP_CTRL_ID_MS, ctrlId))
                {
                    lpwrGrpCtrlRmClientHandler(RM_PMU_LPWR_GRP_CTRL_ID_MS);
                }
            }
        }
        else
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_STATE;
        }
    }
    else
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_NOT_SUPPORTED;
    }

    return status;
}

/*!
 * @brief Exelwtes PgCtrl threshold update RPC
 *
 * @param[in]   pParams     Pointer to RM_PMU_RPC_STRUCT_LPWR_PG_CTRL_THRESHOLD_UPDATE
 * @param[out]  pLwstom     Void pointer used to send back data to caller.
 *                          Caller sends pointer to PG_LOGIC_STATE. Parameter
 *                          not used in this RPC.
 *
 * @return      FLCN_OK                 On success
 * @return      FLCN_ERR_NOT_SUPPORTED  PgCtrl is not supported
 */
static FLCN_STATUS
s_pmuRpcLpwrPgCtrlThresholdUpdate
(
    RM_PMU_RPC_STRUCT_LPWR_PG_CTRL_THRESHOLD_UPDATE  *pParams,
    void                                             *pLwstom
)
{
    LwU32       ctrlId = pParams->ctrlId;
    FLCN_STATUS status = FLCN_OK;

    if (pgIsCtrlSupported(ctrlId))
    {
        pgCtrlThresholdsUpdate(ctrlId, &pParams->thresholdCycles);
    }
    else
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_NOT_SUPPORTED;
    }

    return status;
}

/*!
 * @brief Exelwtes PgCtrl statistic get RPC
 *
 * @param[in]   pParams     Pointer to RM_PMU_RPC_STRUCT_LPWR_PG_CTRL_STATS_GET
 * @param[out]  pLwstom     Void pointer used to send back data to caller.
 *                          Caller sends pointer to PG_LOGIC_STATE.
 *
 * @return      FLCN_OK                 On success
 * @return      FLCN_ERR_NOT_SUPPORTED  PgCtrl is not supported
 */
static FLCN_STATUS
s_pmuRpcLpwrPgCtrlStatsGet
(
    RM_PMU_RPC_STRUCT_LPWR_PG_CTRL_STATS_GET  *pParams,
    void                                      *pLwstom
)
{
    LwU32        ctrlId = pParams->ctrlId;
    FLCN_STATUS  status = FLCN_OK;
    OBJPGSTATE  *pPgState;

    if (pgIsCtrlSupported(ctrlId))
    {
        pPgState = GET_OBJPGSTATE(ctrlId);

        // Update stats for PgCtrl
        pgStatUpdate(ctrlId);

        // Copy latest stats to RPC response
        pParams->stats = pPgState->stats;
    }
    else
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_NOT_SUPPORTED;
    }

    return status;
}

/*!
 * @brief Exelwtes PgCtrl statistic reset RPC
 *
 * @param[in]   pParams     Pointer to RM_PMU_RPC_STRUCT_LPWR_PG_CTRL_STATS_RESET
 * @param[out]  pLwstom     Void pointer used to send back data to caller.
 *                          Caller sends pointer to PG_LOGIC_STATE. Parameter
 *                          not used in this RPC.
 *
 * @return      FLCN_OK                 On success
 * @return      FLCN_ERR_NOT_SUPPORTED  PgCtrl is not supported
 */
static FLCN_STATUS
s_pmuRpcLpwrPgCtrlStatsReset
(
    RM_PMU_RPC_STRUCT_LPWR_PG_CTRL_STATS_RESET  *pParams,
    void                                        *pLwstom
)
{
    LwU32       ctrlId = pParams->ctrlId;
    FLCN_STATUS status = FLCN_OK;
    OBJPGSTATE *pPgState;

    if (pgIsCtrlSupported(ctrlId))
    {
        pPgState = GET_OBJPGSTATE(ctrlId);
        memset(&pPgState->stats, 0, sizeof(RM_PMU_PG_STATISTICS));
    }
    else
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_NOT_SUPPORTED;
    }

    return status;
}

/*!
 * @brief Handle toggling request for wakeup events (Normal/Cumulative)
 *
 * @param[in]   pParams  Pointer to RM_PMU_RPC_STRUCT_LPWR_PG_CTRL_TOGGLE_WAKEUP_TYPE
 * @param[out]  pLwstom  Void pointer used to send back data to caller.
 *                       Caller sends pointer to PG_LOGIC_STATE. Parameter
 *                       not used in this RPC.
 *
 * @return      FLCN_OK     On success
 */
static FLCN_STATUS
s_pmuRpcLpwrPgCtrlWakeupType
(
    RM_PMU_RPC_STRUCT_LPWR_PG_CTRL_WAKEUP_TYPE   *pParams,
    void                                         *pLwstom
)
{
    OBJPGSTATE *pPgState;
    LwU32       ctrlId = pParams->ctrlId;
    FLCN_STATUS status = FLCN_OK;

    if (pgIsCtrlSupported(ctrlId))
    {
        pPgState = GET_OBJPGSTATE(ctrlId);
        pPgState->bLwmulativeWakeupMask = 
                             pParams->bLwmulativeWakeupMask;
    }
    else
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_NOT_SUPPORTED;
    }

    return status;
}

/*!
 * @brief Sets the value for ICross onephase current (mA)
 *
 * @param[in]   pParams     Pointer to RM_PMU_RPC_STRUCT_LPWR_PSI_I_OPTIMAL_UPDATE
 * @param[out]  pLwstom     Void pointer used to send back data to caller.
 *                          Caller sends pointer to PG_LOGIC_STATE. Parameter
 *                          not used in this RPC.
 *
 * @return      FLCN_OK                 On success
 * @return      FLCN_ERR_NOT_SUPPORTED  PSI Rail is not supported
 */
static FLCN_STATUS
s_pmuRpcLpwrPsiICrossUpdate
(
    RM_PMU_RPC_STRUCT_LPWR_PSI_I_CROSS_UPDATE  *pParams,
    void                                       *pLwstom
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       railId = pParams->railId;

    if (PSI_IS_RAIL_SUPPORTED(railId))
    {
        PSI_GET_RAIL(railId)->iCrossmA = pParams->lwrrentmA;
    }
    else
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_NOT_SUPPORTED;
    }

    return status;
}

/*!
 * @brief Reset PSI engage count for the specified PSI CTRL.
 *
 * @param[in]   pParams     Pointer to RM_PMU_RPC_STRUCT_LPWR_PSI_STATS_RESET
 * @param[out]  pLwstom     Void pointer used to send back data to caller.
 *                          Caller sends pointer to PG_LOGIC_STATE. Parameter
 *                          not used in this RPC.
 *
 * @return      FLCN_OK                  On success
 * @return      FLCN_ERR_NOT_SUPPORTED   On invalid ctrlId
 */
static FLCN_STATUS
s_pmuRpcLpwrPsiStatsReset
(
    RM_PMU_RPC_STRUCT_LPWR_PSI_STATS_RESET  *pParams,
    void                                    *pLwstom
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       ctrlId = pParams->ctrlId;
    PSI_CTRL   *pPsiCtrl;

    if (PSI_IS_CTRL_SUPPORTED(ctrlId))
    {
        pPsiCtrl = PSI_GET_CTRL(ctrlId);
        pPsiCtrl->pRailStat[pParams->railId]->engageCount = 0;
    }
    else
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_NOT_SUPPORTED;
    }

    return status;
}

/*!
 * @brief update the railEnabledMask for the specified PSI CTRL.
 *
 * @param[in]   pParams     Pointer to RM_PMU_RPC_STRUCT_LPWR_PSI_CTRL_RAIL_MASK_UPDATE
 * @param[out]  pLwstom     Void pointer used to send back data to caller.
 *                          Caller sends pointer to PG_LOGIC_STATE. Parameter
 *                          not used in this RPC.
 *
 * @return      FLCN_OK                  On success
 * @return      FLCN_ERR_NOT_SUPPORTED   PsiCtrl is not supported
 */
static FLCN_STATUS
s_pmuRpcLpwrPsiCtrlRailMaskUpdate
(
    RM_PMU_RPC_STRUCT_LPWR_PSI_CTRL_RAIL_MASK_UPDATE  *pParams,
    void                                              *pLwstom
)
{
    LwU8        ctrlId   = pParams->ctrlId;
    PSI_CTRL   *pPsiCtrl = NULL;
    FLCN_STATUS status   = FLCN_OK;

    // Update the railEnabled Mask for psiCtrl.
    if (PSI_IS_CTRL_SUPPORTED(ctrlId))
    {
        pPsiCtrl = PSI_GET_CTRL(ctrlId);

        pPsiCtrl->railEnabledMask = Psi.railSupportMask &
                                    pParams->railEnabledMask;
    }
    else
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_NOT_SUPPORTED;
    }

    return status;
}

/*!
 * @brief Enable ApCtrl
 *
 * Refer @apCtrlEnable() for detailed sequence
 *
 * @param[in]   pParams     Pointer to RM_PMU_RPC_STRUCT_LPWR_AP_CTRL_ENABLE
 * @param[out]  pLwstom     Void pointer used to send back data to caller.
 *                          Caller sends pointer to PG_LOGIC_STATE. Parameter
 *                          not used in this RPC.
 *
 * @return      FLCN_OK                 On success
 * @return      FLCN_ERR_NOT_SUPPORTED  ApCtrl is not supported
 */
static FLCN_STATUS
s_pmuRpcLpwrApCtrlEnable
(
    RM_PMU_RPC_STRUCT_LPWR_AP_CTRL_ENABLE  *pParams,
    void                                   *pLwstom
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       ctrlId = pParams->ctrlId;

    if (AP_IS_CTRL_SUPPORTED(ctrlId))
    {
        status = apCtrlEnable(ctrlId, AP_CTRL_ENABLE_MASK(RM));
    }
    else
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_NOT_SUPPORTED;
    }

    return status;
}

/*!
 * @brief Disable ApCtrl
 *
 * Refer @apCtrlDisable() for detailed sequence.
 *
 * @param[in]   pParams     Pointer to RM_PMU_RPC_STRUCT_LPWR_AP_CTRL_DISABLE
 * @param[out]  pLwstom     Void pointer used to send back data to caller.
 *                          Caller sends pointer to PG_LOGIC_STATE. Parameter
 *                          not used in this RPC.
 *
 * @return      FLCN_OK                 On success
 * @return      FLCN_ERR_NOT_SUPPORTED  ApCtrl is not supported
 */
static FLCN_STATUS
s_pmuRpcLpwrApCtrlDisable
(
    RM_PMU_RPC_STRUCT_LPWR_AP_CTRL_DISABLE  *pParams,
    void                                    *pLwstom
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       ctrlId = pParams->ctrlId;

    if (AP_IS_CTRL_SUPPORTED(ctrlId))
    {
        status = apCtrlDisable(ctrlId, AP_CTRL_DISABLE_MASK(RM));
    }
    else
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_NOT_SUPPORTED;
    }

    return status;
}

/*!
 * @brief Kick off AELPG
 *
 * To be called pre/post a disruptive event. An event is be considered
 * disruptive if it:
 *      1. Changes the graphics activity pattern
 *      2. The histogram settings need to be adjusted
 *
 * @param[in]   pParams     Pointer to RM_PMU_RPC_STRUCT_LPWR_AP_CTRL_KICK
 * @param[out]  pLwstom     Void pointer used to send back data to caller.
 *                          Caller sends pointer to PG_LOGIC_STATE. Parameter
 *                          not used in this RPC.
 *
 * @return       FLCN_OK                 On success
 * @return      FLCN_ERR_NOT_SUPPORTED  ApCtrl is not supported
 */
static FLCN_STATUS
s_pmuRpcLpwrApCtrlKick
(
    RM_PMU_RPC_STRUCT_LPWR_AP_CTRL_KICK  *pParams,
    void                                 *pLwstom
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       ctrlId = pParams->ctrlId;

    if (AP_IS_CTRL_SUPPORTED(ctrlId))
    {
        status = apCtrlKick(ctrlId, pParams->skipCount);
    }
    else
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_NOT_SUPPORTED;
    }

    return status;
}
/*!
 * @brief Exelwtes the pstate change begin/end events.
 *
 * @param[in]   pParams     Pointer to RM_PMU_RPC_STRUCT_LPWR_PSTATE_CHANGE
 * @param[out]  pLwstom     Void pointer used to send back data to caller.
 *                          Caller sends pointer to PG_LOGIC_STATE. Parameter
 *                          not used in this RPC.
 *
 * @return      FLCN_OK                  On success
 */
static FLCN_STATUS
s_pmuRpcLpwrPstateChange
(
    RM_PMU_RPC_STRUCT_LPWR_PSTATE_CHANGE  *pParams,
    void                                  *pLwstom
)
{
    lpwrProcessPstateChange(pParams);

    return FLCN_OK;
}

/*!
 * @brief Exelwtes the pstate change begin/end events.
 *
 * @param[in]   pParams     Pointer to RM_PMU_RPC_STRUCT_LPWR_PG_CTRL_SFM_UPDATE
 * @param[out]  pLwstom     Void pointer used to send back data to caller.
 *                          Caller sends pointer to PG_LOGIC_STATE. Parameter
 *                          not used in this RPC.
 *
 * @return      FLCN_OK                  On success
 * @return      FLCN_ERR_NOT_SUPPORTED   LpwrCtrl is not supported
 */
static FLCN_STATUS
s_pmuRpcLpwrPgCtrlSfmUpdate
(
    RM_PMU_RPC_STRUCT_LPWR_PG_CTRL_SFM_UPDATE  *pParams,
    void                                       *pLwstom
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       ctrlId = pParams->ctrlId;
    OBJPGSTATE *pPgState;

    if (pgIsCtrlSupported(ctrlId))
    {
        if (PMUCFG_FEATURE_ENABLED(PMU_PG_SFM_2))
        {
            pgCtrlSubfeatureMaskRequest(ctrlId, LPWR_CTRL_SFM_CLIENT_ID_RM,
                                        pParams->enabledMask);
        }
        else
        {
            // Update PgCtrl enabled mask
            pPgState = GET_OBJPGSTATE(ctrlId);
            pPgState->enabledMask = pPgState->supportMask & pParams->enabledMask;

            if ((PMUCFG_FEATURE_ENABLED(PMU_PG_MS)) &&
                (ctrlId == RM_PMU_LPWR_CTRL_ID_MS_MSCG))
            {
                // Re-enable HW wakeup interrupts as the sub-feature mask has changed
                msEnableIntr_HAL(&Ms);
            }
        }
    }
    else
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_NOT_SUPPORTED;
    }

    return status;
}

/*!
 * @brief Exelwtes the pstate change begin/end events.
 *
 * @param[in]   pParams     Pointer to RM_PMU_RPC_STRUCT_LPWR_PG_CTRL_DI_VOLTAGE_CHANGE
 * @param[out]  pLwstom     Void pointer used to send back data to caller.
 *                          Caller sends pointer to PG_LOGIC_STATE. Parameter
 *                          not used in this RPC.
 *
 * @return      FLCN_OK                  On success
 * @return      FLCN_ERR_NOT_SUPPORTED   LPWR volt rail framework not supported
 */
static FLCN_STATUS
s_pmuRpcLpwrDiVoltageChange
(
    RM_PMU_RPC_STRUCT_LPWR_DI_VOLTAGE_CHANGE  *pParams,
    void                                      *pLwstom
)
{
    FLCN_STATUS status = FLCN_OK;

    if (!PMUCFG_FEATURE_ENABLED(PMU_VOLT) &&
        PMUCFG_FEATURE_ENABLED(PMU_DI_SEQUENCE_COMMON))
    {
        PG_VOLT_SET_SLEEP_VOLTAGE_UV(PG_VOLT_RAIL_IDX_LOGIC, 
                                    pParams->diVoltageChangeValue);
    }
    else
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_NOT_SUPPORTED;
    }

    return status;

}

/*!
 * @brief Exelwtes rpc to reset SRAM Sequencer stats
 *
 * @param[in]   pParams     Pointer to RM_PMU_RPC_STRUCT_LPWR_SEQ_SRAM_STATS_RESET
 * @param[out]  pLwstom     Void pointer used to send back data to caller.
 *                          Caller sends pointer to PG_LOGIC_STATE. Parameter
 *                          not used in this RPC.
 *
 * @return      FLCN_OK                  On success
 * @return      FLCN_ERR_NOT_SUPPORTED   RPPG or RppgCtrl not support
 */
static FLCN_STATUS
s_pmuRpcLpwrSeqSramStatsReset
(
    RM_PMU_RPC_STRUCT_LPWR_SEQ_SRAM_STATS_RESET  *pParams,
    void                                         *pLwstom
)
{
    LwU8           ctrlId = pParams->ctrlId;
    LPWR_SEQ_CTRL *pSramSeqCtrl;
    FLCN_STATUS    status = FLCN_ERR_NOT_SUPPORTED;

    if (LPWR_SEQ_IS_SRAM_CTRL_SUPPORTED(ctrlId))
    {
        pSramSeqCtrl = LPWR_SEQ_GET_SRAM_CTRL(ctrlId);

        if (pSramSeqCtrl != NULL)
        {
            pSramSeqCtrl->pStats->entryCount = 0;
            pSramSeqCtrl->pStats->exitCount  = 0;

            status = FLCN_OK;
        }
    }
    return status;
}

/*!
 * @brief Exelwtes the rpc to To enable/disable the ELCG
 *
 * @param[in]   pParams     Pointer to RM_PMU_RPC_STRUCT_LPWR_CG_ELCG_STATE_CHANGE
 * @param[out]  pLwstom     Void pointer used to send back data to caller.
 *                          Caller sends pointer to PG_LOGIC_STATE. Parameter
 *                          not used in this RPC.
 *
 * @return      FLCN_OK                      On success
 *              FLCN_ERR_NOT_SUPPORTED       If requestMask size is > 32 bits
 *              FLCN_ERR_RPC_ILWALID_INPUT   If requestMask is invalid
 */
static FLCN_STATUS
s_pmuRpcLpwrCgElcgStateChange
(
    RM_PMU_RPC_STRUCT_LPWR_CG_ELCG_STATE_CHANGE  *pParams,
    void                                         *pLwstom
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       engHwId;

    for (engHwId = 0; engHwId < RM_PMU_LPWR_CG_ELCG_CTRL__COUNT; engHwId++)
    {
        if (pParams->bEnableReq[engHwId])
        {
            status = lpwrCgElcgCtrlEnable_HAL(&Lpwr, engHwId, LPWR_CG_ELCG_CTRL_REASON_RM);
            if (status != FLCN_OK)
            {
                goto s_pmuRpcLpwrCgElcgStateChange_exit;
            }
        }

        if (pParams->bDisableReq[engHwId])
        {
            status = lpwrCgElcgCtrlDisable_HAL(&Lpwr, engHwId, LPWR_CG_ELCG_CTRL_REASON_RM);
            if (status != FLCN_OK)
            {
                goto s_pmuRpcLpwrCgElcgStateChange_exit;
            }
        }
    }

s_pmuRpcLpwrCgElcgStateChange_exit:

    return status;
}

/*!
 * @brief Exelwtes rpc to initialize RPPG in PMU
 *
 * @param[in]   pParams     Pointer to RM_PMU_RPC_STRUCT_LPWR_PG_LOG_FLUSH
 * @param[out]  pLwstom     Void pointer used to send back data to caller.
 *                          Caller sends pointer to PG_LOGIC_STATE. Parameter
 *                          not used in this RPC.
 *
 * @return      FLCN_OK     On success
 */
static FLCN_STATUS
s_pmuRpcLpwrPgLogFlush
(
    RM_PMU_RPC_STRUCT_LPWR_PG_LOG_FLUSH  *pParams,
    void                                 *pLwstom
)
{
    // Check if PG log is initialized
    if (PgLog.bInitialized)
    {
        // Force a flush
        PgLog.bFlushRequired = LW_TRUE;
        pgLogFlush();
    }

    return FLCN_OK;
}

/*!
 * @brief Handle Oneshot state change (Enable/Disable) command
 *
 * @param[in]   pParams     Pointer to RM_PMU_RPC_STRUCT_LPWR_ONESHOT
 * @param[out]  pLwstom     Void pointer used to send back data to caller.
 *                          Caller sends pointer to PG_LOGIC_STATE. Parameter
 *                          not used in this RPC.
 *
 * @return      FLCN_OK                      On success
 * @return      FLCN_ERR_RPC_ILWALID_INPUT   Invalid input from RPC parameter pParams.
 */
static FLCN_STATUS
s_pmuRpcLpwrOneshot
(
    RM_PMU_RPC_STRUCT_LPWR_ONESHOT   *pParams,
    void                             *pLwstom
)
{
    FLCN_STATUS status = FLCN_OK;

    if (PMUCFG_FEATURE_ENABLED(PMU_SELWRITY_HARDENING))
    {
        // Validate headIdx before set it to pRmOneshotCtx->head.idx
        if (pParams->headIdx >= Disp.numHeads)
        {
            status = FLCN_ERR_RPC_ILWALID_INPUT;
            goto s_pmuRpcLpwrOneshot_exit;
        }
    }

    Pg.bOsmAckPending = LW_TRUE;

    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(disp));
    if (dispRpcLpwrOsmStateChange(pParams))
    {
        lpwrOsmAckSend();
    }
    OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(disp));

s_pmuRpcLpwrOneshot_exit:
    return status;
}

/*!
 * @brief Handle DIFR Prefetch Request response
 *
 * @param[in]   pParams     Pointer to RM_PMU_RPC_STRUCT_LPWR_DIFR_PREFETCH_RESPONSE
 * @param[out]  pLwstom     Void pointer used to send back data to caller.
 *                          Caller sends pointer to PG_LOGIC_STATE. Parameter
 *                          not used in this RPC.
 *
 * @return      FLCN_OK                      On success
 * @return      FLCN_ERR_RPC_ILWALID_INPUT   Invalid input from RPC parameter pParams.
 */
static FLCN_STATUS
s_pmuRpcLpwrDifrPrefetchResponse
(
    RM_PMU_RPC_STRUCT_LPWR_DIFR_PREFETCH_RESPONSE   *pParams,
    void                                            *pLwstom
)
{
    // Call the DFPR API to handle the prefetch response
    return dfprPrefetchResponseHandler(pParams->bIsPrefetchResponseSuccess);
}
