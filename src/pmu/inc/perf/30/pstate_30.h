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
 * @file    pstate_30.h
 * @brief   Declarations, type definitions, and macros for the PSTATE_30 SW module.
 */

#ifndef PSTATE_30_H
#define PSTATE_30_H

/* ------------------------ System Includes -------------------------------- */
/* ------------------------ Application Includes --------------------------- */
#include "perf/3x/pstate_3x.h"

/* ------------------------ Forward Definitions ---------------------------- */
typedef struct PSTATE_30 PSTATE_30;

/* ------------------------ Macros and Defines ----------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Types Definitions ------------------------------ */
/*!
 * @brief PSTATE_30 represents a single PSTATE entry in PSTATE 3.0.
 *
 * @extends PSTATE_3X
 */
struct PSTATE_30
{
    /*!
     * @brief PSTATE_3X parent class.
     *
     * Must be first element of the structure!
     *
     * @protected
     */
    PSTATE_3X super;

    /*!
     * @brief Array of clock entries; one entry per @ref CLK_DOMAIN.
     *
     * See @ref LW2080_CTRL_PERF_PSTATE_CLK_DOM_INFO_DECOUPLED.
     *
     * Bounded in size by Perf.pstates.numClkDomains.
     *
     * @private
     */
    LW2080_CTRL_PERF_PSTATE_30_CLOCK_ENTRY *pClkEntries;
};

/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ BOARDOBJ Interfaces ---------------------------- */
BoardObjGrpIfaceModel10ObjSet   (perfPstateGrpIfaceModel10ObjSet_30)
    GCC_ATTRIB_SECTION("imem_libPstateBoardObj", "perfPstateGrpIfaceModel10ObjSet_30");

/* ------------------------ PSTATE Interfaces ------------------------------ */
PerfPstateClkFreqGet    (perfPstateClkFreqGet_30)
    GCC_ATTRIB_SECTION("imem_perfVf", "perfPstateClkFreqGet_30");

/* ------------------------ Include Derived Types -------------------------- */
/* ------------------------ End of File ------------------------------------ */
#endif // PSTATE_30_H
