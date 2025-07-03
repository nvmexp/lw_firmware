/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    perf_cf_controller_optp_2x.h
 * @brief   OPTP 2.x PERF_CF Controller related defines.
 *
 * @copydoc perf_cf.h
 */

#ifndef PERF_CF_CONTROLLER_OPTP_2X_H
#define PERF_CF_CONTROLLER_OPTP_2X_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "perf/cf/perf_cf_controller.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct PERF_CF_CONTROLLER_OPTP_2X PERF_CF_CONTROLLER_OPTP_2X;

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * PERF_CF_CONTROLLER child class providing attributes of
 * OPTP 2.x PERF_CF Controller.
 */
struct PERF_CF_CONTROLLER_OPTP_2X
{
    /*!
     * PERF_CF_CONTROLLER parent class.
     * Must be first element of the structure!
     */
    PERF_CF_CONTROLLER  super;
    /*!
     * Latest observed perf target (aka perf ratio).
     */
    LwUFXP8_24          perfTarget;
    /*!
     * Latest observed graphics frequency in kHz.
     */
    LwU32               grkHz;
    /*!
     * Latest observed video frequency in kHz.
     */
    LwU32               vidkHz;
    /*!
     * Do not cap clocks below this floor (in kHz) .
     */
    LwU32               freqFloorkHz;
    /*!
     * Trigger limit change when perfTarget goes above this threshold.
     */
    LwUFXP8_24          highThreshold;
    /*!
     * Trigger limit change when perfTarget goes below this threshold.
     */
    LwUFXP8_24          lowThreshold;
    /*!
     * Index of topology for graphics clock frequency.
     */
    LwU8                grClkTopologyIdx;
    /*!
     * Index of topology for video clock frequency.
     */
    LwU8                vidClkTopologyIdx;
    /*!
     * Index of topology for power gating %. Can be
     * LW2080_CTRL_PERF_PERF_CF_CONTROLLER_INFO_OPTP_2X_PG_TOPOLOGY_IDX_NONE.
     */
    LwU8                pgTopologyIdx;
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet                             (perfCfControllerGrpIfaceModel10ObjSetImpl_OPTP_2X)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfControllerGrpIfaceModel10ObjSetImpl_OPTP_2X");

BoardObjIfaceModel10GetStatus               (perfCfControllerIfaceModel10GetStatus_OPTP_2X)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfControllerIfaceModel10GetStatus_OPTP_2X");

// PERF_CF_CONTROLLER interfaces.
PerfCfControllerExelwte     (perfCfControllerExelwte_OPTP_2X);

PerfCfControllerLoad                          (perfCfControllerLoad_OPTP_2X)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfControllerLoad_OPTP_2X");
PerfCfControllerSetMultData                   (perfCfControllerSetMultData_OPTP_2X)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfControllerSetMultData_OPTP_2X");

/* ------------------------ Include Derived Types --------------------------- */

#endif // PERF_CF_CONTROLLER_OPTP_2X_H
