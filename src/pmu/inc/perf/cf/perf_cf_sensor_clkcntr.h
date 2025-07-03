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
 * @file    perf_cf_sensor_clkcntr.h
 * @brief   CLKCNTR PERF_CF Sensor related defines.
 *
 * @copydoc perf_cf.h
 */

#ifndef PERF_CF_SENSOR_CLKCNTR_H
#define PERF_CF_SENSOR_CLKCNTR_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "perf/cf/perf_cf_sensor.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct PERF_CF_SENSOR_CLKCNTR PERF_CF_SENSOR_CLKCNTR;

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * PERF_CF_SENSOR child class providing attributes of
 * CLKCNTR PERF_CF Sensor.
 */
struct PERF_CF_SENSOR_CLKCNTR
{
    /*!
     * PERF_CF_SENSOR parent class.
     * Must be first element of the structure!
     */
    PERF_CF_SENSOR  super;
    /*!
     * Clock domain @ref LW2080_CTRL_CLK_DOMAIN_<xyz> to count.
     */
    LwU32           clkApiDomain;
    /*!
     * Clock domain index to count. Used to get @ref clkApiDomain at load.
     */
    LwU8            clkDomIdx;
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet       (perfCfSensorGrpIfaceModel10ObjSetImpl_CLKCNTR)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfSensorGrpIfaceModel10ObjSetImpl_CLKCNTR");
BoardObjIfaceModel10GetStatus           (perfCfSensorIfaceModel10GetStatus_CLKCNTR)
    GCC_ATTRIB_SECTION("imem_perfCfTopology", "perfCfSensorIfaceModel10GetStatus_CLKCNTR");

// PERF_CF_SENSOR interfaces.
PerfCfSensorLoad        (perfCfSensorLoad_CLKCNTR)
    GCC_ATTRIB_SECTION("imem_perfCfLoad", "perfCfSensorLoad_CLKCNTR");

/* ------------------------ Include Derived Types --------------------------- */

#endif // PERF_CF_SENSOR_CLKCNTR_H
