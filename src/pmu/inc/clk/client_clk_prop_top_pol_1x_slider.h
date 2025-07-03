/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLIENT_CLK_PROP_TOP_POL_1X_SLIDER_H_
#define CLIENT_CLK_PROP_TOP_POL_1X_SLIDER_H_

/*!
 * @file client_clk_prop_top_pol_1x_slider.h
 * @brief @copydoc client_clk_prop_top_pol_1x_slider.c
 */

/* ------------------------ Includes --------------------------------------- */
#include "clk/client_clk_prop_top_pol_1x.h"

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Datatypes -------------------------------------- */
/*!
 * Forward definition of CLIENT_CLK_PROP_TOP_POL_1X_SLIDER structure so that can use the
 * type in interface definitions.
 */
typedef struct CLIENT_CLK_PROP_TOP_POL_1X_SLIDER CLIENT_CLK_PROP_TOP_POL_1X_SLIDER;

/*!
 * Structure containing parameters specific to version 1.x slider based clock
 * propagation topology policy.
 */
struct CLIENT_CLK_PROP_TOP_POL_1X_SLIDER
{
    /*!
     * super class. Must always be first object in the structure.
     */
    CLIENT_CLK_PROP_TOP_POL_1X super;

    /*!
     * Total number of discrete points supported for this policy slider.
     */
    LwU8    numPoints;

    /*!
     * Default POR point index on this slider.
     */
    LwU8    defaultPoint;

    /*!
     * Client chosen point index on this slider.
     *
     * Each policy exposes set of available points from which client could
     * choose a single point. The internal SW arbitration logic will determine
     * final active topology based on all actively chosen point from all
     * active policys.
     *
     * Logic:
     * Use chosen point index if
     * @ref chosenPoint != LW2080_CTRL_CLK_CLIENT_CLK_PROP_TOP_POL_SLIDER_POINT_IDX_ILWALID
     */
    LwU8    chosenPoint;

    /*!
     * Mask of internal clock propagation topologies that this
     * client policy is allowed to choose based on client's chosen
     * point hint.
     *
     * The internal clock propagation topology infrastructure will
     * have arbitration logic to force the final active topology
     * based on all internal and external hints.
     *
     * @private
     *      This is private member variable and must not be exposed
     *      to external RMCTRLs.
     */
    BOARDOBJGRPMASK_E32     propTopsMask;

    /*!
     * Array of points with associated POR information.
     */
    RM_PMU_CLK_CLIENT_CLK_PROP_TOP_POL_1X_SLIDER_POINT
            points[LW2080_CTRL_CLK_CLIENT_CLK_PROP_TOP_POL_SLIDER_POINT_IDX_MAX];
};

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// BOARDOBJ interfaces
BoardObjGrpIfaceModel10ObjSet           (clkClientClkPropTopPolGrpIfaceModel10ObjSet_1X_SLIDER)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkClientClkPropTopPolGrpIfaceModel10ObjSet_1X_SLIDER");

// CLK_PROP_TOP_POL interfaces
ClkClientClkPropTopPolGetChosenTopId     (clkClientClkPropTopPolGetChosenTopId_1X_SLIDER)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkClientClkPropTopPolGetChosenTopId_1X_SLIDER");

/* ------------------------ Include Derived Types -------------------------- */

#endif // CLIENT_CLK_PROP_TOP_POL_1X_SLIDER_H_
