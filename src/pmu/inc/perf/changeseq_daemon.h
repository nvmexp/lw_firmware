/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    changeseq_daemon.h
 * @brief   Common perf daemon change sequence defines.
 */

#ifndef CHANGESEQ_DAEMON_H
#define CHANGESEQ_DAEMON_H

#include "g_changeseq_daemon.h"

#ifndef G_CHANGESEQ_DAEMON_H
#define G_CHANGESEQ_DAEMON_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "perf/changeseq.h"

/* ------------------------ Forward Definitions ----------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @brief Begins profiling a section of code.
 * @memberof CHANGE_SEQ
 * @private
 *
 * Subtracts out the current timestamp (ns) from the provided counter value.
 *
 * @param[out]  pTimens    Pointer to 64-bit counter value in ns
 *
 * @note Calls lw64Sub_MOD because we require modulo behaviour on underflow (CERT-C INT30-C-EX1).
 * In cases where pTimens is small, the subtraction will yield a number <0.
 * It will wrap around to a high positive number. At some later time, PERF_CHANGE_SEQ_PROFILE_END
 * will be called and the addition will overflow from the high number back into the following value:
 *
 * __timensResult += end_time - start_time
 *
 */
#define PERF_CHANGE_SEQ_PROFILE_BEGIN(pTimens)                                \
do {                                                                          \
    FLCN_TIMESTAMP  __timensLwrr;                                             \
    LwU64           __timensResult;                                           \
                                                                              \
    osPTimerTimeNsLwrrentGet(&__timensLwrr);                                  \
                                                                              \
    LwU64_ALIGN32_UNPACK(&__timensResult, (pTimens));                         \
    lw64Sub_MOD(&__timensResult, &__timensResult, &__timensLwrr.data);        \
    LwU64_ALIGN32_PACK((pTimens), &__timensResult);                           \
} while (LW_FALSE)

/*!
 * @brief Ends profiling a section of code.
 * @memberof CHANGE_SEQ
 * @private
 *
 * Adds the current timestamp (ns) to the provided counter value.
 *
 * @param[out]  pTimens    Pointer to 64-bit counter value in ns
 *
 * @note Calls lw64Add_MOD because we require modulo behaviour on overflow (CERT-C INT30-C-EX1).
 * In cases where pTimens is small, the subtraction in PERF_CHANGE_SEQ_PROFILE_END will yield a number <0.
 * It will wrap around to a high positive number. At some later time, PERF_CHANGE_SEQ_PROFILE_END
 * will be called and the addition will overflow from the high number back into the following value:
 *
 * __timensResult += end_time - start_time
 */
#define PERF_CHANGE_SEQ_PROFILE_END(pTimens)                                  \
do {                                                                          \
    FLCN_TIMESTAMP  __timensLwrr;                                             \
    LwU64           __timensResult;                                           \
                                                                              \
    osPTimerTimeNsLwrrentGet(&__timensLwrr);                                  \
                                                                              \
    LwU64_ALIGN32_UNPACK(&__timensResult, (pTimens));                         \
    lw64Add_MOD(&__timensResult, &__timensResult, &__timensLwrr.data);        \
    LwU64_ALIGN32_PACK((pTimens), &__timensResult);                           \
} while (LW_FALSE)

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * @brief Initialize and build the perf change sequence script from input
 * current perf change request.
 *
 * @param[in]   pChangeSeq      CHANGE_SEQ pointer
 * @param[in]   pScript         CHANGE_SEQ_SCRIPT pointer
 * @param[in]   pChangeLwrr     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE pointer
 * @param[in]   pChangeLast     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE pointer
 *
 * @return FLCN_OK if the change sequencer script step was successfully
 *         initialized; an implementation specific error code otherwise.
 */
#define PerfDaemonChangeSeqScriptInit(fname) FLCN_STATUS (fname)(CHANGE_SEQ *pChangeSeq, CHANGE_SEQ_SCRIPT *pScript, LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeLwrr, LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeLast)

/*!
 * @brief Builds specific perf change sequence script step.
 *
 * @param[in]  pChangeSeq       CHANGE_SEQ pointer.
 * @param[in]  pScript          Pointer to CHANGE_SEQ_SCRIPT buffer.
 * @param[in]  pChangeLwrr      Pointer to current LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE.
 * @param[in]  pChangeLast      Pointer to last completed LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE.
 *
 * @return FLCN_OK if the change sequencer script step was successfully built;
 *         an implementation specific error code otherwise.
 */
#define PerfDaemonChangeSeqScriptBuildStep(fname) FLCN_STATUS (fname)(CHANGE_SEQ *pChangeSeq, CHANGE_SEQ_SCRIPT *pScript, LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeLwrr, LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeLast)

/*!
 * @brief Exelwtes specific perf change sequence script step.
 *
 * @param[in]  pChangeSeq       CHANGE_SEQ pointer.
 * @param[in]  pSuper           Pointer to LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_SUPER
 *
 * @return FLCN_OK if the change sequence script step was succesfully exelwted;
 *         an implementation specific error code otherwise.
 */
#define PerfDaemonChangeSeqScriptExelwteStep(fname) FLCN_STATUS (fname)(CHANGE_SEQ *pChangeSeq, LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_SUPER *pStepSuper)

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
FLCN_STATUS     perfDaemonChangeSeqScriptExelwte(CHANGE_SEQ *pChangeSeq)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeq", "perfDaemonChangeSeqScriptExelwte");

mockable FLCN_STATUS     perfDaemonChangeSeqScriptStepInsert(CHANGE_SEQ *pChangeSeq, CHANGE_SEQ_SCRIPT *pScript, LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_SUPER *pSuper)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeq", "perfDaemonChangeSeqScriptStepInsert");

LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_HEADER *
                perfDaemonChangeSeqRead_HEADER(CHANGE_SEQ *pChangeSeq, CHANGE_SEQ_SCRIPT *pScript)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeq", "perfDaemonChangeSeqRead_HEADER");

FLCN_STATUS     perfDaemonChangeSeqFlush_HEADER(CHANGE_SEQ *pChangeSeq, CHANGE_SEQ_SCRIPT *pScript)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeq", "perfDaemonChangeSeqFlush_HEADER");

mockable FLCN_STATUS     perfDaemonChangeSeqRead_STEP(CHANGE_SEQ *pChangeSeq, CHANGE_SEQ_SCRIPT *pScript, LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_SUPER **pSuper, LwU8 lwrStepIndex, LwBool bDmaReadRequired)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeq", "perfDaemonChangeSeqRead_STEP");

mockable FLCN_STATUS     perfDaemonChangeSeqFlush_STEP(CHANGE_SEQ *pChangeSeq, CHANGE_SEQ_SCRIPT *pScript, LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_SUPER *pSuper, LwU8 lwrStepIndex, LwBool bDmaWriteRequired)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeq", "perfDaemonChangeSeqFlush_STEP");

mockable FLCN_STATUS     perfDaemonChangeSeqFlush_CHANGE(CHANGE_SEQ *pChangeSeq, CHANGE_SEQ_SCRIPT *pScript)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeq", "perfDaemonChangeSeqFlush_CHANGE");

mockable FLCN_STATUS     perfDaemonChangeSeqValidateChange(CHANGE_SEQ *pChangeSeq, LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeLwrr)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeq", "perfDaemonChangeSeqValidateChange");

PerfDaemonChangeSeqScriptExelwteStep   (perfDaemonChangeSeqScriptExelwteStep_SUPER)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeq", "perfDaemonChangeSeqScriptExelwteStep_SUPER");

FLCN_STATUS perfDaemonChangeSeqFireNotifications(CHANGE_SEQ *pChangeSeq, RM_PMU_PERF_CHANGE_SEQ_PMU_NOTIFICATION_DATA *pData)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeq", "perfDaemonChangeSeqFireNotifications");

FLCN_STATUS perfDaemonChangeSeqScriptBuildAndInsert(CHANGE_SEQ *pChangeSeq, CHANGE_SEQ_SCRIPT *pScript, LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeLwrr, LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeLast, LW2080_CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID stepId)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeq", "perfDaemonChangeSeqScriptBuildAndInsert");

/* ------------------------ Exported for Unit Testing ----------------------- */
FLCN_STATUS  perfDaemonChangeSeqScriptExelwteAllSteps(CHANGE_SEQ *pChangeSeq, CHANGE_SEQ_SCRIPT *pScript)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeq", "perfDaemonChangeSeqScriptExelwteAllSteps");

/* ------------------------ Include Derived Types --------------------------- */
#include "perf/changeseqscriptstep_bif.h"
#include "perf/changeseqscriptstep_change.h"
#include "perf/changeseqscriptstep_lpwr.h"
#include "perf/changeseqscriptstep_pstate.h"
#include "perf/changeseqscriptstep_volt.h"
#include "perf/changeseqscriptstep_mem_tune.h"
#include "perf/changeseq_lpwr_daemon.h"
#include "perf/35/changeseq_35_daemon.h"

#endif // G_CHANGESEQ_DAEMON_H
#endif // CHANGESEQ_DAEMON_H
