/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLK_DOMAIN_30_SECONDARY_H
#define CLK_DOMAIN_30_SECONDARY_H

/*!
 * @file clk_domain_30_secondary.h
 * @brief @copydoc clk_domain_30_secondary.c
 */

/* ------------------------ Includes --------------------------------------- */
#include "clk/clk_domain_3x_prog.h"
#include "clk/clk_domain_3x_secondary.h"

/* ------------------------ Macros ----------------------------------------- */
/*!
 * Accessor macro for a CLK_DOMAIN_3X_SECONDARY structure from CLK_DOMAIN_30_SECONDARY.
 */
#define CLK_CLK_DOMAIN_3X_SECONDARY_GET_FROM_30_SECONDARY(_pSecondary30)                  \
    &((_pSecondary30)->secondary)

/* ------------------------ Datatypes -------------------------------------- */
/*!
 * Forward definition of CLK_DOMAIN_30_SECONDARY structures so that can use the
 * type in interface definitions.
 */
typedef struct CLK_DOMAIN_30_SECONDARY CLK_DOMAIN_30_SECONDARY;

/*!
 * Clock Domain 30 Secondary structure.  Contains information specific to a
 * Secondary Clock Domain.
 */
struct CLK_DOMAIN_30_SECONDARY
{
    /*!
     * CLK_DOMAIN_30_PROG super class. Must always be first object in the
     * structure.
     */
    CLK_DOMAIN_30_PROG  super;

    /*!
     * CLK_DOMAIN_3X_SECONDARY super class.
     */
    CLK_DOMAIN_3X_SECONDARY secondary;
};

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// BOARDOBJ interfaces
BoardObjGrpIfaceModel10ObjSet (clkDomainGrpIfaceModel10ObjSet_30_SECONDARY)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkDomainGrpIfaceModel10ObjSet_30_SECONDARY");

/* ------------------------ Include Derived Types -------------------------- */

#endif // CLK_DOMAIN_30_SECONDARY_H
