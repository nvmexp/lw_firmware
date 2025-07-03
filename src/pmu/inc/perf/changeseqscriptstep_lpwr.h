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
 * @file    changeseqscriptstep_lpwr.h
 * @brief   Interface for handling LPWR-based steps by the change sequencer.
 */

#ifndef CHANGESEQSCRIPTSTEP_LPWR_H
#define CHANGESEQSCRIPTSTEP_LPWR_H

#include "g_changeseqscriptstep_lpwr.h"

#ifndef G_CHANGESEQSCRIPTSTEP_LPWR_H
#define G_CHANGESEQSCRIPTSTEP_LPWR_H

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
mockable PerfDaemonChangeSeqScriptBuildStep      (perfDaemonChangeSeqScriptBuildStep_LPWR)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeq", "perfDaemonChangeSeqScriptBuildStep_LPWR");
mockable PerfDaemonChangeSeqScriptExelwteStep    (perfDaemonChangeSeqScriptExelwteStep_LPWR)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeq", "perfDaemonChangeSeqScriptExelwteStep_LPWR");

#endif // G_CHANGESEQSCRIPTSTEP_LPWR_H
#endif // CHANGESEQSCRIPTSTEP_LPWR_H
