/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  pwrdev.h
 * @brief Interface Specification for Power Devices.
 *
 * A 'Power Device' is any device capable of some form of current/voltage
 * reporting on a specific power-rail. This could be provided by an actual
 * power sensor or even by a voltage regulator with output reporting
 * capability.
 *
 * The interfaces defined here are generic and intended to abstract away any
 * device-specific attributes (i2c information, protocol specifics, etc ...).
 */

#ifndef PWRDEV_H
#define PWRDEV_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "pmgr/i2cdev.h"
#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobjgrp_iface_model_10.h"
#include "boardobj/boardobj_iface_model_10.h"

/* ------------------------- Types Definitions ----------------------------- */
typedef struct PWR_DEVICE PWR_DEVICE, PWR_DEVICE_BASE;

/* ------------------------- Macros ---------------------------------------- */
/*!
 * @copydoc BOARDOBJGRP_GRP_GET
 */
#define BOARDOBJGRP_DATA_LOCATION_PWR_DEVICE \
    (&(Pmgr.pwr.devices.super))

/*!
 * @copydoc BOARDOBJGRP_OBJ_GET
 */
#define PWR_DEVICE_GET(_objIdx) \
    (BOARDOBJGRP_OBJ_GET(PWR_DEVICE, (_objIdx)))

/*!
 * @copydoc BOARDOBJGRP_IS_VALID
 */
#define PWR_DEVICE_IS_VALID(_objIdx) \
    (BOARDOBJGRP_IS_VALID(PWR_DEVICE, (_objIdx)))

/*!
 * Main structure for an object which implments the PWR_DEVICE interface.
 */
struct PWR_DEVICE
{
    BOARDOBJ     super;      //<! Board Object super class

    /*!
     * Pointer to the corresponding I2C_DEVICE object when this device supports
     * and I2C interface (NULL otherwise).
     */
    I2C_DEVICE  *pI2cDev;

    /*!
     * Fixed-point multiplier used as a correction-factor for the power before
     * being returned by getPower().
     */
    LwUFXP20_12  powerCorrFactor;
};

/*!
 * @interface PWR_DEVICE
 *
 * Responsible for device-specific HW-initialization and configuration. This
 * may include writting out configuration parameters to the device or reading
 * static data from the device to avoid unnecessary device communication later.
 *
 * @note
 *     This PWR_DEVICE interface is optional. The need for this function is
 *     purely a function of the device-type. If no HW-actions are required for
 *     this device-type do not provide an implementation and simply allow the
 *     stub in @ref pwrdev.c return FLCN_OK when called for instances of this
 *     type.
 *
 * @param[in] pDev   Pointer to a PWR_DEVICE object
 *
 * @return FLCN_OK
 *     Upon successful hardware initialization/configuration.
 *
 * @return pmuI2cError<xyz>
 *     Internal I2C failure (Address NACK, SCL timeout, etc ...) for I2C power
 *     devices.
 */
#define PwrDevLoad(fname) FLCN_STATUS (fname) (PWR_DEVICE *pDev)

/*!
 * @interface PWR_DEVICE
 *
 * Get a voltage reading (in uV) from the power device.
 *
 * @param[in]  pDev        Pointer to the power device object
 * @param[in]  provIdx
 *     Provider Index for which caller wants corresponding voltage.
 * @param[out] pVoltageuV  Written with the voltage reading (in uV)
 *
 * @return FLCN_OK
 *     Upon successful retrieval of the voltage reading
 *
 * @return FLCN_ERR_NOT_SUPPORTED
 *     The given device object does not support this PWR_DEVICE interface. Also
 *     returned if the device does not support this particular PWR_DEVICE
 *     interface.
 *
 * @return FLCN_ERROR
 *     Unexpected internal error (such as a failure to issue the I2C request or
 *     I2C power devices)
 *
 * @return FLCN_ERR_TIMEOUT
 *     A timeout fired before the sample was acquired
 *
 * @return pmuI2cError<xyz>
 *     Internal I2C failure (Address NACK, SCL timeout, etc ...) for I2C power
 *     devices.
 */
#define PwrDevGetVoltage(fname) FLCN_STATUS (fname) (PWR_DEVICE *pDev, LwU8 provIdx, LwU32 *pVoltageuV)

/*!
 * @interface PWR_DEVICE
 *
 * Get a current reading (in mA) from the power device.
 *
 * @param[in]  pDev        Pointer to the power device object
 * @param[in]  provIdx
 *     Provider Index for which caller wants corresponding voltage.
 * @param[out] pLwrrentmA  Written with the current reading (in mA)
 *
 * @return FLCN_OK
 *     Upon successful retrieval of the current reading
 *
 * @return FLCN_ERR_NOT_SUPPORTED
 *     The given device object does not support this PWR_DEVICE interface. Also
 *     returned if the device does not support this particular PWR_DEVICE
 *     interface.
 *
 * @return FLCN_ERROR
 *     Unexpected internal error (such as a failure to issue the I2C request or
 *     I2C power devices)
 *
 * @return FLCN_ERR_TIMEOUT
 *     A timeout fired before the sample was acquired
 *
 * @return pmuI2cError<xyz>
 *     Internal I2C failure (Address NACK, SCL timeout, etc ...) for I2C power
 *     devices.
 */
#define PwrDevGetLwrrent(fname) FLCN_STATUS (fname) (PWR_DEVICE *pDev, LwU8 provIdx, LwU32 *pLwrrentmA)

/*!
 * @interface PWR_DEVICE
 *
 * Get a power reading (in mW) from the power device.
 *
 * @param[in]  pDev     Pointer to the power device object
 * @param[in]  provIdx
 *     Provider Index for which caller wants corresponding voltage.
 * @param[out] pPowermW Written with the power reading
 *
 * @return FLCN_OK
 *     Upon successful retrieval of the power reading
 *
 * @return FLCN_ERR_NOT_SUPPORTED
 *     The given device object does not support this PWR_DEVICE interface.
 *
 * @return FLCN_ERROR
 *     Unexpected internal error (such as a failure to issue the I2C request or
 *     I2C power devices)
 *
 * @return FLCN_ERR_TIMEOUT
 *     A timeout fired before the sample was acquired
 *
 * @return FLCN_ERR_PWR_DEVICE_TAMPERED
 *     Returned if the device has detected tampering of its configuration (ie.
 *     current configuration is compromised). The means for tamper-detection
 *     (as well as the necessity to perform it) is device-specific. Usually
 *     however, its performed by checking for inconsistencies between the
 *     values SW expects to be programmed on the device and the actual settings
 *     read from the device.
 *
 * @return pmuI2cError<xyz>
 *     Internal I2C failure (Address NACK, SCL timeout, etc ...) for I2C power
 *     devices.
 */
#define PwrDevGetPower(fname) FLCN_STATUS (fname) (PWR_DEVICE *pDev, LwU8 provIdx, LwU32 *pPowermW)

/*!
 * @interface PWR_DEVICE
 *
 * Optional PWR_DEVICE interface primarily applicable to voltage-regulators.
 * May be used to inform the device object of changes to the point set-point
 * voltage that may have been initiated and carried-out externally (such as by
 * the RM).
 *
 * @param[in]  pBoardObj      Pointer to the board object
 * @param[in]  voltageuV
 *     Current set-point voltage programmed on the device. Note that this
 *     interface does not require that this value change per-call. The only
 *     requirement on the voltage is that it remains an accurate reflection of
 *     the set-point voltage programmed on the device.
 *
 * @return FLCN_OK
 *     Upon successful notification to the device object.
 *
 * @return FLCN_ERR_NOT_SUPPORTED
 *     The given device object does not support this PWR_DEVICE interface.
 */
#define PwrDevVoltageSetChanged(fname) FLCN_STATUS (fname)(BOARDOBJ *pBoardObj, LwU32 voltageuV)

/*!
 * @interface PWR_DEVICE
 *
 * Set a power/current limit value for the device to apply to the rail(s) it
 * is connected to. Devices implementing this interface must provide some means
 * of controlling power on the rail. The means of power-control is by definition
 * device-specific but may include things like direct voltage regulation or
 * internal power-monitoring combined with GPU-assisted slowdown during
 * power/current overage conditions.
 *
 * The power-device model is required to report a PWR_DEVICE_ALERT_OVER_POWER
 * alert or PWR_DEVICE_ALERT_OVER_LWRRENT alert (when queried) when the
 * power/current limit was exceeded and additional upper-level software
 * assistance is required.
 *
 * This interface may also be used to remove a limit (if one is in effect). To
 * accomplish this, simply pass in @ref PWR_DEVICE_NO_LIMIT as the power/current
 * limit. That value is therefore reserved. The limit may also be removed by
 * using the @ref pwrDevRemoveLimit macro (which is just a wrapper for
 * this interface).
 *
 * @param[in]   pDev        Pointer to the power device object
 * @param[in]   provIdx
 *     Provider Index for which caller wants corresponding voltage.
 * @param[in]   limitIdx
       Threshold limit index indication which type of threshold we are applying.
 * @param[in]   limitUnit
 *      Units of requested limit as LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_UNIT_<xyz>
 * @param[in]   limitValue  Limit value to apply
 *
 * @return FLCN_OK
 *     Upon successful application of the limit value.
 *
 * @return FLCN_ERR_NOT_SUPPORTED
 *     The given device object does not support this PWR_DEVICE interface.
 *
 * @return pmuI2cError<xyz>
 *     Internal I2C failure (Address NACK, SCL timeout, etc ...) for I2C power
 *     devices.
 */
#define PwrDevSetLimit(fname) FLCN_STATUS (fname)(PWR_DEVICE *pDev, LwU8 provIdx, LwU8 limitIdx, LwU8 limitUnit, LwU32 limitValue)

/*!
 * @interface PWR_DEVICE
 *
 * This interface is used to retrieve the cached limits for the provider and
 * threshold limit index specified. These limits are cached by the PWR_DEVICE
 * using @ref PwrDevCacheLimit interface when we set these limits.
 *
 * @param[in]   pDev        Pointer to the power device object
 * @param[in]   provIdx
 *     Provider Index for which caller wants the limit.
 * @param[in]   limitIdx
       Threshold limit index indicating the type of threshold we want.
 * @param[out]  pBuf        Pointer to payload buffer.
 *
 * @return FLCN_OK
 *     Upon successful retrieval of the limit value.
 *
 * @return FLCN_ERR_NOT_SUPPORTED
 *     The given device object does not support this PWR_DEVICE interface.
 */
#define PwrDevGetLimit(fname) FLCN_STATUS (fname)(PWR_DEVICE *pDev, LwU8 provIdx, LwU8 limitIdx, RM_PMU_BOARDOBJ *pBuf)

/*!
 * @interface PWR_DEVICE
 *
 * Performs periodic updates of the power device's internal state.
 *
 * @param[in]   pDev    Pointer to the power device object
 *
 * @return FLCN_OK
 *     Upon successful update of the internal state.
 *
 * @return FLCN_ERR_NOT_SUPPORTED
 *     The given device object does not support this PWR_DEVICE interface.
 */
#define PwrDevStateSync(fname) FLCN_STATUS (fname)(PWR_DEVICE *pDev)

/*!
 * @interface PWR_DEVICE
 *
 * Get violation counter raw data of this provIdx / limitIdx combination.
 *
 * @param[in]   pDev            Pointer to the power device object
 * @param[in]   provIdx         Provider index
 * @param[in]   limitIdx        The limit index this violation counter
                                corresponds to
 * @param[out]  pViolateCnt     Violation counter raw data
 *
 * @return FLCN_OK
 *     Upon successful retrieval of requested value.
 *
 * @return FLCN_ERR_NOT_SUPPORTED
 *     The given device object does not support this PWR_DEVICE interface.
 */
typedef FLCN_STATUS PwrDevViolateCntGet(PWR_DEVICE *pDev, LwU8 provIdx, LwU8 limitIdx, LwU32 *pCounter);

/*!
 * @interface PWR_DEVICE
 *
 * Get Power (in mW), Current (in mA), Voltage (in uV) and Energy (in mJ)
 * from the power device.
 *
 * @param[in]  pDev     Pointer to the power device object
 * @param[in]  provIdx
 *     Provider Index for which caller wants corresponding voltage.
 * @param[out] pTuple   @ref LW2080_CTRL_PMGR_PWR_TUPLE.
 *
 * @return FLCN_OK
 *     Upon successful retrieval of the power reading
 *
 * @return FLCN_ERR_NOT_SUPPORTED
 *     The given device object does not support this PWR_DEVICE interface.
 *
 * @return FLCN_ERROR
 *     Unexpected internal error (such as a failure to issue the I2C request or
 *     I2C power devices)
 *
 * @return FLCN_ERR_TIMEOUT
 *     A timeout fired before the sample was acquired
 *
 * @return FLCN_ERR_PWR_DEVICE_TAMPERED
 *     Returned if the device has detected tampering of its configuration (ie.
 *     current configuration is compromised). The means for tamper-detection
 *     (as well as the necessity to perform it) is device-specific. Usually
 *     however, its performed by checking for inconsistencies between the
 *     values SW expects to be programmed on the device and the actual settings
 *     read from the device.
 *
 * @return pmuI2cError<xyz>
 *     Internal I2C failure (Address NACK, SCL timeout, etc ...) for I2C power
 *     devices.
 */
#define PwrDevTupleGet(fname) FLCN_STATUS (fname)(PWR_DEVICE *pDev, LwU8 provIdx, \
                        LW2080_CTRL_PMGR_PWR_TUPLE *pTuple)

/*!
 * @interface PWR_DEVICE
 *
 * Get Power Aclwmulation(Energy) (in nJ), Current Aclwmulation(Charge) (in nC)
 * and Voltage Aclwmulation (in mV*us) from the power device.
 *
 * @param[in]  pDev     Pointer to the power device object
 * @param[in]  provIdx
 *     Provider Index for which caller wants corresponding voltage.
 * @param[out] pTupleAcc   @ref LW2080_CTRL_PMGR_PWR_TUPLE_ACC.
 *
 * @return FLCN_OK
 *     Upon successful retrieval of the power reading
 *
 * @return FLCN_ERR_NOT_SUPPORTED
 *     The given device object does not support this PWR_DEVICE interface.
 *
 * @return FLCN_ERROR
 *     Unexpected internal error (such as a failure to issue the I2C request or
 *     I2C power devices)
 *
 * @return FLCN_ERR_TIMEOUT
 *     A timeout fired before the sample was acquired
 *
 */
#define PwrDevTupleAccGet(fname) FLCN_STATUS (fname)(PWR_DEVICE *pDev, LwU8 provIdx, \
                        LW2080_CTRL_PMGR_PWR_TUPLE_ACC *pTupleAcc)

/*!
 * Implement this interface to update baseSamplingPeriod when a new Perf update
 * is sent to PMU.
 *
 * @param[in]  pStatus     new RM_PMU_PERF_STATUS to be updated
 */
#define PwrDevProcessPerfStatus(fname) void (fname)(RM_PMU_PERF_STATUS *pStatus)

/* ------------------------- Defines ---------------------------------------- */

/*!
 * The following is a list of alerts representing both conditions that the
 * physical device may be reporting or virtual conditions generated by the
 * software power device model. Upper-level software may query for these alerts
 * through use of the @ref pwrDeviceGetAlerts interface.
 */
#define PWR_DEVICE_ALERT_OVER_VOLTAGE                                     BIT(0)
#define PWR_DEVICE_ALERT_OVER_LWRRENT                                     BIT(1)
#define PWR_DEVICE_ALERT_OVER_POWER                                       BIT(2)
#define PWR_DEVICE_ALERT_ALL                                                   \
    (PWR_DEVICE_ALERT_OVER_VOLTAGE |                                           \
     PWR_DEVICE_ALERT_OVER_LWRRENT |                                           \
     PWR_DEVICE_ALERT_OVER_POWER)

/*!
 * Define a reserved limit value that may be passed into @ref
 * pwrDevSetLimit as an indication to the interface that any limit value
 * lwrrenly in-effect should be removed and/or disabled.
 */
#define PWR_DEVICE_NO_LIMIT                                           LW_U32_MAX

/*!
 * This is a colwenience macro that may be used to remove a power/current limit
 * that may be active on the device.  It is disguised as to appear
 * as a function to keep the naming of interfaces uniform without requiring a
 * standalone (and largely unnecessary) limit-removal function.
 *
 * @param[in]   pDev    Pointer to the power device object
 * @param[in]   provIdx
 *     Provider Index for which caller wants to remove a power or current limit
 * @param[in]   limitUnit
 *      Units of requested limit as LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_UNIT_<xyz>
 *
 * @return FLCN_OK
 *     Upon successful removal of the limit.
 *
 * @return FLCN_ERR_NOT_SUPPORTED
 *     The given device object does not support this PWR_DEVICE interface.
 *
 * @return pmuI2cError<xyz>
 *     Internal I2C failure (Address NACK, SCL timeout, etc ...) for I2C power
 *     devices.
 */
#define pwrDevRemoveLimit(pDev, provIdx, limitUnit)                            \
    pwrDevSetLimit(pDev, provIdx, limitUnit, PWR_DEVICE_NO_LIMIT)

#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_I2C_DEVICE)
#define OSTASK_OVL_DESC_DEFINE_PWR_POLICIES_I2C_DEVICE_LOAD                    \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libi2c)
#else
#define OSTASK_OVL_DESC_DEFINE_PWR_POLICIES_I2C_DEVICE_LOAD                    \
    OSTASK_OVL_DESC_ILWALID
#endif

/*!
 * @brief   List of an overlay descriptors required by a pwr devices load code.
 *
 * @note    Review the use cases in Tasks.pm and update MAX_OVERLAYS_IMEM after
 *          updates to this macro. Failure to do so might result in falcon halt.
 */
#define OSTASK_OVL_DESC_DEFINE_PWR_DEVICES_LOAD                                \
    OSTASK_OVL_DESC_DEFINE_PWR_POLICIES_I2C_DEVICE_LOAD

/*!
 * @brief   List of an overlay descriptors required by a pwr device state sync.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_DEVICE_GPUADC10)
#define OSTASK_OVL_DESC_DEFINE_PWR_DEVICE_STATE_SYNC                           \
    OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, pmgrPwrDeviceStateSync)
#else
#define OSTASK_OVL_DESC_DEFINE_PWR_DEVICE_STATE_SYNC                           \
    OSTASK_OVL_DESC_ILWALID
#endif

/* ------------------------- Function Prototypes  --------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * PWR_DEV interfaces
 */
PwrDevLoad              (pwrDevLoad)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "pwrDevLoad");
PwrDevGetVoltage        (pwrDevGetVoltage)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrDevGetVoltage");
PwrDevGetLwrrent        (pwrDevGetLwrrent)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrDevGetLwrrent");
PwrDevGetPower          (pwrDevGetPower)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrDevGetPower");
PwrDevVoltageSetChanged (pwrDevVoltageSetChanged)
    GCC_ATTRIB_SECTION("imem_pmgrPwrDeviceStateSync", "pwrDevVoltageSetChanged");
PwrDevSetLimit          (pwrDevSetLimit)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "pwrDevSetLimit");
PwrDevGetLimit          (pwrDevGetLimit)
    GCC_ATTRIB_SECTION("imem_pmgrLibQuery", "pwrDevGetLimit");
PwrDevStateSync         (pwrDevStateSync)
    GCC_ATTRIB_SECTION("imem_pmgrPwrDeviceStateSync", "pwrDevStateSync");
PwrDevTupleGet          (pwrDevTupleGet)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrDevTupleGet");
PwrDevTupleGet          (pwrDevTupleFullRangeGet)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrDevTupleFullRangeGet");
PwrDevTupleAccGet       (pwrDevTupleAccGet)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrDevTupleAccGet");
BoardObjIfaceModel10GetStatus           (pwrDevIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_pmgrLibQuery", "pwrDevIfaceModel10GetStatus");
BoardObjIfaceModel10GetStatus           (pwrDevIfaceModel10GetStatus_SUPER)
    GCC_ATTRIB_SECTION("imem_pmgrLibQuery", "pwrDevIfaceModel10GetStatus_SUPER");

/*!
 * Callback functions for updating power device.
 * Lwrrently we are using this callback routines only for updating BA devices.
 */
PwrDevProcessPerfStatus (pwrDevProcessPerfStatus);
void pwrDevScheduleCallback(LwU32 ticksNow, LwBool bLowSampling)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "pwrDevScheduleCallback");
void pwrDevCancelCallback(void)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "pwrDevCancelCallback");

/*!
 * Top level function to update all Power Devices when Voltage change happens
 */
void pwrDevVoltStateSync(void)
    GCC_ATTRIB_SECTION("imem_pmgrPwrDeviceStateSync", "pwrDevVoltStateSync");

/*!
 * Constructor for the PWR_DEVICE super class
 */
BoardObjGrpIfaceModel10ObjSet       (pwrDevGrpIfaceModel10ObjSetImpl_SUPER)
    GCC_ATTRIB_SECTION("imem_pmgrLibConstruct", "pwrDevGrpIfaceModel10ObjSetImpl_SUPER");

/*!
 * Board Object interfaces.
 */
BoardObjGrpIfaceModel10CmdHandler       (pwrDeviceBoardObjGrpIfaceModel10Set)
    GCC_ATTRIB_SECTION("imem_pmgrLibBoardObj", "pwrDeviceBoardObjGrpIfaceModel10Set");
BoardObjGrpIfaceModel10CmdHandler       (pwrDeviceBoardObjGrpIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_pmgrLibBoardObj", "pwrDeviceBoardObjGrpIfaceModel10GetStatus");
BoardObjGrpIfaceModel10SetHeader  (pmgrPwrDevIfaceModel10SetHeader)
    GCC_ATTRIB_SECTION("imem_pmgrLibConstruct", "pmgrPwrDevIfaceModel10SetHeader");
BoardObjGrpIfaceModel10SetEntry   (pmgrPwrDevIfaceModel10SetEntry)
    GCC_ATTRIB_SECTION("imem_pmgrLibConstruct", "pmgrPwrDevIfaceModel10SetEntry");

/* ------------------------- Debug Macros ---------------------------------- */
/* ------------------------- Child Class Includes -------------------------- */

#include "pmgr/pwrdev_ba1xhw.h"
#include "pmgr/pwrdev_ba20.h"
#include "pmgr/pwrdev_ina219.h"
#include "pmgr/pwrdev_ina3221.h"
#include "pmgr/pwrdev_nct3933u.h"
#include "pmgr/pwrdev_gpuadc1x.h"

#endif // PWRDEV_H
