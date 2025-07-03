/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef THRMPOLICY_DTC_VF_H
#define THRMPOLICY_DTC_VF_H

/* ------------------------ Includes --------------------------------------- */
#include "therm/thrmpolicy_domgrp.h"
#include "therm/thrmpolicy_dtc.h"

/*!
 * Extends THERM_POLICY_TC to implement a marching controller using VF points
 * as the limiting type.
 */
typedef struct
{
    /*!
     * THERM_POLICY_TC super class.
     */
    THERM_POLICY_DOMGRP super;

    /*!
     * THERM_POLICY_DTC interface.
     */
    THERM_POLICY_DTC    dtc;

    /*!
     * Parameters obtained from RM.
     */

    /*!
     * VF index containing the min frequency for P0.
     */
    LwU32               vfIdxMin;

    /*!
     * VF index containing the max frequency for P0.
     */
    LwU32               vfIdxMax;

    /*!
     * Maximum limit level used by the controller.
     */
    LwU32               limitMax;

    /*!
     * The limit level that will result in GPC2CLK being equal to or just
     * slightly greater than the rated TDP clock.
     */
    LwU32               limitTdp;

    /*!
     * Specifices the pivot limit where the controller stops limiting
     * GPC2CLK and begins limiting P-states. This limit is when the controller
     * has the fastest P-states with the lowest possible GPC2CLK for that
     * P-state.
     */
    LwU32               limitPivot;

    /*!
     * Parameters callwlated in the PMU.
     */

    /*!
     * Current limit level imposed by the controller. A lower number indicates
     * cooler limits (slower performance) while a higher number indicates
     * hotter limits (faster performance).
     */
    LwU32               limitLwrr;
}  THERM_POLICY_DTC_VF;

/* ------------------------- Defines --------------------------------------- */
/* ------------------------ Macros ----------------------------------------- */
#define thermPolicyLoad_DTC_VF(pPolicy)     thermPolicyLoad_DOMGRP(pPolicy)

/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
/*!
 * THERM_POLICY interfaces
 */
BoardObjGrpIfaceModel10ObjSet           (thermPolicyGrpIfaceModel10ObjSetImpl_DTC_VF)
    GCC_ATTRIB_SECTION("imem_thermLibConstruct", "thermPolicyGrpIfaceModel10ObjSetImpl_DTC_VF");
BoardObjIfaceModel10GetStatus               (thermPolicyIfaceModel10GetStatus_DTC_VF)
    GCC_ATTRIB_SECTION("imem_thermLibPolicy", "thermPolicyIfaceModel10GetStatus_DTC_VF");
ThermPolicyCallbackExelwte  (thermPolicyCallbackExelwte_DTC_VF)
    GCC_ATTRIB_SECTION("imem_thermLibPolicy", "thermPolicyCallbackExelwte_DTC_VF");

#endif // THRMPOLICY_DTC_VF_H
