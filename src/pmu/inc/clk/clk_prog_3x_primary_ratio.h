/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLK_PROG_3X_PRIMARY_RATIO_H
#define CLK_PROG_3X_PRIMARY_RATIO_H

/*!
 * @file clk_prog_3x_primary_ratio.h
 * @brief @copydoc clk_prog_3x_primary_ratio.c
 */

/* ------------------------ Includes --------------------------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Datatypes -------------------------------------- */
/*!
 * Forward definition of CLK_PROG_3X_PRIMARY_RATIO structure so that can use the type in
 * interface definitions.
 */
typedef struct CLK_PROG_3X_PRIMARY_RATIO CLK_PROG_3X_PRIMARY_RATIO;

/*!
 * Clock Programming 3X Primary Ratio entry.  Defines the how the RM will program a given
 * frequency on a clock domain per the VBIOS specification.
 */
struct CLK_PROG_3X_PRIMARY_RATIO
{
    /*!
     * BOARDOBJ_INTERFACE super class. Must always be first object in the
     * structure.
     */
    BOARDOBJ_INTERFACE  super;

    /*!
     * Array of ratio entries.
     *
     * Has valid indexes in the range [0, @ref
     * LW2080_CTRL_CLK_CLK_PROGS_INFO::numSecondaryEntries).
     */
    LW2080_CTRL_CLK_CLK_PROG_1X_PRIMARY_RATIO_SECONDARY_ENTRY *pSecondaryEntries;
};

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// OBJECT interfaces
BoardObjInterfaceConstruct                (clkProgConstruct_3X_PRIMARY_RATIO)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkProgConstruct_3X_PRIMARY_RATIO");

// CLK_PROG_3X_PRIMARY interfaces
ClkProg3XPrimaryFreqTranslatePrimaryToSecondary (clkProg3XPrimaryFreqTranslatePrimaryToSecondary_RATIO)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkProg3XPrimaryFreqTranslatePrimaryToSecondary_RATIO");
ClkProg3XPrimaryFreqTranslateSecondaryToPrimary (clkProg3XPrimaryFreqTranslateSecondaryToPrimary_RATIO)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkProg3XPrimaryFreqTranslateSecondaryToPrimary_RATIO");
ClkProg3XPrimaryIsPrimaryProgIdxForSecondaryFreqMHz
                                          (clkProg3XPrimaryIsPrimaryProgIdxForSecondaryFreqMHz_RATIO)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkProg3XPrimaryIsPrimaryProgIdxForSecondaryFreqMHz_RATIO");

/* ------------------------ Include Derived Types -------------------------- */

#endif // CLK_PROG_3X_PRIMARY_RATIO_H
