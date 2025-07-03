/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLK_PROP_TOP_REL_1X_H_
#define CLK_PROP_TOP_REL_1X_H_

/*!
 * @file clk_prop_top_rel_1x.h
 * @brief @copydoc clk_prop_top_rel_1x.c
 */

/* ------------------------ Includes --------------------------------------- */
#include "clk/clk_prop_top_rel.h"

/* ------------------------ Macros ----------------------------------------- */
/*!
 * Wrapper to _SUPER class implementation.
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
#define clkPropTopRelGrpIfaceModel10ObjSet_1X(pModel10, ppBoardObj, size, pBoardObjSet)   \
    clkPropTopRelGrpIfaceModel10ObjSet_SUPER((pModel10), (ppBoardObj), (size), (pBoardObjSet))

/* ------------------------ Datatypes -------------------------------------- */
/*!
 * Forward definition of CLK_PROP_TOP_REL_1X structure so that can use the
 * type in interface definitions.
 */
typedef struct CLK_PROP_TOP_REL_1X CLK_PROP_TOP_REL_1X;

/*!
 * Struct containing parameters specific to ratio based clock propagation
 * topology relationship.
 */
struct CLK_PROP_TOP_REL_1X
{
    /*!
     * super class. Must always be first object in the structure.
     */
    CLK_PROP_TOP_REL super;
};

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Include Derived Types -------------------------- */
#include "clk/clk_prop_top_rel_1x_ratio.h"
#include "clk/clk_prop_top_rel_1x_table.h"
#include "clk/clk_prop_top_rel_1x_volt.h"
#include "clk/clk_prop_top_rel_1x_vfe.h"

#endif // CLK_PROP_TOP_REL_1X_H_
