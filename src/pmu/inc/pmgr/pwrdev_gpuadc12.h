/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pwrdev_gpuadc13.h
 * @copydoc pwrdev_gpuadc13.c
 */

#ifndef PWRDEV_GPUADC13_H
#define PWRDEV_GPUADC13_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "pmgr/pwrdev_gpuadc1x.h"

/* ------------------------- Macros ---------------------------------------- */
/* ------------------------- Types Definitions ----------------------------- */
typedef struct PWR_DEVICE_GPUADC13 PWR_DEVICE_GPUADC13;

/*!
 * Structure holding the parameters for each provider of GPUADC13
 */
typedef struct
{
    /*!
     * Super PWR_DEVICE_GPUADC1X_PROV structure
     */
    PWR_DEVICE_GPUADC1X_PROV super;
    /*!
     * Voltage (V) corresponding to @ref adcMaxValue (FXP8.8)
     * Since GPUADC13 can have different fullRangeVoltage for IMON providers,
     * this will override the PWR_DEVICE_GPUADC1X::fullRangeVoltage which is
     * common for all providers.
     */
    LwUFXP8_8  fullRangeVoltage;
    /*!
     * Colwersion factor which represents the voltage value in mV of a single
     * bit of ADC (mV/code)
     * Since GPUADC13 can have different voltColwFactor for IMON providers, this
     * will override the PWR_DEVICE_GPUADC1X::voltColwFactor which is common for
     * all providers.
     */
    LwUFXP28_4 voltColwFactor;
} PWR_DEVICE_GPUADC13_PROV;

struct PWR_DEVICE_GPUADC13
{
    /*!
     * Super PWR_DEVICE_GPUADC1X structure
     */
    PWR_DEVICE_GPUADC1X super;

     /*!
     * Operating mode of ADC, Non-alternating vs Alternating
     * @ref LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_OPERATING_MODE_<XYZ>
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
     * Boolean indicating if PMU needs to configure ADC registers.
     * If LW_TRUE, the ADC registers will be configured by PMU ucode at load.
     * If LW_FALSE, the ADC registers would have already been configured by
     * devinit ucode and raised the PLM of these registers to L3.
     * TODO-Tariq: Remove this field once emulation testing is done.
     */
    LwBool bPmuConfigReg;
    /*!
     * OVRM configuration parameters
     */
    LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_OVRM    ovrm;
    /*!
     * IPC configuration parameters
     */
    LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_IPC     ipc;
    /*!
     * Beacon configuration parameters
     */
    LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_BEACON  beacon;
    /*!
     * Offset configuration parameters
     */
    LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_OFFSET  offset;
    /*!
     * Sum block configuration parameters
     */
    LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_SUM     sum;
    /*!
     * ADC PWM configuration parameters
     */
    LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_ADC_PWM adcPwm;
    /*!
     * Structure holding the parameters required to evaluate tuple.
     */
    PWR_DEVICE_GPUADC1X_TUPLE_PARAMS             tupleParams;
    /*!
     * Structure holding the parameters for each provider of GPUADC
     */
    PWR_DEVICE_GPUADC13_PROV
        prov[LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_PHYSICAL_PROV_NUM];
};

/* ------------------------- Inline Functions  ----------------------------- */
/*!
 * @brief   Colwert @ref PWR_DEVICE_GPUADC13_PROV::fullRangeVoltage
 *     from FXP8.8 (V) to FXP32.0 (mV).
 *
 * @param[in]  pProv     Pointer to @ref PWR_DEVICE_GPUADC13_PROV
 *
 * @return  LwU32 representing voltage in mV.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline LwU32
pwrDevGpuAdc13FullRangeVoltmVGet
(
    PWR_DEVICE_GPUADC13      *pGpuAdc13,
    PWR_DEVICE_GPUADC13_PROV *pProv
)
{
    LwU32 voltmV = pProv->fullRangeVoltage;

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
 *     from FXP16.16 (A) to FXP32.0 (mA).
 *
 * @param[in]  pProv    Pointer to @ref PWR_DEVICE_GPUADC13_PROV
 *
 * @return  LwU32 representing current in mA.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline LwU32
pwrDevGpuAdc13FullRangeLwrrmAGet
(
    PWR_DEVICE_GPUADC13      *pGpuAdc13,
    PWR_DEVICE_GPUADC13_PROV *pProv
)
{
    LwU32 lwrrmA = pProv->super.fullRangeLwrrent;

    //
    // fullRangeLwrrent is always within FXP12.16 (A) for GPUADC1X
    // Multiplying fullRangeLwrrent by 1000 to colwert to mA
    // 12.16 A * 10.0 mA/A => 22.16 mA
    // Discarding 16 fractional bits => 22.0 mA which cannot have 32-bit
    // overflow.
    //
    lwrrmA = multUFXP32(16, lwrrmA, 1000U);

    return lwrrmA;
}

/*!
 * @brief   Callwlate full range power in mW for given provider.
 *
 * @param[in]  pGpuAdc13 Pointer to @ref PWR_DEVICE_GPUADC13
 * @param[in]  pProv     Pointer to @ref PWR_DEVICE_GPUADC13_PROV
 *
 * @return  LwU32 representing power in mW.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline LwU32
pwrDevGpuAdc13FullRangePwrmWGet
(
    PWR_DEVICE_GPUADC13      *pGpuAdc13,
    PWR_DEVICE_GPUADC13_PROV *pProv
)
{
    LwU32 pwrmW;

    //
    // Multiplying fullRangeVoltage and fullRangeLwrrent
    //   8.8 V * 12.16 A     => 20.24 A
    // Discarding 12 fractional bits to keep within 32-bits
    //   20.24 >> 12         => 20.12 A
    // Multiplying by 1000 to colwert to mW
    //   20.12 W * 10.0 mW/W => 30.12 mW
    // Discarding 12 fractional bits
    //
    pwrmW = multUFXP32(12, pProv->fullRangeVoltage,
                pProv->super.fullRangeLwrrent);
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
#define pwrDevProvNumGet_GPUADC13(pDev)                                       \
            LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_PROV_NUM

/*!
 * Macro to return the number of thresholds (limits) supported by this device.
 *
 * @param[in] pDev Pointer to the PWR_DEVICE object
 *
 * @return Number of thresholds.
 */
#define pwrDevThresholdNumGet_GPUADC13(pDev)                                  \
            LW2080_CTRL_PMGR_PWR_DEVICE_GPUADC13_THRESHOLD_NUM

/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
/* ------------------------- PWR_DEVICE Interfaces ------------------------- */

BoardObjGrpIfaceModel10ObjSet       (pwrDevGrpIfaceModel10ObjSetImpl_GPUADC13)
    GCC_ATTRIB_SECTION("imem_pmgrLibConstruct", "pwrDevGrpIfaceModel10ObjSetImpl_GPUADC13");
PwrDevLoad              (pwrDevLoad_GPUADC13)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "pwrDevLoad_GPUADC13");
PwrDevTupleGet          (pwrDevTupleGet_GPUADC13)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrDevTupleGet_GPUADC13");
PwrDevTupleGet          (pwrDevTupleFullRangeGet_GPUADC13)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrDevTupleFullRangeGet_GPUADC13");
PwrDevTupleAccGet       (pwrDevTupleAccGet_GPUADC13)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrDevTupleAccGet_GPUADC13");
PwrDevSetLimit          (pwrDevSetLimit_GPUADC13)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "pwrDevSetLimit_GPUADC13");
PwrDevGetLimit          (pwrDevGetLimit_GPUADC13)
    GCC_ATTRIB_SECTION("imem_pmgrLibQuery", "pwrDevGetLimit_GPUADC13");

/* ------------------------- PWR_DEVICE_GPUADC1X Interfaces ---------------- */
PwrDevGpuAdc1xBaSetLimit             (pwrDevGpuAdc1xBaSetLimit_GPUADC13)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "pwrDevGpuAdc1xBaSetLimit_GPUADC13");
PwrDevGpuAdc1xFullRangeVoltmVGet     (pwrDevGpuAdc1xFullRangeVoltmVGet_GPUADC13)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrDevGpuAdc1xFullRangeVoltmVGet_GPUADC13");
PwrDevGpuAdc1xFullRangeLwrrmAGet     (pwrDevGpuAdc1xFullRangeLwrrmAGet_GPUADC13)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrDevGpuAdc1xFullRangeLwrrmAGet_GPUADC13");
PwrDevGpuAdc1xFullRangePwrmWGet      (pwrDevGpuAdc1xFullRangePwrmWGet_GPUADC13)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrDevGpuAdc1xFullRangePwrmWGet_GPUADC13");
PwrDevGpuAdc1xVoltColwFactorGet      (pwrDevGpuAdc1xVoltColwFactorGet_GPUADC13)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrDevGpuAdc1xVoltColwFactorGet_GPUADC13");
PwrDevGpuAdc1xSwResetLengthSet       (pwrDevGpuAdc1xSwResetLengthSet_GPUADC13)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "pwrDevGpuAdc1xSwResetLengthSet_GPUADC13");
PwrDevGpuAdc1xSnapTrigger            (pwrDevGpuAdc1xSnapTrigger_GPUADC13)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrDevGpuAdc1xSnapTrigger_GPUADC13");
PwrDevGpuAdc1xSwResetTrigger         (pwrDevGpuAdc1xSwResetTrigger_GPUADC13)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrDevGpuAdc1xSwResetTrigger_GPUADC13");

/* ------------------------- Debug Macros ---------------------------------- */

#endif // PWRDEV_GPUADC13_H
