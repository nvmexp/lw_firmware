/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2019 by LWPU Corpotablen.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corpotablen.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corpotablen is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLK_PROG_35_PRIMARY_TABLE_H
#define CLK_PROG_35_PRIMARY_TABLE_H

/*!
 * @file clk_prog_35_primary_table.h
 * @brief @copydoc clk_prog_35_primary_table.c
 */

/* ------------------------ Includes --------------------------------------- */
#include "clk/clk_prog_35_primary.h"
#include "clk/clk_prog_3x_primary_table.h"

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Datatypes -------------------------------------- */
/*!
 * Forward definition of CLK_PROG_35_PRIMARY_TABLE structures so that  can
 * use the type in interface definitions.
 */
typedef struct CLK_PROG_35_PRIMARY_TABLE CLK_PROG_35_PRIMARY_TABLE;


/*!
 * Clock Programming entry.  Defines the how the RM will program a given
 * frequency on a clock domain per the VBIOS specification.
 *
 */
//! https://wiki.lwpu.com/engwiki/index.php/Resman/PState/Data_Tables/Clock_Programming_Table/1.0_Spec#Primary
struct CLK_PROG_35_PRIMARY_TABLE
{
    /*!
     * CLK_PROG_35_PRIMARY super class.  Must always be first object in the
     * structure.
     */
    CLK_PROG_35_PRIMARY              super;

    /*!
     * 3X Primary Table super class.
     */
    CLK_PROG_3X_PRIMARY_TABLE        table;
};

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// BOARDOBJ interfaces
BoardObjGrpIfaceModel10ObjSet (clkProgGrpIfaceModel10ObjSet_35_PRIMARY_TABLE)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkProgGrpIfaceModel10ObjSet_35_PRIMARY_TABLE");

/* ------------------------ Include Derived Types -------------------------- */

#endif // CLK_PROG_35_PRIMARY_TABLE_H
