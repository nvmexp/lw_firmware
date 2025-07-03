/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2012-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_CLKCNTR_H
#define PMU_CLKCNTR_H

/*!
 * @file pmu_clkcntr.h
 *
 * @copydoc pmu_clkcntr.c
 */

/* ------------------------ System Includes --------------------------------- */
#include "main.h"

/* ------------------------ Application Includes ---------------------------- */
#include "pmu_oslayer.h"
#include "pmu_objtimer.h"
#include "clk_freq_effective_avg.h"

/* ------------------------ Defines ----------------------------------------- */
/*!
 * @brief   List of an overlay descriptors required by a clock counter code.
 *
 * @note    Review the use cases in Tasks.pm and update MAX_OVERLAYS_IMEM after
 *          updates to this macro. Failure to do so might result in falcon halt.
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CNTR_64BIT_LIBS))
#define OSTASK_OVL_DESC_DEFINE_CLK_COUNTER                                     \
    OSTASK_OVL_DESC_DEFINE_LIB_LW64                                            \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, clkLibCntr)                          \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, clkLibCntrSwSync)
#else
#define OSTASK_OVL_DESC_DEFINE_CLK_COUNTER                                     \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, clkLibCntr)                          \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, clkLibCntrSwSync)
#endif

/*!
 * Helper macro to get the average measured frequency in KHz
 */
#if PMUCFG_FEATURE_ENABLED(PMU_CLK_CNTR_64BIT_LIBS)
#define clkCntrAvgFreqKHz(clkDomain, partIdx, pSample)                         \
    clkCntrAvgFreqKHz64Bit(clkDomain, partIdx, pSample)
#else
#define clkCntrAvgFreqKHz(clkDomain, partIdx, pSample)                         \
    clkCntrAvgFreqKHz32Bit(clkDomain, partIdx, pSample)
#endif

/*!
 * Enum to represent Clock counter access status
 */
enum
{
    CLK_CNTR_ACCESS_DISABLED = 0x0,
    CLK_CNTR_ACCESS_ENABLED_TRIGGER_CALLBACK,
    CLK_CNTR_ACCESS_ENABLED_SKIP_CALLBACK,
};

/*!
 * Helper macro to check whether clock counter access is enabled
 */
#define CLK_CNTR_IS_HW_ACCESS_ENABLED(pClkCntr)                               \
    (((pClkCntr) != NULL) ?                                                   \
     ((pClkCntr)->accessDisableClientsMask == 0U) :                           \
     LW_FALSE)

/* ------------------------ Types Definitions ------------------------------- */
/*!
 * Structure describing a clock counter state.
 */
typedef struct
{
    /*!
     * Clock domain @ref LW2080_CTRL_CLK_DOMAIN_<xyz>.
     */
    LwU32   clkDomain;

    /*!
     * Partition index associated with a clock domain
     * @ref LW2080_CTRL_CLK_DOMAIN_PART_IDX_<xyz>.
     */
    LwU8    partIdx;

    /*!
     * Mask of client IDs who have disabled access to the clock counter.
     * When the mask is non-zero, HW counter register access is disabled
     * and the cached counter value will be used instead.
     * @ref LW2080_CTRL_CLK_CNTR_CLIENT_ID_<xyz>
     */
    LwU8    accessDisableClientsMask;

    /*!
     * Scaling factor by which counter value should be scaled.
     */
    LwU8    scalingFactor;

    /*!
     * Clock counter source which needs to be measured.
     */
    LwU8    cntrSource;

    /*!
     * Address of the CFG register. This register should be accessed via
     * FECS bus to be efficient.
     */
    LwU32   cfgReg;

    /*!
     * Last HW counter value read, used to compute difference and update SW
     * value.
     */
    LwU64   hwCntrLast;

    /*!
     * SW clock counter value.
     */
    LwU64   swCntr;

#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_EFFECTIVE_AVG))
    /*!
     * SW state for effective average frequency.
     */
    CLK_FREQ_EFFECTIVE_AVG  freqEffAvg;
#endif // (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_EFFECTIVE_AVG))
} CLK_CNTR;

/*!
 * Structure defining state for the clock counter (@ref PMU_CLK_CNTR) feature.
 */
typedef struct
{
    /*!
     * Number of bits in the HW counter register.
     */
    LwU8        hwCntrSize;

    /*!
     * Number of clock counters.
     */
    LwU8        numCntrs;

    /*!
     * Pointer to the statically allocated array of clock counter
     * state.
     */
    CLK_CNTR   *pCntr;
} CLK_CNTRS;
/*!
 * Use new LW2080_CTRL_CLK_CNTR_SAMPLE_ALIGNED structure to prevent 8 byte alignment
 * from using up DMEM. Using this typedef to prevent having to change all references
 * to the old type until a follow-up cleanup CL.
 */
typedef LW2080_CTRL_CLK_CNTR_SAMPLE_ALIGNED CLK_CNTR_AVG_FREQ_START;

/* ------------------------ External Variables ------------------------------ */
/* ------------------------ Static Variables -------------------------------- */
/* ------------------------ Function Prototypes ----------------------------- */
// Clock Counter Interfaces
void clkCntrPreInit(void)
    GCC_ATTRIB_SECTION("imem_init", "clkCntrPreInit");
void  clkCntrSchedPcmCallback(void)
    GCC_ATTRIB_SECTION("imem_init", "clkCntrSchedPcmCallback");
void  clkCntrSchedPerfCallback(void)
    GCC_ATTRIB_SECTION("imem_init", "clkCntrSchedPerfCallback");

CLK_CNTR * clkCntrGet(LwU32 clkDomain, LwU8 partIdx)
    GCC_ATTRIB_SECTION("imem_clkLibCntr", "clkCntrGet");
LwU64 clkCntrRead(LwU32 clkDomain, LwU8 partIdx)
    GCC_ATTRIB_SECTION("imem_clkLibCntr", "clkCntrRead");
FLCN_STATUS clkCntrSample(RM_PMU_CLK_CNTR_SAMPLE_DOMAIN *pSample)
    GCC_ATTRIB_SECTION("imem_clkLibCntr", "clkCntrSample");
void clkCntrSamplePart(LwU32 clkDomain, LwU8 partIdx, LW2080_CTRL_CLK_CNTR_SAMPLE_ALIGNED *pSample)
    GCC_ATTRIB_SECTION("imem_clkLibCntr", "clkCntrSamplePart");
LwU32 clkCntrAvgFreqKHz32Bit(LwU32 clkDomain, LwU8 partIdx, LW2080_CTRL_CLK_CNTR_SAMPLE_ALIGNED *pSample)
    GCC_ATTRIB_SECTION("imem_clkLibCntr", "clkCntrAvgFreqKHz32Bit");
LwU32 clkCntrAvgFreqKHz64Bit(LwU32 clkDomain, LwU8 partIdx, LW2080_CTRL_CLK_CNTR_SAMPLE_ALIGNED *pSample)
    GCC_ATTRIB_SECTION("imem_clkLibCntr", "clkCntrAvgFreqKHz64Bit");
void  clkCntrAccessSync(LwU32 clkDomMask, LwU8 clientId, LwU8 clkCntrAccessState)
    GCC_ATTRIB_SECTION("imem_clkLibCntrSwSync", "clkCntrAccessSync");
FLCN_STATUS clkCntrDomainResetCachedHwLast(LwU32 clkDomain)
    GCC_ATTRIB_SECTION("imem_clkLibCntr", "clkCntrDomainResetCachedHwLast");
FLCN_STATUS clkCntrDomainEnable(LwU32 clkDomain)
    GCC_ATTRIB_SECTION("imem_clkLibCntr", "clkCntrDomainEnable");

#endif // PMU_CLKCNTR_H
