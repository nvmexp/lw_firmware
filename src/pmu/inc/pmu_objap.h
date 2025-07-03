/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_OBJAP_H
#define PMU_OBJAP_H

/*!
 * @file pmu_objap.h
 *
 * @Brief Generic declarations for Adaptive PG offloaded to PMU (OBJAP)
 *
 * OBJAP object provides generic infrastructure to make any power feature
 * adaptive. E.g. We can make GR-ELPG, MSCG, GC4/5 adaptive by using
 * infrastructure of OBJAP. We call these features as clients of OBJAP.
 * OBJAP implements OBJAPCTR for each client. OBJAPCTRL help us to cache
 * and control client specific characteristics of adaptive algorithm.
 *
 * OBJAP implements whole Adaptive algorithm. But a client decides when to
 * execute that algorithm. Thus it's responsibility of client to implement
 * callback function and related logic that triggers the algorithm.
 *
 * Refer - "Design: Adaptive Algorithm offload to PMU" @pmuifap.h
 *
 */

/* ------------------------ System includes -------------------------------- */
#include "flcntypes.h"

/* ------------------------ Application includes --------------------------- */
#include "pmu_objpg.h"
#include "config/g_ap_hal.h"

/* ------------------------ Macros & Defines ------------------------------- */
/* ------------------------ Types definitions ------------------------------ */

// Get OBJAPCTRL
#define AP_GET_CTRL(ctrlId)             (Ap.pApCtrl[ctrlId])

//
// Deep-TODO: Define this macro into OBJPMU as per Nemko's suggestion
// OBJPMU will store current value of PWRCLK. PMU_GET_PWRCLK_MHZ() macro
// will return that value. For now, I am defining it as 324MHz.
//
// If this solution takes time then sending frequency as a parameter to the
// init command is quick and prefered way.
//
#define AP_GET_PWRCLK_MHZ()             (Ap.pwrClkMHz)

// Check if APCTRL is supported
#define AP_IS_CTRL_SUPPORTED(ctrlId)    ((Ap.supportedMask & BIT(ctrlId)) != 0)

// Check if APCTRL is enabled
#define AP_IS_CTRL_ENABLED(ctrlId)      (Ap.pApCtrl[ctrlId]->disableReasonMask == 0)

// Get Predicted Idle Threshold in pwrclk cycles.
#define AP_CTRL_PREDICTED_THRESHOLD_CYCLES_GET(ctrlId)                      \
                                        (Ap.pApCtrl[ctrlId]->idleThresholdCycles)

//
// By default, skip count is set to one so that we will skip the first sample
//
// Centralised LPWR callback exelwtes AP algorithm. It likely possible that AP
// got enabled in middle of base callback period. Thus first callback might
// come before ApCtrl sampling period.
//
#define AP_CTRL_SKIP_COUNT_DEFAULT                  1

/*!
 * @brief Default latency overhead for AP callwlations
 *
 * Residency callwlation - 400us (entry + exit latency)
 * Power callwlation     - 400us (entry + exit latency)
 *
 * The latencies are considered for GR feature here
 */
#define AP_LATENCY_OVERHEAD_FOR_RESIDENCY_DEFAULT_US   400
#define AP_LATENCY_OVERHEAD_FOR_POWER_DEFAULT_US       400

/*!
 * @brief Latency overhead for Adaptive MSCG callwlations
 *
 * Residency callwlation - 900us (entry + exit latency)
 * Power callwlation     - 450us (entry latency)
 *
 * MSCG does not have power loss due to entry/exit sequence,
 * but we are considering the entry latency as overhead for
 * power callwlations to avoid aborts which help help optimize
 * PMU bandwidth
 */
#define AP_LATENCY_OVERHEAD_FOR_RESIDENCY_MSCG_US      900
#define AP_LATENCY_OVERHEAD_FOR_POWER_MSCG_US          450

/*!
 * @brief Minimum targeted power saving
 * This is temporary define.
 */
#define AP_CTRL_MINIMUM_TARGET_SAVING_DEFAULT_US    10000

#define AP_MAX_VALUE_STORED_BY_HISTOGRAM_BIN        0xffff

/*!
 * @Enable/Disable mask for ApCtrl
 */
#define AP_CTRL_ENABLE_DISABLE_REASON_ID_MASK_RM         BIT(0)
#define AP_CTRL_ENABLE_DISABLE_REASON_ID_MASK_PERF       BIT(1)

/*!
 * @brief Macros to get enable/disable mask for ApCtrl
 */
#define AP_CTRL_ENABLE_MASK(reason)      (AP_CTRL_ENABLE_DISABLE_REASON_ID_MASK_##reason)
#define AP_CTRL_DISABLE_MASK(reason)     (AP_CTRL_ENABLE_DISABLE_REASON_ID_MASK_##reason)

/*!
 * @brief AP_CTRL
 *
 * AP_CTRL cache controller specific characteristics of adaptive algorithm.
 *
 * NOTE : HCycles means Hundred Cycles.
 */
typedef struct
{
    // Disable reason mask - AP_CTRL_ENABLE_DISABLE_REASON_ID_MASK_xyz
    LwU32   disableReasonMask;

    // Minimum idle filter value in usec and PWRCLK Cycle
    LwU32   idleThresholdMinUs;
    LwU32   idleThresholdMinCycles;

    // Maximum idle filter value in usec and PWRCLK Cycle
    LwU32   idleThresholdMaxUs;
    LwU32   idleThresholdMaxCycles;

    //
    // Minimum Targeted Saving in usec and HCycles (Hundred PWRCLK Cycles)
    // AP updates idle thresholds only if power saving achieved by new idle
    // thresholds is greater than minimum targeted saving.
    //
    LwU32   minTargetSavingUs;
    LwU32   minTargetSavingHCycles;

    // Minimum targeted residency of power feature in usec and HCycles
    LwU32   powerBreakEvenUs;
    LwS32   powerBreakEvenHCycles;

    //
    // Maximum number of allowed power feature cycles per sample.
    //
    // We are allowing at max "pgPerSampleMax" cycles in one iteration of AP.
    // AKA pgPerSampleMax in original algorithm.
    //
    LwU16   cyclesPerSampleMax;

    // Minimum targeted residency
    LwU8    minResidency;

    //
    // ApCtrl callback ID - LPWR_CALLBACK_ID_AP_xyz. Its used to
    // schedule/deschedule callback
    //
    LwU8    callbackId;

    // Number of pwrClk cycles represented by one sample in HistogramBin(0)
    LwU32   bin0Cycles;

    //
    // Current predicted idle threshold in pwrclk cycles.
    // It has two possible values -
    // 1) "bin0Cycles << idleFilterX" if AP is enabled and active.
    // 2) "Default" if AP is disabled or not active.
    //
    LwU32   idleThresholdCycles;

    // Pointer to AP statistics
    RM_PMU_AP_CTRL_STAT *pStat;

    // Supplemental Idle Mask for ApCtrl
    LwU32   idleMask[RM_PMU_PG_IDLE_BANK_SIZE];

    // SW managed Bins
    LwS16  *pSwBins;
} AP_CTRL;

/*!
 * OBJAP
 *
 * OBJAP object provides generic infrastructure to make any power feature
 * adaptive. OBJAP does not have any mechanism to implement callback. Its
 * responsibility of client to implement callback mechanism.
 *
 * Refer - "Design: Adaptive Algorithm offload to PMU" @pmuifap.h
 */
typedef struct
{
    // Adaptive PG supported Mask
    LwU32           supportedMask;

    // pwrclk in MHz
    LwU32           pwrClkMHz;

    // Set of All AP Ctrl Pointers.
    AP_CTRL        *pApCtrl[RM_PMU_AP_CTRL_ID__COUNT];
} OBJAP;

/* ------------------------ External definitions --------------------------- */
extern OBJAP Ap;

/* ------------------------ Function Prototypes ---------------------------- */
FLCN_STATUS apPostInit(void)
            GCC_ATTRIB_SECTION("imem_lpwrLoadin", "apPostInit");

FLCN_STATUS apInit(RM_PMU_RPC_STRUCT_LPWR_LOADIN_AP_INIT *)
            GCC_ATTRIB_SECTION("imem_lpwrLoadin", "apInit");

FLCN_STATUS apCtrlInit(RM_PMU_RPC_STRUCT_LPWR_LOADIN_AP_CTRL_INIT_AND_ENABLE *)
            GCC_ATTRIB_SECTION("imem_lpwrLoadin", "apCtrlInit");

// Object interfaces of OBJAPCTRL
FLCN_STATUS apCtrlEnable(LwU8, LwU32)
            GCC_ATTRIB_SECTION("imem_libAp", "apCtrlEnable");
FLCN_STATUS apCtrlDisable(LwU8, LwU32)
            GCC_ATTRIB_SECTION("imem_libAp", "apCtrlDisable");
FLCN_STATUS apCtrlKick(LwU8 ctrlId, LwU32)
            GCC_ATTRIB_SECTION("imem_libAp", "apCtrlKick");
FLCN_STATUS apCtrlExelwte(LwU8)
            GCC_ATTRIB_SECTION("imem_libAp", "apCtrlExelwte");
LwU8        apCtrlGetHistogramIndex(LwU8)
            GCC_ATTRIB_SECTION("imem_libAp", "apCtrlGetHistogramIndex");
LwU8        apCtrlGetIdleMaskIndex(LwU8)
            GCC_ATTRIB_SECTION("imem_libAp", "apCtrlGetIdleMaskIndex");
void        apCtrlIncrementHistCntr(LwU8, LwU32)
            GCC_ATTRIB_SECTION("imem_libAp", "apCtrlIncrementHistCntr");
void        apCtrlMergeSwHistBin(LwU8)
            GCC_ATTRIB_SECTION("imem_libAp", "apCtrlMergeSwHistBin");
void        apCtrlIdleThresholdSet(LwU8 apCtrlId, LwBool bImmediateThresholdUpdate);
FLCN_STATUS apCtrlEventHandlerPwrSrc(LwU8, LwBool)
            GCC_ATTRIB_SECTION("imem_libAp", "apCtrlEventHandlerPwrSrc");

#endif // PMU_OBJAP_H
