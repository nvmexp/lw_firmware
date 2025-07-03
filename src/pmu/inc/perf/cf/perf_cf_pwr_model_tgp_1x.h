/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    perf_cf_pwr_model_tgp_1X.h
 * @brief   TGP_1X PERF_CF Pwr Model related defines.
 *
 * @copydoc perf_cf.h
 */

#ifndef PERF_CF_PWR_MODEL_TGP_1X_H
#define PERF_CF_PWR_MODEL_TGP_1X_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "perf/cf/perf_cf_pwr_model.h"
#include "ctrl/ctrl2080/ctrl2080perf_cf.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct PERF_CF_PWR_MODEL_TGP_1X PERF_CF_PWR_MODEL_TGP_1X;

/* ------------------------ Macros and Defines ------------------------------ */
 /*!
 * @brief   Base list of ovl. descriptors required by PERF_CF TGP_1X.
  *
  *@todo    Build up this lists as feature is enabled for ga100 profile
 */
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PWR_MODEL_TGP_1X)
#define OSTASK_OVL_DESC_DEFINE_PERF_CF_MODEL_TGP_1X                            \
    OSTASK_OVL_DESC_ILWALID
#else
#define OSTASK_OVL_DESC_DEFINE_PERF_CF_MODEL_TGP_1X                            \
    OSTASK_OVL_DESC_ILWALID
#endif

 /*!
 * @brief   List of ovl. descriptors required by PERF_CF TGP_1X for scaling.
  *
  *@todo    Build up this lists as feature is enabled for ga100 profile
 */
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PWR_MODEL_TGP_1X)
#define OSTASK_OVL_DESC_DEFINE_PERF_PWR_MODEL_SCALE_TGP_1X                     \
    PWR_POLICY_PERF_CF_PWR_MODEL_SCALE_OVERLAYS                                \
    PWR_POLICY_WORKLOAD_COMBINED_1X_SCALE_OVERLAYS                             \
    OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfCfPwrModelTgp1xRuntimeScratch)   \
    OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, pmgr)
#else
#define OSTASK_OVL_DESC_DEFINE_PERF_PWR_MODEL_SCALE_TGP_1X                     \
    OSTASK_OVL_DESC_ILWALID
#endif

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * PERF_CF_PWR_MODEL child class providing attributes of TGP_1X PERF_CF PwrModel
 */
struct PERF_CF_PWR_MODEL_TGP_1X
{
    /*!
     * PERF_CF_PWR_MODEL parent class.
     * Must be first element of the structure!
     */
    PERF_CF_PWR_MODEL   super;

    /*!
     * Index into the PWR_POLICY BoardObjGrp of the workload policy used to
     * estimate core power.
     */
    LwBoardObjIdx       workloadPwrPolicyIdx;
};

/*!
 * @copydoc LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS_TGP_1X
 */
typedef struct
{
    /*!
     * Super member.
     */
    PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS super;

    /*!
     * Temporary dummy data, actual fields to be filled in at a later date.
     *
     * There is a possibility there will be no type-specific inputs for this class.
     */
    LwU32 rsvd;
} PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS_TGP_1X;

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet                             (perfCfPwrModelGrpIfaceModel10ObjSetImpl_TGP_1X)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfPwrModelGrpIfaceModel10ObjSetImpl_TGP_1X");
BoardObjIfaceModel10GetStatus                                 (perfCfPwrModelIfaceModel10GetStatus_TGP_1X)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfPwrModelIfaceModel10GetStatus_TGP_1X");

// PERF_CF Pwr Model related interfaces.
PerfCfPwrModelLoad                            (perfCfPwrModelLoad_TGP_1X)
    GCC_ATTRIB_SECTION("imem_perfCfLoad",     "perfCfPwrModelLoad_TGP_1X");
PerfCfPwrModelSampleDataInit                  (perfCfPwrModelSampleDataInit_TGP_1X)
    GCC_ATTRIB_SECTION("imem_perfCfLoad", "perfCfPwrModelSampleDataInit_TGP_1X");
PerfCfPwrModelObserveMetrics                  (perfCfPwrModelObserveMetrics_TGP_1X)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "perfCfPwrModelObserveMetrics_TGP_1X");
PerfCfPwrModelScale                           (perfCfPwrModelScale_TGP_1X)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "perfCfPwrModelScale_TGP_1X");
PerfCfPwrModelScaleTypeParamsInit             (perfCfPwrModelScaleTypeParamsInit_TGP_1X)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "perfCfPwrModelScaleTypeParamsInit_TGP_1X");

/* ------------------------ Include Derived Types --------------------------- */

#endif // PERF_CF_PWR_MODEL_TGP_1X_H
