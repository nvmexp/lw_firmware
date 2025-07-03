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
 * @file    client_perf_cf_pwr_model_profile_tgp_1x.h
 * @brief   TGP_1X Client PERF_CF Pwr Model Profile related defines.
 *
 * @copydoc perf_cf.h
 */

#ifndef CLIENT_PERF_CF_PWR_MODEL_PROFILE_TGP_1X_H
#define CLIENT_PERF_CF_PWR_MODEL_PROFILE_TGP_1X_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "perf/cf/client_perf_cf_pwr_model_profile.h"
#include "ctrl/ctrl2080/ctrl2080perf_cf.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct CLIENT_PERF_CF_PWR_MODEL_PROFILE_TGP_1X CLIENT_PERF_CF_PWR_MODEL_PROFILE_TGP_1X;

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @brief   Base list of ovl. descriptors required by PERF_CF TGP_1X Client Pwr
 *          Model.
 *
 * @todo    Build up this lists as feature is enabled for ga100 profile
 */
#if PMUCFG_FEATURE_ENABLED(PMU_CLIENT_PERF_CF_PWR_MODEL_PROFILE_TGP_1X)
#define OSTASK_OVL_DESC_DEFINE_CLIENT_PERF_CF_MODEL_PROFILE_TGP_1X             \
    OSTASK_OVL_DESC_ILWALID
#else
#define OSTASK_OVL_DESC_DEFINE_CLIENT_PERF_CF_MODEL_PROFILE_TGP_1X             \
    OSTASK_OVL_DESC_ILWALID
#endif

/*!
 * @brief   List of ovl. descriptors required by PERF_CF TGP_1X Client Pwr Model
 *          for scaling.
 *
 * @todo    Build up this lists as feature is enabled for ga100 profile
 */
#if PMUCFG_FEATURE_ENABLED(PMU_CLIENT_PERF_CF_PWR_MODEL_PROFILE_TGP_1X)
#define OSTASK_OVL_DESC_DEFINE_CLIENT_PERF_PWR_MODEL_PROFILE_SCALE_TGP_1X      \
    OSTASK_OVL_DESC_ILWALID
#else
#define OSTASK_OVL_DESC_DEFINE_CLIENT_PERF_PWR_MODEL_PROFILE_SCALE_TGP_1X      \
    OSTASK_OVL_DESC_ILWALID
#endif

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * CLIENT_PERF_CF_PWR_MODEL_PROFILE child class providing attributes of TGP_1X
 * PERF_CF Client PwrModel Profile
 */
struct CLIENT_PERF_CF_PWR_MODEL_PROFILE_TGP_1X
{
    /*!
     * CLIENT_PERF_CF_PWR_MODEL_PROFILE parent class.
     * Must be first element of the structure!
     */
    CLIENT_PERF_CF_PWR_MODEL_PROFILE super;

    /*!
     * Parameters specific to the workload being modeled.
     */
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_TGP_1X_WORKLOAD_PARAMETERS workloadParameters;

    /*!
     * Characterized frame buffer power in milliwatts.
     */
    LwU32 fbPwrmW;

    /*!
     * Characterized sources of power consumption from other sources (i.e. not
     * core power or fb rail power) in milliwatts.
     */
    LwU32 otherPwrmW;
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet                             (clientPerfCfPwrModelProfileGrpIfaceModel10ObjSetImpl_TGP_1X)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "clientPerfCfPwrModelProfileGrpIfaceModel10ObjSetImpl_TGP_1X");

// Client PerfCf PwrModel Profile interfaces.
ClientPerfCfPwrModelProfileScaleInternalMetricsInit     (clientPerfCfPwrModelProfileScaleInternalMetricsInit_TGP_1X)
    GCC_ATTRIB_SECTION("imem_perfCfModel",              "clientPerfCfPwrModelProfileScaleInternalMetricsInit_TGP_1X");
ClientPerfCfPwrModelProfileScaleInternalMetricsExtract  (clientPerfCfPwrModelProfileScaleInternalMetricsExtract_TGP_1X)
    GCC_ATTRIB_SECTION("imem_perfCfModel",              "clientPerfCfPwrModelProfileScaleInternalMetricsExtract_TGP_1X");
/* ------------------------ Include Derived Types --------------------------- */

#endif // CLIENT_PERF_CF_PWR_MODEL_PROFILE_TGP_1X_H
