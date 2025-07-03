/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2007-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CMDMGMT_DISPATCH_H
#define CMDMGMT_DISPATCH_H

/*!
 * @file cmdmgmt_dispatch.h
 *
 * Provides the definitions and interfaces for dispatching commands to tasks
 */

#include "g_cmdmgmt_dispatch.h"

#ifndef G_CMDMGMT_DISPATCH_H
#define G_CMDMGMT_DISPATCH_H

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "pmu_oslayer.h"

/* ------------------------- Includes --------------------------------------- */
#include "cmdmgmt/cmdmgmt.h"
#include "pmu_didle.h"
#include "pmu_disp.h"
#include "pmu_i2capi.h"
#include "pmu_objbif.h"
#include "pmu_objlowlatency.h"
#include "pmu_objpg.h"
#include "pmu_objlpwr.h"
#include "pmu_objseq.h"
#include "task_i2c.h"
#if (PMUCFG_FEATURE_ENABLED(PMUTASK_ACR))
#include "acr/task_acr.h"
#endif
#include "task_spi.h"
#include "task_therm.h"
#include "task_perf.h"
#include "task_perf_cf.h"
#include "task_perf_daemon.h"
#include "task_watchdog.h"

/* ------------------------- External Definitions --------------------------- */

/* ------------------------- Type Definitions ------------------------------- */

/* ------------------------- Macros and Defines ----------------------------- */

/* ------------------------- Prototypes ------------------------------------- */
mockable FLCN_STATUS cmdmgmtExtCmdDispatch(DISP2UNIT_RM_RPC *pRequest);

/* ------------------------- Global Variables ------------------------------ */

#endif // G_CMDMGMT_DISPATCH_H
#endif // CMDMGMT_DISPATCH_H
