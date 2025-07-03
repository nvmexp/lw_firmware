/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_gc6.c
 * @brief  PMU PG GC6 related functions.
 */

/* ------------------------- System Includes ------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes -------------------------- */
#include "pmu_gc6.h"
#include "pmu_objpg.h"

#include "cmdmgmt/cmdmgmt.h"
#include "g_pmurmifrpc.h"

/* ------------------------- Prototypes -------------------------------------*/
/* ------------------------- Private Functions ----------------------------- */


#if PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_MULTI_FAN)
/*
 * @brief Attach FAN related overlay for RTD3 entry
 *        There is no overlay detach function as this is specifically for
 *        RTD3/GC6 entry purpose. DMA will be suspended during the entry so
 *        There is no way for doing DMA to detach anyway.
 * @param[in] void
 */
void
pgGc6SmartFanOvlAttachAndLoad(void)
{
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_PINNED_OVL_DESC_BUILD(_IMEM, _ATTACH, _LS, libPwm)
        OSTASK_PINNED_OVL_DESC_BUILD(_IMEM, _ATTACH, _LS, libFxpBasic)
        OSTASK_PINNED_OVL_DESC_BUILD(_IMEM, _ATTACH, _LS, thermLibFanCommon)
        OSTASK_PINNED_OVL_DESC_BUILD(_IMEM, _ATTACH, _LS, rtd3Entry)
        OSTASK_PINNED_OVL_DESC_BUILD(_IMEM, _ATTACH, _LS, lpwrLoadin)
    };
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
}
#endif // PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_MULTI_FAN)

/*
 * @brief Pre-RTD3 entry configuration
 * @param[in]  gc6DetachMask The GC6 detach mask from RM
 *
 * @return void
 */
void
pgGc6RTD3PreEntryConfig(LwU16 gc6DetachMask)
{
    // Check for configuration needed to support RTD3
    pgGc6PreInitRtd3Entry_HAL();

    return;
}

/*
 * @brief Perf logging if it's enabled
 * @param[in]  gc6DetachMask The GC6 detach mask from RM
 *
 * @return void
 */
void
pgGc6LoggingStats(LwU16 gc6DetachMask)
{
    //
    // Write Devinit and Bootstrap Stats to FB
    // Write GC5 Stats to FB - TODO
    //
    if ((PmuGc6.pStats != NULL) &&
        (FLD_TEST_DRF(_RM_PMU_GC6, _DETACH_MASK,
                      _GC6_STATS, _ENABLE, gc6DetachMask)) &&
        (PmuGc6.pCtx->bGC6StatsEnabled))
    {
        PMU_HALT_COND(dmaWrite(PmuGc6.pStats, &PmuGc6.pCtx->exitStats,
                               0, sizeof(RM_PMU_GC6_EXIT_STATS)) == FLCN_OK);
    }
}

/*
 * @brief Notify RM that PMU is detached so no traffic between RM/PMU after this function
 *
 * @return void
 */
void
pgGc6DetachedMsgSend(void)
{
    PMU_RM_RPC_STRUCT_LPWR_PG_ASYNC_CMD_RESP rpc;
    FLCN_STATUS                              status;

    //
    // Inform the RM that the PMU is now detached.
    // Send the asynchronous response message
    //
    rpc.ctrlId = 0;
    rpc.msgId  = RM_PMU_PG_MSG_ASYNC_CMD_PMU_DETACH;

    appTaskCriticalEnter();
    {
        lwrtosSemaphoreGive(OsCmdmgmtQueueDescriptorRM.mutex);
        // RC-TODO, properly handle return code.
        PMU_RM_RPC_EXELWTE_BLOCKING(status, LPWR, PG_ASYNC_CMD_RESP, &rpc);
    }
    appTaskCriticalExit();
}

/*
 * @brief The actions needs to be taken in the beginning of PMU detach
 *        Before PMU detach message send back to RM
 *
 * @return void
 */
void
rtd3Gc6EntryActionPrepare(RM_PMU_GC6_DETACH_DATA *pGc6Detach)
{
    //
    // Lower priv level masks when we unload.
    // It has to be here because according to Intel RTD3 spec,
    // the transition from D3Hot to D3Cold is root port to initiate L2
    // entry flow by broadcasting PME_Turn_Off message down to the devices.
    // Once root port received the PMU_TO_Ack message, it sets link to L2
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_GC6_LOWER_PMU_PRIV_LEVEL))
    {
        // Lower the priv level of secure reset so that RM can reset PMU.
        OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(libPrivSec));
        pmuDetachUnloadSequence_HAL(&Pmu, LW_TRUE);
        OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(libPrivSec));
    }
}

/*
 * @brief The actions needs to be taken in the beginning of PMU detach
 *        After PMU detach message send back to RM and
 *        before waiting PCIE link L2
 *
 * @return void
 */
void
rtd3Gc6EntryActionBeforeLinkL2(RM_PMU_GC6_DETACH_DATA *pGc6Detach)
{
}

/*
 * @brief The actions needs to be taken in the beginning of PMU detach
 *        After PCIE link is L2, i.e. D3Cold
 *
 * @return void
 */
void
rtd3Gc6EntryActionAfterLinkL2(RM_PMU_GC6_DETACH_DATA *pGc6Detach)
{
    LwU16 gc6DetachMask = pGc6Detach->gc6DetachMask;

    //
    // compile out before we concluded on RTD3 fan control approach
    // tracking bug#200597644
    //
#ifndef UPROC_RISCV
    //
    // In Turing and later which supports dual fan, need to turn them off
    // after link l2. i.e. D3Cold
    //
#if PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_MULTI_FAN)
    pgGc6TurnFanOff_HAL();
#endif // PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_MULTI_FAN)

#endif //#ifndef UPROC_RISCV

    // Sanity check and log error if the current mode is not supported
    pgGc6LogErrorStatus(pgGc6EntryCtrlSanityCheck_HAL(&Pg, FLD_TEST_DRF(_RM_PMU_GC6, _DETACH_MASK,
                                                                        _RTD3_GC6_ENTRY, _TRUE, gc6DetachMask),
                                                           FLD_TEST_DRF(_RM_PMU_GC6, _DETACH_MASK,
                                                                        _FAST_GC6_ENTRY, _TRUE, gc6DetachMask)));

    // Sanity check and log error if there is PLM error for triggering island
    pgGc6LogErrorStatus(pgGc6EntryPlmSanityCheck_HAL());
}

/*
 * @brief The actions needs to be taken in the beginning of PMU detach
 *        After PCIE link is L2 and right before island trigger
 *
 * @return void
 */
void
rtd3Gc6EntryActionBeforeIslandTrigger(RM_PMU_GC6_DETACH_DATA *pGc6Detach)
{
}

/*!
 * @brief Accumulate the GC6 entry status mask for debug purpose
 *        Any error detected in GC6 entry is fatal and we will log
 *        the error and to continue..
 *
 *        In order to log all the error encountered, need to transfer the
 *        status code into mask so we can have multiple error log.
 *
 * @param[in]   status    the checkpoint to update in the scratch register
 */
void
pgGc6LogErrorStatus(LwU32 status)
{
    if (GCX_GC6_OK != status)
    {
        // The incremental write is to log the following errors.
        PmuGc6.gc6EntryStatusMask |= BIT(status);
    }
}

