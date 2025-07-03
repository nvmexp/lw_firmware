/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pwrdev_gpuadc1x.h
 * @copydoc pwrdev_gpuadc1x.c
 */

#ifndef PWRDEV_GPUADC1X_H
#define PWRDEV_GPUADC1X_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "lib/lib_fxp.h"
#include "pmgr/pwrdev.h"
#include "pmgr/pwrmonitor.h"
#include "pmgr/pmgr_acc.h"

/* ------------------------- Macros ---------------------------------------- */
#define PWR_DEVICE_GPUADC1X_PROV_CH_MAP_IDX_NONE                           0xFF

/* ------------------------- Types Definitions ----------------------------- */
typedef struct PWR_DEVICE_GPUADC1X PWR_DEVICE_GPUADC1X;

/*!
 * Structure containing information regarding the constituent HW channels
 * for a physical provider of GPUADC
 */
typedef struct
{
    /*!
     * Channel index corresponding to bus (voltage) channel of this provider.
     * @ref PWR_DEVICE_GPUADC1X_PROV_CH_MAP_IDX_NONE if no channel specified
     */
    LwU8 bus;
    /*!
     * Channel index corresponding to shunt (current) channel of this provider.
     * @ref PWR_DEVICE_GPUADC1X_PROV_CH_MAP_IDX_NONE if no channel specified
     */
    LwU8 shunt;
    /*!
     * Channel Pair index corresponding to multiplier (power) channel of this
     * provider.
     * @ref PWR_DEVICE_GPUADC1X_PROV_CH_MAP_IDX_NONE if no channel specified
     */
    LwU8 pair;
} PWR_DEVICE_GPUADC1X_PROV_CH_MAP;

/*!
 * Structure holding the parameters for each provider of GPUADC
 */
typedef struct
{
    /*!
     * Current (A) corresponding to @ref adcMaxValue (FXP24.8) for each
     * GPUADC provider
     */
    LwUFXP24_8 fullRangeLwrrent;
    /*!
     * Colwersion factor which represents the current value in mA of a single
     * bit of ADC (mA/code)
     */
    LwUFXP28_4 lwrrColwFactor;
    /*!
     * Colwersion factor which represents the power value in mW of a single
     * bit of ADC (mW/code)
     */
    LwUFXP28_4 pwrColwFactor;
    /*!
     * Structure containing aclwmulator data for voltage. Source aclwmulator
     * is in raw HW units (code*cycles) and destination is in mV*us.
     */
    PMGR_ACC volt;
    /*!
     * Structure containing aclwmulator data for current. Source aclwmulator
     * is in raw HW units (code*cycles) and destination is in nC.
     */
    PMGR_ACC lwrr;
    /*!
     * Structure containing aclwmulator data for power. Source aclwmulator
     * is in raw HW units (code*cycles) and destination is in nJ.
     */
    PMGR_ACC pwr;
    /*!
     * Structure containing aclwmulator data for energy. Source aclwmulator
     * is in nJ and destination is in mJ.
     */
    PMGR_ACC energy;
    /*!
     * Provider to channel mapping
     */
    PWR_DEVICE_GPUADC1X_PROV_CH_MAP channelMap;
} PWR_DEVICE_GPUADC1X_PROV;

/*!
 * Structure holding the parameters required to evaluate a tuple
 */
typedef struct
{
    /*!
     * Represents number of samples aclwmulated in an IIR value. The IIR value
     * will be divided by numSamples before colwerting to physical units.
     */
    LwU8 numSamples;
    /*!
     * Number of fractional bits to be discarded after colwerting IIR value of
     * bus (voltage) channel into uV.
     */
    LwU8 voltShiftBits;
    /*!
     * Number of fractional bits to be discarded after colwerting IIR value of
     * shunt (current) channel into mA.
     */
    LwU8 lwrrShiftBits;
    /*!
     * Number of fractional bits to be discarded after colwerting IIR value of
     * multiplier (power) channel into mW.
     */
    LwU8 pwrShiftBits;
} PWR_DEVICE_GPUADC1X_TUPLE_PARAMS;

struct PWR_DEVICE_GPUADC1X
{
    /*!
     * Super PWR_DEVICE structure
     */
    PWR_DEVICE super;
    /*!
     * Length of IIR applied to ADC raw samples
     */
    LwU8       iirLength;
    /*!
     * Sampling delay in units of utilsclk cycles
     */
    LwU8       sampleDelay;
    /*!
     * Active GPUADC providers
     */
    LwU8       activeProvCount;
    /*!
     * Number of active channels. Represents the number channels that ADC will
     * cycle through
     */
    LwU8       activeChannelsNum;
    /*!
     * Maximum possible value of the ADC raw output after correction
     */
    LwU8       adcMaxValue;
    /*!
     * Length of reset cycle in us
     */
    LwU8       resetLength;
    /*!
     * Boolean indicating first load
     */
    LwBool     bFirstLoad;
    /*!
     * VCM Offset VFE Variable Index
     */
    LwU8       vcmOffsetVfeVarIdx;
    /*!
     * Differential Offset VFE Variable Index
     */
    LwU8       diffOffsetVfeVarIdx;
    /*!
     * Differential Gain VFE Variable Index
     */
    LwU8       diffGailwfeVarIdx;
    /*!
     * ADC PWM period in units of utilsclk cycles
     */
    LwU16      pwmPeriod;
    /*!
     * Voltage (V) corresponding to @ref adcMaxValue (FXP8.8)
     * This is the default value that all providers of GPUADC will use unless
     * the sub-class overrides it with provider specific values.
     */
    LwUFXP8_8  fullRangeVoltage;
    /*!
     * Colwersion factor which represents the voltage value in mV of a single
     * bit of ADC (mV/code)
     * This is the default value that all providers of GPUADC will use unless
     * the sub-class overrides it with provider specific values.
     */
    LwUFXP28_4 voltColwFactor;
    /*!
     * Multiplier value to colwert aclwmulator cycles to time (in us)
     * - i.e. us/cycle.
     *
     * UFXP 20.12, but actual extent is 3.12.
     */
    LwUFXP20_12 accColwusPerCycle;
    /*!
     * Sequence ID of the aclwmulator
     */
    LwU32       seqId;
};

/*!
 * @interface PWR_DEVICE_GPUADC1X
 *
 * This interface is similar to the @ref PwrDevSetLimit interface because it
 * calls into the same implementations of GPUADC10, GPUADC11, ... . But this
 * seperate interface is created to distinguish the use-case from where this
 * is called, i.e. BA
 *
 * The use-case for this interface is to call setLimit from a BA device which
 * needs to set a limit for GPUADC1X device but is not aware of which type
 * (10 vs 11) the device might be and which provider index to use. This function
 * selects the correct GPUADC1X type and finds the correct provider index based
 * on the BA window.
 *
 * @param[in]   pGpuAdc1x        Pointer to the PWR_DEVICE_GPUADC1X object
 * @param[in]   baWindowIdx
 *     BA window index for which caller wants to set the limit.
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
 *     The given device object does not support this interface.
 */
#define PwrDevGpuAdc1xBaSetLimit(fname) FLCN_STATUS (fname) (PWR_DEVICE_GPUADC1X *pGpuAdc1x, LwU8 baWindowIdx, LwU8 limitIdx, LwU8 limitUnit, LwU32 limitValue)

/*!
 * @interface PWR_DEVICE_GPUADC1X
 *
 * @brief   Colwert @ref fullRangeVoltage to FXP32.0 (mV).
 *
 * @param[in]  pGpuAdc1x Pointer to @ref PWR_DEVICE_GPUADC1X
 * @param[in]  pProv     Pointer to @ref PWR_DEVICE_GPUADC1X_PROV
 *
 * @return  LwU32 representing current in mV.
 */
#define PwrDevGpuAdc1xFullRangeVoltmVGet(fname) LwU32 (fname) (PWR_DEVICE_GPUADC1X *pGpuAdc1x, PWR_DEVICE_GPUADC1X_PROV *pProv)

/*!
 * @interface PWR_DEVICE_GPUADC1X
 *
 * @brief   Colwert @ref PWR_DEVICE_GPUADC1X_PROV::fullRangeLwrrent
 *     to FXP32.0 (mA).
 *
 * @param[in]  pGpuAdc1x Pointer to @ref PWR_DEVICE_GPUADC1X
 * @param[in]  pProv     Pointer to @ref PWR_DEVICE_GPUADC1X_PROV
 *
 * @return  LwU32 representing current in mA.
 */
#define PwrDevGpuAdc1xFullRangeLwrrmAGet(fname) LwU32 (fname) (PWR_DEVICE_GPUADC1X *pGpuAdc1x, PWR_DEVICE_GPUADC1X_PROV *pProv)

/*!
 * @interface PWR_DEVICE_GPUADC1X
 *
 *
 * @brief   Callwlate full range power in mW for given provider.
 *
 * @param[in]  pGpuAdc1x Pointer to @ref PWR_DEVICE_GPUADC1X
 * @param[in]  pProv     Pointer to @ref PWR_DEVICE_GPUADC1X_PROV
 *
 * @return  LwU32 representing power in mW.
 */
#define PwrDevGpuAdc1xFullRangePwrmWGet(fname) LwU32 (fname) (PWR_DEVICE_GPUADC1X *pGpuAdc1x, PWR_DEVICE_GPUADC1X_PROV *pProv)

/*!
 * @interface PWR_DEVICE_GPUADC1X
 *
 * @brief   Get the voltColwFactor depending on the GPUADC device version.
 *
 * @param[in]  pGpuAdc1x Pointer to @ref PWR_DEVICE_GPUADC1X
 * @param[in]  pProv     Pointer to @ref PWR_DEVICE_GPUADC1X_PROV
 *
 * @return  LwUFXP28_4 representing voltColwFactor in mV/code.
 */
#define PwrDevGpuAdc1xVoltColwFactorGet(fname) LwU32 (fname) (PWR_DEVICE_GPUADC1X *pGpuAdc1x, PWR_DEVICE_GPUADC1X_PROV *pProv)

/*!
 * @interface PWR_DEVICE_GPUADC1X
 *
 * @brief   Set the reset length by writing to register.
 *
 * @param[in]  pGpuAdc1x Pointer to @ref PWR_DEVICE_GPUADC1X
 *
 */
#define PwrDevGpuAdc1xSwResetLengthSet(fname) void (fname) (PWR_DEVICE_GPUADC1X *pGpuAdc1x)

/*!
 * @interface PWR_DEVICE_GPUADC1X
 *
 * @brief   Take a snapshot of the ADC registers by setting the trigger bit.
 *
 * @param[in]  pGpuAdc1x Pointer to @ref PWR_DEVICE_GPUADC1X
 *
 */
#define PwrDevGpuAdc1xSnapTrigger(fname) void (fname) (PWR_DEVICE_GPUADC1X *pGpuAdc1x)

/*!
 * @interface PWR_DEVICE_GPUADC1X
 *
 * @brief   Trigger a SW reset of the GPUADC1X device.
 *
 * @param[in]  pGpuAdc1x Pointer to @ref PWR_DEVICE_GPUADC1X
 *
 */
#define PwrDevGpuAdc1xSwResetTrigger(fname) void (fname) (PWR_DEVICE_GPUADC1X *pGpuAdc1x)

/* ------------------------- Inline Functions  ----------------------------- */
/*!
 * @brief   Accessor for PWR_DEVICE_GPUADC1X_PROV::channelMap.bus.
 *
 * @param[in]   pProv    PWR_DEVICE_GPUADC1X_PROV pointer.
 *
 * @return  @ref PWR_DEVICE_GPUADC1X_PROV::channelMap.bus for passed in object.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline LwU8
pwrDevGpuAdc1xProvChBusGet
(
    PWR_DEVICE_GPUADC1X_PROV *pProv
)
{
    return pProv->channelMap.bus;
}

/*!
 * @brief   Accessor for PWR_DEVICE_GPUADC1X_PROV::channelMap.shunt.
 *
 * @param[in]   pProv    PWR_DEVICE_GPUADC1X_PROV pointer.
 *
 * @return  @ref PWR_DEVICE_GPUADC1X_PROV::channelMap.shunt for passed in object.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline LwU8
pwrDevGpuAdc1xProvChShuntGet
(
    PWR_DEVICE_GPUADC1X_PROV *pProv
)
{
    return pProv->channelMap.shunt;
}

/*!
 * @brief   Accessor for PWR_DEVICE_GPUADC1X_PROV::channelMap.pair.
 *
 * @param[in]   pProv    PWR_DEVICE_GPUADC1X_PROV pointer.
 *
 * @return  @ref PWR_DEVICE_GPUADC1X_PROV::channelMap.pair for passed in object.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline LwU8
pwrDevGpuAdc1xProvChPairGet
(
    PWR_DEVICE_GPUADC1X_PROV *pProv
)
{
    return pProv->channelMap.pair;
}

/* ------------------------- Defines --------------------------------------- */
/* ------------------------- Function Prototypes  -------------------------- */
/*!
 * GPUADC1X function to get the tuple
 *
 * Get Power (in mW), Current (in mA), Voltage (in uV) and Energy (in mJ)
 * from the power device.
 *
 * @param[in]  pDev     Pointer to @ref PWR_DEVICE
 * @param[in]  pProv    Pointer to @ref PWR_DEVICE_GPUADC1X_PROV for which tuple
 *     is requested
 * @param[out] pTuple   Pointer to @ref LW2080_CTRL_PMGR_PWR_TUPLE
 * @param[in]  pParams  Pointer to @ref PWR_DEVICE_GPUADC1X_TUPLE_PARAMS
 *
 * @return FLCN_OK
 *
 * Note that this function is not a @ref PWR_DEVICE interface as it uses a different
 * set of arguments than the @ref PwrDevTupleGet() interface.
 */
FLCN_STATUS pwrDevTupleGet_GPUADC1X(PWR_DEVICE *pDev, PWR_DEVICE_GPUADC1X_PROV *pProv, LW2080_CTRL_PMGR_PWR_TUPLE *pTuple, PWR_DEVICE_GPUADC1X_TUPLE_PARAMS *pParams)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrDevTupleGet_GPUADC1X");

/*!
 * GPUADC1X function to get the tuple aclwmulation
 *
 * Get Power Aclwmulation(Energy) (in nJ), Current Aclwmulation(Charge) (in nC)
 * and Voltage Aclwmulation (in mV*us) from the power device.
 *
 * @param[in]  pDev       Pointer to @ref PWR_DEVICE
 * @param[in]  pProv      Pointer to @ref PWR_DEVICE_GPUADC1X_PROV for which
 *     tuple is requested
 * @param[out] pTupleAcc  Pointer to @ref LW2080_CTRL_PMGR_PWR_TUPLE_ACC
 *
 * @return FLCN_OK
 *
 * Note that this function is not a @ref PWR_DEVICE interface as it uses a
 * different set of arguments than the @ref PwrDevTupleAccGet() interface.
 */
FLCN_STATUS pwrDevTupleAccGet_GPUADC1X(PWR_DEVICE *pDev, PWR_DEVICE_GPUADC1X_PROV *pProv, LW2080_CTRL_PMGR_PWR_TUPLE_ACC *pTupleAcc)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrDevTupleAccGet_GPUADC1X");

/*!
 * GPUADC1X function to return full range tuple. Full range is defined for each
 * provider of GPUADC1X in the VBIOS.
 *
 * @param[in]     pDev     Pointer to @ref PWR_DEVICE
 * @param[in]     pProv    Pointer to @ref PWR_DEVICE_GPUADC1X_PROV for which
 *     full range tuple is requested
 * @param[out]    pTuple   Pointer to @ref LW2080_CTRL_PMGR_PWR_TUPLE
 *
 * @return        FLCN_OK
 *
 * Note that this function is not a @ref PWR_DEVICE interface as it uses a
 * different set of arguments than the @ref PwrDevTupleGet() interface.
 */
FLCN_STATUS pwrDevTupleFullRangeGet_GPUADC1X(PWR_DEVICE *pDev, PWR_DEVICE_GPUADC1X_PROV *pProv, LW2080_CTRL_PMGR_PWR_TUPLE *pTuple)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrDevTupleFullRangeGet_GPUADC1X");

/*!
 * Helper function to get the power aclwmulation for the given provider
 *
 * @param[in]  pGpuAdc1x Pointer to @ref PWR_DEVICE_GPUADC1X
 * @param[in]  pProv     Pointer to @ref PWR_DEVICE_GPUADC1X_PROV for which
 *     power aclwmulation is requested
 * @param[out] pPwrAccnJ Pointer to LwU64 in which to return power aclwmulation
 *
 * @return     FLCN_OK
 */
FLCN_STATUS pwrDevGpuAdc1xPowerAccGet(PWR_DEVICE_GPUADC1X *pGpuAdc1x, PWR_DEVICE_GPUADC1X_PROV *pProv, LwU64 *pPwrAccnJ)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrDevGpuAdc1xPowerAccGet");

/*!
 * Function to trigger SW_RESET of the GPUADC1X device.
 * Increments the seqId after triggering a reset.
 *
 * @param[in] pGpuAdc1x    Pointer to PWR_DEVICE_GPUADC1X
 *
 */
void pwrDevGpuAdc1xReset(PWR_DEVICE_GPUADC1X *pGpuAdc1x)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrDevGpuAdc1xReset");

/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
/* ------------------------- PWR_DEVICE Interfaces ------------------------- */
BoardObjGrpIfaceModel10ObjSet       (pwrDevGrpIfaceModel10ObjSetImpl_GPUADC1X)
    GCC_ATTRIB_SECTION("imem_pmgrLibConstruct", "pwrDevGrpIfaceModel10ObjSetImpl_GPUADC1X");
PwrDevLoad              (pwrDevLoad_GPUADC1X)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "pwrDevLoad_GPUADC1X");

/* ------------------------- PWR_DEVICE_GPUADC1X Interfaces ---------------- */
PwrDevGpuAdc1xBaSetLimit             (pwrDevGpuAdc1xBaSetLimit)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "pwrDevGpuAdc1xBaSetLimit");
PwrDevGpuAdc1xFullRangeVoltmVGet     (pwrDevGpuAdc1xFullRangeVoltmVGet)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrDevGpuAdc1xFullRangeVoltmVGet");
PwrDevGpuAdc1xFullRangeLwrrmAGet     (pwrDevGpuAdc1xFullRangeLwrrmAGet)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrDevGpuAdc1xFullRangeLwrrmAGet");
PwrDevGpuAdc1xFullRangePwrmWGet      (pwrDevGpuAdc1xFullRangePwrmWGet)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrDevGpuAdc1xFullRangePwrmWGet");
PwrDevGpuAdc1xVoltColwFactorGet      (pwrDevGpuAdc1xVoltColwFactorGet)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrDevGpuAdc1xVoltColwFactorGet");
PwrDevGpuAdc1xSwResetLengthSet       (pwrDevGpuAdc1xSwResetLengthSet)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "pwrDevGpuAdc1xSwResetLengthSet");
PwrDevGpuAdc1xSnapTrigger            (pwrDevGpuAdc1xSnapTrigger)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrDevGpuAdc1xSnapTrigger");
PwrDevGpuAdc1xSwResetTrigger         (pwrDevGpuAdc1xSwResetTrigger)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrDevGpuAdc1xSwResetTrigger");

/* ------------------------- Debug Macros ---------------------------------- */
/* ------------------------- Child Class Includes -------------------------- */
#include "pmgr/pwrdev_gpuadc10.h"
#include "pmgr/pwrdev_gpuadc11.h"
#include "pmgr/pwrdev_gpuadc13.h"

#endif // PWRDEV_GPUADC1X_H
