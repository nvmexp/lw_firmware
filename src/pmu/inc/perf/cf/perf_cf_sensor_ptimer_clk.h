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
 * @file    perf_cf_sensor_ptimer_clk.h
 * @brief   PTIMER PERF_CF Sensor related defines.
 *
 * @copydoc perf_cf.h
 */

#ifndef PERF_CF_SENSOR_PTIMER_CLK_H
#define PERF_CF_SENSOR_PTIMER_CLK_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "perf/cf/perf_cf_sensor_ptimer.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct PERF_CF_SENSOR_PTIMER_CLK PERF_CF_SENSOR_PTIMER_CLK;

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * PERF_CF_SENSOR_PTIMER child class providing attributes of
 * PTIMER CLK PERF_CF Sensor.
 */
struct PERF_CF_SENSOR_PTIMER_CLK
{
    /*!
     * PERF_CF_SENSOR_PTIMER parent class.
     * Must be first element of the structure!
     */
    PERF_CF_SENSOR_PTIMER   super;
    /*!
     * Latest sensor reading.
     */
    LwU64                   reading;
    /*!
     * Last read timestamp.
     */
    FLCN_TIMESTAMP          last;
    /*!
     * Clock frequency (kHz) for PTIMER to HW base counter scaling.
     */
    LwU32                   clkFreqKHz;
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet       (perfCfSensorGrpIfaceModel10ObjSetImpl_PTIMER_CLK)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfSensorGrpIfaceModel10ObjSetImpl_PTIMER_CLK");
BoardObjIfaceModel10GetStatus           (perfCfSensorIfaceModel10GetStatus_PTIMER_CLK)
    GCC_ATTRIB_SECTION("imem_perfCfTopology", "perfCfSensorIfaceModel10GetStatus_PTIMER_CLK");

// PERF_CF_SENSOR interfaces.
PerfCfSensorLoad        (perfCfSensorLoad_PTIMER_CLK)
    GCC_ATTRIB_SECTION("imem_perfCfLoad", "perfCfSensorLoad_PTIMER_CLK");

/* ------------------------ Include Derived Types --------------------------- */

#endif // PERF_CF_SENSOR_PTIMER_CLK_H
