/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2007-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef MAIN_H
#define MAIN_H

/*!
 * @file main.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "rmpmucmdif.h"
#include "pmu_rtos_queues.h"

/* ------------------------ Application includes ---------------------------- */
#include "config/g_pmu-config_private.h"

/* ------------------------ Public Functions -------------------------------- */
/* ------------------------ External definitions ---------------------------- */
extern RM_PMU_CMD_LINE_ARGS PmuInitArgs;

extern LwrtosTaskHandle OsTaskIdle;
extern LwrtosTaskHandle OsTaskCmdMgmt;
extern LwrtosTaskHandle OsTaskPcm;
extern LwrtosTaskHandle OsTaskPcmEvent;
extern LwrtosTaskHandle OsTaskLpwr;
extern LwrtosTaskHandle OsTaskLpwrLp;
extern LwrtosTaskHandle OsTaskSeq;
extern LwrtosTaskHandle OsTaskI2c;
extern LwrtosTaskHandle OsTaskGCX;
extern LwrtosTaskHandle OsTaskTherm;
extern LwrtosTaskHandle OsTaskDisp;
extern LwrtosTaskHandle OsTaskHdcp;
extern LwrtosTaskHandle OsTaskPmgr;
extern LwrtosTaskHandle OsTaskAcr;
extern LwrtosTaskHandle OsTaskSpi;
extern LwrtosTaskHandle OsTaskPerf;
extern LwrtosTaskHandle OsTaskPerfMon;
extern LwrtosTaskHandle OsTaskPerfDaemon;
extern LwrtosTaskHandle OsTaskBif;
extern LwrtosTaskHandle OsTaskPerfCf;
extern LwrtosTaskHandle OsTaskWatchdog;
extern LwrtosTaskHandle OsTaskLowlatency;
extern LwrtosTaskHandle OsTaskNne;

#endif // MAIN_H

