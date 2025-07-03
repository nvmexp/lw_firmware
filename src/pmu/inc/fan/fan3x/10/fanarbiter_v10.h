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
 * @file fanarbiter_v10.h
 * @brief @copydoc fanarbiter_v10.c
 */

#ifndef FAN_ARBITER_V10_H
#define FAN_ARBITER_V10_H

/* ------------------------- System Includes ------------------------------- */
#include "fan/fan3x/fanarbiter.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct FAN_ARBITER_V10 FAN_ARBITER_V10;

/* ------------------------- Application Includes -------------------------- */
/* ------------------------- Macros ---------------------------------------- */
/* ------------------------- Datatypes ------------------------------------- */

/*!
 * Extends FAN_ARBITER providing attributes common to all FAN_ARBITER_V10.
 */
struct FAN_ARBITER_V10
{
    /*!
     * FAN_ARBITER super class. This should always be the first member!
     */
    FAN_ARBITER super;
};

/* ------------------------- Defines --------------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/*!
 * Board Object interfaces.
 */
BoardObjGrpIfaceModel10ObjSet  (fanArbiterGrpIfaceModel10ObjSetImpl_V10)
    GCC_ATTRIB_SECTION("imem_thermLibFanCommonConstruct", "fanArbiterGrpIfaceModel10ObjSetImpl_V10");

/* ------------------------ Include Derived Types -------------------------- */

#endif // FAN_ARBITER_V10_H
