/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLK_VF_REL_RATIO_H
#define CLK_VF_REL_RATIO_H

/*!
 * @file clk_vf_rel_ratio.h
 * @brief @copydoc clk_vf_rel_ratio.c
 */

/* ------------------------ Includes --------------------------------------- */
#include "clk/clk_vf_rel.h"

/* ------------------------ Macros ----------------------------------------- */
/*!
 * Wrapper to super class defination.
 */
#define clkVfRelIfaceModel10Set_RATIO_FREQ    clkVfRelGrpIfaceModel10ObjSet_RATIO

/* ------------------------ Datatypes -------------------------------------- */
/*!
 * Forward definition of CLK_VF_REL_RATIO and CLK_VF_REL_RATIO
 * structures so that can use the type in interface definitions.
 */
typedef struct CLK_VF_REL_RATIO CLK_VF_REL_RATIO;

/*!
 * Clock VF Relationship - Ratio
 * Struct containing parameters specific to ratio based VF relationship.
 */
struct CLK_VF_REL_RATIO
{
    /*!
     * super class. Must always be first object in the structure.
     */
    CLK_VF_REL  super;

    /*!
     * Array of ratio entries.
     *
     * Has valid indexes in the range [0, @ref
     * LW2080_CTRL_CLK_CLK_VF_RELS_INFO::secondaryEntryCount).
     */
    LW2080_CTRL_CLK_CLK_VF_REL_RATIO_SECONDARY_ENTRY
        secondaryEntries[LW2080_CTRL_CLK_CLK_VF_REL_RATIO_SECONDARY_ENTRIES_MAX];
};

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// BOARDOBJ interfaces
BoardObjGrpIfaceModel10ObjSet (clkVfRelGrpIfaceModel10ObjSet_RATIO)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkVfRelGrpIfaceModel10ObjSet_RATIO");

// CLK_VF_REL interfaces
ClkVfRelFreqTranslatePrimaryToSecondary  (clkVfRelFreqTranslatePrimaryToSecondary_RATIO)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkVfRelFreqTranslatePrimaryToSecondary_RATIO");
ClkVfRelOffsetVFFreqTranslatePrimaryToSecondary  (clkVfRelOffsetVFFreqTranslatePrimaryToSecondary_RATIO)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkVfRelOffsetVFFreqTranslatePrimaryToSecondary_RATIO");
ClkVfRelFreqTranslateSecondaryToPrimary  (clkVfRelFreqTranslateSecondaryToPrimary_RATIO)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkVfRelFreqTranslateSecondaryToPrimary_RATIO");
ClkVfRelGetIdxFromFreq              (clkVfRelGetIdxFromFreq_RATIO)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkVfRelGetIdxFromFreq_RATIO");

/* ------------------------ Include Derived Types -------------------------- */
#include "clk/clk_vf_rel_ratio_volt.h"

#endif // CLK_VF_REL_RATIO_H
