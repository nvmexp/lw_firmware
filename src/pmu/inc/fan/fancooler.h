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
 * @file fancooler.h
 * @brief @copydoc fancooler.c
 */

#ifndef FAN_COOLER_H
#define FAN_COOLER_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobjgrp_iface_model_10.h"
#include "boardobj/boardobj_iface_model_10.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct FAN_COOLERS FAN_COOLERS;
typedef struct FAN_COOLER  FAN_COOLER, FAN_COOLER_BASE;

/* ------------------------- Macros ---------------------------------------- */
/*!
 * @copydoc BOARDOBJGRP_GRP_GET
 */
#define BOARDOBJGRP_DATA_LOCATION_FAN_COOLER \
    (&(Fan.coolers.super.super))

/*!
 * @copydoc BOARDOBJGRP_IS_VALID
 */
#define FAN_COOLER_INDEX_IS_VALID(_objIdx) \
    BOARDOBJGRP_IS_VALID(FAN_COOLER, (_objIdx))

/*!
 * @copydoc BOARDOBJGRP_OBJ_GET
 */
#define FAN_COOLER_GET(_objIdx) \
    (BOARDOBJGRP_OBJ_GET(FAN_COOLER, (_objIdx)))

/* ------------------------- Datatypes ------------------------------------- */
// FAN_COOLER interfaces

/*!
 * @interface FAN_COOLER
 *
 * Loads a FAN_COOLER.
 *
 * @param[in]  pCooler  FAN_COOLER object pointer
 *
 * @return FLCN_OK
 *      FAN_COOLER loaded successfully.
 */
#define FanCoolerLoad(fname) FLCN_STATUS (fname)(FAN_COOLER *pCooler)

/*!
 * @interface FAN_COOLER
 *
 * Obtains current cooler's tachometer reading in RPM.
 *
 * @param[in]   pCooler     FAN_COOLER object pointer
 * @param[out]  pRpm        Buffer to store cooler's RPM speed
 *
 * @return  FLCN_ERR_NOT_SUPPORTED
 *      Cooler object does not support this interface.
 * @return  FLCN_OK
 *      Requested tachometer reading was successfully retrieved.
 */
#define FanCoolerRpmGet(fname) FLCN_STATUS (fname)(FAN_COOLER *pCooler, LwS32 *pRpm)

/*!
 * @interface FAN_COOLER
 *
 * Sets the desired cooler's speed in RPMs. Cooler cannot immediately switch to
 * the desired value, and it will only correct it's current speed towards the
 * desired value at predefined rate. Multiple ilwocations of this interface are
 * necessary (over the period of time) to assure that target speed is reached.
 *
 * @param[in]   pCooler     FAN_COOLER object pointer
 * @param[out]  rpm         Desired RPM speed of the cooler
 *
 * @return  FLCN_ERR_NOT_SUPPORTED
 *      Cooler object does not support this interface.
 * @return  FLCN_OK
 *      Requested RPM correction was performed successfully.
 */
#define FanCoolerRpmSet(fname) FLCN_STATUS (fname)(FAN_COOLER *pCooler, LwS32 rpm)

/*!
 * @interface FAN_COOLER
 *
 * Obtains coolers PWM rate in percents (as SFXP16_16)
 *
 * @param[in]   pCooler     FAN_COOLER object pointer
 * @param[in]   bActual     Set if actual/measured PWM rate is required
 * @param[out]  pPwm        Buffer to store cooler's PWM rate
 *
 * @return  FLCN_ERR_NOT_SUPPORTED
 *      Cooler object does not support this interface.
 * @return  FLCN_OK
 *      Requested PWM rate was successfully retrieved.
 */
#define FanCoolerPwmGet(fname) FLCN_STATUS (fname)(FAN_COOLER *pCooler, LwBool bActual, LwSFXP16_16 *pPwm)

/*!
 * @interface FAN_COOLER
 *
 * Sets the desired cooler's PWM rate in percents (as SFXP16_16)
 *
 * @param[in]   pCooler     FAN_COOLER object pointer
 * @param[in]   bBound      Set if PWM needs to be bounded
 * @param[out]  pwm         Desired PWM rate of the cooler
 *
 * @return  FLCN_ERR_NOT_SUPPORTED
 *      Cooler object does not support this interface.
 * @return  FLCN_OK
 *      Requested PWM rate was applied successfully.
 */
#define FanCoolerPwmSet(fname) FLCN_STATUS (fname)(FAN_COOLER *pCooler, LwBool bBound, LwSFXP16_16 pwm)

/*!
 * Container to hold data for all FAN_COOLERS.
 */
struct FAN_COOLERS
{
    /*!
     * Board Object Group of all FAN_COOLER objects.
     */
    BOARDOBJGRP_E32 super;
};

/*!
 * Extends BOARDOBJ providing attributes common to all FAN_COOLERS.
 */
struct FAN_COOLER
{
    /*!
     * BOARDOBJ super-class.
     */
    BOARDOBJ         super;
};

/* ------------------------- Defines ---------------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
BoardObjGrpIfaceModel10CmdHandler (fanCoolersBoardObjGrpIfaceModel10Set)
    GCC_ATTRIB_SECTION("imem_thermLibFanCommonConstruct", "fanCoolersBoardObjGrpIfaceModel10Set");

FLCN_STATUS           fanCoolersLoad(void);

BoardObjGrpIfaceModel10ObjSet     (fanCoolerGrpIfaceModel10ObjSetImpl_SUPER)
    GCC_ATTRIB_SECTION("imem_thermLibFanCommonConstruct", "fanCoolerGrpIfaceModel10ObjSetImpl_SUPER");

BoardObjGrpIfaceModel10CmdHandler (fanCoolersBoardObjGrpIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_thermLibFanCommon", "fanCoolersBoardObjGrpIfaceModel10GetStatus");

FanCoolerLoad         (fanCoolerLoad);

FanCoolerRpmGet       (fanCoolerRpmGet);

FanCoolerRpmSet       (fanCoolerRpmSet);

FanCoolerPwmGet       (fanCoolerPwmGet);

FanCoolerPwmSet       (fanCoolerPwmSet);

/*!
 * Board Object interfaces.
 */
BoardObjGrpIfaceModel10SetEntry   (fanFanCoolerIfaceModel10SetEntry)
    GCC_ATTRIB_SECTION("imem_thermLibFanCommonConstruct", "fanFanCoolerIfaceModel10SetEntry");
BoardObjIfaceModel10GetStatus               (fanFanCoolerIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_thermLibFanCommon", "fanFanCoolerIfaceModel10GetStatus");

/* ------------------------ Include Derived Types -------------------------- */
#include "fan/fancooler_active.h"

#endif // FAN_COOLER_H
