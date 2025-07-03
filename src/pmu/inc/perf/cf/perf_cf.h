/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    perf_cf.h
 * @brief   Common PERF_CF related defines (public interfaces).
 *          PERF_CF - GPU Performance Controller Framework.
 *
 * Detailed documentation is located at:
 * https://confluence.lwpu.com/display/RMPER/GPU+Performance+Framework+and+Controllers
 */

#ifndef PERF_CF_H
#define PERF_CF_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "perf/cf/perf_cf_sensor.h"
#include "perf/cf/perf_cf_pm_sensor.h"
#include "perf/cf/perf_cf_topology.h"
#include "perf/cf/perf_cf_controller.h"
#include "perf/cf/client_perf_cf_controller.h"
#include "perf/cf/perf_cf_policy.h"
#include "perf/cf/client_perf_cf_policy.h"
#include "perf/cf/perf_cf_pwr_model.h"
#include "perf/cf/client_perf_cf_pwr_model_profile.h"
#include "clk/pmu_clkcntr.h"
#include "task_perf_cf.h"

#include "config/g_perf_cf_hal.h"

/* ------------------------ Forward Definitions ----------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * Colwenience macro to look-up PERF_CF structure.
 */
#define PERF_CF_GET_OBJ()               (&PerfCf)

/*!
 * @brief   List of ovl. descriptors required by most PERF_CF code paths.
 */
#define OSTASK_OVL_DESC_DEFINE_PERF_CF_BASE                                    \
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libBoardObj)                     \
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfCf)                          \
        OSTASK_OVL_DESC_DEFINE_LIB_LW64

 /*!
 * @brief   List of ovl. descriptors required by most PERF_CF SET and
 *          GET_STATUS code paths.
 */
#define OSTASK_OVL_DESC_DEFINE_PERF_CF_BOARD_OBJ_COMMON                        \
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfCfBoardObj)                  \
        OSTASK_OVL_DESC_DEFINE_PERF_CF_TOPOLOGY                                \
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfCfModel)                     \
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfCfModel)                     \
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfCfController)

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * Structure holding all PERF_CF related data.
 */
typedef struct
{
    /*!
     * State of the PERF_CF groups.
     */
    LwBool                              bLoaded;
    /*!
     * Board Object Group of all PERF_CF_SENSOR objects.
     */
#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_SENSOR))
    PERF_CF_SENSORS                     sensors;
#endif
    /*!
     * Board Object Group of all PERF_CF_PM_SENSOR objects.
     */
#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PM_SENSOR))
    PERF_CF_PM_SENSORS                  pmSensors;
#endif
    /*!
     * Board Object Group of all PERF_CF_TOPOLOGY objects.
     */
#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_TOPOLOGY))
    PERF_CF_TOPOLOGYS                   topologys;
#endif
    /*!
     * Board Object Group of all PERF_CF_CONTROLLER objects.
     */
#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER))
    PERF_CF_CONTROLLERS                *pControllers;
#endif
    /*!
     * Board Object Group of all CLIENT_PERF_CF_CONTROLLER objects.
     */
#if (PMUCFG_FEATURE_ENABLED(PMU_CLIENT_PERF_CF_CONTROLLER))
    CLIENT_PERF_CF_CONTROLLERS          clientControllers;
#endif
    /*!
     * Board Object Group of all PERF_CF_POLICY objects.
     */
#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_POLICY))
    PERF_CF_POLICYS                     policys;
#endif
    /*!
     * Board Object Group of all CLIENT_PERF_CF_POLICY objects.
     */
#if (PMUCFG_FEATURE_ENABLED(PMU_CLIENT_PERF_CF_POLICY))
    CLIENT_PERF_CF_POLICYS              clientPolicys;
#endif
    /*!
     * Board Object Group of all PERF_CF_PWR_MODEL objects.
     */
#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PWR_MODEL))
    PERF_CF_PWR_MODELS                  pwrModels;
#endif
    /*!
     * Board Object Group of all CLIENT_PERF_CF_PWR_MODEL_PROFILE objects.
     */
#if (PMUCFG_FEATURE_ENABLED(PMU_CLIENT_PERF_CF_PWR_MODEL_PROFILE))
    CLIENT_PERF_CF_PWR_MODEL_PROFILES   clientPwrModelProfiles;
#endif
} OBJPERF_CF;

/* ------------------------ External Definitions ---------------------------- */
extern OBJPERF_CF PerfCf;

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
FLCN_STATUS perfCfPostInit(void)
    GCC_ATTRIB_SECTION("imem_perfCf", "perfCfPostInit");

#endif // PERF_CF_H
