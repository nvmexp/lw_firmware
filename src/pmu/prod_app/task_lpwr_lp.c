/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file task_lpwr_lp.c
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "lwostimer.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objlpwr.h"
#include "main.h"
#include "main_init_api.h"
#include "cmdmgmt/cmdmgmt.h"
#include "os/pmu_lwos_task.h"

#include "pmu_i2capi.h"
#include "g_pmurpc.h"
#include "g_pmurmifrpc.h"

/* ------------------------- Prototypes ------------------------------------- */
lwrtosTASK_FUNCTION(task_lpwr_lp, pvParameters);

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/*!
 * @brief Union of all possible RPC subsurface data that can be handled by the LPWR_LP task.
 */
typedef union
{
    /*!
     * @brief Default size, as defined by configuration
     */
    LwU8 defaultSize[OSTASK_ATTRIBUTE_LPWR_LP_CMD_SCRATCH_SIZE];
} TASK_SURFACE_UNION_LPWR_LP;

/*!
 * @brief   Defines the command buffer for the LPWR_LP task.
 */
PMU_TASK_CMD_BUFFER_DEFINE_SCRATCH_SIZE(LPWR_LP, "dmem_lpwrLpCmdBuffer");

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief      Initialize the LPWR_LP Task.
 *
 * @return     FLCN_OK on success,
 * @return     descriptive error code otherwise.
 */
FLCN_STATUS
lpwrLpPreInitTask(void)
{
    FLCN_STATUS status = FLCN_OK;
    // TODO-pankumar: Revisit this queue size in coming Cls.
    LwU32 queueSize = RM_PMU_PG_QUEUE_SIZE_DEFAULT;

    //
    // 16 bins of 2^15[ns] == ~32.8[us] ending at ~524[us]
    // 16 bins of 2^18[ns] == ~262[us] last bin starting at ~4.2[ms]
    //
    OSTASK_CREATE(status, PMU, LPWR_LP);
    if (status != FLCN_OK)
    {
        goto lpwrLpPreInitTask_exit;
    }

    status = lwrtosQueueCreateOvlRes(&LWOS_QUEUE(PMU, LPWR_LP), queueSize,
                                     sizeof(DISPATCH_LPWR_LP));
    if (status != FLCN_OK)
    {
        goto lpwrLpPreInitTask_exit;
    }

    // For PState Based PSI, we need to create the I2C communication queue
    if ((PMUCFG_FEATURE_ENABLED(PMU_PSI_I2C_INTERFACE)) &&
        (PMUCFG_FEATURE_ENABLED(PMU_I2C_INT)))
    {
        status = lwrtosQueueCreateOvlRes(&LpwrI2cQueue, 1, sizeof(I2C_INT_MESSAGE));
        if (status != FLCN_OK)
        {
            goto lpwrLpPreInitTask_exit;
        }
    }

lpwrLpPreInitTask_exit:
    return status;
}

/*!
 * @brief Entry point for LPWR_LP task
 *
 * LPWR_LP task handles different LowPower features like - Engine Idle Framework
 */
lwrtosTASK_FUNCTION(task_lpwr_lp, pvParameters)
{
    DISPATCH_LPWR_LP lpwrLpEvt;

    LWOS_TASK_LOOP(LWOS_QUEUE(PMU, LPWR_LP), &lpwrLpEvt, status, PMU_TASK_CMD_BUFFER_PTRS_GET(LPWR_LP))
    {
        status = lpwrLpEventHandler(&lpwrLpEvt);
    }
    LWOS_TASK_LOOP_END(status);
}

/* ------------------------- Private Functions ------------------------------ */
