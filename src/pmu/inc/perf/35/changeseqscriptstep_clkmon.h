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
 * @file    changeseqscriptstep_clkmon.h
 * @brief   Interface for handling CLK_MON-based steps by the change sequencer.
 */

#ifndef CHANGESEQSCRIPTSTEP_CLKMON_H
#define CHANGESEQSCRIPTSTEP_CLKMON_H

#include "g_changeseqscriptstep_clkmon.h"

#ifndef G_CHANGESEQSCRIPTSTEP_CLKMON_H
#define G_CHANGESEQSCRIPTSTEP_CLKMON_H

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
mockable PerfDaemonChangeSeqScriptBuildStep      (perfDaemonChangeSeq35ScriptBuildStep_CLK_MON)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeq", "perfDaemonChangeSeq35ScriptBuildStep_CLK_MON");
mockable PerfDaemonChangeSeqScriptExelwteStep    (perfDaemonChangeSeq35ScriptExelwteStep_CLK_MON)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeq", "perfDaemonChangeSeq35ScriptExelwteStep_CLK_MON");

#endif // G_CHANGESEQSCRIPTSTEP_CLKMON_H
#endif // CHANGESEQSCRIPTSTEP_CLKMON_H
