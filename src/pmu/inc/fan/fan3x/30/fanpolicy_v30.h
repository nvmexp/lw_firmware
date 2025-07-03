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
 * @file fanpolicy_v30.h
 * @brief @copydoc fanpolicy_v30.c
 */

#ifndef FAN_POLICY_V30_H
#define FAN_POLICY_V30_H

/* ------------------------- System Includes ------------------------------- */
#include "fan/fanpolicy.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct FAN_POLICY_V30 FAN_POLICY_V30;

/* ------------------------- Application Includes -------------------------- */
/* ------------------------- Macros ---------------------------------------- */
#define fanPolicyIfaceModel10GetStatus_V30(pModel10, pPayload)                             \
    fanPolicyIfaceModel10GetStatus_SUPER(pModel10, pPayload)

/* ------------------------- Datatypes ------------------------------------- */

/*!
 * Extends FAN_POLICY providing attributes common to all FAN_POLICY_V30.
 */
struct FAN_POLICY_V30
{
    /*!
     * FAN_POLICY super class. This should always be the first member!
     */
    FAN_POLICY  super;
};

/* ------------------------- Defines --------------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/*!
 * Board Object interfaces.
 */
BoardObjGrpIfaceModel10ObjSet   (fanPolicyGrpIfaceModel10ObjSetImpl_V30)
    GCC_ATTRIB_SECTION("imem_thermLibFanCommonConstruct", "fanPolicyGrpIfaceModel10ObjSetImpl_V30");

FanPolicyLoad       (fanPolicyLoad_V30);

/* ------------------------ Include Derived Types -------------------------- */

#endif // FAN_POLICY_V30_H
