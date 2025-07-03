/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pstate.h
 * @brief   Declarations, type definitions, and macros for the PSTATES SW module.
 */

#ifndef PSTATE_H
#define PSTATE_H

#include "g_pstate.h"

#ifndef G_PSTATE_H
#define G_PSTATE_H

/* ------------------------ Includes --------------------------------------- */
#include "boardobj/boardobjgrp.h"
#include "pmu/pmuifperf.h"
#include "boardobj/boardobjgrp_iface_model_10.h"
#include "boardobj/boardobj_iface_model_10.h"

/* ------------------------ Macros ----------------------------------------- */
/*!
 * @brief   List of overlay descriptors required to read PSTATE clk entries.
 *
 * @note    Review the use cases in Tasks.pm and update MAX_OVERLAYS_IMEM_LS after
 *          updates to this macro. Failure to do so might result in falcon halt.
 */
#define OSTASK_OVL_DESC_DEFINE_PSTATE_CLIENT \
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libBoardObj) \
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfVf) \
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perf) \
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libSemaphoreRW)

/*!
 * @brief       Helper accessor to the PSTATES BOARDOBJGRP.
 *
 * @return      Pointer to PSTATES BOARDOBJGRP or NULL if
 *              feature is not supported.
 *
 * @memberof    PSTATES
 *
 * @public
 */
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_PSTATES)
#define PSTATES_GET()   \
    (&Perf.pstates)
#else
#define PSTATES_GET()   \
    ((PSTATES *)NULL)
#endif

/*!
 * @brief       Macro to locate PSTATES BOARDOBJGRP.
 *
 * Allows @ref BOARDOBJGRP_GRP_GET to work for the PSTATE class name.
 *
 * @memberof    PSTATES
 *
 * @public
 */
#define BOARDOBJGRP_DATA_LOCATION_PSTATE    \
    (&(PSTATES_GET()->super.super))

/*!
 * @brief       Checks to see if a PSTATE BOARDOBJGRP index is valid.
 *
 * @param[in]   _pstateIdx  BOARDOBJGRP index for a PSTATE BOARDOBJ.
 *
 * @return      LW_TRUE     if _pstateIdx is valid within the PSTATES BOARODBJGRP.
 * @return      LW_FALSE    if _pstateIdx is not valid within the PSTATES BOARODBJGRP.
 *
 * @memberof    PSTATES
 *
 * @public
 */
#define PSTATE_INDEX_IS_VALID(_pstateIdx)   \
    BOARDOBJGRP_IS_VALID(PSTATE, _pstateIdx)

/*!
 * @brief       Accessor for a PSTATE BOARDOBJ by BOARDOBJGRP index.
 *
 * @param[in]   _pstateIdx  BOARDOBJGRP index for a PSTATE BOARDOBJ.
 *
 * @return      Pointer to a PSTATE object at the provided BOARDOBJGRP index.
 *
 * @memberof    PSTATES
 *
 * @public
 */
#define PSTATE_GET(_pstateIdx)                  \
    (BOARDOBJGRP_OBJ_GET(PSTATE, (_pstateIdx)))

/*!
 * @brief       Helper macro to get the least performant Pstate (usually P8).
 *
 * @return      Pointer to the PSTATE object with lowest index.
 *
 * @memberof    PSTATES
 *
 * @public
 */
#define PSTATE_GET_BY_LOWEST_INDEX()                                            \
    PSTATE_GET(boardObjGrpMaskBitIdxLowest(&(PSTATES_GET()->super.objMask)))

/*!
 * @brief       Helper macro to get the most performant @ref PSTATE
 *              (usually P0).
 *
 * @return      Pointer to the @ref PSTATE object with highest index.
 *
 * @memberof    PSTATES
 *
 * @public
 */
#define PSTATE_GET_BY_HIGHEST_INDEX()                                           \
    PSTATE_GET(boardObjGrpMaskBitIdxHighest(&(PSTATES_GET()->super.objMask)))

/*!
 * @brief       Helper macro to get a PSTATE's low power entry index.
 *
 * @param[in]   _pstateIdx  BOARDOBJGRP index for a PSTATE BOARDOBJ.
 *
 * @return      @ref PSTATE::lpwrEntryIdx for the given PSTATE BOARDOBJGRP index.
 * @return      @ref RM_PMU_LPWR_VBIOS_IDX_ENTRY_RSVD if there is no LPWR entry.
 *
 * @memberof    PSTATE
 *
 * @public
 */
#define PSTATE_GET_LPWR_ENTRY_IDX(_pstateIdx)       \
    (BOARDOBJGRP_IS_VALID(PSTATE, (_pstateIdx)) ?   \
        (PSTATE_GET(_pstateIdx)->lpwrEntryIdx) :    \
        RM_PMU_LPWR_VBIOS_IDX_ENTRY_RSVD)

/*!
 * @brief       Helper macro to check a PSTATE's OCOV enabled flag.
 *
 * @param[in]   _pPstate    Pointer to PSTATE object.
 *
 * @return      LW_TRUE     if the PSTATE object has its OCOV flag set.
 * @return      LW_FALSE    if the PSTATE object does not have its OCOV flag set.
 *
 * @memberof    PSTATE
 *
 * @public
 */
#define PSTATE_OCOV_ENABLED(_pPstate)               \
    (((_pPstate) == NULL) ? LW_FALSE :              \
        FLD_TEST_DRF(2080_CTRL_PERF, _PSTATE_FLAGS, \
            _OVOC, _ENABLED, (_pPstate)->flags))

/*!
 * @brief       PSTATE super wrapper to BOARDOBJ QUERY (GET_STATUS) interface
 *
 * @copydoc     BoardObjIfaceModel10GetStatus
 *
 * @memberof    PSTATE
 *
 * @protected
 */
#define pstateIfaceModel10GetStatus_SUPER(_pModel10, _pPayload)    \
    (boardObjIfaceModel10GetStatus((_pModel10), (_pPayload)))

/*!
 * @brief       Helper macro to retrieve the VBIOS boot Pstate index.
 *
 * @return      @ref PSTATES::bootPstateIdx
 *
 * @memberof    PSTATE
 *
 * @public
 */
#define perfPstatesBootPstateIdxGet()                                         \
    (Perf.pstates.bootPstateIdx)

/* ------------------------ Forward Definitions ---------------------------- */
typedef struct PSTATE  PSTATE, PSTATE_BASE;
typedef struct PSTATES PSTATES;

/* ------------------------ Datatypes -------------------------------------- */
/*!
 * @brief   PSTATE BOARDOBJ base class.
 *
 * @details Provides information about a given PSTATE as per the VBIOS
 *          specification.
 *
 * @extends BOARDOBJ
 */
struct PSTATE
{
    /*!
     * @brief BOARDOBJ super class.
     *
     * Must always be the first element in the structure.
     *
     * @protected
     */
    BOARDOBJ    super;

    /*!
     * @brief Index into LPWR table describing this PSTATE's LPWR feature set.
     *
     * @private
     */
    LwU8        lpwrEntryIdx;

    /*!
     * @brief Mask of PSTATES feature flags.
     *
     * @ref LW2080_CTRL_PERF_PSTATE_FLAGS *.
     *
     * @private
     */
    LwU32       flags;

    /*!
     * @brief PSTATE num
     */
    LwU8        pstateNum;
};

/*!
 * @brief   PSTATES BOARDOBJGRP base class.
 *
 * Stores class wide information on the PSTATE group, as per the VBIOS
 * specification, as well as the individual PSTATE BOARDOBJs.
 *
 * @extends BOARDOBJGRP_E32
 */
struct PSTATES
{
    /*!
     * @brief BOARDOBJGRP_E32 super class.
     *
     * Must always be the first element in the structure.
     *
     * @protected
     */
    BOARDOBJGRP_E32 super;

    /*!
     * @brief Number of clock domains supported on this GPU.
     *
     * @protected
     */
    LwU8            numClkDomains;

    /*!
     * @brief VBIOS Boot Pstate index.
     *
     * @protected
     */
    LwU8            bootPstateIdx;

#if PMUCFG_FEATURE_ENABLED(PMU_PERF_PSTATES_35_FMAX_AT_VMIN)
    /*!
     * @brief Number of volt domains supported on this GPU.
     *
     * @protected
     */
    LwU8            numVoltDomains;
#endif
};

/* ------------------------ Interface Definitions -------------------------- */
/*!
 * @brief       Helper function to get a PSTATE's clock tuple for a given
 *              clock domain
 *
 * @param[in]   pPstate         PSTATE object pointer.
 * @param[in]   clkDomainIdx    Clock domain index.
 * @param[out]  pPstateClkEntry Pointer to a clock entry that will be populated.
 *
 * @return      FLCN_OK                     in case of success.
 * @return      FLCN_ERR_ILWALID_ARGUMENT   in case of any invalid inputs.
 * @return      Other                       propagated from nested function
 *                                          calls.
 */
#define PerfPstateClkFreqGet(fname) FLCN_STATUS (fname)(PSTATE *pPstate, LwU8 clkDomainIdx, LW2080_CTRL_PERF_PSTATE_CLOCK_ENTRY *pPstateClkEntry)

/* ------------------------ BOARDOBJGRP Interfaces ------------------------- */
BoardObjGrpIfaceModel10CmdHandler       (pstateBoardObjGrpIfaceModel10Set)
    GCC_ATTRIB_SECTION("imem_libPerfBoardObj", "pstateBoardObjGrpIfaceModel10Set");
BoardObjGrpIfaceModel10SetHeader  (perfPstateIfaceModel10SetHeader)
    GCC_ATTRIB_SECTION("imem_libPstateBoardObj", "perfPstateIfaceModel10SetHeader");
BoardObjGrpIfaceModel10SetEntry   (perfPstateIfaceModel10SetEntry)
    GCC_ATTRIB_SECTION("imem_libPstateBoardObj", "perfPstateIfaceModel10SetEntry");
BoardObjGrpIfaceModel10CmdHandler       (pstateBoardObjGrpIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_libPerfBoardObj", "pstateBoardObjGrpIfaceModel10GetStatus");
BoardObjIfaceModel10GetStatus               (pstateIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_libPstateBoardObj", "pstateIfaceModel10GetStatus");

/* ------------------------ BOARDOBJ Interfaces ---------------------------- */
mockable BoardObjGrpIfaceModel10ObjSet  (perfPstateGrpIfaceModel10ObjSet_SUPER)
    GCC_ATTRIB_SECTION("imem_libPstateBoardObj", "perfPstateGrpIfaceModel10ObjSet_SUPER");

/* ------------------------ PSTATE Interfaces ------------------------------ */
mockable PerfPstateClkFreqGet    (perfPstateClkFreqGet)
    GCC_ATTRIB_SECTION("imem_perfVf", "perfPstateClkFreqGet");
/*!
 * Helper function to set numVoltDomains
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline void
perfPstateSetNumVoltDomains
(
    PSTATES  *pPstate,
    LwU8      numVoltDomains
)
{
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_PSTATES_35_FMAX_AT_VMIN)
    pPstate->numVoltDomains = numVoltDomains;
#endif
}

/* ------------------------ Include Derived Types -------------------------- */
#include "perf/pstates_self_init.h"
#include "perf/pstate_infra.h"
#include "perf/3x/pstate_3x.h"

/* ------------------------ End of File ------------------------------------ */
#endif // G_PSTATE_H
#endif // PSTATE_H
