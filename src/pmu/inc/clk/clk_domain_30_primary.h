/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLK_DOMAIN_30_PRIMARY_H
#define CLK_DOMAIN_30_PRIMARY_H

/*!
 * @file clk_domain_30_primary.h
 * @brief @copydoc clk_domain_30_primary.c
 */

/* ------------------------ Includes --------------------------------------- */
#include "clk/clk_domain_3x_prog.h"
#include "clk/clk_domain_3x_primary.h"

/* ------------------------ Macros ----------------------------------------- */
/*!
 * Accessor macro for a CLK_DOMAIN_3X_PRIMARY structure from CLK_DOMAIN_30_PRIMARY.
 */
#define CLK_CLK_DOMAIN_3X_PRIMARY_GET_FROM_30_PRIMARY(_pPrimary30)               \
    &((_pPrimary30)->primary)

/* ------------------------ Datatypes -------------------------------------- */
/*!
 * Forward definition of CLK_DOMAIN_30_PRIMARY structures so that can use the
 * type in interface definitions.
 */
typedef struct CLK_DOMAIN_30_PRIMARY CLK_DOMAIN_30_PRIMARY;

/*!
 * Clock Domain 30 Primary structure.  Contains information specific to a Primary
 * Clock Domain.
 */
struct CLK_DOMAIN_30_PRIMARY
{
    /*!
     * CLK_DOMAIN_30_PROG super class.  Must always be first object in the
     * structure.
     */
    CLK_DOMAIN_30_PROG      super;

    /*!
     * CLK_DOMAIN_3X_PRIMARY super class.
     */
    CLK_DOMAIN_3X_PRIMARY    primary;
};

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// BOARDOBJ interfaces
BoardObjGrpIfaceModel10ObjSet (clkDomainGrpIfaceModel10ObjSet_30_PRIMARY)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkDomainGrpIfaceModel10ObjSet_30_PRIMARY");

// CLK_DOMAIN interfaces
ClkDomainCache                  (clkDomainCache_30_Primary)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkDomainCache_30_Primary");

/* ------------------------ Include Derived Types -------------------------- */

#endif // CLK_DOMAIN_30_PRIMARY_H
