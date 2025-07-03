/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PERF_DAEMON_FBFLCN_H
#define PERF_DAEMON_FBFLCN_H

/*!
 * @file    perf_daemon_fbfln.h
 * @copydoc perf_daemon_fbfln.c
 */

/* ------------------------ System Includes -------------------------------- */
#include "main.h"

/* ------------------------ Application Includes --------------------------- */
/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
FLCN_STATUS perfDaemonProcessMclkSwitchEvent(DISPATCH_PERF_DAEMON *pRequest)
    GCC_ATTRIB_SECTION("imem_perfDaemon", "perfDaemonProcessMclkSwitchEvent");

#endif // PERF_DAEMON_FBFLCN_H
