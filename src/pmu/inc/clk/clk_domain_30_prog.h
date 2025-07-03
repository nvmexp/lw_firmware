/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLK_DOMAIN_30_PROG_H
#define CLK_DOMAIN_30_PROG_H

/*!
 * @file clk_domain_30_prog.h
 * @brief @copydoc clk_domain_30_prog.c
 */

/* ------------------------ Includes --------------------------------------- */
#include "clk/clk_domain_3x_prog.h"

/* ------------------------ Macros ----------------------------------------- */
/*!
 * _30_PROG wrapper to _3X_PROG implemenation.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
#define clkDomainProgClientFreqDeltaAdj_30(pDomainProg, pFreqMHz)         \
    (clkDomain3XProgFreqAdjustDeltaMHz(                                   \
        ((CLK_DOMAIN_3X_PROG *)INTERFACE_TO_BOARDOBJ_CAST(pDomainProg)),   \
        (pFreqMHz), LW_TRUE, LW_TRUE))

/* ------------------------ Datatypes -------------------------------------- */
/*!
 * Clock Domain 30 Prog structure. Contains information specific to a
 * Programmable Clock Domain.
 */
typedef struct
{
    /*!
     * CLK_DOMAIN_3X_PROG super class. Must always be first object in the
     * structure.
     */
    CLK_DOMAIN_3X_PROG super;
    /*!
     * Noise-unaware ordering index for clock programming changes.
     */
    LwU8 noiseUnawareOrderingIndex;
    /*!
     * Noise-Aware ordering index for clock programming changes.  Applicable
     * only if @ref CLK_DOMAIN_3X::bNoiseAwareCapable == LW_TRUE.
     */
    LwU8 noiseAwareOrderingIndex;
} CLK_DOMAIN_30_PROG;

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// BOARDOBJ interfaces
BoardObjGrpIfaceModel10ObjSet (clkDomainGrpIfaceModel10ObjSet_30_PROG)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkDomainGrpIfaceModel10ObjSet_30_PROG");

// CLK_DOMAIN_PROG interfaces
ClkDomainProgIsSecVFLwrvesEnabled       (clkDomainProgIsSecVFLwrvesEnabled_30)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainProgIsSecVFLwrvesEnabled_30");

/* ------------------------ Include Derived Types -------------------------- */
#include "clk/clk_domain_30_primary.h"
#include "clk/clk_domain_30_secondary.h"

#endif // CLK_DOMAIN_30_PROG_H
