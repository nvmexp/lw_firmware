/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLIENT_CLK_VF_POINT_H
#define CLIENT_CLK_VF_POINT_H

/*!
 * @file client_clk_vf_point.h
 * @brief @copydoc client_clk_vf_point.c
 */

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"
#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobjgrp_iface_model_10.h"
#include "boardobj/boardobj_iface_model_10.h"

/* ------------------------ Macros ----------------------------------------- */
/*!
 * Macro to locate CLIENT_CLK_VF_POINTS BOARDOBJGRP.
 *
 * @copydoc BOARDOBJGRP_OBJ_GET
 */
#define BOARDOBJGRP_DATA_LOCATION_CLIENT_CLK_VF_POINT                         \
    (&Clk.clientVfPoints.super.super)

/*!
 * Macro to get BOARDOBJ pointer from CLIENT_CLK_VF_POINTS BOARDOBJGRP.
 *
 * @param[in]  type        CLIENT_CLK_VF_POINT BOARDOBJGRP type (PRI | SEC).
 * @param[in]  idx         CLIENT_CLK_VF_POINT index.
 *
 * @copydoc BOARDOBJGRP_OBJ_GET
 */
#define CLIENT_CLK_VF_POINT_GET(idx)                                          \
    (BOARDOBJGRP_OBJ_GET(CLIENT_CLK_VF_POINT, (_idx)))

/* ------------------------ Datatypes -------------------------------------- */
/*!
 * Forward definition of CLIENT_CLK_VF_POINT_LINK structure so that can use the type in
 * interface definitions.
 */
typedef union CLIENT_CLK_VF_POINT_LINK CLIENT_CLK_VF_POINT_LINK;

/*!
 * CLIENT_CLK_VF_POINT_LINK types.
 *
 * _FREQ - A frequency-based CLIENT_CLK_VF_POINT_LINK, pointing to
 *     CLK_PROG_1X_PRIMARY.
 * _VOLT - A voltage-based CLIENT_CLK_VF_POINT_LINK, pointing to
 *     CLK_VF_POINT_VOLT.
 */
#define CLIENT_CLK_VF_POINT_LINK_TYPE_FREQ                  0x00U
#define CLIENT_CLK_VF_POINT_LINK_TYPE_VOLT                  0x01U


/*!
 * CLK_VF_POINT_LINK SUPER-class structure.
 */
typedef struct
{
    /*!
     * Must always be first element in structure!
     *
     * @copydoc CLIENT_CLK_VF_POINT_LINK_TYPE_<XYZ>.
     */
    LwU8 type;

    /*!
     * Index of the corresponding CLK_VF_POINT_VOLT within the CLK_VF_POINTS
     * BOARDOBJGRP.
     */
    LwBoardObjIdx vfPointIdx;
} CLIENT_CLK_VF_POINT_LINK_SUPER;

/*!
 * CLK_VF_POINT_LINK_TYPE_FREQ-specific data.
 */
typedef struct
{
    /*!
     * SUPER class structure.
     *
     * Must always be first element in structure!
     */
    CLIENT_CLK_VF_POINT_LINK_SUPER super;
    /*!
     * Index of the corresponding CLK_VF_REL within the CLK_VF_RELS
     * BOARDOBJGRP. This will be copied from progIdx from RM.
     */
    LwU8 vfRelIdx;
} CLIENT_CLK_VF_POINT_LINK_FREQ;

/*!
 * CLK_VF_POINT_LINK_TYPE_VOLT-specific data.
 */
typedef struct
{
    /*!
     * SUPER class structure.
     *
     * Must always be first element in structure!
     */
    CLIENT_CLK_VF_POINT_LINK_SUPER super;
} CLIENT_CLK_VF_POINT_LINK_VOLT;

/*!
 * Union of all CLIENT_VF_POINT_LINK_TYPE structures.
 *
 * The CLIENT_CLK_VF_POINT_LINK structure describes how the CLIENT_CLK_VF_POINT
 * links to internal RM CLK state to implement the CLIENT_CLK_VF_POINT
 * functionality.
 *
 * To be used within @ref CLIENT_CLK_VF_POINT to implement polymorphism without
 * requiring a separately-allocated structure.
 */
union CLIENT_CLK_VF_POINT_LINK
{
    CLIENT_CLK_VF_POINT_LINK_SUPER super;
    CLIENT_CLK_VF_POINT_LINK_FREQ  freq;
    CLIENT_CLK_VF_POINT_LINK_VOLT  volt;
};

/*!
 * Client Clock VF Point structure.
 */
typedef struct
{
    /*!
     * BOARDOBJ super class.  Must always be the first element in the structure.
     */
    BOARDOBJ                    super;
    /*!
     * Represent the size of ref@ link array which is basically the count
     * of valid links for given vf point.
     */
    LwU8                        numLinks;
    /*!
     * If the GPU chip support multiple volt rails than we will have one link
     * corresponding to each volt rails.
     */
    CLIENT_CLK_VF_POINT_LINK    link[LW2080_CTRL_VOLT_VOLT_RAIL_CLIENT_MAX_RAILS];
} CLIENT_CLK_VF_POINT;

/*!
 * Group of Client Clock VF Points.  Implements BOARDOBJGRP_E255.
 */
typedef struct
{
    /*!
     * BOARDOBJGRP_E255 super class representing VF lwrve.
     * Must always be the first element in the structure.
     */
    BOARDOBJGRP_E255    super;
} CLIENT_CLK_VF_POINTS;

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// BOARDOBJGRP interfaces
BoardObjGrpIfaceModel10CmdHandler  (clientClkVfPointBoardObjGrpIfaceModel10Set)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clientClkVfPointBoardObjGrpIfaceModel10Set");
BoardObjGrpIfaceModel10CmdHandler  (clientClkVfPointBoardObjGrpIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_libClkStatus", "clientClkVfPointBoardObjGrpIfaceModel10GetStatus");

// BOARDOBJ interfaces
BoardObjGrpIfaceModel10ObjSet (clientClkVfPointGrpIfaceModel10ObjSet_SUPER)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clientClkVfPointGrpIfaceModel10ObjSet_SUPER");
BoardObjIfaceModel10GetStatus     (clientClkVfPointIfaceModel10GetStatus_SUPER)
    GCC_ATTRIB_SECTION("imem_libClkStatus", "clientClkVfPointIfaceModel10GetStatus_SUPER");

// CLIENT_CLK_VF_POINT interfaces

/* ------------------------ Include Derived Types -------------------------- */
#include "clk/client_clk_vf_point_prog.h"

#endif // CLIENT_CLK_VF_POINT_H
