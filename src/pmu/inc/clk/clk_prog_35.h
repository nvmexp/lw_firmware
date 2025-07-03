/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLK_PROG_35_H
#define CLK_PROG_35_H

/*!
 * @file clk_prog_35.h
 * @brief @copydoc clk_prog_35.c
 */

/* ------------------------ Includes --------------------------------------- */
#include "clk/clk_prog_3x.h"

/* ------------------------ Macros ----------------------------------------- */
/*!
 * @memberof CLK_PROG_35
 *
 * Accessor to offset adjusted max frequency of this CLK_PROG_35.
 *
 * @param[in] pProg35 CLK_PROG_35 pointer
 *
 * @return CLK_PROG_35::offsettedFreqMaxMHz
 */
#define clkProg35OffsettedFreqMaxMHzGet(pProg35)                              \
    ((pProg35)->offsettedFreqMaxMHz)

/* ------------------------ Datatypes -------------------------------------- */
/*!
 * Forward definition of CLK_PROG_35 structures so that can use
 * the type in interface definitions.
 */
typedef struct CLK_PROG_35 CLK_PROG_35;

/*!
 * Clock Programming 35 entry. Defines the how the RM will program a given
 * frequency on a clock domain per the VBIOS specification.
 *
 */
//! https://wiki.lwpu.com/engwiki/index.php/Resman/PState/Data_Tables/Clock_Programming_Table/1.0_Spec
struct CLK_PROG_35
{
    /*!
     * CLK_PROG_3X super class. Must always be first object in the
     * structure.
     */
    CLK_PROG_3X super;

    /*!
     * @ref freqMaxMHz + applied OC adjustments
     *
     * @note This is NOT enabled on PASCAL due to RM - PMU sync issues.
     */
    LwU16 offsettedFreqMaxMHz;
};

/*!
 * @brief Evaluates and Caches the latest @ref offsettedFreqMaxMHz for input clock programming entry.
 *
 * @memberof CLK_PROG_35
 *
 * @param[in]     pProg35          CLK_PROG_35 pointer
 * @param[in]     pDomain3XProg    CLK_DOMAIN_3X_PROG pointer
 *
 * @return FLCN_OK
 *     The offseted frequency was successfully cached.
 * @return Other errors
 *     An unexpected error coming from higher functions.
 */
#define ClkProg35CacheOffsettedFreqMaxMHz(fname) FLCN_STATUS (fname)(CLK_PROG_35 *pProg35, CLK_DOMAIN_3X_PROG *pDomain3XProg)

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// BOARDOBJ interfaces
BoardObjGrpIfaceModel10ObjSet (clkProgGrpIfaceModel10ObjSet_35)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkProgGrpIfaceModel10ObjSet_35");

ClkProg3XFreqMaxAdjAndQuantize   (clkProg3XFreqMaxAdjAndQuantize_35)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkProg3XFreqMaxAdjAndQuantize_35");

BoardObjIfaceModel10GetStatus                       (clkProgIfaceModel10GetStatus_35)
    GCC_ATTRIB_SECTION("imem_libClkStatus", "clkProgIfaceModel10GetStatus_35");
ClkProg35CacheOffsettedFreqMaxMHz   (clkProg35CacheOffsettedFreqMaxMHz)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkProg35CacheOffsettedFreqMaxMHz");

/* ------------------------ Include Derived Types -------------------------- */
#include "clk/clk_prog_35_primary.h"

#endif // CLK_PROG_35_H
