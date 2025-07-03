/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef VPSTATE_H
#define VPSTATE_H

/*!
 * @file vpstate.h
 * @copydoc vpstate.c
 */

/* ------------------------ Includes --------------------------------------- */
#include "boardobj/boardobjgrp.h"
#include "pmu/pmuifperf.h"
#include "boardobj/boardobjgrp_iface_model_10.h"
#include "boardobj/boardobj_iface_model_10.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct VPSTATE VPSTATE, VPSTATE_BASE;

/* ------------------------ Macros ----------------------------------------- */
/*!
 * Macro to locate CLK_VF_POINTS BOARDOBJGRP.
 *
 * @copydoc BOARDOBJGRP_OBJ_GET
 */
#define BOARDOBJGRP_DATA_LOCATION_VPSTATE                                     \
    (&Perf.vpstates.super.super)

/* ------------------------ Datatypes -------------------------------------- */
/*!
 * TO_DO: Pratik
 */
struct VPSTATE
{
    /*!
     * BOARDOBJ super class.  Must always be the first element in the structure.
     */
    BOARDOBJ super;
};

/*!
 * TO_DO - Pratik
 */
typedef struct
{
    /*!
     * BOARDOBJGRP_E255 super class. Must always be the first element in the
     * structure.
     */
    BOARDOBJGRP_E255    super;

    /*!
     * Domain Groups available in this VPstate table. For Pstate 2.0. This is typically
     * Pstate and GPC2CLK (2). For Pstate 3.0, this will be number of enabled
     * clock domains.
     */
    LwU32               nDomainGroups;
    /*!
     * Array of semantic VPstate indexes indexed by VPstate name.  These are the
     * indexes as specified in the vpstate table header.
     */
    LwU8                vPstateIdxs[RM_PMU_PERF_VPSTATE_IDX_MAX_IDXS];
} VPSTATES;

/* ------------------------ Types Definitions ------------------------------- */
/*!
 * @interface PERF_VPSTATE
 *
 * Helper function to get the VPSTATE pointer using the RM_PMU_PERF_VPSTATE_IDX
 *
 * @param[in]   vPstateIdx  RM_PMU_PERF_VPSTATE_IDX to get the VPSTATE
 *
 * @return VPSTATE  VPSTATE pointer
 * @return other    Error propagated from inside.
 */
#define VpstateGetBySemanticIdx(fname) VPSTATE * (fname)(RM_PMU_PERF_VPSTATE_IDX vPstateIdx)

/*!
 * @interface PERF_VPSTATE
 *
 * Heler function to get the domain group value in a VPSTATE entry for a given
 * domain group index.
 *
 * @param[out]  pVpstate    VPSTATE pointer
 * @param[in]   domGrpIdx   Index of the domain group to use
 * @param[out]  pValue      Pointer to domain group value
 *
 * @return FLCN_OK
 *      The operation is completed successfully.
 * @return other
 *      Error propagated from inside.
 */
#define VpstateDomGrpGet(fname) FLCN_STATUS (fname)(VPSTATE *pVpstate, LwU8 domGrpIdx, LwU32 *pValue)

/*!
 * @brief       Helper interface to ilwalidate vpstates.
 *
 * @return  FLCN_OK Successfully ilwalidated the PSTATEs
 * @return  Other   Errors passed through from internal function calls
 */
#define VpstatesIlwalidate(fname) FLCN_STATUS (fname)(void)

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// BOARDOBJGRP interfaces
BoardObjGrpIfaceModel10CmdHandler   (vpstateBoardObjGrpIfaceModel10Set)
    GCC_ATTRIB_SECTION("imem_libPerfBoardObj", "vpstateBoardObjGrpIfaceModel10Set");

// BOARDOBJ interfaces
BoardObjGrpIfaceModel10ObjSet       (vpstateGrpIfaceModel10ObjSetImpl_SUPER)
    GCC_ATTRIB_SECTION("imem_libVpstateConstruct", "vpstateGrpIfaceModel10ObjSetImpl_SUPER");

// VPSTATE Accessors
VpstateGetBySemanticIdx (vpstateGetBySemanticIdx)
    GCC_ATTRIB_SECTION("imem_perfVf", "vpstateGetBySemanticIdx");
VpstateDomGrpGet        (vpstateDomGrpGet)
    GCC_ATTRIB_SECTION("imem_perfVf", "vpstateDomGrpGet");

// VPSTATE Interfaces
VpstatesIlwalidate   (vpstatesIlwalidate)
    GCC_ATTRIB_SECTION("imem_perfIlwalidation", "vpstatesIlwalidate");

/* ------------------------ Include Derived Types -------------------------- */
#include "perf/3x/vpstate_3x.h"

#endif // VPSTATE_H

