/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   perf_daemon.c
 * @brief  Perf daemon task Related Interfaces
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "main_init_api.h"
#include "pmu_objpmu.h"
#include "perf/perf_daemon.h"
#include "task_perf_daemon.h"

/* ------------------------- Static Function Prototypes  -------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * Construct the PERF DAEMON object.  This includes the HAL interface as well as
 * all software objects (ex. semaphores) used by the PERF DAEMON task.
 *
 * @return  FLCN_OK     On success
 */
FLCN_STATUS
constructPerfDaemon_IMPL(void)
{
    // Place holder for constructing PERF DAEMON object
    return FLCN_OK;
}

/* ------------------------- Private Functions ------------------------------ */
