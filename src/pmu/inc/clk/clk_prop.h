/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLK_PROP_H
#define CLK_PROP_H

/*!
 * @file clk_prop.h
 * @brief @copydoc clk_prop.c
 */

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"
#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobjgrp_iface_model_10.h"
#include "boardobj/boardobj_iface_model_10.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct CLK_PROP CLK_PROP, CLK_PROP_BASE;

/* ------------------------ Macros ----------------------------------------- */
/*!
 * Macro to locate CLK_PROPS BOARDOBJGRP.
 *
 * @copydoc BOARDOBJGRP_OBJ_GET
 */
#define BOARDOBJGRP_DATA_LOCATION_CLK_PROP                                    \
    (&Clk.props.super.super)

/* ------------------------ Datatypes -------------------------------------- */
/*!
 * Clock propagation class params.
 */
struct CLK_PROP
{
    /*!
     * BOARDOBJ super class. Must always be the first element in the structure.
     */
    BOARDOBJ    super;
    /*!
     * Temporary variable for defining the infrastrcture.
     */
    LwU8        rsvd;
};

/*!
 * Clock propagation group params.
 */
typedef struct
{
    /*!
     * BOARDOBJGRP_E32 super class. Must always be the first element in the
     * structure.
     */
    BOARDOBJGRP_E32 super;
} CLK_PROPS;

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// BOARDOBJGRP interfaces
BoardObjGrpIfaceModel10CmdHandler   (clkPropBoardObjGrpIfaceModel10Set)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkPropGrpSet");

// BOARDOBJ interfaces
BoardObjGrpIfaceModel10ObjSet       (clkPropGrpIfaceModel10ObjSet_SUPER)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkPropGrpIfaceModel10ObjSet_SUPER");

// CLK_PROP interfaces

/* ------------------------ Include Derived Types -------------------------- */

#endif // CLK_PROP_H
