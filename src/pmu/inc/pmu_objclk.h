/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_OBJCLK_H
#define PMU_OBJCLK_H

#include "g_pmu_objclk.h"

#ifndef G_PMU_OBJCLK_H
#define G_PMU_OBJCLK_H

/*!
 * @file pmu_objclk.h
 */

/* ------------------------ System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes --------------------------- */
#include "clk/pmu_clkcntr.h"
#include "clk/pmu_clknafll.h"
#include "clk/pmu_clkadc.h"
#include "clk/clk_domain.h"
#include "clk/client_clk_domain.h"
#include "clk/clk_prog.h"
#include "clk/clk_prop_regime.h"
#include "clk/clk_prop_top.h"
#include "clk/clk_prop_top_rel.h"
#include "clk/client_clk_prop_top_pol.h"
#include "clk/clk_enum.h"
#include "clk/clk_vf_rel.h"
#include "clk/clk_vf_point.h"
#include "clk/client_clk_vf_point.h"
#include "clk/clk_controller.h"
#include "clk/clk_freq_controller.h"
#include "clk/clk_freq_counted_avg.h"
#include "clk/clk_volt_controller.h"
#include "clk/clk_pll_info.h"
#include "clk3/dom/clkfreqdomain.h"
#include "pmu_objpmu.h"
#include "config/g_clk_hal.h"
#include "lib/lib_avg.h"
#include "clk3/clk3.h"
#include "clk/pmu_clklpwr.h"

/* ------------------------ Defines ---------------------------------------- */
/* ------------------------ Types Definitions ------------------------------ */

/*!
 * @brief  Helper macro for Clock mutex timeout value - 100us
 */
#define CLK_MUTEX_ACQUIRE_TIMEOUT_NS       (100000)

/*!
 * @brief  Helper macro for Clock mutex acquire
 */
#define clkMutexAcquire(tk)        mutexAcquire(LW_MUTEX_ID_CLK,               \
                                                CLK_MUTEX_ACQUIRE_TIMEOUT_NS, tk)

/*!
 * @brief  Helper macro for Clock mutex release
 */
#define clkMutexRelease(tk)        mutexRelease(LW_MUTEX_ID_CLK, tk)

/*!
 * @brief  Helper macro to get TRRD reset value
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_FBFLCN_TRRD_PROGRAM_SUPPORTED))
#define clkFbflcnTrrdValResetGet()    (Clk.trrdValReset)
#else
#define clkFbflcnTrrdValResetGet()    LW_U16_MIN
#endif // (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VOLT_CONTROLLER))

/*!
 * @brief  Helper macro to set TRRD reset value
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_FBFLCN_TRRD_PROGRAM_SUPPORTED))
#define clkFbflcnTrrdValResetSet(_trrdValReset)    (Clk.trrdValReset = (_trrdValReset))
#else
#define clkFbflcnTrrdValResetSet(_trrdValReset)    lwosVarNOP1(_trrdValReset)
#endif // (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VOLT_CONTROLLER))


/*!
 * @brief  Helper macro for check if AVFS (ADCs and NAFLLs) are supported
 * i.e. all basic features and both the ADC and NAFLL boardObjGrps are non-empty
 */
#define CLK_IS_AVFS_SUPPORTED()                                \
            (PMUCFG_FEATURE_ENABLED(PMU_PERF_AVFS)          && \
             PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_DEVICES) && \
             PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_DEVICES)   && \
             (!BOARDOBJGRP_IS_EMPTY(NAFLL_DEVICE))          && \
             (!BOARDOBJGRP_IS_EMPTY(ADC_DEVICE)))

/*!
 * @brief       Clock Domains Abstractions
 * @see         http://txcovlinc.lwpu.com:8080/doc/en/cov_checker_ref.html#static_checker_DEADCODE
 *
 * @details     Previous to November 2017, we would test the 1X feature using
 *              ordinary 'if' statements via a macro called 'clkDomains1xSupported'.
 *              The problem with that approach is that Coverity issues lots of
 *              'Logically Dead Code' errors.
 *
 * @note        CLK_DOMAIN_MASK_LTC is _UNDEFINED when supporting the 1X
 *              distribution mode (Volta and later) since there has never been
 *              a 1X LTCLK.  To avoid Coverity errors, it is often necessary
 *              to use '#if' to check for a nonzero value to wrap logic that
 *              that uses this value in a normal 'if' or '?:' conditional.
 */

#if PMUCFG_FEATURE_ENABLED(PMU_CLK_DOMAINS_1X_SUPPORTED)
#define CLK_DOMAIN_MASK_GPC             LW2080_CTRL_CLK_DOMAIN_GPCCLK
#define CLK_DOMAIN_MASK_XBAR            LW2080_CTRL_CLK_DOMAIN_XBARCLK
#define CLK_DOMAIN_MASK_SYS             LW2080_CTRL_CLK_DOMAIN_SYSCLK
#define CLK_DOMAIN_MASK_HUB             LW2080_CTRL_CLK_DOMAIN_HUBCLK
#define CLK_DOMAIN_MASK_LTC             LW2080_CTRL_CLK_DOMAIN_UNDEFINED

#else
#define CLK_DOMAIN_MASK_GPC             LW2080_CTRL_CLK_DOMAIN_GPC2CLK
#define CLK_DOMAIN_MASK_XBAR            LW2080_CTRL_CLK_DOMAIN_XBAR2CLK
#define CLK_DOMAIN_MASK_SYS             LW2080_CTRL_CLK_DOMAIN_SYS2CLK
#define CLK_DOMAIN_MASK_HUB             LW2080_CTRL_CLK_DOMAIN_HUB2CLK
#define CLK_DOMAIN_MASK_LTC             LW2080_CTRL_CLK_DOMAIN_LTC2CLK
#endif

#define CLK_DOMAIN_MASK_LWD             LW2080_CTRL_CLK_DOMAIN_LWDCLK
#define CLK_DOMAIN_MASK_PWR             LW2080_CTRL_CLK_DOMAIN_PWRCLK
#define CLK_DOMAIN_MASK_UTILS           LW2080_CTRL_CLK_DOMAIN_UTILSCLK
#define CLK_DOMAIN_MASK_DISP            LW2080_CTRL_CLK_DOMAIN_DISPCLK
#define CLK_DOMAIN_MASK_VCLK0           LW2080_CTRL_CLK_DOMAIN_VCLK0
#define CLK_DOMAIN_MASK_VCLK1           LW2080_CTRL_CLK_DOMAIN_VCLK1
#define CLK_DOMAIN_MASK_VCLK2           LW2080_CTRL_CLK_DOMAIN_VCLK2
#define CLK_DOMAIN_MASK_VCLK3           LW2080_CTRL_CLK_DOMAIN_VCLK3

#if PMUCFG_FEATURE_ENABLED(PMU_HOSTCLK_PRESENT)
#define CLK_DOMAIN_MASK_HOST            LW2080_CTRL_CLK_DOMAIN_HOSTCLK
#else // HOPPER_and_later
#define CLK_DOMAIN_MASK_HOST            0
#endif

/*!
 * Clock Object definition
 */
typedef struct
{
#if PMUCFG_FEATURE_ENABLED(PMU_CLK_DOMAINS_1X_AND_2X_SUPPORTED)
    /*!
     * @brief Mask of clock domains having distribution mode set to 1x.
     *
     * @details This mask is needed only for chips which have both 1x and 2x
     *          domains supported, and not for chips which have either 1x or
     *          2x supported.  This data comes from LW_PTRIM_SYS_CLK_DIST_MODE.
     */
    LwU32          clkDist1xDomMask;
#endif // PMUCFG_FEATURE_ENABLED(PMU_CLK_DOMAINS_1X_AND_2X_SUPPORTED)

#if PMUCFG_FEATURE_ENABLED(PMU_LIB_AVG)
    /*!
     * @brief Software clock counter average
     *
     *  \note Not applicable for Auto
     */
    COUNTER_AVG    clkSwCounterAvg[COUNTER_AVG_CLIENT_ID_MAX];
#endif // (PMUCFG_FEATURE_ENABLED(PMU_LIB_AVG))

#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CNTR))
    /*!
     * @brief HW clock counters
     *
     *  \note Not applicable for Auto
     */
    CLK_CNTRS               cntrs;
#endif // (PMUCFG_FEATURE_ENABLED(PMU_CLK_CNTR))

#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_AVFS))
    /*!
     * @brief ADC devices
     */
    struct
    {
        BOARDOBJGRP_E32     super;

        /*!
         * Mask which tracks the synchronized register access state.  When the
         * mask is non-zero, HW register access to all ADCS is disabled and the
         * cached register value will be used instead.
         *
         * NOTE: This variable had to be moved here out of CLK_ADC because it
         * needs to be accessed in clkAdcPreInit - well before we can dynamically
         * allocate memory for CLK_ADC object.
         */
        LwU32               adcAccessDisabledMask;

        CLK_ADC            *pAdcs;
    } clkAdc;

    /*!
     * @brief NAFLL devices
     */
    CLK_NAFLL               clkNafll;
#endif // (PMUCFG_FEATURE_ENABLED(PMU_PERF_AVFS))

#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN))
    /*!
     * @brief CLK_SW clock domains
     */
    CLK_DOMAINS             domains;
#endif // (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN))

#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLIENT_CLK_DOMAIN))
    /*!
     * @brief CLK_SW client clock domains
     */
    CLIENT_CLK_DOMAINS      clientClkDomains;
#endif // (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLIENT_CLK_DOMAIN))

#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_PROG))
    /*!
     * @brief CLK_SW programmable clock domains
     */
    CLK_PROGS               progs;
#endif // (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_PROG))

#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_ENUM))
    /*!
     * @brief CLK_SW clock enums
     */
    CLK_ENUMS               enums;
#endif // (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_ENUM))

#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_REL))
    /*!
     * @brief CLK_SW VF rel
     */
    CLK_VF_RELS             vfRels;
#endif // (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_REL))

#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT))
    /*!
     * @brief CLK_SW VF points
     */
    CLK_VF_POINTS           vfPoints;
#endif // (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT))

#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLIENT_CLK_VF_POINT))
    /*!
     * @brief CLK_SW VF points
     */
    CLIENT_CLK_VF_POINTS    clientVfPoints;
#endif // (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLIENT_CLK_VF_POINT))

#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_PROP_REGIME))
    /*!
     * @brief CLK_SW clock prop regimes
     */
    CLK_PROP_REGIMES        propRegimes;
#endif // (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_PROP_REGIME))

#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_PROP_TOP))
    /*!
     * @brief CLK_SW clock prop tops
     */
    CLK_PROP_TOPS           propTops;
#endif // (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_PROP_TOP))

#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_PROP_TOP_REL))
    /*!
     * @brief CLK_SW clock prop top rels
     */
    CLK_PROP_TOP_RELS       propTopRels;
#endif // (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_PROP_TOP_REL))

#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLIENT_CLK_PROP_TOP_POL))
    /*!
     * @brief CLK_SW clock prop top policys
     */
    CLIENT_CLK_PROP_TOP_POLS       propTopPols;
#endif // (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLIENT_CLK_PROP_TOP_POL))

#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_CONTROLLER))
    /*!
     * @brief clock frequency controllers
     *
     *  \note Not applicable for Auto
     */
    CLK_FREQ_CONTROLLERS    *pFreqControllers;
#endif // (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_CONTROLLER))

#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_COUNTED_AVG))
    /*!
     * @brief SW cached clock counters
     *
     *  \note Not applicable for Auto
     */
    CLK_FREQ_COUNTED_AVG    freqCountedAvg;
#endif // (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_COUNTED_AVG))

#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_PLL_DEVICE))
    /*!
     * @brief CLK30 Pll devices
     *
     *  \note Not applicable for Auto
     */
    PLL_DEVICES             pllDevices;
#endif // (PMUCFG_FEATURE_ENABLED(PMU_CLK_PLL_DEVICE))

#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLKS_ON_PMU))
    /*!
     * @brief CLK30 frequency domain groups
     *
     *  \note Not applicable for Auto
     */
    FREQ_DOMAIN_GRP         freqDomainGrp;
#endif // (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLKS_ON_PMU))

#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VOLT_CONTROLLER))
    /*!
     * @brief voltage controllers
     *
     *  \note Not applicable for Auto
     */
    CLK_VOLT_CONTROLLERS    voltControllers;
#endif // (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VOLT_CONTROLLER))

#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_FBFLCN_TRRD_PROGRAM_SUPPORTED))
    /*!
     * @brief Cache the reset value of tRRD memory setting.
     *
     *  \note This is temporary code to unblock WAR.
     */
    LwU16 trrdValReset;
#endif // (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VOLT_CONTROLLER))

} OBJCLK;

/*!
 * Helper function to adjust a frequency by the provided delta.
 *
 * @note The frequency units are intentionally mismatched - MHz vs kHz.  The
 * frequency deltas are specified in kHz but the CLK code internally stores
 * frequency in MHz.  This helper functions takes care of doing colwersion kHz
 * -> MHz.  Eventually, CLK code will move to internally using kHz.  At that
 * point, this interface will be reworked.
 *
 * @param[in]  freqMHz       Frequency (MHz) to adjust
 * @param[in]  freqDeltakHz  Frequency delta (kHz)
 *
 * @return Adjusted frequency value (MHz)
 */
#define ClkFreqDeltaAdjust(fname) LwU16 (fname)(LwU16 freqMHz, LW2080_CTRL_CLK_FREQ_DELTA *pFreqDelta)

/*!
 * Helper function to adjust a voltage by the provided delta.
 *
 * @param[in]  voltageuV     Frequency (uV) to adjust
 * @param[in]  voltDeltakHz  Voltage delta (uV)
 *
 * @return Adjusted voltage value (uV)
 */
#define ClkVoltDeltaAdjust(fname) LwU32 (fname)(LwU32 voltageuV, LwS32 voltDeltauV)

/*!
 * Helper function to adjust a frequency by the provided delta.
 *
 * @note The frequency units are intentionally mismatched - MHz vs kHz.  The
 * frequency deltas are specified in kHz but the CLK code internally stores
 * frequency in MHz.  This helper functions takes care of doing colwersion kHz
 * -> MHz.  Eventually, CLK code will move to internally using kHz.  At that
 * point, this interface will be reworked.
 *
 * @param[in]  freqMHz       Frequency (MHz) to adjust
 * @param[in]  freqDeltakHz  Frequency delta (kHz)
 *
 * @return Adjusted frequency value (MHz)
 */
#define ClkOffsetVFFreqDeltaAdjust(fname) LwS16 (fname)(LwS16 freqMHz, LW2080_CTRL_CLK_FREQ_DELTA *pFreqDelta)

/*!
 * Helper function to adjust a voltage by the provided delta.
 *
 * @param[in]  voltageuV     Frequency (uV) to adjust
 * @param[in]  voltDeltakHz  Voltage delta (uV)
 *
 * @return Adjusted voltage value (uV)
 */
#define ClkOffsetVFVoltDeltaAdjust(fname) LwS32 (fname)(LwS32 voltageuV, LwS32 voltDeltauV)

/*!
 * Helper function to ilwert the sign of the deltakHz parameter in
 * a LW2080_CTRL_CLK_FREQ_DELTA struct.
 *
 * @param[in] pFreqDeltaIlw LW2080_CTRL_CLK_FREQ_DELTA struct to adjust
 *
 */
#define ClkFreqDeltaIlwert(fname) void (fname)(LW2080_CTRL_CLK_FREQ_DELTA *pFreqDeltaIlw)

/* ------------------------ External Definitions --------------------------- */
extern OBJCLK               Clk;

/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
mockable void clkPreInit(void)
    GCC_ATTRIB_SECTION("imem_init", "clkPreInit");
FLCN_STATUS clkPostInit(void)
    // Called only after init time -> from PostPerf libPerfInit overlay.
    GCC_ATTRIB_SECTION("imem_libPerfInit", "clkPostInit");

FLCN_STATUS clkIlwalidate(LwBool bVFEEvalRequired)
    GCC_ATTRIB_SECTION("imem_perfIlwalidation", "clkIlwalidate");
FLCN_STATUS preClkIlwalidate(LwBool bVFEEvalRequired)
    GCC_ATTRIB_SECTION("imem_perfIlwalidation", "preClkIlwalidate");
FLCN_STATUS postClkIlwalidate(void)
    GCC_ATTRIB_SECTION("imem_perfIlwalidation", "postClkIlwalidate");

// CLK Delta Adjustment Helper Functions
ClkFreqDeltaAdjust (clkFreqDeltaAdjust)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkFreqDeltaAdjust");
ClkVoltDeltaAdjust (clkVoltDeltaAdjust)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkVoltDeltaAdjust");
ClkOffsetVFFreqDeltaAdjust (clkOffsetVFFreqDeltaAdjust)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkOffsetVFFreqDeltaAdjust");
ClkOffsetVFVoltDeltaAdjust (clkOffsetVFVoltDeltaAdjust)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkOffsetVFVoltDeltaAdjust");
ClkFreqDeltaIlwert (clkFreqDeltaIlwert)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkFreqDeltaIlwert");

#endif // G_PMU_OBJCLK_H
#endif // PMU_OBJCLK_H

