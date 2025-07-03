/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  pmgrtach.h
 * @brief PGMR TACH interface.
 */

#ifndef PMGRTACH_H
#define PMGRTACH_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
/* ------------------------- Types Definitions ----------------------------- */

/*!
 *  Available TACH sources:
 *
 *  PMU_PMGR_TACH_SOURCE_ILWALID
 *      Default return value in all error cases (when TACH source expected).
 *
 *  PMU_PMGR_PMGR_TACH_SOURCE
 *      The fan tach input is selected from LW_PMGR_GPIO_INPUT_CNTL_24 in the
 *      PMGR and the tach is physically located in PMGR.
 *
 *  PMU_PMGR_THERM_TACH_SOURCE_0
 *      The fan tach input is selected from LW_PMGR_GPIO_INPUT_CNTL_24 in the
 *      PMGR and the tach is physically located in THERM.
 *
 *  PMU_PMGR_THERM_TACH_SOURCE_1
 *      The fan tach input is selected from LW_PMGR_GPIO_INPUT_CNTL_12 in the
 *      PMGR and the tach is physically located in THERM.
 *
 * @note    Always keep PMU_PMGR_THERM_TACH_SOURCE_0 and PMU_PMGR_THERM_TACH_SOURCE_1
 *          conselwtive to each other and in same order.
 *
 *  PMU_PMGR_TACH_SOURCE__COUNT
 *      Total number of TACH source types. Must always be last.
 */
typedef enum
{
    PMU_PMGR_TACH_SOURCE_ILWALID = 0,
    PMU_PMGR_PMGR_TACH_SOURCE,
    PMU_PMGR_THERM_TACH_SOURCE_0,
    PMU_PMGR_THERM_TACH_SOURCE_1,

    // Add new sources above this.
    PMU_PMGR_TACH_SOURCE__COUNT,
}   PMU_PMGR_TACH_SOURCE;

/* ------------------------- Defines --------------------------------------- */
/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
/* ------------------------- Debug Macros ---------------------------------- */
/* ------------------------- Child Class Includes -------------------------- */

#endif // PMGRTACH_H
