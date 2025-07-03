/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pstate_35_infra.h
 * @brief   Additional declarations, type definitions, and macros related to
 *          the PSTATE_35 SW module infrastructure.
 */

#ifndef PSTATE_35_INFRA_H
#define PSTATE_35_INFRA_H

/* ------------------------ System Includes -------------------------------- */
/* ------------------------ Application Includes --------------------------- */
#include "perf/35/pstate_35.h"

/* ------------------------ Forward Definitions ---------------------------- */
/* ------------------------ Macros and Defines ----------------------------- */
/* ------------------------ Interface Definitions -------------------------- */
/*!
 * @brief       Gets a pointer to the P-state's frequency typle for a given
 *              clock domain
 *
 * Similar to @ref PerfPstateClkFreqGet() except that it returns a const
 * pointer to the tuple instead of a copy of it.
 *
 * @param[in]   pPstate             PSTATE_35 object pointer
 * @param[in]   clkDomainIdx        Clock domain index
 * @param[out]  ppcPstateClkEntry   A const pointer to the tuple requested
 *
 * @return      FLCN_OK upon success
 * @return      FLCN_ERR_ILWALID_ARGUMENT if any of the arguments invalid
 */
#define PerfPstate35ClkFreqGetRef(fname) FLCN_STATUS (fname)(PSTATE_35 *pPstate35, LwU8 clkDomainIdx, const PERF_PSTATE_35_CLOCK_ENTRY **ppcPstateClkEntry)

/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ PSTATE Interfaces ------------------------------- */
PerfPstateCache             (perfPstateCache_35)
    GCC_ATTRIB_SECTION("imem_perfIlwalidation", "perfPstateCache_35");

/* ------------------------ PSTATE_35 Interfaces ---------------------------- */
PerfPstate35ClkFreqGetRef   (perfPstate35ClkFreqGetRef)
    GCC_ATTRIB_SECTION("imem_perfVf", "perfPstate35ClkFreqGetRef");

/* ------------------------ PSTATE_35 Helpers ------------------------------- */
FLCN_STATUS perfPstate35SanityCheckOrig(PSTATE_35 *pPstate35)
    GCC_ATTRIB_SECTION("imem_libPstateBoardObj", "perfPstate35SanityCheckOrig");
FLCN_STATUS perfPstate35AdjFreqTupleByFactoryOCOffset(PSTATE_35 *pPstate35)
    GCC_ATTRIB_SECTION("imem_libPstateBoardObj", "perfPstate35AdjFreqTupleByFactoryOCOffset");

/* ------------------------ Include Derived Types --------------------------- */
/* ------------------------ End of File ------------------------------------- */
#endif // PSTATE_35_INFRA_H
