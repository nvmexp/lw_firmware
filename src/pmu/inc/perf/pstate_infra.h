/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pstate_infra.h
 * @brief   Additional declarations, type definitions, and macros related to
 *          the PSTATES SW module infrastructure.
 */

#ifndef PSTATE_INFRA_H
#define PSTATE_INFRA_H

/* ------------------------ Includes --------------------------------------- */
#include "perf/pstate.h"
#include "lib/lib_perf.h"

/* ------------------------ Forward Definitions ---------------------------- */
/* ------------------------ Macros ----------------------------------------- */
/*!
 * @brief       Helper macro to get PSTATE index by PSTATE Level.
 *
 * @param[in]   _pstateLevel the level for a given pstate.
 *
 * @return     The BOARDOBJGRP index for the corresponding PSTATE.
 *
 * @memberof    PSTATES
 *
 * @public
 */
#define PSTATE_GET_INDEX_BY_LEVEL(_pstateLevel)                                  \
    (((perfPstateGetByLevel(_pstateLevel)) != NULL) ?                            \
     (BOARDOBJ_GET_GRP_IDX_8BIT(&(perfPstateGetByLevel(_pstateLevel))->super)) : \
        LW2080_CTRL_PERF_PSTATE_INDEX_ILWALID)

/*!
 * @copydoc     PerfPstateGetByLevel()
 *
 * @memberof    PSTATES
 *
 * @public
 */
#define PSTATE_GET_BY_LEVEL(_pstateLevel)   \
    (perfPstateGetByLevel(_pstateLevel))

/* ------------------------ Datatypes -------------------------------------- */
/* ------------------------ Interface Definitions -------------------------- */
/*!
 * @brief       Get the number of PSTATEs.
 *
 * @return      The number of valid PSTATEs. PSTATEs are identified by index
 *              and can range from zero to the pstate count (exclusive).
 */
#define PerfGetPstateCount(fname) LwU8 (fname)(void)

/*!
 * @brief       Pack the MAX PSTATE and GPC2CLK value in the
 *              @ref RM_PMU_PERF_DOMAIN_GROUP_LIMITS structure.
 *
 * This limit value will be used by the the PWR POLICY DOMAIN GRP to
 * limit the GPC2CLK value to the MAX ceiling value.
 *
 * @param[out]  pDomGrpLimits   Pointer to PERF_DOMAIN_GROUP_LIMITS struct.
 *
 * @return      FLCN_OK if successully packed pDomGrpLimits with DOM GRP limits.
 * @return      FLCN_ERR_ILWALID_INDEX if invalid type or other error from type specific
 *              implemenation.
 */
#define PerfPstatePackPstateCeilingToDomGrpLimits(fname) FLCN_STATUS (fname)(PERF_DOMAIN_GROUP_LIMITS *pDomGrpLimits)

/*!
 * @brief       Helper function to retrieve the PSTATE for a given perf level.
 *
 * @param[in]   level   Perf level to search for.
 *
 * @return      PSTATE pointer for object corresponding to the given level.
 * @return      NULL if there is no PSTATE corresponding to the given level.
 */
#define PerfPstateGetByLevel(fname) PSTATE * (fname)(LwU8 level)

/*!
 * @brief       Helper interface to recompute and cache PSTATE frequency tuple.
 *
 * Updates the LW2080_CTRL_PERF_PSTATE_CLOCK_FREQUENCY::freqkHz
 * and LW2080_CTRL_PERF_PSTATE_CLOCK_FREQUENCY::freqVFMaxkHz
 * frequencies.
 *
 * This is typically called when the vf lwrve is ilwalidated.
 *
 * @param[in]   pPstate     PSTATE object pointer
 *
 * @return      FLCN_OK  If successfully recomputed cache for the provided PSTATE
 * @return      Other    If unexpected error oclwrred.
 */
#define PerfPstateCache(fname) FLCN_STATUS (fname)(PSTATE *pPstate)

/*!
 * @brief       Helper interface to recompute and cache PSTATE frequency tuple
 *              for allPSTATE objects.
 *
 * See @ref PerfPstateCache.
 *
 * @return      FLCN_OK If successfully recomputed cache for all PSTATE objects
 * @return      Other   If unexpected error oclwrred.
 */
#define PerfPstatesCache(fname) FLCN_STATUS (fname)(void)

/*!
 * @brief       Helper interface to ilwalidate pstates.
 *
 * Recompute and cache the offseted PSTATE frequency tuple as well
 * as notify the host driver to update its cache.
 *
 * @param[in]   bTriggerMsg     If LW_TRUE then send event to notify host
 *                              driver of ilwalidation
 *
 * @return  FLCN_OK Successfully ilwalidated the PSTATEs
 * @return  Other   Errors passed through from internal function calls
 */
#define PerfPstatesIlwalidate(fname) FLCN_STATUS (fname)(LwBool bTriggerMsg)

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ PSTATES Interfaces ----------------------------- */
PerfPstatesIlwalidate   (perfPstatesIlwalidate)
    GCC_ATTRIB_SECTION("imem_perfIlwalidation", "perfPstatesIlwalidate");
PerfPstatesCache        (perfPstatesCache)
    GCC_ATTRIB_SECTION("imem_perfIlwalidation", "perfPstatesCache");


/* ------------------------ PSTATE Interfaces ------------------------------ */
PerfPstatePackPstateCeilingToDomGrpLimits   (perfPstatePackPstateCeilingToDomGrpLimits)
    GCC_ATTRIB_SECTION("imem_perfVf", "perfPstatePackPstateCeilingToDomGrpLimits");
PerfGetPstateCount                          (perfGetPstateCount)
    GCC_ATTRIB_SECTION("imem_perfVf", "perfGetPstateCount");
PerfPstateGetByLevel                        (perfPstateGetByLevel)
    GCC_ATTRIB_SECTION("imem_perfVf", "perfPstateGetByLevel");
PerfPstateCache                             (perfPstateCache)
    GCC_ATTRIB_SECTION("imem_perfIlwalidation", "perfPstateCache");

/* ------------------------ PSTATE Helpers --------------------------------- */
LwU8 perfPstateGetLevelByIndex(LwU8 pstateIdx)
    GCC_ATTRIB_SECTION("imem_perfVf", "perfPstateGetLevelByIndex");

/* ------------------------ Include Derived Types -------------------------- */
/* ------------------------ End of File ------------------------------------ */
#endif // PSTATE_INFRA_H
