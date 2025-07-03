/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLK_PROP_TOP_REL_1X_TABLE_H_
#define CLK_PROP_TOP_REL_1X_TABLE_H_

/*!
 * @file clk_prop_top_rel_1x_table.h
 * @brief @copydoc clk_prop_top_rel_1x_table.c
 */

/* ------------------------ Includes --------------------------------------- */
#include "clk/clk_prop_top_rel_1x.h"

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Datatypes -------------------------------------- */
/*!
 * Forward definition of CLK_PROP_TOP_REL_1X_TABLE structure so that can use the
 * type in interface definitions.
 */
typedef struct CLK_PROP_TOP_REL_1X_TABLE CLK_PROP_TOP_REL_1X_TABLE;

/*!
 * Struct containing parameters specific to table based clock propagation
 * topology relationship.
 */
struct CLK_PROP_TOP_REL_1X_TABLE
{
    /*!
     * super class. Must always be first object in the structure.
     */
    CLK_PROP_TOP_REL_1X super;
    /*!
     * First array index into table relationship array.
     */
    LwU8                tableRelIdxFirst;
    /*!
     * Lirst array index into table relationship array.
     */
    LwU8                tableRelIdxLast;
};

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// BOARDOBJ interfaces
BoardObjGrpIfaceModel10ObjSet           (clkPropTopRelGrpIfaceModel10ObjSet_1X_TABLE)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkPropTopRelGrpIfaceModel10ObjSet_1X_TABLE");

// CLK_PROP_TOP_REL interfaces
ClkPropTopRelFreqPropagate  (clkPropTopRelFreqPropagate_1X_TABLE)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkPropTopRelFreqPropagate_1X_TABLE");
ClkPropTopRelCache          (clkPropTopRelCache_1X_TABLE)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkPropTopRelCache_1X_TABLE");

/* ------------------------ Include Derived Types -------------------------- */

#endif // CLK_PROP_TOP_REL_1X_TABLE_H_
