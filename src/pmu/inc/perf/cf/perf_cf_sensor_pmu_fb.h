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
 * @file    perf_cf_sensor_pmu_fb.h
 * @brief   PMU PERF_CF Sensor related defines.
 *
 * @copydoc perf_cf.h
 */

#ifndef PERF_CF_SENSOR_PMU_FB_H
#define PERF_CF_SENSOR_PMU_FB_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "perf/cf/perf_cf_sensor_pmu.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct PERF_CF_SENSOR_PMU_FB PERF_CF_SENSOR_PMU_FB;

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * PERF_CF_SENSOR_PMU child class providing attributes of
 * PMU FB PERF_CF Sensor.
 */
struct PERF_CF_SENSOR_PMU_FB
{
    /*!
     * PERF_CF_SENSOR_PMU parent class.
     * Must be first element of the structure!
     */
    PERF_CF_SENSOR_PMU  super;
    /*!
     * Last read HW clock counter value for multiplier.
     */
    LwU64               lastMulti;
    /*!
     * Last read HW clock counter value for divisor.
     */
    LwU64               lastDiv;
    /*!
     * Constant scaling factor.
     */
    LwUFXP20_12         scale;
    /*!
     * HW idle counter for FB needs to be scaled by clocks ratio. This is the
     * multipler clock domain @ref LW2080_CTRL_CLK_DOMAIN_<xyz>.
     */
    LwU32               clkApiDomainMulti;
    /*!
     * HW idle counter for FB needs to be scaled by clocks ratio. This is the
     * divisor clock domain @ref LW2080_CTRL_CLK_DOMAIN_<xyz>.
     */
    LwU32               clkApiDomainDiv;
    /*!
     * Multipler clock domain index. Used to get @ref clkApiDomainMulti at load.
     */
    LwU8                clkDomIdxMulti;
    /*!
     * Divisor clock domain index. Used to get @ref clkApiDomainDiv at load.
     */
    LwU8                clkDomIdxDiv;
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet       (perfCfSensorGrpIfaceModel10ObjSetImpl_PMU_FB)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfSensorGrpIfaceModel10ObjSetImpl_PMU_FB");
BoardObjIfaceModel10GetStatus           (perfCfSensorIfaceModel10GetStatus_PMU_FB)
    GCC_ATTRIB_SECTION("imem_perfCfTopology", "perfCfSensorIfaceModel10GetStatus_PMU_FB");

// PERF_CF_SENSOR interfaces.
PerfCfSensorLoad        (perfCfSensorLoad_PMU_FB)
    GCC_ATTRIB_SECTION("imem_perfCfLoad", "perfCfSensorLoad_PMU_FB");

/* ------------------------ Include Derived Types --------------------------- */

#endif // PERF_CF_SENSOR_PMU_FB_H
