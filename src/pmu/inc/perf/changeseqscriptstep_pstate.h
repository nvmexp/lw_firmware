/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    changeseqscriptstep_pstate.h
 * @brief   Interface for handling PSTATE-based steps by the change sequencer.
 */

#ifndef CHANGESEQSCRIPTSTEP_PSTATE_H
#define CHANGESEQSCRIPTSTEP_PSTATE_H

#include "g_changeseqscriptstep_pstate.h"

#ifndef G_CHANGESEQSCRIPTSTEP_PSTATE_H
#define G_CHANGESEQSCRIPTSTEP_PSTATE_H

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "perf/changeseq_daemon.h"

/* ------------------------ Forward Definitions ----------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
mockable PerfDaemonChangeSeqScriptBuildStep      (perfDaemonChangeSeqScriptBuildStep_PSTATE)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeq", "perfDaemonChangeSeqScriptBuildStep_PSTATE");
mockable PerfDaemonChangeSeqScriptExelwteStep    (perfDaemonChangeSeqScriptExelwteStep_PRE_PSTATE_PMU)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeq", "perfDaemonChangeSeqScriptExelwteStep_PRE_PSTATE_PMU");
mockable PerfDaemonChangeSeqScriptExelwteStep    (perfDaemonChangeSeqScriptExelwteStep_POST_PSTATE_PMU)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeq", "perfDaemonChangeSeqScriptExelwteStep_POST_PSTATE_PMU");

#endif // G_CHANGESEQ_SCRIPT_STEP_PSTATE_H
#endif // CHANGESEQ_SCRIPT_STEP_PSTATE_H
