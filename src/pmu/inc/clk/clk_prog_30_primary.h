/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLK_PROG_30_PRIMARY_H
#define CLK_PROG_30_PRIMARY_H

/*!
 * @file clk_prog_30_primary.h
 * @brief @copydoc clk_prog_30_primary.c
 */

/* ------------------------ Includes --------------------------------------- */
#include "clk/clk_prog_30.h"
#include "clk/clk_prog_3x_primary.h"

/* ------------------------ Macros ----------------------------------------- */
/*!
 * Accessor macro for a CLK_PROG_3X_PRIMARY structure from CLK_PROG_35_PRIMARY.
 */
#define CLK_CLK_PROG_3X_PRIMARY_GET_FROM_30_PRIMARY(_pPrimary30)                 \
    &((_pPrimary30)->primary)

/* ------------------------ Datatypes -------------------------------------- */
typedef struct CLK_PROG_30_PRIMARY CLK_PROG_30_PRIMARY;

/*!
 * Clock Programming entry.  Defines the how the RM will program a given
 * frequency on a clock domain per the VBIOS specification.
 *
 */
//! https://wiki.lwpu.com/engwiki/index.php/Resman/PState/Data_Tables/Clock_Programming_Table/1.0_Spec#Primary
struct CLK_PROG_30_PRIMARY
{
    /*!
     * CLK_PROG_30 super class.  Must always be first object in the
     * structure.
     */
    CLK_PROG_30         super;

    /*!
     * CLK_PROG_3X_PRIMARY super class.
     */
    CLK_PROG_3X_PRIMARY  primary;
};

/*!
 * @interface CLK_PROG_30_PRIMARY
 *
 * Caches the dynamically evaluated VFE Equation values for this
 * CLK_PROG_30_PRIMARY object.  Expected to be called form the VFE polling loop.
 *
 * Iterates over the set of CLK_VF_POINTs pointed to by this CLK_PROG_30_PRIMARY
 * and caches their VF values.  Ensures monotonically increasing by always
 * returning the last VF value in the @ref pVFPairLast param.
 *
 * @param[in]     pProg30Primary     CLK_PROG_30_PRIMARY pointer
 * @param[in]     pDomain30Primary   CLK_DOMAIN_30_PRIMARY pointer
 * @param[in]     voltRailIdx
 *      Index of the VOLTAGE_RAIL to cache.
 * @param[in/out] pVFPairLast
 *      Pointer to LW2080_CTRL_CLK_VF_PAIR structure containing the last VF pair
 *      evaluated by previous CLK_PROG_30_PRIMARY objects, and in which this
 *      CLK_PROG_30_PRIMARY object will return its last VF pair.
 * @param[in/out] pBaseVFPairLast
 *      Pointer to LW2080_CTRL_CLK_VF_PAIR structure containing the last base
 *      VF pair evaluated by previous CLK_PROG_30_PRIMARY objects, and in which
 *      this CLK_PROG_30_PRIMARY object will return its last base VF pair.
 *
 * @return FLCN_OK
 *     CLK_PROG_30_PRIMARY successfully cached.
 * @return Other errors
 *     Unexpected errors oclwred during caching.
 */
#define ClkProg30PrimaryCache(fname) FLCN_STATUS (fname)(CLK_PROG_30_PRIMARY *pProg30Primary, CLK_DOMAIN_30_PRIMARY *pDomain30Primary, LwU8 voltRailIdx, LW2080_CTRL_CLK_VF_PAIR *pVFPairLast, LW2080_CTRL_CLK_VF_PAIR *pBaseVFPairLast)

/*!
 * @interface CLK_PROG_30_PRIMARY
 *
 * Iterates over the set of CLK_VF_POINTs pointed to by this CLK_PROG_30_PRIMARY
 * and smoothen their VF values. Ensures that the discontinuity between two
 * conselwtive VF points are within the max allowed bound.
 *
 * @param[in]     pProg30Primary     CLK_PROG_30_PRIMARY pointer
 * @param[in]     pDomain30Primary   CLK_DOMAIN_30_PRIMARY pointer
 * @param[in]     voltRailIdx       Index of the VOLTAGE_RAIL to cache.
 *
 * @return FLCN_OK
 *     CLK_PROG_30_PRIMARY successfully smoothen.
 * @return Other errors
 *     Unexpected errors oclwred during caching.
 */
#define ClkProg30PrimarySmoothing(fname) FLCN_STATUS (fname)(CLK_PROG_30_PRIMARY *pProg30Primary, CLK_DOMAIN_30_PRIMARY *pDomain30Primary, LwU8 voltRailIdx)

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// BOARDOBJ interfaces
BoardObjGrpIfaceModel10ObjSet (clkProgGrpIfaceModel10ObjSet_30_PRIMARY)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkProgGrpIfaceModel10ObjSet_30_PRIMARY");

// CLK_PROG_30_PRIMARY interfaces
ClkProg30PrimaryCache                      (clkProg30PrimaryCache)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkProg30PrimaryCache");
ClkProg30PrimarySmoothing                  (clkProg30PrimarySmoothing)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkProg30PrimarySmoothing");

/* ------------------------ Include Derived Types -------------------------- */
#include "clk/clk_prog_30_primary_ratio.h"
#include "clk/clk_prog_30_primary_table.h"

#endif // CLK_PROG_30_PRIMARY_H
