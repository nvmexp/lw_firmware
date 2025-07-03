/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLK_VF_REL_TABLE_H
#define CLK_VF_REL_TABLE_H

/*!
 * @file clk_vf_rel_table.h
 * @brief @copydoc clk_vf_rel_table.c
 */

/* ------------------------ Includes --------------------------------------- */
#include "clk/clk_vf_rel.h"

/* ------------------------ Macros ----------------------------------------- */
/*!
 * Wrapper to super class defination.
 */
#define clkVfRelIfaceModel10Set_TABLE_FREQ    clkVfRelGrpIfaceModel10ObjSet_TABLE
/* ------------------------ Datatypes -------------------------------------- */
/*!
 * Forward definition of CLK_VF_REL_TABLE and CLK_VF_REL_TABLE
 * structures so that can use the type in interface definitions.
 */
typedef struct CLK_VF_REL_TABLE CLK_VF_REL_TABLE;

/*!
 * Clock VF Relationship - Ratio
 * Struct containing parameters specific to ratio based VF relationship.
 */
struct CLK_VF_REL_TABLE
{
    /*!
     * super class. Must always be first object in the structure.
     */
    CLK_VF_REL  super;

    /*!
     * Array of table entries.
     *
     * Has valid indexes in the range [0, @ref
     * LW2080_CTRL_CLK_CLK_VF_RELS_INFO::secondaryEntryCount).
     */
    LW2080_CTRL_CLK_CLK_VF_REL_TABLE_SECONDARY_ENTRY
        secondaryEntries[LW2080_CTRL_CLK_CLK_VF_REL_TABLE_SECONDARY_ENTRIES_MAX];
};

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// BOARDOBJ interfaces
BoardObjGrpIfaceModel10ObjSet (clkVfRelGrpIfaceModel10ObjSet_TABLE)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkVfRelGrpIfaceModel10ObjSet_TABLE");

// CLK_VF_REL interfaces
ClkVfRelFreqTranslatePrimaryToSecondary  (clkVfRelFreqTranslatePrimaryToSecondary_TABLE)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkVfRelFreqTranslatePrimaryToSecondary_TABLE");
ClkVfRelOffsetVFFreqTranslatePrimaryToSecondary  (clkVfRelOffsetVFFreqTranslatePrimaryToSecondary_TABLE)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkVfRelOffsetVFFreqTranslatePrimaryToSecondary_TABLE");
ClkVfRelFreqTranslateSecondaryToPrimary  (clkVfRelFreqTranslateSecondaryToPrimary_TABLE)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkVfRelFreqTranslateSecondaryToPrimary_TABLE");
ClkVfRelGetIdxFromFreq              (clkVfRelGetIdxFromFreq_TABLE)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkVfRelGetIdxFromFreq_TABLE");

/* ------------------------ Include Derived Types -------------------------- */

#endif // CLK_VF_REL_TABLE_H
