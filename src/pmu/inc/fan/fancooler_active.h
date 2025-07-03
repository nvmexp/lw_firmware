/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file fancooler_active.h
 * @brief @copydoc fancooler_active.c
 */

#ifndef FAN_COOLER_ACTIVE_H
#define FAN_COOLER_ACTIVE_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "fan/fancooler.h"
#include "pmgr/pmgrtach.h"

/* ------------------------- Macros ---------------------------------------- */
/* ------------------------- Datatypes ------------------------------------- */
typedef struct FAN_COOLER_ACTIVE FAN_COOLER_ACTIVE;

/*!
 * Extends FAN_COOLER providing attributes specific to ACTIVE cooler.
 */
struct FAN_COOLER_ACTIVE
{
    /*!
     * FAN_COOLER super class. This should always be the first member!
     */
    FAN_COOLER  super;

    /*!
     * Parameters obtained from RM.
     */

    /*!
     * Minimum RPM.
     */
    LwU32       rpmMin;

    /*!
     * Acoustic Maximum RPM.
     */
    LwU32       rpmMax;

    /*!
     * Tachometer is present and supported.
     */
    LwBool      bTachSupported;

    /*!
     * Tachometer Rate (PPR).
     */
    LwU8        tachRate;

    /*!
     * Tachometer GPIO pin.
     */
    LwU8        tachPin;

    /*!
     * PMU specific parameters.
     */

    /*!
     * Fan TACH source.
     */
    PMU_PMGR_TACH_SOURCE
                tachSource;
};

/* ------------------------ Function Prototypes ---------------------------- */
BoardObjGrpIfaceModel10ObjSet       (fanCoolerGrpIfaceModel10ObjSetImpl_ACTIVE)
    GCC_ATTRIB_SECTION("imem_thermLibFanCommonConstruct", "fanCoolerGrpIfaceModel10ObjSetImpl_ACTIVE");
BoardObjIfaceModel10GetStatus           (fanCoolerActiveIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_thermLibFanCommon", "fanCoolerActiveIfaceModel10GetStatus");
FanCoolerLoad           (fanCoolerActiveLoad);
FanCoolerRpmGet         (fanCoolerActiveRpmGet);

/* ------------------------ Include Derived Types -------------------------- */
#include "fan/fancooler_active_pwm.h"

#endif // FAN_COOLER_ACTIVE_H
