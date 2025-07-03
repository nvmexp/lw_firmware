/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    changeseq_lpwr_daemon.h
 * @brief   Common perf daemon change sequence LPWR defines.
 */

#ifndef CHANGESEQ_LPWR_DAEMON_H
#define CHANGESEQ_LPWR_DAEMON_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "perf/changeseq_daemon.h"

extern LwU32 g_CHSEQ_LPWR_DEBUG[18];

/* ------------------------ Forward Definitions ----------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @brief Set of overlays required for change sequencer LPWR Scripts DMEM.
 *        Client will be required to attach this to update change sequencer
 *        step id exclusion mask.
 *
 * @memberof CHANG_SEQ
 * @public
 *
 * Expected to be used with @ref OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER() and
 * @ref OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT().
 */
#define CHANGE_SEQ_OVERLAYS_LPWR_SCRIPTS_DMEM                                 \
    OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfDaemon)                         \
    OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfChangeSeqLpwr)

/*!
 * @brief Set of overlays required for change sequencer LPWR Scripts exelwtion
 * (both IMEM and DMEM).
 * @memberof CHANG_SEQ
 * @public
 *
 * Expected to be used with @ref OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER() and
 * @ref OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT().
 */
#define CHANGE_SEQ_OVERLAYS_LPWR_SCRIPTS                                      \
    CHANGE_SEQ_OVERLAYS_LPWR_SCRIPTS_DMEM                                     \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfDaemonChangeSeqLpwr)

/*!
 * @brief Helper macro to prepare perf daemon task for VF switch exelwtion.
 *
 * @memberof CHANG_SEQ
 * @private
 *
 * Exepected to be used by perf daemon task upon receiving new VF switch request.
 */
#define perfDaemonChangeSeqPrepareForVfSwitch()                                     \
    do {                                                                            \
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_LPWR))                   \
            {                                                                       \
                OSTASK_OVL_DESC ovlDescList[] = {                                   \
                    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfDaemonChangeSeqLpwr)  \
                    OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfChangeSeqLpwr)        \
                    OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, lpwr)                     \
                    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libLpwr)                  \
                };                                                                  \
                                                                                    \
                OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);                \
                {                                                                   \
                    /* 1. Trigger GPC-RG Disable */                                 \
                    pgCtrlDisallowExt(RM_PMU_LPWR_CTRL_ID_GR_RG);                   \
                    /* 2. Grab GPC-RG <-> VF Switch sync semaphore */               \
                    grRgSemaphoreTakeWaitForever();                                 \
                    /* 3. Execute post processing of GPC-RG script inline. */       \
                    (void)perfDaemonChangeSeqLpwrPostScriptExelwte(                 \
                                LW2080_CTRL_PERF_CHANGE_SEQ_SCRIPT_ID_LPWR_POST);   \
                }                                                                   \
                OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);                 \
            }                                                                       \
    } while (LW_FALSE)

/*!
 * @brief Helper macro to unprepare perf daemon task for VF switch exelwtion.
 *
 * @memberof CHANG_SEQ
 * @private
 *
 * Exepected to be used by perf daemon task upon completion of VF switch request.
 */
#define perfDaemonChangeSeqUnprepareForVfSwitch()                                   \
    do {                                                                            \
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_LPWR))                   \
            {                                                                       \
                OSTASK_OVL_DESC ovlDescList[] = {                                   \
                    OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, lpwr)                     \
                    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libLpwr)                  \
                };                                                                  \
                                                                                    \
                OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);                \
                {                                                                   \
                    /* 1. Release GPC-RG <-> VF Switch sync semaphore */            \
                    grRgSemaphoreGive();                                            \
                    /* 2. Trigger GPC-RG Enable */                                  \
                    pgCtrlAllowExt(RM_PMU_LPWR_CTRL_ID_GR_RG);                      \
                }                                                                   \
                OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);                 \
            }                                                                       \
    } while (LW_FALSE)

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/* ------------------------ Include Derived Types --------------------------- */
FLCN_STATUS perfDaemonChangeSeqLpwrScriptExelwte(LwU8 lpwrScriptId)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeqLpwr", "perfDaemonChangeSeqLpwrScriptExelwte");
FLCN_STATUS perfDaemonChangeSeqLpwrPostScriptExelwte(LwU8 lpwrScriptId)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeqLpwr", "perfDaemonChangeSeqLpwrPostScriptExelwte");

FLCN_STATUS perfDaemonChangeSeqScriptExelwteStep_CLK_DEINIT(void)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeqLpwr", "perfDaemonChangeSeqScriptExelwteStep_CLK_DEINIT");
FLCN_STATUS perfDaemonChangeSeqScriptExelwteStep_CLK_INIT(void)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeqLpwr", "perfDaemonChangeSeqScriptExelwteStep_CLK_INIT");
FLCN_STATUS perfDaemonChangeSeqScriptExelwteStep_CLK_RESTORE(void)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeqLpwr", "perfDaemonChangeSeqScriptExelwteStep_CLK_RESTORE");

#endif // CHANGESEQ_LPWR_DAEMON_H
