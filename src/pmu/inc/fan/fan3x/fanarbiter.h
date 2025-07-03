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
 * @file fanarbiter.h
 * @brief @copydoc fanarbiter.c
 */

#ifndef FAN_ARBITER_H
#define FAN_ARBITER_H

/* ------------------------- System Includes ------------------------------- */
#include "boardobj/boardobjgrp.h"
#include "pmu_fanctrl.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct FAN_ARBITERS FAN_ARBITERS;
typedef struct FAN_ARBITER  FAN_ARBITER, FAN_ARBITER_BASE;

/* ------------------------- Application Includes -------------------------- */
#include "boardobj/boardobjgrp_iface_model_10.h"
#include "boardobj/boardobj_iface_model_10.h"

/* ------------------------- Macros ---------------------------------------- */
/*!
 * @copydoc BOARDOBJGRP_GRP_GET
 */
#define BOARDOBJGRP_DATA_LOCATION_FAN_ARBITER \
    (&(Fan.arbiters.super.super))

/*!
 * @copydoc BOARDOBJGRP_OBJ_GET
 */
#define FAN_ARBITER_GET(_objIdx) \
    (BOARDOBJGRP_OBJ_GET(FAN_ARBITER, (_objIdx)))

/* ------------------------- Datatypes ------------------------------------- */
/*!
 * @interface FAN_ARBITER
 *
 * Loads a FAN_ARBITER.
 *
 * @param[in]  pArbiter  FAN_ARBITER object pointer
 *
 * @return FLCN_OK
 *      FAN_ARBITER loaded successfully.
 */
#define FanArbiterLoad(fname) FLCN_STATUS (fname)(FAN_ARBITER *pArbiter)

/*!
 * @interface FAN_ARBITER
 *
 * Timer callback for the FAN_ARBITER.
 *
 * @param[in]   pArbiter    FAN_ARBITER object pointer
 *
 * @return  FLCN_ERR_NOT_SUPPORTED
 *      Arbiter object does not support this interface.
 * @return FLCN_OK
 *      FAN_ARBITER timer callback exelwted successfully.
 */
#define FanArbiterCallback(fname) FLCN_STATUS (fname)(FAN_ARBITER *pArbiter)

/*!
 * Container to hold data for all FAN_ARBITERS.
 */
struct FAN_ARBITERS
{
    /*!
     * Board Object Group of all FAN_ARBITER objects.
     */
    BOARDOBJGRP_E32 super;
};

/*!
 * Extends BOARDOBJ providing attributes common to all FAN_ARBITERS.
 */
struct FAN_ARBITER
{
    /*!
     * BOARDOBJ super class. This should always be the first member!
     */
    BOARDOBJ    super;

    /*!
     * Parameters obtained from RM.
     */

    /*!
     * Computation mode @ref LW2080_CTRL_FAN_ARBITER_MODE_<xyz>.
     */
    LwU8        mode;

    /*!
     * Pointer to the FAN_COOLER object for setting PWM/RPM after arbitration.
     */
    FAN_COOLER *pCooler;

    /*!
     * Sampling period.
     */
    LwU16   samplingPeriodms;

    /*!
     * Mask of FAN_POLICIES that RM/PMU will use.
     */
    BOARDOBJGRPMASK_E32
            fanPoliciesMask;

    /*!
     * All FAN_POLICIES that RM/PMU will use must have same control unit.
     * This is guaranteed by @ref FanArbiterSanityCheck implementation.
     * Caching the control unit for the LSB set in @ref fanPoliciesMask
     * to be later used by the PMU for setting PWM/RPM respectively.
     */
    LwU8    fanPoliciesControlUnit;

    /*!
     * PMU specific parameters.
     */

    /*!
     * The time, in microseconds, the next callback should occur. If the arbiter
     * does not poll, this value shall be FAN_ARBITER_TIME_INFINITY.
     */
    LwU32            callbackTimestampus;

    /*!
     * Denotes the control action to be taken on the fan described via
     * @ref LW2080_CTRL_FAN_CTRL_ACTION_<xyz>.
     */
    LwU8             fanCtrlAction;

    /*!
     * Denotes the policy that is lwrrently driving the fan.
     */
    LwBoardObjIdx    drivingPolicyIdx;

    /*!
     * Target settings requested by the arbiter.
     */
    union
    {
        /*!
         * Target PWM (in percents), used when @ref fanPoliciesControlUnit is
         * LW2080_CTRL_FAN_COOLER_ACTIVE_CONTROL_UNIT_PWM.
         */
        LwSFXP16_16 pwm;

        /*!
         * Target RPM, used when @ref fanPoliciesControlUnit is
         * LW2080_CTRL_FAN_COOLER_ACTIVE_CONTROL_UNIT_RPM.
         */
        LwS32       rpm;
    } target;
};

/* ------------------------- Defines --------------------------------------- */
/*!
 * Used for fan arbiters that do not require timer callbacks.
 */
#define FAN_ARBITER_TIME_INFINITY   LW_U32_MAX

/* ------------------------ Function Prototypes ---------------------------- */
FLCN_STATUS fanArbitersLoad(void);

FanArbiterLoad       (fanArbiterLoad);
FanArbiterCallback   (fanArbiterCallback);

/*!
 * Board Object interfaces.
 */
BoardObjGrpIfaceModel10ObjSet           (fanArbiterGrpIfaceModel10ObjSetImpl_SUPER)
    GCC_ATTRIB_SECTION("imem_thermLibFanCommonConstruct", "fanArbiterGrpIfaceModel10ObjSetImpl_SUPER");
BoardObjGrpIfaceModel10CmdHandler       (fanArbitersBoardObjGrpIfaceModel10Set)
    GCC_ATTRIB_SECTION("imem_thermLibFanCommonConstruct", "fanArbitersBoardObjGrpIfaceModel10Set");
BoardObjGrpIfaceModel10SetEntry   (fanFanArbiterIfaceModel10SetEntry)
    GCC_ATTRIB_SECTION("imem_thermLibFanCommonConstruct", "fanFanArbiterIfaceModel10SetEntry");
BoardObjGrpIfaceModel10CmdHandler       (fanArbitersBoardObjGrpIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_thermLibFanCommon", "fanArbitersBoardObjGrpIfaceModel10GetStatus");
BoardObjIfaceModel10GetStatus               (fanFanArbiterIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_thermLibFanCommon", "fanFanArbiterIfaceModel10GetStatus");

/* ------------------------ Include Derived Types -------------------------- */
#include "fan/fan3x/10/fanarbiter_v10.h"

#endif // FAN_ARBITER_H
