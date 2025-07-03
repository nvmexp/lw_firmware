/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file objfan.h
 * @brief @copydoc objfan.c
 */

#ifndef OBJFAN_H
#define OBJFAN_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "fan/fancooler.h"
#include "fan/fanpolicy.h"
#include "fan/fan3x/fanarbiter.h"
#include "lwostimer.h"

/* ------------------------- Macros ---------------------------------------- */
/* ------------------------- Datatypes ------------------------------------- */

/*!
 * FAN Object Definition
 */
typedef struct
{
#if PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_COOLER)
    /*!
     * @copydoc FAN_COOLERS
     */
    FAN_COOLERS     coolers;
#endif // PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_COOLER)

#if PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_POLICY)
    /*!
     * @copydoc FAN_POLICIES
     */
    FAN_POLICIES    policies;
#endif // PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_POLICY)

#if PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_ARBITER)
    /*!
     * @copydoc FAN_ARBITERS
     */
    FAN_ARBITERS    arbiters;
#endif // PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_ARBITER)

    /*!
     * Set if task is detached and all necessary lib overlays are preloaded.
     */
    LwBool          bTaskDetached;

    /*!
     * Boolean to indicate whether Fan Control 2.X+ should be enabled in PMU.
     */
    LwBool          bFan2xAndLaterEnabled;
} OBJFAN;

/* ------------------------ External Definitions --------------------------- */
extern  OBJFAN  Fan;

/* --------------------------------- Macros -------------------------------- */

/*!
 * @brief   Helper macro to determine which of thermLibSensor2X and libi2c
 *          overlays is to be used.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_POLICY_THERM_CHANNEL_USE)
#define THERMLIBSENSOR2X_OR_LIBI2C_OVERLAY_USE                                \
            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, thermLibSensor2X)
#else
#define THERMLIBSENSOR2X_OR_LIBI2C_OVERLAY_USE                                \
            OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, libi2c)
#endif

/*!
 * @brief   Colwenience macro for the FAN 2X+ overlays attachment/detachment.
 *
 * @note    Review the use cases in Tasks.pm and update MAX_OVERLAYS_IMEM_LS
 *          after updates to this macro. Failure to do so might result in
 *          falcon halt. Also when updating this macro please review all use
 *          cases using this macro. Failure to do so might result in unexpected
 *          issues.
 */
#define OSTASK_OVL_DESC_DEFINE_FAN_CTRL_COMMON_BASE                           \
            OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, smbpbi)                     \
            THERMLIBSENSOR2X_OR_LIBI2C_OVERLAY_USE                            \
            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libBoardObj)

/* ------------------------ Function Prototypes ---------------------------- */
void fanConstruct(void)
    GCC_ATTRIB_SECTION("imem_init", "fanConstruct");

FLCN_STATUS fanLoad(void);

/* ------------------------ Include Derived Types -------------------------- */

#endif // OBJFAN_H
