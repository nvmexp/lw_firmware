/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLK_VF_POINT_35_H
#define CLK_VF_POINT_35_H

#include "g_clk_vf_point_35.h"

#ifndef G_CLK_VF_POINT_35_H
#define G_CLK_VF_POINT_35_H

/*!
 * @file clk_vf_point_35.h
 * @brief @copydoc clk_vf_point_35.c
 */

/* ------------------------ Includes --------------------------------------- */
#include "clk/clk_vf_point_3x.h"

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Datatypes -------------------------------------- */
/*!
 * Clock VF Point 35 structure.
 */
struct CLK_VF_POINT_35
{
    /*!
     * CLK_VF_POINT super class. Must always be the first element in the structure.
     */
    CLK_VF_POINT super;
};

/*!
 * @brief Caches the dynamically evaluated voltage or frequency value for this
 * CLK_VF_POINT_35 per the semantics of this CLK_VF_POINT_35 object's class.
 *
 * @note This is a virtual interface which the each CLK_VF_POINT_35 child class
 * must implement.
 *
 * @memberof CLK_VF_POINT_35
 *
 * @param[in]       pVfPoint35          CLK_VF_POINT_35 pointer
 * @param[in,out]   pVfPoint35Last      CLK_VF_POINT_35 pointer pointing to the
 *                                      last evaluated clk vf point.
 * @param[in]       pDomain35Primary     CLK_DOMAIN_35_PRIMARY pointer
 * @param[in]       pProg35Primary       CLK_PROG_35_PRIMARY Pointer
 * @param[in]       voltRailIdx
 *      Index of VOLT_RAIL of the CLK_VF_POINT which is being cached.
 * @param[in]       lwrveIdx            Vf lwrve index.
 * @param[in]       bVFEEvalRequired    Whether VFE Evaluation is required?
 *
 * @return FLCN_OK
 *     CLK_VF_POINT_35 successfully cached.
 * @return Other errors
 *     Unexpected errors oclwred during caching.
 */
#define ClkVfPoint35Cache(fname) FLCN_STATUS (fname)(CLK_VF_POINT_35 *pVfPoint35, CLK_VF_POINT_35 *pVfPoint35Last, CLK_DOMAIN_35_PRIMARY *pDomain35Primary, CLK_PROG_35_PRIMARY *pProg35Primary, LwU8 voltRailIdx, LwU8 lwrveIdx, LwBool bVFEEvalRequired)

/*!
 * @brief Smoothen the dynamically evaluated voltage or frequency value for this
 * CLK_VF_POINT_35 per the semantics of this CLK_VF_POINT_35 object's class.
 *
 * @memberof CLK_VF_POINT_35
 *
 * @param[in]       pVfPoint35          CLK_VF_POINT_35 pointer
 * @param[in/out]   pVfPoint35Last      CLK_VF_POINT_35 pointer pointing to the last
 *                                      evaluated clk vf point.
 * @param[in]       pDomain35Primary     CLK_DOMAIN_35_PRIMARY pointer
 * @param[in]       pProg35Primary       CLK_PROG_35_PRIMARY Pointer
 * @param[in]       lwrveIdx            Vf lwrve index.
 *
 * @return FLCN_OK
 *     CLK_VF_POINT_35 successfully smoothened.
 * @return Other errors
 *     Unexpected errors oclwred during VF lwrve smoothing.
 */
#define ClkVfPoint35Smoothing(fname) FLCN_STATUS (fname)(CLK_VF_POINT_35 *pVfPoint35, CLK_VF_POINT_35 *pVfPoint35Last, CLK_DOMAIN_35_PRIMARY *pDomain35Primary, CLK_PROG_35_PRIMARY *pProg35Primary, LwU8 lwrveIdx)

/*!
 * @brief Trim the VF tuple by enumeration max.
 *
 * @memberof CLK_VF_POINT_35
 *
 * @param[in]       pVfPoint35          CLK_VF_POINT_35 pointer
 * @param[in]       pDomain35Primary     CLK_DOMAIN_35_PRIMARY pointer
 * @param[in]       pProg35Primary       CLK_PROG_35_PRIMARY Pointer
 * @param[in]       lwrveIdx            Vf lwrve index.
 *
 * @return FLCN_OK
 *     CLK_VF_POINT_35 successfully trimmed.
 * @return Other errors
 *     Unexpected errors oclwred.
 */
#define ClkVfPoint35Trim(fname) FLCN_STATUS (fname)(CLK_VF_POINT_35 *pVfPoint35, CLK_DOMAIN_35_PRIMARY *pDomain35Primary, CLK_PROG_35_PRIMARY *pProg35Primary, LwU8 lwrveIdx)

/*!
 * @brief Helper interface to get offset VF tuple.
 *
 * @memberof CLK_VF_POINT_35
 *
 * @param[in] pVfPoint35    CLK_VF_POINT_35 pointer
 *
 * @return Pointer to LW2080_CTRL_CLK_CLK_VF_POINT_VF_TUPLE
 * @return NULL
 *     Unexpected errors oclwred while looking for pointer.
 */
#define ClkVfPoint35OffsetedVFTupleGet(fname) LW2080_CTRL_CLK_CLK_VF_POINT_VF_TUPLE * (fname)(CLK_VF_POINT_35 *pVfPoint35)

/*!
 * @brief Helper interface to get base VF tuple.
 *
 * @memberof CLK_VF_POINT_35
 *
 * @param[in] pVfPoint35    CLK_VF_POINT_35 pointer
 *
 * @return Pointer to LW2080_CTRL_CLK_CLK_VF_POINT_BASE_VF_TUPLE
 * @return NULL
 *     Unexpected errors oclwred while looking for pointer.
 */
#define ClkVfPoint35BaseVFTupleGet(fname) LW2080_CTRL_CLK_CLK_VF_POINT_BASE_VF_TUPLE * (fname)(CLK_VF_POINT_35 *pVfPoint35)

/*!
 * @brief Accessor to a CLK_VF_POINT_35's LUT frequency tuple values.
 *
 * @note - This interface is supported only on _VOLT_SEC class. For all
 * other use cases, client MUST use @ref ClkVfPointAccessor
 *
 * @param[in]  pVfPoint35       CLK_VF_POINT_35 *pointer
 * @param[in]  pDomain3XPrimary  CLK_DOMAIN_3X_PRIMARY pointer
 * @param[in]  clkDomIdx
 *     Index of the CLK_DOMAIN for which to look-up.
 * @param[in]  lwrveIdx         VF lwrve index.
 * @param[in]  bOffseted        Requested offseted frequency values?
 * @param[out] pOutput
 *     Pointer to structure in which to return the frequency tuple value.
 *
 * @return FLCN_OK
 *     CLK_VF_POINT_35 frequency tuple successfully accessed.
 * @return Other errors
 *     An unexpected error oclwrred and frequency tuple could not be
 *     accessed.
 */
#define ClkVfPoint35FreqTupleAccessor(fname) FLCN_STATUS (fname)(CLK_VF_POINT_35 *pVfPoint35, CLK_DOMAIN_3X_PRIMARY *pDomain3XPrimary, LwU8 clkDomIdx, LwU8 lwrveIdx, LwBool bOffseted, LW2080_CTRL_CLK_CLK_VF_POINT_LUT_FREQ_TUPLE *pOutput)

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// BOARDOBJGRP interfaces
BoardObjGrpIfaceModel10CmdHandler  (clkVfPointBoardObjGrpIfaceModel10Set_35)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkVfPointBoardObjGrpIfaceModel10Set_35");
BoardObjGrpIfaceModel10CmdHandler  (clkVfPointBoardObjGrpIfaceModel10GetStatus_35)
    GCC_ATTRIB_SECTION("imem_libClkStatus", "clkVfPointBoardObjGrpIfaceModel10GetStatus_35");

mockable BoardObjGrpIfaceModel10ObjSet (clkVfPointGrpIfaceModel10ObjSet_35)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkVfPointGrpIfaceModel10ObjSet_35");
mockable BoardObjIfaceModel10GetStatus     (clkVfPointIfaceModel10GetStatus_35)
    GCC_ATTRIB_SECTION("imem_libClkStatus", "clkVfPointIfaceModel10GetStatus_35");

// CLK_VF_POINT interfaces
ClkVfPointAccessor      (clkVfPointAccessor_35)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkVfPointAccessor_35");
mockable ClkVfPointVoltageuVGet  (clkVfPointVoltageuVGet_35)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkVfPointVoltageuVGet_35");

// CLK_VF_POINT_35 interfaces
mockable ClkVfPoint35Cache      (clkVfPoint35Cache)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkVfPoint35Cache");
ClkVfPoint35Smoothing           (clkVfPoint35Smoothing)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkVfPoint35Smoothing");
ClkVfPoint35Trim                (clkVfPoint35Trim)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkVfPoint35Trim");
ClkVfPoint35OffsetedVFTupleGet  (clkVfPoint35OffsetedVFTupleGet)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkVfPoint35OffsetedVFTupleGet");
ClkVfPoint35BaseVFTupleGet      (clkVfPoint35BaseVFTupleGet)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkVfPoint35BaseVFTupleGet");
ClkVfPoint35FreqTupleAccessor   (clkVfPoint35FreqTupleAccessor)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkVfPoint35FreqTupleAccessor");

/* ------------------------ Include Derived Types -------------------------- */
#include "clk/clk_vf_point_35_freq.h"
#include "clk/clk_vf_point_35_volt.h"

#endif // G_CLK_VF_POINT_35_H
#endif // CLK_VF_POINT_35_H
