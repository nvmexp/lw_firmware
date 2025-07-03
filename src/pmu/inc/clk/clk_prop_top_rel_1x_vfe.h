/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLK_PROP_TOP_REL_1X_VFE_H_
#define CLK_PROP_TOP_REL_1X_VFE_H_

/*!
 * @file clk_prop_top_rel_1x_vfe.h
 * @brief @copydoc clk_prop_top_rel_1x_vfe.c
 */

/* ------------------------ Includes --------------------------------------- */
#include "clk/clk_prop_top_rel_1x.h"
#include "perf/3x/vfe.h"

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Datatypes -------------------------------------- */
/*!
 * Forward definition of CLK_PROP_TOP_REL_1X_VFE structure so that can use the
 * type in interface definitions.
 */
typedef struct CLK_PROP_TOP_REL_1X_VFE CLK_PROP_TOP_REL_1X_VFE;

/*!
 * Struct containing parameters specific to vfe based clock propagation
 * topology relationship.
 */
struct CLK_PROP_TOP_REL_1X_VFE
{
    /*!
     * super class. Must always be first object in the structure.
     */
    CLK_PROP_TOP_REL_1X super;
    /*!
     * VFE Equation Index.
     */
    LwVfeEquIdx         vfeIdx;
};

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// BOARDOBJ interfaces
BoardObjGrpIfaceModel10ObjSet           (clkPropTopRelGrpIfaceModel10ObjSet_1X_VFE)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkPropTopRelGrpIfaceModel10ObjSet_1X_VFE");

// CLK_PROP_TOP_REL interfaces
ClkPropTopRelFreqPropagate  (clkPropTopRelFreqPropagate_1X_VFE)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkPropTopRelFreqPropagate_1X_VFE");

/* ------------------------ Include Derived Types -------------------------- */

#endif // CLK_PROP_TOP_REL_1X_VFE_H_
