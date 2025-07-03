/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    perf_cf_controller_util.h
 * @brief   Utilization PERF_CF Controller related defines.
 *
 * @copydoc perf_cf.h
 */

#ifndef PERF_CF_CONTROLLER_UTIL_H
#define PERF_CF_CONTROLLER_UTIL_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "perf/cf/perf_cf_controller.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct PERF_CF_CONTROLLER_UTIL PERF_CF_CONTROLLER_UTIL;

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * PERF_CF_CONTROLLER child class providing attributes of
 * Utilization PERF_CF Controller.
 */
struct PERF_CF_CONTROLLER_UTIL
{
    /*!
     * PERF_CF_CONTROLLER parent class.
     * Must be first element of the structure!
     */
    PERF_CF_CONTROLLER  super;
    /*!
     * Target utilization value to which controller is driving.
     */
    LwUFXP20_12         target;
    /*!
     * Threshold above which controller will jump to Base Clock.
     */
    LwUFXP20_12         jumpThreshold;
    /*!
     * Proportional error gain for increasing clocks.
     */
    LwUFXP20_12         gainInc;
    /*!
     * Proportional error gain for decreasing clocks.
     */
    LwUFXP20_12         gainDec;
    /*!
     * Latest utilization reading.
     */
    LwUFXP20_12         pct;
    /*!
     * Latest current frequency in kHz.
     */
    LwU32               lwrrentkHz;
    /*!
     * Latest ideal frequency in kHz.
     */
    LwU32               targetkHz;
    /*!
     * Average target frequency in kHz, with hysteresis consideration.
     */
    LwU32               avgTargetkHz;
    /*!
     * Clock domain index this controller is controlling.
     */
    LwU8                clkDomIdx;
    /*!
     * Index of topology for clock frequency input to the controller.
     */
    LwU8                clkTopologyIdx;
    /*!
     * Number of samples above target before increasing clocks.
     */
    LwU8                hysteresisCountInc;
    /*!
     * Number of samples below target before decreasing clocks.
     */
    LwU8                hysteresisCountDec;
    /*!
     * Current number of samples above/below target.
     */
    LwU8                hysteresisCountLwrr;
    /*!
     * Limit index to set.
     */
    LwU8                limitIdx;
    /*!
     * Index of topology for power gating % input to the controller. Can be
     * LW2080_CTRL_PERF_PERF_CF_CONTROLLER_INFO_UTIL_PG_TOPOLOGY_IDX_NONE.
     */
    LwU8                pgTopologyIdx;
    /*!
     * Is last sample above or below target? LW_TRUE = above, LW_FALSE = below.
     */
    LwBool              bIncLast;
    /*!
     * Do not program clocks if TRUE
     */
    LwBool              bDoNotProgram;
    /*!
     * Supports feature of scaling the utilization threshold in proportion
     * with number of VM slots that are active in vGPU's scheduler.
     */
    LwBool              bUtilThresholdScaleByVMCountSupported;
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet   (perfCfControllerGrpIfaceModel10ObjSetImpl_UTIL)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfControllerGrpIfaceModel10ObjSetImpl_UTIL");
BoardObjIfaceModel10GetStatus       (perfCfControllerIfaceModel10GetStatus_UTIL)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfControllerIfaceModel10GetStatus_UTIL");

// PERF_CF_CONTROLLER interfaces.
PerfCfControllerExelwte     (perfCfControllerExelwte_UTIL);
PerfCfControllerLoad        (perfCfControllerLoad_UTIL)
    GCC_ATTRIB_SECTION("imem_perfCfLoad", "perfCfControllerLoad_UTIL");
PerfCfControllerSetMultData (perfCfControllerSetMultData_UTIL)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfControllerSetMultData_UTIL");

/*!
 * @copydoc perfCfControllerGetReducedStatus_UTIL
 */
#define perfCfControllerGetReducedStatus_UTIL   \
    perfCfControllerIfaceModel10GetStatus_UTIL

/* ------------------------ Include Derived Types --------------------------- */
#include "perf/cf/perf_cf_controller_util_2x.h"

#endif // PERF_CF_CONTROLLER_UTIL_H
