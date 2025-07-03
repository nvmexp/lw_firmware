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
 * @file    changeseqscriptstep_nafllclk.h
 * @brief   Interface for handling NAFLL_CLK-based steps by the change sequencer.
 */

#ifndef CHANGESEQSCRIPTSTEP_NAFLLCLK_H
#define CHANGESEQSCRIPTSTEP_NAFLLCLK_H

#include "g_changeseqscriptstep_nafllclk.h"

#ifndef G_CHANGESEQSCRIPTSTEP_NAFLLCLK_H
#define G_CHANGESEQSCRIPTSTEP_NAFLLCLK_H

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
mockable PerfDaemonChangeSeqScriptExelwteStep    (perfDaemonChangeSeq35ScriptExelwteStep_NAFLL_CLKS)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeq", "perfDaemonChangeSeq35ScriptExelwteStep_NAFLL_CLKS");

#endif // G_CHANGESEQSCRIPTSTEP_NAFLLCLK_H
#endif // CHANGESEQSCRIPTSTEP_NAFLLCLK_H
