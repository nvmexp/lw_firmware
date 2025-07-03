/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2009-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_OBJPMGR_H
#define PMU_OBJPMGR_H

/*!
 * @file pmu_objpmgr.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "lwostimer.h"

/* ------------------------ Application Includes ---------------------------- */
#include "lib/lib_pwm.h"
#include "lib/lib_gpio.h"
#include "pmu_objclk.h"

#include "pmgr/pwrdev.h"
#include "pmgr/pwrdev_ba.h"
#include "pmgr/pwrequation.h"
#include "pmgr/pwrmonitor.h"
#include "pmgr/pwrpolicies.h"
#include "pmgr/pmgrtach.h"
#include "perf/perf_limit_client.h"
#include "pmgr/objillum.h"

#include "config/g_pmgr_hal.h"
#include "pmgr/pmgr_hal-mock.h"
#include "config/g_pmu-config_private.h"

/* ------------------------ Types Definitions ------------------------------- */
/*!
 * PSI callback scheduling info
 */
typedef struct
{
    /*!
     * Boolean indicating if a request for scheduling callbacks has been
     * received.
     */
    LwBool bRequested;
    /*!
     * Boolean to keep track of whether the PSI callback has been deferred
     * or not and needs to be re-scheduled accordingly.
     */
    LwBool bDeferred;
} PMGR_PSI_CALLBACK;

/*!
 * PMGR Object Definition
 */
typedef struct
{
    /*!
     * Event-queue used when communicating with PMGR I2C devices.
     */
    LwrtosQueueHandle   i2cQueue;

    /*!
     * Board Object Group of all I2C_DEVICE objects.
     */
    BOARDOBJGRP_E32     i2cDevices;

    /*!
     * All PWR code related fields.
     *
     * TODO: move other as well and extract structure into a separate header.
     */
    struct
    {
        /*!
         * Board Object Group of all PWR_DEVICE objects.
         */
        BOARDOBJGRP_E32 devices;

        /*!
         * Board Object Group of all PWR_EQUATION objects.
         */
        BOARDOBJGRP_E32 equations;

        /*!
         * Board Object Group of all PWR_CHANNEL objects.
         */
        BOARDOBJGRP_E32 channels;

        /*!
         * Board Object Group of all PWR_CHANNELREL objects.
         */
        BOARDOBJGRP_E32 chRels;

        /*!
         * Board Object Group of all PWR_POLICY objects.
         */
        BOARDOBJGRP_E32 policies;

        /*!
         * Board Object Group of all PWR_POLICY_RELATIONSHIP objects.
         */
        BOARDOBJGRP_E32 policyRels;

#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_VIOLATION)
        /*!
         * Board Object Group of all PWR_VIOLATION objects.
         */
        BOARDOBJGRP_E32 violations;
#endif // PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_VIOLATION)
    } pwr;

    /*!
     * @copydoc PWR_MONITOR
     */
    PWR_MONITOR        *pPwrMonitor;

    /*!
     * @copydoc PWR_POLICIES
     */
    PWR_POLICIES       *pPwrPolicies;

    /*!
     * Pointer to the PERF_LIMITS_CLIENT structure. This is used to set and
     * clear PERF_LIMITS when using the PMU arbiter.
     */
    PERF_LIMITS_CLIENT *pPerfLimits;

    /*!
     * BA HW state that is independent of any given BA PWR_DEVICE.
     */
    PWR_DEVICE_BA_INFO  baInfo;

    /*!
     * Mask of I2C ports which are blocked for CPU access
     */
    LwU32               i2cPortsPrivMask;

    /*!
     * IDDQ value to use for callwlating leakage power values.
     *
     * SPTODO: Move this value into a PWR_LEAKAGES struct.
     */
    LwU32               iddqmA;

    /*!
     * Keeps track of whether or not the LOAD command has been received and
     * successfully processed. Only after being processed is it safe to
     * commence high-level PMGR functionality (such as power-monitoring and
     * capping).
     */
    LwBool              bLoaded;

    /*!
     * Flag to disable PMU internal HwI2c, if set.
     */
    LwBool              bDisableHwI2c;

#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_DEVICE_GPUADC10))
    /*!
     * Flag to schedule device callback for GPUADC10.
     */
    LwBool              bDevCallbackGpuAdc10;
#endif
#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_DEVICE_BEACON_INTERRUPT))
    /*!
     * Index of power device for which the beacon interrupt is generated.
     */
    LwBoardObjIdx       pwrDevIdxBeacon;
#endif
#if (PMUCFG_FEATURE_ENABLED(PMU_PSI_PMGR_SLEEP_CALLBACK))
    /*!
     * PSI callback scheduling info
     */
    PMGR_PSI_CALLBACK   psiCallback;
#endif
} OBJPMGR;

/*!
 * Enumeration of PMGR task's PMU_OS_TIMER callback entries.
 */
typedef enum
{
    /*!
     * Power policy 3X evaluation callback. This callback will run at minimum
     * sampling period to check for expired power policies and kicks off
     * evaluation function for 3x.
     */
    PMGR_OS_TIMER_ENTRY_PWR_POLICY_3X_EVAL_CALLBACK = 0,

    /*!
     * Callback function to update Power devices.Lwrrently we are using this call back
     * function to update BA devices. During normal operation this callback will run at
     * minimum of 200ms with latest voltage and temperature.when in Low Power Feature
     * pstate, it will run for 1000ms.
     */
    PMGR_OS_TIMER_ENTRY_PWR_DEV_CALLBACK = 1,

    /*!
     * Callback function to periodically callwlate sleep current for PG-PSI.
     * Lwrrently this will run at the same frequency in normal and lowpower mode
     * The period is decided by the lowpower table and PSI table in VBIOS
     */
    PMGR_OS_TIMER_ENTRY_PG_PSI_CALLBACK  = 2,

    /*!
     * Must always be the last entry.
     */
    PMGR_OS_TIMER_ENTRY_NUM_ENTRIES
} PMGR_OS_TIMER_ENTRIES;

/* ------------------------ External Definitions ---------------------------- */
extern OBJPMGR         Pmgr;
#if (!PMUCFG_FEATURE_ENABLED(PMU_PMGR_CALLBACKS_CENTRALIZED))
extern OS_TIMER        PmgrOsTimer;
#endif

/* ------------------------ Static Variables -------------------------------- */
/* ------------------------ Macros ------------------------------------------ */
#define pmgrSwAsrNotify(bEnter)                                               \
{                                                                             \
    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_DEVICE_BA_SWASR_WAR_BUG_1168927)) \
    {                                                                         \
        pmgrSwAsrNotify_IMPL(bEnter);                                         \
    }                                                                         \
}
/* ------------------------ Defines ----------------------------------------- */
/*!
 * @brief   Unit Colwersion Factor used to colwert a nanoWatt value to a
 *          milliWatt value.
 */
#define PMGR_NANOWATT_TO_MILLIWATT_UNIT_COLWERSION_FACTOR               1000000

/*!
 * @brief   Unit Colwersion Factor used to colwert a milliWatt value to a
 *          nanoWatt value.
 */
#define PMGR_MILLIWATT_TO_NANOWATT_UNIT_COLWERSION_FACTOR               1000000

/*!
 * @brief   Additional overlays required along OSTASK_OVL_DESC_DEFINE_LIB_PWR_MONITOR
 *          attach paths when PMU_PMGR_PWR_MONITOR_ENERGY_COUNTER
 *          feature is enabled.
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_MONITOR_ENERGY_COUNTER))
#define OSTASK_OVL_DESC_DEFINE_LIB_PWR_MONITOR_ENERGY_COUNTER                       \
    OSTASK_OVL_DESC_DEFINE_LIB_LW64
#else
#define OSTASK_OVL_DESC_DEFINE_LIB_PWR_MONITOR_ENERGY_COUNTER                       \
    OSTASK_OVL_DESC_ILWALID
#endif

/*!
 * @brief   List of an overlay descriptors required by a pwr monitor code.
 *
 * @note    Review the use cases in Tasks.pm and update MAX_OVERLAYS_IMEM after
 *          updates to this macro. Failure to do so might result in falcon halt.
 */
#define OSTASK_OVL_DESC_DEFINE_LIB_PWR_MONITOR                                 \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libi2c)                              \
    OSTASK_OVL_DESC_DEFINE_LIB_PWR_MONITOR_ENERGY_COUNTER                      \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, pmgrLibPwrMonitor)                   \
    OSTASK_OVL_DESC_DEFINE_LIB_PWR_MONITOR_CHANNEL_PSTATE_ESTIMATION_LUT

/*!
 * Macro to check if PMGR task wants to get volt change notification or not.
 * Lwrrently, use case is for BA and PMGR wants to get notification of Voltage
 * change in case of BA is enabled and used.
 */
#define PMGR_PERF_VOLT_CHANGE_NOTIFY(_pPmgr)                                   \
    ((_pPmgr)->baInfo.bInitializedAndUsed)

/*!
 * Accessor macro for @ref OBJPMGR::pwrDevIdxBeacon
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_DEVICE_BEACON_INTERRUPT))
#define PMGR_PWR_DEV_IDX_BEACON_GET(_pPmgr) ((_pPmgr)->pwrDevIdxBeacon)
#else
#define PMGR_PWR_DEV_IDX_BEACON_GET(_pPmgr) (LW2080_CTRL_BOARDOBJ_IDX_ILWALID)
#endif

/*!
 * Macro to initialize @ref OBJPMGR::pwrDevIdxBeacon
 * Initializes the index to @ref LW2080_CTRL_BOARDOBJ_IDX_ILWALID
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_DEVICE_BEACON_INTERRUPT))
#define PMGR_PWR_DEV_IDX_BEACON_INIT(_pPmgr)                                   \
    ((_pPmgr)->pwrDevIdxBeacon = LW2080_CTRL_BOARDOBJ_IDX_ILWALID)
#else
#define PMGR_PWR_DEV_IDX_BEACON_INIT(_pPmgr)
#endif

/* ------------------------- Inline Functions ------------------------------ */
/*!
 * @brief   Obtain PMGR_PSI_CALLBACK pointer from global Pmgr object.
 *
 * @param[in, out]  ppPsiCallback Pointer to Pointer to @ref PMGR_PSI_CALLBACK
 *
 * @return  FLCN_OK if the transaction was successful, error otherwise.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
pmgrPsiCallbackGet
(
    PMGR_PSI_CALLBACK **ppPsiCallback
)
{
    FLCN_STATUS status = FLCN_OK;

    // Sanity check.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (ppPsiCallback != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        pmgrPsiCallbackGet_done);

#if (PMUCFG_FEATURE_ENABLED(PMU_PSI_PMGR_SLEEP_CALLBACK))
    *ppPsiCallback = &(Pmgr.psiCallback);
#else
    *ppPsiCallback = NULL;
#endif

pmgrPsiCallbackGet_done:
    return status;
}

/*!
 * @brief   A PMGR wrapper over multUFXP20_12FailSafe() for chips later than
 *          AD10X. For chips older than AD10X, we fallback to default
 *          implementation.
 *
 * @param[in]  x  Multiplicand 1
 * @param[in]  y  Multiplicand 2
 *
 * @return  The required product in FXP20.12 format if there is no 32-bit
 *          overflow, LW_U32_MAX otherwise.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline LwUFXP20_12
pmgrMultUFXP20_12OverflowFixes
(
    LwUFXP20_12 x,
    LwUFXP20_12 y
)
{
    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_32BIT_OVERFLOW_FIXES))
    {
        return multUFXP20_12FailSafe(x, y);
    }
    else
    {
        return LW_TYPES_UFXP_X_Y_TO_U32_ROUNDED(20, 12, x * y);
    }
}

/*!
 * @brief   Use 64-bit math to compute power value (mW) from a current
 *          value (mA) and a voltage value (uV).
 *
 * @param[in]       lwrrmA Current value in mA
 * @param[in]       voltuV Voltage value in uV
 * @param[in, out]  pPwrmW Pointer to the field that will hold the computed
 *                         power value in mW
 *                  Output parameters:
 *                      *pPwrmW
 *
 * @return  FLCN_OK if the transaction was successful, error otherwise.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
pmgrComputePowermWFromLwrrentmAAndVoltuV
(
    LwU32  lwrrmA,
    LwU32  voltuV,
    LwU32 *pPwrmW
)
{
    FLCN_STATUS status = FLCN_OK;

    // Sanity check.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pPwrmW != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        pmgrComputePowermWFromLwrrentmAAndVoltuV_done);

    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_32BIT_OVERFLOW_FIXES))
    {
        LWU64_TYPE  result;
        LwU64       lwrrmAU64 = (LwU64)lwrrmA;
        LwU64       voltuVU64 = (LwU64)voltuV;

        // Intermediate step in 64-bit Math: Obtain power in nanoWatt.
        lwU64Mul(
            &(result.val),
            &lwrrmAU64,
            &voltuVU64);

        // Colwert nanoWatt value to milliWatt value in 64-bit Math.
        lwU64DivRounded(
            &(result.val),
            &(result.val),
            &(LwU64){ PMGR_NANOWATT_TO_MILLIWATT_UNIT_COLWERSION_FACTOR });

        //
        // Check for 32-bit overflow and if true, cap the power value to
        // LW_U32_MAX. otherwise, extract power as the lower 32 bits of the
        // 64-bit result.
        //
        if (LWU64_HI(result))
        {
            *pPwrmW = LW_U32_MAX;
        }
        else
        {
            *pPwrmW = LWU64_LO(result);
        }
    }
    else
    {
        *pPwrmW = LW_UNSIGNED_ROUNDED_DIV((lwrrmA *
            LW_UNSIGNED_ROUNDED_DIV(voltuV, 1000)), 1000);
    }

pmgrComputePowermWFromLwrrentmAAndVoltuV_done:
    return status;
}

/*!
 * @brief   Use 64-bit math to compute current value (mA) from a power
 *          value (mW) and a voltage value (uV).
 *
 * @param[in]       pwrmW   Power value in mW
 * @param[in]       voltuV  Voltage value in uV
 * @param[in, out]  pLwrrmA Pointer to the field that will hold the computed
 *                          current value in mA
 *                  Output parameters:
 *                      *pLwrrmA
 *
 * @return  FLCN_OK if the transaction was successful, error otherwise.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
pmgrComputeLwrrmAFromPowermWAndVoltuV
(
    LwU32  pwrmW,
    LwU32  voltuV,
    LwU32 *pLwrrmA
)
{
    FLCN_STATUS status = FLCN_OK;

    // Sanity check.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pLwrrmA != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        pmgrComputeLwrrmAFromPowermWAndVoltuV_done);

    //
    // If the voltage value is zero, set current to LW_U32_MAX to mimic
    // the infinity value (on a division by zero).
    //
    if (voltuV == 0)
    {
        *pLwrrmA = LW_U32_MAX;
        goto pmgrComputeLwrrmAFromPowermWAndVoltuV_done;
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_32BIT_OVERFLOW_FIXES))
    {
        LWU64_TYPE  result;
        LwU64       pwrmWU64  = (LwU64)pwrmW;
        LwU64       voltuVU64 = (LwU64)voltuV;

        // Intermediate step in 64-bit Math: Obtain power in nanoWatt.
        lwU64Mul(
            &(result.val),
            &pwrmWU64,
            &(LwU64){ PMGR_MILLIWATT_TO_NANOWATT_UNIT_COLWERSION_FACTOR });

        //
        // Divide power / voltage (nanoWatt / microVolt) in 64-bit Math to obtain
        // current value in milliAmp.
        //
        lwU64DivRounded(
            &(result.val),
            &(result.val),
            &voltuVU64);

        //
        // Check for 32-bit overflow and if true, cap the current value to
        // LW_U32_MAX. otherwise, extract current as the lower 32 bits of the
        // 64-bit result.
        //
        if (LWU64_HI(result))
        {
            *pLwrrmA = LW_U32_MAX;
        }
        else
        {
            *pLwrrmA = LWU64_LO(result);
        }
    }
    else
    {
        *pLwrrmA = (LW_UNSIGNED_ROUNDED_DIV(pwrmW * 1000, voltuV) * 1000);
    }

pmgrComputeLwrrmAFromPowermWAndVoltuV_done:
    return status;
}

/*!
 * @brief   Use 64-bit math to compute voltage value (uV) from a power
 *          value (mW) and a current value (mA).
 *
 * @param[in]       pwrmW   Power value in mW
 * @param[in]       lwrrmA  Current value in mA
 * @param[in, out]  pVoltuV Pointer to the field that will hold the computed
 *                          voltage value in uV
 *                  Output parameters:
 *                      *pVoltuV
 *
 * @return  FLCN_OK if the transaction was successful, error otherwise.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
pmgrComputeVoltuVFromPowermWAndLwrrmA
(
    LwU32  pwrmW,
    LwU32  lwrrmA,
    LwU32 *pVoltuV
)
{
    FLCN_STATUS status = FLCN_OK;

    // Sanity check.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pVoltuV != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        pmgrComputeVoltuVFromPowermWAndLwrrmA_done);

    //
    // If the current value is zero, set voltage to LW_U32_MAX to mimic
    // the infinity value (on a division by zero).
    //
    if (lwrrmA == 0)
    {
        *pVoltuV = LW_U32_MAX;
        goto pmgrComputeVoltuVFromPowermWAndLwrrmA_done;
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_32BIT_OVERFLOW_FIXES))
    {
        LWU64_TYPE  result;
        LwU64       pwrmWU64  = (LwU64)pwrmW;
        LwU64       lwrrmAU64 = (LwU64)lwrrmA;

        // Intermediate step in 64-bit Math: Obtain power in nanoWatt.
        lwU64Mul(
            &(result.val),
            &pwrmWU64,
            &(LwU64){ PMGR_MILLIWATT_TO_NANOWATT_UNIT_COLWERSION_FACTOR });

        //
        // Divide power / current (nanoWatt / milliAmp) in 64-bit Math to obtain
        // voltage value in microVolt.
        //
        lwU64DivRounded(
            &(result.val),
            &(result.val),
            &lwrrmAU64);

        //
        // Check for 32-bit overflow and if true, cap the voltage value to
        // LW_U32_MAX. otherwise, extract voltage as the lower 32 bits of the
        // 64-bit result.
        //
        if (LWU64_HI(result))
        {
            *pVoltuV = LW_U32_MAX;
        }
        else
        {
            *pVoltuV = LWU64_LO(result);
        }
    }
    else
    {
        *pVoltuV = (LW_UNSIGNED_ROUNDED_DIV(pwrmW * 1000, lwrrmA) * 1000);
    }

pmgrComputeVoltuVFromPowermWAndLwrrmA_done:
    return status;
}

/* ------------------------ Function Prototypes ---------------------------- */
void pmgrPreInit(void)
    // Called only at init time -> init overlay.
    GCC_ATTRIB_SECTION("imem_init", "pmgrPreInit");

FLCN_STATUS pmgrPostInit(void)
    // Called when the pmgr task is schedule for the first time.
    GCC_ATTRIB_SECTION("imem_libPmgrInit", "pmgrPostInit");

FLCN_STATUS pmgrBaLoad(void)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "pmgrBaLoad");
LwU32      pmgrFreqMHzGet(LwU32 clkDom, CLK_CNTR_AVG_FREQ_START *pCntrStart);

void       pmgrProcessPerfStatus(RM_PMU_PERF_STATUS *pStatus, LwU32 coreVoltageuV);
void       pmgrProcessPerfChange(LwU32 coreVoltageuV);

void       pmgrSwAsrNotify_IMPL(LwBool bEnter)
    GCC_ATTRIB_SECTION("imem_libMs", "pmgrSwAsrNotify");

LwBool     pmgrIsHwI2cEnabled(void)
    GCC_ATTRIB_SECTION("imem_pmgrLibConstruct", "pmgrIsHwI2cEnabled");

LwU32      pmgrI2cGetPortFlags(LwU8 i2cPort, LwU16 i2cAddress)
    GCC_ATTRIB_SECTION("imem_therm", "pmgrI2cSetPortFlags");

LwU32      pmgrComputeFactorA(PWR_DEVICE *pDev, LwU32 voltageuV)
    GCC_ATTRIB_SECTION("imem_pmgrLibFactorA", "pmgrComputeFactorA");

#endif // PMU_OBJPMGR_H

