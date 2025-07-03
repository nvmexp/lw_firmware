/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pwrdev_gpuadc10.h
 * @copydoc pwrdev_gpuadc10.c
 */

#ifndef PWRDEV_GPUADC10_H
#define PWRDEV_GPUADC10_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "pmgr/pwrdev_gpuadc1x.h"

/* ------------------------- Macros ---------------------------------------- */
/*!
 * Macro to access the callback flag for GPUADC10 from OBJPMGR
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_DEVICE_GPUADC10))
#define PMGR_PWR_DEVICE_CALLBACK_GPUADC10                                     \
    Pmgr.bDevCallbackGpuAdc10
#else
#define PMGR_PWR_DEVICE_CALLBACK_GPUADC10                                     \
    LW_FALSE
#endif

/* ------------------------- Types Definitions ----------------------------- */
typedef struct PWR_DEVICE_GPUADC10 PWR_DEVICE_GPUADC10;

/*!
 * Structure holding the parameters required for beacon feature internal to PMU.
 */
typedef struct
{
    /*!
     * Threshold Voltage (uV) corresponding to the beacon threshold specified
     * in GPUADC code in VBIOS
     */
    LwU32  thresholduV;
    /*!
     * Max spread Voltage (uV) corresponding to the beacon max spread specified
     * in GPUADC code in VBIOS
     */
    LwU32  maxSpreaduV;
    /*!
     * Pointer to structure containing the last channel status.
     */
    PWR_CHANNELS_STATUS *pChannelStatus;
} PWR_DEVICE_GPUADC10_BEACON;

struct PWR_DEVICE_GPUADC10
{
    /*!
     * Super PWR_DEVICE_GPUADC1X structure
     */
    PWR_DEVICE_GPUADC1X super;
    /*!
     * IPC configuration parameters
     */
    LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC10_IPC ipc;
    /*!
     * Beacon configuration parameters
     */
    LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC10_BEACON beacon;
    /*!
     * Beacon internal state
     */
    PWR_DEVICE_GPUADC10_BEACON beaconState;
    /*!
     * Structure holding the parameters required to evaluate tuple.
     */
    PWR_DEVICE_GPUADC1X_TUPLE_PARAMS tupleParams;
    /*!
     * Structure holding the parameters for each provider of GPUADC
     */
    PWR_DEVICE_GPUADC1X_PROV
        prov[LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC10_PHYSICAL_PROV_NUM];
};

/* ------------------------- Inline Functions  ----------------------------- */
/*!
 * @brief   Colwert @ref PWR_DEVICE_GPUADC1X::fullRangeVoltage
 *     from FXP8.8 (V) to FXP32.0 (mV).
 *
 * @param[in]  pGpuAdc10     Pointer to @ref PWR_DEVICE_GPUADC10
 *
 * @return  LwU32 representing voltage in mV.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline LwU32
pwrDevGpuAdc10FullRangeVoltmVGet
(
    PWR_DEVICE_GPUADC10      *pGpuAdc10,
    PWR_DEVICE_GPUADC1X_PROV *pProv
)
{
    LwU32 voltmV = pGpuAdc10->super.fullRangeVoltage;

    //
    // Multiplying fullRangeVoltage by 1000 to colwert to mV
    //   8.8 V * 10.0 mV/V => 18.8 mV
    // Discarding 8 fractional bits
    //
    if (voltmV <= (LW_U32_MAX / 1000U))
    {
        voltmV *= 1000U;
        voltmV  = LW_TYPES_UFXP_X_Y_TO_U32_ROUNDED(24, 8, voltmV);
    }
    else
    {
        voltmV = LW_U32_MAX;
        PMU_BREAKPOINT();
    }
    return voltmV;
}

/*!
 * @brief   Colwert @ref PWR_DEVICE_GPUADC1X_PROV::fullRangeLwrrent
 *     from FXP24.8 (A) to FXP32.0 (mA).
 *
 * @param[in]  pProv    Pointer to @ref PWR_DEVICE_GPUADC1X_PROV
 *
 * @return  LwU32 representing current in mA.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline LwU32
pwrDevGpuAdc10FullRangeLwrrmAGet
(
    PWR_DEVICE_GPUADC10      *pGpuAdc10,
    PWR_DEVICE_GPUADC1X_PROV *pProv
)
{
    LwU32 lwrrmA = pProv->fullRangeLwrrent;

    //
    // fullRangeLwrrent is always within FXP12.8 (A) for GPUADC1X
    // Multiplying fullRangeLwrrent by 1000 to colwert to mA
    //   12.8 A * 10.0 mA/A => 22.8 mA
    // Discarding 8 fractional bits
    //
    if (lwrrmA <= (LW_U32_MAX / 1000U))
    {
        lwrrmA *= 1000U;
        lwrrmA  = LW_TYPES_UFXP_X_Y_TO_U32_ROUNDED(24, 8, lwrrmA);
    }
    else
    {
        lwrrmA = LW_U32_MAX;
        PMU_BREAKPOINT();
    }
    return lwrrmA;
}

/*!
 * @brief   Callwlate full range power in mW for given provider.
 *
 * @param[in]  pGpuAdc10 Pointer to @ref PWR_DEVICE_GPUADC10
 * @param[in]  pProv     Pointer to @ref PWR_DEVICE_GPUADC1X_PROV
 *
 * @return  LwU32 representing power in mW.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline LwU32
pwrDevGpuAdc10FullRangePwrmWGet
(
    PWR_DEVICE_GPUADC10      *pGpuAdc10,
    PWR_DEVICE_GPUADC1X_PROV *pProv
)
{
    LwU32 pwrmW;

    //
    // Multiplying fullRangeVoltage and fullRangeLwrrent
    //   8.8 V * 12.8 A      => 20.16 A
    // Discarding 4 fractional bits to keep within 32-bits
    //   20.16 >> 4          => 20.12 A
    // Multiplying by 1000 to colwert to mW
    //   20.12 W * 10.0 mW/W => 30.12 mW
    // Discarding 12 fractional bits
    //
    pwrmW = multUFXP32(4, pGpuAdc10->super.fullRangeVoltage,
                pProv->fullRangeLwrrent);
    pwrmW = multUFXP32(12, pwrmW, 1000);
    return pwrmW;
}

/* ------------------------- Defines --------------------------------------- */
/*!
 * Macro to return the number of supported providers.
 *
 * @param[in] pDev Pointer to the PWR_DEVICE object
 *
 * @return Number of PWR_DEVICE providers supported by this PWR_DEVICE class.
 */
#define pwrDevProvNumGet_GPUADC10(pDev)                                       \
            RM_PMU_PMGR_PWR_DEVICE_GPUADC10_PROV_NUM

/*!
 * Macro to return the number of thresholds (limits) supported by this device.
 *
 * @param[in] pDev Pointer to the PWR_DEVICE object
 *
 * @return Number of thresholds.
 */
#define pwrDevThresholdNumGet_GPUADC10(pDev)                                  \
            RM_PMU_PMGR_PWR_DEVICE_GPUADC10_THRESHOLD_NUM

/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
/* ------------------------- PWR_DEVICE Interfaces ------------------------- */

BoardObjGrpIfaceModel10ObjSet       (pwrDevGrpIfaceModel10ObjSetImpl_GPUADC10)
    GCC_ATTRIB_SECTION("imem_pmgrLibConstruct", "pwrDevGrpIfaceModel10ObjSetImpl_GPUADC10");
PwrDevLoad              (pwrDevLoad_GPUADC10)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "pwrDevLoad_GPUADC10");
PwrDevTupleGet          (pwrDevTupleGet_GPUADC10)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrDevTupleGet_GPUADC10");
PwrDevTupleGet          (pwrDevTupleFullRangeGet_GPUADC10)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrDevTupleFullRangeGet_GPUADC10");
PwrDevTupleAccGet       (pwrDevTupleAccGet_GPUADC10)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrDevTupleAccGet_GPUADC10");
PwrDevSetLimit          (pwrDevSetLimit_GPUADC10)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "pwrDevSetLimit_GPUADC10");
PwrDevGetLimit          (pwrDevGetLimit_GPUADC10)
    GCC_ATTRIB_SECTION("imem_pmgrLibQuery", "pwrDevGetLimit_GPUADC10");
PwrDevStateSync         (pwrDevStateSync_GPUADC10)
    GCC_ATTRIB_SECTION("imem_pmgrPwrDeviceStateSync", "pwrDevStateSync_GPUADC10");

/* ------------------------- PWR_DEVICE_GPUADC1X Interfaces ---------------- */
PwrDevGpuAdc1xBaSetLimit             (pwrDevGpuAdc1xBaSetLimit_GPUADC10)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "pwrDevGpuAdc1xBaSetLimit_GPUADC10");
PwrDevGpuAdc1xFullRangeVoltmVGet     (pwrDevGpuAdc1xFullRangeVoltmVGet_GPUADC10)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrDevGpuAdc1xFullRangeVoltmVGet_GPUADC10");
PwrDevGpuAdc1xFullRangeLwrrmAGet     (pwrDevGpuAdc1xFullRangeLwrrmAGet_GPUADC10)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrDevGpuAdc1xFullRangeLwrrmAGet_GPUADC10");
PwrDevGpuAdc1xFullRangePwrmWGet      (pwrDevGpuAdc1xFullRangePwrmWGet_GPUADC10)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrDevGpuAdc1xFullRangePwrmWGet_GPUADC10");
PwrDevGpuAdc1xVoltColwFactorGet      (pwrDevGpuAdc1xVoltColwFactorGet_GPUADC10)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrDevGpuAdc1xVoltColwFactorGet_GPUADC10");
PwrDevGpuAdc1xSwResetLengthSet       (pwrDevGpuAdc1xSwResetLengthSet_GPUADC10)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "pwrDevGpuAdc1xSwResetLengthSet_GPUADC10");
PwrDevGpuAdc1xSnapTrigger            (pwrDevGpuAdc1xSnapTrigger_GPUADC10)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrDevGpuAdc1xSnapTrigger_GPUADC10");
PwrDevGpuAdc1xSwResetTrigger         (pwrDevGpuAdc1xSwResetTrigger_GPUADC10)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrDevGpuAdc1xSwResetTrigger_GPUADC10");

/* ------------------------- Debug Macros ---------------------------------- */

#endif // PWRDEV_GPUADC10_H
