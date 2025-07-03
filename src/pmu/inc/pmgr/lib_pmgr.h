/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    lib_pmgr.h
 * @copydoc lib_pmgr.c
 */

#ifndef LIB_PMGR_H
#define LIB_PMGR_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
/* ------------------------------ Macros ------------------------------------*/
/* ------------------------- Types Definitions ----------------------------- */
/*!
 * Shared violation structure that will contain the information about the
 * violation observed on the given RM_PMU_THERM_EVENT_<xyz>
 */
typedef struct
{
    /*!
     * Target violation rate to maintain (normalized to [0.0,1.0]).
     */
    LwUFXP4_12      violTarget;

    /*!
     * Most recently evaluated violation rate (normalized to [0.0,1.0]).
     */
    LwUFXP4_12      violLwrrent;
    /*!
     * Previously sampled violation time [ns] of @ref thrmIdx.
     */
    FLCN_TIMESTAMP  lastTimeViol;
    /*!
     * Time-stamp of the most recent violation rate evaluation [ns].
     */
    FLCN_TIMESTAMP  lastTimeEval;
} LIB_PMGR_VIOLATION;

/*!
 * Structure to cache lpwr residencies for the caller's evaluation period
 */
typedef struct
{
    /*!
     * Residency of lowpower features
     */
    LwUFXP20_12 lpwrResidency[LW2080_CTRL_MC_POWERGATING_ENGINE_ID_MAX];
} PMGR_LPWR_RESIDENCIES;

/*!
 * @interface LIB_PMGR
 *
 * Interface that can be used to evaluate the current violation rate for the
 * given evaluation period.
 *
 * @param[in/out]   pViolation   LIB_PMGR_VIOLATION object pointer.
 * @param[in]       thrmIdx      Index of therm event or monitor.
 *
 * @return          violLwrr     Evaluated violation rate in FXP4.12.
 */
#define LibPmgrViolationLwrrGet(fname) LwUFXP4_12 (fname)(LIB_PMGR_VIOLATION *pViolation, LwU8 thrmIdx)

/* ------------------------- Macros ---------------------------------------- */
/* ------------------------- Function Prototypes --------------------------- */
LibPmgrViolationLwrrGet (libPmgrViolationLwrrGetEvent)
    GCC_ATTRIB_SECTION("imem_pmgr", "libPmgrViolationLwrrGetEvent");
LibPmgrViolationLwrrGet (libPmgrViolationLwrrGetMon)
    GCC_ATTRIB_SECTION("imem_pmgr", "libPmgrViolationLwrrGetMon");

/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
/* ------------------------- Debug Macros ---------------------------------- */
/* ------------------------- Child Class Includes -------------------------- */

#endif // LIB_PMGR_H
