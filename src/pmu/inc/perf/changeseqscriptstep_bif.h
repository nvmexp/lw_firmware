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
 * @file    changeseqscriptstep_bif.h
 * @brief   Interface for handling BIF-based steps by the change sequencer.
 */

#ifndef CHANGESEQSCRIPTSTEP_BIF_H
#define CHANGESEQSCRIPTSTEP_BIF_H

#include "g_changeseqscriptstep_bif.h"

#ifndef G_CHANGESEQSCRIPTSTEP_BIF_H
#define G_CHANGESEQSCRIPTSTEP_BIF_H

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
mockable PerfDaemonChangeSeqScriptBuildStep      (perfDaemonChangeSeqScriptBuildStep_BIF)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeq", "perfDaemonChangeSeqScriptBuildStep_BIF");
mockable PerfDaemonChangeSeqScriptExelwteStep    (perfDaemonChangeSeqScriptExelwteStep_BIF)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeq", "perfDaemonChangeSeqScriptExelwteStep_BIF");

#endif // g-CHANGESEQ_SCRIPT_STEP_BIF_H
#endif // CHANGESEQ_SCRIPT_STEP_BIF_H
