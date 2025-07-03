/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLK_DOMAIN_35_SECONDARY_H
#define CLK_DOMAIN_35_SECONDARY_H

#include "g_clk_domain_35_secondary.h"

#ifndef G_CLK_DOMAIN_35_SECONDARY_H
#define G_CLK_DOMAIN_35_SECONDARY_H

/*!
 * @file clk_domain_35_secondary.h
 * @brief @copydoc clk_domain_35_secondary.c
 */

/* ------------------------ Includes --------------------------------------- */
#include "clk/clk_domain_35_prog.h"
#include "clk/clk_domain_3x_secondary.h"

/* ------------------------ Macros ----------------------------------------- */
/*!
 * Accessor macro for a CLK_DOMAIN_3X_SECONDARY structure from CLK_DOMAIN_35_SECONDARY.
 */
#define CLK_CLK_DOMAIN_3X_SECONDARY_GET_FROM_35_SECONDARY(_pSecondary35)                  \
    &((_pSecondary35)->secondary)

/* ------------------------ Datatypes -------------------------------------- */
/*!
 * Forward definition of CLK_DOMAIN_35_SECONDARY structures so that can use the
 * type in interface definitions.
 */
typedef struct CLK_DOMAIN_35_SECONDARY CLK_DOMAIN_35_SECONDARY;

/*!
 * Clock Domain 35 Secondary structure.  Contains information specific to a
 * Secondary Clock Domain.
 */
struct CLK_DOMAIN_35_SECONDARY
{
    /*!
     * CLK_DOMAIN_35_PROG super class. Must always be first object in the
     * structure.
     */
    CLK_DOMAIN_35_PROG  super;

    /*!
     * CLK_DOMAIN_3X_SECONDARY super class.
     */
    CLK_DOMAIN_3X_SECONDARY secondary;
};

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// BOARDOBJ interfaces
BoardObjGrpIfaceModel10ObjSet (clkDomainGrpIfaceModel10ObjSet_35_SECONDARY)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkDomainGrpIfaceModel10ObjSet_35_SECONDARY");

ClkDomainProgClientFreqDeltaAdj     (clkDomainProgClientFreqDeltaAdj_35_SECONDARY)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainProgClientFreqDeltaAdj_35_SECONDARY");
mockable ClkDomainProgVoltToFreqTuple (clkDomainProgVoltToFreqTuple_35_SECONDARY)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkDomainProgVoltToFreqTuple_35_SECONDARY");
ClkDomain3XProgVoltAdjustDeltauV    (clkDomain3XProgVoltAdjustDeltauV_35_SECONDARY)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomain3XProgVoltAdjustDeltauV_35_SECONDARY");

/* ------------------------ Include Derived Types -------------------------- */

#endif // G_CLK_DOMAIN_35_SECONDARY_H
#endif // CLK_DOMAIN_35_SECONDARY_H
