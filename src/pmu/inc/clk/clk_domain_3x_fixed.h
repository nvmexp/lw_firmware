/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLK_DOMAIN_3X_FIXED_H
#define CLK_DOMAIN_3X_FIXED_H

/*!
 * @file clk_domain_3x_fixed.h
 * @brief @copydoc clk_domain_3x_fixed.c
 */

/* ------------------------ Includes --------------------------------------- */
#include "clk/clk_domain_3x.h"

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Datatypes -------------------------------------- */
/*!
 * Clock Domain 3X Fixed structure.  Contains information specific to a Fixed
 * Clock Domain.
 */
typedef struct
{
    /*!
     * CLK_DOMAIN_3X super class.  Must always be first object in the
     * structure.
     */
    CLK_DOMAIN_3X super;

    /*!
     * Fixed frequency of the given CLK_DOMAIN in MHz.  This is the frequency
     * that the VBIOS DEVINIT has programmed for the CLK_DOMAIN.
     */
    LwU16  freqMHz;
} CLK_DOMAIN_3X_FIXED;

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// BOARDOBJ interfaces
BoardObjGrpIfaceModel10ObjSet (clkDomainGrpIfaceModel10ObjSet_3X_FIXED)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkDomainGrpIfaceModel10ObjSet_3X_FIXED");

/* ------------------------ Include Derived Types -------------------------- */

#endif // CLK_DOMAIN_3X_FIXED_H
