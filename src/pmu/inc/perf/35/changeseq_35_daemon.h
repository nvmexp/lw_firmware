/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    changeseq_35_daemon.h
 * @brief   Common perf daemon change sequence 35 defines.
 */

#ifndef CHANGESEQ_35_DAEMON_H
#define CHANGESEQ_35_DAEMON_H

#include "g_changeseq_35_daemon.h"

#ifndef G_CHANGESEQ_35_DAEMON_H
#define G_CHANGESEQ_35_DAEMON_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "perf/changeseq_daemon.h"

/* ------------------------ Forward Definitions ----------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
PerfDaemonChangeSeqScriptInit          (perfDaemonChangeSeqScriptInit_35)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeq", "perfDaemonChangeSeqScriptInit_35");
mockable PerfDaemonChangeSeqScriptExelwteStep   (perfDaemonChangeSeqScriptExelwteStep_35)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeq", "perfDaemonChangeSeqScriptExelwteStep_35");

/* ------------------------ Include Derived Types --------------------------- */
#include "perf/35/changeseqscriptstep_clk.h"
#include "perf/35/changeseqscriptstep_clkmon.h"
#include "perf/35/changeseqscriptstep_nafllclk.h"

#endif // G_CHANGESEQ_35_DAEMON_H
#endif // CHANGESEQ_35_DAEMON_H
