/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLIENT_CLK_PROP_TOP_POL_H
#define CLIENT_CLK_PROP_TOP_POL_H

/*!
 * @file client_clk_prop_top_pol.h
 * @brief @copydoc client_clk_prop_top_pol.c
 */

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"
#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobjgrp_iface_model_10.h"
#include "boardobj/boardobj_iface_model_10.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct CLIENT_CLK_PROP_TOP_POL CLIENT_CLK_PROP_TOP_POL, CLIENT_CLK_PROP_TOP_POL_BASE;

/* ------------------------ Macros ----------------------------------------- */
/*!
 * Macro to locate CLIENT_CLK_PROP_TOP_POLS BOARDOBJGRP.
 *
 * @copydoc BOARDOBJGRP_OBJ_GET
 */
#define BOARDOBJGRP_DATA_LOCATION_CLIENT_CLK_PROP_TOP_POL                            \
    (&Clk.propTopPols.super.super)

/*!
 * Helper macro to return a pointer to the CLIENT_CLK_PROP_TOP_POLS object.
 *
 * @return Pointer to the CLIENT_CLK_PROP_TOP_POLS object.
 */
#define CLK_CLIENT_CLK_PROP_TOP_POLS_GET()                                           \
    (&Clk.propTopPols)

/*!
 * @brief       Accessors for a CLIENT_CLK_PROP_TOP_POL BOARDOBJ by BOARDOBJGRP index.
 *
 * @param[in]   _idx  BOARDOBJGRP index for a CLIENT_CLK_PROP_TOP_POL BOARDOBJ.
 *
 * @return      Pointer to a CLIENT_CLK_PROP_TOP_POL object at the provided BOARDOBJGRP index.
 *
 * @memberof    CLIENT_CLK_PROP_TOP_POL
 *
 * @public
 */
#define CLIENT_CLK_PROP_TOP_POL_GET(_idx)                                            \
    (BOARDOBJGRP_OBJ_GET(CLIENT_CLK_PROP_TOP_POL, (_idx)))

/* ------------------------ Datatypes -------------------------------------- */
/*!
 * Clock Propagation Topology Policy class params.
 */
struct CLIENT_CLK_PROP_TOP_POL
{
    /*!
     * BOARDOBJ super class. Must always be the first element in the structure.
     */
    BOARDOBJ    super;

    /*!
     * Client Clock Propagation Topology Policy Id.
     * @ ref LW2080_CTRL_CLK_CLIENT_CLK_PROP_TOP_POL_ID_MAX
     */
    LwU8        topPolId;
};

/*!
 * Clock Propagation TopPol group params.
 */
typedef struct
{
    /*!
     * BOARDOBJGRP_E32 super class. Must always be the first element in the
     * structure.
     */
    BOARDOBJGRP_E32    super;

    /*!
     * Clock Propagation Topology Policy HAL.
     * @ref LW2080_CTRL_CLK_CLIENT_CLK_PROP_TOP_POL_HAL_<xyz>
     */
    LwU8                topPolHal;
} CLIENT_CLK_PROP_TOP_POLS;

/*!
 * @interface CLK_PROP_TOPS
 *
 * @brief Helper interface to arbitrate among the available policys to determine
 *        the final topology to force based on client's request.
 *
 * @param[in]   pPropTopPols CLIENT_CLK_PROP_TOP_POLS pointer
 *
 * @return FLCN_OK
 *     Successfully arbitrated and forced the active topology.
 * @return FLCN_ERR_NOT_SUPPORTED
 *     More than one policys are enabled via POR, must extend the support.
 * @return other
 *     Other errors from function calls.
 */
#define ClkClientClkPropTopPolsArbitrate(fname) FLCN_STATUS (fname)(CLIENT_CLK_PROP_TOP_POLS *pPropTopPols)

/*!
 * @interface CLK_PROP_TOP
 *
 * @brief Accessors function which captures the chosen topology id from
 *        each individual policy based on their client's chosen point.
 *
 * @param[in]   pPropTopPol CLIENT_CLK_PROP_TOP_POL pointer
 * @param[out]  pTopId      Chosen topology from policy
 *
 * @return FLCN_OK
 *     Successfully updated the chosen topology index.
 */
#define ClkClientClkPropTopPolGetChosenTopId(fname) FLCN_STATUS (fname)(CLIENT_CLK_PROP_TOP_POL *pPropTopPol, LwU8 *pTopId)

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// BOARDOBJGRP interfaces
BoardObjGrpIfaceModel10CmdHandler       (clkClientClkPropTopPolBoardObjGrpIfaceModel10Set)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkClientClkPropTopPolGrpSet");

// BOARDOBJ interfaces
BoardObjGrpIfaceModel10ObjSet           (clkClientClkPropTopPolGrpIfaceModel10ObjSet_SUPER)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkClientClkPropTopPolGrpIfaceModel10ObjSet_SUPER");

// CLIENT_CLK_PROP_TOP_POLS interfaces
ClkClientClkPropTopPolsArbitrate (clkClientClkPropTopPolsArbitrate)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkClientClkPropTopPolsArbitrate");

// CLIENT_CLK_PROP_TOP_POL interfaces
ClkClientClkPropTopPolGetChosenTopId (clkClientClkPropTopPolGetChosenTopId)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkClientClkPropTopPolGetChosenTopId");

/* ------------------------ Include Derived Types -------------------------- */
#include "clk/client_clk_prop_top_pol_1x.h"

#endif // CLIENT_CLK_PROP_TOP_POL_H
