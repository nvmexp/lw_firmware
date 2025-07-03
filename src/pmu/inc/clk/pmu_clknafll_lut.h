/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_CLKNAFLL_LUT_H
#define PMU_CLKNAFLL_LUT_H

/*!
 * @file pmu_clknafll_lut.h
 *
 */

/* ------------------------ System Includes -------------------------------- */
#include "main.h"

/* ------------------------ Forward Definitions ----------------------------- */
/* ------------------------ Application Includes --------------------------- */
/* ------------------------ Macros ----------------------------------------- */
/*!
 * Total number of V/F entries in the LUT.
 */
#define CLK_NAFLL_LUT_VF_NUM_ENTRIES() \
    (Clk.clkNafll.lutNumEntries)

/*!
 * The maximum number of entries we program in the LUT at a time.
 */
#define CLK_LUT_ENTRIES_PER_STRIDE() (2U)

/*!
 * The LUT stride is the number of DWORDS in one table of the LUT
 */
#define CLK_LUT_STRIDE  (CLK_NAFLL_LUT_VF_NUM_ENTRIES() / CLK_LUT_ENTRIES_PER_STRIDE())

/*!
 * LUT temperature index defines.
 * There is space for 5 tables in the LUT, so the max is set to 4.
 * Note: For GP100 only 2 temperature indices are going to be used.
 */
#define CLK_LUT_TEMP_IDX_0                                          (0x00000000U)
#define CLK_LUT_TEMP_IDX_1                                          (0x00000001U)
#define CLK_LUT_TEMP_IDX_2                                          (0x00000002U)
#define CLK_LUT_TEMP_IDX_3                                          (0x00000003U)
#define CLK_LUT_TEMP_IDX_4                                          (0x00000004U)
#define CLK_LUT_TEMP_IDX_MAX                                        CLK_LUT_TEMP_IDX_4

#define CLK_LUT_TEMP_IDX_NUM_IN_USE                                 (0x00000002U)

/*!
 * Macro for the minimum voltage supported by the LUT in uV
 */
#define CLK_NAFLL_LUT_MIN_VOLTAGE_UV() \
    (Clk.clkNafll.lutMilwoltageuV)

/*!
 * Macro for the maximum voltage supported by the LUT in uV
 */
#define CLK_NAFLL_LUT_MAX_VOLTAGE_UV() \
    (Clk.clkNafll.lutMaxVoltageuV)

/*!
 * Macro for the LUT step size in uV
 */
#define CLK_NAFLL_LUT_STEP_SIZE_UV() \
    (Clk.clkNafll.lutStepSizeuV)

/*!
 * Given a voltage value in uV, find the LUT index. Truncate to lower value
 * rather than rounding to closest, while also making sure the codes remain
 * inside the range.
 */
#define CLK_NAFLL_LUT_IDX(v) \
    (((v) < Clk.clkNafll.lutMilwoltageuV) ? 0U : \
     ((v) > Clk.clkNafll.lutMaxVoltageuV) ? (Clk.clkNafll.lutNumEntries - 1U) : \
     (LwU8)(((v) - Clk.clkNafll.lutMilwoltageuV) / Clk.clkNafll.lutStepSizeuV))

/*!
 * Check if LUT is initialized
 */
#define CLK_NAFLL_LUT_IS_INITIALIZED(pNafllDev) \
    (pNafllDev->lutDevice.bInitialized)

/*!
 * Define macros with values which will be used for
 * PMU_PERF_NAFLL_LUT_BROADCAST.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_LUT_BROADCAST)

/*!
 * Helper macro to iterate over the LUT PRIMARY NAFLLs - i.e. the NAFLLs secified
 * in the @ref CLK_NAFLL::lutProgPrimaryMask mask.
 *
 * @note This macro is expect to be used with @ref
 * NAFLL_LUT_PRIMARY_ITERATOR_END.
 *
 * @param[out] pNafll
 *     Pointer in which the current iterator CLK_NAFLL_DEV pointer will be
 *     returned.
 * @param[out] devIdx
 *     LwU32 in which the current iterator index of the current @ref pNafll will
 *     be returned.
 */
#define NAFLL_LUT_PRIMARY_ITERATOR_BEGIN(pNafll, devIdx)                        \
    BOARDOBJGRP_ITERATOR_BEGIN(NAFLL_DEVICE, (pNafll), (devIdx),           \
        &Clk.clkNafll.lutProgPrimaryMask.super)

/*!
 * Helper macro to iterate over the LUT BROADCAST SECONDARY NAFLLs of the current
 * LUT BROADCAST PRIMARY NAFLL.  @note PRIMARY NAFLL is one its own SECONDARY
 * DEVICES.
 *
 * This macro is intended to be used to around any HW or SW operations which
 * need to broadcast to all SECONDARY NAFLLs of the PRIMARY - e.g. SW state tracking,
 * register writes, etc.
 *
 * @note This macro is expect to be used with @ref
 * NAFLL_LUT_BROADCAST_ITERATOR_END.
 *
 * @param[in/out] pNafll
 *     Pointer to original PRIMARY NAFLL.  This macro will iterate over its mask
 *     of SECONDARY NAFLLs, returning each SECONDARY in this same variable.  @ref
 *     NAFLL_LUT_BROADCAST_ITERATOR_END will return this pointer back to its
 *     original PRIMARY NAFLL value.
 */
#define NAFLL_LUT_BROADCAST_ITERATOR_BEGIN(pNafll)                             \
    do {                                                                       \
        CLK_NAFLL_DEVICE *_pNafllOrig = (pNafll);                              \
        LwBoardObjIdx     _idx;                                                \
        BOARDOBJGRP_ITERATOR_BEGIN(NAFLL_DEVICE, (pNafll), _idx,           \
            &_pNafllOrig->lutProgBroadcastSecondaryMask.super)                     \
        {

/*!
 * Helper macro to iterate over the LUT BROADCAST SECONDARY NAFLLs of the current
 * LUT BROADCAST PRIMARY NAFLL.  @note PRIMARY NAFLL is one its own SECONDARY DEVICES.
 *
 * This macro completes the iterator loop and returns pNafll back to its
 * original value.
 *
 * @note This macro is expect to be used with @ref
 * NAFLL_LUT_BROADCAST_ITERATOR_BEGIN.
 *
 * @param[out] pNafll
 *     Pointer in which to return the original PRIMARY NAFLL.
 */
#define NAFLL_LUT_BROADCAST_ITERATOR_END(pNafll)                               \
        }                                                                      \
        BOARDOBJGRP_ITERATOR_END;                                              \
        (pNafll) = _pNafllOrig;                                                \
    } while (LW_FALSE)

/*!
 * Define macros with for use without PMU_PERF_NAFLL_LUT_BROADCAST.  These
 * collapse back to the legacy behavior.
 */
#else
/*!
 * Helper macro to iterate over the LUT PRIMARY NAFLLs - i.e. the NAFLLs secified
 * in the @ref CLK_NAFLL::lutProgPrimaryMask mask.
 *
 * @note On GPUs without PMU_PERF_NAFLL_LUT_BROADCAST this macro iterates over
 * the entire NAFLLs BOARDOBJGRP.
 *
 * @note This macro is expect to be used with @ref
 * NAFLL_LUT_PRIMARY_ITERATOR_END.
 *
 * @param[out] pNafll
 *     Pointer in which the current iterator CLK_NAFLL_DEV pointer will be
 *     returned.
 * @param[out] devIdx
 *     LwU32 in which the current iterator index of the current @ref pNafll will
 *     be returned.
 */
#define NAFLL_LUT_PRIMARY_ITERATOR_BEGIN(pNafll, devIdx)                        \
    BOARDOBJGRP_ITERATOR_BEGIN(NAFLL_DEVICE, (pNafll), (devIdx), NULL)

/*!
 * Helper macro to iterate over the LUT BROADCAST SECONDARY NAFLLs of the current
 * LUT BROADCAST PRIMARY NAFLL.  @note PRIMARY NAFLL is one its own SECONDARY DEVICES.
 *
 * @note On GPUs without PMU_PERF_NAFLL_LUT_BROADCAST this macro is a no-op.
 *
 * @note This macro is expect to be used with @ref
 * NAFLL_LUT_BROADCAST_ITERATOR_END.
 *
 * @param[in/out] pNafll Unused.
 */
#define NAFLL_LUT_BROADCAST_ITERATOR_BEGIN(pNafll)
/*!
 * Helper macro to iterate over the LUT BROADCAST SECONDARY NAFLLs of the current
 * LUT BROADCAST PRIMARY NAFLL.  @note PRIMARY NAFLL is one its own SECONDARY DEVICES.
 *
 * @note On GPUs without PMU_PERF_NAFLL_LUT_BROADCAST this macro is a no-op.
 *
 * @note This macro is expect to be used with @ref
 * NAFLL_LUT_BROADCAST_ITERATOR_BEGIN.
 *
 * @param[in/out] pNafll Unused.
 */
#define NAFLL_LUT_BROADCAST_ITERATOR_END(pNafll)

#endif

/*!
 * Helper macro to iterate over the LUT PRIMARY NAFLLs - i.e. the NAFLLs secified
 * in the @ref CLK_NAFLL::lutProgPrimaryMask mask.
 *
 * This macro completes the iterator loop.
 */
#define NAFLL_LUT_PRIMARY_ITERATOR_END                                          \
    BOARDOBJGRP_ITERATOR_END

/*!
 * Helper macro to figure out if secondary lwrve is supported and enabled for
 * slowdown
 *
 */
#define PMU_CLK_NAFLL_IS_SEC_LWRVE_ENABLED()                                   \
        (PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_SEC_LWRVE) &&                   \
         (clkDomainsIsSecVFLwrvesEnabled()))

/* ------------------------ Types Definitions ------------------------------ */
/*!
 * Structure describing the state of a LUT
 */
typedef struct
{
    /*!
     * The global ID @ref LW2080_CTRL_PERF_NAFLL_ID_<xyz> of this NAFLL device
     * This should be the same as the one in NAFLL_DEVICE.
     */
    LwU8     id;

    /*!
     * The mode value determined by the LUT Vselect Mux.
     */
    LwU8     vselectMode;

    /*!
     * The ADC code threshold for hysteresis.
     */
    LwU16    hysteresisThreshold;

    /*!
     * The mode value determined by the SW Override Mux.
     */
    LwU8     swOverrideMode;

    /*!
     * Current Temperature Index for the LUT.
     */
    LwU8     lwrrentTempIndex;

    /*!
     * Previous Temperature Index for the LUT.
     */
    LwU8     prevTempIndex;

    /*!
     * Set to LW_TRUE once a LUT table is initialized
     */
    LwBool   bInitialized;

    /*!
     * Cache SW override LUT entry value.
     */
    LW2080_CTRL_CLK_LUT_VF_ENTRY_DATA   swOverride;

#if PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_LUT_QUICK_SLOWDOWN_ENGAGE_SUPPORT)
    /*!
     * Boolean to indicate the state of the quick slowdown feature
     */
    LwBool   bQuickSlowdownForceEngage[LW2080_CTRL_CLK_NAFLL_LUT_VF_LWRVE_SEC_MAX];
#endif
} CLK_LUT_DEVICE;

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/*!
 * @brief Construct the LUT device.
 *
 * @param[in]   pNafllDev       Pointer to the NAFLL device
 * @param[in]   pLutDevice      Pointer to the LUT device
 * @param[in]   pLutDeviceDesc  Pointer to the LUT device descriptor
 * @param[in]   bFirstConstruct Is this the first call for LUT construct?
 *
 * @return FLCN_OK                      LUT device constructed successfully
 * @return FLCN_ERR_ILWALID_ARGUMENT    At least one of the input arguments passed is invalid
 * @return other                        Descriptive status code from sub-routines on success/failure
 */
#define ClkNafllLutConstruct(fname) FLCN_STATUS (fname)(CLK_NAFLL_DEVICE *pNafllDev, CLK_LUT_DEVICE *pLutDevice, \
    RM_PMU_CLK_LUT_DEVICE_DESC *pLutDeviceDesc, LwBool bFirstConstruct)

/*!
 * @brief Set the LUT SW override
 *
 * @param[in]   pNafllDev       Pointer to the NAFLL device
 * @param[in]   overrideMode    The LUT override mode to set
 * @param[in]   freqMHz         SW target frequency in MHz
 *
 * @return FLCN_OK                      LUT override set successfully
 * @return FLCN_ERR_ILWALID_ARGUMENT    Invalid ID or override mode
 * @return other                        Descriptive status code from sub-routines on success/failure
 */
#define ClkNafllLutOverride(fname) FLCN_STATUS (fname)(CLK_NAFLL_DEVICE *pNafllDev, LwU8 overrideMode, LwU16 freqMHz)

/*!
 * @brief Program all LUT entries for a given NAFLL device.
 *
 * @param[in]   pNafllDev      Pointer to the NAFLL device
 *
 * @return FLCN_OK                Successfully programmed all LUT entries.
 * @return other                  Descriptive status code from sub-routines on success/failure
 */
#define ClkNafllLutEntriesProgram(fname) FLCN_STATUS (fname)(CLK_NAFLL_DEVICE *pNafllDev)

/*!
 * @brief Get LUT VF Lwrve data
 *
 * @param[in]   pNafllDev       Pointer to the NAFLL device
 * @param[out]  pLutVfLwrve     Pointer to the LUT VF Lwrve
 *
 * @return  FLCN_OK     Lut VF Lwrve data successfully retrieved
 * @return  other       Descriptive status code from sub-routines on success/failure
 */
#define ClkNafllLutVfLwrveGet(fname) FLCN_STATUS (fname)(CLK_NAFLL_DEVICE *pNafllDev, LW2080_CTRL_CLK_LUT_VF_LWRVE *pLutVfLwrve)

/*!
 * @brief Helper to callwlate the ndiv, ndivOffset and dvcoOffset tuple for a
 * given vfIdx by looking up the freq for the corrected voltage
 *
 * @param[in]   pNafllDev       Pointer to the NAFLL device
 * @param[in]   vfIdx           V/F index for which params are required
 * @param[out]  pLutEntryData   LUT Entry Data
 * @param[out]  pVfIterState    VF Iteration State
 *
 * @return  FLCN_OK     Lut Entry Data callwlated successfully
 * @return  other       Descriptive status code from sub-routines on success/failure
 */
#define ClkNafllLutEntryCallwlate(fname) FLCN_STATUS (fname)(CLK_NAFLL_DEVICE *pNafllDev, LwU16 vfIdx, \
    LW2080_CTRL_CLK_LUT_VF_ENTRY_DATA *pLutEntryData, LW2080_CTRL_CLK_VF_ITERATION_STATE *pVfIterState)

// Routines that belong to the .perfClkAvfsInit overlay
ClkNafllLutConstruct        (clkNafllLutConstruct)
    GCC_ATTRIB_SECTION("imem_perfClkAvfsInit", "clkNafllLutConstruct");
ClkNafllLutConstruct        (clkNafllLutConstruct_SUPER)
    GCC_ATTRIB_SECTION("imem_perfClkAvfsInit", "clkNafllLutConstruct_SUPER");

// Routines that belong to the .perfClkAvfs overlay
LwU8        clkNafllLutNextTempIdxGet(CLK_NAFLL_DEVICE *pNafllDev)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkNafllLutNextTempIdxGet");
FLCN_STATUS clkNafllLutFreqMHzGet(CLK_NAFLL_DEVICE *pNafllDev, LwU32 voltageuV, LwU8 voltType, LwU16 *pFreqMHz, LW2080_CTRL_CLK_VF_ITERATION_STATE *pVfIterState)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkNafllLutFreqMHzGet");
ClkNafllLutOverride         (clkNafllLutOverride)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkNafllLutOverride");
FLCN_STATUS         clkNafllUpdateLutForAll(void)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkNafllUpdateLutForAll");
FLCN_STATUS clkNafllLutProgram(CLK_NAFLL_DEVICE *pNafllDev)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkNafllLutProgram");
ClkNafllLutEntriesProgram   (clkNafllLutEntriesProgram)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkNafllLutEntriesProgram");
FLCN_STATUS clkNafllTempIdxUpdate(CLK_NAFLL_DEVICE *pNafllDev, LwU8 tempIdx)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkNafllTempIdxUpdate");
LwU32       clkNafllLutVoltageGet(CLK_NAFLL_DEVICE *pNafllDev)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkNafllLutVoltageGet");
void        clkNafllLutEntryVoltCallwlate(CLK_NAFLL_DEVICE *pNafllDev, LwU16 vfIdx, LwU32 *pVoltuV)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkNafllLutEntryVoltCallwlate");
ClkNafllLutVfLwrveGet       (clkNafllLutVfLwrveGet)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkNafllLutVfLwrveGet");

// Routines that belong to the .perfVf overlay
FLCN_STATUS clkNafllLutOffsetTupleFromFreqCompute (CLK_NAFLL_DEVICE *pNafllDev, LwU16 freqMHz, LW2080_CTRL_CLK_CLK_VF_POINT_LUT_FREQ_TUPLE *pClkVfTupleOutput)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkNafllLutOffsetTupleFromFreqCompute");

/* ------------------------ Include Derived Types -------------------------- */
#include "clk/pmu_clknafll_lut_10.h"
#include "clk/pmu_clknafll_lut_20.h"
#include "clk/pmu_clknafll_lut_30.h"
#include "clk/pmu_clknafll_lut_reg_map.h"

#endif // PMU_CLKNAFLL_LUT_H
