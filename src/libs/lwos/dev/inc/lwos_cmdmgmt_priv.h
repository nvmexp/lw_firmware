/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LWOS_CMDMGMT_PRIV_H
#define LWOS_CMDMGMT_PRIV_H

/*!
 * @file    lwos_cmdmgmt_priv.h
 * @brief   This file contains declarations that are shared across various LWOS
 *          CmdMgmt code but which is meant to be private to the LWOS CmdMgmt
 *          code.
 */

/* ------------------------ System includes --------------------------------- */
#include "lwuproc.h"
#include "flcntypes.h"
#include "lwrtos.h"

/* ------------------------ Application includes ---------------------------- */
/* ------------------------ Defines ----------------------------------------- */
#if defined(DPU_RTOS)

    #include "dev_disp_falcon.h"
    #define OS_CMDMGMT_CMDQ_HEAD_REG(i)     LW_PDISP_FALCON_CMDQ_HEAD(i)
    #define OS_CMDMGMT_CMDQ_TAIL_REG(i)     LW_PDISP_FALCON_CMDQ_TAIL(i)

extern LwU32 DpuMgmtCmdOffset[];
    #define OS_CMDMGMT_QUEUE_START(i)       DpuMgmtCmdOffset[i]

#elif defined(PMU_RTOS)

    #include "dev_pwr_csb.h"
    #define OS_CMDMGMT_CMDQ_HEAD_REG(i)     LW_CPWR_PMU_QUEUE_HEAD(i)
    #define OS_CMDMGMT_CMDQ_TAIL_REG(i)     LW_CPWR_PMU_QUEUE_TAIL(i)

#ifndef FB_QUEUES
extern LwU32 QueueDmemQueuePtrStart[];
    #define OS_CMDMGMT_QUEUE_START(i)       QueueDmemQueuePtrStart[i]
#else
    #define OS_CMDMGMT_QUEUE_START(i)       (0U)
#endif

#elif defined(SEC2_RTOS)

    #include "dev_sec_csb.h"
    #define OS_CMDMGMT_CMDQ_HEAD_REG(i)     LW_CSEC_QUEUE_HEAD(i)
    #define OS_CMDMGMT_CMDQ_TAIL_REG(i)     LW_CSEC_QUEUE_TAIL(i)

extern LwU32 Sec2MgmtCmdOffset[];
    #define OS_CMDMGMT_QUEUE_START(i)       Sec2MgmtCmdOffset[i]

#elif defined(SOE_RTOS)

    #include "dev_soe_csb.h"
    #define OS_CMDMGMT_CMDQ_HEAD_REG(i)     LW_CSOE_QUEUE_HEAD(i)
    #define OS_CMDMGMT_CMDQ_TAIL_REG(i)     LW_CSOE_QUEUE_TAIL(i)

extern LwU32 SoeMgmtCmdOffset[];
    #define OS_CMDMGMT_QUEUE_START(i)       SoeMgmtCmdOffset[i]

#elif defined(GSPLITE_RTOS)

    #include "dev_gsp_csb.h"
    #define OS_CMDMGMT_CMDQ_HEAD_REG(i)     LW_CGSP_QUEUE_HEAD(i)
    #define OS_CMDMGMT_CMDQ_TAIL_REG(i)     LW_CGSP_QUEUE_TAIL(i)

extern LwU32 DpuMgmtCmdOffset[];
    #define OS_CMDMGMT_QUEUE_START(i)       DpuMgmtCmdOffset[i]

#elif defined(GSP_RTOS)

    // Stubbed for now.
    #define OS_CMDMGMT_CMDQ_HEAD_REG(i)     0
    #define OS_CMDMGMT_CMDQ_TAIL_REG(i)     0

    #define OS_CMDMGMT_QUEUE_START(i)       0
#else
    #error !!! Unsupported falcon !!!
#endif

/* ------------------------ External definitions ---------------------------- */
/* ------------------------ Static variables -------------------------------- */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Inline Functions -------------------------------- */

#endif // LWOS_CMDMGMT_PRIV_H
