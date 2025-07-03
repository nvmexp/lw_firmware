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
 * @file    pwrdev_gpuadc11.h
 * @copydoc pwrdev_gpuadc11.c
 */

#ifndef PWRDEV_GPUADC11_H
#define PWRDEV_GPUADC11_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "pmgr/pwrdev_gpuadc1x.h"

/* ------------------------- Macros ---------------------------------------- */
/* ------------------------- Types Definitions ----------------------------- */
typedef struct PWR_DEVICE_GPUADC11 PWR_DEVICE_GPUADC11;

struct PWR_DEVICE_GPUADC11
{
    /*!
     * Super PWR_DEVICE_GPUADC1X structure
     */
    PWR_DEVICE_GPUADC1X super;

     /*!
     * Operating mode of ADC, Non-alternating vs Alternating
     * @ref LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC11_OPERATING_MODE_<XYZ>
     */
    LwU8   operatingMode;
    /*!
     * Number of samples that the HW multisampler block needs to accumulate
     * before sending to IIR filter. HW will accumulate (numSamples + 1).
     */
    LwU8   numSamples;
    /*!
     * VCM Coarse Offset VFE Variable Index
     */
    LwU8   vcmCoarseOffsetVfeVarIdx;
    /*!
     * Boolean indicating whether ADC_RAW_CORRECTION is to be updated with
     * fuse value or not.
     */
    LwBool bAdcRawCorrection;
    /*!
     * OVRM configuration parameters
     */
    LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC11_OVRM   ovrm;
    /*!
     * IPC configuration parameters
     */
    LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC11_IPC    ipc;
    /*!
     * Beacon configuration parameters
     */
    LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC11_BEACON beacon;
    /*!
     * Offset configuration parameters
     */
    LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC11_OFFSET offset;
    /*!
     * Sum block configuration parameters
     */
    LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC11_SUM    sum;
    /*!
     * Structure holding the parameters required to evaluate tuple.
     */
    PWR_DEVICE_GPUADC1X_TUPLE_PARAMS            tupleParams;
    /*!
     * Structure holding the parameters for each provider of GPUADC
     */
    PWR_DEVICE_GPUADC1X_PROV
        prov[LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC11_PHYSICAL_PROV_NUM];
    /*!
     * Boolean denoting whether IPC needs to be patched for Bug 3282024
     */
    LwBool                                      bPatchIpcBug3282024;
};

/* ------------------------- Inline Functions  ----------------------------- */
/*!
 * @brief   Colwert @ref PWR_DEVICE_GPUADC1X::fullRangeVoltage
 *     from FXP8.8 (V) to FXP32.0 (mV).
 *
 * @param[in]  pGpuAdc11     Pointer to @ref PWR_DEVICE_GPUADC11
 *
 * @return  LwU32 representing voltage in mV.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline LwU32
pwrDevGpuAdc11FullRangeVoltmVGet
(
    PWR_DEVICE_GPUADC11      *pGpuAdc11,
    PWR_DEVICE_GPUADC1X_PROV *pProv
)
{
    LwU32 voltmV = pGpuAdc11->super.fullRangeVoltage;

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
pwrDevGpuAdc11FullRangeLwrrmAGet
(
    PWR_DEVICE_GPUADC11      *pGpuAdc11,
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
 * @param[in]  pGpuAdc11 Pointer to @ref PWR_DEVICE_GPUADC11
 * @param[in]  pProv     Pointer to @ref PWR_DEVICE_GPUADC1X_PROV
 *
 * @return  LwU32 representing power in mW.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline LwU32
pwrDevGpuAdc11FullRangePwrmWGet
(
    PWR_DEVICE_GPUADC11      *pGpuAdc11,
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
    pwrmW = multUFXP32(4, pGpuAdc11->super.fullRangeVoltage,
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
#define pwrDevProvNumGet_GPUADC11(pDev)                                       \
            LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC11_PROV_NUM

/*!
 * Macro to return the number of thresholds (limits) supported by this device.
 *
 * @param[in] pDev Pointer to the PWR_DEVICE object
 *
 * @return Number of thresholds.
 */
#define pwrDevThresholdNumGet_GPUADC11(pDev)                                  \
            LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC11_THRESHOLD_NUM

/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
/* ------------------------- PWR_DEVICE Interfaces ------------------------- */

BoardObjGrpIfaceModel10ObjSet       (pwrDevGrpIfaceModel10ObjSetImpl_GPUADC11)
    GCC_ATTRIB_SECTION("imem_pmgrLibConstruct", "pwrDevGrpIfaceModel10ObjSetImpl_GPUADC11");
PwrDevLoad              (pwrDevLoad_GPUADC11)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "pwrDevLoad_GPUADC11");
PwrDevTupleGet          (pwrDevTupleGet_GPUADC11)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrDevTupleGet_GPUADC11");
PwrDevTupleGet          (pwrDevTupleFullRangeGet_GPUADC11)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrDevTupleFullRangeGet_GPUADC11");
PwrDevTupleAccGet       (pwrDevTupleAccGet_GPUADC11)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrDevTupleAccGet_GPUADC11");
PwrDevSetLimit          (pwrDevSetLimit_GPUADC11)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "pwrDevSetLimit_GPUADC11");
PwrDevGetLimit          (pwrDevGetLimit_GPUADC11)
    GCC_ATTRIB_SECTION("imem_pmgrLibQuery", "pwrDevGetLimit_GPUADC11");

/* ------------------------- PWR_DEVICE_GPUADC1X Interfaces ---------------- */
PwrDevGpuAdc1xBaSetLimit             (pwrDevGpuAdc1xBaSetLimit_GPUADC11)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "pwrDevGpuAdc1xBaSetLimit_GPUADC11");
PwrDevGpuAdc1xFullRangeVoltmVGet     (pwrDevGpuAdc1xFullRangeVoltmVGet_GPUADC11)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrDevGpuAdc1xFullRangeVoltmVGet_GPUADC11");
PwrDevGpuAdc1xFullRangeLwrrmAGet     (pwrDevGpuAdc1xFullRangeLwrrmAGet_GPUADC11)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrDevGpuAdc1xFullRangeLwrrmAGet_GPUADC11");
PwrDevGpuAdc1xFullRangePwrmWGet      (pwrDevGpuAdc1xFullRangePwrmWGet_GPUADC11)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrDevGpuAdc1xFullRangePwrmWGet_GPUADC11");
PwrDevGpuAdc1xVoltColwFactorGet      (pwrDevGpuAdc1xVoltColwFactorGet_GPUADC11)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrDevGpuAdc1xVoltColwFactorGet_GPUADC11");
PwrDevGpuAdc1xSwResetLengthSet       (pwrDevGpuAdc1xSwResetLengthSet_GPUADC11)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "pwrDevGpuAdc1xSwResetLengthSet_GPUADC11");
PwrDevGpuAdc1xSnapTrigger            (pwrDevGpuAdc1xSnapTrigger_GPUADC11)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrDevGpuAdc1xSnapTrigger_GPUADC11");
PwrDevGpuAdc1xSwResetTrigger         (pwrDevGpuAdc1xSwResetTrigger_GPUADC11)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrDevGpuAdc1xSwResetTrigger_GPUADC11");

/* ------------------------- Debug Macros ---------------------------------- */

#endif // PWRDEV_GPUADC11_H
