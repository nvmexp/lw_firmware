/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLIENT_CLK_PROP_TOP_POL_1X_H_
#define CLIENT_CLK_PROP_TOP_POL_1X_H_

/*!
 * @file client_clk_prop_top_pol_1x.h
 * @brief @copydoc client_clk_prop_top_pol_1x.c
 */

/* ------------------------ Includes --------------------------------------- */
#include "clk/client_clk_prop_top_pol.h"

/* ------------------------ Macros ----------------------------------------- */
/*!
 * Wrapper to _SUPER class implementation.
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
#define clkClientClkPropTopPolGrpIfaceModel10ObjSet_1X(pModel10, ppBoardObj, size, pBoardObjSet)   \
    clkClientClkPropTopPolGrpIfaceModel10ObjSet_SUPER((pModel10), (ppBoardObj), (size), (pBoardObjSet))

/* ------------------------ Datatypes -------------------------------------- */
/*!
 * Forward definition of CLIENT_CLK_PROP_TOP_POL_1X structure so that can use the
 * type in interface definitions.
 */
typedef struct CLIENT_CLK_PROP_TOP_POL_1X CLIENT_CLK_PROP_TOP_POL_1X;

/*!
 * Structure containing parameters specific to 1.0 version based clock propagation
 * topology policy.
 */
struct CLIENT_CLK_PROP_TOP_POL_1X
{
    /*!
     * super class. Must always be first object in the structure.
     */
    CLIENT_CLK_PROP_TOP_POL super;
};

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Include Derived Types -------------------------- */
#include "clk/client_clk_prop_top_pol_1x_slider.h"

#endif // CLIENT_CLK_PROP_TOP_POL_1X_H_
