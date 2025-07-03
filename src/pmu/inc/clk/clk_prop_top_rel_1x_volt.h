/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLK_PROP_TOP_REL_1X_VOLT_H_
#define CLK_PROP_TOP_REL_1X_VOLT_H_

/*!
 * @file clk_prop_top_rel_1x_volt.h
 * @brief @copydoc clk_prop_top_rel_1x_volt.c
 */

/* ------------------------ Includes --------------------------------------- */
#include "clk/clk_prop_top_rel_1x.h"

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Datatypes -------------------------------------- */
/*!
 * Forward definition of CLK_PROP_TOP_REL_1X_VOLT structure so that can use the
 * type in interface definitions.
 */
typedef struct CLK_PROP_TOP_REL_1X_VOLT CLK_PROP_TOP_REL_1X_VOLT;

/*!
 * Struct containing parameters specific to volt based clock propagation
 * topology relationship.
 */
struct CLK_PROP_TOP_REL_1X_VOLT
{
    /*!
     * super class. Must always be first object in the structure.
     */
    CLK_PROP_TOP_REL_1X super;
    /*!
     * Voltage Rail Index.
     */
    LwU8                voltRailIdx;
};

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// BOARDOBJ interfaces
BoardObjGrpIfaceModel10ObjSet           (clkPropTopRelGrpIfaceModel10ObjSet_1X_VOLT)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkPropTopRelGrpIfaceModel10ObjSet_1X_VOLT");

// CLK_PROP_TOP_REL interfaces
ClkPropTopRelFreqPropagate  (clkPropTopRelFreqPropagate_1X_VOLT)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkPropTopRelFreqPropagate_1X_VOLT");

/* ------------------------ Include Derived Types -------------------------- */

#endif // CLK_PROP_TOP_REL_1X_VOLT_H_
