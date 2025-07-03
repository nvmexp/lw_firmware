/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLK_PROG_30_H
#define CLK_PROG_30_H

/*!
 * @file clk_prog_30.h
 * @brief @copydoc clk_prog_30.c
 */

/* ------------------------ Includes --------------------------------------- */
#include "clk/clk_prog_3x.h"

/* ------------------------ Macros ----------------------------------------- */
/*!
 * PROG_30 wrapper to PROG_3X implemenation.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
#define clkProgGrpIfaceModel10ObjSet_30(pModel10, ppBoardObj, size, pBoardObjSet)  \
    (clkProgGrpIfaceModel10ObjSet_3X(pModel10, ppBoardObj, size, pBoardObjSet))

/* ------------------------ Datatypes -------------------------------------- */
/*!
 * Forward definition of CLK_PROG_30 structures so that can use
 * the type in interface definitions.
 */
typedef struct CLK_PROG_30 CLK_PROG_30;

/*!
 * Clock Programming 30 entry. Defines the how the RM will program a given
 * frequency on a clock domain per the VBIOS specification.
 *
 */
//! https://wiki.lwpu.com/engwiki/index.php/Resman/PState/Data_Tables/Clock_Programming_Table/1.0_Spec
struct CLK_PROG_30
{
    /*!
     * CLK_PROG_3X super class. Must always be first object in the
     * structure.
     */
    CLK_PROG_3X super;
};

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
ClkProg3XFreqMaxAdjAndQuantize   (clkProg3XFreqMaxAdjAndQuantize_30)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkProg3XFreqMaxAdjAndQuantize_30");

/* ------------------------ Include Derived Types -------------------------- */
#include "clk/clk_prog_30_primary.h"

#endif // CLK_PROG_30_H
