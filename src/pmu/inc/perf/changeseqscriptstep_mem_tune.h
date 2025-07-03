/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    changeseqscriptstep_mem_tune.h
 * @brief   Interface for handling memory settings tuning steps by the change sequencer.
 */

#ifndef CHANGESEQSCRIPTSTEP_MEM_TUNE_H
#define CHANGESEQSCRIPTSTEP_MEM_TUNE_H

#include "g_changeseqscriptstep_mem_tune.h"

#ifndef G_CHANGESEQSCRIPTSTEP_MEM_TUNE_H
#define G_CHANGESEQSCRIPTSTEP_MEM_TUNE_H

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
mockable PerfDaemonChangeSeqScriptBuildStep      (perfDaemonChangeSeqScriptBuildStep_MEM_TUNE)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeq", "perfDaemonChangeSeqScriptBuildStep_MEM_TUNE");
mockable PerfDaemonChangeSeqScriptExelwteStep    (perfDaemonChangeSeqScriptExelwteStep_MEM_TUNE)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeq", "perfDaemonChangeSeqScriptExelwteStep_MEM_TUNE");

#endif // g-CHANGESEQ_SCRIPT_STEP_MEM_TUNE_H
#endif // CHANGESEQ_SCRIPT_STEP_MEM_TUNE_H
