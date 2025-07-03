/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  pwrpolicy_workload_multirail_iface.h
 * @brief @copydoc pwrpolicy_workload_multirail_iface.c
 */

#ifndef PWRPOLICY_WORKLOAD_MULTIRAIL_IFACE_H
#define PWRPOLICY_WORKLOAD_MULTIRAIL_IFACE_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "pmgr/pwrequation.h"
#include "pmgr/pwrpolicy_workload_shared.h"
#include "pmgr/3x/pwrpolicy_3x.h"
#include "volt/voltrail.h"

/* ------------------------- Types Definitions ----------------------------- */
typedef struct PWR_POLICY_WORKLOAD_MULTIRAIL_INTERFACE PWR_POLICY_WORKLOAD_MULTIRAIL_INTERFACE;

/*!
 * Strulwture representing the moving average filter.  Holds the last N samples
 * and the metadata required to callwlate the output/filtered value.
 */
typedef struct
{
    /*!
     * Index of the last value inserted into the @ref pEntries array.
     */
    LwU8         idx;
    /*!
     * Current size of the moving average filter. This is the
     * number of samples to use as input for the filter.
     */
    LwU8         sizeLwrr;
    /*!
     * The moving average window size of the PG residency data.
     */
    LwU8         bufferSize;
    /*!
     * Pointer to cirlwlar buffer of the latest N samples to which the
     * filter will be applied.
     */
    LwUFXP20_12 *pEntries;
} PWR_POLICY_WORKLOAD_MULTIRAIL_MOVING_AVG_FILTER;

/*!
 * Structure representing a per rail workload-based power policy parameters.
 */
typedef struct
{
    /*!
     * @copydoc LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_MULTIRAIL_VOLT_RAIL
     */
    LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_MULTIRAIL_VOLT_RAIL
        rail;
    /*!
     * The current workload/active capacitance (w) callwlated by @ref
     * s_pwrPolicyWorkloadComputeWorkload().  This value is unfiltered.
     */
    LwUFXP20_12 workload;
    /*!
     * Current value retrieved from the monitored PWR_CHANNEL.
     */
    LwU32       valueLwrr;
#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_MULTIRAIL_WORKLOAD_SENSED_VOLTAGE)
    /*!
     * @copydoc VOLT_RAIL_SENSED_VOLTAGE_DATA.
     */
    VOLT_RAIL_SENSED_VOLTAGE_DATA sensed;
#endif //  PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_MULTIRAIL_WORKLOAD_SENSED_VOLTAGE)
} PWR_POLICY_WORKLOAD_MULTIRAIL_VOLT_RAIL_IFACE;

/*!
 * Structure representing a workload-based power policy.
 */
struct PWR_POLICY_WORKLOAD_MULTIRAIL_INTERFACE
{
    /*!
     * @copydoc BOARDOBJ_INTERFACE
     */
    BOARDOBJ_INTERFACE      super;
    /*!
     * Boolean indicating if MSCG Residency feature is enabled or not.
     */
    LwBool                  bMscgResidencyEnabled;
    /*!
     * Boolean indicating if PG Residency feature is enabled or not.
     */
    LwBool                  bPgResidencyEnabled;
    /*!
     * Boolean indicating if sensed voltage is to be used instead of set
     * voltage which is used by default.
     */
    LwBool                  bSensedVoltage;
    /*!
     * Voltage policy table index required for applying voltage delta.
     */
    LwU8                    voltPolicyIdx;
    /*!
     * Mask of clock domains that CWC will consider for obtaining the
     * voltage floor value from the PERF. Basically this is the MASK
     * excluding the GPC clock and its secondary clock
     */
    LwU32                   nonGpcClkDomainMask;
    /*!
     * Moving average filter for PG residency values.
     */
    PWR_POLICY_WORKLOAD_MULTIRAIL_MOVING_AVG_FILTER pgResFilter;
    /*!
     * @copydoc LW2080_CTRL_PMGR_PWR_POLICY_STATUS_DATA_WORKLOAD_MULTIRAIL_WORK_INPUT
     */
    LW2080_CTRL_PMGR_PWR_POLICY_STATUS_DATA_WORKLOAD_MULTIRAIL_WORK_INPUT
        work;
    /*!
     * @copydoc LW2080_CTRL_PMGR_PWR_POLICY_STATUS_DATA_WORKLOAD_MULTIRAIL_FREQ_INPUT
     */
    LW2080_CTRL_PMGR_PWR_POLICY_STATUS_DATA_WORKLOAD_MULTIRAIL_FREQ_INPUT
        freq;
    /*!
     * @copydoc PWR_POLICY_WORKLOAD_MULTIRAIL_VOLT_RAIL_IFACE
     */
    PWR_POLICY_WORKLOAD_MULTIRAIL_VOLT_RAIL_IFACE
        rail[LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_MULTIRAIL_VOLT_RAIL_IDX_MAX];
};

/* ------------------------- Defines --------------------------------------- */
/*!
 * @brief   Macros allowing simple iteration over valid volt rails in
 * PWR_POLICY_WORKLOAD_MULTIRAIL structure.
 *
 * @param[in]       pWorkIface   PWR_POLICY_WORKLOAD_MULTIRAIL_INTERFACE pointer
 * @param[in,out]   idx          volt rail index into the ref@ pWorkIface
 *
 */
#define FOR_EACH_VALID_VOLT_RAIL_IFACE(pWorkIface, idx)                     \
{                                                                               \
    for (idx = 0;                                                               \
        idx < LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_MULTIRAIL_VOLT_RAIL_IDX_MAX; \
        idx++)                                                                  \
    {                                                                           \
        if (pWorkIface->rail[idx].rail.voltRailIdx == LW_U8_MAX)            \
        {                                                                       \
            continue;                                                           \
        }
#define FOR_EACH_VALID_VOLT_RAIL_IFACE_END                                      \
    }                                                                           \
}

/*!
 * @brief   Macro to access the VOLT_RAIL_SENSED_VOLTAGE_DATA structure
 *
 * @param[in]  pWorkIface   PWR_POLICY_WORKLOAD_MULTIRAIL_INTERFACE pointer
 * @param[in]  railIdx      Index of the rail for which to get the pointer
 * @param[out] pSensed      VOLT_RAIL_SENSED_VOLTAGE_DATA pointer
 *
 */
#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_MULTIRAIL_WORKLOAD_SENSED_VOLTAGE)
#define PWR_POLICY_WORKLOAD_MULTIRAIL_VOLT_RAIL_IFACE_SENSED_GET(pWorkIface, railIdx)\
    &(pWorkIface->rail[railIdx].sensed)
#else
#define PWR_POLICY_WORKLOAD_MULTIRAIL_VOLT_RAIL_IFACE_SENSED_GET(pWorkIface, railIdx)\
    NULL
#endif //  PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_MULTIRAIL_WORKLOAD_SENSED_VOLTAGE)

/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
/*!
 * BOARDOBJ_INTERFACE interfaces
 */
BoardObjInterfaceConstruct (pwrPolicyConstructImpl_WORKLOAD_MULTIRAIL_INTERFACE)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicyConstruct", "pwrPolicyConstructImpl_WORKLOAD_MULTIRAIL_INTERFACE");

/*!
 * PWR_POLICY_WORKLOAD_MULTIRAIL_INTERFACE interfaces
 */
FLCN_STATUS pwrPolicyLoad_WORKLOAD_MULTIRAIL_INTERFACE(PWR_POLICY_WORKLOAD_MULTIRAIL_INTERFACE *pWorkIface)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "pwrPolicyLoad_WORKLOAD_MULTIRAIL_INTERFACE");
FLCN_STATUS pwrPolicyGetStatus_WORKLOAD_MULTIRAIL_INTERFACE(PWR_POLICY_WORKLOAD_MULTIRAIL_INTERFACE *pWorkIface, RM_PMU_PMGR_PWR_POLICY_WORKLOAD_MULTIRAIL_INTERFACE_BOARDOBJ_GET_STATUS *pGetStatus)
    GCC_ATTRIB_SECTION("imem_pmgrLibQuery", "pwrPolicyQuery_WORKLOAD_MULTIRAIL_INTERFACE");

LwU32       pwrPolicy3XChannelMaskGet_WORKLOAD_MULTIRAIL_INTERFACE(PWR_POLICY_WORKLOAD_MULTIRAIL_INTERFACE *pWorkIface)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPolicy3XChannelMaskGet_WORKLOAD_MULTIRAIL_INTERFACE");
FLCN_STATUS pwrPolicy3XFilter_WORKLOAD_MULTIRAIL_INTERFACE(PWR_POLICY_WORKLOAD_MULTIRAIL_INTERFACE *pWorkIface, PWR_POLICY_3X_FILTER_PAYLOAD_TYPE *pPayload)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPolicy3XFilter_WORKLOAD_MULTIRAIL_INTERFACE");

LwUFXP20_12 pwrPolicyWorkloadMultiRailComputeWorkload(PWR_POLICY_WORKLOAD_MULTIRAIL_INTERFACE *pWorkload, LW2080_CTRL_PMGR_PWR_POLICY_STATUS_DATA_WORKLOAD_MULTIRAIL_WORK_INPUT *pInput, LwU8 railIdx, LwU8 limitUnit)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicyWorkloadMultirail", "pwrPolicyWorkloadMultiRailComputeWorkload");
FLCN_STATUS pwrPolicyWorkloadMultiRailGetVoltageFloor(PWR_POLICY_WORKLOAD_MULTIRAIL_INTERFACE *pWorkload, LwU32 mclkkHz)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicyWorkloadMultirail", "pwrPolicyWorkloadMultiRailGetVoltageFloor");
void        pwrPolicyWorkloadMultiRailEvaluateTotalPowerAtFreq(PWR_POLICY_WORKLOAD_MULTIRAIL_INTERFACE *pWorkload, CLK_DOMAIN *pDomain, LwU32 freqMHz, LwU32* pTotalPwr, LwU8 limitUnit)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicyWorkloadMultirail", "pwrPolicyWorkloadMultiRailEvaluateTotalPowerAtFreq");
FLCN_STATUS pwrPolicyWorkloadMultiRailInputStateGetCommonParams(PWR_POLICY_WORKLOAD_MULTIRAIL_INTERFACE *pWorkIface, LW2080_CTRL_PMGR_PWR_POLICY_STATUS_DATA_WORKLOAD_MULTIRAIL_WORK_INPUT *pInput, LwU8 limitUnit)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicyWorkloadMultirail", "pwrPolicyWorkloadMultiRailInputStateGetCommonParams");
FLCN_STATUS pwrPolicyWorkloadMultiRailMovingAvgFilterInsert(PWR_POLICY_WORKLOAD_MULTIRAIL_INTERFACE *pWorkIface, PWR_POLICY_WORKLOAD_MULTIRAIL_MOVING_AVG_FILTER *pMovingAvg, LwUFXP20_12)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicyWorkloadMultirail", "pwrPolicyWorkloadMultiRailMovingAvgFilterInsert");
LwUFXP20_12 pwrPolicyWorkloadMultiRailMovingAvgFilterQuery(PWR_POLICY_WORKLOAD_MULTIRAIL_INTERFACE *pWorkIface, PWR_POLICY_WORKLOAD_MULTIRAIL_MOVING_AVG_FILTER *pMovingAvg)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicyWorkloadMultirail", "pwrPolicyWorkloadMultiRailMovingAvgFilterQuery");
/* ------------------------- Debug Macros ---------------------------------- */

#endif // PWRPOLICY_WORKLOAD_MULTIRAIL_IFACE_H
