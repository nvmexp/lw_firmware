/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  pwrpolicy_workload_shared.h
 * @brief @copydoc pwrpolicy_workload_shared.c
 */

#ifndef PWRPOLICY_WORKLOAD_SHARED_H
#define PWRPOLICY_WORKLOAD_SHARED_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "pmgr/pwrpolicy_domgrp.h"

/* ------------------------- Types Definitions ----------------------------- */


/*!
 * Structure representing a set of values to apply as input to the workload
 * median filter @ref PWR_POLICY_WORKLOAD_MEDIAN_FILTER.
 */
typedef struct
{
    /*!
     * The workload term callwlated from the observed power, clocks, and
     * voltage, and leakage for a given iteration of power monitoring.  This is
     * the input value which will be applied to the median filter.
     */
    LwUFXP20_12 workload;
    /*!
     * The observed input value (lwrrently expected to be power) corresponding
     * to @ref workloadmWperMHzmV2.  This value is recorded so that
     * PWR_POLICY_WORKLOAD can later retrieve it when queried via @ref
     * PwrPolicyRelationshipValueLwrrGet() interface.  These two values must
     * stay in sync or can see unstable behavior from high/low limit
     * callwlations which depend on this power value being in sync with workload
     * value used to callwlate the maximum clock values in @ref
     * s_pwrPolicyWorkloadComputeClkMHz().
     */
    LwU32       value;
} PWR_POLICY_WORKLOAD_MEDIAN_FILTER_ENTRY;

/*!
 * Strulwture representing the workload median filter.  Holds the last N samples
 * and the metadata required to callwlate the output/filtered value.
 */
typedef struct
{
    /*!
     * Index of the last value inserted into the @ref pValues array.
     */
    LwU8        idx;
    /*!
     * Current size of the median filter - i.e. the number of values which have
     * been placed into the filter.  This is used for tracking cases starting
     * the filter, when have taken < @ref sizeTotal samples.  Will always be <=
     * @ref sizeTotal.
     */
    LwU8        sizeLwrr;
    /*!
     * Total/max size of the workload median filter.  This is the number of
     * samples to use as input for the filter.
     */
    LwU8        sizeTotal;
    /*!
     * Cache of the current output values of the filter.
     */
    PWR_POLICY_WORKLOAD_MEDIAN_FILTER_ENTRY  entryLwrrOutput;
    /*!
     * Pointer to cirlwlar buffer of the latest N workload samples to which the
     * filter will be applied.
     */
    PWR_POLICY_WORKLOAD_MEDIAN_FILTER_ENTRY *pEntries;
    /*!
     * Pointer to scratch buffer which will be used to sort the current set of
     * N input samples.  Reserving this buffer in the object to reduce stack
     * pressure/consumption and to account for fact that can't allocate/free off
     * heap in filtering routine.
     */
    PWR_POLICY_WORKLOAD_MEDIAN_FILTER_ENTRY *pEntriesSort;
} PWR_POLICY_WORKLOAD_MEDIAN_FILTER;

/*!
 * Structure representing time spent in low power.
 */
typedef struct
{
    /*!
     * Previous evaluation time. Record the timestamp when this policy was last
     * evaluated.
     */
    FLCN_TIMESTAMP    lastEvalTime;
    /*!
     * Previous sleep time. Record the total sleep time till last
     * evaluation. Units in nano second.
     */
    FLCN_TIMESTAMP    lastSleepTime;
} PWR_POLICY_WORKLOAD_LPWR_TIME;

/* ------------------------- Defines --------------------------------------- */
/*!
 * Helper macro to return the current output/filtered workload value from the
 * median filter.
 *
 * @param[in] pMedian  PWR_POLICY_WORKLOAD_MEDIAN_FILTER pointer.
 *
 * @return Filtered workload value.
 */
#define PWR_POLICY_WORKLOAD_MEDIAN_FILTER_OUTPUT_WORKLOAD_GET(pMedian)         \
    ((pMedian)->entryLwrrOutput.workload)

/*!
 * Helper macro to return the current output/filtered input value corresponding
 * to the current/output filtered workload value from the median filter.
 *
 * @param[in] pMedian  PWR_POLICY_WORKLOAD_MEDIAN_FILTER pointer.
 *
 * @return Filtered input value.
 */
#define PWR_POLICY_WORKLOAD_MEDIAN_FILTER_OUTPUT_VALUE_GET(pMedian)            \
    ((pMedian)->entryLwrrOutput.value)

/*!
 * Helper macro to colwert a target voltage (specified and tracked within RM/PMU
 * in uV) to mV.
 *
 * @param[in]  voltuV  Target voltage value (uV)
 *
 * @return Voltage value colwerted to mV.
 */
#define PWR_POLICY_WORKLOAD_UV_TO_MV(voltuV)                                   \
    LW_UNSIGNED_ROUNDED_DIV((voltuV), 1000)


/*!
 * Helper macro to colwert a 2CLK (e.g. GPC2CLK vs GPCCLK) clock frequency
 * (specified and tracked within RM/PMU in kHz) to a 1CLK clock frequncy in MHz
 * for the PWR_POLICY_WORKLOAD algorithm.
 *
 * Scaling to MHz (which is really the granularity we use for our clocks), helps
 * provide better resolution in the workload/active capacitance term (w) as
 * dividing by a smaller number.
 *
 * 2kHz / 2000 => MHz
 *
 * @param[in] clk2KHz  2CLK Clock value (kHz) to colwert.
 */
#define PWR_POLICY_WORKLOAD_2KHZ_TO_MHZ(clk2KHz)                               \
    LW_UNSIGNED_ROUNDED_DIV((clk2KHz), 2000)

/*!
 * Helper macro to colwert a 1CLK clock frequncy in MHz to a 2CLK (e.g. GPC2CLK
 * vs GPCCLK) clock frequency (specified and tracked within RM/PMU in kHz).
 *
 * MHz * 2000 => 2kHz
 */
#define PWR_POLICY_WORKLOAD_MHZ_TO_2KHZ(clkMHz)                                \
    ((clkMHz) * 2000)

/*!
 * Helper macro to colwert a 1CLK clock frequncy in MHz to a 1CLK
 * clock frequency (specified and tracked within RM/PMU in kHz).
 *
 * MHz * 1000 => kHz
 */
#define PWR_POLICY_WORKLOAD_MHZ_TO_KHZ(clkMHz)                                \
    ((clkMHz) * 1000)

/*!
 * Helper macro to colwert a clock frequncy in KHz to MHz
 */
#define PWR_POLICY_WORKLOAD_KHZ_TO_MHZ(clkKHz)                                 \
    LW_UNSIGNED_ROUNDED_DIV((clkKHz), 1000)

/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
void        pwrPolicyWorkloadMedianFilterInsert_SHARED(PWR_POLICY_WORKLOAD_MEDIAN_FILTER *pMedian, PWR_POLICY_WORKLOAD_MEDIAN_FILTER_ENTRY *pEntryInput)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPolicyWorkloadMedianFilterInsert_SHARED");

LwUFXP20_12 pwrPolicyWorkloadResidencyCompute(PWR_POLICY_WORKLOAD_LPWR_TIME *pLpwrTime, LwU8 engine)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicyWorkloadShared", "pwrPolicyWorkloadResidencyCompute");
/* ------------------------- Debug Macros ---------------------------------- */

#endif // PWRPOLICY_WORKLOAD_SHARED_H
