/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLK_PROP_TOP_H
#define CLK_PROP_TOP_H

/*!
 * @file clk_prop_top.h
 * @brief @copydoc clk_prop_top.c
 */

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"
#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobjgrp_iface_model_10.h"
#include "boardobj/boardobj_iface_model_10.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct CLK_PROP_TOP CLK_PROP_TOP, CLK_PROP_TOP_BASE;

/* ------------------------ Macros ----------------------------------------- */
/*!
 * Helper macro to return a pointer to the CLK_PROP_TOPS object.
 *
 * @return Pointer to the CLK_PROP_TOPS object.
 */
#define CLK_CLK_PROP_TOPS_GET()                                                  \
    (&Clk.propTops)

/*!
 * @brief       Accesor for a CLK_PROP_TOP BOARDOBJ by BOARDOBJGRP index.
 *
 * @param[in]   _idx  BOARDOBJGRP index for a CLK_PROP_TOP BOARDOBJ.
 *
 * @return      Pointer to a CLK_PROP_TOP object at the provided BOARDOBJGRP index.
 *
 * @memberof    CLK_PROP_TOP
 *
 * @public
 */
#define CLK_PROP_TOP_GET(_idx)                                                  \
    (BOARDOBJGRP_OBJ_GET(CLK_PROP_TOP, (_idx)))

/*!
 * Macro to locate CLK_PROP_TOPS BOARDOBJGRP.
 *
 * @copydoc BOARDOBJGRP_OBJ_GET
 */
#define BOARDOBJGRP_DATA_LOCATION_CLK_PROP_TOP                                \
    (&(CLK_CLK_PROP_TOPS_GET())->super.super)

/*!
 * Accessor macro to get active clock propagation topology id.
 *
 * @param[in]   pPropTops  Pointer to the CLK_PROP_TOPS object.
 *
 * @return Active clock propagation topology id.
 */
#define clkPropTopsGetActiveTopologyId(pPropTops)                                 \
    ((pPropTops->activeTopIdForced != LW2080_CTRL_CLK_CLK_PROP_TOP_ID_ILWALID) ?  \
        pPropTops->activeTopIdForced : pPropTops->activeTopId)

/* ------------------------ Datatypes -------------------------------------- */

/*!
 * Clock Propagation Top class params.
 */
struct CLK_PROP_TOP
{
    /*!
     * BOARDOBJ super class. Must always be the first element in the structure.
     */
    BOARDOBJ                                            super;

    /*!
     * Clock Propagation Topology Id.
     */
    LwU8                                                topId;

    /*!
     * Mask of clock propagation topology relationships that are valid for this clock
     * propagation topology.
     */
    BOARDOBJGRPMASK_E255                                clkPropTopRelMask;

    /*!
     * @ref LW2080_CTRL_CLK_CLK_PROP_TOP_CLK_DOMAINS_DST_PATH
     */
    LW2080_CTRL_CLK_CLK_PROP_TOP_CLK_DOMAINS_DST_PATH   domainsDstPath;
};

/*!
 * Clock Propagation Top group params.
 */
typedef struct
{
    /*!
     * BOARDOBJGRP_E32 super class. Must always be the first element in the
     * structure.
     */
    BOARDOBJGRP_E32 super;

    /*!
     * Clock Propagation Topology HAL.
     * @ref LW2080_CTRL_CLK_CLK_PROP_TOP_HAL_<xyz>
     */
    LwU8            topHal;

    /*!
     * Active Clock Propagation Topology Id.
     * SW will dynamically select the active topology from set of available
     * topologies based on the active workload hints coming from KMD/DX.
     */
    LwU8            activeTopId;

    /*!
     * Forced active Clock Propagation Topology Id.
     * When client force an active topology id, the SW will respect the forced
     * topology instead of selecting the topology based on workload.
     *
     * Logic:
     * Use forced active topology if @ref activeTopIdForced == VALID
     * otherwise use @ref activeTopId
     */
    LwU8            activeTopIdForced;
} CLK_PROP_TOPS;

/*!
 * @interface CLK_PROP_TOPS
 *
 * @brief Accessors function which provides CLK_PROP_TOP index of clkPropTopId topology.
 *
 * @param[in] clkPropTopId   CLK_PROP_TOP Id
 *
 * @return    LwBoardObjIdx  Index to CLK_PROP_TOP object
 * @return    _ILWALID       CLK_PROP_TOP index for clkPropTopId not found
 *
 */
#define ClkPropTopsGetTopologyIdxFromId(fname) LwBoardObjIdx (fname)(LwU8 clkPropTopId)

/*!
 * @interface CLK_PROP_TOPS
 *
 * @brief Mutator function to update CLK_PROP_TOPS::activeTopIdForced.
 *
 * @param[in]     pPropTops CLK_PROP_TOPS pointer
 * @param[in]     topId     Topology id to force.
 *
 * @return FLCN_OK
 *     Successfully updated the forced topology.
 * @return Errors
 *     Error during update.
 */
#define ClkPropTopsSetActiveTopologyForced(fname) FLCN_STATUS (fname)(CLK_PROP_TOPS *pPropTops, LwU8 topId)

/*!
 * @interface CLK_PROP_TOP_REL
 *
 * @brief   Clock domain programmable interface will call this interface with src and
 *          dst clock domain along with src frequency. This interface will traverse
 *          the clock propagation topology from src to dst clock to get the final
 *          dst propagated frequency.
 *
 * @param[in]     pPropTop      CLK_PROP_TOP pointer
 * @param[in]     pDomainSrc    Source CLK_DOMAIN pointer
 * @param[in]     pDomainDst    Destination CLK_DOMAIN pointer
 * @param[in/out] pFreqMHz
 *     Pointer in which caller specifies the input frequency to propagate and in
 *     which the function will return the propagated frequency.
 *
 * @return FLCN_OK
 *     Frequency successfully propagated per the clk propagation relationship.
 * @return FLCN_ERR_ILWALID_INDEX
 *     This CLK_PROP_TOP object does not does not contain valid relationship index.
 * @return other
 *     Other errors returned from the function calls.
 */
#define ClkPropTopFreqPropagate(fname) FLCN_STATUS (fname)(CLK_PROP_TOP *pPropTop, CLK_DOMAIN *pDomainSrc, CLK_DOMAIN *pDomainDst, LwU16 *pFreqMHz)

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// BOARDOBJGRP interfaces
BoardObjGrpIfaceModel10CmdHandler   (clkPropTopBoardObjGrpIfaceModel10Set)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkPropTopGrpSet");
BoardObjGrpIfaceModel10CmdHandler   (clkPropTopBoardObjGrpIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_libClkStatus", "clkPropTopBoardObjGrpIfaceModel10GetStatus");

// BOARDOBJ interfaces
BoardObjGrpIfaceModel10ObjSet       (clkPropTopGrpIfaceModel10ObjSet_SUPER)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkPropTopGrpIfaceModel10ObjSet_SUPER");

// CLK_PROP_TOP interfaces
ClkPropTopFreqPropagate             (clkPropTopFreqPropagate)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkPropTopFreqPropagate");

// CLK_PROP_TOPS interfaces
ClkPropTopsGetTopologyIdxFromId     (clkPropTopsGetTopologyIdxFromId)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkPropTopsGetTopologyIdxFromId");
ClkPropTopsSetActiveTopologyForced  (clkPropTopsSetActiveTopologyForced)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkPropTopsSetActiveTopologyForced");

/* ------------------------ Include Derived Types -------------------------- */

#endif // CLK_PROP_TOP_H
