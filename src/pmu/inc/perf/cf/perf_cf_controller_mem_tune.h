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
 * @file    perf_cf_controller_mem_tune.h
 * @brief   Memory tunning PERF_CF Controller related defines.
 *
 * @copydoc perf_cf.h
 */

#ifndef PERF_CF_CONTROLLER_MEM_TUNE_H
#define PERF_CF_CONTROLLER_MEM_TUNE_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "perf/cf/perf_cf_controller.h"
#include "g_lwconfig.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct PERF_CF_CONTROLLER_MEM_TUNE   PERF_CF_CONTROLLER_MEM_TUNE;

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * @brief   Helper macro to get the latest requested tFAW value.
 */
#define perfCfControllerMemTunetFAWLwrrGet(pControllerMemTune)          \
            ((pControllerMemTune)->tFAWLwrr)

/*!
 * @brief   Helper macro to get the previous sample requested tFAW value.
 */
#define perfCfControllerMemTunetFAWPrevGet(pControllerMemTune)          \
            ((pControllerMemTune)->tFAWPrev)

/*!
 * @brief   Helper macro to get the current decision of memtune controller
 */
#define perfCfControllerMemTuneIsEngagedLwrr(pControllerMemTune)        \
            (perfCfControllerMemTunetFAWLwrrGet(pControllerMemTune) != LW2080_CTRL_PERF_PERF_CF_MEM_TUNE_DISENGAGE_TFAW_VAL)

/*!
 * @brief   Helper macro to get the previous decision of memtune controller
 */
#define perfCfControllerMemTuneIsEngagedPrev(pControllerMemTune)        \
            (perfCfControllerMemTunetFAWPrevGet(pControllerMemTune) != LW2080_CTRL_PERF_PERF_CF_MEM_TUNE_DISENGAGE_TFAW_VAL)

/*!
 * @brief   Helper macro to get the samples counter reading of memory tuning
 *          controller.
 */
#define perfCfControllerMemTuneSamplesCounterGet(pControllerMemTune)          \
            ((pControllerMemTune)->samplesCounter)

/*!
 * @brief   Helper macro to reset the samples counter value of memory tuning
 *          controller.
 */
#define perfCfControllerMemTuneSamplesCounterReset(pControllerMemTune)        \
            ((pControllerMemTune)->samplesCounter = 0)

/*!
 * @brief   Period between samples while in active power callback mode.
 */
#define PERF_CF_CONTROLLER_MEM_TUNE_MONITOR_ACTIVE_POWER_CALLBACK_PERIOD_US() \
    (5000000U)

/*!
 * @brief   Period between samples while in low power callback mode.
 */
#define PERF_CF_CONTROLLER_MEM_TUNE_MONITOR_LOW_POWER_CALLBACK_PERIOD_US()    \
    (5000000U)

/*!
 * @brief   Memory tuning controller will record time stamp every time it is
 *          able to make the decision. The watchdog in ISR will compare the
 *          last updated time stamp against the current time stamp and if the
 *          difference is above the threshold, it will trigger the HALT.
 *          The timer threshold is set to 5 samples x sampling period (1 sec)
 *          in LHR 1.0. In LHR 1.1 Enhancement timer threshold is set to
 *          1.5 seconds.
 */
#define PERF_CF_CONTROLLER_MEM_TUNE_ISR_WATCHDOG_TIMER_THRESHOLD_US()         \
    (1500000U)

/*!
 * @brief   Memory tuning controller monitor will record time stamp every time
 *          it is able to make the decision. The watchdog in ISR will compare the
 *          last updated time stamp against the current time stamp and if the
 *          difference is above the threshold, it will trigger the HALT.
 *          The timer threshold is set to 2 samples x sampling period (5 sec)
 *          in LHR 1.0. In LHR1.1 Enhancement timer threshold is set to 6 sec
 */
#define PERF_CF_CONTROLLER_MEM_TUNE_MONITOR_ISR_WATCHDOG_TIMER_THRESHOLD_US() \
    (6000000U)

/*!
 * @brief   High and low threshold of expected number of controller samples.
 */
#define PERF_CF_CONTROLLER_MEM_TUNE_MONITOR_SAMPLES_THRESH_LOW              4
#define PERF_CF_CONTROLLER_MEM_TUNE_MONITOR_SAMPLES_THRESH_HIGH             6

/*!
 * @brief   Threshold if crossed trigger the failure.
 */
#define PERF_CF_CONTROLLER_MEM_TUNE_MONITOR_TRRD_MISMATCH_THRESH            3
#define PERF_CF_CONTROLLER_MEM_TUNE_MONITOR_SAMPLES_MISMATCH_THRESH         3

/*!
 * All state required for PERF_CF_CONTROLLER_MEM_TUNE monitor feature.
 */
typedef struct _PERF_CF_CONTROLLER_MEM_TUNE_MONITOR
{
    /*!
     * Number of samples completed mis-match counter.
     * Increment by one if the observed sample counter cross the
     * pre-determined threshold values.
     */
    LwU32 samplesMisMatchCounter;

    /*!
     * tRRD settings mis-match counter
     * Increment by one if the change sequencer programmed tRRD value
     * does not match with the SW controller requested value.
     */
    LwU32 trrdMisMatchCounterSW;

    /*!
     * tRRD settings mis-match counter
     * Increment by one if the HW programmed tRRD value does not
     * match with the SW controller requested value.
     */
    LwU32 trrdMisMatchCounterHW;
    /*!
     * Measured frequency counter sample info.
     * This is required to callwlate the current MCLK frequency.
     */
    LW2080_CTRL_CLK_CNTR_SAMPLE_ALIGNED clkCntrSample;
} PERF_CF_CONTROLLER_MEM_TUNE_MONITOR;

#define PERF_CF_CONTROLLER_MEM_TUNE_MOVING_AVG_FILTER_WINDOW_SIZE     20

/*!
 * Structure holding moving Average type filter data.
 * This controller decision will evaluate every time a new sample
 * comes in, however the new sample will replace oldest sample within
 * this window. Controller decision will be evaluated based on data within
 * this window.
 * Lwrrently, the function to be applied on this window is "Averaging"
 * i.e. output value will be average of all data within window.
 */
typedef struct
{
    /*!
     * Boolean indicating if window is full
     */
    LwBool          bWindowFull;

    /*!
     * Counter to help in inserting the new samples
     * resets to 0 when it becomes same as windowSize.
     */
    LwU8            count;

    /*!
     * Struct keeping track of target specific filter data.
     */
    struct PERF_CF_CONTROLLER_MEM_TUNE_MOVING_AVG_FILTER_TARGET_DATA
    {
        /*!
         * Keep track of @ref observed over the sampling window.
         * Acts as cirlwlar buffer for evaluating moving average.
         */
        LwUFXP20_12 observedSample[PERF_CF_CONTROLLER_MEM_TUNE_MOVING_AVG_FILTER_WINDOW_SIZE];

        /*!
         * Avg. of observed value over the filter window to compare against the target value
         * to which controller is driving.
         */
        LwUFXP20_12 observedAvg;
    } targetData[LW2080_CTRL_PERF_PERF_CF_CONTROLLER_MEM_TUNE_TARGET_MAX];
} PERF_CF_CONTROLLER_MEM_TUNE_MOVING_AVG_FILTER;

/*!
 * Structure holding static and dynamic per target variables.
 */
typedef struct
{
    /*!
     * Target values
     */
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_CONTROL_MEM_TUNE_TARGET
                target;

    /*!
     * Store aclwmulated sum of all signals counted value.
     */
    LwU64       cntDiffTotal;
    /*!
     * Observed value to compare against the target value to which
     * controller is driving.
     */
    LwUFXP20_12 observed;

    /*!
     * Total number of valid signals.
     */
    LwU8        numSignals;
    /*!
     * Array of BA PM signals being tracked by this controller.
     */
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_STATUS_MEM_TUNE_PM_SIGNAL
                signal[LW2080_CTRL_PERF_PERF_CF_CONTROLLER_MEM_TUNE_TARGET_PM_SIGNAL_MAX];
} PERF_CF_CONTROLLER_MEM_TUNE_TARGET;

/*!
 * Array of all targets data.
 * @ref PERF_CF_CONTROLLER_MEM_TUNE_TARGET
 */
typedef struct
{
    /*!
     * Total number of valid targets.
     */
    LwU8    numTargets;

    /*!
     * Array of target's static POR information.
     * @ref LW2080_CTRL_PERF_PERF_CF_CONTROLLER_STATUS_MEM_TUNE_TARGET
     */
    PERF_CF_CONTROLLER_MEM_TUNE_TARGET
            target[LW2080_CTRL_PERF_PERF_CF_CONTROLLER_MEM_TUNE_TARGET_MAX];
} PERF_CF_CONTROLLER_MEM_TUNE_TARGETS;

/*!
 * PERF_CF_CONTROLLER child class providing attributes of
 * memory tuning PERF_CF Controller.
 */
struct PERF_CF_CONTROLLER_MEM_TUNE
{
    /*!
     * PERF_CF_CONTROLLER parent class.
     * Must be first element of the structure!
     */
    PERF_CF_CONTROLLER      super;

    /*!
     * Boolean tracking whether LHR enhancement is enabled.
     */
    LwBool                  bLhrEnhancementEnabled;
    /*!
     * Index into PERF_CF sensor table for PERF_CF_SENSOR representing
     * all PM's.
     */
    LwBoardObjIdx           pmSensorIdx;
    /*!
     * Index of topology for PTIMER input to the controller.
     */
    LwBoardObjIdx           timerTopologyIdx;
    /*!
     * @ref PERF_CF_CONTROLLER_MEM_TUNE_TARGETS
     */
    PERF_CF_CONTROLLER_MEM_TUNE_TARGETS targets;

    /*!
     * Boolean tracking whether filter based controller enhancement is enabled.
     */
    LwBool                  bFilterEnabled;
    /*!
     * @ref PERF_CF_CONTROLLER_MEM_TUNE_MOVING_AVG_FILTER
     */
    PERF_CF_CONTROLLER_MEM_TUNE_MOVING_AVG_FILTER   filter;
    /*!
     * Number of samples above target before increasing clocks.
     */
    LwU8                    hysteresisCountInc;
    /*!
     * Number of samples below target before decreasing clocks.
     */
    LwU8                    hysteresisCountDec;
    /*!
     * Current number of samples above target.
     */
    LwU8                    hysteresisCountIncLwrr;
    /*!
     * Current number of samples below target.
     */
    LwU8                    hysteresisCountDecLwrr;
    /*!
     * POR max memory clock frequency in kHz.
     */
    LwUFXP52_12             porMaxMclkkHz52_12;
    /*!
     * Latest current memory clock frequency in kHz.
     */
    LwUFXP52_12             mclkkHz52_12;
    /*!
     * PTIMER reading in msec.
     */
    LwUFXP52_12             perfms52_12;
    /*!
     * Store act_rate_sol = <mem freq> * 2 * <# channels> * bytes/channel / bytes
     */
    LwUFXP52_12             activateRateSol52_12;
    /*!
     * Store acc_rate_sol = <mem freq> * 2 * <# channels> * bytes/channel / bytes
     */
    LwUFXP52_12             accessRateSol52_12;
    /*!
     * Boolean tracking whether PCIe override is enabled.
     */
    LwBool                  bPcieOverrideEn;
    /*!
     * Boolean tracking whether DISP override is enabled.
     */
    LwBool                  bDispOverrideEn;
    /*!
     * Boolean tracking whether to engage vs disengage tRRD WAR before
     * any overrides. This is basically tracking the controller decision.
     */
    LwBool                  bTrrdWarEngagePreOverride;
    /*!
     * Current tFAW value to be programmed in HW
     */
    LwU8                    tFAWLwrr;
    /*!
     * Last requested tFAW value to be programmed in HW
     */
    LwU8                    tFAWPrev;
    /*!
     * Counter tracking the samples where tRRD WAR was engaged.
     */
    LwU32                   trrdWarEngageCounter;
    /*!
     * Counter tracking the total number of samples from last reset
     * by @ref PERF_CF_CONTROLLER_MEM_TUNE_MONITOR.
     */
    LwU32                   samplesCounter;
};

/* ------------------------ Global Variables -------------------------------- */
// Timestamp values for memtune controller, checked to make sure it's not starved
extern FLCN_TIMESTAMP  memTuneControllerTimeStamp;
extern FLCN_TIMESTAMP  memTuneControllerMonitorTimeStamp;

//! Identifier values for the timestamps
enum {
    PERF_MEM_TUNE_CONTROLLER_TIMESTAMP,
    PERF_MEM_TUNE_CONTROLLER_MONITOR_TIMESTAMP,
};

/* ------------------------ Public Functions -------------------------------- */
// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet   (perfCfControllerGrpIfaceModel10ObjSetImpl_MEM_TUNE)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfControllerGrpIfaceModel10ObjSetImpl_MEM_TUNE");
BoardObjIfaceModel10GetStatus       (perfCfControllerIfaceModel10GetStatus_MEM_TUNE)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfControllerIfaceModel10GetStatus_MEM_TUNE");

// PERF_CF_CONTROLLER interfaces.
PerfCfControllerExelwte     (perfCfControllerExelwte_MEM_TUNE);
PerfCfControllerLoad        (perfCfControllerLoad_MEM_TUNE)
    GCC_ATTRIB_SECTION("imem_perfCfLoad", "perfCfControllerLoad_MEM_TUNE");
PerfCfControllerSetMultData (perfCfControllerSetMultData_MEM_TUNE)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfControllerSetMultData_MEM_TUNE");

/*!
 * @brief      Creates and schedules the tmr callback responsible for validating
 *             sanity of performance memory tuning controller.
 *
 * @return     FLCN_OK on success.
 */
FLCN_STATUS perfCfControllerMemTuneMonitorLoad(void)
    GCC_ATTRIB_SECTION("imem_perfCf", "perfCfControllerMemTuneMonitorLoad");

// PERF_CF_CONTROLLER_MEM_TUNE helper interfaces.
LwBool perfCfControllerMemTuneIsEngaged(PERF_CF_CONTROLLER_MEM_TUNE *pControllerMemTune)
    GCC_ATTRIB_SECTION("imem_perfCf", "perfCfControllerMemTuneIsEngaged");
LwU32  perfCfControllerMemTuneEngageCounterGet(PERF_CF_CONTROLLER_MEM_TUNE *pControllerMemTune)
    GCC_ATTRIB_SECTION("imem_perfCf", "perfCfControllerMemTuneEngageCounterGet");

/* ------------------------ Inline Functions -------------------------------- */
/*!
 * Macro tracking the PMU LS UCODE VERSION above which LHR enhancement is enabled.
 */
#if (PMU_PROFILE_GA10X_RISCV)
#define LHR_ENHANCEMENT_LS_UCODE_VERSION    4U
#else
#define LHR_ENHANCEMENT_LS_UCODE_VERSION    1U
#endif

/*!
 * @brief      Interface to check whether the LHR enhancement fuse is enabled.
 *
 * @note       There is addition fuse to check whether LHR feature is enabled
 *             which is validated by the parent interfaces.
 *
 * @return  LW_TRUE     if enabled
 * @return  LW_FALSE    if disabled
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline LwBool
perfCfControllerMemTuneIsLhrEnhancementFuseEnabled(void)
{
    return (LWCFG(GLOBAL_FEATURE_GR1295_MEM_TUNE_ENHANCEMENT) &&
            PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_MEM_TUNE_ENHANCEMENT) &&
            (pmuFuseVersionGet_HAL() >= LHR_ENHANCEMENT_LS_UCODE_VERSION) &&
            (pmuFpfVersionGet_HAL() >= LHR_ENHANCEMENT_LS_UCODE_VERSION));
}

/*!
 * @brief      Interface to check whether the LHR enhancement is enabled.
 *
 * @note       There is addition fuse to check whether LHR feature is enabled
 *             which is validated by the parent interfaces.
 *
 * @return  LW_TRUE     if enabled
 * @return  LW_FALSE    if disabled
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline LwBool
perfCfControllerMemTuneIsLhrEnhancementEnabled(PERF_CF_CONTROLLER_MEM_TUNE *pControllerMemTune)
{
    return (LWCFG(GLOBAL_FEATURE_GR1295_MEM_TUNE_ENHANCEMENT) &&
            PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_MEM_TUNE_ENHANCEMENT) &&
            (pControllerMemTune->bLhrEnhancementEnabled));
}

/* ------------------------ Include Derived Types --------------------------- */

#endif // PERF_CF_CONTROLLER_MEM_TUNE_H
