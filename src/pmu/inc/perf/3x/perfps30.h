/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PERFPS30_H
#define PERFPS30_H

/*!
 * @file    perfps30.h
 * @copydoc perfps30.c
 */

/* ------------------------ System Includes -------------------------------- */
/* ------------------------ Application Includes --------------------------- */
#include "boardobj/boardobjgrp.h"
#include "perf/pstate.h"
#include "lib/lib_perf.h"

/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Macros ----------------------------------------- */
#define perfStatusGetClkFreqkHz(clkIdx)                                       \
    Perf.status.pClkDomains[clkIdx].clkFreqKHz

/* ------------------------ Datatypes -------------------------------------- */
/*!
 * PERF PSTATES status info.
 * The latest PERF state received from the RM.  This represents the current
 * PERF-related state of the GPU.
 */
typedef struct
{
    /*!
     * The current GPU Pstate.
     */
    LwU8        pstateLwrrIdx;

    /*!
     * copydoc@ RM_PMU_PERF_PSTATE_CLK_DOMAIN_STATUS
     */
    RM_PMU_PERF_PSTATE_CLK_DOMAIN_STATUS   *pClkDomains;
} PERF_PSTATE_STATUS;

/* ------------------------ Function Prototypes ---------------------------- */

FLCN_STATUS perfProcessPerfStatus_3X(RM_PMU_PERF_PSTATE_STATUS *pStatus)
    GCC_ATTRIB_SECTION("imem_perfVf", "perfProcessPerfStatus_3X");
FLCN_STATUS perfClkDomainsBoardObjGrpSet_30(BOARDOBJGRP *pDomains)
    GCC_ATTRIB_SECTION("imem_perf", "perfClkDomainsBoardObjGrpSet_30");

#endif // PERFPS30_H
