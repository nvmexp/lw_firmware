/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 - 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   lpwr_lp_rpc.c
 * @brief  Manage RPCs for LowPower module
 */

/* ------------------------- System Includes ------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes -------------------------- */
#include "pmu_objhal.h"
#include "dbgprintf.h"
#include "pmu_objlpwr.h"
#include "pmu_objei.h"
#include "pmu_lpwr_test.h"

#include "g_pmuifrpc.h"

// Wrapper for the generated LPWR_LP RPC source file.
#include "rpcgen/g_pmurpc_lpwr_lp.c"

/* ------------------------- Private Functions ------------------------------ */


/*!
 * @brief Exelwtes EI Client initialization RPC in PMU
 *
 * RPC initializes EI Client
 *
 * @param[in]   pParams     Pointer to RM_PMU_RPC_STRUCT_LPWR_LP_EI_CLIENT_INIT
 *
 * @return      FLCN_OK                     On success
 * @return      FLCN_ERR_ILWALID_STATE      Duplicate initialization request
 * @return      FLCN_ERR_NO_FREE_MEM        No free space in DMEM or in DMEM overlay
 * @return      FLCN_ERR_NOT_SUPPORTED      HW PG_ENG not supported
 */
static FLCN_STATUS
s_pmuRpcLpwrLpEiClientInit
(
    RM_PMU_RPC_STRUCT_LPWR_LP_EI_CLIENT_INIT *pParams
)
{
    FLCN_STATUS status = FLCN_OK;

    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(lpwrLoadin));

    status = eiClientInit(pParams->clientId);
    if (status != FLCN_OK)
    {
        goto s_pmuRpcLpwrLpEiClientInit_exit;
    }

s_pmuRpcLpwrLpEiClientInit_exit:
    OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(lpwrLoadin));

    return status;
}

/*!
 * @brief Enable/Disable the Client Notification
 *
 * Enable/disable the EI Client's Notification
 *
 * @param[in]   pParams     Pointer to RM_PMU_RPC_STRUCT_LPWR_LP_EI_CLIENT_NOTIFICATION_ENABLE
 *
 * @return      FLCN_OK                     On success
 * @return      FLCN_ERR_ILWALID_STATE      Duplicate initialization request
 * @return      FLCN_ERR_NOT_SUPPORTED      HW PG_ENG not supported
 */
static FLCN_STATUS
s_pmuRpcLpwrLpEiClientNotificationEnable
(
    RM_PMU_RPC_STRUCT_LPWR_LP_EI_CLIENT_NOTIFICATION_ENABLE *pParams
)
{
    return eiClientNotificationEnable(pParams->clientId, pParams->bEnable);
}

/*!
 * @brief Update the EI Client's callback time
 *
 * @param[in]   pParams     Pointer to RM_PMU_RPC_STRUCT_LPWR_LP_EI_CLIENT_DEFERRED_CALLBACK_UPDATE
 *
 * @return      FLCN_OK                     On success
 * @return      FLCN_ERR_NOT_SUPPORTED
 */
static FLCN_STATUS
s_pmuRpcLpwrLpEiClientDeferredCallbackUpdate
(
    RM_PMU_RPC_STRUCT_LPWR_LP_EI_CLIENT_DEFERRED_CALLBACK_UPDATE *pParams
)
{
    return eiClientDeferredCallbackUpdate(pParams->clientId, pParams->callbackTimeMs);
}

/*!
 * @brief Initialize callback time period.
 * @param[in]   pParams Pointer to RM_PMU_RPC_STRUCT_LPWR_LP_TEST_INIT
 *
 * @return      FLCN_OK      On success
 *              FLCN_ERR_XXX otherwise.
 */
static FLCN_STATUS
s_pmuRpcLpwrLpTestInit
(
    RM_PMU_RPC_STRUCT_LPWR_LP_TEST_INIT *pParams
)
{
    return lpwrTestInit(pParams->supportMask, pParams->callbackTimeMs);
}

/*!
 * @brief Start LPWR test exelwtion.
 * @param[in]   pParams Pointer to RM_PMU_RPC_STRUCT_LPWR_LP_TEST_TRIGGER_CTRL
 *
 * @return      FLCN_OK      On success
 *              FLCN_ERR_XXX otherwise.
 */
static FLCN_STATUS
s_pmuRpcLpwrLpTestStart
(
    RM_PMU_RPC_STRUCT_LPWR_LP_TEST_START *pParams
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    if (lpwrTestIsSupported(pParams->testId))
    {
        status = lpwrTestStart(pParams->testId);
    }

    return status;
}

/*!
 * @brief Stop LPWR test exelwtion.
 * @param[in]   pParams Pointer to RM_PMU_RPC_STRUCT_LPWR_LP_TEST_TRIGGER_CTRL
 *
 * @return      FLCN_OK      On success
 *              FLCN_ERR_XXX otherwise.
 */
static FLCN_STATUS
s_pmuRpcLpwrLpTestStop
(
    RM_PMU_RPC_STRUCT_LPWR_LP_TEST_STOP *pParams
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    if (lpwrTestIsSupported(pParams->testId))
    {
        status = lpwrTestStop(pParams->testId);
    }

    return status;
}

/*!
 * @brief Enable Disable the Pstate Coupled PSI
 * @param[in]   pParams Pointer to PRM_PMU_RPC_STRUCT_LPWR_LP_TEST_TRIGGER_CTRL
 *
 * @return      FLCN_OK      On success
 *              FLCN_ERR_XXX otherwise.
 */
static FLCN_STATUS
s_pmuRpcLpwrLpPsiPstateDisallow
(
    RM_PMU_RPC_STRUCT_LPWR_LP_PSI_PSTATE_DISALLOW *pParams
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    if (PMUCFG_FEATURE_ENABLED(PMU_PSI_CTRL_PSTATE) &&
        PSI_IS_CTRL_SUPPORTED(RM_PMU_PSI_CTRL_ID_PSTATE))
    {
        status = psiPstateDisallow(pParams->bDisallow);
    }

    return status;
}
