/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_RTOS_QUEUES
#define PMU_RTOS_QUEUES

/*!
 * @file    pmu_rtos_queues.h
 * @brief   Provides declarations needed for interacting with the PMU's RTOS
 *          queues
 */

/* ------------------------ System includes -------------------------------- */
#include "rmpmucmdif.h"

/* ------------------------ Forward Declartion ----------------------------- */
/* ------------------------ Application includes --------------------------- */
#include "config/g_pmu-config_private.h"

/* ------------------------ Defines ---------------------------------------- */
#define PMU_QUEUE_ID_CMDMGMT                0x00U
#define PMU_QUEUE_ID_GCX                    0x01U
#define PMU_QUEUE_ID_LPWR                   0x02U
#define PMU_QUEUE_ID_I2C                    0x03U
#define PMU_QUEUE_ID_SEQ                    0x04U
#define PMU_QUEUE_ID_PCMEVT                 0x05U
#define PMU_QUEUE_ID_PMGR                   0x06U
#define PMU_QUEUE_ID_PERFMON                0x07U
#define PMU_QUEUE_ID_DISP                   0x08U
#define PMU_QUEUE_ID_THERM                  0x09U
#define PMU_QUEUE_ID_HDCP                   0x0AU
#define PMU_QUEUE_ID_ACR                    0x0BU
#define PMU_QUEUE_ID_SPI                    0x0LW
#define PMU_QUEUE_ID_PERF                   0x0DU
#define PMU_QUEUE_ID_LOWLATENCY             0x0EU
#define PMU_QUEUE_ID_PERF_DAEMON            0x0FU
#define PMU_QUEUE_ID_BIF                    0x10U
#define PMU_QUEUE_ID_PERF_CF                0x11U
#define PMU_QUEUE_ID_LPWR_LP                0x12U
#define PMU_QUEUE_ID_NNE                    0x13U
#define PMU_QUEUE_ID__LAST_TASK_QUEUE       PMU_QUEUE_ID_NNE
#define PMU_QUEUE_ID_PERF_DAEMON_RESPONSE   (PMU_QUEUE_ID__LAST_TASK_QUEUE + 1U)
#define PMU_QUEUE_ID__LAST_RESPONSE_QUEUE   PMU_QUEUE_ID_PERF_DAEMON_RESPONSE
#define PMU_QUEUE_ID__COUNT                 (PMU_QUEUE_ID__LAST_RESPONSE_QUEUE + 1U)

/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/*!
 * @brief   Maintains a mapping from queue identifiers to their respective
 *          queue handle. Indexed by queue IDs.
 */
extern LwrtosQueueHandle UcodeQueueIdToQueueHandleMap[];

/*!
 * @brief   For queues that are directly associated with a task, maintains a
 *          mapping from the queue ID to the respective task ID. Indexed by
            queue IDs.
 */
extern LwU8              UcodeQueueIdToTaskIdMap[];

/*!
 * @breif   Used externally to the PMU app (in the RTOS) to indicate the last
 *          queue ID corresponding to a task
 */
extern const LwU8        UcodeQueueIdLastTaskQueue;

extern LwrtosQueueHandle DpAuxAckQueue;

/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */

#endif // PMU_RTOS_QUEUES

