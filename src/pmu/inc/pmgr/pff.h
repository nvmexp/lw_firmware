/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  pff.h
 * @brief @copydoc pff.c
 */

#ifndef PFF_H
#define PFF_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
/* ------------------------- Types Definitions ----------------------------- */
/*!
 * Structure simply grouping PFF control and status data together. Any policies
 * implementing a PFF should contain this structure.
 */
typedef struct
{
    /*!
     * Control information about the PFF, such as the lwrve and flags dictating
     * its features.
     */
    LW2080_CTRL_PMGR_PFF_PMU_CONTROL    control;

    /*!
     * Status information about the PFF, this includes its last evaluated
     * intersection.
     */
    LW2080_CTRL_PMGR_PFF_STATUS         status;
} PIECEWISE_FREQUENCY_FLOOR;

/*!
 * @interface PIECEWISE_FREQUENCY_FLOOR
 *
 * To evaluate the PFF at the given domainInput. pPff->status should be updated
 * to reflect any changes to the frequency floor for the PFF.
 *
 * @param[in/out]   pPff         PFF pointer
 * @param[in]       domainInput  x-value which may intersect the pff lwrve
 * @param[in]       domainMin    defines the minimum valid value in the domain (usually limitLwrr for a policy)
 */
#define PffEvaluate(fname) FLCN_STATUS (fname)(PIECEWISE_FREQUENCY_FLOOR *pPff, LwU32 domainInput, LwU32 domainMin)

/*!
 * @interface PIECEWISE_FREQUENCY_FLOOR
 *
 * To sanity check the PFF's parameters
 *
 * @param[in/out]   pPff         PFF pointer
 * @param[in]       domainMin    defines the minimum valid value in the domain (usually limitLwrr for a policy)
 */
#define PffSanityCheck(fname) FLCN_STATUS (fname)(PIECEWISE_FREQUENCY_FLOOR *pPff, LwU32 domainMin)

/*!
 * @interface PIECEWISE_FREQUENCY_FLOOR
 *
 * Helper funcion to simply initialize PIECEWISE_FREQUENCY_FLOOR::status with the base
 * PFF values from PIECEWISE_FREQUENCY_FLOOR::control and then apply any OC from the
 * VF lwrve proportional to the tuple ocRatios
 *
 * @param[in/out]   pPff         PFF pointer
 * @param[in]       domainInput  x-value which may intersect the pff lwrve
 * @param[in]       domainMin    defines the minimum valid value in the domain (usually limitLwrr for a policy)
 */
#define PffIlwalidate(fname) FLCN_STATUS (fname)(PIECEWISE_FREQUENCY_FLOOR *pPff, LwU32 domainInput, LwU32 domainMin)

/* ------------------------- Macros and Defines ---------------------------- */
/*!
 * Helper macro to get the last evaluated floor value (interection) of the PFF
 * lwrve at an observed domain input (power, temperature, etc.). It is assumed
 * the time elapsed between evaluation of the PFF and this call is minimal.
 */
#define pffGetCachedFrequencyFloor(pPff)    (pPff->status.lastFloorkHz)

/*!
 * @brief   List of an overlay descriptors required by PIECEWISE_FREQUENCY_FLOOR
 *          during pwr policy domgrp evaluation (DOMGRP).
 *
 * @note    Review the use cases in Tasks.pm and update MAX_OVERLAYS_IMEM after
 *          updates to this macro. Failure to do so might result in falcon halt.
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PIECEWISE_FREQUENCY_FLOOR))
#define OSTASK_OVL_DESC_DEFINE_PWR_POLICY_DOMGRP_EVALUATE_PIECEWISE_FREQUENCY_FLOOR    \
    OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, therm)
#else
#define OSTASK_OVL_DESC_DEFINE_PWR_POLICY_DOMGRP_EVALUATE_PIECEWISE_FREQUENCY_FLOOR    \
    OSTASK_OVL_DESC_ILWALID
#endif

/*!
 * @brief   List of an overlay descriptors required by pffIlwalidate.
 *
 * @note    Review the use cases in Tasks.pm and update MAX_OVERLAYS_IMEM after
 *          updates to this macro. Failure to do so might result in falcon halt.
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PIECEWISE_FREQUENCY_FLOOR))
#define OSTASK_OVL_DESC_DEFINE_PFF_ILWALIDATE           \
    OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perf)         \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, pmgrLibPff)   \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libBoardObj)  \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfVf)
#else
#define OSTASK_OVL_DESC_DEFINE_PFF_ILWALIDATE           \
    OSTASK_OVL_DESC_ILWALID
#endif

/*!
 * @brief   List of an overlay descriptors required by pffEvaluate.
 *
 * @note    Review the use cases in Tasks.pm and update MAX_OVERLAYS_IMEM after
 *          updates to this macro. Failure to do so might result in falcon halt.
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PIECEWISE_FREQUENCY_FLOOR))
#define OSTASK_OVL_DESC_DEFINE_PFF_EVALUATE             \
    OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perf)         \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, pmgrLibPff)   \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libBoardObj)  \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfVf)
#else
#define OSTASK_OVL_DESC_DEFINE_PFF_EVALUATE             \
    OSTASK_OVL_DESC_ILWALID
#endif

/*!
 * @brief   List of an overlay descriptors required by PIECEWISE_FREQUENCY_FLOOR
 *          during pwr policy Limit evaluation (TOTAL_GPU).
 *
 * @note    Review the use cases in Tasks.pm and update MAX_OVERLAYS_IMEM after
 *          updates to this macro. Failure to do so might result in falcon halt.
 *
 */
#define OSTASK_OVL_DESC_DEFINE_PWR_POLICY_LIMIT_EVALUATE_PIECEWISE_FREQUENCY_FLOOR  \
    OSTASK_OVL_DESC_DEFINE_PFF_EVALUATE

/*!
 * @brief   List of an overlay descriptors required by PIECEWISE_FREQUENCY_FLOOR
 *          during pwrPoliciesVfIlwalidate.
 *
 * @note    Review the use cases in Tasks.pm and update MAX_OVERLAYS_IMEM after
 *          updates to this macro. Failure to do so might result in falcon halt.
 */
#define OSTASK_OVL_DESC_DEFINE_PWR_POLICIES_VF_ILWALIDATE           \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, pmgrLibPwrPolicy)         \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, pmgrLibPwrPolicyClient)   \
    OSTASK_OVL_DESC_DEFINE_PFF_ILWALIDATE

/*!
 * @brief   List of an overlay descriptors required by PIECEWISE_FREQUENCY_FLOOR
 *          during thermPolicyCallbackExelwte (THERM_POLICY).
 *
 * @note    Review the use cases in Tasks.pm and update MAX_OVERLAYS_IMEM after
 *          updates to this macro. Failure to do so might result in falcon halt.
 */
#define OSTASK_OVL_DESC_DEFINE_THERM_POLICY_CALLBACK_EXELWTE_PIECEWISE_FREQUENCY_FLOOR  \
    OSTASK_OVL_DESC_DEFINE_PFF_EVALUATE

/* ------------------------- Function Prototypes --------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
PffEvaluate     (pffEvaluate)
    GCC_ATTRIB_SECTION("imem_pmgrLibPff", "pffEvaluate");
PffSanityCheck  (pffSanityCheck)
    GCC_ATTRIB_SECTION("imem_pmgrLibPff", "pffSanityCheck");
PffIlwalidate   (pffIlwalidate)
    GCC_ATTRIB_SECTION("imem_pmgrLibPff", "pffIlwalidate");

/* ------------------------- Debug Macros ---------------------------------- */

#endif // PFF_H
