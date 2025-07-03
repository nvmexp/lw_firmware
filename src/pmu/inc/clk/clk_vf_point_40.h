/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLK_VF_POINT_40_H
#define CLK_VF_POINT_40_H

/*!
 * @file clk_vf_point_40.h
 * @brief @copydoc clk_vf_point_40.c
 */

/* ------------------------ Includes --------------------------------------- */
#include "clk/clk_vf_point.h"

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Datatypes -------------------------------------- */
/*!
 * Clock VF Point 40 structure.
 */
struct CLK_VF_POINT_40
{
    /*!
     * CLK_VF_POINT super class. Must always be the first element in the structure.
     */
    CLK_VF_POINT super;
    /*!
     * This will give the deviation of given freq from it's nominal value
     * aaplied from client
     */
    LW2080_CTRL_CLK_FREQ_DELTA  clientFreqDelta;
};

/*!
 * @interface CLK_VF_POINT_40
 *
 * Load Clock VF Point 40 object
 *
 * @param[in]  pVfPoint40       CLK_VF_POINT_40 pointer
 * @param[in]  pDomain40Prog    CLK_DOMAIN_40_PROG pointer
 * @param[in]  pVfRel           CLK_VF_REL pointer
 * @param[in]  lwrveIdx         VF lwrve index.
 *
 * @return FLCN_OK
 *     VF points successfully loaded.
 * @return Other errors while loading Clock VF Points.
 */
#define ClkVfPoint40Load(fname) FLCN_STATUS (fname)(CLK_VF_POINT_40 *pVfPoint40, CLK_DOMAIN_40_PROG *pDomain40Prog, CLK_VF_REL *pVfRel, LwU8 lwrveIdx)

/*!
 * @interface CLK_VF_POINT_40
 *
 * Caches the dynamically evaluated voltage or frequency value for this
 * CLK_VF_POINT_40 per the semantics of this CLK_VF_POINT_40 object's class.
 *
 * @note This is a virtual interface which the each CLK_VF_POINT_40 child class
 * must implement.
 *
 * @param[in]       pVfPoint40          CLK_VF_POINT_40 pointer
 * @param[in/out]   pVfPoint40Last      CLK_VF_POINT_40 pointer pointing to the
 *                                      last evaluated clk vf point.
 * @param[in]       pDomain40Prog     CLK_DOMAIN_40_PRIMARY pointer
 * @param[in]       pVfRel              CLK_VF_REL Pointer
 * @param[in]       lwrveIdx            Vf lwrve index.
 * @param[in]       bVFEEvalRequired    Whether VFE Evaluation is required?
 *
 * @return FLCN_OK
 *     CLK_VF_POINT_40 successfully cached.
 * @return Other errors
 *     Unexpected errors oclwrred during caching.
 */
#define ClkVfPoint40Cache(fname) FLCN_STATUS (fname)(CLK_VF_POINT_40 *pVfPoint40, CLK_VF_POINT_40 *pVfPoint40Last, CLK_DOMAIN_40_PROG *pDomain40Prog, CLK_VF_REL *pVfRel, LwU8 lwrveIdx, LwBool bVFEEvalRequired)

/*!
 * @interface CLK_VF_POINT_40
 *
 * Cache secondary clock domains VF point using cached primary clock domain
 * VF point.
 *
 * @param[in]       pVfPoint40          CLK_VF_POINT_40 pointer
 * @param[in/out]   pVfPoint40Last      CLK_VF_POINT_40 pointer pointing to the
 *                                      previous clk vf point.
 * @param[in]       pDomain40Prog       CLK_DOMAIN_40_PRIMARY pointer
 * @param[in]       pVfRel              CLK_VF_REL Pointer
 * @param[in]       lwrveIdx            Vf lwrve index.
 * @param[in]       bVFEEvalRequired    Whether VFE Evaluation is required?
 *
 * @return FLCN_OK
 *     CLK_VF_POINT_40 successfully cached.
 * @return Other errors
 *     Unexpected errors oclwrred during caching.
 */
#define ClkVfPoint40SecondaryCache(fname) FLCN_STATUS (fname)(CLK_VF_POINT_40 *pVfPoint40, CLK_VF_POINT_40 *pVfPoint40Last, CLK_DOMAIN_40_PROG *pDomain40Prog, CLK_VF_REL *pVfRel, LwU8 lwrveIdx, LwBool bVFEEvalRequired)

/*!
 * @interface CLK_VF_POINT_40
 *
 * Caches the evaluated offset voltage or frequency value for this
 * CLK_VF_POINT_40 per the semantics of this CLK_VF_POINT_40 object's class.
 *
 * @note This is a virtual interface which the each CLK_VF_POINT_40 child class
 * must implement.
 *
 * @param[in]       pVfPoint40          CLK_VF_POINT_40 pointer
 * @param[in/out]   pVfPoint40Last      CLK_VF_POINT_40 pointer pointing to the
 *                                      last evaluated clk vf point.
 * @param[in]       pDomain40Prog       CLK_DOMAIN_40_PRIMARY pointer
 * @param[in]       pVfRel              CLK_VF_REL Pointer
 *
 * @return FLCN_OK
 *     CLK_VF_POINT_40 successfully cached.
 * @return Other errors
 *     Unexpected errors oclwrred during caching.
 */
#define ClkVfPoint40OffsetVFCache(fname) FLCN_STATUS (fname)(CLK_VF_POINT_40 *pVfPoint40, CLK_VF_POINT_40 *pVfPoint40Last, CLK_DOMAIN_40_PROG *pDomain40Prog, CLK_VF_REL *pVfRel)

/*!
 * @interface CLK_VF_POINT_40
 *
 * Cache secondary clock domains offset VF point using cached primary clock domain
 * VF point.
 *
 * @param[in]       pVfPoint40          CLK_VF_POINT_40 pointer
 * @param[in/out]   pVfPoint40Last      CLK_VF_POINT_40 pointer pointing to the
 *                                      previous clk vf point.
 * @param[in]       pDomain40Prog       CLK_DOMAIN_40_PRIMARY pointer
 * @param[in]       pVfRel              CLK_VF_REL Pointer
 * @param[in]       lwrveIdx            Vf lwrve index.
 *
 * @return FLCN_OK
 *     CLK_VF_POINT_40 successfully cached.
 * @return Other errors
 *     Unexpected errors oclwrred during caching.
 */
#define ClkVfPoint40SecondaryOffsetVFCache(fname) FLCN_STATUS (fname)(CLK_VF_POINT_40 *pVfPoint40, CLK_VF_POINT_40 *pVfPoint40Last, CLK_DOMAIN_40_PROG *pDomain40Prog, CLK_VF_REL *pVfRel, LwU8 lwrveIdx) 

/*!
 * @interface CLK_VF_POINT_40
 *
 * Caches the dynamically evaluated base voltage or frequency value for this
 * CLK_VF_POINT_40 per the semantics of this CLK_VF_POINT_40 object's class.
 *
 * @note This is a virtual interface which the each CLK_VF_POINT_40 child class
 * must implement.
 *
 * @param[in]       pVfPoint40          CLK_VF_POINT_40 pointer
 * @param[in/out]   pVfPoint40Last      CLK_VF_POINT_40 pointer pointing to the
 *                                      last evaluated clk vf point.
 * @param[in]       pDomain40Prog       CLK_DOMAIN_40_PRIMARY pointer
 * @param[in]       pVfRel              CLK_VF_REL Pointer
 * @param[in]       lwrveIdx            Vf lwrve index.
 * @param[in]       cacheIdx            VF lwrve cache buffer index (temperature / step size).
 *
 * @return FLCN_OK
 *     CLK_VF_POINT_40 successfully cached.
 * @return Other errors
 *     Unexpected errors oclwrred during caching.
 */
#define ClkVfPoint40BaseVFCache(fname) FLCN_STATUS (fname)(CLK_VF_POINT_40 *pVfPoint40, CLK_VF_POINT_40 *pVfPoint40Last, CLK_DOMAIN_40_PROG *pDomain40Prog, CLK_VF_REL *pVfRel, LwU8 lwrveIdx, LwU8 cacheIdx)

/*!
 * @interface CLK_VF_POINT_40
 *
 * Cache secondary clock domains base VF point using cached primary clock domain
 * VF point.
 *
 * @param[in]       pVfPoint40          CLK_VF_POINT_40 pointer
 * @param[in/out]   pVfPoint40Last      CLK_VF_POINT_40 pointer pointing to the
 *                                      previous clk vf point.
 * @param[in]       pDomain40Prog       CLK_DOMAIN_40_PRIMARY pointer
 * @param[in]       pVfRel              CLK_VF_REL Pointer
 * @param[in]       lwrveIdx            Vf lwrve index.
 * @param[in]       cacheIdx            VF lwrve cache buffer index (temperature / step size).
 *
 * @return FLCN_OK
 *     CLK_VF_POINT_40 successfully cached.
 * @return Other errors
 *     Unexpected errors oclwrred during caching.
 */
#define ClkVfPoint40SecondaryBaseVFCache(fname) FLCN_STATUS (fname)(CLK_VF_POINT_40 *pVfPoint40, CLK_VF_POINT_40 *pVfPoint40Last, CLK_DOMAIN_40_PROG *pDomain40Prog, CLK_VF_REL *pVfRel, LwU8 lwrveIdx, LwU8 cacheIdx)

/*!
 * @interface CLK_VF_POINT_40
 *
 * Helper interface to get offset VF tuple.
 *
 * @param[in] pVfPoint40    CLK_VF_POINT_40 pointer
 *
 * @return Pointer to LW2080_CTRL_CLK_CLK_VF_POINT_VF_TUPLE
 * @return NULL
 *     Unexpected errors oclwrred while looking for pointer.
 */
#define ClkVfPoint40OffsetedVFTupleGet(fname) LW2080_CTRL_CLK_CLK_VF_POINT_VF_TUPLE * (fname)(CLK_VF_POINT_40 *pVfPoint40)

/*!
 * @interface CLK_VF_POINT_40
 *
 * Helper interface to get solely the offset VF tuple.
 *
 * @param[in] pVfPoint40    CLK_VF_POINT_40 pointer
 *
 * @return Pointer to LW2080_CTRL_CLK_CLK_VF_POINT_VF_TUPLE
 * @return NULL
 *     Unexpected errors oclwrred while looking for pointer.
 */
#define ClkVfPoint40OffsetVFTupleGet(fname) LW2080_CTRL_CLK_OFFSET_VF_TUPLE * (fname)(CLK_VF_POINT_40 *pVfPoint40)

/*!
 * @interface CLK_VF_POINT_40
 *
 * Helper interface to get base VF tuple.
 *
 * @param[in] pVfPoint40    CLK_VF_POINT_40 pointer
 *
 * @return Pointer to LW2080_CTRL_CLK_CLK_VF_POINT_BASE_VF_TUPLE
 * @return NULL
 *     Unexpected errors oclwrred while looking for pointer.
 */
#define ClkVfPoint40BaseVFTupleGet(fname) LW2080_CTRL_CLK_CLK_VF_POINT_BASE_VF_TUPLE * (fname)(CLK_VF_POINT_40 *pVfPoint40)

/*!
 * Accessor to a CLK_VF_POINT's voltage and frequency value with/without all applied
 * frequency and voltage deltas adjustments.
 *
 * @parma[in]  pVfPoint40       CLK_VF_POINT_40 pointer
 * @param[in]  pDomain40Prog    CLK_DOMAIN_40_PROG pointer
 * @param[in]  pVfRel           CLK_VF_REL pointer
 * @param[in]  lwrveIdx         Vf lwrve index.
 * @param[in]  voltageType
 *     Type of voltage value by which to look-up.  @ref
 *     LW2080_CTRL_CLK_VOLTAGE_TYPE_<xyz>.
 * @param[in]  bOffseted        Requested offseted VF pair?
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
#define ClkVfPoint40Accessor(fname) FLCN_STATUS (fname)(CLK_VF_POINT_40 *pVfPoint40, CLK_DOMAIN_40_PROG *pDomain40Prog, CLK_VF_REL *pVfRel, LwU8 lwrveIdx, LwU8 voltageType, LwBool bOffseted, LW2080_CTRL_CLK_VF_PAIR *pVfPair)

/*!
 * Accessor to a CLK_VF_POINT_40's LUT frequency tuple values.
 *
 * @param[in]  pVfPoint40       CLK_VF_POINT_40 pointer
 * @param[in]  pDomain40Prog    CLK_DOMAIN_40_PRIMARY pointer
 * @param[in]  pVfRel           CLK_VF_REL pointer
 * @param[in]  lwrveIdx         VF lwrve index.
 * @param[in]  bOffseted        Requested offseted frequency values?
 * @param[out] pOutput
 *     Pointer to structure in which to return the frequency tuple value.
 *
 * @return FLCN_OK
 *     CLK_VF_POINT_40 frequency tuple successfully accessed.
 * @return Other errors
 *     An unexpected error oclwrred and frequency tuple could not be
 *     accessed.
 */
#define ClkVfPoint40FreqTupleAccessor(fname) FLCN_STATUS (fname)(CLK_VF_POINT_40 *pVfPoint40, CLK_DOMAIN_40_PROG *pDomain40Prog, CLK_VF_REL *pVfRel, LwU8 lwrveIdx, LwBool bOffseted, LW2080_CTRL_CLK_CLK_VF_POINT_LUT_FREQ_TUPLE *pOutput)

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// BOARDOBJGRP interfaces
BoardObjGrpIfaceModel10CmdHandler   (clkVfPointBoardObjGrpIfaceModel10Set_40)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkVfPointBoardObjGrpIfaceModel10Set_40");
BoardObjGrpIfaceModel10CmdHandler   (clkVfPointBoardObjGrpIfaceModel10GetStatus_40)
    GCC_ATTRIB_SECTION("imem_libClkStatus", "clkVfPointBoardObjGrpIfaceModel10GetStatus_40");

BoardObjGrpIfaceModel10ObjSet       (clkVfPointGrpIfaceModel10ObjSet_40)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkVfPointGrpIfaceModel10ObjSet_40");
BoardObjIfaceModel10GetStatus           (clkVfPointIfaceModel10GetStatus_40)
    GCC_ATTRIB_SECTION("imem_libClkStatus", "clkVfPointIfaceModel10GetStatus_40");

// CLK_VF_POINT interfaces
ClkVfPointVoltageuVGet          (clkVfPointVoltageuVGet_40)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkVfPointVoltageuVGet_40");
ClkVfPointClientFreqDeltaSet    (clkVfPointClientFreqDeltaSet_40)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkVfPointClientFreqDeltaSet_40");
ClkVfPointClientVfTupleGet      (clkVfPointClientVfTupleGet_40)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkVfPointClientVfTupleGet_40");

// CLK_VF_POINT_40 interfaces
ClkVfPoint40Load                (clkVfPoint40Load)
    GCC_ATTRIB_SECTION("imem_vfLoad", "clkVfPoint40Load");
ClkVfPoint40Cache               (clkVfPoint40Cache)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkVfPoint40Cache");
ClkVfPoint40SecondaryCache          (clkVfPoint40SecondaryCache)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkVfPoint40SecondaryCache");
ClkVfPoint40OffsetVFCache       (clkVfPoint40OffsetVFCache)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkVfPoint40OffsetVFCache");
ClkVfPoint40SecondaryOffsetVFCache  (clkVfPoint40SecondaryOffsetVFCache)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkVfPoint40SecondaryOffsetVFCache");
ClkVfPoint40BaseVFCache         (clkVfPoint40BaseVFCache)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkVfPoint40BaseVFCache");
ClkVfPoint40SecondaryBaseVFCache    (clkVfPoint40SecondaryBaseVFCache)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkVfPoint40SecondaryBaseVFCache");
ClkVfPoint40OffsetedVFTupleGet  (clkVfPoint40OffsetedVFTupleGet)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkVfPoint40OffsetedVFTupleGet");
ClkVfPoint40OffsetVFTupleGet    (clkVfPoint40OffsetVFTupleGet)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkVfPoint40OffsetVFTupleGet");
ClkVfPoint40BaseVFTupleGet      (clkVfPoint40BaseVFTupleGet)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkVfPoint40BaseVFTupleGet");
ClkVfPoint40Accessor            (clkVfPoint40Accessor)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkVfPoint40Accessor");
ClkVfPoint40FreqTupleAccessor   (clkVfPoint40FreqTupleAccessor)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkVfPoint40FreqTupleAccessor");

/* ------------------------ Include Derived Types -------------------------- */
#include "clk/clk_vf_point_40_freq.h"
#include "clk/clk_vf_point_40_volt.h"

#endif // CLK_VF_POINT_40_H
