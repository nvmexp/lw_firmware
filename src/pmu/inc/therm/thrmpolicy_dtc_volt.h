/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef THRMPOLICY_DTC_VOLT_H
#define THRMPOLICY_DTC_VOLT_H

/* ------------------------ Includes --------------------------------------- */
#include "therm/thrmpolicy_domgrp.h"
#include "therm/thrmpolicy_dtc.h"

/*!
 * Extends THERM_POLICY_DOMGRP to implement a marching controller using voltage
 * as the limiting type.
 */
typedef struct
{
    /*!
     * THERM_POLICY_DOMGRP super class.
     */
    THERM_POLICY_DOMGRP super;

    /*!
     * THERM_POLICY_DTC interface.
     */
    THERM_POLICY_DTC    dtc;

    /*!
     * The maximum voltage value the controller is allowed to cap to.
     */
    LwU32               voltageMaxuV;

    /*!
     * The minimum voltage value the controller is allowed to cap to before
     * limiting P-states.
     */
    LwU32               voltageMinuV;

    /*!
     * The size of the voltage steps the controller will take.
     */
    LwU32               voltageStepuV;

    /*!
     * Maximum limit level used by the controller.
     */
    LwU32               limitMax;

    /*!
     * Current limit level imposed by the controller. A lower number indicates
     * cooler limits (slower performance) while a higher number indicates
     * hotter limits (faster performance).
     */
    LwU32               limitLwrr;

    /*!
     * Specifices the pivot limit where the controller stops limiting
     * voltage and begins limiting P-states. This limit is when the controller
     * has the fastest P-states with the lowest possible voltage for that
     * P-state.
     */
    LwU32               limitPivot;

    /*!
     * Specifies the rated TDP VF entry index.
     */
    LwU8                ratedTdpVfEntryIdx;

    /*!
     * Store the rated TDP VF entry pointer so that we dont need to callwlate
     * it again
     */
     RM_PMU_PERF_VF_ENTRY_INFO *pRatedTdpVfEntry;
} THERM_POLICY_DTC_VOLT;

/* ------------------------- Defines --------------------------------------- */
/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
/*!
 * THERM_POLICY interfaces
 */
BoardObjGrpIfaceModel10ObjSet           (thermPolicyGrpIfaceModel10ObjSetImpl_DTC_VOLT)
    GCC_ATTRIB_SECTION("imem_thermLibConstruct", "thermPolicyGrpIfaceModel10ObjSetImpl_DTC_VOLT");
BoardObjIfaceModel10GetStatus               (thermPolicyIfaceModel10GetStatus_DTC_VOLT)
    GCC_ATTRIB_SECTION("imem_thermLibPolicy", "thermPolicyIfaceModel10GetStatus_DTC_VOLT");
ThermPolicyLoad             (thermPolicyLoad_DTC_VOLT)
    GCC_ATTRIB_SECTION("imem_thermLibPolicy", "thermPolicyLoad_DTC_VOLT");
ThermPolicyCallbackExelwte  (thermPolicyCallbackExelwte_DTC_VOLT)
    GCC_ATTRIB_SECTION("imem_thermLibPolicy", "thermPolicyCallbackExelwte_DTC_VOLT");

#endif // THRMPOLICY_DTC_VOLT_H
