/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLK_VF_POINT_3X_H
#define CLK_VF_POINT_3X_H

#include "g_clk_vf_point_3x.h"

#ifndef G_CLK_VF_POINT_3X_H
#define G_CLK_VF_POINT_3X_H

/*!
 * @file clk_vf_point_3x.h
 * @brief @copydoc clk_vf_point_3x.c
 */

/* ------------------------ Includes --------------------------------------- */
#include "clk/clk_vf_point.h"

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Datatypes -------------------------------------- */
/*!
 * @brief Load Clock VF Point object
 *
 * @memberof CLK_VF_POINT
 *
 * @param[in]  pVfPoint         CLK_VF_POINT pointer
 * @param[in]  pProg3XPrimary    CLK_PROG_3X_PRIMARY pointer
 * @param[in]  pDomain3XPrimary
 *     CLK_DOMAIN_3X_PRIMARY pointer to the PRIMARY CLK_DOMAIN which references
 *     this CLK_PROG_3X_PRIMARY object.
 * @param[in]       voltRailIdx
 *      Index of VOLT_RAIL of the CLK_VF_POINT which is being cached.
 * @param[in]  lwrveIdx         VF lwrve index.
 *
 * @return FLCN_OK
 *     VF points successfully loaded.
 * @return Other errors while loading Clock VF Points.
 */
#define ClkVfPointLoad(fname) FLCN_STATUS (fname)(CLK_VF_POINT *pVfPoint, CLK_PROG_3X_PRIMARY *pProg3XPrimary, CLK_DOMAIN_3X_PRIMARY *pDomain3XPrimary, LwU8 voltRailIdx, LwU8 lwrveIdx)

/*!
 * Accessor to a CLK_VF_POINT's voltage and frequency value with all applied
 * frequency and voltage deltas adjustments.
 *
 * @parma[in]  pVfPoint         CLK_VF_POINT pointer
 * @param[in]  pProg3XPrimary    CLK_PROG_1X_PRIMARY pointer
 * @param[in]  pDomain3XPrimary
 *     CLK_DOMAIN_3X_PRIMARY pointer to the PRIMARY CLK_DOMAIN which references
 *     this CLK_PROG_1X_PRIMARY object.
 * @param[in]  clkDomIdx
 *     Index of the CLK_DOMAIN for which to look-up.  The special value of @ref
 *     LW2080_CTRL_CLK_CLK_DOMAIN_INDEX_ILWALID indicates that the requested
 *     domain is the PRIMARY domain of this CLK_PROG_1X_PRIMARY.  Valid indexes
 *     are specified for CLK_DOMAINs which are SECONDARIES to the PRIMARY CLK_DOMAIN.
 * @param[in]  voltRailIdx
 *     Index of the VOLT_RAIL for which to look-up.
 * @param[in]  voltageType
 *     Type of voltage value by which to look-up.  @ref
 *     LW2080_CTRL_CLK_VOLTAGE_TYPE_<xyz>.
 * @param[in]  bOffseted    requested offseted VF pair?
 * @param[out] pVfPair
 *     Pointer to LW2080_CTRL_CLK_VF_PAIR structure in which to return the
 *     (voltage, frequency) pair for this CLK_VF_POINT.
 *
 * @return FLCN_OK
 *     CLK_VF_POINT voltage and frequency successfully accessed.
 * @return Other errors
 *     An unexpected error oclwrred and voltage and frequency could not be
 *     accessed.
 */
#define ClkVfPointAccessor(fname) FLCN_STATUS (fname)(CLK_VF_POINT *pVfPoint, CLK_PROG_3X_PRIMARY *pProg3XPrimary, CLK_DOMAIN_3X_PRIMARY *pDomain3XPrimary, LwU8 clkDomIdx, LwU8 voltRailIdx, LwU8 voltageType, LwBool bOffseted, LW2080_CTRL_CLK_VF_PAIR *pVfPair)

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// CLK_VF_POINTS interfaces
mockable ClkVfPointLoad (clkVfPointLoad)
    GCC_ATTRIB_SECTION("imem_vfLoad", "clkVfPointLoad");
ClkVfPointAccessor  (clkVfPointAccessor)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkVfPointAccessor");

/* ------------------------ Include Derived Types -------------------------- */
#include "clk/clk_vf_point_30.h"
#include "clk/clk_vf_point_35.h"

#endif // G_CLK_VF_POINT_3X_H
#endif // CLK_VF_POINT_3X_H
