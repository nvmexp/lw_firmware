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
 * @file    changeseqscriptstep_prevoltclk.h
 * @brief   Interface for handling PRE_VOLT_CLK-based steps by the change sequencer.
 */

#ifndef CHANGESEQSCRIPTSTEP_CLK_H
#define CHANGESEQSCRIPTSTEP_CLK_H

#include "g_changeseqscriptstep_clk.h"

#ifndef G_CHANGESEQSCRIPTSTEP_CLK_H
#define G_CHANGESEQSCRIPTSTEP_CLK_H

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
mockable PerfDaemonChangeSeqScriptBuildStep      (perfDaemonChangeSeq35ScriptBuildStep_PRE_VOLT_CLKS)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeq", "perfDaemonChangeSeq35ScriptBuildStep_PRE_VOLT_CLKS");
mockable PerfDaemonChangeSeqScriptBuildStep      (perfDaemonChangeSeq35ScriptBuildStep_POST_VOLT_CLKS)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeq", "perfDaemonChangeSeq35ScriptBuildStep_POST_VOLT_CLKS");
mockable PerfDaemonChangeSeqScriptExelwteStep    (perfDaemonChangeSeq35ScriptExelwteStep_PRE_VOLT_CLKS)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeq", "perfDaemonChangeSeq35ScriptExelwteStep_PRE_VOLT_CLKS");
mockable PerfDaemonChangeSeqScriptExelwteStep    (perfDaemonChangeSeq35ScriptExelwteStep_POST_VOLT_CLKS)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeq", "perfDaemonChangeSeq35ScriptExelwteStep_POST_VOLT_CLKS");

#endif // G_CHANGESEQSCRIPTSTEP_CLK_H
#endif // CHANGESEQSCRIPTSTEP_CLK_H
