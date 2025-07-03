/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef DPU_TASK_H
#define DPU_TASK_H

#include "lwrtos.h"
#include "rmdpucmdif.h"

#include "config/g_tasks.h"
#include "config/dpu-config.h"
#include "config/g_dpu-config_private.h"

/* ------------------------- Defines --------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
int main(int argc, char **ppArgv)
    GCC_ATTRIB_SECTION("imem_resident", "main");
FLCN_STATUS InitDpuApp(RM_DPU_CMD_LINE_ARGS *)
    GCC_ATTRIB_SECTION("imem_init", "InitDpuAppp");

/* ------------------------ External definitions --------------------------- */
extern RM_DPU_CMD_LINE_ARGS DpuInitArgs;

extern LwrtosTaskHandle OsTaskMscgWithFrl;
extern LwrtosTaskHandle OsTaskVrr;

extern LwrtosQueueHandle DpuMgmtCmdDispQueue;
extern LwrtosQueueHandle VrrQueue;
extern LwrtosQueueHandle Hdcp22WiredQueue;
extern LwrtosQueueHandle HdcpQueue;
extern LwrtosQueueHandle ScanoutLoggingQueue;
extern LwrtosQueueHandle MscgWithFrlQueue;
extern LwrtosQueueHandle Disp2QWkrThd;

#endif // DPU_TASK_H

