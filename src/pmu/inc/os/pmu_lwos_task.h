/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_LWOS_TASK_H
#define PMU_LWOS_TASK_H

/*!
 * @file    pmu_lwos_task.h
 * @brief   PMU hooks for LWOS-task functionality.
 */

/* ------------------------ System includes --------------------------------- */
#include "pmusw.h"
#include "lwostask.h"

/* ------------------------ Application includes ---------------------------- */
/* ------------------------ Defines ----------------------------------------- */
/*!
 * @copydoc LWOS_TASK_CMD_BUFFER
 *
 * @note    @ref LWOS_TASK_CMD_BUFFER with a fixed first argument of PMU
 */
#define PMU_TASK_CMD_BUFFER(task)                                              \
    LWOS_TASK_CMD_BUFFER(PMU, task)

/*!
 * @copydoc LWOS_TASK_CMD_BUFFER_DEFINE
 *
 * @param[in]   task    Name of the task
 * @param[in]   section Section in which to place the buffer
 *
 * @note    @ref LWOS_TASK_CMD_BUFFER_DEFINE with a fixed first argument of PMU
 */
#define PMU_TASK_CMD_BUFFER_DEFINE(task, section)                              \
    LWOS_TASK_CMD_BUFFER_DEFINE(PMU, task, section)

/*!
 * @copydoc LWOS_TASK_CMD_BUFFER_DEFINE_STRUCT_SIZE
 *
 * @param[in]   task    Name of the task
 * @param[in]   section Section in which to place the buffer
 *
 * @note    @ref LWOS_TASK_CMD_BUFFER_DEFINE_STRUCT_SIZE with a fixed first argument of PMU
 */
#define PMU_TASK_CMD_BUFFER_DEFINE_SCRATCH_SIZE(task, section)                 \
    LWOS_TASK_CMD_BUFFER_DEFINE_SCRATCH_SIZE(PMU, task, section)

#if PMUCFG_FEATURE_ENABLED(PMU_CMDMGMT_PER_TASK_COMMAND_BUFFER)

/*!
 * @copydoc LWOS_TASK_CMD_BUFFER_PTRS_GET
 *
 * @note    @ref LWOS_TASK_CMD_BUFFER_PTRS_GET with a fixed first argument of
 *          PMU
 */
#define PMU_TASK_CMD_BUFFER_PTRS_GET(task)                                     \
    LWOS_TASK_CMD_BUFFER_PTRS_GET(PMU, task)

#else // PMUCFG_FEATURE_ENABLED(PMU_CMDMGMT_PER_TASK_COMMAND_BUFFER)

/*!
 * @copydoc LWOS_TASK_CMD_BUFFER_PTRS_GET
 *
 * @note    Always returns @ref LWOS_TASK_CMD_BUFFER_ILWALID for when the
 *          feature is disabled.
 *
 * @note    The construct of casting @ref PMU_TASK_CMD_BUFFER to void is to
 *          eliminate unused static variable errors for the per-task buffer.
 *          The buffers are declared static and should generally only be
 *          accessed through this macro, so even when the feature is disabled,
 *          this macro is made to reference the name so that the variable
 *          doesn't go unreferenced.
 */
#define PMU_TASK_CMD_BUFFER_PTRS_GET(name)                                     \
    ((void)PMU_TASK_CMD_BUFFER(name), LWOS_TASK_CMD_BUFFER_ILWALID)

#endif // PMUCFG_FEATURE_ENABLED(PMU_CMDMGMT_PER_TASK_COMMAND_BUFFER)

/* ------------------------ Types definitions ------------------------------- */
/* ------------------------ External definitions ---------------------------- */
/* ------------------------ Static variables -------------------------------- */
/* ------------------------ Function Prototypes ----------------------------- */

#endif // PMU_LWOS_TASK_H
