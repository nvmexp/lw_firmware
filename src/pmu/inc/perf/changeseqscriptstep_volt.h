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
 * @file    changeseqscriptstep_volt.h
 * @brief   Interface for handling VOLT-based steps by the change sequencer.
 */

#ifndef CHANGESEQSCRIPTSTEP_VOLT_H
#define CHANGESEQSCRIPTSTEP_VOLT_H

#include "g_changeseqscriptstep_volt.h"

#ifndef G_CHANGESEQSCRIPTSTEP_VOLT_H
#define G_CHANGESEQSCRIPTSTEP_VOLT_H

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
mockable PerfDaemonChangeSeqScriptBuildStep      (perfDaemonChangeSeqScriptBuildStep_VOLT)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeq", "perfDaemonChangeSeqScriptBuildStep_VOLT");
mockable PerfDaemonChangeSeqScriptExelwteStep    (perfDaemonChangeSeqScriptExelwteStep_VOLT)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeq", "perfDaemonChangeSeqScriptExelwteStep_VOLT");

#endif // G_CHANGESEQSCRIPTSTEP_VOLT_H
#endif // CHANGESEQSCRIPTSTEP_VOLT_H
