/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLK_PROP_TOP_REL_1X_RATIO_H_
#define CLK_PROP_TOP_REL_1X_RATIO_H_

/*!
 * @file clk_prop_top_rel_1x_ratio.h
 * @brief @copydoc clk_prop_top_rel_1x_ratio.c
 */

/* ------------------------ Includes --------------------------------------- */
#include "clk/clk_prop_top_rel_1x.h"

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Datatypes -------------------------------------- */
/*!
 * Forward definition of CLK_PROP_TOP_REL_1X_RATIO structure so that can use the
 * type in interface definitions.
 */
typedef struct CLK_PROP_TOP_REL_1X_RATIO CLK_PROP_TOP_REL_1X_RATIO;

/*!
 * Struct containing parameters specific to ratio based clock propagation
 * topology relationship.
 */
struct CLK_PROP_TOP_REL_1X_RATIO
{
    /*!
     * super class. Must always be first object in the structure.
     */
    CLK_PROP_TOP_REL_1X super;
    /*!
     * Ratio Relationship between source and destination. (unsigned percentage)
     *
     * @note - To callwlate reciprocal of an FXP X.Y number without losing any
     *         precision, the reciprocal will be of size (1+Y).X. While VBIOS
     *         can store number as 4.12, internally we should store as 16.16 so
     *         we have enough precision to store both ratio and its reciprocal
     *         in the same format w/o any potential loss of precision, and with
     *         same code to compute in either direction.
     */
    LwUFXP16_16         ratio;

    /*!
     * Ilwerse value of @ref ratio which will be callwlated and used by SW
     * if @ref bBiDirectional is TRUE.
     */
    LwUFXP16_16         ratioIlwerse;
};

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// BOARDOBJ interfaces
BoardObjGrpIfaceModel10ObjSet           (clkPropTopRelGrpIfaceModel10ObjSet_1X_RATIO)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkPropTopRelGrpIfaceModel10ObjSet_1X_RATIO");

// CLK_PROP_TOP_REL interfaces
ClkPropTopRelFreqPropagate  (clkPropTopRelFreqPropagate_1X_RATIO)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkPropTopRelFreqPropagate_1X_RATIO");

/* ------------------------ Include Derived Types -------------------------- */

#endif // CLK_PROP_TOP_REL_1X_RATIO_H_
