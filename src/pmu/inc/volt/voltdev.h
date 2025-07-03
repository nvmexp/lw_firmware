/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   voltdev.h
 * @brief  VOLT Voltage Device Model
 *
 * @details This module is a collection of functions managing and manipulating state
 * related to the volt device which controls/regulates the voltage on an
 * output power rail (e.g. LWVDD).
 */

#ifndef VOLTDEV_H
#define VOLTDEV_H

#include "g_voltdev.h"

#ifndef G_VOLTDEV_H
#define G_VOLTDEV_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobjgrp_iface_model_10.h"
#include "boardobj/boardobj_iface_model_10.h"

/* ------------------------- Macros ---------------------------------------- */
/*!
 * @copydoc BOARDOBJGRP_GRP_GET
 */
#define BOARDOBJGRP_DATA_LOCATION_VOLT_DEVICE \
    (&(Volt.devices.super))

/*!
 * @copydoc BOARDOBJGRP_OBJ_GET
 */
#define VOLT_DEVICE_GET(_objIdx) \
    (BOARDOBJGRP_OBJ_GET(VOLT_DEVICE, (_objIdx)))

/* ------------------------- Datatypes ------------------------------------- */
typedef struct VOLT_DEVICE VOLT_DEVICE, VOLT_DEVICE_BASE;

// VOLT_DEVICE interfaces

/*!
 * @brief Gets voltage of the appropriate VOLT_DEVICE.
 *
 * @param[in]  pDevice      VOLT_DEVICE object pointer
 * @param[out] pVoltageuV   Current voltage
 *
 * @return  FLCN_ERR_NOT_SUPPORTED
 *      Device object does not support this interface.
 * @return  FLCN_ERR_ILWALID_STATE
 *      Invalid period value for the device.
 * @return  FLCN_OK
 *      Requested current voltage was obtained successfully.
 */
#define VoltDeviceGetVoltage(fname) FLCN_STATUS (fname)(VOLT_DEVICE *pDevice, LwU32 *pVoltageuV)

/*!
 * @brief Sets voltage on the appropriate VOLT_DEVICE.
 *
 * @param[in]  pDevice      VOLT_DEVICE object pointer
 * @param[in]  voltageuV    Target voltage to set in uV
 * @param[in]  bTrigger     Boolean to trigger the voltage switch into the HW
 * @param[in]  bWait        Boolean to wait for settle time after switch
 * @param[in]  railAction   Control action to be applied on the rail
 *
 * @return  FLCN_ERR_NOT_SUPPORTED
 *      Device object does not support this interface.
 * @return  FLCN_OK
 *      Requested target voltage was set successfully.
 */
#define VoltDeviceSetVoltage(fname) FLCN_STATUS (fname)(VOLT_DEVICE *pDevice, LwU32 voltageuV, LwBool bTrigger, LwBool bWait, LwU8 railAction)

/*!
 * @brief Round voltage to a value supported by the VOLT_DEVICE.
 *
 * @param[in]     pDevice       VOLT_DEVICE object pointer
 * @param[in/out] pVoltageuV    Rounded value
 * @param[in]     bRoundUp      Boolean to round up or down
 * @param[in]     bBound
 *      Boolean flag indicating whether the rounded value should be bound to
 *      the range of voltages supported on the regulator.  If this flag is
 *      LW_FALSE and the provided value is outside the range, the value will
 *      be rounded (if possible) but outside the range of supported voltages.
 *
 * @return FLCN_OK
 *     Voltage successfully rounded to a supported value.
 */
#define VoltDeviceRoundVoltage(fname) FLCN_STATUS (fname)(VOLT_DEVICE *pDevice, LwS32 *pVoltageuV, LwBool bRoundUp, LwBool bBound)

/*!
 * @brief Configure this VOLT_DEVICE.
 *
 * @param[in]   pDevice   VOLT_DEVICE object pointer
 * @param[in]   bEnable   Denotes if this VOLT_DEVICE should be enabled/disabled
 *
 * @return  FLCN_ERR_NOT_SUPPORTED
 *      Device object does not support this interface.
 * @return  FLCN_OK
 *      VOLT_DEVICE was enabled/disabled successfully.
 */
#define VoltDeviceConfigure(fname) FLCN_STATUS (fname)(VOLT_DEVICE *pDevice, LwBool bEnable)

/*!
 * @brief Sanity check for VOLT_DEVICE SW state.
 *
 * @param[in]   pDevice    VOLT_DEVICE object pointer
 * @param[in]   railAction Rail action requested by client
 *
 * @return  FLCN_OK
 *      VOLT_DEVICE was sanitized successfully.
 * @return Other errors from child functions.
 */
#define VoltDeviceSanityCheck(fname) FLCN_STATUS (fname)(VOLT_DEVICE *pDevice, LwU8 railAction)

/*!
 * @brief Sanity check for loading VOLT_DEVICE SW state.
 *
 * @param[in]   pDevice    VOLT_DEVICE object pointer
 *
 * @return  FLCN_OK
 *      VOLT_DEVICE was sanitized successfully.
 * @return Other errors from child functions.
 */
#define VoltDeviceLoadSanityCheck(fname) FLCN_STATUS (fname)(VOLT_DEVICE *pDevice)

/*!
 * @brief Extends BOARDOBJ providing attributes common to all VOLT_DEVICES.
 */
struct VOLT_DEVICE
{
    /*!
     * @brief @ref BOARDOBJ super-class.
     */
    BOARDOBJ                 super;

    /*!
     * @brief Voltage Switch delay in us
     */
    LwU32                    switchDelayus;

    /*!
     * @brief Min voltage
     */
    LwU32                   voltageMinuV;

    /*!
     * @brief Max voltage
     */
    LwU32                   voltageMaxuV;

    /*!
     * @brief Voltage step value in uV
     */
    LwU32                    voltStepuV;

#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_ACTION))
    /*!
     * @brief Voltage regulator delay required during power up in us.
     */
    LwU16                    powerUpSettleTimeus;

    /*!
     * @brief Voltage regulator delay required during power down in us.
     */
    LwU16                    powerDownSettleTimeus;
#endif // PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_ACTION)
};

/*!
 * @brief Defines the structure that holds data about voltage domains for
 *        different VOLT_RAILs.
 */
typedef struct
{
    /*!
     * @brief Number of VOLT_RAILs.
     */
    LwU8    numDomains;

    /*!
     * @brief List of voltage domain entries.
     */
    LwU8  voltDomains[LW2080_CTRL_VOLT_VOLT_DOMAIN_MAX_ENTRIES];
} VOLT_DOMAINS_LIST;

/* ------------------------- Defines ---------------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
BoardObjGrpIfaceModel10CmdHandler   (voltDeviceBoardObjGrpIfaceModel10Set)
    GCC_ATTRIB_SECTION("imem_libVoltConstruct", "voltDeviceBoardObjGrpIfaceModel10Set");

BoardObjGrpIfaceModel10ObjSet       (voltDeviceGrpIfaceModel10ObjSetImpl_SUPER)
    GCC_ATTRIB_SECTION("imem_libVoltConstruct", "voltDeviceGrpIfaceModel10ObjSetImpl_SUPER");

FLCN_STATUS             voltDevicesLoad(void);

VoltDeviceSetVoltage    (voltDeviceSetVoltage_SUPER);

VoltDeviceConfigure     (voltDeviceConfigure);

void                    voltDeviceSwitchDelayApply(VOLT_DEVICE *pDevice);

mockable VoltDeviceGetVoltage    (voltDeviceGetVoltage);

//
// Most PERF code needs VOLT rounding functions.  Placing in "imem_perfVf" so
// that PERF client code doesn't need to attach.
//
mockable VoltDeviceRoundVoltage  (voltDeviceRoundVoltage)
    GCC_ATTRIB_SECTION("imem_perfVf", "voltDeviceRoundVoltage");

mockable VoltDeviceSetVoltage    (voltDeviceSetVoltage);

VoltDeviceSanityCheck            (voltDeviceSanityCheck);

VoltDeviceLoadSanityCheck        (voltDeviceLoadSanityCheck);
/*!
 * Board Object interfaces.
 */
BoardObjGrpIfaceModel10SetEntry   (voltVoltDeviceIfaceModel10SetEntry)
    GCC_ATTRIB_SECTION("imem_libVoltConstruct", "voltVoltDeviceIfaceModel10SetEntry");

/* ------------------------ Include Derived Types -------------------------- */
#include "volt/voltdev_vid.h"
#include "volt/voltdev_pwm.h"

#endif // G_VOLTDEV_H
#endif // VOLTDEV_H
